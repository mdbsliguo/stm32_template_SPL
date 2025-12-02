/**
 * @file timer_encoder.h
 * @brief 定时器编码器模式模块
 * @details 提供定时器编码器接口功能，用于读取正交编码器（如电机编码器）的位置和速度
 */

#ifndef TIMER_ENCODER_H
#define TIMER_ENCODER_H

#include "error_code.h"
#include <stdint.h>
#include <stdbool.h>  /* 用于bool类型 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 编码器状态枚举
 */
typedef enum {
    ENCODER_OK = ERROR_OK,                                    /**< 操作成功 */
    ENCODER_ERROR_NOT_IMPLEMENTED = ERROR_BASE_TIMER - 99,   /**< 功能未实现（占位空函数） */
    ENCODER_ERROR_NULL_PTR = ERROR_BASE_TIMER - 20,          /**< 空指针错误 */
    ENCODER_ERROR_INVALID_PARAM = ERROR_BASE_TIMER - 21,     /**< 参数错误（通用） */
    ENCODER_ERROR_INVALID_INSTANCE = ERROR_BASE_TIMER - 22,   /**< 无效实例编号 */
    ENCODER_ERROR_NOT_INITIALIZED = ERROR_BASE_TIMER - 23,    /**< 未初始化 */
    ENCODER_ERROR_INVALID_PERIPH = ERROR_BASE_TIMER - 24,     /**< 无效的外设 */
    ENCODER_ERROR_GPIO_FAILED = ERROR_BASE_TIMER - 25,        /**< GPIO配置失败 */
    ENCODER_ERROR_ALREADY_INITIALIZED = ERROR_BASE_TIMER - 26, /**< 已初始化 */
} ENCODER_Status_t;

/**
 * @brief 编码器实例索引
 */
typedef enum {
    ENCODER_INSTANCE_TIM1 = 0,   /**< TIM1实例 */
    ENCODER_INSTANCE_TIM2 = 1,   /**< TIM2实例 */
    ENCODER_INSTANCE_TIM3 = 2,   /**< TIM3实例 */
    ENCODER_INSTANCE_TIM4 = 3,   /**< TIM4实例 */
    ENCODER_INSTANCE_MAX         /**< 最大实例数 */
} ENCODER_Instance_t;

/**
 * @brief 编码器模式枚举
 */
typedef enum {
    ENCODER_MODE_TI1 = 0,        /**< 仅在TI1边沿计数（根据TI2电平） */
    ENCODER_MODE_TI2 = 1,        /**< 仅在TI2边沿计数（根据TI1电平） */
    ENCODER_MODE_TI12 = 2,       /**< 在TI1和TI2边沿计数（4倍频） */
} ENCODER_Mode_t;

/**
 * @brief 编码器方向枚举
 */
typedef enum {
    ENCODER_DIR_FORWARD = 0,     /**< 正向 */
    ENCODER_DIR_BACKWARD = 1,    /**< 反向 */
} ENCODER_Direction_t;

/* Include board.h, which includes stm32f10x.h and stm32f10x_tim.h */
#include "board.h"

/**
 * @brief 编码器初始化
 * @param[in] instance 编码器实例索引
 * @param[in] mode 编码器模式
 * @return ENCODER_Status_t 状态码
 */
ENCODER_Status_t ENCODER_Init(ENCODER_Instance_t instance, ENCODER_Mode_t mode);

/**
 * @brief 编码器反初始化
 * @param[in] instance 编码器实例索引
 * @return ENCODER_Status_t 状态码
 */
ENCODER_Status_t ENCODER_Deinit(ENCODER_Instance_t instance);

/**
 * @brief 读取编码器计数值
 * @param[in] instance 编码器实例索引
 * @param[out] count 计数值（有符号，可正可负）
 * @return ENCODER_Status_t 状态码
 */
ENCODER_Status_t ENCODER_ReadCount(ENCODER_Instance_t instance, int32_t *count);

/**
 * @brief 读取编码器计数值（无符号）
 * @param[in] instance 编码器实例索引
 * @param[out] count 计数值（无符号）
 * @return ENCODER_Status_t 状态码
 */
ENCODER_Status_t ENCODER_ReadCountUnsigned(ENCODER_Instance_t instance, uint32_t *count);

/**
 * @brief 设置编码器计数值
 * @param[in] instance 编码器实例索引
 * @param[in] count 计数值
 * @return ENCODER_Status_t 状态码
 */
