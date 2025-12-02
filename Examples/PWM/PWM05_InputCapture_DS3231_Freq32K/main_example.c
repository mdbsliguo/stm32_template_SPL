/**
 * @file main_example.c
 * @brief PWM05 - DS3231 32K PWMI输入捕获测量示例
 * @details 演示使用PWMI（PWM输入模式）测量DS3231的32K输出频率（32kHz）
 * @version 1.0.0
 * @date 2024-01-01
 *
 * 功能说明：
 * - 使用DS3231的32K引脚输出32kHz方波
 * - 使用TIM3_CH2（PA7）PWMI输入捕获测量32K频率
 * - 使用PWMI模式（双边沿捕获）同时测量频率和占空比
 * - OLED显示测量结果（频率、周期、占空比）
 * - UART输出详细日志（支持中文，GB2312编码）
 *
 * 硬件要求：
 * - DS3231实时时钟模块（I2C接口）
 *   - SCL连接到PB10（软件I2C2）
 *   - SDA连接到PB11（软件I2C2）
 *   - 32K引脚连接到PA7（TIM3_CH2，PWMI输入捕获）
 *   - VCC连接到3.3V
 *   - GND连接到GND
 * - USART1：PA9(TX), PA10(RX)，115200波特率
 * - OLED显示屏：SSD1306（I2C接口），PB8(SCL), PB9(SDA)
 *
 * 技术要点：
 * - DS3231配置32K输出（DS3231_Enable32kHz）
 * - TIM3_CH2配置为PWMI模式（IC_POLARITY_BOTH）
 * - 使用IC_MeasurePWM测量32K频率和占空比
 * - PWMI模式：通道1捕获周期，通道2捕获脉宽（或相反）
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "error_code.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include "i2c_sw.h"
#include "ds3231.h"
#include "timer_input_capture.h"
#include <stdio.h>
#include <string.h>

/* ========== 全局变量 ========== */

static uint32_t last_measure_time = 0;
static uint32_t last_oled_update_time = 0;

/* ========== 函数声明 ========== */

static void UpdateOLEDDisplay(const IC_MeasureResult_t *result);

/* ========== 主函数 ========== */

