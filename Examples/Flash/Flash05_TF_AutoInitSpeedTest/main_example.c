/**
 * @file main_example.c
 * @brief Flash05 - TF卡（MicroSD卡）读写测速示例
 * @details 演示TF卡高级API使用、不同分频下的1MB测速、增量写入（100KB）和插拔卡处理
 * 
 * 硬件连接：
 * - TF卡（MicroSD卡）连接到SPI2
 *   - CS：PA11（软件NSS模式）
 *   - SCK：PB13（SPI2_SCK）
 *   - MISO：PB14（SPI2_MISO）
 *   - MOSI：PB15（SPI2_MOSI）
 *   - VCC：3.3V
 *   - GND：GND
 * - OLED显示屏（用于显示关键信息）
 *   - SCL：PB8
 *   - SDA：PB9
 * - UART1（用于详细日志输出）
 *   - TX：PA9
 *   - RX：PA10
 * 
 * 功能演示：
 * 1. 高级API函数演示（TF_SPI_Init、TF_SPI_GetInfo、TF_SPI_ReadBlock等）
 * 2. 不同SPI分频下的1MB读写速度测试（优化后使用32块批量传输）
 * 3. 增量写入功能（每分钟写入100KB，使用8分频，读取全部并校验）
 * 4. 插拔卡检测和自动重初始化
 * 
 * @note 本示例使用TF_SPI模块的高级API
 * @note 测速测试：1MB测试数据，使用32块批量传输（约16KB）提高效率，便于调试
 * @note 增量写入：100KB数据，使用8分频（9MHz）标准速度
 * @note 测速测试已优化，预计每个分频耗时1-3秒
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "led.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include "i2c_sw.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "spi_hw.h"
#include "tf_spi.h"
#include "gpio.h"
#include "config.h"
#include "board.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ==================== 测试配置 ==================== */

/* 测速测试数据大小：1MB（便于调试） */
#define SPEED_TEST_SIZE_MB       1
#define SPEED_TEST_SIZE_BYTES    (SPEED_TEST_SIZE_MB * 1024 * 1024)
#define SPEED_TEST_BLOCK_COUNT   (SPEED_TEST_SIZE_BYTES / 512)  /* 每块512字节 */

/* 增量写入配置 */
#define INCREMENTAL_WRITE_SIZE_KB    100
#define INCREMENTAL_WRITE_BLOCK_COUNT (INCREMENTAL_WRITE_SIZE_KB * 1024 / 512)  /* 100KB = 200块 */
#define INCREMENTAL_WRITE_START_BLOCK 1000  /* 起始块地址（避开系统区域） */
#define INCREMENTAL_WRITE_INTERVAL_MS 5000   /* 增量写入间隔：5秒 */
#define INCREMENTAL_WRITE_PRESCALER   SPI_BaudRatePrescaler_8  /* 增量写入使用8分频（9MHz） */
#define INCREMENTAL_WRITE_MAX_COUNT   10     /* 最大写入次数（用于测试插拔卡功能） */

/* 插拔卡检测间隔 */
#define CARD_DETECT_INTERVAL_MS  5000  /* 每5秒检测一次 */

/* SPI分频测试列表 */
/* 注意：初始化时使用256分频（≤400kHz），初始化完成后可以切换到更高速度 */
/* 从2分频开始测试，如果失败则自动跳过（已有测试写入验证机制） */
#define PRESCALER_COUNT       8
static const uint16_t g_prescalers[PRESCALER_COUNT] = {
    SPI_BaudRatePrescaler_2,      /* 分频2（36MHz，最高速度） */
    SPI_BaudRatePrescaler_4,      /* 分频4（18MHz） */
    SPI_BaudRatePrescaler_8,      /* 分频8（9MHz） */
    SPI_BaudRatePrescaler_16,     /* 分频16（4.5MHz） */
    SPI_BaudRatePrescaler_32,     /* 分频32（2.25MHz） */
    SPI_BaudRatePrescaler_64,     /* 分频64（1.125MHz） */
    SPI_BaudRatePrescaler_128,    /* 分频128（562.5kHz） */
    SPI_BaudRatePrescaler_256,    /* 分频256（281.25kHz） */
};

/* 分频值对应的数值（用于显示） */
static const uint16_t g_prescaler_values[PRESCALER_COUNT] = {2, 4, 8, 16, 32, 64, 128, 256};

/* ==================== 全局状态变量 ==================== */

/* 速度测试结果结构体 */
typedef struct {
    uint16_t prescaler_value;     /**< 分频值（2, 4, 8...） */
    uint32_t write_time_ms;        /**< 写入耗时（毫秒） */
    uint32_t read_time_ms;         /**< 读取耗时（毫秒） */
    float write_speed_kbps;        /**< 写入速度（KB/s） */
    float read_speed_kbps;         /**< 读取速度（KB/s） */
} SpeedTestResult_t;

/* 测试结果数组 */
static SpeedTestResult_t g_speed_test_results[PRESCALER_COUNT];

/* 测速测试缓冲区（静态分配，避免使用malloc） */
/* 注意：STM32F103C8T6只有20KB RAM，使用16KB缓冲区 */
/* 增加到32块（约16KB），提高传输效率 */
static uint8_t g_speed_test_buffer[512 * 32];  /* 每次处理32块（约16KB） */

/* 增量写入状态 */
typedef struct {
    uint32_t current_block;        /**< 当前写入块地址 */
    uint32_t write_count;          /**< 写入次数 */
    uint32_t last_write_time_ms;   /**< 上次写入时间（毫秒） */
    uint8_t initialized;           /**< 是否已初始化 */
} IncrementalWriteState_t;

static IncrementalWriteState_t g_incremental_write_state = {
    .current_block = INCREMENTAL_WRITE_START_BLOCK,
    .write_count = 0,
    .last_write_time_ms = 0,
    .initialized = 0
};

/* 插拔卡检测状态 */
typedef struct {
    uint32_t last_detect_time_ms;  /**< 上次检测时间（毫秒） */
    uint8_t card_present;          /**< 卡是否存在（1=存在，0=不存在） */
    uint8_t last_init_status;      /**< 上次初始化状态 */
} CardDetectState_t;

static CardDetectState_t g_card_detect_state = {
    .last_detect_time_ms = 0,
    .card_present = 0,
    .last_init_status = 0
};

/* ==================== 辅助函数 ==================== */

/**
 * @brief 动态修改SPI分频
 * @param[in] prescaler SPI分频值（SPI_BaudRatePrescaler_2等）
 * @return SPI_Status_t 错误码
 * @note 直接操作SPI2的CR1寄存器修改BR位（bit 3-5）
 */
static SPI_Status_t ChangeSPIPrescaler(uint16_t prescaler)
{
    SPI_TypeDef *spi_periph = SPI2;
    uint16_t cr1_temp;
    
    if (spi_periph == NULL)
    {
        return SPI_ERROR_INVALID_PERIPH;
    }
    
    /* 等待SPI总线空闲 */
    {
        uint32_t timeout_count = 10000;
        while (SPI_I2S_GetFlagStatus(spi_periph, SPI_I2S_FLAG_BSY) == SET)
        {
            if (timeout_count-- == 0)
            {
                return SPI_ERROR_TIMEOUT;
            }
        }
    }
    
    /* 禁用SPI（修改配置前必须禁用） */
    SPI_Cmd(spi_periph, DISABLE);
    
    /* 读取CR1寄存器 */
    cr1_temp = spi_periph->CR1;
    
    /* 清除BR位（bit 3-5），BR位掩码：0x38 (二进制 111000) */
    cr1_temp &= ~(0x38);
    
    /* 设置新的分频值 */
    cr1_temp |= prescaler;
    
    /* 写回CR1寄存器 */
    spi_periph->CR1 = cr1_temp;
    
    /* 重新使能SPI */
    SPI_Cmd(spi_periph, ENABLE);
    
    /* 等待SPI总线稳定 */
    Delay_us(10);
    
    return SPI_OK;
}

