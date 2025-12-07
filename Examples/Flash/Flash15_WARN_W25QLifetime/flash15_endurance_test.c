/**
 * @file flash15_endurance_test.c
 * @brief Flash15正式寿命测试模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details W25Q系列Flash芯片破坏性寿命极限测试流程实现（测到报废）
 */

#include "flash15_endurance_test.h"
#include "w25q_spi.h"
#include "delay.h"
#include "board.h"
#include "config.h"
#include "core_cm3.h"
#include "system_stm32f10x.h"
#include <string.h>

#if CONFIG_MODULE_LOG_ENABLED
#include "log.h"
#endif

/* ==================== DWT定义 ==================== */
#ifndef DWT_BASE
#define DWT_BASE            (0xE0001000UL)
#endif

#ifndef DWT_Type
typedef struct
{
  __IO uint32_t CTRL;
  __IO uint32_t CYCCNT;
  __I  uint32_t CPICNT;
  __I  uint32_t EXCCNT;
  __I  uint32_t SLEEPCNT;
  __I  uint32_t LSUCNT;
  __I  uint32_t FOLDCNT;
  __I  uint32_t PCSR;
  __I  uint32_t COMP0;
  __I  uint32_t MASK0;
  __I  uint32_t FUNCTION0;
       uint32_t RESERVED0[1];
  __I  uint32_t COMP1;
  __I  uint32_t MASK1;
  __I  uint32_t FUNCTION1;
       uint32_t RESERVED1[1];
  __I  uint32_t COMP2;
  __I  uint32_t MASK2;
  __I  uint32_t FUNCTION2;
       uint32_t RESERVED2[1];
  __I  uint32_t COMP3;
  __I  uint32_t MASK3;
  __I  uint32_t FUNCTION3;
} DWT_Type;
#endif

#ifndef DWT_CTRL_CYCCNTENA_Msk
#define DWT_CTRL_CYCCNTENA_Pos                0
#define DWT_CTRL_CYCCNTENA_Msk                (1ul << DWT_CTRL_CYCCNTENA_Pos)
#endif

#ifndef DWT
#define DWT                 ((DWT_Type *)     DWT_BASE)
#endif

/* ==================== 常量定义 ==================== */

#define W25Q_PAGE_SIZE              256      /**< 页大小（字节） */
#define W25Q_SECTOR_SIZE            4096     /**< 扇区大小（字节） */
#define W25Q_BLOCK_SIZE             65536    /**< Block大小（64KB） */

/* 报废判定阈值 */
#define EOL_ERASE_TIME_THRESHOLD_MS     500.0f    /**< 单个Block擦除时间阈值（毫秒），>500ms为报废（对应性能档位"偏弱"档位） */
#define EOL_CHIP_ERASE_TIME_THRESHOLD_MS 120000.0f /**< 全片擦除时间阈值（毫秒），>120秒为报废 */
#define EOL_BAD_BLOCK_RATE_THRESHOLD    5.0f      /**< 坏块率阈值（%），>5%为报废 */
#define EOL_ERROR_RATE_THRESHOLD        1e-3f     /**< 误码率阈值，>1e-3为报废 */
#define EOL_ERASE_FAIL_COUNT            3        /**< 连续擦除失败次数阈值 */
#define EOL_PROGRAM_FAIL_COUNT           10       /**< 连续编程失败次数阈值 */
#define EOL_READ_DISTURB_THRESHOLD      10       /**< 读干扰错误阈值 */

/* 性能退化阈值 */
#define DEGRADATION_WARNING_RATE        30.0f     /**< 退化预警阈值（%） */
#define DEGRADATION_DANGER_RATE         50.0f     /**< 退化危险阈值（%） */

/* ==================== 模块状态 ==================== */

static uint8_t g_endurance_test_initialized = 0;  /**< 模块初始化标志 */

/* ==================== 静态缓冲区 ==================== */

static uint8_t s_test_buffer[256] __attribute__((aligned(4)));      /**< 测试数据缓冲区 */
static uint8_t s_read_buffer[256] __attribute__((aligned(4)));       /**< 读取数据缓冲区 */

/* ==================== DWT辅助函数 ==================== */

/**
 * @brief 初始化DWT周期计数器
 */
static void DWT_Init(void)
{
    if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk))
    {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
}

/**
 * @brief 使用DWT计算毫秒级延迟
 */
static float DWT_GetElapsedMs(uint32_t start_cycles, uint32_t end_cycles)
{
    uint32_t cycles = end_cycles - start_cycles;
    extern uint32_t SystemCoreClock;
    return (float)((uint64_t)cycles * 1000ULL) / (float)SystemCoreClock;
}

/* ==================== 辅助函数 ==================== */

/**
 * @brief 生成固定值数据模式
 * @param[out] buffer 数据缓冲区
 * @param[in] size 缓冲区大小（字节）
 * @param[in] seed 种子值（用作固定值，每轮循环+1）
 * @note 使用固定值填充缓冲区，便于快速发现写入错误
 */
static void GeneratePattern(uint8_t *buffer, uint32_t size, uint32_t seed)
{
    uint8_t fixed_value = (uint8_t)(seed & 0xFF);  /* 使用seed作为固定值 */
    memset(buffer, fixed_value, size);  /* 填充固定值 */
}

/**
 * @brief 统计位错误数
 * @param[in] expected 期望数据缓冲区
 * @param[in] actual 实际数据缓冲区
 * @param[in] size 缓冲区大小（字节）
 * @param[out] error_count 位错误数（输出参数）
 */
static void CountBitErrors(const uint8_t *expected, const uint8_t *actual, uint32_t size, uint32_t *error_count)
{
    uint32_t i;
    uint32_t bit_errors = 0;
    
    if (expected == NULL || actual == NULL || error_count == NULL)
    {
        return;
    }
    
    /* 逐字节比较，统计位错误 */
    for (i = 0; i < size; i++)
    {
        uint8_t diff = expected[i] ^ actual[i];  /* 异或运算，找出不同的位 */
        
        /* 统计不同的位数 */
        while (diff != 0)
        {
            if (diff & 0x01)
            {
                bit_errors++;
            }
            diff >>= 1;
        }
    }
    
    *error_count = bit_errors;
}

/* ==================== 公共接口函数实现 ==================== */

/**
 * @brief 寿命测试模块初始化
 * @return Endurance_Test_Status_t 错误码
 */
