/**
 * @file main_example.c
 * @brief Flash02 - W25Q SPI Flash不同分频读写速度测试
 * @details 测试W25Q Flash在不同SPI分频下的读写速度，测试数据量1MB
 * 
 * 硬件连接：
 * - W25Q SPI Flash模块连接到SPI2
 *   - CS：PA11
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
 * 1. W25Q初始化与设备识别
 * 2. 擦除测试扇区（至少1MB）
 * 3. 循环测试所有SPI分频（2, 4, 8, 16, 32, 64, 128, 256）
 * 4. 每个分频测试1MB数据写入和读取速度
 * 5. OLED显示关键信息（当前分频、最快/最慢分频）
 * 6. UART输出详细对比表
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
#include "w25q_spi.h"
#include "gpio.h"
#include "config.h"
#include "board.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 测试数据大小：1MB */
#define TEST_DATA_SIZE        (1024 * 1024)  /* 1MB = 1024 * 1024 字节 */

/* 测试起始地址（需要确保有足够空间） */
#define TEST_START_ADDR       0x0000

/* SPI分频测试列表 */
#define PRESCALER_COUNT       8
static const uint16_t g_prescalers[PRESCALER_COUNT] = {
    SPI_BaudRatePrescaler_2,      /* 分频2 */
    SPI_BaudRatePrescaler_4,      /* 分频4 */
    SPI_BaudRatePrescaler_8,      /* 分频8 */
    SPI_BaudRatePrescaler_16,     /* 分频16 */
    SPI_BaudRatePrescaler_32,     /* 分频32 */
    SPI_BaudRatePrescaler_64,     /* 分频64 */
    SPI_BaudRatePrescaler_128,    /* 分频128 */
    SPI_BaudRatePrescaler_256,    /* 分频256 */
};

/* 分频值对应的数值（用于显示） */
static const uint16_t g_prescaler_values[PRESCALER_COUNT] = {2, 4, 8, 16, 32, 64, 128, 256};

/* 速度测试结果结构体 */
typedef struct {
    uint16_t prescaler_value;     /**< 分频值（2, 4, 8...） */
    uint32_t write_time_ms;        /**< 写入耗时（毫秒） */
    uint32_t read_time_ms;         /**< 读取耗时（毫秒） */
    float write_speed_kbps;        /**< 写入速度（KB/s） */
    float read_speed_kbps;         /**< 读取速度（KB/s） */
} SpeedTestResult_t;

/* 测试结果数组 */
static SpeedTestResult_t g_test_results[PRESCALER_COUNT];

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
 * @brief 在OLED上显示当前测试状态
 * @param[in] prescaler_value 当前分频值
 * @param[in] test_index 当前测试索引（0-7）
 * @param[in] total_tests 总测试数
 * @param[in] operation 操作类型（"Write"或"Read"）
 */
static void DisplayTestStatus(uint16_t prescaler_value, uint8_t test_index, uint8_t total_tests, const char *operation)
{
    char buffer[17];
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Speed Test");
    snprintf(buffer, sizeof(buffer), "P:%d %d/%d", prescaler_value, test_index + 1, total_tests);
    OLED_ShowString(2, 1, buffer);
    snprintf(buffer, sizeof(buffer), "%s 1MB...", operation);
    OLED_ShowString(3, 1, buffer);
    OLED_ShowString(4, 1, "Please wait...");
}

/**
 * @brief 准备测试数据（1MB递增序列）
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

/**
 * @brief 在OLED上显示测试结果摘要
 * @param[in] results 测试结果数组
 * @param[in] count 结果数量
 */
