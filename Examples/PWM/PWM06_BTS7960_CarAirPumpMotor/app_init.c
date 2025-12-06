/**
 * @file app_init.c
 * @brief 应用初始化函数实现
 * @example Examples/PWM/PWM06_BTS7960_CarAirPumpMotor/app_init.c
 */

#include "app_init.h"
#include "system_init.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "error_code.h"
#include "delay.h"
#include "led.h"
#include "oled_ssd1306.h"
#include "i2c_sw.h"
#include "bts7960.h"
#include "timer_pwm.h"
#include "timer_encoder.h"
#include "board.h"
#include "config.h"
#include <string.h>
#include <stdint.h>

/* ==================== 宏定义 ==================== */

#define PWM_FREQ_FIXED      20000        /**< PWM频率（Hz），固定20kHz */
#define PWM_ARR_FIXED       1023         /**< PWM ARR值（分辨率），固定1023（10位分辨率，1024级） */

/* ==================== 函数实现 ==================== */

/**
 * @brief 初始化系统模块（UART、Debug、Log）
 */
void App_Init_System(void)
{
    UART_Status_t uart_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    
    /* 系统初始化 */
    System_Init();
    
    /* UART初始化 */
    uart_status = UART_Init(UART_INSTANCE_1);
    if (uart_status != UART_OK) {
        while(1) { Delay_ms(1000); }
    }
    Delay_ms(100);
    
    /* Debug模块初始化 */
    debug_status = Debug_Init(DEBUG_MODE_UART, 115200);
    if (debug_status != 0) {
        while(1) { Delay_ms(1000); }
    }
    Delay_ms(100);
    
    /* Log模块初始化 */
    memset(&log_config, 0, sizeof(log_config));
    log_config.level = (log_level_t)CONFIG_LOG_LEVEL;
    log_config.enable_timestamp = CONFIG_LOG_TIMESTAMP_EN;
    log_config.enable_module = CONFIG_LOG_MODULE_EN;
    log_config.enable_color = CONFIG_LOG_COLOR_EN;
    
    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK) {
        ErrorHandler_Handle(log_status, "LOG");
    }
    
    /* 输出初始化信息 */
    LOG_INFO("MAIN", "=== PWM06 BTS7960 CarAirPumpMotor ===");
    LOG_INFO("MAIN", "系统初始化完成");
    LOG_INFO("MAIN", "UART1已初始化: PA9(TX), PA10(RX), 115200（标准配置）");
    LOG_INFO("MAIN", "Debug模块已初始化: UART模式");
    LOG_INFO("MAIN", "Log模块已初始化");
}

/**
 * @brief 初始化硬件模块（LED、OLED、I2C）
 */
void App_Init_Hardware(void)
{
    LED_Status_t led_status;
    SoftI2C_Status_t i2c_status;
    OLED_Status_t oled_status;
    
    /* LED初始化 */
    led_status = LED_Init();
    if (led_status != LED_OK) {
        while(1) { Delay_ms(1000); }
    }
    
    /* 软件I2C初始化 */
    i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_1);
    if (i2c_status != SOFT_I2C_OK) {
        while(1) { Delay_ms(1000); }
    }
    
    /* OLED初始化 */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK) {
        while(1) { Delay_ms(1000); }
    } else {
        OLED_Clear();
        OLED_ShowString(1, 1, "PWM06 Init");
        OLED_ShowString(2, 1, "System Ready");
    }
}

/**
 * @brief 初始化PWM模块（TIM1用于RPWM）
 */