Endurance_Test_Status_t EnduranceTest_Init(void)
{
    /* 检查W25Q是否已初始化 */
    if (!W25Q_IsInitialized())
    {
        return ENDURANCE_TEST_ERROR_W25Q_FAILED;
    }
    
    /* 初始化DWT周期计数器 */
    DWT_Init();
    
    /* 标记为已初始化 */
    g_endurance_test_initialized = 1;
    
    return ENDURANCE_TEST_OK;
}

/**
 * @brief 记录基准数据（0次擦写循环时）
 * @param[out] result 测试结果结构体指针
 * @return Endurance_Test_Status_t 错误码
 */
Endurance_Test_Status_t EnduranceTest_RecordBaseline(Endurance_Test_Result_t *result)
{
    const w25q_dev_t *dev_info;
    uint64_t capacity_bytes;
    uint32_t total_pages;
    uint32_t page_addr;
    W25Q_Status_t w25q_status;
    uint32_t start_cycles, end_cycles;
    float program_time_sum = 0.0f;
    float read_time_sum = 0.0f;
    uint32_t program_count = 0;
    uint32_t read_count = 0;
    uint32_t bit_errors = 0;
    uint32_t total_errors = 0;
    
    if (result == NULL)
    {
        return ENDURANCE_TEST_ERROR_NULL_PTR;
    }
    
    if (!g_endurance_test_initialized)
    {
        return ENDURANCE_TEST_ERROR_NOT_INIT;
    }
    
    /* 获取设备信息 */
    dev_info = W25Q_GetInfo();
    if (dev_info == NULL)
    {
        return ENDURANCE_TEST_ERROR_W25Q_FAILED;
    }
    
    capacity_bytes = (uint64_t)dev_info->capacity_mb * 1024ULL * 1024ULL;
    total_pages = (uint32_t)(capacity_bytes / W25Q_PAGE_SIZE);
    
#if CONFIG_MODULE_LOG_ENABLED
    LOG_INFO("ENDURANCE_TEST", "=== 记录基准数据（0次循环） ===");
#endif
    
    /* 测试全片擦除时间（作为基准） */
#if CONFIG_MODULE_LOG_ENABLED
    LOG_INFO("ENDURANCE_TEST", "记录基准：执行全片擦除...");
#endif
    start_cycles = DWT->CYCCNT;
    w25q_status = W25Q_EraseChip();
    end_cycles = DWT->CYCCNT;
    
    if (w25q_status == W25Q_OK)
    {
        /* 根据芯片容量动态计算超时时间：Block数量 × 200ms/块 × 安全系数3 */
        uint32_t timeout_ms = 120000;  /* 默认120秒 */
        if (dev_info != NULL)
        {
            uint64_t capacity_bytes_baseline = (uint64_t)dev_info->capacity_mb * 1024ULL * 1024ULL;
            uint32_t total_blocks_baseline = (uint32_t)(capacity_bytes_baseline / W25Q_BLOCK_SIZE);
            if (total_blocks_baseline > 0)
            {
                /* 超时时间 = Block数量 × 200ms/块 × 3（安全系数） */
                timeout_ms = total_blocks_baseline * 200 * 3;
                /* 最小120秒，最大600秒（10分钟） */
                if (timeout_ms < 120000) timeout_ms = 120000;
                if (timeout_ms > 600000) timeout_ms = 600000;
            }
        }
        w25q_status = W25Q_WaitReady(timeout_ms);
        if (w25q_status == W25Q_OK)
        {
            float erase_time = DWT_GetElapsedMs(start_cycles, end_cycles);
            result->baseline.erase_time_min = erase_time;
            result->baseline.erase_time_avg = erase_time;
        }
    }
    
    /* 测试编程和读取时间（测试100个Page） */
    for (program_count = 0; program_count < 100 && program_count < total_pages; program_count++)
    {
        page_addr = program_count * W25Q_PAGE_SIZE;
        
        /* 生成测试数据 */
        GeneratePattern(s_test_buffer, W25Q_PAGE_SIZE, page_addr);
        
        /* 测量编程时间 */
        start_cycles = DWT->CYCCNT;
        w25q_status = W25Q_Write(page_addr, s_test_buffer, W25Q_PAGE_SIZE);
        end_cycles = DWT->CYCCNT;
        
        if (w25q_status == W25Q_OK)
        {
            w25q_status = W25Q_WaitReady(2000);
            if (w25q_status == W25Q_OK)
            {
                program_time_sum += DWT_GetElapsedMs(start_cycles, end_cycles);
            }
        }
        
        /* 测量读取时间 */
        start_cycles = DWT->CYCCNT;
        w25q_status = W25Q_Read(page_addr, s_read_buffer, W25Q_PAGE_SIZE);
        end_cycles = DWT->CYCCNT;
        
        if (w25q_status == W25Q_OK)
        {
            read_time_sum += DWT_GetElapsedMs(start_cycles, end_cycles);
            read_count++;
            
            /* 统计错误 */
            CountBitErrors(s_test_buffer, s_read_buffer, W25Q_PAGE_SIZE, &bit_errors);
            total_errors += bit_errors;
        }
    }
    
    if (program_count > 0)
    {
        result->baseline.program_time_avg = program_time_sum / program_count;
    }
    
    if (read_count > 0)
    {
        float read_time_avg = read_time_sum / read_count;
        result->baseline.read_speed = (W25Q_PAGE_SIZE / 1024.0f) / (read_time_avg / 1000.0f);  /* KB/s */
    }
    
    result->baseline.error_rate = total_errors;
    result->baseline.unique_id = 0;  /* 未实现Unique ID读取 */
    
    result->baseline_recorded = 1;
    
#if CONFIG_MODULE_LOG_ENABLED
    /* 计算每块平均擦除时间 */
    uint32_t total_blocks_baseline = (uint32_t)(capacity_bytes / W25Q_BLOCK_SIZE);
    float erase_time_per_block_ms_baseline = 0.0f;
    if (total_blocks_baseline > 0)
    {
        erase_time_per_block_ms_baseline = result->baseline.erase_time_min / (float)total_blocks_baseline;
    }
    
    LOG_INFO("ENDURANCE_TEST", "基准数据记录完成");
    LOG_INFO("ENDURANCE_TEST", "  最小擦除时间: %.2f 秒 (%.2f ms/块)", 
             result->baseline.erase_time_min / 1000.0f, erase_time_per_block_ms_baseline);
    LOG_INFO("ENDURANCE_TEST", "  平均擦除时间: %.2f 秒 (%.2f ms/块)", 
             result->baseline.erase_time_avg / 1000.0f, erase_time_per_block_ms_baseline);
    LOG_INFO("ENDURANCE_TEST", "  平均编程时间: %.3f ms/页", result->baseline.program_time_avg);
    LOG_INFO("ENDURANCE_TEST", "  读取速度: %.2f KB/s", result->baseline.read_speed);
    LOG_INFO("ENDURANCE_TEST", "  初始错误率: %lu", result->baseline.error_rate);
#endif
    
    return ENDURANCE_TEST_OK;
}

