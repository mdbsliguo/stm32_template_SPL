/**
 * @file spi_sw.h
 * @brief 软件SPI驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于GPIO的软件SPI驱动，支持主模式、8位/16位数据格式、多种SPI模式等功能
 *          适用于任意GPIO引脚配置，不依赖硬件SPI外设
 */

#ifndef SPI_SW_H
#define SPI_SW_H

#ifdef __cplusplus
extern "C" {
#endif

#include "board.h"
#include "error_code.h"
#include "gpio.h"
#include <stdint.h>

/**
 * @brief 软件SPI错误码
 */
typedef enum {
    SPI_SW_OK = ERROR_OK,                                    /**< 操作成功 */
    SPI_SW_ERROR_NOT_INITIALIZED = ERROR_BASE_SOFT_SPI - 1, /**< 未初始化 */
    SPI_SW_ERROR_INVALID_PARAM = ERROR_BASE_SOFT_SPI - 2,    /**< 参数非法 */
    SPI_SW_ERROR_GPIO_FAILED = ERROR_BASE_SOFT_SPI - 3,      /**< GPIO配置失败 */
    SPI_SW_ERROR_TIMEOUT = ERROR_BASE_SOFT_SPI - 4,         /**< 操作超时 */
} SPI_SW_Status_t;

/**
 * @brief 软件SPI实例索引（用于多SPI支持）
 */
typedef enum {
    SPI_SW_INSTANCE_1 = 0,   /**< 软件SPI实例1 */
    SPI_SW_INSTANCE_2 = 1,   /**< 软件SPI实例2 */
    SPI_SW_INSTANCE_3 = 2,   /**< 软件SPI实例3 */
    SPI_SW_INSTANCE_4 = 3,   /**< 软件SPI实例4 */
    SPI_SW_INSTANCE_MAX      /**< 最大实例数 */
} SPI_SW_Instance_t;

/**
 * @brief 软件SPI初始化
 * @param[in] instance 软件SPI实例索引
 * @return SPI_SW_Status_t 错误码
 * @note 根据board.h中的配置初始化GPIO引脚
 */
SPI_SW_Status_t SPI_SW_Init(SPI_SW_Instance_t instance);

/**
 * @brief 软件SPI反初始化
 * @param[in] instance 软件SPI实例索引
 * @return SPI_SW_Status_t 错误码
 */
SPI_SW_Status_t SPI_SW_Deinit(SPI_SW_Instance_t instance);

/**
 * @brief 检查软件SPI是否已初始化
 * @param[in] instance 软件SPI实例索引
 * @return uint8_t 1=已初始化，0=未初始化
 */
uint8_t SPI_SW_IsInitialized(SPI_SW_Instance_t instance);

/**
 * @brief 软件SPI主模式发送数据（8位）
 * @param[in] instance 软件SPI实例索引
 * @param[in] data 要发送的数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @return SPI_SW_Status_t 错误码
 */
SPI_SW_Status_t SPI_SW_MasterTransmit(SPI_SW_Instance_t instance, const uint8_t *data, uint16_t length);

/**
 * @brief 软件SPI主模式接收数据（8位）
 * @param[in] instance 软件SPI实例索引
 * @param[out] data 接收数据缓冲区
 * @param[in] length 要接收的数据长度（字节数）
 * @return SPI_SW_Status_t 错误码
 */
SPI_SW_Status_t SPI_SW_MasterReceive(SPI_SW_Instance_t instance, uint8_t *data, uint16_t length);

/**
 * @brief 软件SPI主模式全双工传输（8位）
 * @param[in] instance 软件SPI实例索引
 * @param[in] tx_data 要发送的数据缓冲区
 * @param[out] rx_data 接收数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @return SPI_SW_Status_t 错误码
 */
SPI_SW_Status_t SPI_SW_MasterTransmitReceive(SPI_SW_Instance_t instance, const uint8_t *tx_data, uint8_t *rx_data, uint16_t length);

/**
 * @brief 软件SPI主模式发送单个字节
 * @param[in] instance 软件SPI实例索引
 * @param[in] data 要发送的数据
 * @return SPI_SW_Status_t 错误码
 */
SPI_SW_Status_t SPI_SW_MasterTransmitByte(SPI_SW_Instance_t instance, uint8_t data);

/**
 * @brief 软件SPI主模式接收单个字节
 * @param[in] instance 软件SPI实例索引
 * @param[out] data 接收的数据
 * @return SPI_SW_Status_t 错误码
 */
SPI_SW_Status_t SPI_SW_MasterReceiveByte(SPI_SW_Instance_t instance, uint8_t *data);

/**
 * @brief 软件SPI主模式发送数据（16位）
 * @param[in] instance 软件SPI实例索引
 * @param[in] data 要发送的数据缓冲区
 * @param[in] length 数据长度（16位字数量）
 * @return SPI_SW_Status_t 错误码
 */
SPI_SW_Status_t SPI_SW_MasterTransmit16(SPI_SW_Instance_t instance, const uint16_t *data, uint16_t length);

/**
 * @brief 软件SPI主模式接收数据（16位）
 * @param[in] instance 软件SPI实例索引
 * @param[out] data 接收数据缓冲区
 * @param[in] length 要接收的数据长度（16位字数量）
 * @return SPI_SW_Status_t 错误码
 */
SPI_SW_Status_t SPI_SW_MasterReceive16(SPI_SW_Instance_t instance, uint16_t *data, uint16_t length);

/**
 * @brief 软件SPI主模式全双工传输（16位）
 * @param[in] instance 软件SPI实例索引
 * @param[in] tx_data 要发送的数据缓冲区
 * @param[out] rx_data 接收数据缓冲区
 * @param[in] length 数据长度（16位字数量）
 * @return SPI_SW_Status_t 错误码
 */
SPI_SW_Status_t SPI_SW_MasterTransmitReceive16(SPI_SW_Instance_t instance, const uint16_t *tx_data, uint16_t *rx_data, uint16_t length);

/**
 * @brief 软件SPI NSS控制（拉低NSS）
 * @param[in] instance 软件SPI实例索引
 * @return SPI_SW_Status_t 错误码
 */
SPI_SW_Status_t SPI_SW_NSS_Low(SPI_SW_Instance_t instance);

/**
 * @brief 软件SPI NSS控制（拉高NSS）
 * @param[in] instance 软件SPI实例索引
 * @return SPI_SW_Status_t 错误码
 */
SPI_SW_Status_t SPI_SW_NSS_High(SPI_SW_Instance_t instance);

#ifdef __cplusplus
}
#endif

#endif /* SPI_SW_H */



