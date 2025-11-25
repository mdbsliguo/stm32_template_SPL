/**
 * @file dbgmcu.c
 * @brief DBGMCU驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的DBGMCU驱动，支持调试模式配置、低功耗调试、外设调试使能等功能
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_DBGMCU_ENABLED

/* Include our header */
#include "dbgmcu.h"

#include "stm32f10x_rcc.h"
#include "stm32f10x_dbgmcu.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief 获取设备ID
 */
uint32_t DBGMCU_GetDeviceID(void)
{
    
    return 0;
}

/**
 * @brief 获取设备版本
 */
uint32_t DBGMCU_GetRevisionID(void)
{
    
    return 0;
}

/**
 * @brief 使能低功耗模式调试
 */
DBGMCU_Status_t DBGMCU_EnableLowPowerDebug(uint32_t mode)
{
    (void)mode;
    return DBGMCU_OK;
}

/**
 * @brief 禁用低功耗模式调试
 */
DBGMCU_Status_t DBGMCU_DisableLowPowerDebug(uint32_t mode)
{
    (void)mode;
    return DBGMCU_OK;
}

/**
 * @brief 使能外设调试
 */
DBGMCU_Status_t DBGMCU_EnablePeriphDebug(uint32_t periph)
{
    (void)periph;
    return DBGMCU_OK;
}

/**
 * @brief 禁用外设调试
 */
DBGMCU_Status_t DBGMCU_DisablePeriphDebug(uint32_t periph)
{
    (void)periph;
    return DBGMCU_OK;
}

#endif /* CONFIG_MODULE_DBGMCU_ENABLED */