/**
 * @brief 执行单次P/E循环
 * @param[in,out] result 测试结果结构体指针
 * @return Endurance_Test_Status_t 错误码
 */
Endurance_Test_Status_t EnduranceTest_RunSingleCycle(Endurance_Test_Result_t *result)
{
    const w25q_dev_t *dev_info;
    uint64_t capacity_bytes;
    uint32_t total_pages;
    uint32_t total_blocks;
    uint32_t page_addr;
    W25Q_Status_t w25q_status;
    uint32_t start_cycles, end_cycles;
    float erase_time_ms = 0.0f;
    float erase_time_per_block_ms = 0.0f;
    float program_time_sum = 0.0f;
    uint32_t program_count = 0;
    uint32_t erase_fail_count = 0;
    uint32_t program_fail_count = 0;
    uint32_t verify_errors_cycle = 0;
    uint32_t bit_errors = 0;
    
    if (result == NULL)
    {
        return ENDURANCE_TEST_ERROR_NULL_PTR;
    }
    
    if (!g_endurance_test_initialized)
    {
        return ENDURANCE_TEST_ERROR_NOT_INIT;
    }
    
    /* 获取设备信息 */
    dev_info = W25Q_GetInfo();
    if (dev_info == NULL)
    {
        return ENDURANCE_TEST_ERROR_W25Q_FAILED;
    }
    
    capacity_bytes = (uint64_t)dev_info->capacity_mb * 1024ULL * 1024ULL;
    total_pages = (uint32_t)(capacity_bytes / W25Q_PAGE_SIZE);
    total_blocks = (uint32_t)(capacity_bytes / W25Q_BLOCK_SIZE);
    
    /* 注意：current_cycle 在循环开始时递增，用于标识当前循环编号 */
    result->current_cycle++;
    
#if CONFIG_MODULE_LOG_ENABLED
    if (result->current_cycle % 100 == 0 || result->current_cycle <= 10)
    {
        LOG_INFO("ENDURANCE_TEST", "=== 第 %lu 次P/E循环 ===", result->current_cycle);
    }
    else
    {
        /* 即使不输出详细日志，也输出循环号 */
        LOG_INFO("ENDURANCE_TEST", "[Cycle %lu] 开始...", result->current_cycle);
    }
#endif
    
    /* ========== 步骤1：全片擦除 ========== */
#if CONFIG_MODULE_LOG_ENABLED
    /* 动态计算预计时间：Block数量 × 150ms/块（典型值） */
    float estimated_erase_time_sec = 0.0f;
    if (dev_info != NULL && total_blocks > 0)
    {
        estimated_erase_time_sec = (float)total_blocks * 0.15f;  /* 150ms/块 = 0.15秒/块 */
    }
    LOG_INFO("ENDURANCE_TEST", "[Cycle %lu] [Erase] 开始全片擦除（%lu MB，%lu 块，预计%.1f-%.1f秒）...", 
             result->current_cycle, dev_info ? dev_info->capacity_mb : 0, total_blocks,
             estimated_erase_time_sec * 0.8f, estimated_erase_time_sec * 1.2f);
#endif
    start_cycles = DWT->CYCCNT;
    w25q_status = W25Q_EraseChip();
    end_cycles = DWT->CYCCNT;
    
    if (w25q_status != W25Q_OK)
    {
        erase_fail_count++;
        result->erase_errors++;
        
        if (erase_fail_count >= EOL_ERASE_FAIL_COUNT)
        {
            result->chip_dead = 1;
            result->chip_status = CHIP_STATUS_DEAD;
            return ENDURANCE_TEST_ERROR_CHIP_DEAD;
        }
        
        return ENDURANCE_TEST_ERROR_W25Q_FAILED;
    }
    
    /* 等待擦除完成（根据芯片容量动态计算超时时间） */
    uint32_t erase_timeout_ms = 60000;  /* 默认60秒 */
    if (dev_info != NULL && total_blocks > 0)
    {
        /* 超时时间 = Block数量 × 200ms/块 × 3（安全系数） */
        erase_timeout_ms = total_blocks * 200 * 3;
        /* 最小60秒，最大600秒（10分钟） */
        if (erase_timeout_ms < 60000) erase_timeout_ms = 60000;
        if (erase_timeout_ms > 600000) erase_timeout_ms = 600000;
    }
    w25q_status = W25Q_WaitReady(erase_timeout_ms);
    if (w25q_status != W25Q_OK)
    {
        erase_fail_count++;
        result->erase_errors++;
#if CONFIG_MODULE_LOG_ENABLED
        LOG_ERROR("ENDURANCE_TEST", "[Cycle %lu] [Erase] 擦除超时！", result->current_cycle);
#endif
        return ENDURANCE_TEST_ERROR_TIMEOUT;
    }
    
    erase_time_ms = DWT_GetElapsedMs(start_cycles, end_cycles);
    result->erase_time_current = erase_time_ms;
#if CONFIG_MODULE_LOG_ENABLED
    /* 计算每块平均擦除时间 */
    if (total_blocks > 0)
    {
        erase_time_per_block_ms = erase_time_ms / (float)total_blocks;
    }
    LOG_INFO("ENDURANCE_TEST", "[Cycle %lu] [Erase] 擦除完成，耗时: %.2f 秒 (%.2f ms/块)", 
             result->current_cycle, erase_time_ms / 1000.0f, erase_time_per_block_ms);
#endif
    
    /* 更新擦除时间统计 */
    if (result->erase_time_min == 0.0f || erase_time_ms < result->erase_time_min)
    {
        result->erase_time_min = erase_time_ms;
    }
    if (erase_time_ms > result->erase_time_max)
    {
        result->erase_time_max = erase_time_ms;
    }
    /* 更新平均擦除时间（避免除以0） */
    if (result->total_cycles == 0)
    {
        result->erase_time_avg = erase_time_ms;
    }
    else
    {
        result->erase_time_avg = (result->erase_time_avg * (result->total_cycles - 1) + erase_time_ms) / result->total_cycles;
    }
    
    /* ========== 步骤2：全片写入固定值数据 ========== */
    /* 生成固定值数据（全盘相同值，使用循环编号，每轮+1） */
    GeneratePattern(s_test_buffer, W25Q_PAGE_SIZE, result->current_cycle);
    
    /* 获取当前写入的值（固定值，每轮循环+1） */
    uint8_t write_value = (uint8_t)(result->current_cycle & 0xFF);
    
#if CONFIG_MODULE_LOG_ENABLED
    /* 动态计算预计时间：页数 × 1.5ms/页（典型值） */
    float estimated_write_time_min = 0.0f;
    if (total_pages > 0)
    {
        estimated_write_time_min = ((float)total_pages * 1.5f) / 60000.0f;  /* 转换为分钟 */
    }
    LOG_INFO("ENDURANCE_TEST", "[Cycle %lu] [Write] 开始全片写入（%lu 页，预计%.1f-%.1f分钟）...", 
             result->current_cycle, total_pages, estimated_write_time_min * 0.8f, estimated_write_time_min * 1.2f);
    LOG_INFO("ENDURANCE_TEST", "[Cycle %lu] [Write] 写入固定值: 0x%02X (十进制: %u)", 
             result->current_cycle, write_value, write_value);
    uint32_t write_progress_step = total_pages / 10;  /* 每10%输出一次进度 */
    if (write_progress_step == 0) write_progress_step = 1;
    uint32_t write_progress_count = 0;
#endif
    
    for (page_addr = 0; page_addr < capacity_bytes; page_addr += W25Q_PAGE_SIZE)
    {
        /* 测量编程时间 */
        start_cycles = DWT->CYCCNT;
        w25q_status = W25Q_Write(page_addr, s_test_buffer, W25Q_PAGE_SIZE);
        end_cycles = DWT->CYCCNT;
        
        if (w25q_status != W25Q_OK)
        {
            program_fail_count++;
            result->program_errors++;
            
            if (program_fail_count >= EOL_PROGRAM_FAIL_COUNT)
            {
                result->chip_dead = 1;
                result->chip_status = CHIP_STATUS_DEAD;
                return ENDURANCE_TEST_ERROR_CHIP_DEAD;
            }
            
            continue;
        }
        
        /* 等待编程完成 */
        w25q_status = W25Q_WaitReady(2000);
        if (w25q_status != W25Q_OK)
        {
            program_fail_count++;
            result->program_errors++;
            continue;
        }
        
        program_time_sum += DWT_GetElapsedMs(start_cycles, end_cycles);
        program_count++;
        
#if CONFIG_MODULE_LOG_ENABLED
        /* 每10%输出一次进度 */
        write_progress_count++;
        if (write_progress_count >= write_progress_step)
        {
            uint32_t progress_percent = (program_count * 100) / total_pages;
            LOG_INFO("ENDURANCE_TEST", "[Cycle %lu] [Write] 写入进度: %lu/%lu 页 (%lu%%)", 
                     result->current_cycle, program_count, total_pages, progress_percent);
            write_progress_count = 0;
        }
#endif
    }
    
#if CONFIG_MODULE_LOG_ENABLED
    if (program_count > 0)
    {
        float program_time_avg = program_time_sum / program_count;
        LOG_INFO("ENDURANCE_TEST", "[Cycle %lu] [Write] 写入完成，平均编程时间: %.3f ms/页", 
                 result->current_cycle, program_time_avg);
    }
#endif
    
    if (program_count > 0)
    {
        float program_time_avg = program_time_sum / program_count;
        /* 更新平均编程时间（避免除以0） */
        if (result->total_cycles == 0)
        {
            result->program_time_avg = program_time_avg;
        }
        else
        {
            result->program_time_avg = (result->program_time_avg * (result->total_cycles - 1) + program_time_avg) / result->total_cycles;
        }
    }
    
    /* ========== 步骤3：全片读取并校验 ========== */
    /* 生成期望数据（与写入时相同，使用循环编号，全盘应该都是相同的值） */
    GeneratePattern(s_test_buffer, W25Q_PAGE_SIZE, result->current_cycle);
    
    /* 获取期望读取的值（应该与写入值相同） */
    uint8_t expected_value = (uint8_t)(result->current_cycle & 0xFF);
    
#if CONFIG_MODULE_LOG_ENABLED
    /* 动态计算预计时间：页数 × 1.0ms/页（典型值） */
    float estimated_read_time_min = 0.0f;
    if (total_pages > 0)
    {
        estimated_read_time_min = ((float)total_pages * 1.0f) / 60000.0f;  /* 转换为分钟 */
    }
    LOG_INFO("ENDURANCE_TEST", "[Cycle %lu] [Read] 开始全片读取并校验（%lu 页，预计%.1f-%.1f分钟）...", 
             result->current_cycle, total_pages, estimated_read_time_min * 0.8f, estimated_read_time_min * 1.5f);
    LOG_INFO("ENDURANCE_TEST", "[Cycle %lu] [Read] 期望读取值: 0x%02X (十进制: %u)", 
             result->current_cycle, expected_value, expected_value);
    uint32_t read_progress_step = total_pages / 10;  /* 每10%输出一次进度 */
    if (read_progress_step == 0) read_progress_step = 1;
    uint32_t read_progress_count = 0;
    uint32_t read_page_count = 0;
#endif
    
    for (page_addr = 0; page_addr < capacity_bytes; page_addr += W25Q_PAGE_SIZE)
    {
        /* 读取数据 */
        w25q_status = W25Q_Read(page_addr, s_read_buffer, W25Q_PAGE_SIZE);
        if (w25q_status != W25Q_OK)
        {
            continue;
        }
        
        /* 统计位错误 */
        CountBitErrors(s_test_buffer, s_read_buffer, W25Q_PAGE_SIZE, &bit_errors);
        if (bit_errors > 0)
        {
            verify_errors_cycle += bit_errors;
        }
        
#if CONFIG_MODULE_LOG_ENABLED
        /* 每10%输出一次进度 */
        read_page_count++;
        read_progress_count++;
        if (read_progress_count >= read_progress_step)
        {
            uint32_t progress_percent = (read_page_count * 100) / total_pages;
            LOG_INFO("ENDURANCE_TEST", "[Cycle %lu] [Read] 读取进度: %lu/%lu 页 (%lu%%)", 
                     result->current_cycle, read_page_count, total_pages, progress_percent);
            read_progress_count = 0;
        }
#endif
    }
    
#if CONFIG_MODULE_LOG_ENABLED
    LOG_INFO("ENDURANCE_TEST", "[Cycle %lu] [Read] 读取完成，校验错误: %lu 位", 
             result->current_cycle, verify_errors_cycle);
#endif
    
    result->verify_errors += verify_errors_cycle;
    
    /* 计算误码率 */
    uint32_t total_bits = total_pages * W25Q_PAGE_SIZE * 8;
    if (total_bits > 0)
    {
        result->error_rate = (float)result->verify_errors / (float)total_bits;
    }
    
    /* 更新循环计数 */
    result->total_cycles++;
    
    /* 计算总写入数据量（每次循环写入全片数据） */
    /* 使用函数开始时获取的dev_info和capacity_bytes，确保数据一致性 */
    if (dev_info != NULL && dev_info->capacity_mb > 0 && dev_info->capacity_mb <= 1024)
    {
        /* 直接使用capacity_mb计算，确保类型转换正确 */
        uint32_t cap_mb = dev_info->capacity_mb;
        uint32_t cycles = result->total_cycles;
        /* 强制清零并重新计算，避免累积错误 */
        result->total_data_written_mb = 0ULL;
        result->total_data_written_mb = (uint64_t)cycles * (uint64_t)cap_mb;
#if CONFIG_MODULE_LOG_ENABLED
        if (result->current_cycle <= 3)
        {
            LOG_INFO("ENDURANCE_TEST", "[DEBUG] cycles=%lu, cap_mb=%lu, total_data_written_mb=%llu", 
                     cycles, cap_mb, (unsigned long long)result->total_data_written_mb);
        }
#endif
    }
    else
    {
        /* 如果dev_info无效或值异常，使用capacity_bytes计算 */
        result->total_data_written_mb = 0ULL;
        result->total_data_written_mb = ((uint64_t)result->total_cycles * capacity_bytes) / (1024ULL * 1024ULL);
#if CONFIG_MODULE_LOG_ENABLED
        if (result->current_cycle <= 3)
        {
            LOG_INFO("ENDURANCE_TEST", "[DEBUG] 使用capacity_bytes计算: total_cycles=%lu, capacity_bytes=%llu, total_data_written_mb=%llu", 
                     result->total_cycles, (unsigned long long)capacity_bytes, 
                     (unsigned long long)result->total_data_written_mb);
        }
#endif
    }
    
    /* 计算性能退化率（如果已记录基准数据，且至少完成1次循环） */
    if (result->baseline_recorded && result->total_cycles > 0)
    {
        if (result->baseline.erase_time_avg > 0.0f && result->erase_time_avg > 0.0f)
        {
            result->erase_degradation_rate = ((result->erase_time_avg - result->baseline.erase_time_avg) / result->baseline.erase_time_avg) * 100.0f;
        }
        
        if (result->baseline.program_time_avg > 0.0f && result->program_time_avg > 0.0f)
        {
            result->program_degradation_rate = ((result->program_time_avg - result->baseline.program_time_avg) / result->baseline.program_time_avg) * 100.0f;
        }
        
        /* 计算寿命评分（至少完成1次循环后才计算） */
        EnduranceTest_CalculateLifetimeScore(result);
    }
    
#if CONFIG_MODULE_LOG_ENABLED
    /* 每轮都输出该轮信息和汇总信息 */
    LOG_INFO("ENDURANCE_TEST", "=== 第 %lu 次P/E循环完成 ===", result->current_cycle);
    LOG_INFO("ENDURANCE_TEST", "[本轮信息]");
    /* 计算每块平均擦除时间（复用之前计算的变量） */
    if (total_blocks > 0)
    {
        erase_time_per_block_ms = erase_time_ms / (float)total_blocks;
    }
    LOG_INFO("ENDURANCE_TEST", "  擦除时间: %.2f 秒 (%.2f ms/块)", 
             erase_time_ms / 1000.0f, erase_time_per_block_ms);
    LOG_INFO("ENDURANCE_TEST", "  编程错误: %lu 次", program_fail_count);
    LOG_INFO("ENDURANCE_TEST", "  校验错误: %lu 位", verify_errors_cycle);
    LOG_INFO("ENDURANCE_TEST", "[累计汇总]");
    LOG_INFO("ENDURANCE_TEST", "  总循环数: %lu 次", result->total_cycles);
    LOG_INFO("ENDURANCE_TEST", "  总写入数据: %llu MB (%.2f GB)", 
             (unsigned long long)result->total_data_written_mb, 
             (float)result->total_data_written_mb / 1024.0f);
    LOG_INFO("ENDURANCE_TEST", "  累计擦除错误: %lu 次", result->erase_errors);
    LOG_INFO("ENDURANCE_TEST", "  累计编程错误: %lu 次", result->program_errors);
    LOG_INFO("ENDURANCE_TEST", "  累计校验错误: %lu 位", result->verify_errors);
    /* 计算平均每块擦除时间 */
    float erase_time_per_block_avg_ms = 0.0f;
    if (total_blocks > 0)
    {
        erase_time_per_block_avg_ms = result->erase_time_avg / (float)total_blocks;
    }
    LOG_INFO("ENDURANCE_TEST", "  平均擦除时间: %.2f 秒 (%.2f ms/块)", 
             result->erase_time_avg / 1000.0f, erase_time_per_block_avg_ms);
    LOG_INFO("ENDURANCE_TEST", "  当前误码率: %.2e", result->error_rate);
    if (result->baseline_recorded)
    {
        LOG_INFO("ENDURANCE_TEST", "  擦除时间退化率: %.2f%%", result->erase_degradation_rate);
        LOG_INFO("ENDURANCE_TEST", "  寿命评分: %.2f", result->lifetime_score);
    }
#endif
    
    return ENDURANCE_TEST_OK;
}