int main(void)
{
    UART_Status_t uart_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    SoftI2C_Status_t soft_i2c_status;
    OLED_Status_t oled_status;
    DS3231_Status_t ds3231_status;
    IC_Status_t ic_status;
    DS3231_Config_t ds3231_config;
    uint8_t osf_flag;
    IC_MeasureResult_t measure_result;
    
    /* ========== 步骤1：系统初始化 ========== */
    System_Init();
    
    /* ========== 步骤2：初始化UART1 ========== */
    uart_status = UART_Init(UART_INSTANCE_1);
    if (uart_status != UART_OK)
    {
        /* UART初始化失败，无法继续，进入死循环 */
        while(1) { Delay_ms(1000); }
    }
    Delay_ms(100);
    
    /* ========== 步骤3：初始化Debug模块（UART模式） ========== */
    debug_status = Debug_Init(DEBUG_MODE_UART, 115200);
    if (debug_status != 0)
    {
        /* Debug初始化失败，无法继续，进入死循环 */
        while(1) { Delay_ms(1000); }
    }
    Delay_ms(100);
    
    /* ========== 步骤4：初始化Log模块 ========== */
    memset(&log_config, 0, sizeof(log_config));
    log_config.level = LOG_LEVEL_DEBUG;        /* 日志级别：DEBUG（显示所有日志） */
    log_config.enable_timestamp = 0;          /* 禁用时间戳（简化输出） */
    log_config.enable_module = 1;              /* 启用模块名显示 */
    log_config.enable_color = 0;               /* 禁用颜色输出（串口助手可能不支持） */
    
    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK)
    {
        ErrorHandler_Handle(log_status, "LOG");
        /* Log初始化失败，但可以继续运行（使用UART直接输出） */
    }
    
    /* ========== 步骤5：错误处理模块 ========== */
    /* 注意：ErrorHandler模块在编译时自动初始化，无需显式调用 */
    
    /* ========== 步骤6：输出初始化信息 ========== */
    LOG_INFO("MAIN", "=== PWM05 PWMI输入捕获测DS3231 32K频率示例 ===");
    LOG_INFO("MAIN", "系统初始化完成");
    LOG_INFO("MAIN", "UART1已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "Debug模块已初始化: UART模式");
    LOG_INFO("MAIN", "Log模块已初始化");
    LOG_INFO("MAIN", "ErrorHandler模块已就绪");
    
    /* ========== 步骤7：初始化OLED ========== */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        LOG_ERROR("MAIN", "OLED初始化失败: %d", oled_status);
        ErrorHandler_Handle(oled_status, "OLED");
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "OLED已初始化");
    
    /* ========== 步骤8：初始化软件I2C2（用于DS3231） ========== */
    LOG_INFO("MAIN", "初始化软件I2C2（PB10/11，用于DS3231）");
    soft_i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_2);
    if (soft_i2c_status != SOFT_I2C_OK)
    {
        LOG_ERROR("MAIN", "软件I2C2初始化失败: %d", soft_i2c_status);
        ErrorHandler_Handle(soft_i2c_status, "SoftI2C");
        if (oled_status == OLED_OK)
        {
            OLED_ShowString(3, 1, "I2C Init Fail!");
        }
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "软件I2C2已初始化: PB10(SCL), PB11(SDA)");
    
    /* ========== 步骤9：初始化DS3231 ========== */
    LOG_INFO("MAIN", "初始化DS3231模块");
    ds3231_config.interface_type = DS3231_INTERFACE_SOFTWARE;
    ds3231_config.config.software.soft_i2c_instance = SOFT_I2C_INSTANCE_2;
    
    ds3231_status = DS3231_Init(&ds3231_config);
    if (ds3231_status != DS3231_OK)
    {
        LOG_ERROR("MAIN", "DS3231初始化失败: %d", ds3231_status);
        ErrorHandler_Handle(ds3231_status, "DS3231");
        if (oled_status == OLED_OK)
        {
            OLED_ShowString(3, 1, "DS3231 Init Fail!");
        }
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "DS3231已初始化");
    
    /* ========== 步骤10：检查并清除OSF标志 ========== */
    ds3231_status = DS3231_CheckOSF(&osf_flag);
    if (ds3231_status == DS3231_OK)
    {
        if (osf_flag)
        {
            LOG_WARN("MAIN", "DS3231 OSF标志已设置，正在清除");
            DS3231_ClearOSF();
            Delay_ms(100);
        }
        LOG_INFO("MAIN", "DS3231 OSF标志正常");
    }
    
    /* ========== 步骤11：启动DS3231振荡器 ========== */
    ds3231_status = DS3231_Start();
    if (ds3231_status != DS3231_OK)
    {
        LOG_ERROR("MAIN", "DS3231启动失败: %d", ds3231_status);
        ErrorHandler_Handle(ds3231_status, "DS3231");
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "DS3231振荡器已启动");
    
    /* ========== 步骤12：使能DS3231 32K输出 ========== */
    LOG_INFO("MAIN", "使能DS3231 32K输出");
    ds3231_status = DS3231_Enable32kHz();
    if (ds3231_status != DS3231_OK)
    {
        LOG_ERROR("MAIN", "DS3231 32K输出使能失败: %d", ds3231_status);
        ErrorHandler_Handle(ds3231_status, "DS3231");
    }
    else
    {
        LOG_INFO("MAIN", "DS3231 32K输出已使能（32kHz方波）");
    }
    
    /* ========== 步骤13：初始化PWMI输入捕获（TIM3_CH2，用于32K） ========== */
    LOG_INFO("MAIN", "初始化PWMI输入捕获: TIM3_CH2（PA7，32K）");
    /* 注意：PWMI模式需要使用IC_POLARITY_BOTH（双边沿捕获） */
    ic_status = IC_Init(IC_INSTANCE_TIM3, IC_CHANNEL_2, IC_POLARITY_BOTH);
    if (ic_status != IC_OK)
    {
        LOG_ERROR("MAIN", "PWMI输入捕获初始化失败: %d", ic_status);
        ErrorHandler_Handle(ic_status, "IC");
        if (oled_status == OLED_OK)
        {
            OLED_ShowString(3, 1, "IC Init Fail!");
        }
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "PWMI输入捕获已初始化: TIM3_CH2（32K）");
    
    /* ========== 步骤14：启动输入捕获 ========== */
    LOG_INFO("MAIN", "输入捕获已启动: TIM3_CH2（32K）");
    ic_status = IC_Start(IC_INSTANCE_TIM3, IC_CHANNEL_2);
    if (ic_status != IC_OK)
    {
        LOG_ERROR("MAIN", "输入捕获启动失败: %d", ic_status);
        ErrorHandler_Handle(ic_status, "IC");
    }
    
    /* 等待DS3231 32K输出稳定 */
    Delay_ms(500);
    LOG_INFO("MAIN", "DS3231 32K输出已稳定，可以开始测量");
    
    /* ========== 步骤15：主循环 ========== */
    LOG_INFO("MAIN", "初始化完成，开始测量32K频率");
    
    /* 初始化测量结果 */
    memset(&measure_result, 0, sizeof(measure_result));
    
    while (1)
    {
        uint32_t current_time = Delay_GetTick();
        
        /* 每500ms测量一次32K频率和占空比 */
        if (Delay_GetElapsed(last_measure_time, current_time) >= 500)
        {
            last_measure_time = current_time;
            
            /* 使用PWMI模式测量32K频率和占空比 */
            /* 注意：PWMI模式需要IC_POLARITY_BOTH，超时时间根据32kHz信号设置（周期约31.25us） */
            ic_status = IC_MeasurePWM(IC_INSTANCE_TIM3, IC_CHANNEL_2, &measure_result, 100);
            if (ic_status != IC_OK)
            {
                LOG_WARN("IC", "32K频率测量失败: %d", ic_status);
                memset(&measure_result, 0, sizeof(measure_result));
            }
            else
            {
                LOG_INFO("IC", "32K测量结果: 频率=%lu Hz, 周期=%lu us, 脉宽=%lu us, 占空比=%lu%%",
                         measure_result.frequency, measure_result.period, 
                         measure_result.pulse_width, measure_result.duty_cycle);
            }
        }
        
        /* 每200ms更新一次OLED显示 */
        if (Delay_GetElapsed(last_oled_update_time, current_time) >= 200)
        {
            last_oled_update_time = current_time;
            UpdateOLEDDisplay(&measure_result);
        }
        
        Delay_ms(50);
    }
}

