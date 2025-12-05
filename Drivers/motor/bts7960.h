/**
 * @file bts7960.h
 * @brief BTS7960大电流H桥电机驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的BTS7960驱动，支持正反转、制动、PWM速度控制、电流报警检测
 */

#ifndef BTS7960_H
#define BTS7960_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include "board.h"
#include <stdint.h>

/**
 * @brief BTS7960错误码
 */
typedef enum {
    BTS7960_OK = ERROR_OK,                                    /**< 操作成功 */
    BTS7960_ERROR_NULL_PTR = ERROR_BASE_BTS7960 - 1,          /**< 空指针错误 */
    BTS7960_ERROR_INVALID_PARAM = ERROR_BASE_BTS7960 - 2,     /**< 参数非法（通用） */
    BTS7960_ERROR_INVALID_INSTANCE = ERROR_BASE_BTS7960 - 3,  /**< 无效实例编号 */
    BTS7960_ERROR_NOT_INITIALIZED = ERROR_BASE_BTS7960 - 4,   /**< 未初始化 */
    BTS7960_ERROR_GPIO_FAILED = ERROR_BASE_BTS7960 - 5,        /**< GPIO配置失败 */
    BTS7960_ERROR_PWM_FAILED = ERROR_BASE_BTS7960 - 6,         /**< PWM配置失败 */
    BTS7960_ERROR_CURRENT_ALARM = ERROR_BASE_BTS7960 - 7,     /**< 电流报警检测 */
    BTS7960_ERROR_FREQ_OUT_OF_RANGE = ERROR_BASE_BTS7960 - 8, /**< PWM频率超出范围（>25kHz） */
} BTS7960_Status_t;

/**
 * @brief BTS7960实例索引
 */
typedef enum {
    BTS7960_INSTANCE_1 = 0,   /**< BTS7960实例1 */
    BTS7960_INSTANCE_2 = 1,   /**< BTS7960实例2 */
    BTS7960_INSTANCE_MAX      /**< 最大实例数 */
} BTS7960_Instance_t;

/**
 * @brief BTS7960电机方向
 */
typedef enum {
    BTS7960_DIR_STOP = 0,     /**< 停止 */
    BTS7960_DIR_FORWARD = 1,  /**< 正转 */
    BTS7960_DIR_BACKWARD = 2, /**< 反转 */
    BTS7960_DIR_BRAKE = 3,    /**< 制动 */
} BTS7960_Direction_t;

/**
 * @brief 电流报警状态
 */
typedef struct {
    uint8_t r_is_alarm;        /**< R_IS报警状态：1=报警，0=正常 */
    uint8_t l_is_alarm;        /**< L_IS报警状态：1=报警，0=正常 */
} BTS7960_CurrentAlarmStatus_t;

/**
 * @brief 电流报警回调函数类型
 * @param[in] instance BTS7960实例索引
 * @param[in] alarm_status 报警状态
 */
typedef void (*BTS7960_CurrentAlarmCallback_t)(BTS7960_Instance_t instance, BTS7960_CurrentAlarmStatus_t alarm_status);

/**
 * @brief BTS7960初始化
 * @param[in] instance BTS7960实例索引
 * @return BTS7960_Status_t 错误码
 * @note 根据board.h中的配置初始化GPIO和PWM，PWM频率默认设置为20kHz（≤25kHz限制）
 */
BTS7960_Status_t BTS7960_Init(BTS7960_Instance_t instance);

/**
 * @brief BTS7960反初始化
 * @param[in] instance BTS7960实例索引
 * @return BTS7960_Status_t 错误码
 */
BTS7960_Status_t BTS7960_Deinit(BTS7960_Instance_t instance);

/**
 * @brief 设置电机方向
 * @param[in] instance BTS7960实例索引
 * @param[in] direction 方向（BTS7960_DIR_FORWARD/BACKWARD/STOP/BRAKE）
 * @return BTS7960_Status_t 错误码
 */
