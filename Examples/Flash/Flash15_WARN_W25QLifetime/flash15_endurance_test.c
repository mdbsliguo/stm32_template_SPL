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
#include <stdio.h>

/* 从 board.h 获取误码率阈值配置 */
#ifndef ENDURANCE_TEST_ERROR_RATE_THRESHOLD
#define ENDURANCE_TEST_ERROR_RATE_THRESHOLD  1e-3f  /* 默认值：一般应用标准 */
#endif

#if CONFIG_MODULE_LOG_ENABLED
#include "log.h"
#endif

#if CONFIG_MODULE_OLED_ENABLED
#include "oled_ssd1306.h"
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

/* ==================== 模拟写入错误 ==================== */
/* 
 * ?? 警告：此功能仅用于验证代码逻辑，正常测试时请勿开启！
 * 
 * 模拟写入错误功能会在主测试中随机选择指定数量的字节地址，故意写入错误数据（0xFF），
 * 用于验证读取验证功能是否能正确检测到错误，并验证误码率计算是否正确。
 * 在正常寿命测试中，应该在 board.h 中将 ENDURANCE_TEST_SIMULATE_WRITE_ERROR_ENABLED 设置为 0 来禁用此功能。
 */

/* 模拟写入错误：记录随机选择的字节地址，用于在写入时写入0xFF（模拟写入失败） */
/* 数组大小由配置项 ENDURANCE_TEST_SIMULATE_WRITE_ERROR_COUNT 控制，最大10 */
#define MAX_SIMULATE_ERROR_COUNT  10
static uint32_t s_simulate_write_error_addrs[MAX_SIMULATE_ERROR_COUNT] = {0};  /* 模拟写入错误的字节地址数组 */
static uint8_t s_simulate_write_error_count = 0;  /* 模拟写入错误的字节数（0或配置值） */

/**
 * @brief 模拟写入错误：随机选择指定数量的字节地址，用于测试读取验证功能
 * @warning ?? 此功能仅用于验证代码逻辑，正常测试时请勿开启！应在 board.h 中设置为 0。
 * @note 在主测试写入循环中，这些字节地址会被写入0xFF（而不是循环编号的值），
 *       用于模拟写入失败的情况，测试读取验证是否能检测到数据错误
 * @note 功能是否启用和数量由 board.h 中的配置项控制：
 *       - ENDURANCE_TEST_SIMULATE_WRITE_ERROR_ENABLED：功能开关（1=启用仅验证代码，0=禁用正常测试）
 *       - ENDURANCE_TEST_SIMULATE_WRITE_ERROR_COUNT：字节数量（1-10，默认3）
 */
static void EnduranceTest_SimulateWriteError(void)
{
#if ENDURANCE_TEST_SIMULATE_WRITE_ERROR_ENABLED
    uint32_t block1_start = W25Q_BLOCK_SIZE;  /* Block 1起始地址：0x00010000 */
    uint32_t block1_end = block1_start + W25Q_BLOCK_SIZE;  /* Block 1结束地址：0x00020000 */
    uint8_t error_count = ENDURANCE_TEST_SIMULATE_WRITE_ERROR_COUNT;
    
    /* 限制数量范围：1-10 */
    if (error_count == 0 || error_count > MAX_SIMULATE_ERROR_COUNT)
    {
        error_count = 3;  /* 默认值 */
    }
    
    /* 清零数组 */
    memset(s_simulate_write_error_addrs, 0, sizeof(s_simulate_write_error_addrs));
    s_simulate_write_error_count = 0;
    
    /* 随机选择指定数量的字节地址（使用简单的伪随机算法，基于当前循环计数） */
    static uint32_t seed = 0;
    seed++;
    
    for (uint8_t i = 0; i < error_count; i++)
    {
        uint32_t addr;
        uint8_t retry = 0;
        
        do
        {
            /* 生成随机字节地址（在Block 1范围内，任意字节地址） */
            addr = block1_start + ((seed * (12345 + i * 1000) + (6789 + i * 100)) % W25Q_BLOCK_SIZE);
            
            /* 确保地址在Block 1范围内 */
            if (addr >= block1_end)
            {
                addr = block1_start + (addr % W25Q_BLOCK_SIZE);
            }
            
            /* 检查是否与已有地址重复 */
            uint8_t duplicate = 0;
            for (uint8_t j = 0; j < s_simulate_write_error_count; j++)
            {
                if (s_simulate_write_error_addrs[j] == addr)
                {
                    duplicate = 1;
                    break;
                }
            }
            
            if (!duplicate)
            {
                break;  /* 地址不重复，退出循环 */
            }
            
            retry++;
            seed++;  /* 改变种子，生成新地址 */
        } while (retry < 100);  /* 最多重试100次 */
        
        /* 记录字节地址 */
        s_simulate_write_error_addrs[s_simulate_write_error_count] = addr;
        s_simulate_write_error_count++;
    }
    
#if CONFIG_MODULE_LOG_ENABLED
    if (s_simulate_write_error_count > 0)
    {
        LOG_INFO("ENDURANCE_TEST", "[模拟写入错误] 随机选择 %u 个字节地址用于模拟写入失败:", s_simulate_write_error_count);
        for (uint8_t i = 0; i < s_simulate_write_error_count; i++)
        {
            LOG_INFO("ENDURANCE_TEST", "[模拟写入错误]   地址 %u: 0x%06lX", i + 1, (unsigned long)s_simulate_write_error_addrs[i]);
        }
    }
#endif
#else
    /* 功能被禁用，清零数组 */
    memset(s_simulate_write_error_addrs, 0, sizeof(s_simulate_write_error_addrs));
    s_simulate_write_error_count = 0;
#endif
}

/* 报废判定阈值 */
#define EOL_ERASE_TIME_THRESHOLD_MS     500.0f    /**< 单个Block擦除时间阈值（毫秒），>500ms为报废（对应性能档位"偏弱"档位） */
#define EOL_CHIP_ERASE_TIME_THRESHOLD_MS 120000.0f /**< 全片擦除时间阈值（毫秒），>120秒为报废 */
#define EOL_BAD_BLOCK_RATE_THRESHOLD    5.0f      /**< 坏块率阈值（%），>5%为报废 */
#define EOL_ERROR_RATE_THRESHOLD        ENDURANCE_TEST_ERROR_RATE_THRESHOLD  /**< 误码率阈值，从 board.h 配置项获取，默认1e-3（0.1%） */
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
 * @brief 格式化误码率为易读格式
 * @param[in] error_rate 误码率（0.0~1.0之间的浮点数）
 * @param[out] buffer 输出缓冲区
 * @param[in] buffer_size 缓冲区大小
 * @return 格式化后的字符串指针
 * @note 格式说明：
 *   - >= 0.01 (1%)：显示为百分比，如 "1.23%"
 *   - >= 0.0001 (0.01%)：显示为百分比，如 "0.01%"
 *   - >= 0.000001 (0.0001%)：显示为百分比，如 "0.0001%"
 *   - >= 1e-6：显示为 PPM（百万分之一），如 "901 PPM"（整数格式）
 *   - < 1e-6：显示为 PPB（十亿分之一），如 "315 PPB"（整数格式）
 */
static const char* FormatErrorRate(float error_rate, char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0)
    {
        return "N/A";
    }
    
    if (error_rate < 0.0f)
    {
        error_rate = 0.0f;
    }
    
    if (error_rate >= 0.01f)
    {
        /* >= 1%，显示为百分比，保留2位小数 */
        snprintf(buffer, buffer_size, "%.2f%%", error_rate * 100.0f);
    }
    else if (error_rate >= 0.0001f)
    {
        /* >= 0.01%，显示为百分比，保留4位小数 */
        snprintf(buffer, buffer_size, "%.4f%%", error_rate * 100.0f);
    }
    else if (error_rate >= 0.000001f)
    {
        /* >= 0.0001%，显示为百分比，保留6位小数 */
        snprintf(buffer, buffer_size, "%.6f%%", error_rate * 100.0f);
    }
    else if (error_rate >= 1e-6f)
    {
        /* >= 1 PPM，显示为 PPM（百万分之一），整数格式 */
        snprintf(buffer, buffer_size, "%.0f PPM", error_rate * 1000000.0f);
    }
    else if (error_rate >= 1e-9f)
    {
        /* >= 1 PPB，显示为 PPB（十亿分之一），整数格式 */
        snprintf(buffer, buffer_size, "%.0f PPB", error_rate * 1000000000.0f);
    }
    else if (error_rate > 0.0f)
    {
        /* < 1 PPB，显示为 PPB，整数格式（四舍五入） */
        snprintf(buffer, buffer_size, "%.0f PPB", error_rate * 1000000000.0f);
    }
    else
    {
        /* 0 或非常接近0 */
        snprintf(buffer, buffer_size, "0.00");
    }
    
    return buffer;
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
    if (result == NULL)
    {
        return ENDURANCE_TEST_ERROR_NULL_PTR;
    }
    
    if (!g_endurance_test_initialized)
    {
        return ENDURANCE_TEST_ERROR_NOT_INIT;
    }
    
    /* 初始化基准数据为0，将在第1轮循环完成后更新 */
    result->baseline.erase_time_avg = 0.0f;
    result->baseline.program_time_avg = 0.0f;
    result->baseline.read_speed = 0.0f;
    result->baseline.error_rate = 0.0f;
    result->baseline.unique_id = 0;  /* 未实现Unique ID读取 */
    
    result->baseline_recorded = 1;  /* 标记基准数据已准备，将在第1轮循环后更新 */
    
#if CONFIG_MODULE_LOG_ENABLED
    LOG_INFO("ENDURANCE_TEST", "基准数据已初始化（数据将在第1轮循环后记录）");
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
    float read_time_sum = 0.0f;
    uint32_t program_count = 0;
    uint32_t read_count = 0;
    uint32_t erase_fail_count = 0;
    uint32_t program_fail_count = 0;
    uint32_t verify_errors_cycle = 0;
#if !ENDURANCE_TEST_SIMULATE_WRITE_ERROR_ENABLED
    uint32_t bit_errors = 0;  /* 仅在非模拟错误模式下使用 */
#endif
    
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
        LOG_INFO("ENDURANCE_TEST", "=== 第 %u 次P/E循环 ===", result->current_cycle);
    }
    else
    {
        /* 即使不输出详细日志，也输出循环号 */
        LOG_INFO("ENDURANCE_TEST", "[Cycle %u] 开始...", result->current_cycle);
    }
#endif

    /* 更新OLED显示（循环开始时显示初始信息） */
