/**
 * @file timer_input_capture.h
 * @brief 定时器输入捕获模块
 * @details 提供定时器输入捕获功能，用于测量外部信号的频率、占空比、脉冲宽度等
 */

#ifndef TIMER_INPUT_CAPTURE_H
#define TIMER_INPUT_CAPTURE_H

#include "error_code.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 输入捕获状态枚举
 */
typedef enum {
    IC_OK = ERROR_OK,                                    /**< 操作成功 */
    IC_ERROR_NOT_IMPLEMENTED = ERROR_BASE_TIMER - 99,    /**< 功能未实现（占位空函数） */
    IC_ERROR_NULL_PTR = ERROR_BASE_TIMER - 10,           /**< 空指针错误 */
    IC_ERROR_INVALID_PARAM = ERROR_BASE_TIMER - 11,     /**< 参数错误（通用） */
    IC_ERROR_INVALID_INSTANCE = ERROR_BASE_TIMER - 12,   /**< 无效实例编号 */
    IC_ERROR_INVALID_CHANNEL = ERROR_BASE_TIMER - 13,     /**< 无效的通道 */
    IC_ERROR_NOT_INITIALIZED = ERROR_BASE_TIMER - 14,    /**< 未初始化 */
    IC_ERROR_INVALID_PERIPH = ERROR_BASE_TIMER - 15,      /**< 无效的外设 */
    IC_ERROR_GPIO_FAILED = ERROR_BASE_TIMER - 16,        /**< GPIO配置失败 */
    IC_ERROR_TIMEOUT = ERROR_BASE_TIMER - 17,            /**< 操作超时 */
    IC_ERROR_ALREADY_INITIALIZED = ERROR_BASE_TIMER - 18, /**< 已初始化 */
} IC_Status_t;

/**
 * @brief 输入捕获实例索引
 */
typedef enum {
    IC_INSTANCE_TIM1 = 0,   /**< TIM1实例 */
    IC_INSTANCE_TIM2 = 1,   /**< TIM2实例 */
    IC_INSTANCE_TIM3 = 2,   /**< TIM3实例 */
    IC_INSTANCE_TIM4 = 3,   /**< TIM4实例 */
    IC_INSTANCE_MAX         /**< 最大实例数 */
} IC_Instance_t;

/**
 * @brief 输入捕获通道索引
 */
typedef enum {
    IC_CHANNEL_1 = 0,   /**< 通道1 */
    IC_CHANNEL_2 = 1,   /**< 通道2 */
    IC_CHANNEL_3 = 2,   /**< 通道3 */
    IC_CHANNEL_4 = 3,   /**< 通道4 */
    IC_CHANNEL_MAX      /**< 最大通道数 */
} IC_Channel_t;

/**
 * @brief 输入捕获极性枚举
 */
typedef enum {
    IC_POLARITY_RISING = 0,     /**< 上升沿捕获 */
    IC_POLARITY_FALLING = 1,    /**< 下降沿捕获 */
    IC_POLARITY_BOTH = 2,       /**< 双边沿捕获（PWM模式） */
} IC_Polarity_t;

/**
 * @brief 输入捕获测量结果结构体
 */
typedef struct {
    uint32_t frequency;         /**< 频率（Hz） */
    uint32_t period;            /**< 周期（微秒） */
    uint32_t pulse_width;       /**< 脉冲宽度（微秒） */
    uint32_t duty_cycle;        /**< 占空比（百分比，0-100） */
} IC_MeasureResult_t;

/* Include board.h, which includes stm32f10x.h and stm32f10x_tim.h */
#include "board.h"

/**
 * @brief 输入捕获初始化
 * @param[in] instance 输入捕获实例索引
 * @param[in] channel 输入捕获通道
 * @param[in] polarity 捕获极性
 * @return IC_Status_t 状态码
 */
IC_Status_t IC_Init(IC_Instance_t instance, IC_Channel_t channel, IC_Polarity_t polarity);

/**
 * @brief 输入捕获反初始化
 * @param[in] instance 输入捕获实例索引
 * @param[in] channel 输入捕获通道
 * @return IC_Status_t 状态码
 */
IC_Status_t IC_Deinit(IC_Instance_t instance, IC_Channel_t channel);

/**
 * @brief 启动输入捕获
 * @param[in] instance 输入捕获实例索引
 * @param[in] channel 输入捕获通道
 * @return IC_Status_t 状态码
 */
IC_Status_t IC_Start(IC_Instance_t instance, IC_Channel_t channel);

/**
 * @brief 停止输入捕获
 * @param[in] instance 输入捕获实例索引
 * @param[in] channel 输入捕获通道
 * @return IC_Status_t 状态码
 */
IC_Status_t IC_Stop(IC_Instance_t instance, IC_Channel_t channel);

/**
 * @brief 读取捕获值（原始计数器值）
 * @param[in] instance 输入捕获实例索引
 * @param[in] channel 输入捕获通道
 * @param[out] value 捕获值
 * @return IC_Status_t 状态码
 */
IC_Status_t IC_ReadValue(IC_Instance_t instance, IC_Channel_t channel, uint32_t *value);

