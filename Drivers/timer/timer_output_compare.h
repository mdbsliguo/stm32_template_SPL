/**
 * @file timer_output_compare.h
 * @brief 定时器输出比较模块
 * @details 提供定时器输出比较功能，用于单脉冲输出、定时翻转输出等
 */

#ifndef TIMER_OUTPUT_COMPARE_H
#define TIMER_OUTPUT_COMPARE_H

#include "error_code.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 输出比较状态枚举
 */
typedef enum {
    OC_OK = ERROR_OK,                                    /**< 操作成功 */
    OC_ERROR_NOT_IMPLEMENTED = ERROR_BASE_TIMER - 99,    /**< 功能未实现（占位空函数） */
    OC_ERROR_NULL_PTR = ERROR_BASE_TIMER - 30,           /**< 空指针错误 */
    OC_ERROR_INVALID_PARAM = ERROR_BASE_TIMER - 31,     /**< 参数错误（通用） */
    OC_ERROR_INVALID_INSTANCE = ERROR_BASE_TIMER - 32,   /**< 无效实例编号 */
    OC_ERROR_INVALID_CHANNEL = ERROR_BASE_TIMER - 33,     /**< 无效的通道 */
    OC_ERROR_NOT_INITIALIZED = ERROR_BASE_TIMER - 34,    /**< 未初始化 */
    OC_ERROR_INVALID_PERIPH = ERROR_BASE_TIMER - 35,      /**< 无效的外设 */
    OC_ERROR_GPIO_FAILED = ERROR_BASE_TIMER - 36,        /**< GPIO配置失败 */
    OC_ERROR_ALREADY_INITIALIZED = ERROR_BASE_TIMER - 37, /**< 已初始化 */
} OC_Status_t;

/**
 * @brief 输出比较实例索引
 */
typedef enum {
    OC_INSTANCE_TIM1 = 0,   /**< TIM1实例 */
    OC_INSTANCE_TIM2 = 1,   /**< TIM2实例 */
    OC_INSTANCE_TIM3 = 2,   /**< TIM3实例 */
    OC_INSTANCE_TIM4 = 3,   /**< TIM4实例 */
    OC_INSTANCE_MAX         /**< 最大实例数 */
} OC_Instance_t;

/**
 * @brief 输出比较通道索引
 */
typedef enum {
    OC_CHANNEL_1 = 0,   /**< 通道1 */
    OC_CHANNEL_2 = 1,   /**< 通道2 */
    OC_CHANNEL_3 = 2,   /**< 通道3 */
    OC_CHANNEL_4 = 3,   /**< 通道4 */
    OC_CHANNEL_MAX      /**< 最大通道数 */
} OC_Channel_t;

/**
 * @brief 输出比较模式枚举
 */
typedef enum {
    OC_MODE_TIMING = 0,     /**< 定时模式（不输出，仅用于定时） */
    OC_MODE_ACTIVE = 1,     /**< 激活模式（匹配时输出高电平） */
    OC_MODE_INACTIVE = 2,   /**< 非激活模式（匹配时输出低电平） */
    OC_MODE_TOGGLE = 3,     /**< 翻转模式（匹配时翻转输出） */
} OC_Mode_t;

/* Include board.h, which includes stm32f10x.h and stm32f10x_tim.h */
#include "board.h"

/**
 * @brief 输出比较初始化
 * @param[in] instance 输出比较实例索引
 * @param[in] channel 输出比较通道
 * @param[in] mode 输出比较模式
 * @param[in] period 定时器周期值
 * @param[in] compare_value 比较值
 * @return OC_Status_t 状态码
 */
OC_Status_t OC_Init(OC_Instance_t instance, OC_Channel_t channel, OC_Mode_t mode, uint16_t period, uint16_t compare_value);

/**
 * @brief 输出比较反初始化
 * @param[in] instance 输出比较实例索引
 * @param[in] channel 输出比较通道
 * @return OC_Status_t 状态码
 */
