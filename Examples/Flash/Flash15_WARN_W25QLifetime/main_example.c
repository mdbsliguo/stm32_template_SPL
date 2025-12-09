/**
 * @file main_example.c
 * @brief Flash15 - W25Q寿命测试案例
 * @details 演示W25Q系列Flash芯片的寿命测试流程，自动适配不同型号（测到报废）
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
 * - LED1：PA1（系统状态指示）
 * 
 * 功能演示：
 * 1. 系统初始化
 * 2. UART、Debug、Log初始化
 * 3. LED初始化
 * 4. 软件I2C初始化
 * 5. OLED初始化
 * 6. SPI初始化
 * 7. W25Q初始化与设备识别（自动适配不同型号）
 * 8. 正式寿命测试：测到报废（持续运行直至芯片报废）
 * 9. 主循环（LED闪烁，OLED显示状态）
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
#include "flash15_endurance_test.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ==================== 主函数 ==================== */

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
    
    /* 正式寿命测试相关 */
    Endurance_Test_Config_t endurance_config;
    Endurance_Test_Result_t endurance_result;
    Endurance_Test_Status_t endurance_status;
    
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
    LOG_INFO("MAIN", "=== Flash15 - W25Q寿命测试案例 ===");
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
        OLED_ShowString(1, 1, "Flash15");
        OLED_ShowString(2, 1, "Lifetime Test");
        OLED_ShowString(3, 1, "Initializing...");
        LOG_INFO("MAIN", "OLED 已初始化");
    }
    
    Delay_ms(500);
    
    /* ========== 步骤9：SPI初始化 ========== */
    OLED_ShowString(3, 1, "Init SPI2...");
    
    /* 配置SPI2 NSS引脚为GPIO输出（软件NSS模式） */
    GPIO_EnableClock(SPI2_NSS_PORT);
    GPIO_Config(SPI2_NSS_PORT, SPI2_NSS_PIN, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHz);
    GPIO_WritePin(SPI2_NSS_PORT, SPI2_NSS_PIN, Bit_SET);  /* NSS默认拉高（不选中） */
    
    spi_status = SPI_HW_Init(SPI_INSTANCE_2);
    if (spi_status != SPI_OK)
    {
        char err_buf[17];
        snprintf(err_buf, sizeof(err_buf), "SPI Fail:%d", spi_status);
        OLED_ShowString(4, 1, err_buf);
        LOG_ERROR("MAIN", "SPI 初始化失败: %d", spi_status);
        ErrorHandler_Handle(spi_status, "SPI");
        Delay_ms(3000);
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
    
    /* 显示W25Q信息（自动适配不同型号） */
    const w25q_dev_t* dev_info = W25Q_GetInfo();
    if (dev_info != NULL) {
        char buf[17];
        snprintf(buf, sizeof(buf), "Size:%d MB", dev_info->capacity_mb);
        OLED_ShowString(4, 1, buf);
        
        LOG_INFO("MAIN", "W25Q芯片信息（自动识别）:");
        LOG_INFO("MAIN", "  容量: %d MB", dev_info->capacity_mb);
        LOG_INFO("MAIN", "  地址字节数: %d", dev_info->addr_bytes);
        LOG_INFO("MAIN", "  4字节模式: %s", dev_info->is_4byte_mode ? "是" : "否");
        LOG_INFO("MAIN", "  制造商ID: 0x%04X", dev_info->manufacturer_id);
        LOG_INFO("MAIN", "  设备ID: 0x%04X", dev_info->device_id);
    }
    
    Delay_ms(1000);
    
    /* ========== 步骤11：开始正式寿命测试 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "Flash15");
    OLED_ShowString(2, 1, "Endurance Test");
    LOG_INFO("MAIN", "=== 开始正式寿命测试流程（测到报废） ===");
    
    Delay_ms(1000);
    
    /* ========== 步骤12：执行正式寿命测试 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "Endurance Test");
    OLED_ShowString(2, 1, "Initializing...");
    
    endurance_status = EnduranceTest_Init();
    if (endurance_status != ENDURANCE_TEST_OK)
    {
        OLED_ShowString(3, 1, "Init Failed!");
        LOG_ERROR("MAIN", "寿命测试初始化失败: %d", endurance_status);
        ErrorHandler_Handle(endurance_status, "EnduranceTest");
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "寿命测试模块初始化成功");
    OLED_ShowString(3, 1, "Init OK");
    Delay_ms(500);
    
    /* 配置正式寿命测试 */
    memset(&endurance_config, 0, sizeof(endurance_config));
    endurance_config.deep_check_interval = 1000;  /* 每1000次循环执行深度检查 */
    endurance_config.log_interval = 100;           /* 每100次循环记录日志 */
    endurance_config.verbose_log = 1;             /* 输出详细日志 */
    
    /* 执行正式寿命测试 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Endurance Test");
    OLED_ShowString(2, 1, "Running...");
    OLED_ShowString(3, 1, "To Failure");
    
    LOG_WARN("MAIN", "警告：此测试会将芯片测到报废，请确认！（擦除时禁止重启，会导致测试数据丢失）");
    Delay_ms(2000);
    
    LED_On(LED_1);
    Delay_ms(100);
    endurance_status = EnduranceTest_Run(&endurance_config, &endurance_result);
    LED_Off(LED_1);
    
    Delay_ms(500);
    if (endurance_status != ENDURANCE_TEST_OK)
    {
        OLED_ShowString(3, 1, "Test Failed!");
        LOG_ERROR("MAIN", "寿命测试失败: %d", endurance_status);
        ErrorHandler_Handle(endurance_status, "EnduranceTest");
    }
    else
    {
        /* 显示测试结果 */
        char result_str[17];
        char total_written_str[17];
        OLED_Clear();
        OLED_ShowString(1, 1, "Test Complete");
        
        if (endurance_result.chip_dead)
        {
            OLED_ShowString(2, 1, "Chip Dead");
            /* 显示总写入量（GB） */
            snprintf(total_written_str, sizeof(total_written_str), "Total:%.2fGB", 
                     (float)endurance_result.total_data_written_mb / 1024.0f);
            OLED_ShowString(3, 1, total_written_str);
        }
        else
        {
            OLED_ShowString(2, 1, "Chip OK");
            snprintf(result_str, sizeof(result_str), "Cycles:%lu", endurance_result.total_cycles);
            OLED_ShowString(3, 1, result_str);
        }
        
        /* 详细汇总信息已在 EnduranceTest_Run() 中输出，这里只输出关键信息 */
        LOG_INFO("MAIN", "=== 寿命测试完成 ===");
        LOG_INFO("MAIN", "总写入数据量: %llu MB (%.2f GB)", 
                 (unsigned long long)endurance_result.total_data_written_mb, 
                 (float)endurance_result.total_data_written_mb / 1024.0f);
    }
    
    Delay_ms(2000);
    
    /* ========== 步骤13：测试完成，程序结束 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "Flash15");
    OLED_ShowString(2, 1, "Test Complete");
    
    /* 保持LED闪烁，但不输出日志 */
    while (1)
    {
        LED_Toggle(LED_1);
        Delay_ms(500);
    }
}
