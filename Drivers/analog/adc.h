/**
 * @file adc.h
 * @brief ADC驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的ADC驱动，支持ADC1/2/3，单次/连续转换，多通道扫描，中断模式，DMA模式，注入通道，双ADC模式
 */

#ifndef ADC_H
#define ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include <stdint.h>

/**
 * @brief ADC错误码
 */
typedef enum {
    ADC_OK = ERROR_OK,                                    /**< 操作成功 */
    ADC_ERROR_NOT_IMPLEMENTED = ERROR_BASE_ADC - 99,     /**< 功能未实现（占位空函数） */
    ADC_ERROR_NULL_PTR = ERROR_BASE_ADC - 1,              /**< 空指针错误 */
    ADC_ERROR_INVALID_PARAM = ERROR_BASE_ADC - 2,        /**< 参数非法（通用） */
    ADC_ERROR_INVALID_INSTANCE = ERROR_BASE_ADC - 3,      /**< 无效实例编号 */
    ADC_ERROR_INVALID_CHANNEL = ERROR_BASE_ADC - 4,       /**< 无效的通道 */
    ADC_ERROR_INVALID_PERIPH = ERROR_BASE_ADC - 5,        /**< 无效的外设 */
    ADC_ERROR_NOT_INITIALIZED = ERROR_BASE_ADC - 6,       /**< 未初始化 */
    ADC_ERROR_GPIO_FAILED = ERROR_BASE_ADC - 7,           /**< GPIO配置失败 */
    ADC_ERROR_TIMEOUT = ERROR_BASE_ADC - 8,               /**< 操作超时 */
    ADC_ERROR_BUSY = ERROR_BASE_ADC - 9,                   /**< ADC忙 */
} ADC_Status_t;

/**
 * @brief ADC实例索引（用于多ADC支持）
 */
typedef enum {
    ADC_INSTANCE_1 = 0,   /**< ADC1实例 */
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    ADC_INSTANCE_2 = 1,   /**< ADC2实例（仅HD/CL/HD_VL型号） */
    ADC_INSTANCE_3 = 2,   /**< ADC3实例（仅HD/CL/HD_VL型号） */
#endif
    ADC_INSTANCE_MAX      /**< 最大实例数 */
} ADC_Instance_t;

/**
 * @brief ADC双ADC模式枚举
 */
typedef enum {
    ADC_DUAL_MODE_INDEPENDENT = 0,              /**< 独立模式（默认） */
    ADC_DUAL_MODE_REG_INJEC_SIMULT = 1,         /**< 规则通道和注入通道同时转换 */
    ADC_DUAL_MODE_REG_SIMULT_ALTER_TRIG = 2,    /**< 规则通道同时转换，交替触发 */
    ADC_DUAL_MODE_INJEC_SIMULT_FAST_INTERL = 3, /**< 注入通道同时转换，快速交替 */
    ADC_DUAL_MODE_INJEC_SIMULT_SLOW_INTERL = 4, /**< 注入通道同时转换，慢速交替 */
    ADC_DUAL_MODE_INJEC_SIMULT = 5,             /**< 注入通道同时转换 */
    ADC_DUAL_MODE_REG_SIMULT = 6,               /**< 规则通道同时转换 */
    ADC_DUAL_MODE_FAST_INTERL = 7,              /**< 快速交替模式 */
    ADC_DUAL_MODE_SLOW_INTERL = 8,              /**< 慢速交替模式 */
    ADC_DUAL_MODE_ALTER_TRIG = 9,               /**< 交替触发模式 */
} ADC_DualMode_t;

/**
 * @brief ADC转换模式
 */
typedef enum {
    ADC_MODE_SINGLE = 0,      /**< 单次转换模式 */
    ADC_MODE_CONTINUOUS = 1,  /**< 连续转换模式 */
} ADC_Mode_t;

/* Include board.h, which includes stm32f10x.h and stm32f10x_adc.h */
#include "board.h"

/* ========== 基础功能 ========== */

/**
 * @brief ADC模块初始化
 * @param[in] instance ADC实例索引（ADC_INSTANCE_1/2/3）
 * @return ADC_Status_t 错误码
 * @note 根据board.h中的配置初始化ADC外设和GPIO引脚
 * @note 函数名已重命名为ADC_ModuleInit，避免与SPL库的ADC_Init函数名冲突
 */