/* ========== 辅助函数 ========== */

/**
 * @brief 更新OLED显示
 */
static void UpdateOLEDDisplay(const IC_MeasureResult_t *result)
{
    char buf[17];
    
    OLED_Clear();
    OLED_ShowString(1, 1, "PWM05 PWMI Demo");
    
    /* 显示频率 */
    if (result->frequency > 0)
    {
        if (result->frequency < 1000)
        {
            snprintf(buf, sizeof(buf), "Freq: %lu Hz", result->frequency);
        }
        else if (result->frequency < 10000)
        {
            snprintf(buf, sizeof(buf), "Freq: %luHz", result->frequency);
        }
        else
        {
            snprintf(buf, sizeof(buf), "Freq: %lukHz", result->frequency / 1000);
        }
        OLED_ShowString(2, 1, buf);
        
        /* 显示周期 */
        if (result->period < 1000)
        {
            snprintf(buf, sizeof(buf), "Period: %lu us", result->period);
        }
        else
        {
            snprintf(buf, sizeof(buf), "Period: %lu ms", result->period / 1000);
        }
        OLED_ShowString(3, 1, buf);
        
        /* 显示占空比 */
        snprintf(buf, sizeof(buf), "Duty: %lu%%", result->duty_cycle);
        OLED_ShowString(4, 1, buf);
    }
    else
    {
        OLED_ShowString(2, 1, "Freq: -- Hz");
        OLED_ShowString(3, 1, "Period: --");
        OLED_ShowString(4, 1, "Duty: --");
    }
}

