/**
 * @file wwdg.h
 * @brief WWDG窗口看门狗模块
 * @details 提供窗口看门狗功能，用于系统监控和故障恢复
 * @note WWDG与IWDG的区别：WWDG有窗口限制，必须在窗口内喂狗，否则会复位
 */

#ifndef WWDG_H
#define WWDG_H

#include "error_code.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WWDG状态枚举
 */
typedef enum {
    WWDG_OK = ERROR_OK,                                    /**< 操作成功 */
    WWDG_ERROR_NOT_INITIALIZED = ERROR_BASE_IWDG - 10,   /**< 未初始化 */
    WWDG_ERROR_INVALID_PARAM = ERROR_BASE_IWDG - 11,     /**< 参数错误 */
    WWDG_ERROR_OUT_OF_WINDOW = ERROR_BASE_IWDG - 12,     /**< 超出窗口范围 */
    WWDG_ERROR_ALREADY_INITIALIZED = ERROR_BASE_IWDG - 13, /**< 已初始化 */
} WWDG_Status_t;

/**
 * @brief WWDG预分频器枚举
 */
typedef enum {
    WWDG_PRESCALER_1 = 0,   /**< 预分频器1 */
    WWDG_PRESCALER_2 = 1,   /**< 预分频器2 */
    WWDG_PRESCALER_4 = 2,   /**< 预分频器4 */
    WWDG_PRESCALER_8 = 3,   /**< 预分频器8 */
} WWDG_Prescaler_t;

/**
 * @brief WWDG初始化
 * @param[in] prescaler 预分频器
 * @param[in] window_value 窗口值（0x00-0x7F）
 * @param[in] counter 计数器初始值（0x40-0x7F）
 * @return WWDG_Status_t 状态码
 * @note 窗口值必须小于计数器值
 */
WWDG_Status_t WWDG_Init(WWDG_Prescaler_t prescaler, uint8_t window_value, uint8_t counter);

/**
 * @brief WWDG反初始化
 * @return WWDG_Status_t 状态码
 */
WWDG_Status_t WWDG_Deinit(void);

/**
 * @brief 喂狗（刷新计数器）
 * @param[in] counter 新的计数器值（0x40-0x7F）
 * @return WWDG_Status_t 状态码
 * @note 必须在窗口内喂狗，否则会复位
 */
WWDG_Status_t WWDG_Feed(uint8_t counter);

/**
 * @brief 使能WWDG中断
 * @return WWDG_Status_t 状态码
 */
WWDG_Status_t WWDG_EnableInterrupt(void);

/**
 * @brief 禁用WWDG中断
 * @return WWDG_Status_t 状态码
 */
WWDG_Status_t WWDG_DisableInterrupt(void);

/**
 * @brief 检查WWDG标志
 * @return uint8_t 1=标志置位，0=标志未置位
 */
uint8_t WWDG_CheckFlag(void);

/**
 * @brief 清除WWDG标志
 * @return WWDG_Status_t 状态码
 */
WWDG_Status_t WWDG_ClearFlag(void);

/**
 * @brief 检查WWDG是否已初始化
 * @return uint8_t 1=已初始化，0=未初始化
 */
uint8_t WWDG_IsInitialized(void);

/**
 * @brief 获取当前计数器值
 * @return uint8_t 当前计数器值
 */
uint8_t WWDG_GetCounter(void);

#ifdef __cplusplus
}
#endif

#endif /* WWDG_H */