OC_Status_t OC_Deinit(OC_Instance_t instance, OC_Channel_t channel);

/**
 * @brief 设置比较值
 * @param[in] instance 输出比较实例索引
 * @param[in] channel 输出比较通道
 * @param[in] compare_value 比较值
 * @return OC_Status_t 状态码
 */
OC_Status_t OC_SetCompareValue(OC_Instance_t instance, OC_Channel_t channel, uint16_t compare_value);

/**
 * @brief 获取比较值
 * @param[in] instance 输出比较实例索引
 * @param[in] channel 输出比较通道
 * @param[out] compare_value 比较值指针
 * @return OC_Status_t 状态码
 */
OC_Status_t OC_GetCompareValue(OC_Instance_t instance, OC_Channel_t channel, uint16_t *compare_value);

/**
 * @brief 启动输出比较
 * @param[in] instance 输出比较实例索引
 * @param[in] channel 输出比较通道
 * @return OC_Status_t 状态码
 */
OC_Status_t OC_Start(OC_Instance_t instance, OC_Channel_t channel);

/**
 * @brief 停止输出比较
 * @param[in] instance 输出比较实例索引
 * @param[in] channel 输出比较通道
 * @return OC_Status_t 状态码
 */
OC_Status_t OC_Stop(OC_Instance_t instance, OC_Channel_t channel);

/**
 * @brief 生成单脉冲（单次输出）
 * @param[in] instance 输出比较实例索引
 * @param[in] channel 输出比较通道
 * @param[in] pulse_width 脉冲宽度（定时器计数值）
 * @return OC_Status_t 状态码
 * @note 配置为单脉冲模式，输出一个脉冲后自动停止
 */
OC_Status_t OC_GenerateSinglePulse(OC_Instance_t instance, OC_Channel_t channel, uint16_t pulse_width);

/**
 * @brief 检查输出比较是否已初始化
 * @param[in] instance 输出比较实例索引
 * @param[in] channel 输出比较通道
 * @return uint8_t 1=已初始化，0=未初始化
 */
uint8_t OC_IsInitialized(OC_Instance_t instance, OC_Channel_t channel);

/* ========== 高级功能 ========== */

/**
 * @brief 使能输出比较预装载
 * @param[in] instance 输出比较实例索引
 * @param[in] channel 输出比较通道
 * @return OC_Status_t 状态码
 * @note 预装载使能后，比较值在更新事件时生效，确保同步切换
 */
OC_Status_t OC_EnablePreload(OC_Instance_t instance, OC_Channel_t channel);

/**
 * @brief 禁用输出比较预装载
 * @param[in] instance 输出比较实例索引
 * @param[in] channel 输出比较通道
 * @return OC_Status_t 状态码
 */
OC_Status_t OC_DisablePreload(OC_Instance_t instance, OC_Channel_t channel);

/**
 * @brief 清除输出比较输出（OCxClear）
 * @param[in] instance 输出比较实例索引
 * @param[in] channel 输出比较通道
 * @return OC_Status_t 状态码
 * @note 通过软件强制清除输出，用于紧急停止等场景
 */
OC_Status_t OC_ClearOutput(OC_Instance_t instance, OC_Channel_t channel);

/**
 * @brief 强制输出比较输出为高电平
 * @param[in] instance 输出比较实例索引
 * @param[in] channel 输出比较通道
 * @return OC_Status_t 状态码
 */
OC_Status_t OC_ForceOutputHigh(OC_Instance_t instance, OC_Channel_t channel);

/**
 * @brief 强制输出比较输出为低电平
 * @param[in] instance 输出比较实例索引
 * @param[in] channel 输出比较通道
 * @return OC_Status_t 状态码
 */
OC_Status_t OC_ForceOutputLow(OC_Instance_t instance, OC_Channel_t channel);

#ifdef __cplusplus
}
#endif

#endif /* TIMER_OUTPUT_COMPARE_H */
