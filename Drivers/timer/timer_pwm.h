/**
 * @file timer_pwm.h
 * @brief 定时器PWM驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的定时器PWM驱动，支持TIM1/TIM3/TIM4，PWM输出，占空比和频率设置
 */

#ifndef TIMER_PWM_H
#define TIMER_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include <stdint.h>

/**
 * @brief PWM错误码
 */
typedef enum {
    PWM_OK = ERROR_OK,                                    /**< 操作成功 */
    PWM_ERROR_NOT_IMPLEMENTED = ERROR_BASE_TIMER - 99,    /**< 功能未实现（占位空函数） */
    PWM_ERROR_NULL_PTR = ERROR_BASE_TIMER - 1,            /**< 空指针错误 */
    PWM_ERROR_INVALID_PARAM = ERROR_BASE_TIMER - 2,      /**< 参数非法（通用） */
    PWM_ERROR_INVALID_INSTANCE = ERROR_BASE_TIMER - 3,    /**< 无效实例编号 */
    PWM_ERROR_INVALID_CHANNEL = ERROR_BASE_TIMER - 4,     /**< 无效的通道 */
    PWM_ERROR_INVALID_PERIPH = ERROR_BASE_TIMER - 5,      /**< 无效的外设 */
    PWM_ERROR_NOT_INITIALIZED = ERROR_BASE_TIMER - 6,     /**< 未初始化 */
    PWM_ERROR_GPIO_FAILED = ERROR_BASE_TIMER - 7,         /**< GPIO配置失败 */
    PWM_ERROR_FREQ_OUT_OF_RANGE = ERROR_BASE_TIMER - 8,  /**< 频率超出范围 */
    PWM_ERROR_INVALID_RESOLUTION = ERROR_BASE_TIMER - 9,  /**< 无效的分辨率 */
} PWM_Status_t;

/**
 * @brief PWM实例索引（用于多定时器支持）
 */
typedef enum {
    PWM_INSTANCE_TIM1 = 0,   /**< TIM1实例 */
    PWM_INSTANCE_TIM3 = 1,   /**< TIM3实例 */
    PWM_INSTANCE_TIM4 = 2,   /**< TIM4实例 */
    PWM_INSTANCE_MAX         /**< 最大实例数 */
} PWM_Instance_t;

/**
 * @brief PWM通道索引
 */
typedef enum {
    PWM_CHANNEL_1 = 0,   /**< 通道1 */
    PWM_CHANNEL_2 = 1,   /**< 通道2 */
    PWM_CHANNEL_3 = 2,   /**< 通道3 */
    PWM_CHANNEL_4 = 3,   /**< 通道4 */
    PWM_CHANNEL_MAX      /**< 最大通道数 */
} PWM_Channel_t;

/* Include board.h, which includes stm32f10x.h and stm32f10x_tim.h */
#include "board.h"

/**
 * @brief PWM初始化
 * @param[in] instance PWM实例索引（PWM_INSTANCE_TIM1/3/4）
 * @return PWM_Status_t 错误码
 * @note 根据board.h中的配置初始化定时器外设和GPIO引脚
 * @note 初始化后，PWM频率和占空比需要单独配置
 */
PWM_Status_t PWM_Init(PWM_Instance_t instance);

/**
 * @brief PWM反初始化
 * @param[in] instance PWM实例索引
 * @return PWM_Status_t 错误码
 */
PWM_Status_t PWM_Deinit(PWM_Instance_t instance);

/**
 * @brief 设置PWM频率
 * @param[in] instance PWM实例索引
 * @param[in] frequency 频率（Hz），范围：1Hz ~ 72MHz（取决于系统时钟）
 * @return PWM_Status_t 错误码
 * @note 设置频率后，所有通道的频率都会改变
 */
PWM_Status_t PWM_SetFrequency(PWM_Instance_t instance, uint32_t frequency);

/**
 * @brief 设置PWM占空比
 * @param[in] instance PWM实例索引
 * @param[in] channel PWM通道（PWM_CHANNEL_1/2/3/4）
 * @param[in] duty_cycle 占空比（0.0 ~ 100.0，单位：百分比）
 * @return PWM_Status_t 错误码
 */
PWM_Status_t PWM_SetDutyCycle(PWM_Instance_t instance, PWM_Channel_t channel, float duty_cycle);

/**
 * @brief 获取PWM频率
 * @param[in] instance PWM实例索引
 * @param[out] frequency 频率指针（Hz）
 * @return PWM_Status_t 错误码
 */
PWM_Status_t PWM_GetFrequency(PWM_Instance_t instance, uint32_t *frequency);

/**
 * @brief 设置PWM分辨率
 * @param[in] instance PWM实例索引
 * @param[in] resolution 分辨率（PWM_RESOLUTION_8BIT或PWM_RESOLUTION_16BIT）
 * @return PWM_Status_t 错误码
 * @note 设置分辨率时保持当前频率不变（重新计算PSC）
 * @note 8位分辨率：ARR=256，16位分辨率：ARR=65536
 * @note PWM_Resolution_t类型需要在board.h中定义
 */
PWM_Status_t PWM_SetResolution(PWM_Instance_t instance, PWM_Resolution_t resolution);

/**
 * @brief 获取PWM分辨率
 * @param[in] instance PWM实例索引
 * @param[out] resolution 分辨率指针
 * @return PWM_Status_t 错误码
 * @note PWM_Resolution_t类型需要在board.h中定义
 */
PWM_Status_t PWM_GetResolution(PWM_Instance_t instance, PWM_Resolution_t *resolution);