/**
 * @brief 检查芯片是否达到报废标准
 * @param[in] result 测试结果结构体指针
 * @param[out] is_dead 是否报废标志（1=报废，0=未报废）
 * @return Endurance_Test_Status_t 错误码
 */
Endurance_Test_Status_t EnduranceTest_CheckEndOfLife(Endurance_Test_Result_t *result, uint8_t *is_dead)
{
    if (result == NULL || is_dead == NULL)
    {
        return ENDURANCE_TEST_ERROR_NULL_PTR;
    }
    
    *is_dead = 0;
    
    /* 硬失效标准检查 */
    
    /* 1. 全片擦除时间超过阈值（根据芯片容量动态计算） */
    const w25q_dev_t *dev_info_eol = W25Q_GetInfo();
    if (dev_info_eol != NULL)
    {
        uint64_t capacity_bytes_eol = (uint64_t)dev_info_eol->capacity_mb * 1024ULL * 1024ULL;
        uint32_t total_blocks_eol = (uint32_t)(capacity_bytes_eol / W25Q_BLOCK_SIZE);
        
        if (total_blocks_eol > 0)
        {
            /* 动态计算阈值：Block数量 × 单块阈值（200ms/块） */
            float dynamic_threshold = (float)total_blocks_eol * EOL_ERASE_TIME_THRESHOLD_MS;
            float erase_time_per_block = result->erase_time_current / (float)total_blocks_eol;
            
            if (result->erase_time_current > dynamic_threshold)
            {
                *is_dead = 1;
                result->chip_status = CHIP_STATUS_DEAD;
#if CONFIG_MODULE_LOG_ENABLED
                LOG_ERROR("ENDURANCE_TEST", "报废判定: 全片擦除时间超过阈值 (%.2f 秒 > %.2f 秒, %.2f ms/块 > %.2f ms/块)", 
                          result->erase_time_current / 1000.0f, dynamic_threshold / 1000.0f,
                          erase_time_per_block, EOL_ERASE_TIME_THRESHOLD_MS);
#endif
                return ENDURANCE_TEST_OK;
            }
        }
        else
        {
            /* 如果无法获取Block数量，使用固定阈值作为后备 */
            if (result->erase_time_current > EOL_CHIP_ERASE_TIME_THRESHOLD_MS)
            {
                *is_dead = 1;
                result->chip_status = CHIP_STATUS_DEAD;
#if CONFIG_MODULE_LOG_ENABLED
                LOG_ERROR("ENDURANCE_TEST", "报废判定: 全片擦除时间超过阈值 (%.2f ms > %.2f ms)", 
                          result->erase_time_current, EOL_CHIP_ERASE_TIME_THRESHOLD_MS);
#endif
                return ENDURANCE_TEST_OK;
            }
        }
    }
    else
    {
        /* 如果无法获取设备信息，使用固定阈值作为后备 */
        if (result->erase_time_current > EOL_CHIP_ERASE_TIME_THRESHOLD_MS)
        {
            *is_dead = 1;
            result->chip_status = CHIP_STATUS_DEAD;
#if CONFIG_MODULE_LOG_ENABLED
            LOG_ERROR("ENDURANCE_TEST", "报废判定: 全片擦除时间超过阈值 (%.2f ms > %.2f ms)", 
                      result->erase_time_current, EOL_CHIP_ERASE_TIME_THRESHOLD_MS);
#endif
            return ENDURANCE_TEST_OK;
        }
    }
    
    /* 2. 坏块率超过阈值 */
    const w25q_dev_t *dev_info = W25Q_GetInfo();
    if (dev_info != NULL)
    {
        uint64_t capacity_bytes = (uint64_t)dev_info->capacity_mb * 1024ULL * 1024ULL;
        uint32_t total_blocks = (uint32_t)(capacity_bytes / W25Q_BLOCK_SIZE);
        if (total_blocks > 0)
        {
            float bad_block_rate = ((float)result->bad_block_count / (float)total_blocks) * 100.0f;
            if (bad_block_rate > EOL_BAD_BLOCK_RATE_THRESHOLD)
            {
                *is_dead = 1;
                result->chip_status = CHIP_STATUS_DEAD;
#if CONFIG_MODULE_LOG_ENABLED
                LOG_ERROR("ENDURANCE_TEST", "报废判定: 坏块率超过阈值 (%.2f%% > %.2f%%)", 
                          bad_block_rate, EOL_BAD_BLOCK_RATE_THRESHOLD);
#endif
                return ENDURANCE_TEST_OK;
            }
        }
    }
    
    /* 3. 误码率超过阈值 */
    if (result->error_rate > EOL_ERROR_RATE_THRESHOLD)
    {
        *is_dead = 1;
        result->chip_status = CHIP_STATUS_DEAD;
#if CONFIG_MODULE_LOG_ENABLED
        LOG_ERROR("ENDURANCE_TEST", "报废判定: 误码率超过阈值 (%.2e > %.2e)", 
                  result->error_rate, EOL_ERROR_RATE_THRESHOLD);
#endif
        return ENDURANCE_TEST_OK;
    }
    
    /* 4. 读干扰错误超过阈值 */
    if (result->read_disturb_errors > EOL_READ_DISTURB_THRESHOLD)
    {
        *is_dead = 1;
        result->chip_status = CHIP_STATUS_DEAD;
#if CONFIG_MODULE_LOG_ENABLED
        LOG_ERROR("ENDURANCE_TEST", "报废判定: 读干扰错误超过阈值 (%lu > %d)", 
                  result->read_disturb_errors, EOL_READ_DISTURB_THRESHOLD);
#endif
        return ENDURANCE_TEST_OK;
    }
    
    /* 软失效标准检查（性能退化） */
    if (result->baseline_recorded)
    {
        /* 计算退化率 */
        if (result->baseline.erase_time_avg > 0.0f)
        {
            result->erase_degradation_rate = ((result->erase_time_avg - result->baseline.erase_time_avg) / result->baseline.erase_time_avg) * 100.0f;
        }
        
        if (result->baseline.program_time_avg > 0.0f)
        {
            result->program_degradation_rate = ((result->program_time_avg - result->baseline.program_time_avg) / result->baseline.program_time_avg) * 100.0f;
        }
        
        /* 计算寿命评分 */
        EnduranceTest_CalculateLifetimeScore(result);
        
        /* 如果寿命评分过低，判定为报废 */
        if (result->lifetime_score < 20.0f)
        {
            *is_dead = 1;
            result->chip_status = CHIP_STATUS_DEAD;
#if CONFIG_MODULE_LOG_ENABLED
            LOG_ERROR("ENDURANCE_TEST", "报废判定: 寿命评分过低 (%.2f < 20.0)", result->lifetime_score);
#endif
            return ENDURANCE_TEST_OK;
        }
    }
    
    return ENDURANCE_TEST_OK;
}