static void DisplayResultSummary(const SpeedTestResult_t *results, uint8_t count)
{
    uint8_t i;
    uint8_t fastest_write_idx = 0;
    uint8_t fastest_read_idx = 0;
    uint8_t slowest_write_idx = 0;
    uint8_t slowest_read_idx = 0;
    char buffer[17];
    
    if (results == NULL || count == 0)
    {
        return;
    }
    
    /* 查找最快和最慢的分频 */
    for (i = 1; i < count; i++)
    {
        if (results[i].write_speed_kbps > results[fastest_write_idx].write_speed_kbps)
        {
            fastest_write_idx = i;
        }
        if (results[i].write_speed_kbps < results[slowest_write_idx].write_speed_kbps)
        {
            slowest_write_idx = i;
        }
        if (results[i].read_speed_kbps > results[fastest_read_idx].read_speed_kbps)
        {
            fastest_read_idx = i;
        }
        if (results[i].read_speed_kbps < results[slowest_read_idx].read_speed_kbps)
        {
            slowest_read_idx = i;
        }
    }
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Test Complete");
    
    /* 显示最快写入分频 */
    snprintf(buffer, sizeof(buffer), "W Fast: P%d", results[fastest_write_idx].prescaler_value);
    OLED_ShowString(2, 1, buffer);
    snprintf(buffer, sizeof(buffer), "%.1fKB/s", results[fastest_write_idx].write_speed_kbps);
    OLED_ShowString(2, 10, buffer);
    
    /* 显示最快读取分频 */
    snprintf(buffer, sizeof(buffer), "R Fast: P%d", results[fastest_read_idx].prescaler_value);
    OLED_ShowString(3, 1, buffer);
    snprintf(buffer, sizeof(buffer), "%.1fKB/s", results[fastest_read_idx].read_speed_kbps);
    OLED_ShowString(3, 10, buffer);
    
    /* 显示最慢写入分频 */
    snprintf(buffer, sizeof(buffer), "W Slow: P%d", results[slowest_write_idx].prescaler_value);
    OLED_ShowString(4, 1, buffer);
}

/**
 * @brief 在UART上输出详细对比表
 * @param[in] results 测试结果数组
 * @param[in] count 结果数量
 */
static void PrintResultTable(const SpeedTestResult_t *results, uint8_t count)
{
    uint8_t i;
    
    if (results == NULL || count == 0)
    {
        return;
    }
    
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "=== W25Q SPI分频速度测试结果 ===");
    LOG_INFO("MAIN", "测试数据大小: %d KB (1 MB)", TEST_DATA_SIZE / 1024);
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "分频    写入速度(KB/s)  读取速度(KB/s)  写入耗时(ms)  读取耗时(ms)");
    LOG_INFO("MAIN", "----    --------------  --------------  ------------  ------------");
    
    for (i = 0; i < count; i++)
    {
        LOG_INFO("MAIN", "%-4d    %12.2f    %12.2f    %10lu    %10lu",
                 results[i].prescaler_value,
                 results[i].write_speed_kbps,
                 results[i].read_speed_kbps,
                 (unsigned long)results[i].write_time_ms,
                 (unsigned long)results[i].read_time_ms);
    }
    
    LOG_INFO("MAIN", "");
    
    /* 查找并显示最快/最慢分频 */
    {
        uint8_t fastest_write_idx = 0;
        uint8_t fastest_read_idx = 0;
        uint8_t slowest_write_idx = 0;
        uint8_t slowest_read_idx = 0;
        
        for (i = 1; i < count; i++)
        {
            if (results[i].write_speed_kbps > results[fastest_write_idx].write_speed_kbps)
            {
                fastest_write_idx = i;
            }
            if (results[i].write_speed_kbps < results[slowest_write_idx].write_speed_kbps)
            {
                slowest_write_idx = i;
            }
            if (results[i].read_speed_kbps > results[fastest_read_idx].read_speed_kbps)
            {
                fastest_read_idx = i;
            }
            if (results[i].read_speed_kbps < results[slowest_read_idx].read_speed_kbps)
            {
                slowest_read_idx = i;
            }
        }
        
        LOG_INFO("MAIN", "=== 性能总结 ===");
        LOG_INFO("MAIN", "最快写入: 分频%d, %.2f KB/s, 耗时 %lu ms",
                 results[fastest_write_idx].prescaler_value,
                 results[fastest_write_idx].write_speed_kbps,
                 (unsigned long)results[fastest_write_idx].write_time_ms);
        LOG_INFO("MAIN", "最慢写入: 分频%d, %.2f KB/s, 耗时 %lu ms",
                 results[slowest_write_idx].prescaler_value,
                 results[slowest_write_idx].write_speed_kbps,
                 (unsigned long)results[slowest_write_idx].write_time_ms);
        LOG_INFO("MAIN", "最快读取: 分频%d, %.2f KB/s, 耗时 %lu ms",
                 results[fastest_read_idx].prescaler_value,
                 results[fastest_read_idx].read_speed_kbps,
                 (unsigned long)results[fastest_read_idx].read_time_ms);
        LOG_INFO("MAIN", "最慢读取: 分频%d, %.2f KB/s, 耗时 %lu ms",
                 results[slowest_read_idx].prescaler_value,
                 results[slowest_read_idx].read_speed_kbps,
                 (unsigned long)results[slowest_read_idx].read_time_ms);
    }
}

