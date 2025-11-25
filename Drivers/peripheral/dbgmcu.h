/**
 * @file dbgmcu.h
 * @brief DBGMCU驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的DBGMCU驱动，支持调试模式配置、低功耗调试、外设调试使能等功能
 */

#ifndef DBGMCU_H
#define DBGMCU_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include <stdint.h>

/**
 * @brief DBGMCU错误码
 */
typedef enum {
    DBGMCU_OK = ERROR_OK,                                    /**< 操作成功 */
    DBGMCU_ERROR_INVALID_PARAM = ERROR_BASE_DBGMCU - 1,     /**< 参数非法 */
} DBGMCU_Status_t;

/**
 * @brief DBGMCU低功耗调试模式枚举
 */
typedef enum {
    DBGMCU_SLEEP = 0x01,        /**< Sleep模式调试 */
    DBGMCU_STOP = 0x02,          /**< STOP模式调试 */
    DBGMCU_STANDBY = 0x04,       /**< STANDBY模式调试 */
} DBGMCU_LowPowerMode_t;

/**
 * @brief DBGMCU外设调试使能枚举
 */
typedef enum {
    DBGMCU_TIM1_STOP = 0x00000400,      /**< TIM1停止调试 */
    DBGMCU_TIM2_STOP = 0x00000800,      /**< TIM2停止调试 */
    DBGMCU_TIM3_STOP = 0x00001000,      /**< TIM3停止调试 */
    DBGMCU_TIM4_STOP = 0x00002000,      /**< TIM4停止调试 */
    DBGMCU_IWDG_STOP = 0x00000100,      /**< IWDG停止调试 */
    DBGMCU_WWDG_STOP = 0x00000200,      /**< WWDG停止调试 */
    DBGMCU_CAN1_STOP = 0x00004000,      /**< CAN1停止调试 */
    DBGMCU_I2C1_SMBUS_TIMEOUT = 0x00008000,  /**< I2C1 SMBus超时调试 */
    DBGMCU_I2C2_SMBUS_TIMEOUT = 0x00010000,  /**< I2C2 SMBus超时调试 */
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    DBGMCU_TIM8_STOP = 0x00020000,      /**< TIM8停止调试（仅HD/CL/HD_VL） */
    DBGMCU_TIM5_STOP = 0x00040000,      /**< TIM5停止调试（仅HD/CL/HD_VL） */
    DBGMCU_TIM6_STOP = 0x00080000,      /**< TIM6停止调试（仅HD/CL/HD_VL） */
#endif
} DBGMCU_PeriphDebug_t;

/* Include board.h, which includes stm32f10x.h and stm32f10x_dbgmcu.h */
#include "board.h"

/**
 * @brief 获取设备ID
 * @return uint32_t 设备ID
 */
uint32_t DBGMCU_GetDeviceID(void);

/**
 * @brief 获取设备版本
 * @return uint32_t 设备版本
 */
uint32_t DBGMCU_GetRevisionID(void);

/**
 * @brief 使能低功耗模式调试
 * @param[in] mode 低功耗模式（DBGMCU_SLEEP/STOP/STANDBY的组合）
 * @return DBGMCU_Status_t 错误码
 */
DBGMCU_Status_t DBGMCU_EnableLowPowerDebug(uint32_t mode);

/**
 * @brief 禁用低功耗模式调试
 * @param[in] mode 低功耗模式（DBGMCU_SLEEP/STOP/STANDBY的组合）
 * @return DBGMCU_Status_t 错误码
 */
DBGMCU_Status_t DBGMCU_DisableLowPowerDebug(uint32_t mode);

/**
 * @brief 使能外设调试
 * @param[in] periph 外设（DBGMCU_TIM1_STOP等的组合）
 * @return DBGMCU_Status_t 错误码
 */
DBGMCU_Status_t DBGMCU_EnablePeriphDebug(uint32_t periph);

/**
 * @brief 禁用外设调试
 * @param[in] periph 外设（DBGMCU_TIM1_STOP等的组合）
 * @return DBGMCU_Status_t 错误码
 */
DBGMCU_Status_t DBGMCU_DisablePeriphDebug(uint32_t periph);

#ifdef __cplusplus
}
#endif

#endif /* DBGMCU_H */