/**
 * @brief 计算寿命评分
 * @param[in,out] result 测试结果结构体指针
 * @return Endurance_Test_Status_t 错误码
 */
Endurance_Test_Status_t EnduranceTest_CalculateLifetimeScore(Endurance_Test_Result_t *result)
{
    float score = 100.0f;
    
    if (result == NULL)
    {
        return ENDURANCE_TEST_ERROR_NULL_PTR;
    }
    
    if (!result->baseline_recorded)
    {
        result->lifetime_score = score;
        return ENDURANCE_TEST_OK;
    }
    
    /* 擦除时间退化扣分（30%权重） */
    if (result->erase_degradation_rate > 0.0f)
    {
        /* 限制退化率扣分，避免异常值导致评分过低 */
        float degradation_penalty = result->erase_degradation_rate * 0.3f;
        if (degradation_penalty > 50.0f)  /* 最多扣50分 */
        {
            degradation_penalty = 50.0f;
        }
        score -= degradation_penalty;
    }
    
    /* 误码率扣分（30%权重） */
    if (result->error_rate > 0.0f)
    {
        score -= result->error_rate * 30000.0f;  /* 放大系数 */
    }
    
    /* 坏块率扣分（20%权重） */
    const w25q_dev_t *dev_info = W25Q_GetInfo();
    if (dev_info != NULL)
    {
        uint64_t capacity_bytes = (uint64_t)dev_info->capacity_mb * 1024ULL * 1024ULL;
        uint32_t total_blocks = (uint32_t)(capacity_bytes / W25Q_BLOCK_SIZE);
        if (total_blocks > 0)
        {
            float bad_block_rate = ((float)result->bad_block_count / (float)total_blocks) * 100.0f;
            score -= bad_block_rate * 0.2f;
        }
    }
    
    /* 读干扰扣分（20%权重） */
    if (result->read_disturb_errors > 0)
    {
        score -= (float)result->read_disturb_errors * 2.0f;
    }
    
    /* 限制分数范围 */
    if (score < 0.0f)
    {
        score = 0.0f;
    }
    if (score > 100.0f)
    {
        score = 100.0f;
    }
    
    result->lifetime_score = score;
    
    /* 根据评分更新芯片状态 */
    if (score >= 70.0f)
    {
        result->chip_status = CHIP_STATUS_NORMAL;
    }
    else if (score >= 50.0f)
    {
        result->chip_status = CHIP_STATUS_WARNING;
    }
    else if (score >= 20.0f)
    {
        result->chip_status = CHIP_STATUS_DANGER;
    }
    else
    {
        result->chip_status = CHIP_STATUS_DEAD;
    }
    
    return ENDURANCE_TEST_OK;
}

