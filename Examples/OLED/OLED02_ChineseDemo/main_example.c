/**
 * @file main_example.c
 * @brief OLED02 - 中文OLED显示示例
 * @details 演示如何在OLED上显示中文，包括单个字符、字符串、中英文混显等功能
 * 
 * 硬件连接：
 * - W25Q SPI Flash模块连接到SPI2（存储中文字库）
 *   - CS：PA11
 *   - SCK：PB13（SPI2_SCK）
 *   - MISO：PB14（SPI2_MISO）
 *   - MOSI：PB15（SPI2_MOSI）
 *   - VCC：3.3V
 *   - GND：GND
 * - OLED显示屏（用于显示中文）
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
 * 7. W25Q初始化与设备识别
 * 8. LittleFS文件系统初始化、挂载
 * 9. 中文字库初始化
 * 10. 5个中文显示测试
 * 11. 主循环（LED闪烁，OLED显示状态）
 * 
 * 前置条件：
 * - 需要先通过Flash13案例上传字库文件到W25Q Flash
 * - 字库文件路径：/font/chinese16x16.bin
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "config.h"  /* 必须在包含oled_ssd1306.h之前，以定义CONFIG_MODULE_FS_WRAPPER_ENABLED */
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
#include "littlefs_wrapper.h"
#include "fs_wrapper.h"
#include "oled_font_chinese16x16.h"
#include "oled_font_ascii16x16.h"
#include "gpio.h"
#include "board.h"
#include "oled02_tests.h"
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
    OLED_ChineseFont_Status_t chinese_font_status;
    OLED_AsciiFont_Status_t ascii_font_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    
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
    LOG_INFO("MAIN", "=== OLED02 - 中文OLED显示示例 ===");
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
        OLED_ShowString(1, 1, "OLED02");
        OLED_ShowString(2, 1, "Initializing...");
        LOG_INFO("MAIN", "OLED 已初始化");
    }
    
    Delay_ms(500);
    
    /* ========== 步骤9：SPI初始化 ========== */
    OLED_ShowString(3, 1, "Init SPI2...");
    
    /* 配置SPI2 NSS引脚为GPIO输出（软件NSS模式，SPI驱动不会自动配置） */
    /* 使用board.h中定义的宏，避免硬编码 */
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
    
    /* 显示W25Q信息 */
    const w25q_dev_t* dev_info = W25Q_GetInfo();
    if (dev_info != NULL) {
        char buf[17];
        snprintf(buf, sizeof(buf), "Size:%d MB", dev_info->capacity_mb);
        OLED_ShowString(4, 1, buf);
        
        LOG_INFO("MAIN", "W25Q信息:");
        LOG_INFO("MAIN", "  容量: %d MB", dev_info->capacity_mb);
        LOG_INFO("MAIN", "  地址字节数: %d", dev_info->addr_bytes);
        LOG_INFO("MAIN", "  4字节模式: %s", dev_info->is_4byte_mode ? "是" : "否");
    }
    
    Delay_ms(1000);
    
    /* ========== 步骤11：LittleFS初始化 ========== */
    OLED_ShowString(3, 1, "Init LittleFS...");
    LittleFS_Status_t littlefs_status = LittleFS_Init();
    if (littlefs_status != LITTLEFS_OK)
    {
        OLED_ShowString(4, 1, "LittleFS Init Fail!");
        LOG_ERROR("MAIN", "LittleFS 初始化失败: %d", littlefs_status);
        ErrorHandler_Handle(littlefs_status, "LittleFS");
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "LittleFS 初始化成功");
    OLED_ShowString(4, 1, "LittleFS Ready");
    Delay_ms(500);
    
    /* ========== 步骤12：挂载前确保CS引脚配置正确 ========== */
    /* 确保SPI2 NSS引脚（CS）配置为推挽输出并拉高 */
    /* 使用GPIO模块API，避免直接操作寄存器（符合规范） */
    LOG_INFO("MAIN", "挂载前确保CS引脚配置正确...");
    
    /* 确保GPIO时钟已使能 */
    GPIO_EnableClock(SPI2_NSS_PORT);
    
    /* 确保CS引脚配置为推挽输出 */
    GPIO_Config(SPI2_NSS_PORT, SPI2_NSS_PIN, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHz);
    
    /* 确保CS引脚为高（释放状态） */
    GPIO_WritePin(SPI2_NSS_PORT, SPI2_NSS_PIN, Bit_SET);
    
    LOG_INFO("MAIN", "CS引脚已配置为推挽输出并拉高");
    Delay_ms(500);
    
    /* ========== 步骤13：文件系统初始化 ========== */
    OLED_ShowString(3, 1, "Init FS...");
    error_code_t fs_status = FS_Init();
    if (fs_status != FS_WRAPPER_OK)
    {
        OLED_ShowString(4, 1, "FS Init Fail!");
        LOG_ERROR("MAIN", "FS 初始化失败: %d", fs_status);
        ErrorHandler_Handle(fs_status, "FS");
        Delay_ms(2000);
    }
    else
    {
        OLED_ShowString(4, 1, "FS OK");
        LOG_INFO("MAIN", "FS 初始化成功");
    }
    Delay_ms(500);
    
    /* ========== 步骤9：初始化ASCII字库 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "ASCII Font...");
    
    ascii_font_status = OLED_AsciiFont_Init();
    if (ascii_font_status == OLED_ASCII_FONT_OK)
    {
        OLED_ShowString(2, 1, "ASCII Font OK");
        LOG_INFO("MAIN", "ASCII Font Init OK");
    }
    else
    {
        OLED_ShowString(2, 1, "ASCII Font Fail");
        OLED_ShowNum(3, 1, ascii_font_status, 4);
        LOG_ERROR("MAIN", "ASCII Font Init Failed: %d", ascii_font_status);
    }
    Delay_ms(1000);
    
    /* ========== 步骤10：初始化中文字库 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "Chinese Font...");
    
    chinese_font_status = OLED_ChineseFont_Init();
    if (chinese_font_status == OLED_CHINESE_FONT_OK)
    {
        OLED_ShowString(2, 1, "Chinese Font OK");
        LOG_INFO("MAIN", "Chinese Font Init OK");
        Delay_ms(1000);
    }
    else
    {
        OLED_ShowString(2, 1, "Chinese Font Fail");
        OLED_ShowNum(3, 1, chinese_font_status, 4);
        LOG_ERROR("MAIN", "Chinese Font Init Failed: %d", chinese_font_status);
        LOG_ERROR("MAIN", "请先运行Flash13案例上传字库文件");
        Delay_ms(3000);
        while(1) { Delay_ms(1000); }  /* 字库未初始化，无法继续 */
    }
    
    /* ========== 步骤15：执行中文显示演示 ========== */
    LOG_INFO("MAIN", "=== 开始中文显示演示 ===");
    run_all_oled02_tests();
    
    /* 注意：run_all_oled02_tests()会一直循环，不会返回 */
    /* 演示内容：
     * 第1行：方法编号（Method: 1-15，每3秒切换）
     * 第2行：方法中文名称（自动更新）
     * 第3行：中文
     * 第4行：显示
     */
    
    /* 这里不会执行到，因为test1会一直循环 */
    while (1)
    {
        Delay_ms(1000);
    }
}