#if CONFIG_MODULE_OLED_ENABLED
    {
        char oled_line1[17] = {0};
        char oled_line2[17] = {0};
        char oled_line3[17] = {0};
        char oled_line4[17] = {0};
        float total_gb = (float)result->total_data_written_mb / 1024.0f;
        
        /* 第1行：显示测试状态（在误码率计算完成后显示，此时已完成一轮完整的E/W/R循环） */
        /* 注意：擦除/写入/读取过程中会动态更新第1行显示，这里显示完成状态 */
        snprintf(oled_line1, sizeof(oled_line1), "W25Q: Done");
        size_t len1 = strlen(oled_line1);
        if (len1 < 16)
        {
            memset(oled_line1 + len1, ' ', 16 - len1);
            oled_line1[16] = '\0';
        }
        OLED_ShowString(1, 1, oled_line1);
        
        /* 第2行：显示连续擦除值，确保字符串长度为16，用空格填充剩余部分（支持x1到x9999） */
        snprintf(oled_line2, sizeof(oled_line2), "Erase: x%u", ENDURANCE_TEST_CONSECUTIVE_ERASE_COUNT);
        size_t len2 = strlen(oled_line2);
        if (len2 >= 16)
        {
            /* 如果字符串太长，截断到16个字符 */
            oled_line2[16] = '\0';
            len2 = 16;
        }
        else
        {
            /* 如果字符串太短，用空格填充到16个字符 */
            memset(oled_line2 + len2, ' ', 16 - len2);
            oled_line2[16] = '\0';
        }
        OLED_ShowString(2, 1, oled_line2);
        
        /* 第3行：显示当前轮次，确保字符串长度为16，用空格填充剩余部分 */
        snprintf(oled_line3, sizeof(oled_line3), "Cycle: %u", result->current_cycle);
        size_t len3 = strlen(oled_line3);
        if (len3 < 16)
        {
            memset(oled_line3 + len3, ' ', 16 - len3);
            oled_line3[16] = '\0';
        }
        OLED_ShowString(3, 1, oled_line3);
        
        /* 第4行：显示当前误码率（使用易读格式），确保字符串长度为16，用空格填充剩余部分 */
        {
            char error_rate_str[17] = {0};
            FormatErrorRate(result->error_rate, error_rate_str, sizeof(error_rate_str));
            /* OLED 显示空间有限，如果字符串太长则截断 */
            if (strlen(error_rate_str) > 15)
            {
                error_rate_str[15] = '\0';
            }
            snprintf(oled_line4, sizeof(oled_line4), "Err: %s", error_rate_str);
            size_t len4 = strlen(oled_line4);
            if (len4 < 16)
            {
                memset(oled_line4 + len4, ' ', 16 - len4);
                oled_line4[16] = '\0';
            }
        }
        OLED_ShowString(4, 1, oled_line4);
    }
#endif
    
    /* ========== 步骤1：全片擦除（然后恢复Block 0的汇总信息） ========== */
#if CONFIG_MODULE_LOG_ENABLED
    /* 动态计算预计时间：Block数量 × 150ms/块（典型值） */
    float estimated_erase_time_sec = 0.0f;
    if (dev_info != NULL && total_blocks > 0)
    {
        estimated_erase_time_sec = (float)total_blocks * 0.15f;  /* 150ms/块 = 0.15秒/块 */
    }
    LOG_INFO("ENDURANCE_TEST", "[Cycle %u] [Erase] 开始全片擦除（%u MB，%u 块，预计%.1f-%.1f秒）...", 
             result->current_cycle, dev_info ? dev_info->capacity_mb : 0, total_blocks,
             estimated_erase_time_sec * 0.8f, estimated_erase_time_sec * 1.2f);
#endif
    
    /* 先保存Block 0的汇总信息（全片擦除会擦除它） */
    Endurance_Test_Summary_t saved_summary;
    w25q_status = W25Q_Read(ENDURANCE_TEST_SUMMARY_ADDR, (uint8_t *)&saved_summary, sizeof(Endurance_Test_Summary_t));
    if (w25q_status != W25Q_OK)
    {
        memset(&saved_summary, 0, sizeof(Endurance_Test_Summary_t));
    }
    
    /* ========== 擦除循环：支持连续擦除配置 ========== */
    /* 第1轮（total_cycles == 0）：不受连续擦除影响，总是执行完整的擦除-写入-读取流程 */
    /* 第2轮开始：根据配置连续擦除多次，每次擦除后更新计数 */
    uint8_t consecutive_erase_count = 1;  /* 默认擦除1次 */
    uint8_t need_write_read = 1;  /* 是否需要执行写入和读取 */
    
    if (result->total_cycles == 0)
    {
        /* 第1轮：不受连续擦除影响 */
        consecutive_erase_count = 1;
        need_write_read = 1;
    }
    else
    {
        /* 第2轮开始：应用连续擦除配置 */
        consecutive_erase_count = ENDURANCE_TEST_CONSECUTIVE_ERASE_COUNT;
        if (consecutive_erase_count == 0 || consecutive_erase_count > 100)
        {
            consecutive_erase_count = 1;  /* 限制范围：1-100 */
        }
        need_write_read = 1;  /* 连续擦除完成后，执行一次写入和读取 */
    }
    
    /* 累加擦除时间（用于连续擦除时计算平均值） */
    float total_erase_time_ms = 0.0f;
    
    /**
     * @brief 连续擦除循环
     * @warning 重要警告：在擦除（E）状态时，严禁断电或重启！
     * @details
     *   - 擦除操作会清除Flash中的数据，如果在擦除过程中断电或重启：
     *     * 可能导致汇总记录（Block 0）丢失，无法恢复测试进度
     *     * 可能导致测试数据不一致，影响测试结果的准确性
     *     * 可能导致芯片状态异常，需要重新开始测试
     *   - 只有在擦除完成、汇总记录已恢复后，才能安全断电或重启
     *   - 建议在擦除操作开始前确保电源稳定，避免意外断电
     */
    for (uint8_t erase_iter = 0; erase_iter < consecutive_erase_count; erase_iter++)
    {
        /* 更新OLED显示：擦除状态（每次擦除迭代开始时更新） */
#if CONFIG_MODULE_OLED_ENABLED
        {
            char oled_line1[17] = {0};
            snprintf(oled_line1, sizeof(oled_line1), "W25Q: Erase...");
            size_t len1 = strlen(oled_line1);
            if (len1 < 16)
            {
                memset(oled_line1 + len1, ' ', 16 - len1);
                oled_line1[16] = '\0';
            }
            OLED_ShowString(1, 1, oled_line1);
        }
#endif
        
        /* 使用系统滴答计时（更准确，因为W25Q_WaitReady内部使用Delay_GetTick） */
        uint32_t start_tick = Delay_GetTick();
        
        /* 执行全片擦除（包括Block 0） */
        /* 警告：此时处于擦除（E）状态，严禁断电或重启！ */
        w25q_status = W25Q_EraseChip();
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
        
        /* 等待全片擦除完成 */
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
            LOG_ERROR("ENDURANCE_TEST", "[Cycle %u] [Erase] 擦除超时！", result->current_cycle);
#endif
            return ENDURANCE_TEST_ERROR_TIMEOUT;
        }
        
        /* 计算本次擦除时间（使用系统滴答，与W25Q_WaitReady内部计时方式一致） */
        uint32_t end_tick = Delay_GetTick();
        float current_erase_time_ms = (float)Delay_GetElapsed(end_tick, start_tick);
        total_erase_time_ms += current_erase_time_ms;
        
        /* 恢复Block 0的汇总信息（如果之前有保存，只在最后一次擦除后恢复） */
        /* 警告：在恢复汇总信息期间，严禁断电或重启！否则汇总记录可能丢失，无法恢复测试进度 */
        if (erase_iter == consecutive_erase_count - 1 && saved_summary.magic == ENDURANCE_TEST_MAGIC)
        {
            /* 擦除Block 0的第一个扇区 */
            /* 警告：此时处于擦除（E）状态，严禁断电或重启！ */
            w25q_status = W25Q_EraseSector(ENDURANCE_TEST_SUMMARY_ADDR);
            if (w25q_status == W25Q_OK)
            {
                w25q_status = W25Q_WaitReady(5000);
                if (w25q_status == W25Q_OK)
                {
                    /* 使用更新后的result来更新汇总信息（而不是直接恢复旧的saved_summary） */
                    /* 这样可以确保轮次信息（total_cycles和current_cycle）是最新的 */
                    Endurance_Test_Summary_t updated_summary;
                    memcpy(&updated_summary, &saved_summary, sizeof(Endurance_Test_Summary_t));
                    
                    /* 更新汇总信息中的完整result数据（使用更新后的result，确保所有字段都是最新的） */
                    /* 注意：这里需要更新完整的result，而不仅仅是轮次信息，因为total_data_written_mb等字段也需要更新 */
                    memcpy(&updated_summary.result, result, sizeof(Endurance_Test_Result_t));
                    
                    /* 确保端粒进度是最新的 */
                    updated_summary.result.telomere_progress = ((float)result->total_cycles / 100000.0f) * 100.0f;
                    
                    /* 恢复汇总信息到Block 0（使用更新后的汇总信息） */
                    /* 警告：此时正在写入汇总记录，严禁断电或重启！否则汇总记录可能丢失 */
                    w25q_status = W25Q_Write(ENDURANCE_TEST_SUMMARY_ADDR, (uint8_t *)&updated_summary, sizeof(Endurance_Test_Summary_t));
                    if (w25q_status == W25Q_OK)
                    {
                        W25Q_WaitReady(2000);  /* 等待写入完成 */
                        /* 汇总记录已恢复完成（包含最新的轮次信息），此时可以安全断电或重启 */
                    }
                }
            }
        }
        
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
        
        /* ========== 擦除完成：立即更新循环计数 ========== */
        /* 注意：轮次计数在每次擦除后更新，而不是在写入和读取后 */
        result->total_cycles++;
        result->current_cycle = result->total_cycles;
        
        /* 更新擦除时间统计（使用当前的total_cycles值，已递增） */
        if (result->total_cycles == 1)
        {
            /* 第1轮：直接设置 */
            result->erase_time_avg = current_erase_time_ms;
            /* 如果是第1轮循环，且基准数据还未写入（首次数据），同时更新基准数据 */
            /* 注意：首次数据只在第1轮循环完成后写入一次，之后不允许修改 */
            if (result->baseline_recorded && result->baseline.erase_time_avg == 0.0f)
            {
                result->baseline.erase_time_avg = current_erase_time_ms;
            }
        }
        else
        {
            /* 第2轮开始：计算平均值 */
            result->erase_time_avg = (result->erase_time_avg * (result->total_cycles - 1) + current_erase_time_ms) / result->total_cycles;
        }
        
#if CONFIG_MODULE_LOG_ENABLED
        /* 计算每块平均擦除时间（排除Block 0，实际擦除了 total_blocks - 1 个块） */
        uint32_t blocks_erased = (total_blocks > 1) ? (total_blocks - 1) : 0;
        float erase_time_per_block_ms = 0.0f;
        if (blocks_erased > 0)
        {
            erase_time_per_block_ms = current_erase_time_ms / (float)blocks_erased;
        }
        
        if (consecutive_erase_count > 1)
        {
            LOG_INFO("ENDURANCE_TEST", "[Cycle %u] [Erase] 连续擦除 %u/%u 完成，耗时: %.2f 秒 (%.2f ms/块，共擦除 %u 块)", 
                     result->current_cycle, erase_iter + 1, consecutive_erase_count,
                     current_erase_time_ms / 1000.0f, erase_time_per_block_ms, blocks_erased);
        }
        else
        {
            LOG_INFO("ENDURANCE_TEST", "[Cycle %u] [Erase] 擦除完成，耗时: %.2f 秒 (%.2f ms/块，共擦除 %u 块)", 
                     result->current_cycle, current_erase_time_ms / 1000.0f, erase_time_per_block_ms, blocks_erased);
        }
#endif
        
        /* 更新OLED显示（每次擦除完成后更新Cycle显示） */