/**
 * @brief 获取分频值对应的数值
 * @param[in] prescaler SPI分频宏定义值
 * @return uint16_t 分频数值（2, 4, 8...）
 */
static uint16_t GetPrescalerValue(uint16_t prescaler)
{
    uint8_t i;
    for (i = 0; i < PRESCALER_COUNT; i++)
    {
        if (g_prescalers[i] == prescaler)
        {
            return g_prescaler_values[i];
        }
    }
    return 0;
}

/**
 * @brief 计算速度（KB/s）
 * @param[in] size_bytes 数据大小（字节）
 * @param[in] time_ms 耗时（毫秒）
 * @return float 速度（KB/s）
 */
static float CalculateSpeed(uint32_t size_bytes, uint32_t time_ms)
{
    if (time_ms == 0)
    {
        return 0.0f;
    }
    
    /* 速度 = 数据大小(KB) / 耗时(秒) = (size_bytes / 1024) / (time_ms / 1000) */
    return ((float)size_bytes / 1024.0f) / ((float)time_ms / 1000.0f);
}

/* ==================== 演示1：高级API函数演示 ==================== */

/**
 * @brief 演示高级API函数列表和用法
 */
static void DemoHighLevelAPI(void)
{
    const tf_spi_dev_t *dev_info;
    
    LOG_INFO("MAIN", "=== 演示1：TF_SPI高级API函数列表 ===");
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "1. TF_SPI_Init()");
    LOG_INFO("MAIN", "   功能：自动初始化TF卡，检测卡类型并配置");
    LOG_INFO("MAIN", "   返回：TF_SPI_Status_t（TF_SPI_OK表示成功）");
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "2. TF_SPI_GetInfo()");
    LOG_INFO("MAIN", "   功能：获取设备信息（容量、块大小、块数量、卡类型）");
    LOG_INFO("MAIN", "   返回：const tf_spi_dev_t*（NULL表示未初始化）");
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "3. TF_SPI_IsInitialized()");
    LOG_INFO("MAIN", "   功能：检查TF卡是否已初始化");
    LOG_INFO("MAIN", "   返回：uint8_t（1=已初始化，0=未初始化）");
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "4. TF_SPI_ReadBlock(block_addr, buf)");
    LOG_INFO("MAIN", "   功能：读取单个块（512字节）");
    LOG_INFO("MAIN", "   参数：block_addr（块地址），buf（512字节缓冲区）");
    LOG_INFO("MAIN", "   返回：TF_SPI_Status_t");
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "5. TF_SPI_WriteBlock(block_addr, buf)");
    LOG_INFO("MAIN", "   功能：写入单个块（512字节）");
    LOG_INFO("MAIN", "   参数：block_addr（块地址），buf（512字节数据）");
    LOG_INFO("MAIN", "   返回：TF_SPI_Status_t");
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "6. TF_SPI_ReadBlocks(block_addr, block_count, buf)");
    LOG_INFO("MAIN", "   功能：读取多个块");
    LOG_INFO("MAIN", "   参数：block_addr（起始块地址），block_count（块数量），buf（缓冲区）");
    LOG_INFO("MAIN", "   返回：TF_SPI_Status_t");
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "7. TF_SPI_WriteBlocks(block_addr, block_count, buf)");
    LOG_INFO("MAIN", "   功能：写入多个块");
    LOG_INFO("MAIN", "   参数：block_addr（起始块地址），block_count（块数量），buf（数据）");
    LOG_INFO("MAIN", "   返回：TF_SPI_Status_t");
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "=== 当前设备信息 ===");
    
    dev_info = TF_SPI_GetInfo();
    if (dev_info != NULL)
    {
        LOG_INFO("MAIN", "容量: %d MB", dev_info->capacity_mb);
        LOG_INFO("MAIN", "块大小: %d 字节", dev_info->block_size);
        LOG_INFO("MAIN", "块数量: %d", dev_info->block_count);
        LOG_INFO("MAIN", "卡类型: %s", 
                 dev_info->card_type == TF_SPI_CARD_TYPE_SDSC ? "SDSC" :
                 dev_info->card_type == TF_SPI_CARD_TYPE_SDHC ? "SDHC" :
                 dev_info->card_type == TF_SPI_CARD_TYPE_SDXC ? "SDXC" : "Unknown");
    }
    else
    {
        LOG_WARN("MAIN", "设备未初始化，无法获取信息");
    }
    
    /* OLED显示 */
    OLED_Clear();
    OLED_ShowString(1, 1, "API Demo");
    OLED_ShowString(2, 1, "7 Functions");
    if (dev_info != NULL)
    {
        char buf[17];
        snprintf(buf, sizeof(buf), "Cap: %d MB", dev_info->capacity_mb);
        OLED_ShowString(3, 1, buf);
        OLED_ShowString(4, 1, "See UART Log");
    }
    else
    {
        OLED_ShowString(3, 1, "Not Init");
    }
    
    Delay_ms(3000);
}

/* ==================== 演示2：测速功能 ==================== */

/**
 * @brief 在OLED上显示当前测试状态
 * @param[in] prescaler_value 当前分频值
 * @param[in] test_index 当前测试索引（0-7）
 * @param[in] total_tests 总测试数
 * @param[in] operation 操作类型（"Write"或"Read"）
 */
static void DisplaySpeedTestStatus(uint16_t prescaler_value, uint8_t test_index, 
                                   uint8_t total_tests, const char *operation)
{
    char buffer[17];
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Speed Test");
    snprintf(buffer, sizeof(buffer), "P:%d %d/%d", prescaler_value, test_index + 1, total_tests);
    OLED_ShowString(2, 1, buffer);
    snprintf(buffer, sizeof(buffer), "%s %dMB...", operation, SPEED_TEST_SIZE_MB);
    OLED_ShowString(3, 1, buffer);
    OLED_ShowString(4, 1, "Please wait...");
}

/**
 * @brief 准备测试数据（递增序列）
 * @param[out] buffer 数据缓冲区
 * @param[in] size 数据大小
 */
static void PrepareTestData(uint8_t *buffer, uint32_t size)
{
    uint32_t i;
    for (i = 0; i < size; i++)
    {
        buffer[i] = (uint8_t)(i & 0xFF);  /* 递增序列：0x00-0xFF循环 */
    }
}

/**
 * @brief 执行测速测试
 * @return uint8_t 1-成功，0-失败
 */
