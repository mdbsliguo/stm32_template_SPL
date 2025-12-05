/**
 * @file app_encoder.c
 * @brief 编码器处理函数实现
 * @example Examples/PWM/PWM06_BTS7960_CarAirPumpMotor/app_encoder.c
 */

#include "app_encoder.h"
#include "timer_encoder.h"
#include <stdint.h>
#include <stddef.h>

/* ==================== 函数实现 ==================== */

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
                        void (*update_parameter_callback)(int32_t delta))
{
    int32_t encoder_count = 0;
    ENCODER_Status_t encoder_status;
    
    if (encoder_last_count == NULL || encoder_accumulated == NULL || 
        update_display == NULL || update_parameter_callback == NULL) {
        return -1;
    }
    
    encoder_status = ENCODER_ReadCount(ENCODER_INSTANCE_TIM3, &encoder_count);
    
    if (encoder_status == ENCODER_OK) {
        /* 计算变化量 */
        int32_t encoder_delta = encoder_count - *encoder_last_count;
        
        /* 处理16位计数器溢出/下溢 */
        if (encoder_delta > 32767) {
            encoder_delta = encoder_delta - 65536;
        } else if (encoder_delta < -32767) {
            encoder_delta = encoder_delta + 65536;
        }
        
        /* 累积变化量 */
        if (encoder_delta != 0) {
            *encoder_last_count = encoder_count;
            *encoder_accumulated += encoder_delta;
            
            /* 累积到4个计数（1个物理步进）才更新一次 */
            if (*encoder_accumulated >= 4 || *encoder_accumulated <= -4) {
                int32_t steps = *encoder_accumulated / 4;
                *encoder_accumulated = *encoder_accumulated % 4;
                
                if (steps != 0) {
                    update_parameter_callback(steps);
                    *update_display = 1;
                }
            }
        }
    }
    
    return 0;
}

