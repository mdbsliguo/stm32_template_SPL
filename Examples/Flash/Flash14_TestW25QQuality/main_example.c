/**
 * @file main_example.c
 * @brief Flash14 - W25Q品质测试案例
 * @details 演示W25Q系列Flash芯片的纯软件质量检测流程，包括自动化到货抽检、山寨货鉴别、翻新货鉴定和寿命健康度评估
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
 * 7. W25Q初始化与设备识别
 * 8. 质量检测流程：
 *    - 阶段1：数字身份验证（2分钟/片）
 *    - 阶段2：山寨货深度鉴别（15秒/片）
 *    - 阶段3：翻新货时序指纹鉴定（30秒/片）
 *    - 阶段4：寿命健康度量化评估（3分钟/片）
 *    - 阶段5：综合判定与自动化决策
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
#include "flash14_quality_test.h"
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
    Quality_Test_Result_t test_result;
    Quality_Test_Status_t quality_status;
    
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
    LOG_INFO("MAIN", "=== Flash14 - W25Q品质测试案例 ===");
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
        OLED_ShowString(1, 1, "Flash14");
        OLED_ShowString(2, 1, "Quality Test");
        OLED_ShowString(3, 1, "Initializing...");
        LOG_INFO("MAIN", "OLED 已初始化");
    }
    
    Delay_ms(500);
    
    /* ========== 步骤9：SPI初始化 ========== */
    OLED_ShowString(3, 1, "Init SPI2...");
    
    /* 配置SPI2 NSS引脚为GPIO输出（软件NSS模式，SPI驱动不会自动配置） */
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
    
    /* ========== 步骤11：质量检测初始化 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "Quality Test");
    OLED_ShowString(2, 1, "Initializing...");
    
    quality_status = QualityTest_Init();
    if (quality_status != QUALITY_TEST_OK)
    {
        OLED_ShowString(3, 1, "Init Failed!");
        LOG_ERROR("MAIN", "质量检测初始化失败: %d", quality_status);
        ErrorHandler_Handle(quality_status, "QualityTest");
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "质量检测模块初始化成功");
    OLED_ShowString(3, 1, "Init OK");
    Delay_ms(500);
    
    /* ========== 步骤12：执行质量检测流程 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "Quality Test");
    OLED_ShowString(2, 1, "Running...");
    
    LOG_INFO("MAIN", "=== 开始W25Q品质测试 ===");
    Delay_ms(500);  /* 给UART一些时间输出 */
    
    /* 执行完整质量检测流程 */
    LED_On(LED_1);  /* LED亮起表示准备调用函数 */
    Delay_ms(100);
    LOG_INFO("MAIN", "[DEBUG] 准备调用 QualityTest_RunFullTest");
    Delay_ms(500);  /* 给UART一些时间输出 */
    
    LED_Toggle(LED_1);  /* LED切换表示开始调用函数 */
    Delay_ms(100);
    quality_status = QualityTest_RunFullTest(&test_result);
    LED_Toggle(LED_1);  /* LED切换表示函数返回 */
    
    Delay_ms(500);  /* 给UART一些时间输出 */
    LOG_INFO("MAIN", "[DEBUG] QualityTest_RunFullTest 返回，状态 = %d", quality_status);
    Delay_ms(500);  /* 给UART一些时间输出 */
    if (quality_status != QUALITY_TEST_OK)
    {
        OLED_ShowString(3, 1, "Test Failed!");
        LOG_ERROR("MAIN", "质量检测失败: %d", quality_status);
        ErrorHandler_Handle(quality_status, "QualityTest");
    }
    else
    {
        /* 显示测试结果 */
        char grade_str[17];
        const char* grade_names[] = {"Grade A", "Grade B", "Grade C", "Grade D"};
        
        if (test_result.grade < 4)
        {
            snprintf(grade_str, sizeof(grade_str), "%s", grade_names[test_result.grade]);
        }
        else
        {
            snprintf(grade_str, sizeof(grade_str), "Unknown");
        }
        
        OLED_Clear();
        OLED_ShowString(1, 1, "Test Complete");
        OLED_ShowString(2, 1, grade_str);
        
        if (test_result.health_score >= 0)
        {
            char health_str[17];
            snprintf(health_str, sizeof(health_str), "Health:%d%%", test_result.health_score);
            OLED_ShowString(3, 1, health_str);
        }
        
        LOG_INFO("MAIN", "=== 质量检测完成 ===");
        LOG_INFO("MAIN", "质量等级: %s", grade_str);
        if (test_result.health_score >= 0)
        {
            LOG_INFO("MAIN", "健康度: %d%%", test_result.health_score);
        }
    }
    
    Delay_ms(2000);
    
    /* ========== 步骤13：主循环 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "Flash14");
    OLED_ShowString(2, 1, "Quality Test");
    OLED_ShowString(3, 1, "Complete");
    LOG_INFO("MAIN", "=== 进入主循环 ===");
    
    while (1)
    {
        LED_Toggle(LED_1);
        Delay_ms(500);
    }
}