ENCODER_Status_t ENCODER_SetCount(ENCODER_Instance_t instance, int32_t count);

/**
 * @brief 清零编码器计数值
 * @param[in] instance 编码器实例索引
 * @return ENCODER_Status_t 状态码
 */
ENCODER_Status_t ENCODER_ClearCount(ENCODER_Instance_t instance);

/**
 * @brief 获取编码器方向
 * @param[in] instance 编码器实例索引
 * @param[out] direction 方向（ENCODER_DIR_FORWARD或ENCODER_DIR_BACKWARD）
 * @return ENCODER_Status_t 状态码
 */
ENCODER_Status_t ENCODER_GetDirection(ENCODER_Instance_t instance, ENCODER_Direction_t *direction);

/**
 * @brief 启动编码器
 * @param[in] instance 编码器实例索引
 * @return ENCODER_Status_t 状态码
 */
ENCODER_Status_t ENCODER_Start(ENCODER_Instance_t instance);

/**
 * @brief 停止编码器
 * @param[in] instance 编码器实例索引
 * @return ENCODER_Status_t 状态码
 */
ENCODER_Status_t ENCODER_Stop(ENCODER_Instance_t instance);

/**
 * @brief 检查编码器是否已初始化
 * @param[in] instance 编码器实例索引
 * @return uint8_t 1=已初始化，0=未初始化
 */
uint8_t ENCODER_IsInitialized(ENCODER_Instance_t instance);

/**
 * @brief 设置TIM3重映射配置
 * @param[in] enable_remap 是否启用重映射（true=启用，false=默认PA6/PA7）
 * @param[in] full_remap 是否完全重映射（true=PC6/PC7，false=PB4/PB5）
 * @note 必须在ENCODER_Init之前调用
 * @note 仅对TIM3有效
 * @note 重映射选项：
 *   - 默认：CH1=PA6, CH2=PA7
 *   - 部分重映射：CH1=PB4, CH2=PB5
 *   - 完全重映射：CH1=PC6, CH2=PC7
 */
void ENCODER_SetTIM3Remap(bool enable_remap, bool full_remap);

/* ========== 中断模式功能 ========== */

/**
 * @brief 编码器中断类型枚举
 */
typedef enum {
    ENCODER_IT_OVERFLOW = 0,      /**< 计数器溢出中断 */
    ENCODER_IT_DIRECTION = 1,     /**< 方向变化中断 */
} ENCODER_IT_t;

/**
 * @brief 编码器中断回调函数类型
 * @param[in] instance 编码器实例索引
 * @param[in] it_type 中断类型
 * @param[in] count 当前计数值（仅溢出中断有效）
 * @param[in] user_data 用户数据指针
 */
typedef void (*ENCODER_IT_Callback_t)(ENCODER_Instance_t instance, ENCODER_IT_t it_type,
                                      int32_t count, void *user_data);

/**
 * @brief 使能编码器中断
 * @param[in] instance 编码器实例索引
 * @param[in] it_type 中断类型
 * @return ENCODER_Status_t 状态码
 */
ENCODER_Status_t ENCODER_EnableIT(ENCODER_Instance_t instance, ENCODER_IT_t it_type);

/**
 * @brief 禁用编码器中断
 * @param[in] instance 编码器实例索引
 * @param[in] it_type 中断类型
 * @return ENCODER_Status_t 状态码
 */
ENCODER_Status_t ENCODER_DisableIT(ENCODER_Instance_t instance, ENCODER_IT_t it_type);

/**
 * @brief 设置编码器中断回调函数
 * @param[in] instance 编码器实例索引
 * @param[in] it_type 中断类型
 * @param[in] callback 回调函数指针，NULL表示禁用回调
 * @param[in] user_data 用户数据指针
 * @return ENCODER_Status_t 状态码
 */
ENCODER_Status_t ENCODER_SetITCallback(ENCODER_Instance_t instance, ENCODER_IT_t it_type,
                                       ENCODER_IT_Callback_t callback, void *user_data);

/**
 * @brief 编码器中断服务函数（应在中断服务程序中调用）
 * @param[in] instance 编码器实例索引
 */
void ENCODER_IRQHandler(ENCODER_Instance_t instance);

#ifdef __cplusplus
}
#endif

#endif /* TIMER_ENCODER_H */

