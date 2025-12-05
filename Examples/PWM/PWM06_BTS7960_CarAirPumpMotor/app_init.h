/**
 * @file app_init.h
 * @brief 应用初始化函数声明
 * @example Examples/PWM/PWM06_BTS7960_CarAirPumpMotor/app_init.h
 */

#ifndef APP_INIT_H
#define APP_INIT_H

#include <stdint.h>

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化系统模块（UART、Debug、Log）
 */
void App_Init_System(void);

/**
 * @brief 初始化硬件模块（LED、OLED、I2C）
 */
void App_Init_Hardware(void);

/**
 * @brief 初始化PWM模块（TIM1用于RPWM）
 */
void App_Init_PWM(void);

/**
 * @brief 初始化BTS7960电机驱动
 */
void App_Init_BTS7960(void);

/**
 * @brief 初始化编码器接口
 * @param encoder_last_count 编码器初始计数值指针（输出参数）
 * @return 0表示成功，非0表示失败
 */
int App_Init_Encoder(int32_t *encoder_last_count);

#endif /* APP_INIT_H */