/**
 * @brief 使能PWM通道
 * @param[in] instance PWM实例索引
 * @param[in] channel PWM通道
 * @return PWM_Status_t 错误码
 */
PWM_Status_t PWM_EnableChannel(PWM_Instance_t instance, PWM_Channel_t channel);

/**
 * @brief 禁用PWM通道
 * @param[in] instance PWM实例索引
 * @param[in] channel PWM通道
 * @return PWM_Status_t 错误码
 */
PWM_Status_t PWM_DisableChannel(PWM_Instance_t instance, PWM_Channel_t channel);

/**
 * @brief 检查PWM是否已初始化
 * @param[in] instance PWM实例索引
 * @return uint8_t 1-已初始化，0-未初始化
 */
uint8_t PWM_IsInitialized(PWM_Instance_t instance);

/**
 * @brief 获取定时器外设指针
 * @param[in] instance PWM实例索引
 * @return TIM_TypeDef* 定时器外设指针，失败返回NULL
 */
TIM_TypeDef* PWM_GetPeriph(PWM_Instance_t instance);

/* ========== 互补输出和死区时间功能（仅TIM1/TIM8） ========== */

/**
 * @brief 使能PWM互补输出
 * @param[in] instance PWM实例索引（仅支持PWM_INSTANCE_TIM1）
 * @param[in] channel PWM通道（PWM_CHANNEL_1/2/3/4）
 * @return PWM_Status_t 错误码
 * @note 仅TIM1/TIM8支持互补输出
 */
PWM_Status_t PWM_EnableComplementary(PWM_Instance_t instance, PWM_Channel_t channel);

/**
 * @brief 禁用PWM互补输出
 * @param[in] instance PWM实例索引（仅支持PWM_INSTANCE_TIM1）
 * @param[in] channel PWM通道
 * @return PWM_Status_t 错误码
 */
PWM_Status_t PWM_DisableComplementary(PWM_Instance_t instance, PWM_Channel_t channel);

/**
 * @brief 设置PWM死区时间
 * @param[in] instance PWM实例索引（仅支持PWM_INSTANCE_TIM1）
 * @param[in] dead_time 死区时间（ns），范围：0 ~ 65535ns
 * @return PWM_Status_t 错误码
 * @note 仅TIM1/TIM8支持死区时间配置
 * @note 死区时间 = (DTG[7:0] + 1) * tDTS，其中tDTS = 1 / 定时器时钟频率
 */
PWM_Status_t PWM_SetDeadTime(PWM_Instance_t instance, uint16_t dead_time_ns);

/**
 * @brief 使能PWM主输出（MOE）
 * @param[in] instance PWM实例索引（仅支持PWM_INSTANCE_TIM1）
 * @return PWM_Status_t 错误码
 * @note 仅TIM1/TIM8支持主输出使能
 */
PWM_Status_t PWM_EnableMainOutput(PWM_Instance_t instance);

/**
 * @brief 禁用PWM主输出（MOE）
 * @param[in] instance PWM实例索引（仅支持PWM_INSTANCE_TIM1）
 * @return PWM_Status_t 错误码
 */
PWM_Status_t PWM_DisableMainOutput(PWM_Instance_t instance);

/* ========== 刹车功能（仅TIM1/TIM8） ========== */

/**
 * @brief PWM刹车源类型
 */
typedef enum {
    PWM_BRAKE_SOURCE_BKIN = 0,  /**< 刹车输入引脚（BKIN） */
    PWM_BRAKE_SOURCE_LOCK = 1,  /**< 时钟失效事件（LOCK） */
} PWM_BrakeSource_t;

/**
 * @brief PWM刹车极性
 */
typedef enum {
    PWM_BRAKE_POLARITY_LOW = 0,   /**< 低电平有效 */
    PWM_BRAKE_POLARITY_HIGH = 1,  /**< 高电平有效 */
} PWM_BrakePolarity_t;

/**
 * @brief 使能PWM刹车功能
 * @param[in] instance PWM实例索引（仅支持PWM_INSTANCE_TIM1）
 * @param[in] source 刹车源
 * @param[in] polarity 刹车极性
 * @return PWM_Status_t 错误码
 * @note 仅TIM1/TIM8支持刹车功能
 */
PWM_Status_t PWM_EnableBrake(PWM_Instance_t instance, PWM_BrakeSource_t source, PWM_BrakePolarity_t polarity);

/**
 * @brief 禁用PWM刹车功能
 * @param[in] instance PWM实例索引（仅支持PWM_INSTANCE_TIM1）
 * @return PWM_Status_t 错误码
 */
PWM_Status_t PWM_DisableBrake(PWM_Instance_t instance);

/**
 * @brief PWM对齐模式枚举
 */
typedef enum {
    PWM_ALIGN_EDGE = 0,        /**< 边沿对齐模式 */
    PWM_ALIGN_CENTER_1 = 1,    /**< 中心对齐模式1（向上计数时匹配） */
    PWM_ALIGN_CENTER_2 = 2,    /**< 中心对齐模式2（向下计数时匹配） */
    PWM_ALIGN_CENTER_3 = 3,    /**< 中心对齐模式3（向上/向下计数时都匹配） */
} PWM_AlignMode_t;

/**
 * @brief 设置PWM对齐模式
 * @param[in] instance PWM实例索引
 * @param[in] align_mode 对齐模式
 * @return PWM_Status_t 错误码
 * @note 中心对齐模式用于某些电机控制算法
 */
PWM_Status_t PWM_SetAlignMode(PWM_Instance_t instance, PWM_AlignMode_t align_mode);

#ifdef __cplusplus
}
#endif

#endif /* TIMER_PWM_H */

