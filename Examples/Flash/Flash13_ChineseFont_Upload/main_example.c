/**
 * @file main_example.c
 * @brief Flash13 - 中文字库UART上传示例
 * @details 演示如何通过UART接收字库文件并写入文件系统，然后显示中文
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
 * - UART1（用于接收字库文件和详细日志输出）
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
 * 9. 通过UART接收字库文件并写入文件系统
 * 10. 初始化中文字库并显示中文
 * 11. 主循环（LED闪烁，OLED显示状态）
 * 
 * 使用流程：
 * 1. 编译并烧录此程序到STM32
 * 2. 运行Python脚本：python Tools/send_font.py COM3 chinese16x16.bin 115200
 * 3. 等待传输完成（显示进度）
 * 4. 字库文件已写入/font/chinese16x16.bin
 * 5. 程序自动测试中文显示
 */

#include "stm32f10x.h"
#include "config.h"  /* 必须在oled_ssd1306.h之前，以便条件编译生效 */
#include "system_init.h"
#include "led.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include "oled_font_chinese16x16.h"
#include "i2c_sw.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "spi_hw.h"
#include "w25q_spi.h"
#include "littlefs_wrapper.h"
#include "fs_wrapper.h"
#include "font_uploader.h"
#include "gpio.h"
#include "board.h"
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
    OLED_ChineseFont_Status_t font_status;
    Log_Status_t log_status;
    FontUpload_Status_t upload_status;
    error_code_t fs_status;
    int debug_status;
    log_config_t log_config;
    uint32_t loop_count = 0;
    
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
    /* 使用config.h中的CONFIG_LOG_LEVEL配置，避免硬编码 */
    #ifdef CONFIG_LOG_LEVEL
    log_config.level = (log_level_t)CONFIG_LOG_LEVEL;
    #else
    log_config.level = LOG_LEVEL_WARN;  /* 默认WARN级别，禁用INFO输出 */
    #endif
    log_config.enable_timestamp = 0;
    log_config.enable_module = 1;
    log_config.enable_color = 0;
    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK)
    {
        ErrorHandler_Handle(log_status, "LOG");
    }
    
    /* ========== 步骤5：输出初始化信息 ========== */
    LOG_INFO("MAIN", "=== Flash13 - 中文字库UART上传示例 ===");
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
        OLED_ShowString(1, 1, "Flash13");
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
    OLED_Clear();
    OLED_ShowString(1, 1, "Init FS...");
    fs_status = FS_Init();
    if (fs_status != FS_WRAPPER_OK)
    {
        OLED_ShowString(2, 1, "FS Init Fail!");
        LOG_ERROR("MAIN", "FS 初始化失败: %d", fs_status);
        ErrorHandler_Handle(fs_status, "FS");
        Delay_ms(2000);
    }
    else
    {
        OLED_ShowString(2, 1, "FS OK");
        LOG_INFO("MAIN", "FS 初始化成功");
    }
    Delay_ms(500);
    
    /* ========== 步骤14：等待上传命令并接收字库文件 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "Font Upload");
    OLED_ShowString(2, 1, "Waiting...");
    OLED_ShowString(3, 1, "Send A=ASCII");
    OLED_ShowString(4, 1, "Send C=Chinese");
    
    char font_filename[32];
    upload_status = FontUpload_WaitForCommand(UART_INSTANCE_1, font_filename);
    
    if (upload_status == FONT_UPLOAD_OK)
    {
        OLED_Clear();
        OLED_ShowString(1, 1, "Command OK");
        OLED_ShowString(2, 1, font_filename);
        OLED_ShowString(3, 1, "Receiving...");
        LOG_INFO("MAIN", "Command received: %s", font_filename);
        
        /* 开始接收文件 */
        upload_status = FontUpload_ReceiveFile(UART_INSTANCE_1, font_filename);
        
        if (upload_status == FONT_UPLOAD_OK)
        {
            LOG_INFO("MAIN", "Font upload successful: %s", font_filename);
        }
        else
        {
            OLED_ShowString(3, 1, "Upload Fail!");
            OLED_ShowNum(4, 1, upload_status, 4);
            LOG_ERROR("MAIN", "Font upload failed: %d", upload_status);
        }
    }
    else
    {
        OLED_ShowString(2, 1, "Command Fail!");
        LOG_ERROR("MAIN", "Wait for command failed: %d", upload_status);
    }
    
    /* ========== 步骤15：初始化字库并显示测试 ========== */
    OLED_Clear();
    if (upload_status == FONT_UPLOAD_OK)
    {
        OLED_ShowString(1, 1, "Upload OK!");
        LOG_INFO("MAIN", "Font Upload OK: %s", font_filename);
        
        /* 如果是中文字库，初始化并测试显示 */
        if (strcmp(font_filename, "chinese16x16.bin") == 0)
        {
            OLED_ShowString(2, 1, "Font Init...");
            font_status = OLED_ChineseFont_Init();
            if (font_status == OLED_CHINESE_FONT_OK)
            {
                OLED_ShowString(2, 1, "Font Init OK");
                LOG_INFO("MAIN", "Chinese Font Init OK");
                Delay_ms(1000);
                
                /* 测试显示中文（GB2312编码） */
                OLED_Clear();
                /* 注意：需要GB2312编码的字符串 */
                /* 可以使用UTF8_2_GB2312.py工具将UTF-8字符串转换为GB2312编码 */
                /* 示例：测试中文显示 */
                OLED_ShowStringGB2312(1, 1, "\xB2\xE2\xCA\xD4\xD6\xD0\xCE\xC4");  /* "测试中文" (GB2312) */
                OLED_ShowStringGB2312(2, 1, "\xCF\xD4\xCA\xBE\xB3\xC9\xB9\xA6");  /* "显示成功" (GB2312) */
                OLED_ShowString(3, 1, "Font OK!");  /* 英文显示 */
                LOG_INFO("MAIN", "Chinese display test OK");
            }
            else
            {
                OLED_ShowString(2, 1, "Font Init Fail");
                OLED_ShowNum(3, 1, font_status, 4);
                LOG_ERROR("MAIN", "Chinese Font Init Failed: %d", font_status);
            }
        }
        else
        {
            /* ASCII字库，不进行中文显示测试 */
            OLED_ShowString(2, 1, "ASCII Font OK");
            LOG_INFO("MAIN", "ASCII Font Upload OK");
        }
    }
    else
    {
        OLED_ShowString(1, 1, "Upload Failed!");
        OLED_ShowNum(2, 1, upload_status, 4);
        LOG_ERROR("MAIN", "Upload Failed: %d", upload_status);
    }
    
    LOG_INFO("MAIN", "=== 初始化完成，进入主循环 ===");
    Delay_ms(1000);
    
    /* ========== 步骤15：主循环 ========== */
    while (1)
    {
        loop_count++;
        LED_Toggle(LED_1);
        
        /* 每10次循环（约5秒）更新一次OLED显示 */
        if (loop_count % 10 == 0) {
            char buf[17];
            snprintf(buf, sizeof(buf), "Running:%lu", (unsigned long)loop_count);
            OLED_ShowString(4, 1, buf);
            LOG_INFO("MAIN", "主循环运行中... (循环 %lu)", (unsigned long)loop_count);
        }
        
        Delay_ms(500);
    }
}