/**
 * @brief 主函数
 */
int main(void)
{
    SPI_Status_t spi_status;
    W25Q_Status_t w25q_status;
    SoftI2C_Status_t i2c_status;
    UART_Status_t uart_status;
    OLED_Status_t oled_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    uint16_t i;  /* 注意：必须使用uint16_t，因为sectors_to_erase可能为256，uint8_t会溢出！ */
    uint32_t sectors_to_erase;
    uint32_t erase_addr;
    
    /* 测试缓冲区（使用静态数组，避免动态内存分配） */
    #define BUFFER_SIZE (4 * 1024)  /* 4KB缓冲区 */
    static uint8_t s_test_data_buffer[BUFFER_SIZE];
    static uint8_t s_read_buffer[BUFFER_SIZE];
    uint8_t *test_data_buffer = s_test_data_buffer;
    uint8_t *read_buffer = s_read_buffer;
    
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
    LOG_INFO("MAIN", "=== W25Q SPI分频速度测试 ===");
    LOG_INFO("MAIN", "测试数据大小: %d KB (1 MB)", TEST_DATA_SIZE / 1024);
    LOG_INFO("MAIN", "测试分频: 2, 4, 8, 16, 32, 64, 128, 256");
    LOG_INFO("MAIN", "UART1 已初始化: PA9(TX), PA10(RX), 115200");
    
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
        OLED_ShowString(1, 1, "Speed Test");
        OLED_ShowString(2, 1, "Initializing...");
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
        OLED_ShowString(4, 1, "SPI Init Fail!");
        LOG_ERROR("MAIN", "SPI 初始化失败: %d", spi_status);
        ErrorHandler_Handle(spi_status, "SPI");
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "SPI2 已初始化: PB13(SCK), PB14(MISO), PB15(MOSI), PA11(CS)");
    
    /* ========== 步骤10：W25Q初始化 ========== */
    OLED_ShowString(3, 1, "Init W25Q...");
    w25q_status = W25Q_Init();
    if (w25q_status != W25Q_OK)
    {
        OLED_ShowString(4, 1, "W25Q Init Fail!");
        LOG_ERROR("MAIN", "W25Q 初始化失败: %d", w25q_status);
        ErrorHandler_Handle(w25q_status, "W25Q");
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "W25Q 初始化成功");
    
    /* 显示设备信息 */
    {
        const w25q_dev_t *info = W25Q_GetInfo();
        if (info != NULL)
        {
            LOG_INFO("MAIN", "设备容量: %d MB", info->capacity_mb);
            LOG_INFO("MAIN", "地址字节数: %d", info->addr_bytes);
        }
    }
    
    Delay_ms(1000);
    
    /* ========== 步骤11：擦除测试扇区 ========== */
    /* 注意：使用4KB缓冲区循环读写1MB数据，避免RAM不足 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Erasing...");
    OLED_ShowString(2, 1, "Please wait");
    
    LOG_INFO("MAIN", "=== 擦除测试扇区 ===");
    LOG_INFO("MAIN", "需要擦除的扇区数: %d (每个扇区4KB)", (TEST_DATA_SIZE + 4095) / 4096);
    
    sectors_to_erase = (TEST_DATA_SIZE + 4095) / 4096;  /* 向上取整 */
    for (i = 0; i < sectors_to_erase; i++)
    {
        erase_addr = TEST_START_ADDR + (i * 4096);
        
        /* 更新OLED显示进度 */
        {
            char progress_buf[17];
            snprintf(progress_buf, sizeof(progress_buf), "Erase %d/%d", i + 1, sectors_to_erase);
            OLED_ShowString(3, 1, progress_buf);
        }
        
        LOG_INFO("MAIN", "擦除扇区 %d/%d, 地址: 0x%04X", i + 1, sectors_to_erase, erase_addr);
        
        /* W25Q_EraseSector()内部已经等待擦除完成，不需要再次调用W25Q_WaitReady() */
        w25q_status = W25Q_EraseSector(erase_addr);
        if (w25q_status != W25Q_OK)
        {
            LOG_ERROR("MAIN", "扇区擦除失败: 地址 0x%04X, 错误: %d", erase_addr, w25q_status);
            OLED_ShowString(3, 1, "Erase Failed!");
            OLED_ShowString(4, 1, "Check UART log");
            while(1) { Delay_ms(1000); }
        }
        
        /* 每擦除16个扇区输出一次进度（减少UART输出） */
        if ((i + 1) % 16 == 0 || (i + 1) == sectors_to_erase)
        {
            LOG_INFO("MAIN", "擦除进度: %d/%d (%.1f%%)", i + 1, sectors_to_erase, 
                     (float)(i + 1) * 100.0f / sectors_to_erase);
        }
    }
    
    LOG_INFO("MAIN", "扇区擦除完成");
    LOG_INFO("MAIN", "擦除完成，准备开始速度测试");
    OLED_Clear();
    OLED_ShowString(1, 1, "Erase Complete!");
    OLED_ShowString(2, 1, "Start Speed Test");
    Delay_ms(2000);
    
    /* ========== 步骤13：循环测试所有分频 ========== */
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "=== 开始速度测试 ===");
    LOG_INFO("MAIN", "测试分频列表: 2, 4, 8, 16, 32, 64, 128, 256");
    LOG_INFO("MAIN", "");
    
    for (i = 0; i < PRESCALER_COUNT; i++)
    {
        uint16_t prescaler_value = GetPrescalerValue(g_prescalers[i]);
        uint32_t write_time_ms = 0;
        uint32_t read_time_ms = 0;
        uint32_t offset;
        uint32_t remaining;
        uint32_t chunk_size;
        W25Q_Status_t status;
        
        /* 保存分频值 */
        g_test_results[i].prescaler_value = prescaler_value;
        
        LOG_INFO("MAIN", "--- 测试分频 %d (%d/%d) ---", prescaler_value, i + 1, PRESCALER_COUNT);
        
        /* 修改SPI分频 */
        spi_status = ChangeSPIPrescaler(g_prescalers[i]);
        if (spi_status != SPI_OK)
        {
            LOG_ERROR("MAIN", "修改SPI分频失败: %d", spi_status);
            g_test_results[i].write_speed_kbps = 0.0f;
            g_test_results[i].read_speed_kbps = 0.0f;
            g_test_results[i].write_time_ms = 0;
            g_test_results[i].read_time_ms = 0;
            continue;
        }
        
        /* 重新初始化W25Q（确保使用新的SPI配置） */
        W25Q_Deinit();
        Delay_ms(10);
        w25q_status = W25Q_Init();
        if (w25q_status != W25Q_OK)
        {
            LOG_ERROR("MAIN", "W25Q重新初始化失败: %d", w25q_status);
            g_test_results[i].write_speed_kbps = 0.0f;
            g_test_results[i].read_speed_kbps = 0.0f;
            g_test_results[i].write_time_ms = 0;
            g_test_results[i].read_time_ms = 0;
            continue;
        }
        
        /* 显示当前测试状态 */
        DisplayTestStatus(prescaler_value, i, PRESCALER_COUNT, "Write");
        
        /* ========== 写入测试 ========== */
        LOG_INFO("MAIN", "分频 %d: 开始写入测试...", prescaler_value);
        
        {
            uint32_t write_start_tick = Delay_GetTick();
            uint32_t write_end_tick;
            
            /* 循环写入1MB数据（使用4KB缓冲区） */
            remaining = TEST_DATA_SIZE;
            offset = 0;
            
            while (remaining > 0)
            {
                chunk_size = (remaining > BUFFER_SIZE) ? BUFFER_SIZE : remaining;
                
                /* 准备测试数据 */
                PrepareTestData(test_data_buffer, chunk_size);
                
                /* 写入数据 */
                status = W25Q_Write(TEST_START_ADDR + offset, test_data_buffer, chunk_size);
                if (status != W25Q_OK)
                {
                    LOG_ERROR("MAIN", "写入失败: 地址 0x%04X, 大小 %lu, 错误: %d",
                             TEST_START_ADDR + offset, (unsigned long)chunk_size, status);
                    break;
                }
                
                /* 等待写入完成 */
                status = W25Q_WaitReady(0);
                if (status != W25Q_OK)
                {
                    LOG_ERROR("MAIN", "等待写入完成失败: %d", status);
                    break;
                }
                
                offset += chunk_size;
                remaining -= chunk_size;
            }
            
            write_end_tick = Delay_GetTick();
            write_time_ms = Delay_GetElapsed(write_end_tick, write_start_tick);
        }
        
        if (write_time_ms > 0)
        {
            g_test_results[i].write_time_ms = write_time_ms;
            g_test_results[i].write_speed_kbps = CalculateSpeed(TEST_DATA_SIZE, write_time_ms);
            LOG_INFO("MAIN", "分频 %d: 写入完成, 耗时: %lu ms, 速度: %.2f KB/s",
                     prescaler_value, (unsigned long)write_time_ms, g_test_results[i].write_speed_kbps);
        }
        else
        {
            g_test_results[i].write_time_ms = 0;
            g_test_results[i].write_speed_kbps = 0.0f;
            LOG_ERROR("MAIN", "分频 %d: 写入测试失败", prescaler_value);
        }
        
        /* 显示当前测试状态 */
        DisplayTestStatus(prescaler_value, i, PRESCALER_COUNT, "Read");
        
        /* ========== 读取测试 ========== */
        LOG_INFO("MAIN", "分频 %d: 开始读取测试...", prescaler_value);
        
        {
            uint32_t read_start_tick = Delay_GetTick();
            uint32_t read_end_tick;
            
            /* 循环读取1MB数据（使用4KB缓冲区） */
            remaining = TEST_DATA_SIZE;
            offset = 0;
            
            while (remaining > 0)
            {
                chunk_size = (remaining > BUFFER_SIZE) ? BUFFER_SIZE : remaining;
                
                /* 读取数据 */
                status = W25Q_Read(TEST_START_ADDR + offset, read_buffer, chunk_size);
                if (status != W25Q_OK)
                {
                    LOG_ERROR("MAIN", "读取失败: 地址 0x%04X, 大小 %lu, 错误: %d",
                             TEST_START_ADDR + offset, (unsigned long)chunk_size, status);
                    break;
                }
                
                offset += chunk_size;
                remaining -= chunk_size;
            }
            
            read_end_tick = Delay_GetTick();
            read_time_ms = Delay_GetElapsed(read_end_tick, read_start_tick);
        }
        
        if (read_time_ms > 0)
        {
            g_test_results[i].read_time_ms = read_time_ms;
            g_test_results[i].read_speed_kbps = CalculateSpeed(TEST_DATA_SIZE, read_time_ms);
            LOG_INFO("MAIN", "分频 %d: 读取完成, 耗时: %lu ms, 速度: %.2f KB/s",
                     prescaler_value, (unsigned long)read_time_ms, g_test_results[i].read_speed_kbps);
        }
        else
        {
            g_test_results[i].read_time_ms = 0;
            g_test_results[i].read_speed_kbps = 0.0f;
            LOG_ERROR("MAIN", "分频 %d: 读取测试失败", prescaler_value);
        }
        
        LOG_INFO("MAIN", "");
        Delay_ms(500);
    }
    
    /* ========== 步骤14：显示测试结果 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "Test Complete!");
    Delay_ms(1000);
    
    /* OLED显示结果摘要 */
    DisplayResultSummary(g_test_results, PRESCALER_COUNT);
    
    /* UART输出详细对比表 */
    PrintResultTable(g_test_results, PRESCALER_COUNT);
    
    /* ========== 步骤15：主循环 ========== */
    while (1)
    {
        LED_Toggle(LED_1);
        Delay_ms(500);
    }
}