/**
 * @brief 执行深度健康检查
 * @param[in,out] result 测试结果结构体指针
 * @return Endurance_Test_Status_t 错误码
 */
Endurance_Test_Status_t EnduranceTest_DeepHealthCheck(Endurance_Test_Result_t *result)
{
    if (result == NULL)
    {
        return ENDURANCE_TEST_ERROR_NULL_PTR;
    }
    
#if CONFIG_MODULE_LOG_ENABLED
    LOG_INFO("ENDURANCE_TEST", "=== 深度健康检查（第 %lu 次循环） ===", result->current_cycle);
#endif
    
    /* 计算性能退化率 */
    if (result->baseline_recorded)
    {
        if (result->baseline.erase_time_avg > 0.0f)
        {
            result->erase_degradation_rate = ((result->erase_time_avg - result->baseline.erase_time_avg) / result->baseline.erase_time_avg) * 100.0f;
        }
        
        if (result->baseline.program_time_avg > 0.0f)
        {
            result->program_degradation_rate = ((result->program_time_avg - result->baseline.program_time_avg) / result->baseline.program_time_avg) * 100.0f;
        }
        
#if CONFIG_MODULE_LOG_ENABLED
        LOG_INFO("ENDURANCE_TEST", "擦除时间退化率: %.2f%%", result->erase_degradation_rate);
        LOG_INFO("ENDURANCE_TEST", "编程时间退化率: %.2f%%", result->program_degradation_rate);
        
        if (result->erase_degradation_rate > DEGRADATION_DANGER_RATE)
        {
            LOG_WARN("ENDURANCE_TEST", "警告: 擦除时间退化超过危险阈值 (%.2f%% > %.2f%%)", 
                     result->erase_degradation_rate, DEGRADATION_DANGER_RATE);
        }
        else if (result->erase_degradation_rate > DEGRADATION_WARNING_RATE)
        {
            LOG_WARN("ENDURANCE_TEST", "警告: 擦除时间退化超过预警阈值 (%.2f%% > %.2f%%)", 
                     result->erase_degradation_rate, DEGRADATION_WARNING_RATE);
        }
#endif
    }
    
    /* 计算寿命评分 */
    EnduranceTest_CalculateLifetimeScore(result);
    
#if CONFIG_MODULE_LOG_ENABLED
    LOG_INFO("ENDURANCE_TEST", "寿命评分: %.2f", result->lifetime_score);
    LOG_INFO("ENDURANCE_TEST", "芯片状态: %d", result->chip_status);
    LOG_INFO("ENDURANCE_TEST", "误码率: %.2e", result->error_rate);
    LOG_INFO("ENDURANCE_TEST", "坏块数: %lu", result->bad_block_count);
#endif
    
    return ENDURANCE_TEST_OK;
}

