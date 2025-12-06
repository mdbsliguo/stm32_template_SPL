/**
 * @file cec.h
 * @brief CEC驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的CEC驱动，支持消费电子控制协议、HDMI CEC支持
 * @note 仅CL型号支持，特殊应用场景
 */

#ifndef CEC_H
#define CEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include <stdint.h>

/**
 * @brief CEC错误码
 */
typedef enum {
    CEC_OK = ERROR_OK,                                    /**< 操作成功 */
    CEC_ERROR_NOT_IMPLEMENTED = ERROR_BASE_CEC - 99,    /**< 功能未实现（占位空函数） */
    CEC_ERROR_INVALID_PARAM = ERROR_BASE_CEC - 1,       /**< 参数非法（通用） */
    CEC_ERROR_NOT_INITIALIZED = ERROR_BASE_CEC - 2,      /**< 未初始化 */
} CEC_Status_t;

/**
 * @brief CEC位时序错误模式枚举
 */
typedef enum {
    CEC_BIT_TIMING_STD = 0,      /**< 标准模式 */
    CEC_BIT_TIMING_ERR_FREE = 1, /**< 错误自由模式 */
} CEC_BitTimingMode_t;

/**
 * @brief CEC位周期错误模式枚举
 */
typedef enum {
    CEC_BIT_PERIOD_STD = 0,      /**< 标准模式 */
    CEC_BIT_PERIOD_FLEXIBLE = 1, /**< 灵活模式 */
} CEC_BitPeriodMode_t;

/* Include board.h, which includes stm32f10x.h and stm32f10x_cec.h */
#include "board.h"

/**
 * @brief CEC初始化
 * @param[in] bit_timing_mode 位时序错误模式
 * @param[in] bit_period_mode 位周期错误模式
 * @param[in] own_address 本机地址（0-15）
 * @param[in] prescaler 预分频器（0-16383）
 * @return CEC_Status_t 错误码
 */
CEC_Status_t CEC_Init(CEC_BitTimingMode_t bit_timing_mode, CEC_BitPeriodMode_t bit_period_mode,
                      uint8_t own_address, uint16_t prescaler);

/**
 * @brief CEC反初始化
 * @return CEC_Status_t 错误码
 */
CEC_Status_t CEC_Deinit(void);

/**
 * @brief 使能CEC
 * @return CEC_Status_t 错误码
 */
CEC_Status_t CEC_Enable(void);

/**
 * @brief 禁用CEC
 * @return CEC_Status_t 错误码
 */
CEC_Status_t CEC_Disable(void);

/**
 * @brief 使能CEC中断
 * @return CEC_Status_t 错误码
 */
CEC_Status_t CEC_EnableIT(void);

/**
 * @brief 禁用CEC中断
 * @return CEC_Status_t 错误码
 */
CEC_Status_t CEC_DisableIT(void);

/**
 * @brief 配置CEC本机地址
 * @param[in] own_address 本机地址（0-15）
 * @return CEC_Status_t 错误码
 */
CEC_Status_t CEC_SetOwnAddress(uint8_t own_address);

/**
 * @brief 设置CEC预分频器
 * @param[in] prescaler 预分频器（0-16383）
 * @return CEC_Status_t 错误码
 */
CEC_Status_t CEC_SetPrescaler(uint16_t prescaler);

/**
 * @brief 发送CEC数据字节
 * @param[in] data 数据字节
 * @return CEC_Status_t 错误码
 */
CEC_Status_t CEC_SendDataByte(uint8_t data);

/**
 * @brief 接收CEC数据字节
 * @return uint8_t 数据字节
 */
uint8_t CEC_ReceiveDataByte(void);

/**
 * @brief 启动CEC消息
 * @return CEC_Status_t 错误码
 */
CEC_Status_t CEC_StartOfMessage(void);

/**
 * @brief 结束CEC消息
 * @return CEC_Status_t 错误码
 */
CEC_Status_t CEC_EndOfMessage(void);

/**
 * @brief 获取CEC标志状态
 * @param[in] flag 标志位
 * @return uint8_t 1=置位，0=复位
 */
uint8_t CEC_GetFlagStatus(uint32_t flag);

/**
 * @brief 清除CEC标志
 * @param[in] flag 标志位
 * @return CEC_Status_t 错误码
 */
CEC_Status_t CEC_ClearFlag(uint32_t flag);

/**
 * @brief 获取CEC中断状态
 * @param[in] it 中断类型
 * @return uint8_t 1=置位，0=复位
 */
uint8_t CEC_GetITStatus(uint8_t it);

/**
 * @brief 清除CEC中断挂起位
 * @param[in] it 中断类型
 * @return CEC_Status_t 错误码
 */
CEC_Status_t CEC_ClearITPendingBit(uint16_t it);

/**
 * @brief 检查CEC是否已初始化
 * @return uint8_t 1=已初始化，0=未初始化
 */
uint8_t CEC_IsInitialized(void);

#ifdef __cplusplus
}
#endif

#endif /* CEC_H */