#if CONFIG_MODULE_OLED_ENABLED
        {
            char oled_line3[17] = {0};
            snprintf(oled_line3, sizeof(oled_line3), "Cycle: %u", result->current_cycle);
            size_t len3 = strlen(oled_line3);
            if (len3 < 16)
            {
                memset(oled_line3 + len3, ' ', 16 - len3);
                oled_line3[16] = '\0';
            }
            OLED_ShowString(3, 1, oled_line3);
        }
#endif
    }
    
    /* 计算平均擦除时间（用于连续擦除时的统计） */
    erase_time_ms = total_erase_time_ms / (float)consecutive_erase_count;
    
    /* ========== 步骤2：全片写入固定值数据 ========== */
    /* 注意：只有在需要时才执行写入和读取（连续擦除完成后执行一次） */
    if (!need_write_read)
    {
        /* 如果不需要写入和读取，直接返回（这种情况不应该发生，因为need_write_read总是1） */
        return ENDURANCE_TEST_OK;
    }
    
    /* 生成固定值数据（全盘相同值，使用循环编号，每轮+1） */
    GeneratePattern(s_test_buffer, W25Q_PAGE_SIZE, result->current_cycle);
    
    /* 获取当前写入的值（固定值，每轮循环+1） */
    uint8_t write_value = (uint8_t)(result->current_cycle & 0xFF);
    
#if ENDURANCE_TEST_SIMULATE_WRITE_ERROR_ENABLED
    /* ?? 警告：模拟写入错误功能仅用于验证代码逻辑，正常测试时请勿开启！ */
    /* 准备0xFF数据（用于模拟写入错误测试） */
    uint8_t simulate_error_data[W25Q_PAGE_SIZE];
    memset(simulate_error_data, 0xFF, W25Q_PAGE_SIZE);
    
    /* 初始化模拟写入错误：随机选择指定数量的页地址 */
    EnduranceTest_SimulateWriteError();
#endif
    
#if CONFIG_MODULE_LOG_ENABLED
    /* 动态计算预计时间：页数 × 1.5ms/页（典型值） */
    float estimated_write_time_min = 0.0f;
    if (total_pages > 0)
    {
        estimated_write_time_min = ((float)total_pages * 1.5f) / 60000.0f;  /* 转换为分钟 */
    }
    LOG_INFO("ENDURANCE_TEST", "[Cycle %u] [Write] 开始全片写入（%u 页，预计%.1f-%.1f分钟）...", 
             result->current_cycle, total_pages, estimated_write_time_min * 0.8f, estimated_write_time_min * 1.2f);
    LOG_INFO("ENDURANCE_TEST", "[Cycle %u] [Write] 写入固定值: 0x%02X (十进制: %u)", 
             result->current_cycle, write_value, write_value);
    
    /* 更新OLED显示：写入状态 */
#if CONFIG_MODULE_OLED_ENABLED
    {
        char oled_line1[17] = {0};
        snprintf(oled_line1, sizeof(oled_line1), "W25Q: Write...");
        size_t len1 = strlen(oled_line1);
        if (len1 < 16)
        {
            memset(oled_line1 + len1, ' ', 16 - len1);
            oled_line1[16] = '\0';
        }
        OLED_ShowString(1, 1, oled_line1);
    }
#endif
    uint32_t write_progress_step = total_pages / 10;  /* 每10%输出一次进度 */
    if (write_progress_step == 0) write_progress_step = 1;
    uint32_t write_progress_count = 0;
#endif
    
    for (page_addr = 0; page_addr < capacity_bytes; page_addr += W25Q_PAGE_SIZE)
    {
        /* 跳过Block 0（汇总信息存储区域，地址0x00000000-0x0000FFFF） */
        if (page_addr < W25Q_BLOCK_SIZE)
        {
            continue;  /* 跳过Block 0，不写入测试数据 */
        }
        
#if ENDURANCE_TEST_SIMULATE_WRITE_ERROR_ENABLED
        /* ?? 警告：模拟写入错误功能仅用于验证代码逻辑，正常测试时请勿开启！ */
        /* 准备写入缓冲区（默认使用正常数据） */
        uint8_t write_buffer[W25Q_PAGE_SIZE];
        memcpy(write_buffer, s_test_buffer, W25Q_PAGE_SIZE);
        
        /* 检查当前页是否包含模拟写入错误字节地址，如果是，将这些字节写入0xFF */
        if (s_simulate_write_error_count > 0)
        {
            uint32_t page_end = page_addr + W25Q_PAGE_SIZE;
            for (int i = 0; i < s_simulate_write_error_count; i++)
            {
                if (s_simulate_write_error_addrs[i] != 0 && 
                    s_simulate_write_error_addrs[i] >= page_addr && 
                    s_simulate_write_error_addrs[i] < page_end)
                {
                    /* 当前页包含模拟错误字节地址，将该字节写入0xFF */
                    uint32_t offset_in_page = s_simulate_write_error_addrs[i] - page_addr;
                    write_buffer[offset_in_page] = 0xFF;
#if CONFIG_MODULE_LOG_ENABLED
                    LOG_INFO("ENDURANCE_TEST", "[模拟写入错误] 地址 0x%06lX 模拟写入0xFF（用于验证错误检测功能）", 
                             (unsigned long)s_simulate_write_error_addrs[i]);
#endif
                }
            }
        }
#else
        /* 功能被禁用，始终使用正常数据 */
        uint8_t *write_buffer = s_test_buffer;
#endif
        
        /* 测量编程时间 */
        start_cycles = DWT->CYCCNT;
        w25q_status = W25Q_Write(page_addr, write_buffer, W25Q_PAGE_SIZE);
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
            LOG_INFO("ENDURANCE_TEST", "[Cycle %u] [Write] 写入进度: %u/%u 页 (%u%%)", 
                     result->current_cycle, program_count, total_pages, progress_percent);
            write_progress_count = 0;
        }
#endif
    }
    
#if CONFIG_MODULE_LOG_ENABLED
    if (program_count > 0)
    {
        float program_time_avg = program_time_sum / program_count;
        LOG_INFO("ENDURANCE_TEST", "[Cycle %u] [Write] 写入完成，平均编程时间: %.3f ms/页", 
                 result->current_cycle, program_time_avg);
    }