static uint8_t PerformSpeedTest(void)
{
    TF_SPI_Status_t tf_status;
    SPI_Status_t spi_status;
    uint8_t test_index;
    uint32_t start_time, end_time;
    uint32_t test_start_block = 1000;  /* 测试起始块地址 */
    uint8_t *test_buffer;
    uint32_t buffer_size;
    const tf_spi_dev_t *dev_info;
    
    LOG_INFO("MAIN", "=== 演示2：不同分频下的1MB测速测试 ===");
    LOG_INFO("MAIN", "测试数据大小: %d MB (%d 块)", SPEED_TEST_SIZE_MB, SPEED_TEST_BLOCK_COUNT);
    LOG_INFO("MAIN", "测试分频: 2, 4, 8, 16, 32, 64, 128, 256");
    LOG_INFO("MAIN", "注意：初始化时使用256分频（≤400kHz），初始化完成后可切换到更高速度");
    LOG_INFO("MAIN", "如果某个分频测试失败，会自动跳过该分频");
    LOG_INFO("MAIN", "");
    
    /* 检查设备信息 */
    dev_info = TF_SPI_GetInfo();
    if (dev_info == NULL)
    {
        LOG_WARN("MAIN", "设备未初始化，尝试重新初始化...");
        /* 先恢复到初始化速度 */
        ChangeSPIPrescaler(SPI_BaudRatePrescaler_256);
        Delay_ms(10);
        
        TF_SPI_Status_t reinit_status = TF_SPI_Init();
        if (reinit_status != TF_SPI_OK)
        {
            LOG_ERROR("MAIN", "SD卡重新初始化失败: %d，无法执行测速测试", reinit_status);
            return 0;
        }
        
        /* 重新获取设备信息 */
        dev_info = TF_SPI_GetInfo();
        if (dev_info == NULL)
        {
            LOG_ERROR("MAIN", "重新初始化后仍无法获取设备信息");
            return 0;
        }
    }
    
    /* 检查容量是否足够 */
    if (test_start_block + SPEED_TEST_BLOCK_COUNT > dev_info->block_count)
    {
        LOG_ERROR("MAIN", "SD卡容量不足，无法执行1MB测试");
        LOG_ERROR("MAIN", "需要: %d 块，可用: %d 块", 
                 test_start_block + SPEED_TEST_BLOCK_COUNT, dev_info->block_count);
        return 0;
    }
    
    /* 使用全局静态缓冲区（避免使用malloc） */
    buffer_size = sizeof(g_speed_test_buffer);  /* 约5KB */
    test_buffer = g_speed_test_buffer;
    
    /* 准备测试数据 */
    PrepareTestData(test_buffer, buffer_size);
    
    /* 循环测试所有分频 */
    for (test_index = 0; test_index < PRESCALER_COUNT; test_index++)
    {
        uint16_t prescaler = g_prescalers[test_index];
        uint16_t prescaler_value = GetPrescalerValue(prescaler);
        uint32_t block_idx;
        uint32_t blocks_processed = 0;
        
        LOG_INFO("MAIN", "--- 测试分频 %d (%d/%d) ---", prescaler_value, test_index + 1, PRESCALER_COUNT);
        
        /* 检查SD卡状态，如果未初始化则尝试重新初始化 */
        if (!TF_SPI_IsInitialized())
        {
            LOG_WARN("MAIN", "SD卡未初始化，尝试重新初始化...");
            /* 先恢复到初始化速度 */
            ChangeSPIPrescaler(SPI_BaudRatePrescaler_256);
            Delay_ms(10);
            
            TF_SPI_Status_t reinit_status = TF_SPI_Init();
            if (reinit_status != TF_SPI_OK)
            {
                LOG_WARN("MAIN", "SD卡重新初始化失败: %d，跳过此分频测试", reinit_status);
                g_speed_test_results[test_index].write_time_ms = 0;
                g_speed_test_results[test_index].read_time_ms = 0;
                g_speed_test_results[test_index].write_speed_kbps = 0.0f;
                g_speed_test_results[test_index].read_speed_kbps = 0.0f;
                continue;  /* 跳过此分频，继续下一个 */
            }
            else
            {
                LOG_INFO("MAIN", "SD卡重新初始化成功，继续测试");
            }
        }
        
        /* 修改SPI分频 */
        spi_status = ChangeSPIPrescaler(prescaler);
        if (spi_status != SPI_OK)
        {
            LOG_ERROR("MAIN", "修改SPI分频失败: %d", spi_status);
            continue;
        }
        
        /* 分频修改后，等待SPI总线稳定，并发送一些dummy字节同步 */
        Delay_ms(10);
        
        /* 先测试写入1块数据，验证写入功能是否正常 */
        LOG_INFO("MAIN", "测试写入1块数据验证功能...");
        PrepareTestData(test_buffer, 512);
        tf_status = TF_SPI_WriteBlock(test_start_block, test_buffer);
        if (tf_status != TF_SPI_OK)
        {
            LOG_ERROR("MAIN", "测试写入失败，块地址: %lu, 状态: %d", test_start_block, tf_status);
            LOG_ERROR("MAIN", "分频 %d 可能太快，跳过此分频", prescaler_value);
            g_speed_test_results[test_index].write_time_ms = 0;
            g_speed_test_results[test_index].read_time_ms = 0;
            g_speed_test_results[test_index].write_speed_kbps = 0.0f;
            g_speed_test_results[test_index].read_speed_kbps = 0.0f;
            continue;
        }
        LOG_INFO("MAIN", "测试写入成功，开始正式写入测试");
        
        /* 写入测试 */
        DisplaySpeedTestStatus(prescaler_value, test_index, PRESCALER_COUNT, "Write");
        LOG_INFO("MAIN", "开始写入测试...");
        LOG_INFO("MAIN", "测试起始块: %lu, 总块数: %lu", test_start_block, SPEED_TEST_BLOCK_COUNT);
        
        start_time = Delay_GetTick();
        
        /* 进度显示变量（每个分频测试开始时重置） */
        uint8_t last_log_percent_write = 0;
        uint8_t last_oled_percent_write = 0;
        
        /* 分块写入（每次32块，约16KB，提高传输效率） */
        for (block_idx = 0; block_idx < SPEED_TEST_BLOCK_COUNT; block_idx += 32)
        {
            uint32_t blocks_to_write = (SPEED_TEST_BLOCK_COUNT - block_idx > 32) ? 32 : (SPEED_TEST_BLOCK_COUNT - block_idx);
            uint32_t current_block = test_start_block + block_idx;
            
            /* 更新测试数据（包含块索引信息） */
            PrepareTestData(test_buffer, blocks_to_write * 512);
            
            /* 执行写入 */
            /* 写入前检查SD卡状态 */
            if (!TF_SPI_IsInitialized())
            {
                LOG_WARN("MAIN", "写入过程中检测到SD卡拔出，跳过此分频测试");
                g_speed_test_results[test_index].write_time_ms = 0;
                g_speed_test_results[test_index].read_time_ms = 0;
                g_speed_test_results[test_index].write_speed_kbps = 0.0f;
                g_speed_test_results[test_index].read_speed_kbps = 0.0f;
                break;  /* 跳出写入循环，继续下一个分频 */
            }
            
            tf_status = TF_SPI_WriteBlocks(current_block, blocks_to_write, test_buffer);
            if (tf_status != TF_SPI_OK)
            {
                LOG_ERROR("MAIN", "写入失败，块地址: %lu, 状态: %d", current_block, tf_status);
                LOG_WARN("MAIN", "跳过此分频的写入测试，继续下一个分频");
                g_speed_test_results[test_index].write_time_ms = 0;
                g_speed_test_results[test_index].read_time_ms = 0;
                g_speed_test_results[test_index].write_speed_kbps = 0.0f;
                g_speed_test_results[test_index].read_speed_kbps = 0.0f;
                break;  /* 跳出写入循环，继续下一个分频 */
            }
            
            blocks_processed += blocks_to_write;
            
            /* 按10%输出进度和更新OLED（基于百分比判断，而不是取模） */
            {
                uint8_t current_percent = (blocks_processed * 100) / SPEED_TEST_BLOCK_COUNT;
                
                /* 串口日志：每10%输出一次 */
                if (current_percent >= last_log_percent_write + 10 || blocks_processed >= SPEED_TEST_BLOCK_COUNT)
                {
                    LOG_INFO("MAIN", "写入进度: %lu/%lu 块 (%d%%)", 
                             blocks_processed, SPEED_TEST_BLOCK_COUNT, current_percent);
                    last_log_percent_write = (current_percent / 10) * 10;  /* 向下取整到10的倍数 */
                }
                
                /* OLED显示：每20%更新一次 */
                if (current_percent >= last_oled_percent_write + 20 || blocks_processed >= SPEED_TEST_BLOCK_COUNT)
                {
                    char buf[17];
                    snprintf(buf, sizeof(buf), "Write: %d%%", current_percent);
                    OLED_ShowString(4, 1, buf);
                    last_oled_percent_write = (current_percent / 20) * 20;  /* 向下取整到20的倍数 */
                }
            }
        }
        
        end_time = Delay_GetTick();
        g_speed_test_results[test_index].write_time_ms = Delay_GetElapsed(end_time, start_time);
        g_speed_test_results[test_index].prescaler_value = prescaler_value;
        g_speed_test_results[test_index].write_speed_kbps = CalculateSpeed(SPEED_TEST_SIZE_BYTES, 
                                                                           g_speed_test_results[test_index].write_time_ms);
        
        LOG_INFO("MAIN", "写入完成，耗时: %lu ms, 速度: %.2f KB/s", 
                 g_speed_test_results[test_index].write_time_ms,
                 g_speed_test_results[test_index].write_speed_kbps);
        
        Delay_ms(500);
        
        /* 读取测试 */
        DisplaySpeedTestStatus(prescaler_value, test_index, PRESCALER_COUNT, "Read");
        LOG_INFO("MAIN", "开始读取测试...");
        
        start_time = Delay_GetTick();
        blocks_processed = 0;
        
        /* 进度显示变量（每个分频测试开始时重置） */
        uint8_t last_log_percent_read = 0;
        uint8_t last_oled_percent_read = 0;
        
        /* 分块读取（每次32块，约16KB，提高传输效率） */
        for (block_idx = 0; block_idx < SPEED_TEST_BLOCK_COUNT; block_idx += 32)
        {
            uint32_t blocks_to_read = (SPEED_TEST_BLOCK_COUNT - block_idx > 32) ? 32 : (SPEED_TEST_BLOCK_COUNT - block_idx);
            uint32_t current_block = test_start_block + block_idx;
            
            /* 读取前检查SD卡状态 */
            if (!TF_SPI_IsInitialized())
            {
                LOG_WARN("MAIN", "读取过程中检测到SD卡拔出，跳过此分频测试");
                g_speed_test_results[test_index].read_time_ms = 0;
                g_speed_test_results[test_index].read_speed_kbps = 0;
                break;  /* 跳出读取循环，继续下一个分频 */
            }
            
            tf_status = TF_SPI_ReadBlocks(current_block, blocks_to_read, test_buffer);
            if (tf_status != TF_SPI_OK)
            {
                LOG_ERROR("MAIN", "读取失败，块地址: %lu, 状态: %d", current_block, tf_status);
                LOG_WARN("MAIN", "跳过该分频的读取测试，继续下一个分频");
                /* 标记读取失败，但不退出整个测试 */
                g_speed_test_results[test_index].read_time_ms = 0;
                g_speed_test_results[test_index].read_speed_kbps = 0;
                break;  /* 跳出读取循环，继续下一个分频测试 */
            }
            
            blocks_processed += blocks_to_read;
            
            /* 按10%输出进度和更新OLED（基于百分比判断，而不是取模） */
            {
                uint8_t current_percent = (blocks_processed * 100) / SPEED_TEST_BLOCK_COUNT;
                
                /* 串口日志：每10%输出一次 */
                if (current_percent >= last_log_percent_read + 10 || blocks_processed >= SPEED_TEST_BLOCK_COUNT)
                {
                    LOG_INFO("MAIN", "读取进度: %lu/%lu 块 (%d%%)", 
                             blocks_processed, SPEED_TEST_BLOCK_COUNT, current_percent);
                    last_log_percent_read = (current_percent / 10) * 10;  /* 向下取整到10的倍数 */
                }
                
                /* OLED显示：每20%更新一次 */
                if (current_percent >= last_oled_percent_read + 20 || blocks_processed >= SPEED_TEST_BLOCK_COUNT)
                {
                    char buf[17];
                    snprintf(buf, sizeof(buf), "Read: %d%%", current_percent);
                    OLED_ShowString(4, 1, buf);
                    last_oled_percent_read = (current_percent / 20) * 20;  /* 向下取整到20的倍数 */
                }
            }
        }
        
        end_time = Delay_GetTick();
        g_speed_test_results[test_index].read_time_ms = Delay_GetElapsed(end_time, start_time);
        g_speed_test_results[test_index].read_speed_kbps = CalculateSpeed(SPEED_TEST_SIZE_BYTES, 
                                                                         g_speed_test_results[test_index].read_time_ms);
        
        LOG_INFO("MAIN", "读取完成，耗时: %lu ms, 速度: %.2f KB/s", 
                 g_speed_test_results[test_index].read_time_ms,
                 g_speed_test_results[test_index].read_speed_kbps);
        
        Delay_ms(500);
    }
    
    /* 输出速度对比表 */
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "=== 速度测试结果对比表 ===");
    LOG_INFO("MAIN", "分频 | 写入时间(ms) | 写入速度(KB/s) | 读取时间(ms) | 读取速度(KB/s)");
    LOG_INFO("MAIN", "-----|--------------|---------------|--------------|---------------");
    for (test_index = 0; test_index < PRESCALER_COUNT; test_index++)
    {
        LOG_INFO("MAIN", "  %2d  |   %8lu   |   %10.2f   |   %8lu   |   %10.2f",
                 g_speed_test_results[test_index].prescaler_value,
                 g_speed_test_results[test_index].write_time_ms,
                 g_speed_test_results[test_index].write_speed_kbps,
                 g_speed_test_results[test_index].read_time_ms,
                 g_speed_test_results[test_index].read_speed_kbps);
    }
    
    /* 查找最快和最慢的分频 */
    {
        uint8_t fastest_write_idx = 0, fastest_read_idx = 0;
        uint8_t slowest_write_idx = 0, slowest_read_idx = 0;
        
        for (test_index = 1; test_index < PRESCALER_COUNT; test_index++)
        {
            if (g_speed_test_results[test_index].write_speed_kbps > 
                g_speed_test_results[fastest_write_idx].write_speed_kbps)
            {
                fastest_write_idx = test_index;
            }
            if (g_speed_test_results[test_index].write_speed_kbps < 
                g_speed_test_results[slowest_write_idx].write_speed_kbps)
            {
                slowest_write_idx = test_index;
            }
            if (g_speed_test_results[test_index].read_speed_kbps > 
                g_speed_test_results[fastest_read_idx].read_speed_kbps)
            {
                fastest_read_idx = test_index;
            }
            if (g_speed_test_results[test_index].read_speed_kbps < 
                g_speed_test_results[slowest_read_idx].read_speed_kbps)
            {
                slowest_read_idx = test_index;
            }
        }
        
        LOG_INFO("MAIN", "");
        LOG_INFO("MAIN", "最快写入: 分频 %d, 速度 %.2f KB/s", 
                 g_speed_test_results[fastest_write_idx].prescaler_value,
                 g_speed_test_results[fastest_write_idx].write_speed_kbps);
        LOG_INFO("MAIN", "最慢写入: 分频 %d, 速度 %.2f KB/s", 
                 g_speed_test_results[slowest_write_idx].prescaler_value,
                 g_speed_test_results[slowest_write_idx].write_speed_kbps);
        LOG_INFO("MAIN", "最快读取: 分频 %d, 速度 %.2f KB/s", 
                 g_speed_test_results[fastest_read_idx].prescaler_value,
                 g_speed_test_results[fastest_read_idx].read_speed_kbps);
        LOG_INFO("MAIN", "最慢读取: 分频 %d, 速度 %.2f KB/s", 
                 g_speed_test_results[slowest_read_idx].prescaler_value,
                 g_speed_test_results[slowest_read_idx].read_speed_kbps);
    }
    
    /* 测速测试完成后，恢复SPI分频到8分频（增量写入使用的分频） */
    LOG_INFO("MAIN", "测速测试完成，恢复SPI到8分频（增量写入速度）");
    spi_status = ChangeSPIPrescaler(INCREMENTAL_WRITE_PRESCALER);
    if (spi_status != SPI_OK)
    {
        LOG_WARN("MAIN", "恢复SPI分频失败: %d", spi_status);
    }
    else
    {
        Delay_ms(10);  /* 等待SPI总线稳定 */
        LOG_INFO("MAIN", "SPI已恢复到8分频（9MHz）");
    }
    
    /* 检查SD卡状态，如果异常则尝试重新初始化 */
    if (!TF_SPI_IsInitialized())
    {
        LOG_WARN("MAIN", "测速测试后SD卡状态异常，尝试重新初始化...");
        /* 先恢复到初始化速度 */
        ChangeSPIPrescaler(SPI_BaudRatePrescaler_256);
        Delay_ms(10);
        
        tf_status = TF_SPI_Init();
        if (tf_status == TF_SPI_OK)
        {
            LOG_INFO("MAIN", "SD卡重新初始化成功");
            /* 恢复回8分频 */
            ChangeSPIPrescaler(INCREMENTAL_WRITE_PRESCALER);
            Delay_ms(10);
        }
        else
        {
            LOG_WARN("MAIN", "SD卡重新初始化失败: %d", tf_status);
        }
    }
    
    /* OLED显示结果 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Speed Test OK");
    OLED_ShowString(2, 1, "See UART Log");
    OLED_ShowString(3, 1, "For Details");
    
    return 1;
}

/* ==================== 演示3：增量写入功能 ==================== */