void App_Init_PWM(void)
{
    PWM_Status_t pwm_status;
    TIM_TypeDef *tim1_periph;
    
    /* 初始化TIM1 */
    pwm_status = PWM_Init(PWM_INSTANCE_TIM1);
    if (pwm_status != PWM_OK) {
        OLED_ShowString(3, 1, "PWM1 Init Failed");
        while(1) { Delay_ms(1000); }
    }
    
    /* 设置PWM频率 */
    pwm_status = PWM_SetFrequency(PWM_INSTANCE_TIM1, PWM_FREQ_FIXED);
    if (pwm_status != PWM_OK) {
        OLED_ShowString(3, 1, "PWM1 Freq Failed");
        while(1) { Delay_ms(1000); }
    }
    
    /* 设置PWM分辨率 */
    tim1_periph = PWM_GetPeriph(PWM_INSTANCE_TIM1);
    if (tim1_periph != NULL) {
        /* 获取定时器时钟频率 */
        SystemCoreClockUpdate();
        uint32_t apb2_prescaler = (RCC->CFGR >> 11) & 0x07;
        uint32_t apb2_clk;
        if (apb2_prescaler < 4) {
            apb2_clk = SystemCoreClock;
        } else {
            uint8_t presc_table[] = {0, 0, 0, 0, 1, 2, 3, 4};
            apb2_clk = SystemCoreClock >> presc_table[apb2_prescaler];
        }
        uint32_t tim_clk = (apb2_prescaler >= 4) ? (apb2_clk * 2) : apb2_clk;
        
        /* 计算PSC值 */
        uint32_t psc = (tim_clk / (PWM_ARR_FIXED * PWM_FREQ_FIXED)) - 1;
        if (psc > 65535) psc = 65535;
        
        /* 配置TIM1 */
        TIM_Cmd(tim1_periph, DISABLE);
        TIM_SetAutoreload(tim1_periph, PWM_ARR_FIXED);
        TIM_PrescalerConfig(tim1_periph, psc, TIM_PSCReloadMode_Immediate);
        TIM_SetCompare1(tim1_periph, 0);
        TIM_Cmd(tim1_periph, ENABLE);
        
        /* 配置死区时间 */
        pwm_status = PWM_SetDeadTime(PWM_INSTANCE_TIM1, 2000);
        if (pwm_status != PWM_OK) {
            OLED_ShowString(3, 1, "DeadTime Failed");
        }
        
        /* 使能TIM1主输出 */
        pwm_status = PWM_EnableMainOutput(PWM_INSTANCE_TIM1);
        if (pwm_status != PWM_OK) {
            OLED_ShowString(3, 1, "MOE Failed");
            while(1) { Delay_ms(1000); }
        }
    }
}

/**
 * @brief 初始化BTS7960电机驱动
 */
void App_Init_BTS7960(void)
{
    BTS7960_Status_t bts7960_status;
    
    /* BTS7960初始化 */
    bts7960_status = BTS7960_Init(BTS7960_INSTANCE_1);
    if (bts7960_status != BTS7960_OK) {
        OLED_ShowString(3, 1, "BTS7960 Init Fail");
        while(1) { Delay_ms(1000); }
    }
    
    /* BTS7960使能 */
    bts7960_status = BTS7960_Enable(BTS7960_INSTANCE_1);
    if (bts7960_status != BTS7960_OK) {
        OLED_ShowString(3, 1, "BTS7960 Enable Fail");
        while(1) { Delay_ms(1000); }
    }
    
    /* 设置初始方向为停止 */
    bts7960_status = BTS7960_SetDirection(BTS7960_INSTANCE_1, BTS7960_DIR_STOP);
    if (bts7960_status != BTS7960_OK) {
        OLED_ShowString(3, 1, "BTS7960 Dir Fail");
    }
    
    /* 设置初始速度为0% */
    bts7960_status = BTS7960_SetSpeed(BTS7960_INSTANCE_1, 0.0f);
    if (bts7960_status != BTS7960_OK) {
        LOG_WARN("MAIN", "设置初始速度失败: %d", bts7960_status);
    }
}

/**
 * @brief 初始化编码器接口
 * @param encoder_last_count 编码器初始计数值指针（输出参数）
 * @return 0表示成功，非0表示失败
 */
int App_Init_Encoder(int32_t *encoder_last_count)
{
    ENCODER_Status_t encoder_status;
    
    if (encoder_last_count == NULL) {
        return -1;
    }
    
    /* 配置TIM3部分重映射 */
    ENCODER_SetTIM3Remap(true, false);
    
    /* 初始化编码器接口 */
    encoder_status = ENCODER_Init(ENCODER_INSTANCE_TIM3, ENCODER_MODE_TI12);
    if (encoder_status != ENCODER_OK) {
        OLED_ShowString(3, 1, "Encoder Init Fail");
        LOG_ERROR("MAIN", "编码器接口初始化失败: %d", encoder_status);
        ErrorHandler_Handle(encoder_status, "ENCODER");
        return -1;
    }
    
    /* 启动编码器 */
    encoder_status = ENCODER_Start(ENCODER_INSTANCE_TIM3);
    if (encoder_status != ENCODER_OK) {
        OLED_ShowString(3, 1, "Encoder Start Fail");
        LOG_ERROR("MAIN", "编码器启动失败: %d", encoder_status);
        while(1) { Delay_ms(1000); }
    }
    
    /* 清零计数器 */
    encoder_status = ENCODER_ClearCount(ENCODER_INSTANCE_TIM3);
    if (encoder_status != ENCODER_OK) {
        LOG_WARN("MAIN", "编码器清零失败: %d", encoder_status);
    }
    
    /* 初始化编码器计数值 */
    encoder_status = ENCODER_ReadCount(ENCODER_INSTANCE_TIM3, encoder_last_count);
    if (encoder_status != ENCODER_OK) {
        LOG_WARN("MAIN", "读取编码器初始值失败: %d", encoder_status);
        *encoder_last_count = 0;
    }
    
    LOG_INFO("MAIN", "编码器接口已初始化: TIM3，PB4/PB5（部分重映射），4倍频模式，轮询方式");
    
    return 0;
}