#endif
    
    /* ========== 写入完成：立即更新写入数据 ========== */
    if (program_count > 0)
    {
        float program_time_avg = program_time_sum / program_count;
        /* 更新页编程时间（避免除以0） */
        /* 注意：此时 total_cycles 已在擦除后更新 */
        if (result->total_cycles == 1)
        {
            result->program_time_avg = program_time_avg;
            /* 如果是第1轮循环，且基准数据还未写入（首次数据），同时更新基准数据 */
            /* 注意：首次数据只在第1轮循环完成后写入一次，之后不允许修改 */
            if (result->baseline_recorded && result->baseline.program_time_avg == 0.0f)
            {
                result->baseline.program_time_avg = program_time_avg;
            }
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
    LOG_INFO("ENDURANCE_TEST", "[Cycle %u] [Read] 开始全片读取并校验（%u 页，预计%.1f-%.1f分钟）...", 
             result->current_cycle, total_pages, estimated_read_time_min * 0.8f, estimated_read_time_min * 1.5f);
    LOG_INFO("ENDURANCE_TEST", "[Cycle %u] [Read] 期望读取值: 0x%02X (十进制: %u)", 
             result->current_cycle, expected_value, expected_value);
    
    /* 更新OLED显示：读取状态 */
#if CONFIG_MODULE_OLED_ENABLED
    {
        char oled_line1[17] = {0};
        snprintf(oled_line1, sizeof(oled_line1), "W25Q: Read...");
        size_t len1 = strlen(oled_line1);
        if (len1 < 16)
        {
            memset(oled_line1 + len1, ' ', 16 - len1);
            oled_line1[16] = '\0';
        }
        OLED_ShowString(1, 1, oled_line1);
    }
#endif
    uint32_t read_progress_step = total_pages / 10;  /* 每10%输出一次进度 */
    if (read_progress_step == 0) read_progress_step = 1;
    uint32_t read_progress_count = 0;
    uint32_t read_page_count = 0;
    
#if ENDURANCE_TEST_SIMULATE_WRITE_ERROR_ENABLED
    /* 模拟写入错误的错误统计（用于测试读取验证功能，计入误码率用于验证） */
    uint32_t simulate_error_errors = 0;  /* 模拟写入错误字节的错误位数 */
#endif
#endif
    
    for (page_addr = 0; page_addr < capacity_bytes; page_addr += W25Q_PAGE_SIZE)
    {
        /* 跳过Block 0（汇总信息存储区域，地址0x00000000-0x0000FFFF） */
        if (page_addr < W25Q_BLOCK_SIZE)
        {
            continue;  /* 跳过Block 0，不读取测试数据 */
        }
        
        /* 读取数据并测量读取时间 */
        start_cycles = DWT->CYCCNT;
        w25q_status = W25Q_Read(page_addr, s_read_buffer, W25Q_PAGE_SIZE);
        end_cycles = DWT->CYCCNT;
        if (w25q_status != W25Q_OK)
        {
            continue;
        }
        
        /* 统计读取时间 */
        read_time_sum += DWT_GetElapsedMs(start_cycles, end_cycles);
        read_count++;
        
        /* 统计位错误 */
#if ENDURANCE_TEST_SIMULATE_WRITE_ERROR_ENABLED
        /* 检查当前页是否包含模拟写入错误字节地址 */
        uint32_t page_end = page_addr + W25Q_PAGE_SIZE;
        uint32_t page_simulate_error_bytes = 0;  /* 当前页中模拟错误字节的数量 */
        uint32_t page_simulate_error_bits = 0;   /* 当前页中模拟错误字节的位错误数 */
        
        if (s_simulate_write_error_count > 0)
        {
            for (int i = 0; i < s_simulate_write_error_count; i++)
            {
                if (s_simulate_write_error_addrs[i] != 0 && 
                    s_simulate_write_error_addrs[i] >= page_addr && 
                    s_simulate_write_error_addrs[i] < page_end)
                {
                    /* 当前页包含模拟错误字节地址 */
                    uint32_t offset_in_page = s_simulate_write_error_addrs[i] - page_addr;
                    uint8_t expected_byte = s_test_buffer[0];  /* 期望值（循环编号） */
                    uint8_t actual_byte = s_read_buffer[offset_in_page];  /* 实际读取值 */
                    
                    /* 计算这个字节的位错误数 */
                    uint32_t byte_bit_errors = 0;
                    uint8_t diff = expected_byte ^ actual_byte;
                    while (diff != 0)
                    {
                        if (diff & 0x01)
                        {
                            byte_bit_errors++;
                        }
                        diff >>= 1;
                    }
                    
                    page_simulate_error_bits += byte_bit_errors;
                    page_simulate_error_bytes++;
                    
#if CONFIG_MODULE_LOG_ENABLED
                    LOG_INFO("ENDURANCE_TEST", "[模拟写入错误] 地址 0x%06lX 验证成功：检测到 %u 位错误（预期结果，验证功能正常工作）", 
                             (unsigned long)s_simulate_write_error_addrs[i], byte_bit_errors);
#endif
                }
            }
        }
        
        /* 统计正常字节的错误（排除模拟错误字节） */
        uint32_t normal_bit_errors = 0;
        for (uint32_t i = 0; i < W25Q_PAGE_SIZE; i++)
        {
            uint32_t byte_addr = page_addr + i;
            uint8_t is_simulate_error_byte = 0;
            
            /* 检查这个字节是否是模拟错误字节 */
            for (int j = 0; j < s_simulate_write_error_count; j++)
            {
                if (s_simulate_write_error_addrs[j] == byte_addr)
                {
                    is_simulate_error_byte = 1;
                    break;
                }
            }
            
            if (!is_simulate_error_byte)
            {
                /* 正常字节：统计错误 */
                uint8_t expected_byte = s_test_buffer[0];
                uint8_t actual_byte = s_read_buffer[i];
                uint8_t diff = expected_byte ^ actual_byte;
                while (diff != 0)
                {
                    if (diff & 0x01)
                    {
                        normal_bit_errors++;
                    }
                    diff >>= 1;
                }
            }
        }
        
        /* 累加错误统计 */
        if (page_simulate_error_bits > 0)
        {
            simulate_error_errors += page_simulate_error_bits;
            /* 模拟错误字节的错误计入误码率，用于验证误码率计算是否正确 */
            verify_errors_cycle += page_simulate_error_bits;
        }
        
        if (normal_bit_errors > 0)
        {
            /* 正常字节的错误计入误码率 */
            verify_errors_cycle += normal_bit_errors;
#if CONFIG_MODULE_LOG_ENABLED
            if (result->current_cycle <= 3 && normal_bit_errors > 100)
            {
                /* 如果错误位数很多，输出详细信息 */
                LOG_WARN("ENDURANCE_TEST", "[DEBUG] 地址 0x%06lX 检测到 %u 位错误", (unsigned long)page_addr, normal_bit_errors);
            }
#endif
        }
#else
        /* 功能被禁用，统计整页的错误 */
        CountBitErrors(s_test_buffer, s_read_buffer, W25Q_PAGE_SIZE, &bit_errors);
        if (bit_errors > 0)
        {
            verify_errors_cycle += bit_errors;
#if CONFIG_MODULE_LOG_ENABLED
            if (result->current_cycle <= 3 && bit_errors > 100)
            {
                /* 如果错误位数很多，输出详细信息 */
                LOG_WARN("ENDURANCE_TEST", "[DEBUG] 地址 0x%06lX 检测到 %u 位错误", (unsigned long)page_addr, bit_errors);
            }
#endif
        }
#endif
        
#if CONFIG_MODULE_LOG_ENABLED
        /* 每10%输出一次进度 */
        read_page_count++;
        read_progress_count++;
        if (read_progress_count >= read_progress_step)
        {
            uint32_t progress_percent = (read_page_count * 100) / total_pages;
            LOG_INFO("ENDURANCE_TEST", "[Cycle %u] [Read] 读取进度: %u/%u 页 (%u%%)", 
                     result->current_cycle, read_page_count, total_pages, progress_percent);
            read_progress_count = 0;
        }
#endif
    }
    
#if CONFIG_MODULE_LOG_ENABLED
    LOG_INFO("ENDURANCE_TEST", "[Cycle %u] [Read] 读取完成，校验错误: %u 位", 
             result->current_cycle, verify_errors_cycle);
#if ENDURANCE_TEST_SIMULATE_WRITE_ERROR_ENABLED
    if (simulate_error_errors > 0)
    {
        LOG_INFO("ENDURANCE_TEST", "[模拟写入错误] 验证结果：模拟错误字节共 %u 位错误（预期结果，验证功能正常工作，已计入误码率）", simulate_error_errors);
    }
#endif
#endif
    
    result->verify_errors += verify_errors_cycle;
    
    /* 计算误码率 */
    /* 注意：total_pages 是总页数，但实际读取的页数可能少于 total_pages（因为跳过了Block 0） */
    /* 使用实际读取的页数计算错误率更准确 */
    /* 模拟写入错误的页不会被跳过，用于测试读取验证是否能检测到数据错误 */
    /* 如果模拟写入错误功能开启，其错误也会计入误码率，用于验证误码率计算是否正确 */
    uint32_t actual_read_pages = read_count;  /* 实际读取的页数（已排除Block 0） */
    if (actual_read_pages > 0)
    {
        uint64_t total_bits = (uint64_t)actual_read_pages * W25Q_PAGE_SIZE * 8;
        if (total_bits > 0)
        {
            result->error_rate = (float)result->verify_errors / (float)total_bits;
        }
        else
        {
            result->error_rate = 0.0f;
        }
    }
    else
    {
        /* 如果没有读取任何页，错误率设为0 */
        result->error_rate = 0.0f;
    }
    
    /* 更新OLED显示（误码率计算完成后立即更新） */
#if CONFIG_MODULE_OLED_ENABLED
    {
        char oled_line1[17] = {0};
        char oled_line2[17] = {0};
        char oled_line3[17] = {0};
        char oled_line4[17] = {0};
        float total_gb = (float)result->total_data_written_mb / 1024.0f;
        
        /* 第1行：显示测试状态（在误码率计算完成后显示，此时已完成一轮完整的E/W/R循环） */
        /* 注意：擦除/写入/读取过程中会动态更新第1行显示，这里显示完成状态 */
        snprintf(oled_line1, sizeof(oled_line1), "W25Q: Done");
        size_t len1 = strlen(oled_line1);
        if (len1 < 16)
        {
            memset(oled_line1 + len1, ' ', 16 - len1);
            oled_line1[16] = '\0';
        }
        OLED_ShowString(1, 1, oled_line1);
        
        /* 第2行：显示连续擦除值，确保字符串长度为16，用空格填充剩余部分（支持x1到x9999） */
        snprintf(oled_line2, sizeof(oled_line2), "Erase: x%u", ENDURANCE_TEST_CONSECUTIVE_ERASE_COUNT);
        size_t len2 = strlen(oled_line2);
        if (len2 >= 16)
        {
            /* 如果字符串太长，截断到16个字符 */
            oled_line2[16] = '\0';
            len2 = 16;
        }
        else
        {
            /* 如果字符串太短，用空格填充到16个字符 */
            memset(oled_line2 + len2, ' ', 16 - len2);
            oled_line2[16] = '\0';
        }
        OLED_ShowString(2, 1, oled_line2);
        
        /* 第3行：显示当前轮次（跟着进行中的轮次值更新），确保字符串长度为16，用空格填充剩余部分 */
        snprintf(oled_line3, sizeof(oled_line3), "Cycle: %u", result->current_cycle);
        size_t len3 = strlen(oled_line3);
        if (len3 < 16)
        {
            memset(oled_line3 + len3, ' ', 16 - len3);
            oled_line3[16] = '\0';
        }
        OLED_ShowString(3, 1, oled_line3);
        
        /* 第4行：显示当前误码率（跟着进行中的误码率值更新，使用易读格式），确保字符串长度为16，用空格填充剩余部分 */
        {
            char error_rate_str[17] = {0};
            FormatErrorRate(result->error_rate, error_rate_str, sizeof(error_rate_str));
            /* OLED 显示空间有限，如果字符串太长则截断 */
            if (strlen(error_rate_str) > 15)
            {
                error_rate_str[15] = '\0';
            }
            snprintf(oled_line4, sizeof(oled_line4), "Err: %s", error_rate_str);
            size_t len4 = strlen(oled_line4);
            if (len4 < 16)
            {
                memset(oled_line4 + len4, ' ', 16 - len4);
                oled_line4[16] = '\0';
            }
        }
        OLED_ShowString(4, 1, oled_line4);
    }
#endif
    
#if CONFIG_MODULE_LOG_ENABLED
    if (result->current_cycle <= 3)
    {
        LOG_INFO("ENDURANCE_TEST", "[DEBUG] 读取校验统计: 实际读取页数=%u, 校验错误=%u位", 
                 actual_read_pages, verify_errors_cycle);
#if ENDURANCE_TEST_SIMULATE_WRITE_ERROR_ENABLED
        if (s_simulate_write_error_count > 0)
        {
            LOG_INFO("ENDURANCE_TEST", "[DEBUG] 模拟写入错误字节（用于测试读取验证功能）: 共%u个字节，已计入误码率用于验证", 
                     s_simulate_write_error_count);
            for (uint8_t i = 0; i < s_simulate_write_error_count; i++)
            {
                if (s_simulate_write_error_addrs[i] != 0)
                {
                    LOG_INFO("ENDURANCE_TEST", "[DEBUG]   地址 %u: 0x%06lX", 
                             i + 1, (unsigned long)s_simulate_write_error_addrs[i]);
                }
            }
        }
#endif
    }
#endif
    
    /* ========== 验证模拟写入错误字节：确认它们确实包含0xFF（而不是主测试期望的值） ========== */
#if ENDURANCE_TEST_SIMULATE_WRITE_ERROR_ENABLED
    /* 这证明了模拟写入错误确实生效，并且主测试的读取验证能够检测到数据错误 */
    if (s_simulate_write_error_count > 0)
    {
        uint8_t expected_main_value = (uint8_t)(result->current_cycle & 0xFF);  /* 主测试期望的值 */
        
#if CONFIG_MODULE_LOG_ENABLED
        LOG_INFO("ENDURANCE_TEST", "[模拟写入错误] 验证模拟写入错误字节：主测试期望值=0x%02X，实际写入值=0xFF", expected_main_value);
#endif
        
        for (uint8_t i = 0; i < s_simulate_write_error_count; i++)
        {
            if (s_simulate_write_error_addrs[i] != 0)
            {
                /* 读取模拟写入错误字节（需要读取整个页，然后检查对应字节） */
                uint32_t byte_addr = s_simulate_write_error_addrs[i];
                uint32_t page_addr_verify = (byte_addr / W25Q_PAGE_SIZE) * W25Q_PAGE_SIZE;
                uint32_t offset_in_page = byte_addr - page_addr_verify;
                uint8_t verify_buffer[W25Q_PAGE_SIZE];
                
                W25Q_Status_t read_status = W25Q_Read(page_addr_verify, verify_buffer, W25Q_PAGE_SIZE);
                if (read_status == W25Q_OK)
                {
                    uint8_t actual_byte = verify_buffer[offset_in_page];
                    
#if CONFIG_MODULE_LOG_ENABLED
                    if (actual_byte == 0xFF)
                    {
                        LOG_INFO("ENDURANCE_TEST", "[模拟写入错误] 字节 0x%06lX 验证成功：数据为0xFF（与期望值0x%02X不匹配，验证功能正常工作）", 
                                 (unsigned long)byte_addr, expected_main_value);
                    }
                    else if (actual_byte == expected_main_value)
                    {
                        LOG_WARN("ENDURANCE_TEST", "[模拟写入错误] 字节 0x%06lX 验证异常：数据为0x%02X（与期望值相同，模拟错误可能未生效）", 
                                 (unsigned long)byte_addr, expected_main_value);
                    }
                    else
                    {
                        LOG_WARN("ENDURANCE_TEST", "[模拟写入错误] 字节 0x%06lX 验证异常：数据为0x%02X（不是0xFF也不是期望值0x%02X）", 
                                 (unsigned long)byte_addr, actual_byte, expected_main_value);
                    }
#endif
                }
                else
                {
#if CONFIG_MODULE_LOG_ENABLED
                    LOG_WARN("ENDURANCE_TEST", "[模拟写入错误] 字节 0x%06lX 验证失败：读取失败", (unsigned long)byte_addr);
#endif
                }
            }
        }
    }
#endif
    
    /* ========== 读取完成：立即更新读取数据 ========== */
    /* 更新读取速度（如果读取了数据） */
    /* 注意：此时 total_cycles 已在擦除后更新 */
    if (read_count > 0)
    {
        float read_time_avg = read_time_sum / read_count;
        float read_speed_current = (W25Q_PAGE_SIZE / 1024.0f) / (read_time_avg / 1000.0f);  /* KB/s */
        /* 更新读取速度（避免除以0） */
        if (result->total_cycles == 1)
        {
            result->read_speed = read_speed_current;
            /* 如果是第1轮循环，且基准数据还未写入（首次数据），同时更新基准数据 */
            /* 注意：首次数据只在第1轮循环完成后写入一次，之后不允许修改 */
            if (result->baseline_recorded && result->baseline.read_speed == 0.0f)
            {
                result->baseline.read_speed = read_speed_current;
            }
        }
        else
        {
            result->read_speed = (result->read_speed * (result->total_cycles - 1) + read_speed_current) / result->total_cycles;
        }
    }
    
    /* 如果是第1轮循环（total_cycles == 1），且基准数据还未写入（首次数据），同时更新基准数据和进行中数据的错误率 */
    /* 注意：首次数据只在第1轮循环完成后写入一次，之后不允许修改 */
    if (result->baseline_recorded && result->total_cycles == 1 && result->baseline.error_rate == 0.0f)
    {
        /* 第1轮循环完成，将错误率同时写入基准数据（首次数据）和进行中数据 */
        result->baseline.error_rate = result->error_rate;
#if CONFIG_MODULE_LOG_ENABLED
        {
            char error_rate_str[32];
            FormatErrorRate(result->baseline.error_rate, error_rate_str, sizeof(error_rate_str));
            LOG_INFO("ENDURANCE_TEST", "第1轮完整P/E循环完成，同时更新基准数据（首次数据）: 块擦除时间=%.2f秒, 页编程时间=%.3fms, 读取速度=%.2fKB/s, 错误率=%s", 
                     result->baseline.erase_time_avg / 1000.0f, result->baseline.program_time_avg, 
                     result->baseline.read_speed, error_rate_str);
        }
#endif
    }
    
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
            LOG_INFO("ENDURANCE_TEST", "[DEBUG] cycles=%u, cap_mb=%u, total_data_written_mb=%llu", 
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
            LOG_INFO("ENDURANCE_TEST", "[DEBUG] 使用capacity_bytes计算: total_cycles=%u, capacity_bytes=%llu, total_data_written_mb=%llu", 
                     result->total_cycles, (unsigned long long)capacity_bytes, 
                     (unsigned long long)result->total_data_written_mb);
        }
#endif
    }
    
    /* 计算性能退化率（如果已记录基准数据，且至少完成1次循环） */
    if (result->baseline_recorded && result->total_cycles > 0)
    {
        /* 擦除时间退化率：用进行中的块擦除时间与首次的块擦除时间计算 */
        /* 如果比首次快（负值），设为0.00%；如果比首次慢，显示实际百分比 */
        if (result->baseline.erase_time_avg > 0.0f && result->erase_time_avg > 0.0f)
        {
            float rate = ((result->erase_time_avg - result->baseline.erase_time_avg) / result->baseline.erase_time_avg) * 100.0f;
            result->erase_degradation_rate = (rate > 0.0f) ? rate : 0.0f;
        }
        
        /* 编程时间退化率：用进行中的页编程时间与首次的页编程时间计算 */
        /* 如果比首次快（负值），设为0.00%；如果比首次慢，显示实际百分比 */
        if (result->baseline.program_time_avg > 0.0f && result->program_time_avg > 0.0f)
        {
            float rate = ((result->program_time_avg - result->baseline.program_time_avg) / result->baseline.program_time_avg) * 100.0f;
            result->program_degradation_rate = (rate > 0.0f) ? rate : 0.0f;
        }
        
        /* 读取速度退化率：用进行中的读取速度与首次的读取速度计算 */
        /* 如果比首次快（负值），设为0.00%；如果比首次慢，显示实际百分比 */
        if (result->baseline.read_speed > 0.0f && result->read_speed > 0.0f)
        {
            float rate = ((result->baseline.read_speed - result->read_speed) / result->baseline.read_speed) * 100.0f;
            result->read_speed_degradation_rate = (rate > 0.0f) ? rate : 0.0f;
        }
    }
    
    /* 计算端粒进度：按官方标准10万次=100% */
    result->telomere_progress = ((float)result->total_cycles / 100000.0f) * 100.0f;
    
#if CONFIG_MODULE_LOG_ENABLED
    /* 每轮都输出该轮信息和汇总信息 */
    LOG_INFO("ENDURANCE_TEST", "=== 第 %u 次P/E循环完成 ===", result->current_cycle);
    LOG_INFO("ENDURANCE_TEST", "[本轮信息]");
    /* 计算每块平均擦除时间（复用之前计算的变量） */
    if (total_blocks > 0)
    {
        erase_time_per_block_ms = erase_time_ms / (float)total_blocks;
    }
    LOG_INFO("ENDURANCE_TEST", "  擦除时间: %.2f 秒 (%.2f ms/块)", 
             erase_time_ms / 1000.0f, erase_time_per_block_ms);
    LOG_INFO("ENDURANCE_TEST", "  编程错误: %u 次", program_fail_count);
    LOG_INFO("ENDURANCE_TEST", "  校验错误: %u 位", verify_errors_cycle);
    LOG_INFO("ENDURANCE_TEST", "[累计汇总]");
    LOG_INFO("ENDURANCE_TEST", "  总循环数: %u 次", result->total_cycles);
    LOG_INFO("ENDURANCE_TEST", "  总写入数据: %llu MB (%.2f GB)", 
             (unsigned long long)result->total_data_written_mb, 
             (float)result->total_data_written_mb / 1024.0f);
    LOG_INFO("ENDURANCE_TEST", "  累计擦除错误: %u 次", result->erase_errors);
    LOG_INFO("ENDURANCE_TEST", "  累计编程错误: %u 次", result->program_errors);
    LOG_INFO("ENDURANCE_TEST", "  累计校验错误: %u 位", result->verify_errors);
    /* 计算平均每块擦除时间 */
    float erase_time_per_block_avg_ms = 0.0f;
    if (total_blocks > 0)
    {
        erase_time_per_block_avg_ms = result->erase_time_avg / (float)total_blocks;
    }
    LOG_INFO("ENDURANCE_TEST", "  块擦除时间: %.2f 秒 (%.2f ms/块)", 
             result->erase_time_avg / 1000.0f, erase_time_per_block_avg_ms);
    LOG_INFO("ENDURANCE_TEST", "  页编程时间: %.3f ms/页", result->program_time_avg);
    LOG_INFO("ENDURANCE_TEST", "  读取速度: %.2f KB/s", result->read_speed);
    {
        char error_rate_str[32];
        FormatErrorRate(result->error_rate, error_rate_str, sizeof(error_rate_str));
        LOG_INFO("ENDURANCE_TEST", "  当前误码率: %s", error_rate_str);
    }
    if (result->baseline_recorded)
    {
        LOG_INFO("ENDURANCE_TEST", "  擦除时间退化率: %.2f%%", result->erase_degradation_rate);
        LOG_INFO("ENDURANCE_TEST", "  编程时间退化率: %.2f%%", result->program_degradation_rate);
        LOG_INFO("ENDURANCE_TEST", "  读取速度退化率: %.2f%%", result->read_speed_degradation_rate);
        LOG_INFO("ENDURANCE_TEST", "  端粒进度: %.2f%%", result->telomere_progress);
    }
#endif
    
    /* 保存汇总信息到Flash（断点续测） */
    EnduranceTest_SaveSummary(result);
    
    /* 更新OLED显示（测试期间） */
#if CONFIG_MODULE_OLED_ENABLED
    {
        char oled_line1[17] = {0};
        char oled_line2[17] = {0};
        char oled_line3[17] = {0};
        char oled_line4[17] = {0};
        float total_gb = (float)result->total_data_written_mb / 1024.0f;
        
        /* 第1行：显示写入量（GB） */
        snprintf(oled_line1, sizeof(oled_line1), "Write: %.2fGB", total_gb);
        OLED_ShowString(1, 1, oled_line1);
        
        /* 第2行：显示连续擦除值（确保字符串长度为16，用空格填充剩余部分） */
        snprintf(oled_line2, sizeof(oled_line2), "%-16s", "");  /* 先清空 */
        snprintf(oled_line2, sizeof(oled_line2), "Erase: x%u", ENDURANCE_TEST_CONSECUTIVE_ERASE_COUNT);
        /* 确保字符串长度为16，用空格填充 */
        size_t len = strlen(oled_line2);
        if (len < 16)
        {
            memset(oled_line2 + len, ' ', 16 - len);
            oled_line2[16] = '\0';
        }
        OLED_ShowString(2, 1, oled_line2);
        
        /* 第3行：显示当前轮次（跟着进行中的轮次值更新） */
        snprintf(oled_line3, sizeof(oled_line3), "Cycle: %u", result->current_cycle);
        OLED_ShowString(3, 1, oled_line3);
        
        /* 第4行：显示当前误码率（跟着进行中的误码率值更新，使用易读格式） */
        {
            char error_rate_str[17] = {0};
            FormatErrorRate(result->error_rate, error_rate_str, sizeof(error_rate_str));
            /* OLED 显示空间有限，如果字符串太长则截断 */
            if (strlen(error_rate_str) > 15)
            {
                error_rate_str[15] = '\0';
            }
            snprintf(oled_line4, sizeof(oled_line4), "Err: %s", error_rate_str);
        }
        OLED_ShowString(4, 1, oled_line4);
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
            float erase_time_per_block = result->erase_time_avg / (float)total_blocks_eol;
            
            if (result->erase_time_avg > dynamic_threshold)
            {
                *is_dead = 1;
                result->chip_status = CHIP_STATUS_DEAD;
#if CONFIG_MODULE_LOG_ENABLED
                LOG_ERROR("ENDURANCE_TEST", "报废判定: 全片擦除时间超过阈值 (%.2f 秒 > %.2f 秒, %.2f ms/块 > %.2f ms/块)", 
                          result->erase_time_avg / 1000.0f, dynamic_threshold / 1000.0f,
                          erase_time_per_block, EOL_ERASE_TIME_THRESHOLD_MS);
#endif
                return ENDURANCE_TEST_OK;
            }
        }
        else
        {
            /* 如果无法获取Block数量，使用固定阈值作为后备 */
            if (result->erase_time_avg > EOL_CHIP_ERASE_TIME_THRESHOLD_MS)
            {
                *is_dead = 1;
                result->chip_status = CHIP_STATUS_DEAD;
#if CONFIG_MODULE_LOG_ENABLED
                LOG_ERROR("ENDURANCE_TEST", "报废判定: 全片擦除时间超过阈值 (%.2f ms > %.2f ms)", 
                          result->erase_time_avg, EOL_CHIP_ERASE_TIME_THRESHOLD_MS);
#endif
                return ENDURANCE_TEST_OK;
            }
        }
    }
    else
    {
        /* 如果无法获取设备信息，使用固定阈值作为后备 */
        if (result->erase_time_avg > EOL_CHIP_ERASE_TIME_THRESHOLD_MS)
        {
            *is_dead = 1;
            result->chip_status = CHIP_STATUS_DEAD;
#if CONFIG_MODULE_LOG_ENABLED
            LOG_ERROR("ENDURANCE_TEST", "报废判定: 全片擦除时间超过阈值 (%.2f ms > %.2f ms)", 
                      result->erase_time_avg, EOL_CHIP_ERASE_TIME_THRESHOLD_MS);
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
        {
            char error_rate_str[32];
            char threshold_str[32];
            FormatErrorRate(result->error_rate, error_rate_str, sizeof(error_rate_str));
            FormatErrorRate(EOL_ERROR_RATE_THRESHOLD, threshold_str, sizeof(threshold_str));
            LOG_ERROR("ENDURANCE_TEST", "报废判定: 误码率超过阈值 (%s > %s)", 
                      error_rate_str, threshold_str);
        }
#endif
        return ENDURANCE_TEST_OK;
    }
    
    /* 4. 读干扰错误超过阈值 */
    if (result->read_disturb_errors > EOL_READ_DISTURB_THRESHOLD)
    {
        *is_dead = 1;
        result->chip_status = CHIP_STATUS_DEAD;
#if CONFIG_MODULE_LOG_ENABLED
        LOG_ERROR("ENDURANCE_TEST", "报废判定: 读干扰错误超过阈值 (%u > %d)", 
                  result->read_disturb_errors, EOL_READ_DISTURB_THRESHOLD);
#endif
        return ENDURANCE_TEST_OK;
    }
    
    /* 软失效标准检查（性能退化） */
    if (result->baseline_recorded)
    {
        /* 计算退化率 */
        /* 擦除时间退化率：用进行中的块擦除时间与首次的块擦除时间计算 */
        /* 如果比首次快（负值），设为0.00%；如果比首次慢，显示实际百分比 */
        if (result->baseline.erase_time_avg > 0.0f && result->erase_time_avg > 0.0f)
        {
            float rate = ((result->erase_time_avg - result->baseline.erase_time_avg) / result->baseline.erase_time_avg) * 100.0f;
            result->erase_degradation_rate = (rate > 0.0f) ? rate : 0.0f;
        }
        
        /* 编程时间退化率：用进行中的页编程时间与首次的页编程时间计算 */
        /* 如果比首次快（负值），设为0.00%；如果比首次慢，显示实际百分比 */
        if (result->baseline.program_time_avg > 0.0f && result->program_time_avg > 0.0f)
        {
            float rate = ((result->program_time_avg - result->baseline.program_time_avg) / result->baseline.program_time_avg) * 100.0f;
            result->program_degradation_rate = (rate > 0.0f) ? rate : 0.0f;
        }
        
        /* 读取速度退化率：用进行中的读取速度与首次的读取速度计算 */
        /* 如果比首次快（负值），设为0.00%；如果比首次慢，显示实际百分比 */
        if (result->baseline.read_speed > 0.0f && result->read_speed > 0.0f)
        {
            float rate = ((result->baseline.read_speed - result->read_speed) / result->baseline.read_speed) * 100.0f;
            result->read_speed_degradation_rate = (rate > 0.0f) ? rate : 0.0f;
        }
    }
    
    /* 计算端粒进度：按官方标准10万次=100% */
    result->telomere_progress = ((float)result->total_cycles / 100000.0f) * 100.0f;
    
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
    LOG_INFO("ENDURANCE_TEST", "=== 深度健康检查（第 %u 次循环） ===", result->current_cycle);
#endif
    
    /* 计算性能退化率 */
    if (result->baseline_recorded)
    {
        /* 擦除时间退化率：如果比首次快（负值），设为0.00%；如果比首次慢，显示实际百分比 */
        if (result->baseline.erase_time_avg > 0.0f && result->erase_time_avg > 0.0f)
        {
            float rate = ((result->erase_time_avg - result->baseline.erase_time_avg) / result->baseline.erase_time_avg) * 100.0f;
            result->erase_degradation_rate = (rate > 0.0f) ? rate : 0.0f;
        }
        
        /* 编程时间退化率：如果比首次快（负值），设为0.00%；如果比首次慢，显示实际百分比 */
        if (result->baseline.program_time_avg > 0.0f && result->program_time_avg > 0.0f)
        {
            float rate = ((result->program_time_avg - result->baseline.program_time_avg) / result->baseline.program_time_avg) * 100.0f;
            result->program_degradation_rate = (rate > 0.0f) ? rate : 0.0f;
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
    
    /* 计算端粒进度：按官方标准10万次=100% */
    result->telomere_progress = ((float)result->total_cycles / 100000.0f) * 100.0f;
    
#if CONFIG_MODULE_LOG_ENABLED
    LOG_INFO("ENDURANCE_TEST", "端粒进度: %.2f%%", result->telomere_progress);
    LOG_INFO("ENDURANCE_TEST", "芯片状态: %d", result->chip_status);
    {
        char error_rate_str[32];
        FormatErrorRate(result->error_rate, error_rate_str, sizeof(error_rate_str));
        LOG_INFO("ENDURANCE_TEST", "误码率: %s", error_rate_str);
    }
    LOG_INFO("ENDURANCE_TEST", "坏块数: %u", result->bad_block_count);
#endif
    
    return ENDURANCE_TEST_OK;
}

/**
 * @brief 从Flash加载汇总信息（断点续测）
 * @param[out] result 测试结果结构体指针
 * @return Endurance_Test_Status_t 错误码
 * @note 从Block 0（第一个块）读取汇总信息，验证魔数标签
 *       如果魔数有效，恢复测试状态；如果无效，返回错误（表示需要从0开始）
 */
Endurance_Test_Status_t EnduranceTest_LoadSummary(Endurance_Test_Result_t *result)
{
    Endurance_Test_Summary_t summary;
    W25Q_Status_t w25q_status;
    
    if (result == NULL)
    {
        return ENDURANCE_TEST_ERROR_NULL_PTR;
    }
    
    if (!g_endurance_test_initialized)
    {
        return ENDURANCE_TEST_ERROR_NOT_INIT;
    }
    
#if CONFIG_MODULE_LOG_ENABLED
    LOG_INFO("ENDURANCE_TEST", "尝试从Flash加载汇总信息（断点续测）...");
#endif
    
    /* 读取汇总信息结构体（从Block 0） */
    w25q_status = W25Q_Read(ENDURANCE_TEST_SUMMARY_ADDR, (uint8_t *)&summary, sizeof(Endurance_Test_Summary_t));
    if (w25q_status != W25Q_OK)
    {
#if CONFIG_MODULE_LOG_ENABLED
        LOG_WARN("ENDURANCE_TEST", "读取汇总信息失败: %d", w25q_status);
#endif
        return ENDURANCE_TEST_ERROR_W25Q_FAILED;
    }
    
    /* 验证魔数标签 */
    if (summary.magic != ENDURANCE_TEST_MAGIC)
    {
#if CONFIG_MODULE_LOG_ENABLED
        LOG_INFO("ENDURANCE_TEST", "未找到有效汇总信息（魔数不匹配: 0x%08lX），从0开始测试", 
                 (unsigned long)summary.magic);
#endif
        return ENDURANCE_TEST_ERROR_INVALID_PARAM;
    }
    
    /* 验证版本号 */
    if (summary.version != ENDURANCE_TEST_VERSION)
    {
#if CONFIG_MODULE_LOG_ENABLED
        LOG_WARN("ENDURANCE_TEST", "汇总信息版本不匹配: 0x%04X（期望: 0x%04X），从0开始测试", 
                 summary.version, ENDURANCE_TEST_VERSION);
#endif
        return ENDURANCE_TEST_ERROR_INVALID_PARAM;
    }
    
    /* 注意：断点续测时，不进行数据合理性验证 */
    /* 因为数据是每轮测试产生的结果，都是有效的，不应该被判定为异常 */
    
    /* 恢复测试状态 */
    memcpy(result, &summary.result, sizeof(Endurance_Test_Result_t));
    
    /* 恢复基准数据（首轮数据） */
    memcpy(&result->baseline, &summary.baseline, sizeof(Baseline_Data_t));
    
    /* 同步current_cycle和total_cycles（恢复后，下一轮循环应该从total_cycles+1开始） */
    result->current_cycle = result->total_cycles;
    
#if CONFIG_MODULE_LOG_ENABLED
    LOG_INFO("ENDURANCE_TEST", "成功加载汇总信息，从断点恢复测试");
    
    const w25q_dev_t *dev_info = W25Q_GetInfo();
    uint32_t total_blocks = 0;
    if (dev_info != NULL)
    {
        uint64_t capacity_bytes = (uint64_t)dev_info->capacity_mb * 1024ULL * 1024ULL;
        total_blocks = (uint32_t)(capacity_bytes / W25Q_BLOCK_SIZE);
    }
    
    /* 检查测试状态：如果已报废，只显示首次数据和报废数据 */
    if (summary.test_status == TEST_STATUS_COMPLETED && summary.dead_data_recorded)
    {
        /* 已报废：显示首次数据和报废数据 */
        LOG_INFO("ENDURANCE_TEST", "测试状态: 已完成（芯片已报废）");
        LOG_INFO("ENDURANCE_TEST", "");
        
        /* 首次汇总数据（基准数据） */
        if (result->baseline_recorded)
        {
            float erase_time_per_block_avg_ms = 0.0f;
            if (total_blocks > 0)
            {
                erase_time_per_block_avg_ms = result->baseline.erase_time_avg / (float)total_blocks;
            }
            
            LOG_INFO("ENDURANCE_TEST", "【首次汇总数据（基准数据）】");
            LOG_INFO("ENDURANCE_TEST", "  块擦除时间: %.2f 秒 (%.2f ms/块)", 
                     result->baseline.erase_time_avg / 1000.0f, erase_time_per_block_avg_ms);
            LOG_INFO("ENDURANCE_TEST", "  页编程时间: %.3f ms/页", result->baseline.program_time_avg);
            LOG_INFO("ENDURANCE_TEST", "  读取速度: %.2f KB/s", result->baseline.read_speed);
            {
                char error_rate_str[32];
                FormatErrorRate(result->baseline.error_rate, error_rate_str, sizeof(error_rate_str));
                LOG_INFO("ENDURANCE_TEST", "  初始错误率: %s", error_rate_str);
            }
        }
        
        LOG_INFO("ENDURANCE_TEST", "");
        
        /* 报废时数据 */
        LOG_INFO("ENDURANCE_TEST", "【报废时数据（芯片报废时的最终状态）】");
        LOG_INFO("ENDURANCE_TEST", "  报废循环数: %u 次", summary.dead_data.dead_cycle);
        float erase_time_per_block_final_ms = 0.0f;
        if (total_blocks > 0)
        {
            erase_time_per_block_final_ms = summary.dead_data.erase_time_final / (float)total_blocks;
        }
        LOG_INFO("ENDURANCE_TEST", "  最终块擦除时间: %.2f 秒 (%.2f ms/块)", 
                 summary.dead_data.erase_time_final / 1000.0f, erase_time_per_block_final_ms);
        LOG_INFO("ENDURANCE_TEST", "  最终页编程时间: %.3f ms/页", summary.dead_data.program_time_final);
        LOG_INFO("ENDURANCE_TEST", "  最终读取速度: %.2f KB/s", summary.dead_data.read_speed_final);
        LOG_INFO("ENDURANCE_TEST", "  最终擦除错误: %u 次", summary.dead_data.erase_errors_final);
        LOG_INFO("ENDURANCE_TEST", "  最终编程错误: %u 次", summary.dead_data.program_errors_final);
        LOG_INFO("ENDURANCE_TEST", "  最终校验错误: %u 位", summary.dead_data.verify_errors_final);
        LOG_INFO("ENDURANCE_TEST", "  最终坏块数量: %u", summary.dead_data.bad_block_count_final);
        {
            char error_rate_str[32];
            FormatErrorRate(summary.dead_data.error_rate_final, error_rate_str, sizeof(error_rate_str));
            LOG_INFO("ENDURANCE_TEST", "  最终误码率: %s", error_rate_str);
        }
        LOG_INFO("ENDURANCE_TEST", "  最终端粒进度: %.2f%%", summary.dead_data.telomere_progress_final);
        LOG_INFO("ENDURANCE_TEST", "  最终总写入数据: %llu MB (%.2f GB)", 
                 (unsigned long long)summary.dead_data.total_data_written_mb_final,
                 (float)summary.dead_data.total_data_written_mb_final / 1024.0f);
    }
    else
    {
        /* 进行中：显示首次数据、进行中数据和报废数据（如果已记录） */
        
        /* 首次汇总数据（基准数据） */
        if (result->baseline_recorded)
        {
            float erase_time_per_block_avg_ms = 0.0f;
            if (total_blocks > 0)
            {
                erase_time_per_block_avg_ms = result->baseline.erase_time_avg / (float)total_blocks;
            }
            
            LOG_INFO("ENDURANCE_TEST", "【首次汇总数据（基准数据）】");
            LOG_INFO("ENDURANCE_TEST", "  块擦除时间: %.2f 秒 (%.2f ms/块)", 
                     result->baseline.erase_time_avg / 1000.0f, erase_time_per_block_avg_ms);
            LOG_INFO("ENDURANCE_TEST", "  页编程时间: %.3f ms/页", result->baseline.program_time_avg);
            LOG_INFO("ENDURANCE_TEST", "  读取速度: %.2f KB/s", result->baseline.read_speed);
            {
                char error_rate_str[32];
                FormatErrorRate(result->baseline.error_rate, error_rate_str, sizeof(error_rate_str));
                LOG_INFO("ENDURANCE_TEST", "  初始错误率: %s", error_rate_str);
            }
        }
        
        LOG_INFO("ENDURANCE_TEST", "");
        
        /* 进行中汇总数据（当前测试状态） */
        LOG_INFO("ENDURANCE_TEST", "【进行中汇总数据（当前测试状态）】");
        LOG_INFO("ENDURANCE_TEST", "  测试状态: %s", 
                 summary.test_status == TEST_STATUS_RUNNING ? "进行中" :
                 summary.test_status == TEST_STATUS_COMPLETED ? "已完成" : "已暂停");
        LOG_INFO("ENDURANCE_TEST", "  已执行循环数: %u 次", result->total_cycles);
        LOG_INFO("ENDURANCE_TEST", "  总写入数据量: %llu MB (%.2f GB)", 
                 (unsigned long long)result->total_data_written_mb, 
                 (float)result->total_data_written_mb / 1024.0f);
        LOG_INFO("ENDURANCE_TEST", "  端粒进度: %.2f%%", result->telomere_progress);
        LOG_INFO("ENDURANCE_TEST", "  块擦除时间: %.2f 秒 (%.2f ms/块)", 
                 result->erase_time_avg / 1000.0f, 
                 (total_blocks > 0) ? (result->erase_time_avg / (float)total_blocks) : 0.0f);
        LOG_INFO("ENDURANCE_TEST", "  页编程时间: %.3f ms/页", result->program_time_avg);
        LOG_INFO("ENDURANCE_TEST", "  读取速度: %.2f KB/s", result->read_speed);
        LOG_INFO("ENDURANCE_TEST", "  累计擦除错误: %u 次", result->erase_errors);
        LOG_INFO("ENDURANCE_TEST", "  累计编程错误: %u 次", result->program_errors);
        LOG_INFO("ENDURANCE_TEST", "  累计校验错误: %u 位", result->verify_errors);
        LOG_INFO("ENDURANCE_TEST", "  坏块数量: %u", result->bad_block_count);
        {
            char error_rate_str[32];
            FormatErrorRate(result->error_rate, error_rate_str, sizeof(error_rate_str));
            LOG_INFO("ENDURANCE_TEST", "  当前误码率: %s", error_rate_str);
        }
        
        /* 如果已记录报废数据，也显示（虽然不应该在"进行中"状态出现，但为了完整性） */
        if (summary.dead_data_recorded)
        {
            LOG_INFO("ENDURANCE_TEST", "");
            LOG_INFO("ENDURANCE_TEST", "【报废时数据（已记录）】");
            LOG_INFO("ENDURANCE_TEST", "  报废循环数: %u 次", summary.dead_data.dead_cycle);
            LOG_INFO("ENDURANCE_TEST", "  最终端粒进度: %.2f%%", summary.dead_data.telomere_progress_final);
        }
    }
#endif
    
    return ENDURANCE_TEST_OK;
}

/**
 * @brief 保存汇总信息到Flash（断点续测）
 * @param[in] result 测试结果结构体指针
 * @return Endurance_Test_Status_t 错误码
 * @note 将汇总信息写入Block 0（第一个块），包含魔数标签
 *       写入前检查是否需要擦除（Flash只能从1变0）
 */
Endurance_Test_Status_t EnduranceTest_SaveSummary(const Endurance_Test_Result_t *result)
{
    Endurance_Test_Summary_t summary;
    Endurance_Test_Summary_t old_summary;
    W25Q_Status_t w25q_status;
    uint8_t check_buffer[16];
    uint32_t i;
    uint8_t need_erase = 0;
    uint8_t dead_data_already_recorded = 0;
    
    if (result == NULL)
    {
        return ENDURANCE_TEST_ERROR_NULL_PTR;
    }
    
    if (!g_endurance_test_initialized)
    {
        return ENDURANCE_TEST_ERROR_NOT_INIT;
    }
    
    /* 尝试读取现有的汇总信息，检查损坏时数据是否已记录 */
    w25q_status = W25Q_Read(ENDURANCE_TEST_SUMMARY_ADDR, (uint8_t *)&old_summary, sizeof(Endurance_Test_Summary_t));
    if (w25q_status == W25Q_OK && old_summary.magic == ENDURANCE_TEST_MAGIC)
    {
        /* 如果损坏时数据已记录，保留它 */
        if (old_summary.dead_data_recorded)
        {
            dead_data_already_recorded = 1;
            memcpy(&summary.dead_data, &old_summary.dead_data, sizeof(Endurance_Test_DeadData_t));
        }
    }
    
    /* 检查Block 0是否需要擦除（读取前16字节，检查是否有0值） */
    w25q_status = W25Q_Read(ENDURANCE_TEST_SUMMARY_ADDR, check_buffer, sizeof(check_buffer));
    if (w25q_status == W25Q_OK)
    {
        for (i = 0; i < sizeof(check_buffer); i++)
        {
            if (check_buffer[i] != 0xFF)
            {
                need_erase = 1;
                break;
            }
        }
    }
    
    /* 如果需要擦除，先擦除Block 0的第一个扇区 */
    /* 警告：在擦除（E）状态时，严禁断电或重启！否则汇总记录可能丢失，无法恢复测试进度 */
    if (need_erase)
    {
#if CONFIG_MODULE_LOG_ENABLED
        LOG_INFO("ENDURANCE_TEST", "擦除Block 0的第一个扇区（为保存汇总信息做准备）...");
#endif
        /* 警告：此时处于擦除（E）状态，严禁断电或重启！ */
        w25q_status = W25Q_EraseSector(ENDURANCE_TEST_SUMMARY_ADDR);
        if (w25q_status != W25Q_OK)
        {
#if CONFIG_MODULE_LOG_ENABLED
            LOG_ERROR("ENDURANCE_TEST", "擦除Block 0的第一个扇区失败: %d", w25q_status);
#endif
            return ENDURANCE_TEST_ERROR_W25Q_FAILED;
        }
        
        /* 等待擦除完成 */
        /* 警告：擦除操作进行中，严禁断电或重启！ */
        w25q_status = W25Q_WaitReady(5000);
        if (w25q_status != W25Q_OK)
        {
#if CONFIG_MODULE_LOG_ENABLED
            LOG_ERROR("ENDURANCE_TEST", "等待擦除完成超时");
#endif
            return ENDURANCE_TEST_ERROR_TIMEOUT;
        }
    }
    
    /* 构建汇总信息结构体 */
    summary.magic = ENDURANCE_TEST_MAGIC;
    summary.version = ENDURANCE_TEST_VERSION;
    summary.reserved = 0;
    
    /* 设置测试状态 */
    if (result->chip_dead)
    {
        summary.test_status = TEST_STATUS_COMPLETED;  /* 芯片已报废，测试已完成 */
    }
    else
    {
        summary.test_status = TEST_STATUS_RUNNING;    /* 测试进行中 */
    }
    
    /* 保存基准数据（首轮数据） */
    memcpy(&summary.baseline, &result->baseline, sizeof(Baseline_Data_t));
    
    /* 保存进行时数据（当前测试状态） */
    /* 注意：端粒进度应该在调用SaveSummary之前已经计算好了 */
    memcpy(&summary.result, result, sizeof(Endurance_Test_Result_t));
    
    /* 如果芯片已报废且损坏时数据未记录，记录损坏时数据 */
    if (result->chip_dead && !dead_data_already_recorded)
    {
        summary.dead_data.dead_cycle = result->total_cycles;
        summary.dead_data.erase_time_final = result->erase_time_avg;
        summary.dead_data.program_time_final = result->program_time_avg;
        summary.dead_data.read_speed_final = result->read_speed;
        summary.dead_data.erase_errors_final = result->erase_errors;
        summary.dead_data.program_errors_final = result->program_errors;
        summary.dead_data.verify_errors_final = result->verify_errors;
        summary.dead_data.bad_block_count_final = result->bad_block_count;
        summary.dead_data.error_rate_final = result->error_rate;
        summary.dead_data.telomere_progress_final = result->telomere_progress;
        summary.dead_data.chip_status_final = result->chip_status;
        summary.dead_data.total_data_written_mb_final = result->total_data_written_mb;
        summary.dead_data_recorded = 1;
        
#if CONFIG_MODULE_LOG_ENABLED
        LOG_INFO("ENDURANCE_TEST", "记录损坏时数据（芯片已报废）");
#endif
    }
    else
    {
        /* 保持原有的损坏时数据不变（如果已记录） */
        if (dead_data_already_recorded)
        {
            summary.dead_data_recorded = 1;
        }
        else
        {
            summary.dead_data_recorded = 0;
            memset(&summary.dead_data, 0, sizeof(Endurance_Test_DeadData_t));
        }
    }
    
    /* 写入汇总信息 */
    /* 警告：在写入汇总信息期间，严禁断电或重启！否则汇总记录可能丢失，无法恢复测试进度 */
#if CONFIG_MODULE_LOG_ENABLED
    if (result->total_cycles % 100 == 0 || result->total_cycles <= 10)
    {
        LOG_INFO("ENDURANCE_TEST", "准备保存汇总信息到地址 0x%08lX（第 %u 次循环，魔数: 0x%08lX）", 
                 (unsigned long)ENDURANCE_TEST_SUMMARY_ADDR, 
                 result->total_cycles,
                 (unsigned long)summary.magic);
    }
#endif
    /* 警告：此时正在写入汇总记录，严禁断电或重启！否则汇总记录可能丢失 */
    w25q_status = W25Q_Write(ENDURANCE_TEST_SUMMARY_ADDR, (uint8_t *)&summary, sizeof(Endurance_Test_Summary_t));
    if (w25q_status != W25Q_OK)
    {
#if CONFIG_MODULE_LOG_ENABLED
        LOG_ERROR("ENDURANCE_TEST", "写入汇总信息失败: %d", w25q_status);
#endif
        return ENDURANCE_TEST_ERROR_W25Q_FAILED;
    }
    
    /* 等待写入完成 */
    /* 警告：写入操作进行中，严禁断电或重启！ */
    w25q_status = W25Q_WaitReady(2000);
    if (w25q_status != W25Q_OK)
    {
#if CONFIG_MODULE_LOG_ENABLED
        LOG_ERROR("ENDURANCE_TEST", "等待写入完成超时");
#endif
        return ENDURANCE_TEST_ERROR_TIMEOUT;
    }
    /* 汇总记录已保存完成，此时可以安全断电或重启 */
    
#if CONFIG_MODULE_LOG_ENABLED
    /* 每100次循环输出一次保存日志，避免日志过多 */
    if (result->total_cycles % 100 == 0 || result->total_cycles <= 10)
    {
        LOG_INFO("ENDURANCE_TEST", "汇总信息已保存（第 %u 次循环）", result->total_cycles);
        
        /* 验证写入：读取前4字节检查魔数 */
        uint32_t verify_magic = 0;
        W25Q_Read(ENDURANCE_TEST_SUMMARY_ADDR, (uint8_t *)&verify_magic, 4);
        if (verify_magic == ENDURANCE_TEST_MAGIC)
        {
            LOG_INFO("ENDURANCE_TEST", "验证成功：魔数匹配 (0x%08lX)", (unsigned long)verify_magic);
        }
        else
        {
            LOG_WARN("ENDURANCE_TEST", "验证失败：魔数不匹配 (期望: 0x%08lX, 实际: 0x%08lX)", 
                     (unsigned long)ENDURANCE_TEST_MAGIC, (unsigned long)verify_magic);
        }
    }
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
    LOG_INFO("ENDURANCE_TEST", "深度检查间隔: %u 次循环", config->deep_check_interval);
    LOG_INFO("ENDURANCE_TEST", "日志记录间隔: %u 次循环", config->log_interval);
#endif
    
    /* ========== 步骤0：尝试从Flash加载汇总信息（断点续测） ========== */
    test_status = EnduranceTest_LoadSummary(result);
    if (test_status == ENDURANCE_TEST_OK)
    {
        /* 成功加载汇总信息，从断点恢复测试 */
        /* 跳过基准数据记录（如果已记录） */
        if (result->baseline_recorded)
        {
#if CONFIG_MODULE_LOG_ENABLED
            LOG_INFO("ENDURANCE_TEST", "基准数据已存在，跳过基准数据记录");
#endif
        }
        else
        {
            /* 如果基准数据未记录，需要记录基准数据 */
            test_status = EnduranceTest_RecordBaseline(result);
            if (test_status != ENDURANCE_TEST_OK)
            {
#if CONFIG_MODULE_LOG_ENABLED
                LOG_WARN("ENDURANCE_TEST", "基准数据记录失败，继续测试");
#endif
            }
            else
            {
                /* 记录基准数据后立即保存汇总信息（确保断电后能恢复） */
                test_status = EnduranceTest_SaveSummary(result);
                if (test_status != ENDURANCE_TEST_OK)
                {
#if CONFIG_MODULE_LOG_ENABLED
                    LOG_WARN("ENDURANCE_TEST", "保存汇总信息失败（将在第一次循环后重试）");
#endif
                }
            }
        }
    }
    else
    {
        /* 未找到有效汇总信息，从0开始测试 */
#if CONFIG_MODULE_LOG_ENABLED
        LOG_INFO("ENDURANCE_TEST", "未找到有效汇总信息，从0开始测试");
#endif
        
        /* ========== 步骤1：记录基准数据 ========== */
        test_status = EnduranceTest_RecordBaseline(result);
        if (test_status != ENDURANCE_TEST_OK)
        {
#if CONFIG_MODULE_LOG_ENABLED
            LOG_WARN("ENDURANCE_TEST", "基准数据记录失败，继续测试");
#endif
        }
        else
        {
            /* 记录基准数据后立即保存汇总信息（确保断电后能恢复） */
            test_status = EnduranceTest_SaveSummary(result);
            if (test_status == ENDURANCE_TEST_OK)
            {
#if CONFIG_MODULE_LOG_ENABLED
                LOG_INFO("ENDURANCE_TEST", "基准数据已记录并保存汇总信息");
#endif
            }
            else
            {
#if CONFIG_MODULE_LOG_ENABLED
                LOG_WARN("ENDURANCE_TEST", "基准数据已记录，但保存汇总信息失败（将在第一次循环后重试）");
#endif
            }
        }
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
        
        /* 每次P/E循环完成后保存汇总信息（确保断电后能恢复最新进度） */
        /* 注意：在连续擦除模式下，虽然每次擦除后轮次会更新，但只有在完整的P/E循环（擦除+写入+读取）完成后才保存 */
        /* 这样可以确保如果重启发生在写入或读取过程中，至少能恢复到上一次完整的P/E循环 */
        if (test_status == ENDURANCE_TEST_OK)
        {
            /* 保存汇总信息（端粒进度已在EnduranceTest_RunSingleCycle中计算） */
            test_status = EnduranceTest_SaveSummary(result);
            if (test_status != ENDURANCE_TEST_OK)
            {
#if CONFIG_MODULE_LOG_ENABLED
                LOG_WARN("ENDURANCE_TEST", "保存汇总信息失败（将在下次循环后重试）");
#endif
            }
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
    LOG_INFO("ENDURANCE_TEST", "  总P/E循环次数: %u 次", result->total_cycles);
    LOG_INFO("ENDURANCE_TEST", "  总写入数据量: %llu MB (%.2f GB)", 
             (unsigned long long)result->total_data_written_mb, 
             (float)result->total_data_written_mb / 1024.0f);
    LOG_INFO("ENDURANCE_TEST", "");
    LOG_INFO("ENDURANCE_TEST", "【时间统计】");
    /* 计算每块平均擦除时间 */
    const w25q_dev_t *dev_info_final = W25Q_GetInfo();
    uint32_t total_blocks_final = 0;
    if (dev_info_final != NULL)
    {
        uint64_t capacity_bytes_final = (uint64_t)dev_info_final->capacity_mb * 1024ULL * 1024ULL;
        total_blocks_final = (uint32_t)(capacity_bytes_final / W25Q_BLOCK_SIZE);
    }
    float erase_time_per_block_avg_ms = 0.0f;
    if (total_blocks_final > 0)
    {
        erase_time_per_block_avg_ms = result->erase_time_avg / (float)total_blocks_final;
    }
    LOG_INFO("ENDURANCE_TEST", "  块擦除时间: %.2f 秒 (%.2f ms/块)", 
             result->erase_time_avg / 1000.0f, erase_time_per_block_avg_ms);
    LOG_INFO("ENDURANCE_TEST", "  页编程时间: %.3f ms/页", result->program_time_avg);
    LOG_INFO("ENDURANCE_TEST", "  读取速度: %.2f KB/s", result->read_speed);
    LOG_INFO("ENDURANCE_TEST", "");
    LOG_INFO("ENDURANCE_TEST", "【错误统计】");
    LOG_INFO("ENDURANCE_TEST", "  擦除错误次数: %u", result->erase_errors);
    LOG_INFO("ENDURANCE_TEST", "  编程错误次数: %u", result->program_errors);
    LOG_INFO("ENDURANCE_TEST", "  校验错误次数: %u 位", result->verify_errors);
    LOG_INFO("ENDURANCE_TEST", "  坏块数量: %u", result->bad_block_count);
    {
        char error_rate_str[32];
        FormatErrorRate(result->error_rate, error_rate_str, sizeof(error_rate_str));
        LOG_INFO("ENDURANCE_TEST", "  最终误码率: %s", error_rate_str);
    }
    LOG_INFO("ENDURANCE_TEST", "  读干扰错误: %u", result->read_disturb_errors);
    LOG_INFO("ENDURANCE_TEST", "");
    LOG_INFO("ENDURANCE_TEST", "【性能退化】");
    if (result->baseline_recorded)
    {
        LOG_INFO("ENDURANCE_TEST", "  擦除时间退化率: %.2f%%", result->erase_degradation_rate);
        LOG_INFO("ENDURANCE_TEST", "  编程时间退化率: %.2f%%", result->program_degradation_rate);
        LOG_INFO("ENDURANCE_TEST", "  读取速度退化率: %.2f%%", result->read_speed_degradation_rate);
        LOG_INFO("ENDURANCE_TEST", "  基准块擦除时间: %.2f 秒", result->baseline.erase_time_avg / 1000.0f);
        LOG_INFO("ENDURANCE_TEST", "  基准页编程时间: %.3f ms/页", result->baseline.program_time_avg);
        LOG_INFO("ENDURANCE_TEST", "  基准读取速度: %.2f KB/s", result->baseline.read_speed);
    }
    LOG_INFO("ENDURANCE_TEST", "");
    LOG_INFO("ENDURANCE_TEST", "【最终状态】");
    LOG_INFO("ENDURANCE_TEST", "  最终端粒进度: %.2f%%", result->telomere_progress);
    LOG_INFO("ENDURANCE_TEST", "  芯片状态: %s", result->chip_dead ? "已报废" : "正常");
    LOG_INFO("ENDURANCE_TEST", "");
    LOG_INFO("ENDURANCE_TEST", "========================================");
#endif
    
    /* 更新OLED显示（报废时显示错误统计） */
#if CONFIG_MODULE_OLED_ENABLED
    {
        char oled_line1[17];
        char oled_line2[17];
        char oled_line3[17];
        char oled_line4[17];
        
        /* 第1行：擦除错误次数 */
        snprintf(oled_line1, sizeof(oled_line1), "EraseErr: %u", result->erase_errors);
        OLED_ShowString(1, 1, oled_line1);
        
        /* 第2行：编程错误次数 */
        snprintf(oled_line2, sizeof(oled_line2), "ProgErr: %u", result->program_errors);
        OLED_ShowString(2, 1, oled_line2);
        
        /* 第3行：校验错误次数（显示为K位，如果超过1000） */
        if (result->verify_errors >= 1000)
        {
            snprintf(oled_line3, sizeof(oled_line3), "VerifyErr: %uK", result->verify_errors / 1000);
        }
        else
        {
            snprintf(oled_line3, sizeof(oled_line3), "VerifyErr: %u", result->verify_errors);
        }
        OLED_ShowString(3, 1, oled_line3);
        
        /* 第4行：坏块数量 */
        snprintf(oled_line4, sizeof(oled_line4), "BadBlock: %u", result->bad_block_count);
        OLED_ShowString(4, 1, oled_line4);
    }
#endif
    
    return ENDURANCE_TEST_OK;
}