BTS7960_Status_t BTS7960_SetDirection(BTS7960_Instance_t instance, BTS7960_Direction_t direction);

/**
 * @brief 设置电机速度（PWM占空比）
 * @param[in] instance BTS7960实例索引
 * @param[in] speed 速度（0.0 ~ 100.0，单位：百分比）
 * @return BTS7960_Status_t 错误码
 */
BTS7960_Status_t BTS7960_SetSpeed(BTS7960_Instance_t instance, float speed);

/**
 * @brief 使能BTS7960（设置R_EN和L_EN同时为高电平）
 * @param[in] instance BTS7960实例索引
 * @return BTS7960_Status_t 错误码
 * @note R_EN和L_EN必须同时接高电平才能工作，即使只单向驱动也必须同时使能
 */
BTS7960_Status_t BTS7960_Enable(BTS7960_Instance_t instance);

/**
 * @brief 禁用BTS7960（设置R_EN和L_EN同时为低电平）
 * @param[in] instance BTS7960实例索引
 * @return BTS7960_Status_t 错误码
 */
BTS7960_Status_t BTS7960_Disable(BTS7960_Instance_t instance);

/**
 * @brief 检查BTS7960是否已初始化
 * @param[in] instance BTS7960实例索引
 * @return uint8_t 1-已初始化，0-未初始化
 */
uint8_t BTS7960_IsInitialized(BTS7960_Instance_t instance);

/**
 * @brief 获取电流报警状态
 * @param[in] instance BTS7960实例索引
 * @param[out] alarm_status 报警状态指针
 * @return BTS7960_Status_t 错误码
 * @note 读取R_IS/L_IS引脚状态，过流时输出报警信号
 */
BTS7960_Status_t BTS7960_GetCurrentAlarmStatus(BTS7960_Instance_t instance, BTS7960_CurrentAlarmStatus_t *alarm_status);

/**
 * @brief 设置电流报警回调函数
 * @param[in] instance BTS7960实例索引
 * @param[in] callback 回调函数指针（NULL表示禁用回调）
 * @return BTS7960_Status_t 错误码
 * @note 需要先调用BTS7960_EnableCurrentAlarmInterrupt()使能中断
 */
BTS7960_Status_t BTS7960_SetCurrentAlarmCallback(BTS7960_Instance_t instance, BTS7960_CurrentAlarmCallback_t callback);

/**
 * @brief 使能电流报警中断
 * @param[in] instance BTS7960实例索引
 * @return BTS7960_Status_t 错误码
 * @note 配置R_IS/L_IS为EXTI输入模式，支持中断检测
 */
BTS7960_Status_t BTS7960_EnableCurrentAlarmInterrupt(BTS7960_Instance_t instance);

/**
 * @brief 禁用电流报警中断
 * @param[in] instance BTS7960实例索引
 * @return BTS7960_Status_t 错误码
 */
BTS7960_Status_t BTS7960_DisableCurrentAlarmInterrupt(BTS7960_Instance_t instance);

/**
 * @brief 设置PWM频率
 * @param[in] instance BTS7960实例索引
 * @param[in] frequency 频率（Hz），范围：1Hz ~ 25kHz
 * @return BTS7960_Status_t 错误码
 * @note BTS7960要求PWM频率≤25kHz，建议使用20kHz
 */
BTS7960_Status_t BTS7960_SetPWMFrequency(BTS7960_Instance_t instance, uint32_t frequency);

/**
 * @brief 获取当前PWM频率
 * @param[in] instance BTS7960实例索引
 * @param[out] frequency 频率指针（Hz）
 * @return BTS7960_Status_t 错误码
 */
BTS7960_Status_t BTS7960_GetPWMFrequency(BTS7960_Instance_t instance, uint32_t *frequency);

#ifdef __cplusplus
}
#endif

#endif /* BTS7960_H */




