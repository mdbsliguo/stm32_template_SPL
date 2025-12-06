/**
 * @file crc.h
 * @brief CRC驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的CRC驱动，支持CRC32计算
 */

#ifndef CRC_H
#define CRC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include <stdint.h>

/**
 * @brief CRC错误码
 */
typedef enum {
    CRC_OK = ERROR_OK,                                    /**< 操作成功 */
    CRC_ERROR_NOT_IMPLEMENTED = ERROR_BASE_CRC - 99,      /**< 功能未实现（占位空函数） */
    CRC_ERROR_NULL_PTR = ERROR_BASE_CRC - 1,              /**< 空指针错误 */
    CRC_ERROR_INVALID_PARAM = ERROR_BASE_CRC - 2,         /**< 参数非法（通用） */
} CRC_Status_t;

/* Include board.h, which includes stm32f10x.h and stm32f10x_crc.h */
#include "board.h"

/**
 * @brief 复位CRC计算单元
 * @return CRC_Status_t 错误码
 */
CRC_Status_t CRC_Reset(void);

/**
 * @brief 计算单个数据的CRC32
 * @param[in] data 数据（32位）
 * @return uint32_t CRC32值
 */
uint32_t CRC_Calculate(uint32_t data);

/**
 * @brief 计算数据块的CRC32
 * @param[in] buffer 数据缓冲区
 * @param[in] length 数据长度（32位字数量）
 * @return uint32_t CRC32值
 */
uint32_t CRC_CalculateBlock(uint32_t *buffer, uint32_t length);

/**
 * @brief 获取当前CRC32值
 * @return uint32_t CRC32值
 */
uint32_t CRC_GetValue(void);

/**
 * @brief 设置CRC独立数据寄存器（IDR）
 * @param[in] id_value ID值（8位）
 * @return CRC_Status_t 错误码
 * @note 这是对SPL库CRC_SetIDRegister的封装
 */
CRC_Status_t CRC_WriteIDRegister(uint8_t id_value);

/**
 * @brief 获取CRC独立数据寄存器（IDR）
 * @return uint8_t ID值
 * @note 这是对SPL库CRC_GetIDRegister的封装
 */
uint8_t CRC_ReadIDRegister(void);

#ifdef __cplusplus
}
#endif

#endif /* CRC_H */
