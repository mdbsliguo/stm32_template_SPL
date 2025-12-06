/**
 * @file nvic.h
 * @brief NVIC中断控制器模块
 * @details 提供中断优先级配置、中断使能/禁用等功能
 */

#ifndef NVIC_H
#define NVIC_H

#include "error_code.h"
#include "stm32f10x.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief NVIC状态枚举
 */
typedef enum {
    NVIC_OK = ERROR_OK,                                    /**< 操作成功 */
    NVIC_ERROR_INVALID_PARAM = ERROR_BASE_NVIC - 1,      /**< 参数错误 */
    NVIC_ERROR_INVALID_IRQ = ERROR_BASE_NVIC - 2,        /**< 无效的中断号 */
} NVIC_Status_t;

/**
 * @brief NVIC优先级分组枚举
 */
typedef enum {
    NVIC_PRIORITY_GROUP_0 = 0, /**< 0位抢占优先级，4位子优先级 */
    NVIC_PRIORITY_GROUP_1 = 1, /**< 1位抢占优先级，3位子优先级 */
    NVIC_PRIORITY_GROUP_2 = 2, /**< 2位抢占优先级，2位子优先级 */
    NVIC_PRIORITY_GROUP_3 = 3, /**< 3位抢占优先级，1位子优先级 */
    NVIC_PRIORITY_GROUP_4 = 4  /**< 4位抢占优先级，0位子优先级 */
} NVIC_PriorityGroup_t;

/**
 * @brief 配置NVIC优先级分组
 * @param[in] group 优先级分组（0-4）
 * @return NVIC_Status_t 状态码
 * @note 此函数只能调用一次，通常在系统初始化时调用
 */
NVIC_Status_t NVIC_ConfigPriorityGroup(NVIC_PriorityGroup_t group);

/**
 * @brief 配置中断优先级和使能/禁用
 * @param[in] irq 中断号（IRQn_Type）
 * @param[in] preemption_priority 抢占优先级（根据优先级分组确定范围）
 * @param[in] sub_priority 子优先级（根据优先级分组确定范围）
 * @param[in] enable 使能标志：1=使能，0=禁用
 * @return NVIC_Status_t 状态码
 */
NVIC_Status_t NVIC_ConfigIRQ(IRQn_Type irq, uint8_t preemption_priority, uint8_t sub_priority, uint8_t enable);

/**
 * @brief 使能中断
 * @param[in] irq 中断号
 * @return NVIC_Status_t 状态码
 */
NVIC_Status_t NVIC_HW_EnableIRQ(IRQn_Type irq);

/**
 * @brief 禁用中断
 * @param[in] irq 中断号
 * @return NVIC_Status_t 状态码
 */
NVIC_Status_t NVIC_HW_DisableIRQ(IRQn_Type irq);

/**
 * @brief 设置中断挂起标志
 * @param[in] irq 中断号
 * @return NVIC_Status_t 状态码
 */
NVIC_Status_t NVIC_HW_SetPendingIRQ(IRQn_Type irq);

/**
 * @brief 清除中断挂起标志
 * @param[in] irq 中断号
 * @return NVIC_Status_t 状态码
 */
NVIC_Status_t NVIC_HW_ClearPendingIRQ(IRQn_Type irq);

/**
 * @brief 获取中断挂起状态
 * @param[in] irq 中断号
 * @return uint8_t 1=挂起，0=未挂起
 */
uint8_t NVIC_HW_GetPendingIRQ(IRQn_Type irq);

/**
 * @brief 获取中断活动状态
 * @param[in] irq 中断号
 * @return uint8_t 1=活动，0=非活动
 */
uint8_t NVIC_GetActiveIRQ(IRQn_Type irq);

/**
 * @brief 获取当前优先级分组
 * @return NVIC_PriorityGroup_t 优先级分组
 */
NVIC_PriorityGroup_t NVIC_GetPriorityGroup(void);

/**
 * @brief 设置中断优先级（简化接口）
 * @param[in] irq 中断号
 * @param[in] priority 优先级值（0-15，数值越小优先级越高）
 * @return NVIC_Status_t 状态码
 * @note 此函数使用当前优先级分组，将priority分配到抢占优先级和子优先级
 */
NVIC_Status_t NVIC_HW_SetPriority(IRQn_Type irq, uint8_t priority);

/**
 * @brief 获取中断优先级
 * @param[in] irq 中断号
 * @return uint8_t 优先级值（0-15）
 */
uint8_t NVIC_HW_GetPriority(IRQn_Type irq);

#ifdef __cplusplus
}
#endif

#endif /* NVIC_H */
