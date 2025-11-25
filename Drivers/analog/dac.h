/**
 * @file dac.h
 * @brief DAC驱动模块
 * @details 提供DAC电压输出功能，支持DAC1/2双通道
 * @note 仅HD/CL/HD_VL/MD_VL型号支持DAC，MD/LD型号不支持
 */

#ifndef DAC_H
#define DAC_H

#include "error_code.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL) || defined(STM32F10X_MD_VL)

/**
 * @brief DAC状态枚举
 */
typedef enum {
    DAC_OK = ERROR_OK,                                    /**< 操作成功 */
    DAC_ERROR_NOT_IMPLEMENTED = ERROR_BASE_DAC - 99,     /**< 功能未实现（占位空函数） */
    DAC_ERROR_NULL_PTR = ERROR_BASE_DAC - 1,              /**< 空指针错误 */
    DAC_ERROR_INVALID_PARAM = ERROR_BASE_DAC - 2,        /**< 参数错误（通用） */
    DAC_ERROR_INVALID_CHANNEL = ERROR_BASE_DAC - 3,       /**< 无效的通道 */
    DAC_ERROR_NOT_INITIALIZED = ERROR_BASE_DAC - 4,       /**< 未初始化 */
    DAC_ERROR_GPIO_FAILED = ERROR_BASE_DAC - 5,           /**< GPIO配置失败 */
} DAC_Status_t;

/**
 * @brief DAC通道枚举
 */
typedef enum {
    DAC_CHANNEL_1 = 0,  /**< DAC通道1（PA4） */
    DAC_CHANNEL_2 = 1,  /**< DAC通道2（PA5） */
    DAC_CHANNEL_MAX     /**< 最大通道数 */
} DAC_Channel_t;

/**
 * @brief DAC触发模式枚举
 */
typedef enum {
    DAC_TRIGGER_NONE = 0,        /**< 无触发（自动转换） */
    DAC_TRIGGER_SOFTWARE = 1,    /**< 软件触发 */
    DAC_TRIGGER_TIM6 = 2,        /**< TIM6触发 */
    DAC_TRIGGER_TIM7 = 3,        /**< TIM7触发 */
    DAC_TRIGGER_TIM2 = 4,        /**< TIM2触发 */
    DAC_TRIGGER_TIM4 = 5,        /**< TIM4触发 */
    DAC_TRIGGER_EXTI9 = 6,       /**< EXTI9触发 */
} DAC_Trigger_t;

/**
 * @brief DAC波形生成枚举
 */
typedef enum {
    DAC_WAVE_NONE = 0,       /**< 无波形生成 */
    DAC_WAVE_NOISE = 1,      /**< 噪声波形 */
    DAC_WAVE_TRIANGLE = 2,   /**< 三角波形 */
} DAC_Wave_t;

/**
 * @brief DAC输出缓冲使能枚举
 */
typedef enum {
    DAC_OUTPUT_BUFFER_ENABLE = 0,   /**< 使能输出缓冲 */
    DAC_OUTPUT_BUFFER_DISABLE = 1,  /**< 禁用输出缓冲 */
} DAC_OutputBuffer_t;

/**
 * @brief DAC初始化
 * @param[in] channel DAC通道（1或2）
 * @param[in] trigger 触发模式
 * @param[in] output_buffer 输出缓冲使能
 * @return DAC_Status_t 状态码
 */
DAC_Status_t DAC_Init(DAC_Channel_t channel, DAC_Trigger_t trigger, DAC_OutputBuffer_t output_buffer);

/**
 * @brief DAC反初始化
 * @param[in] channel DAC通道
 * @return DAC_Status_t 状态码
 */
DAC_Status_t DAC_Deinit(DAC_Channel_t channel);

/**
 * @brief 设置DAC输出电压值（12位，0-4095）
 * @param[in] channel DAC通道
 * @param[in] value 输出值（0-4095，对应0V-Vref）
 * @return DAC_Status_t 状态码
 */
DAC_Status_t DAC_SetValue(DAC_Channel_t channel, uint16_t value);

/**
 * @brief 设置DAC输出电压（浮点数，0.0-3.3V）
 * @param[in] channel DAC通道
 * @param[in] voltage 电压值（0.0-3.3V）
 * @return DAC_Status_t 状态码
 * @note 假设Vref = 3.3V
 */
DAC_Status_t DAC_SetVoltage(DAC_Channel_t channel, float voltage);

/**
 * @brief 使能DAC通道
 * @param[in] channel DAC通道
 * @return DAC_Status_t 状态码
 */
DAC_Status_t DAC_Enable(DAC_Channel_t channel);

/**
 * @brief 禁用DAC通道
 * @param[in] channel DAC通道
 * @return DAC_Status_t 状态码
 */
DAC_Status_t DAC_Disable(DAC_Channel_t channel);

/**
 * @brief 配置波形生成
 * @param[in] channel DAC通道
 * @param[in] wave 波形类型
 * @param[in] amplitude 幅度（用于三角波，1-4095）
 * @return DAC_Status_t 状态码
 */
DAC_Status_t DAC_ConfigWave(DAC_Channel_t channel, DAC_Wave_t wave, uint16_t amplitude);

/**
 * @brief 禁用波形生成
 * @param[in] channel DAC通道
 * @return DAC_Status_t 状态码
 */
DAC_Status_t DAC_DisableWave(DAC_Channel_t channel);

/**
 * @brief 软件触发DAC转换
 * @param[in] channel DAC通道
 * @return DAC_Status_t 状态码
 */
DAC_Status_t DAC_SoftwareTrigger(DAC_Channel_t channel);

/**
 * @brief 检查DAC是否已初始化
 * @param[in] channel DAC通道
 * @return uint8_t 1=已初始化，0=未初始化
 */
uint8_t DAC_IsInitialized(DAC_Channel_t channel);

/**
 * @brief 获取DAC当前输出值
 * @param[in] channel DAC通道
 * @return uint16_t 输出值（0-4095）
 */
uint16_t DAC_GetValue(DAC_Channel_t channel);

/* ========== DMA模式功能 ========== */

/**
 * @brief DAC DMA启动
 * @param[in] channel DAC通道
 * @param[in] buffer 数据缓冲区（12位数据，0-4095）
 * @param[in] length 数据长度
 * @return DAC_Status_t 状态码
 * @note DAC1使用DMA1_CH3，DAC2使用DMA1_CH4
 */
DAC_Status_t DAC_StartDMA(DAC_Channel_t channel, const uint16_t *buffer, uint16_t length);

/**
 * @brief DAC DMA停止
 * @param[in] channel DAC通道
 * @return DAC_Status_t 状态码
 */
DAC_Status_t DAC_StopDMA(DAC_Channel_t channel);

/* ========== 双通道同步输出功能 ========== */

/**
 * @brief DAC双通道同步设置值
 * @param[in] channel1_value DAC1输出值（12位，0-4095）
 * @param[in] channel2_value DAC2输出值（12位，0-4095）
 * @return DAC_Status_t 状态码
 * @note 使用双通道同步触发，确保两个通道同时更新
 */
DAC_Status_t DAC_SetDualValue(uint16_t channel1_value, uint16_t channel2_value);

/**
 * @brief DAC双通道同步软件触发
 * @return DAC_Status_t 状态码
 * @note 同时触发DAC1和DAC2转换
 */
DAC_Status_t DAC_DualSoftwareTrigger(void);

#endif /* STM32F10X_HD || STM32F10X_CL || STM32F10X_HD_VL || STM32F10X_MD_VL */

#ifdef __cplusplus
}
#endif

#endif /* DAC_H */