/**
 * @brief 执行增量写入（写入100KB数据，使用8分频）
 * @return uint8_t 1-成功，0-失败
 */
static uint8_t PerformIncrementalWrite(void)
{
    TF_SPI_Status_t tf_status;
    SPI_Status_t spi_status;
    uint32_t i;
    uint8_t write_buffer[512];
    uint32_t start_time, end_time;
    const tf_spi_dev_t *dev_info;
    
    /* 检查初始化状态 */
    if (!TF_SPI_IsInitialized())
    {
        LOG_WARN("MAIN", "TF卡未初始化，尝试重新初始化...");
        
        /* 先恢复到初始化速度 */
        SPI_Status_t spi_status = ChangeSPIPrescaler(SPI_BaudRatePrescaler_256);
        if (spi_status == SPI_OK)
        {
            Delay_ms(10);
            TF_SPI_Status_t tf_status = TF_SPI_Init();
            if (tf_status == TF_SPI_OK)
            {
                LOG_INFO("MAIN", "TF卡重新初始化成功");
                /* 恢复回8分频 */
                ChangeSPIPrescaler(INCREMENTAL_WRITE_PRESCALER);
                Delay_ms(10);
            }
            else
            {
                LOG_WARN("MAIN", "TF卡重新初始化失败: %d", tf_status);
                return 0;
            }
        }
        else
        {
            LOG_WARN("MAIN", "恢复SPI分频失败: %d", spi_status);
            return 0;
        }
    }
    
    dev_info = TF_SPI_GetInfo();
    if (dev_info == NULL)
    {
        LOG_WARN("MAIN", "无法获取设备信息，跳过增量写入");
        return 0;
    }
    
    /* 检查是否达到最大写入次数 */
    if (g_incremental_write_state.write_count >= INCREMENTAL_WRITE_MAX_COUNT)
    {
        LOG_INFO("MAIN", "已达到最大写入次数 %d 次，增量写入完成", INCREMENTAL_WRITE_MAX_COUNT);
        /* 禁用增量写入功能，避免主循环继续尝试 */
        g_incremental_write_state.initialized = 0;
        return 0;
    }
    
    /* 检查容量是否足够 */
    if (g_incremental_write_state.current_block + INCREMENTAL_WRITE_BLOCK_COUNT > dev_info->block_count)
    {
        LOG_WARN("MAIN", "SD卡容量不足，增量写入已满");
        return 0;
    }
    
    LOG_INFO("MAIN", "=== 增量写入：写入100KB数据（第 %lu/%d 次） ===", 
             g_incremental_write_state.write_count + 1, INCREMENTAL_WRITE_MAX_COUNT);
    LOG_INFO("MAIN", "写入块地址: %lu - %lu", 
             g_incremental_write_state.current_block,
             g_incremental_write_state.current_block + INCREMENTAL_WRITE_BLOCK_COUNT - 1);
    
    /* 切换到8分频（标准常用速度） */
    LOG_INFO("MAIN", "切换到8分频（9MHz）进行增量写入");
    spi_status = ChangeSPIPrescaler(INCREMENTAL_WRITE_PRESCALER);
    if (spi_status != SPI_OK)
    {
        LOG_ERROR("MAIN", "修改SPI分频失败: %d", spi_status);
        return 0;
    }
    Delay_ms(10);  /* 等待SPI总线稳定 */
    
    start_time = Delay_GetTick();
    
    /* 写入100KB数据（200块） */
    uint8_t last_log_percent = 0;
    uint8_t last_oled_percent = 0;
    
    for (i = 0; i < INCREMENTAL_WRITE_BLOCK_COUNT; i++)
    {
        uint32_t current_block = g_incremental_write_state.current_block + i;
        
        /* 准备数据：包含块序号、时间戳、递增序列 */
        memset(write_buffer, 0, sizeof(write_buffer));
        *((uint32_t*)&write_buffer[0]) = g_incremental_write_state.write_count;  /* 写入次数 */
        *((uint32_t*)&write_buffer[4]) = Delay_GetTick();  /* 时间戳 */
        *((uint32_t*)&write_buffer[8]) = current_block;  /* 块地址 */
        *((uint32_t*)&write_buffer[12]) = i;  /* 块内序号 */
        
        /* 填充递增序列 */
        {
            uint32_t j;
            for (j = 16; j < 512; j++)
            {
                write_buffer[j] = (uint8_t)((current_block + j) & 0xFF);
            }
        }
        
        /* 写入块 */
        tf_status = TF_SPI_WriteBlock(current_block, write_buffer);
        if (tf_status != TF_SPI_OK)
        {
            LOG_ERROR("MAIN", "写入失败，块地址: %lu, 状态: %d", current_block, tf_status);
            
            /* 写入失败可能是SD卡状态异常，尝试重新初始化 */
            if (tf_status == TF_SPI_ERROR_CMD_FAILED || tf_status == TF_SPI_ERROR_TIMEOUT)
            {
                LOG_WARN("MAIN", "检测到SD卡通信异常（状态: %d），尝试清除状态并重新初始化...", tf_status);
                
                /* 先清除初始化状态，避免状态不一致 */
                TF_SPI_Deinit();
                
                /* 先恢复到初始化速度 */
                ChangeSPIPrescaler(SPI_BaudRatePrescaler_256);
                Delay_ms(10);
                
                TF_SPI_Status_t reinit_status = TF_SPI_Init();
                if (reinit_status == TF_SPI_OK)
                {
                    LOG_INFO("MAIN", "SD卡重新初始化成功，但本次写入已失败");
                    /* 恢复回8分频 */
                    ChangeSPIPrescaler(INCREMENTAL_WRITE_PRESCALER);
                    Delay_ms(10);
                }
                else
                {
                    LOG_WARN("MAIN", "SD卡重新初始化失败: %d，可能卡已拔出", reinit_status);
                }
            }
            
            return 0;
        }
        
        /* 进度显示：每10%输出一次串口日志，每20%更新一次OLED */
        {
            uint8_t current_percent = ((i + 1) * 100) / INCREMENTAL_WRITE_BLOCK_COUNT;
            
            /* 串口日志：每10%输出一次 */
            if (current_percent >= last_log_percent + 10 || i + 1 >= INCREMENTAL_WRITE_BLOCK_COUNT)
            {
                LOG_INFO("MAIN", "写入进度: %lu/%lu 块 (%d%%)",
                         i + 1, INCREMENTAL_WRITE_BLOCK_COUNT, current_percent);
                last_log_percent = (current_percent / 10) * 10;  /* 向下取整到10的倍数 */
            }
            
            /* OLED显示：每20%更新一次 */
            if (current_percent >= last_oled_percent + 20 || i + 1 >= INCREMENTAL_WRITE_BLOCK_COUNT)
            {
                char buf[17];
                snprintf(buf, sizeof(buf), "Write: %d%%", current_percent);
                OLED_ShowString(4, 1, buf);
                last_oled_percent = (current_percent / 20) * 20;  /* 向下取整到20的倍数 */
            }
        }
    }
    
    end_time = Delay_GetTick();
    uint32_t write_time_ms = Delay_GetElapsed(end_time, start_time);
    
    /* 更新状态 */
    g_incremental_write_state.current_block += INCREMENTAL_WRITE_BLOCK_COUNT;
    g_incremental_write_state.write_count++;
    g_incremental_write_state.last_write_time_ms = Delay_GetTick();
    
    LOG_INFO("MAIN", "写入完成，耗时: %lu ms, 写入次数: %lu", 
             write_time_ms, g_incremental_write_state.write_count);
    {
        uint32_t total_size_kb = (g_incremental_write_state.current_block - INCREMENTAL_WRITE_START_BLOCK) * 512 / 1024;
        LOG_INFO("MAIN", "当前数据容量: %lu KB (%.2f MB)", 
                 total_size_kb, total_size_kb / 1024.0f);
    }
    
    /* OLED显示 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Incr Write OK");
    char buf[17];
    snprintf(buf, sizeof(buf), "Count: %lu", g_incremental_write_state.write_count);
    OLED_ShowString(2, 1, buf);
    {
        uint32_t total_size_kb = (g_incremental_write_state.current_block - INCREMENTAL_WRITE_START_BLOCK) * 512 / 1024;
        snprintf(buf, sizeof(buf), "Size: %lu KB", total_size_kb);
        OLED_ShowString(3, 1, buf);
    }
    
    return 1;
}

/**
 * @brief 读取并校验所有已写入的数据
 * @return uint8_t 1-校验通过，0-校验失败
 */
static uint8_t VerifyIncrementalData(void)
{
    TF_SPI_Status_t tf_status;
    SPI_Status_t spi_status;
    uint8_t read_buffer[512];
    uint32_t total_blocks;
    uint32_t i;
    uint32_t error_count = 0;
    uint32_t start_time, end_time;
    
    if (!TF_SPI_IsInitialized())
    {
        LOG_WARN("MAIN", "TF卡未初始化，跳过数据校验");
        return 0;
    }
    
    if (g_incremental_write_state.write_count == 0)
    {
        LOG_INFO("MAIN", "尚未写入数据，跳过校验");
        return 1;
    }
    
    total_blocks = g_incremental_write_state.current_block - INCREMENTAL_WRITE_START_BLOCK;
    
    LOG_INFO("MAIN", "=== 读取并校验所有已写入数据 ===");
    LOG_INFO("MAIN", "总块数: %lu", total_blocks);
    
    /* 切换到8分频（标准常用速度） */
    LOG_INFO("MAIN", "使用8分频（9MHz）进行数据校验");
    spi_status = ChangeSPIPrescaler(INCREMENTAL_WRITE_PRESCALER);
    if (spi_status != SPI_OK)
    {
        LOG_ERROR("MAIN", "修改SPI分频失败: %d", spi_status);
        return 0;
    }
    Delay_ms(10);  /* 等待SPI总线稳定 */
    
    start_time = Delay_GetTick();
    
    /* 进度显示变量 */
    uint8_t last_log_percent = 0;
    uint8_t last_oled_percent = 0;
    
    /* 读取所有块并校验 */
    for (i = 0; i < total_blocks; i++)
    {
        uint32_t current_block = INCREMENTAL_WRITE_START_BLOCK + i;
        uint32_t expected_block_addr;
        uint32_t expected_block_idx;
        
        /* 读取块 */
        tf_status = TF_SPI_ReadBlock(current_block, read_buffer);
        if (tf_status != TF_SPI_OK)
        {
            LOG_ERROR("MAIN", "读取失败，块地址: %lu, 状态: %d", current_block, tf_status);
            
            /* 读取失败可能是SD卡状态异常，尝试重新初始化 */
            if (tf_status == TF_SPI_ERROR_CMD_FAILED || tf_status == TF_SPI_ERROR_TIMEOUT)
            {
                LOG_WARN("MAIN", "检测到SD卡通信异常，尝试重新初始化...");
                
                /* 先恢复到初始化速度 */
                ChangeSPIPrescaler(SPI_BaudRatePrescaler_256);
                Delay_ms(10);
                
                TF_SPI_Status_t reinit_status = TF_SPI_Init();
                if (reinit_status == TF_SPI_OK)
                {
                    LOG_INFO("MAIN", "SD卡重新初始化成功，但本次校验已失败");
                    /* 恢复回8分频 */
                    ChangeSPIPrescaler(INCREMENTAL_WRITE_PRESCALER);
                    Delay_ms(10);
                }
                else
                {
                    LOG_WARN("MAIN", "SD卡重新初始化失败: %d", reinit_status);
                }
            }
            
            error_count++;
            continue;
        }
        
        /* 解析数据头 */
        expected_block_addr = *((uint32_t*)&read_buffer[8]);
        expected_block_idx = *((uint32_t*)&read_buffer[12]);
        
        /* 校验块地址 */
        if (expected_block_addr != current_block)
        {
            if (error_count < 5)  /* 只记录前5个错误 */
            {
                LOG_ERROR("MAIN", "块地址不匹配，块 %lu: 期望=%lu, 读取=%lu", 
                         i, current_block, expected_block_addr);
            }
            error_count++;
        }
        
        /* 校验块内序号 */
        if (expected_block_idx != (i % INCREMENTAL_WRITE_BLOCK_COUNT))
        {
            if (error_count < 5)
            {
                LOG_ERROR("MAIN", "块内序号不匹配，块 %lu: 期望=%lu, 读取=%lu", 
                         i, i % INCREMENTAL_WRITE_BLOCK_COUNT, expected_block_idx);
            }
            error_count++;
        }
        
        /* 校验数据内容（检查递增序列） */
        {
            uint32_t j;
            for (j = 16; j < 512; j++)
            {
                uint8_t expected = (uint8_t)((current_block + j) & 0xFF);
                if (read_buffer[j] != expected)
                {
                    if (error_count < 5)
                    {
                        LOG_ERROR("MAIN", "数据不匹配，块 %lu, 偏移 %lu: 期望=0x%02X, 读取=0x%02X", 
                                 i, j, expected, read_buffer[j]);
                    }
                    error_count++;
                    break;  /* 每个块只记录第一个错误 */
                }
            }
        }
        
        /* 进度显示：每10%输出一次串口日志，每20%更新一次OLED */
        {
            uint8_t current_percent = ((i + 1) * 100) / total_blocks;
            
            /* 串口日志：每10%输出一次 */
            if (current_percent >= last_log_percent + 10 || i + 1 >= total_blocks)
            {
                LOG_INFO("MAIN", "校验进度: %lu/%lu 块 (%d%%)",
                         i + 1, total_blocks, current_percent);
                last_log_percent = (current_percent / 10) * 10;  /* 向下取整到10的倍数 */
            }
            
            /* OLED显示：每20%更新一次 */
            if (current_percent >= last_oled_percent + 20 || i + 1 >= total_blocks)
            {
                char buf[17];
                snprintf(buf, sizeof(buf), "Verify: %d%%", current_percent);
                OLED_ShowString(4, 1, buf);
                last_oled_percent = (current_percent / 20) * 20;  /* 向下取整到20的倍数 */
            }
        }
    }
    
    end_time = Delay_GetTick();
    uint32_t verify_time_ms = Delay_GetElapsed(end_time, start_time);
    
    if (error_count == 0)
    {
        LOG_INFO("MAIN", "数据校验通过，总块数: %lu, 耗时: %lu ms", total_blocks, verify_time_ms);
        
        /* OLED显示 */
        OLED_Clear();
        OLED_ShowString(1, 1, "Verify OK");
        char buf[17];
        snprintf(buf, sizeof(buf), "Blocks: %lu", total_blocks);
        OLED_ShowString(2, 1, buf);
        OLED_ShowString(3, 1, "No Errors");
        
        return 1;
    }
    else
    {
        LOG_ERROR("MAIN", "数据校验失败，错误块数: %lu/%lu", error_count, total_blocks);
        
        /* OLED显示 */
        OLED_Clear();
        OLED_ShowString(1, 1, "Verify Failed");
        char buf[17];
        snprintf(buf, sizeof(buf), "Errors: %lu", error_count);
        OLED_ShowString(2, 1, buf);
        
        return 0;
    }
}

/* ==================== 插拔卡处理 ==================== */

/**
 * @brief 检测并处理插拔卡
 * @return uint8_t 1-卡存在且已初始化，0-卡不存在或未初始化
 */
static uint8_t DetectAndHandleCard(void)
{
    uint32_t current_time = Delay_GetTick();
    uint32_t elapsed = Delay_GetElapsed(current_time, g_card_detect_state.last_detect_time_ms);
    uint8_t current_init_status;
    TF_SPI_Status_t tf_status;
    
    /* 检查是否到了检测时间 */
    if (elapsed < CARD_DETECT_INTERVAL_MS)
    {
        return g_card_detect_state.card_present;  /* 返回上次状态 */
    }
    
    g_card_detect_state.last_detect_time_ms = current_time;
    
    /* 检查初始化状态 */
    current_init_status = TF_SPI_IsInitialized();
    
    /* 如果状态发生变化 */
    if (current_init_status != g_card_detect_state.last_init_status)
    {
        if (current_init_status)
        {
            /* 从未初始化变为已初始化：卡插入 */
            LOG_INFO("MAIN", "检测到SD卡插入");
            g_card_detect_state.card_present = 1;
            
            /* OLED显示 */
            OLED_Clear();
            OLED_ShowString(1, 1, "Card Inserted");
            OLED_ShowString(2, 1, "Initialized");
            Delay_ms(1000);
        }
        else
        {
            /* 从已初始化变为未初始化：卡拔出 */
            LOG_WARN("MAIN", "检测到SD卡拔出");
            g_card_detect_state.card_present = 0;
            
            /* OLED显示 */
            OLED_Clear();
            OLED_ShowString(1, 1, "Card Removed");
            Delay_ms(1000);
        }
    }
    else if (!current_init_status)
    {
        /* 如果未初始化，尝试重新初始化 */
        LOG_INFO("MAIN", "尝试重新初始化SD卡...");
        
        /* 重要：重新初始化前，先将SPI恢复到初始化速度（256分频，≤400kHz） */
        /* 因为增量写入可能已经切换到8分频（9MHz），而初始化需要低速 */
        SPI_Status_t spi_status = ChangeSPIPrescaler(SPI_BaudRatePrescaler_256);
        if (spi_status != SPI_OK)
        {
            LOG_WARN("MAIN", "恢复SPI分频失败: %d", spi_status);
        }
        else
        {
            LOG_INFO("MAIN", "已恢复SPI到256分频（初始化速度）");
            Delay_ms(10);  /* 等待SPI总线稳定 */
        }
        
        tf_status = TF_SPI_Init();
        if (tf_status == TF_SPI_OK)
        {
            LOG_INFO("MAIN", "SD卡重新初始化成功");
            g_card_detect_state.card_present = 1;
            g_card_detect_state.last_init_status = 1;
            
            /* OLED显示 */
            OLED_Clear();
            OLED_ShowString(1, 1, "Card Re-Init");
            OLED_ShowString(2, 1, "Success");
            Delay_ms(1000);
        }
        else
        {
            LOG_WARN("MAIN", "SD卡重新初始化失败: %d", tf_status);
            LOG_WARN("MAIN", "可能原因：1.卡未插入 2.MISO上拉电阻 3.SPI速度过快");
            g_card_detect_state.card_present = 0;
        }
    }
    else
    {
        /* 已初始化，卡存在 */
        g_card_detect_state.card_present = 1;
    }
    
    g_card_detect_state.last_init_status = current_init_status;
    
    return g_card_detect_state.card_present;
}

/* ==================== 主函数 ==================== */

int main(void)
{
    TF_SPI_Status_t tf_status;
    SPI_Status_t spi_status;
    SoftI2C_Status_t i2c_status;
    UART_Status_t uart_status;
    OLED_Status_t oled_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    uint32_t last_incremental_write_time = 0;
    
    /* ========== 步骤1：系统初始化 ========== */
    System_Init();
    
    /* ========== 步骤2：UART初始化 ========== */
    uart_status = UART_Init(UART_INSTANCE_1);
    if (uart_status != UART_OK)
    {
        while(1) { Delay_ms(1000); }
    }
    Delay_ms(100);
    
    /* ========== 步骤3：Debug模块初始化 ========== */
    debug_status = Debug_Init(DEBUG_MODE_UART, 115200);
    if (debug_status != 0)
    {
        while(1) { Delay_ms(1000); }
    }
    Delay_ms(100);
    
    /* ========== 步骤4：Log模块初始化 ========== */
    memset(&log_config, 0, sizeof(log_config));
    log_config.level = LOG_LEVEL_INFO;
    log_config.enable_timestamp = 0;
    log_config.enable_module = 1;
    log_config.enable_color = 0;
    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK)
    {
        ErrorHandler_Handle(log_status, "LOG");
    }
    
    /* ========== 步骤5：输出初始化信息 ========== */
    LOG_INFO("MAIN", "=== TF卡（MicroSD卡）读写测速示例 ===");
    LOG_INFO("MAIN", "UART1 已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "Debug 模块已初始化: UART 模式");
    LOG_INFO("MAIN", "Log 模块已初始化");
    
    /* ========== 步骤6：LED初始化 ========== */
    if (LED_Init() != LED_OK)
    {
        LOG_ERROR("MAIN", "LED 初始化失败");
        while(1) { Delay_ms(1000); }
    }
    
    /* ========== 步骤7：软件I2C初始化（OLED需要） ========== */
    i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_1);
    if (i2c_status != SOFT_I2C_OK)
    {
        LOG_ERROR("MAIN", "软件I2C 初始化失败: %d", i2c_status);
        ErrorHandler_Handle(i2c_status, "SOFT_I2C");
    }
    else
    {
        LOG_INFO("MAIN", "软件I2C 已初始化: PB8(SCL), PB9(SDA)");
    }
    
    /* ========== 步骤8：OLED初始化 ========== */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        LOG_ERROR("MAIN", "OLED 初始化失败: %d", oled_status);
        ErrorHandler_Handle(oled_status, "OLED");
    }
    else
    {
        OLED_Clear();
        OLED_ShowString(1, 1, "TF Speed Test");
        OLED_ShowString(2, 1, "Initializing...");
        LOG_INFO("MAIN", "OLED 已初始化");
    }
    
    Delay_ms(500);
    
    /* ========== 步骤9：SPI初始化 ========== */
    OLED_ShowString(3, 1, "Init SPI2...");
    
    /* 手动配置PA11为GPIO输出（软件NSS模式） */
    GPIO_EnableClock(GPIOA);
    GPIO_Config(GPIOA, GPIO_Pin_11, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHz);
    GPIO_WritePin(GPIOA, GPIO_Pin_11, Bit_SET);  /* NSS默认拉高（不选中） */
    
    spi_status = SPI_HW_Init(SPI_INSTANCE_2);
    if (spi_status != SPI_OK)
    {
        char err_buf[17];
        snprintf(err_buf, sizeof(err_buf), "SPI Fail:%d", spi_status);
        OLED_ShowString(4, 1, err_buf);
        LOG_ERROR("MAIN", "SPI2 初始化失败: %d", spi_status);
        ErrorHandler_Handle(spi_status, "SPI");
        while(1) { Delay_ms(1000); }
    }
    else
    {
        OLED_ShowString(4, 1, "SPI2: OK");
        LOG_INFO("MAIN", "SPI2 已初始化: PB13(SCK), PB14(MISO), PB15(MOSI), PA11(CS)");
    }
    
    Delay_ms(500);
    
    /* ========== 步骤10：TF卡自动初始化 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "TF Card Init");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== TF卡自动初始化 ===");
    
    tf_status = TF_SPI_Init();
    if (tf_status == TF_SPI_OK)
    {
        OLED_ShowString(2, 1, "Init: OK");
        LOG_INFO("MAIN", "TF_SPI_Init()成功！");
        
        const tf_spi_dev_t *dev_info = TF_SPI_GetInfo();
        if (dev_info != NULL)
        {
            char buf[17];
            snprintf(buf, sizeof(buf), "Cap: %d MB", dev_info->capacity_mb);
            OLED_ShowString(3, 1, buf);
            
            LOG_INFO("MAIN", "SD卡信息：");
            LOG_INFO("MAIN", "  容量: %d MB", dev_info->capacity_mb);
            LOG_INFO("MAIN", "  块大小: %d 字节", dev_info->block_size);
            LOG_INFO("MAIN", "  块数量: %d", dev_info->block_count);
        }
        
        g_card_detect_state.card_present = 1;
        g_card_detect_state.last_init_status = 1;
    }
    else
    {
        OLED_ShowString(2, 1, "Init: Failed");
        char err_buf[17];
        snprintf(err_buf, sizeof(err_buf), "Error: %d", tf_status);
        OLED_ShowString(3, 1, err_buf);
        LOG_ERROR("MAIN", "TF_SPI_Init()失败，状态: %d", tf_status);
        LOG_ERROR("MAIN", "请检查SD卡是否插入");
        
        g_card_detect_state.card_present = 0;
        g_card_detect_state.last_init_status = 0;
    }
    
    Delay_ms(2000);
    
    /* ========== 步骤11：执行演示1（高级API演示） ========== */
    DemoHighLevelAPI();
    Delay_ms(2000);
    
    /* ========== 步骤12：执行演示2（测速测试） ========== */
    if (TF_SPI_IsInitialized())
    {
        PerformSpeedTest();
        Delay_ms(500);  /* 减少延迟，快速进入主循环 */
    }
    else
    {
        LOG_WARN("MAIN", "TF卡未初始化，跳过测速测试");
    }
    
    /* ========== 步骤13：初始化增量写入状态 ========== */
    g_incremental_write_state.initialized = 1;
    /* 设置为当前时间减去间隔时间，这样进入主循环后会立即执行一次增量写入 */
    last_incremental_write_time = Delay_GetTick() - INCREMENTAL_WRITE_INTERVAL_MS;
    
    LOG_INFO("MAIN", "=== 进入主循环 ===");
    LOG_INFO("MAIN", "增量写入模式：每5秒写入100KB，使用8分频（9MHz），自动校验");
    LOG_INFO("MAIN", "最大写入次数：%d 次（便于测试插拔卡功能）", INCREMENTAL_WRITE_MAX_COUNT);
    LOG_INFO("MAIN", "插拔卡检测：每5秒检测一次");
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Running...");
    OLED_ShowString(2, 1, "Incr Write");
    OLED_ShowString(3, 1, "Mode Active");
    
    /* ========== 步骤14：主循环 ========== */
    while (1)
    {
        uint32_t current_time = Delay_GetTick();
        uint32_t elapsed;
        
        /* 插拔卡检测 */
        DetectAndHandleCard();
        
        /* 增量写入处理 */
        if (TF_SPI_IsInitialized() && g_incremental_write_state.initialized)
        {
            /* 检查是否已达到最大写入次数 */
            if (g_incremental_write_state.write_count >= INCREMENTAL_WRITE_MAX_COUNT)
            {
                /* 已达到最大次数，不再执行增量写入 */
                /* 这个检查已经在PerformIncrementalWrite中做了，但这里提前检查可以避免不必要的函数调用 */
            }
            else
            {
                elapsed = Delay_GetElapsed(current_time, last_incremental_write_time);
                
                if (elapsed >= INCREMENTAL_WRITE_INTERVAL_MS)
                {
                    /* 执行增量写入 */
                    if (PerformIncrementalWrite())
                    {
                        /* 读取并校验所有数据 */
                        VerifyIncrementalData();
                        
                        /* 检查是否已达到最大写入次数 */
                        if (g_incremental_write_state.write_count >= INCREMENTAL_WRITE_MAX_COUNT)
                        {
                            LOG_INFO("MAIN", "增量写入任务完成，已写入 %d 次，停止增量写入", INCREMENTAL_WRITE_MAX_COUNT);
                        }
                        else
                        {
                            /* 写入和校验完成后，等待5秒后继续下一次写入 */
                            LOG_INFO("MAIN", "本次写入完成，等待5秒后继续下一次写入");
                            Delay_ms(INCREMENTAL_WRITE_INTERVAL_MS);  /* 等待5秒 */
                        }
                    }
                    else
                    {
                        /* 增量写入失败，检查是否因为达到最大次数 */
                        if (g_incremental_write_state.write_count >= INCREMENTAL_WRITE_MAX_COUNT)
                        {
                            /* 已达到最大次数，不再重试 */
                            LOG_INFO("MAIN", "增量写入任务完成，已写入 %d 次", INCREMENTAL_WRITE_MAX_COUNT);
                        }
                        else
                        {
                            /* 增量写入失败，记录日志但不阻塞主循环 */
                            LOG_WARN("MAIN", "增量写入失败，将在下次循环时重试（如果SD卡已恢复）");
                            /* 失败时也等待一段时间，避免频繁重试 */
                            Delay_ms(1000);
                        }
                    }
                    
                    /* 无论成功或失败，都更新时间戳，避免立即重试 */
                    last_incremental_write_time = Delay_GetTick();
                }
            }
        }
        else if (!TF_SPI_IsInitialized() && g_incremental_write_state.initialized)
        {
            /* SD卡未初始化，但增量写入已启用，等待插拔卡检测处理 */
            /* 插拔卡检测会尝试重新初始化 */
        }
        
        /* LED闪烁指示系统运行 */
        LED_Toggle(LED_1);
        Delay_ms(500);
    }
}