/**
 * @brief 运行完整寿命测试流程（测到报废）
 * @param[in] config 测试配置指针
 * @param[out] result 测试结果结构体指针
 * @return Endurance_Test_Status_t 错误码
 */
Endurance_Test_Status_t EnduranceTest_Run(Endurance_Test_Config_t *config, Endurance_Test_Result_t *result)
{
    Endurance_Test_Status_t test_status;
    uint8_t is_dead = 0;
    
    if (config == NULL || result == NULL)
    {
        return ENDURANCE_TEST_ERROR_NULL_PTR;
    }
    
    if (!g_endurance_test_initialized)
    {
        return ENDURANCE_TEST_ERROR_NOT_INIT;
    }
    
    /* 参数校验 */
    if (config->deep_check_interval == 0)
    {
        config->deep_check_interval = 1000;  /* 默认每1000次循环 */
    }
    if (config->log_interval == 0)
    {
        config->log_interval = 100;  /* 默认每100次循环 */
    }
    
    /* 初始化结果结构体 */
    memset(result, 0, sizeof(Endurance_Test_Result_t));
    result->chip_status = CHIP_STATUS_NORMAL;
    
#if CONFIG_MODULE_LOG_ENABLED
    LOG_INFO("ENDURANCE_TEST", "=== 开始正式寿命测试流程（测到报废） ===");
    LOG_INFO("ENDURANCE_TEST", "深度检查间隔: %lu 次循环", config->deep_check_interval);
    LOG_INFO("ENDURANCE_TEST", "日志记录间隔: %lu 次循环", config->log_interval);
#endif
    
    /* ========== 步骤1：记录基准数据 ========== */
    test_status = EnduranceTest_RecordBaseline(result);
    if (test_status != ENDURANCE_TEST_OK)
    {
#if CONFIG_MODULE_LOG_ENABLED
        LOG_WARN("ENDURANCE_TEST", "基准数据记录失败，继续测试");
#endif
    }
    
    /* ========== 步骤2：主循环：持续P/E循环直至报废 ========== */
    while (!result->chip_dead)
    {
        /* 执行单次P/E循环 */
        test_status = EnduranceTest_RunSingleCycle(result);
        if (test_status == ENDURANCE_TEST_ERROR_CHIP_DEAD)
        {
            result->chip_dead = 1;
            break;
        }
        
        /* 定期深度健康检查 */
        if (result->current_cycle % config->deep_check_interval == 0)
        {
            EnduranceTest_DeepHealthCheck(result);
        }
        
        /* 检查是否达到报废标准 */
        EnduranceTest_CheckEndOfLife(result, &is_dead);
        if (is_dead)
        {
            result->chip_dead = 1;
            break;
        }
        
        /* 短暂延时，避免过热 */
        Delay_ms(10);
    }
    
#if CONFIG_MODULE_LOG_ENABLED
    LOG_INFO("ENDURANCE_TEST", "========================================");
    LOG_INFO("ENDURANCE_TEST", "=== 寿命测试完成（芯片已报废） ===");
    LOG_INFO("ENDURANCE_TEST", "========================================");
    LOG_INFO("ENDURANCE_TEST", "");
    LOG_INFO("ENDURANCE_TEST", "【关键数据 - 总写入数据量】");
    LOG_INFO("ENDURANCE_TEST", "  总P/E循环次数: %lu 次", result->total_cycles);
    LOG_INFO("ENDURANCE_TEST", "  总写入数据量: %llu MB (%.2f GB)", 
             (unsigned long long)result->total_data_written_mb, 
             (float)result->total_data_written_mb / 1024.0f);
    LOG_INFO("ENDURANCE_TEST", "");
    LOG_INFO("ENDURANCE_TEST", "【时间统计】");
    /* 计算每块平均擦除时间（使用最终擦除时间） */
    const w25q_dev_t *dev_info_final = W25Q_GetInfo();
    uint32_t total_blocks_final = 0;
    if (dev_info_final != NULL)
    {
        uint64_t capacity_bytes_final = (uint64_t)dev_info_final->capacity_mb * 1024ULL * 1024ULL;
        total_blocks_final = (uint32_t)(capacity_bytes_final / W25Q_BLOCK_SIZE);
    }
    float erase_time_per_block_current_ms = 0.0f;
    float erase_time_per_block_avg_ms = 0.0f;
    if (total_blocks_final > 0)
    {
        erase_time_per_block_current_ms = result->erase_time_current / (float)total_blocks_final;
        erase_time_per_block_avg_ms = result->erase_time_avg / (float)total_blocks_final;
    }
    LOG_INFO("ENDURANCE_TEST", "  最终擦除时间: %.2f 秒 (%.2f ms/块)", 
             result->erase_time_current / 1000.0f, erase_time_per_block_current_ms);
    LOG_INFO("ENDURANCE_TEST", "  平均擦除时间: %.2f 秒 (%.2f ms/块)", 
             result->erase_time_avg / 1000.0f, erase_time_per_block_avg_ms);
    LOG_INFO("ENDURANCE_TEST", "  最小擦除时间: %.2f 秒", result->erase_time_min / 1000.0f);
    LOG_INFO("ENDURANCE_TEST", "  最大擦除时间: %.2f 秒", result->erase_time_max / 1000.0f);
    LOG_INFO("ENDURANCE_TEST", "  平均编程时间: %.3f ms/页", result->program_time_avg);
    LOG_INFO("ENDURANCE_TEST", "");
    LOG_INFO("ENDURANCE_TEST", "【错误统计】");
    LOG_INFO("ENDURANCE_TEST", "  擦除错误次数: %lu", result->erase_errors);
    LOG_INFO("ENDURANCE_TEST", "  编程错误次数: %lu", result->program_errors);
    LOG_INFO("ENDURANCE_TEST", "  校验错误次数: %lu 位", result->verify_errors);
    LOG_INFO("ENDURANCE_TEST", "  坏块数量: %lu", result->bad_block_count);
    LOG_INFO("ENDURANCE_TEST", "  最终误码率: %.2e", result->error_rate);
    LOG_INFO("ENDURANCE_TEST", "  读干扰错误: %lu", result->read_disturb_errors);
    LOG_INFO("ENDURANCE_TEST", "");
    LOG_INFO("ENDURANCE_TEST", "【性能退化】");
    if (result->baseline_recorded)
    {
        LOG_INFO("ENDURANCE_TEST", "  擦除时间退化率: %.2f%%", result->erase_degradation_rate);
        LOG_INFO("ENDURANCE_TEST", "  编程时间退化率: %.2f%%", result->program_degradation_rate);
        LOG_INFO("ENDURANCE_TEST", "  基准擦除时间: %.2f 秒", result->baseline.erase_time_avg / 1000.0f);
        LOG_INFO("ENDURANCE_TEST", "  基准编程时间: %.3f ms/页", result->baseline.program_time_avg);
    }
    LOG_INFO("ENDURANCE_TEST", "");
    LOG_INFO("ENDURANCE_TEST", "【最终状态】");
    LOG_INFO("ENDURANCE_TEST", "  最终寿命评分: %.2f", result->lifetime_score);
    LOG_INFO("ENDURANCE_TEST", "  芯片状态: %s", result->chip_dead ? "已报废" : "正常");
    LOG_INFO("ENDURANCE_TEST", "");
    LOG_INFO("ENDURANCE_TEST", "========================================");
#endif
    
    return ENDURANCE_TEST_OK;
}