ADC_Status_t ADC_ModuleInit(ADC_Instance_t instance);

/**
 * @brief ADC反初始化
 * @param[in] instance ADC实例索引
 * @return ADC_Status_t 错误码
 */
ADC_Status_t ADC_Deinit(ADC_Instance_t instance);

/**
 * @brief ADC单次转换（阻塞式）
 * @param[in] instance ADC实例索引
 * @param[in] channel ADC通道（ADC_Channel_0 ~ ADC_Channel_17）
 * @param[out] value 转换结果（12位，0-4095）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return ADC_Status_t 错误码
 */
ADC_Status_t ADC_ReadChannel(ADC_Instance_t instance, uint8_t channel, uint16_t *value, uint32_t timeout);

/**
 * @brief ADC设置通道采样时间
 * @param[in] instance ADC实例索引
 * @param[in] channel ADC通道（ADC_Channel_0 ~ ADC_Channel_17）
 * @param[in] sample_time 采样时间（ADC_SampleTime_1Cycles5 ~ ADC_SampleTime_239Cycles5）
 * @return ADC_Status_t 错误码
 * @note 采样时间越长，转换精度越高，但转换速度越慢
 */
ADC_Status_t ADC_SetChannelSampleTime(ADC_Instance_t instance, uint8_t channel, uint8_t sample_time);

/**
 * @brief ADC多通道扫描转换（阻塞式）
 * @param[in] instance ADC实例索引
 * @param[in] channels 通道数组（ADC_Channel_0 ~ ADC_Channel_17）
 * @param[in] channel_count 通道数量（1-16）
 * @param[out] values 转换结果数组（12位，0-4095）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_ReadChannels(ADC_Instance_t instance, const uint8_t *channels, uint8_t channel_count,
                              uint16_t *values, uint32_t timeout);

/**
 * @brief 启动ADC连续转换
 * @param[in] instance ADC实例索引
 * @param[in] channel ADC通道（ADC_Channel_0 ~ ADC_Channel_17）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_StartContinuous(ADC_Instance_t instance, uint8_t channel);

/**
 * @brief 停止ADC连续转换
 * @param[in] instance ADC实例索引
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_StopContinuous(ADC_Instance_t instance);

/**
 * @brief 读取ADC连续转换结果
 * @param[in] instance ADC实例索引
 * @param[out] value 转换结果（12位，0-4095）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_ReadContinuous(ADC_Instance_t instance, uint16_t *value);

/**
 * @brief 检查ADC是否已初始化
 * @param[in] instance ADC实例索引
 * @return uint8_t 1-已初始化，0-未初始化
 */
uint8_t ADC_IsInitialized(ADC_Instance_t instance);

/**
 * @brief 获取ADC外设指针
 * @param[in] instance ADC实例索引
 * @return ADC_TypeDef* ADC外设指针，失败返回NULL
 */
ADC_TypeDef* ADC_GetPeriph(ADC_Instance_t instance);

/* ========== 中断模式功能 ========== */

/**
 * @brief ADC中断类型枚举
 * @note 注意：枚举值名称与SPL库的宏定义不同，避免冲突
 */
typedef enum {
    ADC_IT_TYPE_EOC = 0,   /**< 转换完成中断 */
    ADC_IT_TYPE_JEOC = 1,  /**< 注入转换完成中断 */
    ADC_IT_TYPE_AWD = 2,   /**< 模拟看门狗中断 */
} ADC_IT_t;

/**
 * @brief ADC中断回调函数类型
 * @param[in] instance ADC实例索引
 * @param[in] it_type 中断类型（ADC_IT_TYPE_EOC/JEOC/AWD）
 * @param[in] value 转换结果（仅EOC和JEOC有效）
 * @param[in] user_data 用户数据指针
 */
typedef void (*ADC_IT_Callback_t)(ADC_Instance_t instance, ADC_IT_t it_type, uint16_t value, void *user_data);