/**
 * @brief 测量频率（单次测量）
 * @param[in] instance 输入捕获实例索引
 * @param[in] channel 输入捕获通道
 * @param[out] frequency 频率值（Hz）
 * @param[in] timeout_ms 超时时间（毫秒）
 * @return IC_Status_t 状态码
 */
IC_Status_t IC_MeasureFrequency(IC_Instance_t instance, IC_Channel_t channel, uint32_t *frequency, uint32_t timeout_ms);

/**
 * @brief 测量PWM信号（频率、占空比）
 * @param[in] instance 输入捕获实例索引
 * @param[in] channel 输入捕获通道
 * @param[out] result 测量结果结构体指针
 * @param[in] timeout_ms 超时时间（毫秒）
 * @return IC_Status_t 状态码
 * @note 需要配置为PWM输入模式（双边沿捕获）
 */
IC_Status_t IC_MeasurePWM(IC_Instance_t instance, IC_Channel_t channel, IC_MeasureResult_t *result, uint32_t timeout_ms);

/**
 * @brief 测量脉冲宽度（单次测量）
 * @param[in] instance 输入捕获实例索引
 * @param[in] channel 输入捕获通道
 * @param[out] pulse_width_us 脉冲宽度（微秒）
 * @param[in] timeout_ms 超时时间（毫秒）
 * @return IC_Status_t 状态码
 */
IC_Status_t IC_MeasurePulseWidth(IC_Instance_t instance, IC_Channel_t channel, uint32_t *pulse_width_us, uint32_t timeout_ms);

/**
 * @brief 检查输入捕获是否已初始化
 * @param[in] instance 输入捕获实例索引
 * @param[in] channel 输入捕获通道
 * @return uint8_t 1=已初始化，0=未初始化
 */
uint8_t IC_IsInitialized(IC_Instance_t instance, IC_Channel_t channel);

/* ========== 中断模式功能 ========== */

/**
 * @brief 输入捕获中断类型枚举
 */
typedef enum {
    IC_IT_CAPTURE = 0,  /**< 捕获中断 */
    IC_IT_OVERFLOW = 1, /**< 溢出中断 */
} IC_IT_t;

/**
 * @brief 输入捕获中断回调函数类型
 * @param[in] instance 输入捕获实例索引
 * @param[in] channel 输入捕获通道
 * @param[in] it_type 中断类型
 * @param[in] capture_value 捕获值（仅捕获中断有效）
 * @param[in] user_data 用户数据指针
 */
typedef void (*IC_IT_Callback_t)(IC_Instance_t instance, IC_Channel_t channel, 
                                 IC_IT_t it_type, uint32_t capture_value, void *user_data);

/**
 * @brief 使能输入捕获中断
 * @param[in] instance 输入捕获实例索引
 * @param[in] channel 输入捕获通道
 * @param[in] it_type 中断类型
 * @return IC_Status_t 状态码
 */
IC_Status_t IC_EnableIT(IC_Instance_t instance, IC_Channel_t channel, IC_IT_t it_type);

/**
 * @brief 禁用输入捕获中断
 * @param[in] instance 输入捕获实例索引
 * @param[in] channel 输入捕获通道
 * @param[in] it_type 中断类型
 * @return IC_Status_t 状态码
 */
IC_Status_t IC_DisableIT(IC_Instance_t instance, IC_Channel_t channel, IC_IT_t it_type);

/**
 * @brief 设置输入捕获中断回调函数
 * @param[in] instance 输入捕获实例索引
 * @param[in] channel 输入捕获通道
 * @param[in] it_type 中断类型
 * @param[in] callback 回调函数指针，NULL表示禁用回调
 * @param[in] user_data 用户数据指针
 * @return IC_Status_t 状态码
 */
IC_Status_t IC_SetITCallback(IC_Instance_t instance, IC_Channel_t channel, IC_IT_t it_type,
                             IC_IT_Callback_t callback, void *user_data);

/**
 * @brief 输入捕获中断服务函数（应在中断服务程序中调用）
 * @param[in] instance 输入捕获实例索引
 */
void IC_IRQHandler(IC_Instance_t instance);

/**
 * @brief 配置输入捕获滤波器
 * @param[in] instance 输入捕获实例索引
 * @param[in] channel 输入捕获通道
 * @param[in] filter_value 滤波器值（0-15）
 * @return IC_Status_t 状态码
 * @note 滤波器值越大，抗干扰能力越强，但响应速度越慢
 */
IC_Status_t IC_SetFilter(IC_Instance_t instance, IC_Channel_t channel, uint8_t filter_value);

/**
 * @brief 配置输入捕获预分频器
 * @param[in] instance 输入捕获实例索引
 * @param[in] channel 输入捕获通道
 * @param[in] prescaler 预分频器值（1/2/4/8）
 * @return IC_Status_t 状态码
 * @note 预分频器用于降低输入信号的采样频率，提高高频信号测量精度
 */
IC_Status_t IC_SetPrescaler(IC_Instance_t instance, IC_Channel_t channel, uint8_t prescaler);

#ifdef __cplusplus
}
#endif

#endif /* TIMER_INPUT_CAPTURE_H */

