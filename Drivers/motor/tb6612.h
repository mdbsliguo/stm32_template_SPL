/**
 * @file tb6612.h
 * @brief TB6612双路直流电机驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的TB6612驱动，支持正反转、制动、PWM速度控制
 */

#ifndef TB6612_H
#define TB6612_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include "board.h"
#include <stdint.h>

/**
 * @brief TB6612错误码
 */
typedef enum {
    TB6612_OK = ERROR_OK,                                    /**< 操作成功 */
    TB6612_ERROR_NULL_PTR = ERROR_BASE_TB6612 - 1,          /**< 空指针错误 */
    TB6612_ERROR_INVALID_PARAM = ERROR_BASE_TB6612 - 2,     /**< 参数非法（通用） */
    TB6612_ERROR_INVALID_INSTANCE = ERROR_BASE_TB6612 - 3,  /**< 无效实例编号 */
    TB6612_ERROR_NOT_INITIALIZED = ERROR_BASE_TB6612 - 4,   /**< 未初始化 */
    TB6612_ERROR_GPIO_FAILED = ERROR_BASE_TB6612 - 5,        /**< GPIO配置失败 */
    TB6612_ERROR_PWM_FAILED = ERROR_BASE_TB6612 - 6,         /**< PWM配置失败 */
} TB6612_Status_t;

/**
 * @brief TB6612实例索引
 */
typedef enum {
    TB6612_INSTANCE_1 = 0,   /**< TB6612实例1 */
    TB6612_INSTANCE_2 = 1,   /**< TB6612实例2 */
    TB6612_INSTANCE_MAX      /**< 最大实例数 */
} TB6612_Instance_t;

/**
 * @brief TB6612电机方向
 */
typedef enum {
    TB6612_DIR_STOP = 0,     /**< 停止 */
    TB6612_DIR_FORWARD = 1,  /**< 正转 */
    TB6612_DIR_BACKWARD = 2, /**< 反转 */
    TB6612_DIR_BRAKE = 3,    /**< 制动 */
} TB6612_Direction_t;

/**
 * @brief TB6612初始化
 * @param[in] instance TB6612实例索引
 * @return TB6612_Status_t 错误码
 * @note 根据board.h中的配置初始化GPIO和PWM
 */
TB6612_Status_t TB6612_Init(TB6612_Instance_t instance);

/**
 * @brief TB6612反初始化
 * @param[in] instance TB6612实例索引
 * @return TB6612_Status_t 错误码
 */
TB6612_Status_t TB6612_Deinit(TB6612_Instance_t instance);

/**
 * @brief 设置电机方向
 * @param[in] instance TB6612实例索引
 * @param[in] direction 方向（TB6612_DIR_FORWARD/BACKWARD/STOP/BRAKE）
 * @return TB6612_Status_t 错误码
 */
TB6612_Status_t TB6612_SetDirection(TB6612_Instance_t instance, TB6612_Direction_t direction);

/**
 * @brief 设置电机速度（PWM占空比）
 * @param[in] instance TB6612实例索引
 * @param[in] speed 速度（0.0 ~ 100.0，单位：百分比）
 * @return TB6612_Status_t 错误码
 */
TB6612_Status_t TB6612_SetSpeed(TB6612_Instance_t instance, float speed);

/**
 * @brief 使能TB6612（退出待机模式）
 * @param[in] instance TB6612实例索引
 * @return TB6612_Status_t 错误码
 */
TB6612_Status_t TB6612_Enable(TB6612_Instance_t instance);

/**
 * @brief 禁用TB6612（进入待机模式）
 * @param[in] instance TB6612实例索引
 * @return TB6612_Status_t 错误码
 */
TB6612_Status_t TB6612_Disable(TB6612_Instance_t instance);

/**
 * @brief 检查TB6612是否已初始化
 * @param[in] instance TB6612实例索引
 * @return uint8_t 1-已初始化，0-未初始化
 */
uint8_t TB6612_IsInitialized(TB6612_Instance_t instance);

#ifdef __cplusplus
}
#endif

#endif /* TB6612_H */
















