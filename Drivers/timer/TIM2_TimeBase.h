/**
 * @file TIM2_TimeBase.h
 * @brief TIM2时间基准模块接口（时间基准，动态降频自适应）
 * @version 2.0.0
 * @date 2024-01-01
 * @note 使用TIM2外设定时器，确保调频时1ms中断始终准确
 * @note delay模块基于此定时器，确保频率变化时1秒永远是1秒
 */

#ifndef TIM2_TIMEBASE_H
#define TIM2_TIMEBASE_H

#include "error_code.h"
#include <stdint.h>
#include <stdbool.h>

/* 任务tick计数器（TIM2中断中递增，代表真实时间，毫秒） */
extern volatile uint32_t g_task_tick;

/**
 * @brief TIM2时间基准错误码
 * @note 历史遗留模块，待重构（当前返回类型已修复为error_code_t）
 */
typedef enum {
    TIM2_TIMEBASE_OK = ERROR_OK,                                    /**< 操作成功 */
    TIM2_TIMEBASE_ERROR_NOT_INITIALIZED = ERROR_BASE_BASE_TIMER - 1, /**< 未初始化 */
    TIM2_TIMEBASE_ERROR_ALREADY_INITIALIZED = ERROR_BASE_BASE_TIMER - 2, /**< 已初始化 */
    TIM2_TIMEBASE_ERROR_CALC_FAILED = ERROR_BASE_BASE_TIMER - 3,   /**< 参数计算失败 */
} TIM2_TimeBase_Status_t;

/**
 * @brief 初始化TIM2时间基准（配置1ms中断，作为时间基准）
 * @return error_code_t 错误码（TIM2_TIMEBASE_OK表示成功）
 * @note 使用TIM2外设定时器，配置为1ms中断一次
 * @note g_task_tick在TIM2_IRQHandler中递增，代表真实时间（毫秒）
 * @note 频率切换时调用TIM2_TimeBase_Reconfig()重新配置，保持1ms中断间隔不变
 * @note 历史遗留模块，待重构
 */
error_code_t TIM2_TimeBase_Init(void);

/**
 * @brief 重新配置TIM2时间基准（频率切换时调用，保持1ms中断间隔不变）
 * @param[in] new_freq 新的系统频率（Hz）
 * @return error_code_t 错误码（TIM2_TIMEBASE_OK表示成功）
 * @note 频率切换时，重新计算TIM2的PSC和ARR，保持1ms中断间隔不变
 * @note 这样g_task_tick始终代表真实时间（毫秒），无论频率如何变化
 * @note 1秒永远是1秒，无论频率如何变化
 * @note 历史遗留模块，待重构
 */
error_code_t TIM2_TimeBase_Reconfig(uint32_t new_freq);

/**
 * @brief 获取当前tick（代表真实时间，毫秒）
 * @return 当前g_task_tick值（代表真实时间，毫秒）
 * @note g_task_tick在TIM2_IRQHandler中递增，频率切换时自动适配
 */
uint32_t TIM2_TimeBase_GetTick(void);

/**
 * @brief 检查TIM2时间基准是否已初始化
 * @return true=已初始化，false=未初始化
 */
bool TIM2_TimeBase_IsInitialized(void);

#endif /* TIM2_TIMEBASE_H */
