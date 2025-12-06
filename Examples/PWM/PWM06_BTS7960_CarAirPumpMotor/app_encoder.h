/**
 * @file app_encoder.h
 * @brief 编码器处理函数声明
 * @example Examples/PWM/PWM06_BTS7960_CarAirPumpMotor/app_encoder.h
 */

#ifndef APP_ENCODER_H
#define APP_ENCODER_H

#include <stdint.h>

/* ==================== 函数声明 ==================== */

/**
 * @brief 处理编码器输入（轮询方式）
 * @param encoder_last_count 编码器上次计数值指针（输入输出参数）
 * @param encoder_accumulated 编码器累积值指针（输入输出参数）
 * @param update_display 显示更新标志指针（输出参数，如果编码器有变化则设置为1）
 * @param update_parameter_callback 参数更新回调函数指针
 * @return 0表示成功，非0表示失败
 */
int App_Process_Encoder(int32_t *encoder_last_count, 
                        int32_t *encoder_accumulated, 
                        uint8_t *update_display,
                        void (*update_parameter_callback)(int32_t delta));

#endif /* APP_ENCODER_H */
