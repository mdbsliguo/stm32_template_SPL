/**
 * @file crc.c
 * @brief CRC驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的CRC驱动，支持CRC32计算
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_CRC_ENABLED

/* Include our header */
#include "crc.h"

#include "stm32f10x_rcc.h"
#include "stm32f10x_crc.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief 复位CRC计算单元
 */
CRC_Status_t CRC_Reset(void)
{
    
    return CRC_OK;
}

/**
 * @brief 计算单个数据的CRC32
 */
uint32_t CRC_Calculate(uint32_t data)
{
    (void)data;
    return 0;
}

/**
 * @brief 计算数据块的CRC32
 */
uint32_t CRC_CalculateBlock(uint32_t *buffer, uint32_t length)
{
    (void)buffer;
    (void)length;
    return 0;
}

/**
 * @brief 获取当前CRC32值
 */
uint32_t CRC_GetValue(void)
{
    
    return 0;
}

/**
 * @brief 设置CRC独立数据寄存器（IDR）
 */
CRC_Status_t CRC_WriteIDRegister(uint8_t id_value)
{
    (void)id_value;
    return CRC_OK;
}

/**
 * @brief 获取CRC独立数据寄存器（IDR）
 */
uint8_t CRC_ReadIDRegister(void)
{
    return CRC_GetIDRegister();
}

#endif /* CONFIG_MODULE_CRC_ENABLED */

