/**
 * @file main_example.c
 * @brief 案例 - ADC单通道电压采集示例
 * @example Examples/ADC/ADC01_SingleChannelVoltage/main_example.c
 * @details ADC单通道电压采集示例，演示ADC的基本使用方法
 *
 * 硬件要求：
 * - ADC输入连接到PA0（ADC_Channel_0）
 * - OLED显示屏（SSD1306，I2C接口，用于显示ADC值和电压值）
 *   - SCL连接到PB8
 *   - SDA连接到PB9
 *
 * 硬件配置：
 * 在 Examples/ADC/ADC01_SingleChannelVoltage/board.h 中配置：
 * ```c
 * #define ADC_CONFIGS { \
 *     {ADC1, {ADC_Channel_0}, 1, ADC_SampleTime_55Cycles5, 1},  // ADC1：PA0，单通道，55.5周期采样，启用 \
 * }
 * ```
 *
 * 使用方法：
 * 1. 打开案例工程：双击 Examples/ADC/ADC01_SingleChannelVoltage/Examples.uvprojx
 * 2. 根据实际硬件修改 Examples/ADC/ADC01_SingleChannelVoltage/board.h 中的ADC配置
 * 3. 编译运行（F7编译，F8下载）
 * 
 * 注意：本案例是独立工程，无需复制文件
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "error_code.h"
#include "delay.h"
#include "board.h"
#include "adc.h"
#include "oled_ssd1306.h"
#include "i2c_sw.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    UART_Status_t uart_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    SoftI2C_Status_t i2c_status;
    OLED_Status_t oled_status;
    ADC_Status_t adc_status;
    
    uint16_t adc_value = 0;
    float voltage = 0.0f;
    char display_buf[32];
    
    /* ========== 步骤1：系统初始化 ========== */
    System_Init();
    
    /* ========== 步骤2：UART初始化 ========== */
    uart_status = UART_Init(UART_INSTANCE_1);
    if (uart_status != UART_OK)
    {
        /* UART初始化失败，无法继续，进入死循环 */
        while (1)
        {
            Delay_ms(1000);
        }
    }
    
    /* 等待UART稳定 */
    Delay_ms(100);
    
    /* ========== 步骤3：Debug模块初始化（UART模式） ========== */
    debug_status = Debug_Init(DEBUG_MODE_UART, 115200);
    if (debug_status != 0)
    {
        /* Debug初始化失败，无法继续，进入死循环 */
        while (1)
        {
            Delay_ms(1000);
        }
    }
    
    /* 等待Debug模块稳定 */
    Delay_ms(100);
    
    /* ========== 步骤4：Log模块初始化 ========== */
    memset(&log_config, 0, sizeof(log_config));
    log_config.level = LOG_LEVEL_DEBUG;        /* 日志级别：DEBUG（显示所有日志） */
    log_config.enable_timestamp = 0;          /* 禁用时间戳（简化输出） */
    log_config.enable_module = 1;              /* 启用模块名显示 */
    log_config.enable_color = 0;               /* 禁用颜色输出（串口助手可能不支持） */
    
    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK)
    {
        ErrorHandler_Handle(log_status, "LOG");
    }
    
    /* ========== 步骤5：错误处理模块 ========== */
    /* 注意：ErrorHandler模块在编译时自动初始化，无需显式调用 */
    
    /* ========== 步骤6：输出初始化信息 ========== */
    LOG_INFO("MAIN", "=== 系统初始化完成 ===");
    LOG_INFO("MAIN", "UART1已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "Debug模块已初始化: UART模式");
    LOG_INFO("MAIN", "Log模块已初始化");
    LOG_INFO("MAIN", "ErrorHandler模块已就绪");
    LOG_INFO("MAIN", "=== ADC单通道电压采集示例 ===");
    
    /* ========== 步骤7：初始化其他模块（按依赖顺序） ========== */
    
    /* ADC初始化 */
    LOG_INFO("MAIN", "正在初始化ADC...");
    adc_status = ADC_ModuleInit(ADC_INSTANCE_1);
    if (adc_status != ADC_OK)
    {
        LOG_ERROR("MAIN", "ADC初始化失败: %d", adc_status);
        ErrorHandler_Handle(adc_status, "ADC");
        /* ADC初始化失败，无法继续，进入死循环 */
        while (1)
        {
            Delay_ms(1000);
        }
    }
    else
    {
        LOG_INFO("MAIN", "ADC已初始化: ADC1，PA0（ADC_Channel_0）");
    }
    
    /* 软件I2C初始化（OLED需要） */
    i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_1);
    if (i2c_status != SOFT_I2C_OK)
    {
        LOG_ERROR("MAIN", "软件I2C初始化失败: %d", i2c_status);
        ErrorHandler_Handle(i2c_status, "SOFT_I2C");
        /* I2C初始化失败，OLED无法使用，但ADC仍可工作 */
    }
    else
    {
        LOG_INFO("MAIN", "软件I2C已初始化: PB8(SCL), PB9(SDA)");
    }
    
    /* OLED初始化 */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        LOG_ERROR("MAIN", "OLED初始化失败: %d", oled_status);
        ErrorHandler_Handle(oled_status, "OLED");
        /* OLED初始化失败，但可以继续运行（ADC仍可工作） */
    }
    else
    {
        OLED_Clear();
        OLED_ShowString(1, 1, "System Init OK");
        OLED_ShowString(2, 1, "UART Ready");
        OLED_ShowString(3, 1, "Log Ready");
        OLED_ShowString(4, 1, "ADC Ready");
        LOG_INFO("MAIN", "OLED已初始化并显示");
    }
    
    /* 清屏并显示标题 */
    Delay_ms(1000);
    OLED_Clear();
    OLED_ShowString(1, 1, "ADC01 Demo");
    OLED_ShowString(2, 1, "Channel: PA0");
    OLED_ShowString(3, 1, "ADC: ----");
    OLED_ShowString(4, 1, "Volt: --.---V");
    LOG_INFO("MAIN", "=== ADC单通道电压采集演示开始 ===");
    
    /* ========== 步骤8：主循环 ========== */
    /* 主循环：每500ms采集一次ADC值，计算电压并显示 */
    while (1)
    {
        /* 读取ADC值 */
        adc_status = ADC_ReadChannel(ADC_INSTANCE_1, ADC_Channel_0, &adc_value, 1000);
        if (adc_status != ADC_OK)
        {
            LOG_ERROR("MAIN", "ADC读取失败: %d", adc_status);
            ErrorHandler_Handle(adc_status, "ADC");
            Delay_ms(500);
            continue;
        }
        
        /* 计算电压值（12位ADC，0-4095对应0V-3.3V） */
        voltage = (float)adc_value * 3.3f / 4095.0f;
        
        /* 串口输出详细日志 */
        LOG_DEBUG("ADC", "ADC值: %d, 电压: %.3fV", adc_value, voltage);
        
        /* OLED显示ADC原始值 */
        memset(display_buf, 0, sizeof(display_buf));
        sprintf(display_buf, "ADC: %4d", adc_value);
        OLED_ShowString(3, 1, display_buf);
        
        /* OLED显示电压值 */
        memset(display_buf, 0, sizeof(display_buf));
        sprintf(display_buf, "Volt: %5.3fV", voltage);
        OLED_ShowString(4, 1, display_buf);
        
        /* 延时500ms */
        Delay_ms(500);
    }
}