/**
 * @brief 使能ADC中断
 * @param[in] instance ADC实例索引
 * @param[in] it_type 中断类型（ADC_IT_TYPE_EOC/JEOC/AWD）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_EnableIT(ADC_Instance_t instance, ADC_IT_t it_type);

/**
 * @brief 禁用ADC中断
 * @param[in] instance ADC实例索引
 * @param[in] it_type 中断类型（ADC_IT_TYPE_EOC/JEOC/AWD）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_DisableIT(ADC_Instance_t instance, ADC_IT_t it_type);

/**
 * @brief 设置ADC中断回调函数
 * @param[in] instance ADC实例索引
 * @param[in] it_type 中断类型（ADC_IT_TYPE_EOC/JEOC/AWD）
 * @param[in] callback 回调函数指针，NULL表示禁用回调
 * @param[in] user_data 用户数据指针
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_SetITCallback(ADC_Instance_t instance, ADC_IT_t it_type,
                                ADC_IT_Callback_t callback, void *user_data);

/**
 * @brief ADC中断服务函数（应在中断服务程序中调用）
 * @param[in] instance ADC实例索引
 * @note 功能未实现，当前为占位函数
 */
void ADC_IRQHandler(ADC_Instance_t instance);

/* ========== DMA模式功能 ========== */

/**
 * @brief ADC DMA启动（多通道扫描）
 * @param[in] instance ADC实例索引
 * @param[in] channels 通道数组（ADC_Channel_0 ~ ADC_Channel_17）
 * @param[in] channel_count 通道数量（1-16）
 * @param[out] buffer 转换结果缓冲区（12位，0-4095）
 * @param[in] buffer_size 缓冲区大小（必须是通道数量的倍数）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 * @note 仅ADC1支持DMA（HD/CL/HD_VL型号使用DMA2_CH1，MD/LD型号不支持DMA）
 */
ADC_Status_t ADC_StartDMA(ADC_Instance_t instance, const uint8_t *channels, uint8_t channel_count,
                          uint16_t *buffer, uint16_t buffer_size);

/**
 * @brief ADC DMA停止
 * @param[in] instance ADC实例索引
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_StopDMA(ADC_Instance_t instance);

/* ========== 注入通道功能 ========== */

/**
 * @brief ADC注入通道配置
 * @param[in] instance ADC实例索引
 * @param[in] channel ADC通道（ADC_Channel_0 ~ ADC_Channel_17）
 * @param[in] rank 注入通道序号（1-4，对应ADC_InjectedChannel_1~4）
 * @param[in] sample_time 采样时间（ADC_SampleTime_1Cycles5 ~ ADC_SampleTime_239Cycles5）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_ConfigInjectedChannel(ADC_Instance_t instance, uint8_t channel, uint8_t rank, uint8_t sample_time);

/**
 * @brief ADC启动注入转换
 * @param[in] instance ADC实例索引
 * @param[in] external_trigger 外部触发使能（1=使能，0=禁用）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_StartInjectedConversion(ADC_Instance_t instance, uint8_t external_trigger);

/**
 * @brief ADC读取注入转换结果
 * @param[in] instance ADC实例索引
 * @param[in] rank 注入通道序号（1-4）
 * @param[out] value 转换结果（12位，0-4095）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_ReadInjectedChannel(ADC_Instance_t instance, uint8_t rank, uint16_t *value);

/**
 * @brief ADC设置注入通道偏移量
 * @param[in] instance ADC实例索引
 * @param[in] rank 注入通道序号（1-4）
 * @param[in] offset 偏移量（12位，0-4095）
 * @return ADC_Status_t 错误码
 * @note 此函数与SPL库函数名冲突，暂时注释，待实现时重命名
 */
/* ADC_Status_t ADC_SetInjectedOffset(ADC_Instance_t instance, uint8_t rank, uint16_t offset); */

/* ========== 双ADC模式功能 ========== */

/**
 * @brief 配置ADC双ADC模式
 * @param[in] mode 双ADC模式（ADC_DUAL_MODE_INDEPENDENT等）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 * @note 需要ADC1和ADC2都已初始化（仅HD/CL/HD_VL型号支持）
 * @note 双ADC模式下，ADC1为主ADC，ADC2为从ADC
 */
ADC_Status_t ADC_ConfigDualMode(ADC_DualMode_t mode);

/**
 * @brief 获取当前ADC双ADC模式
 * @param[out] mode 双ADC模式（输出参数）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_GetDualMode(ADC_DualMode_t *mode);

#ifdef __cplusplus
}
#endif

#endif /* ADC_H */
