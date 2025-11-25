/**
 * @file spi_hw.h
 * @brief 硬件SPI驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的硬件SPI驱动，支持主/从模式、全双工/半双工、8位/16位数据格式等功能
 */

#ifndef SPI_HW_H
#define SPI_HW_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include <stdint.h>

/* Note: We use SPI_HW_Init instead of SPI_Init to avoid conflict with STM32 standard library's SPI_Init */
/* Standard library: void SPI_Init(SPI_TypeDef *, SPI_InitTypeDef *) */
/* Our function: SPI_Status_t SPI_HW_Init(SPI_Instance_t instance) */

/**
 * @brief SPI错误码
 */
typedef enum {
    SPI_OK = ERROR_OK,                                    /**< 操作成功 */
    SPI_ERROR_NOT_INITIALIZED = ERROR_BASE_SPI - 1,      /**< 未初始化 */
    SPI_ERROR_INVALID_PARAM = ERROR_BASE_SPI - 2,         /**< 参数非法 */
    SPI_ERROR_INVALID_PERIPH = ERROR_BASE_SPI - 3,        /**< 无效的外设 */
    SPI_ERROR_GPIO_FAILED = ERROR_BASE_SPI - 4,          /**< GPIO配置失败 */
    SPI_ERROR_BUSY = ERROR_BASE_SPI - 5,                 /**< SPI总线忙 */
    SPI_ERROR_TIMEOUT = ERROR_BASE_SPI - 6,              /**< 操作超时 */
    SPI_ERROR_OVERRUN = ERROR_BASE_SPI - 7,              /**< 数据溢出 */
    SPI_ERROR_MODE_FAULT = ERROR_BASE_SPI - 8,           /**< 模式错误 */
    SPI_ERROR_CRC = ERROR_BASE_SPI - 9,                   /**< CRC错误 */
} SPI_Status_t;

/**
 * @brief SPI实例索引（用于多SPI支持）
 */
typedef enum {
    SPI_INSTANCE_1 = 0,   /**< SPI1实例 */
    SPI_INSTANCE_2 = 1,   /**< SPI2实例 */
    SPI_INSTANCE_3 = 2,   /**< SPI3实例 */
    SPI_INSTANCE_MAX      /**< 最大实例数 */
} SPI_Instance_t;

/* Include board.h, which includes stm32f10x.h and stm32f10x_spi.h */
#include "board.h"

/**
 * @brief 硬件SPI初始化
 * @param[in] instance SPI实例索引（SPI_INSTANCE_1、SPI_INSTANCE_2或SPI_INSTANCE_3）
 * @return SPI_Status_t 错误码
 * @note 根据board.h中的配置初始化SPI外设和GPIO引脚
 * @note 使用SPI_HW_Init而非SPI_Init，避免与STM32标准库的SPI_Init函数冲突
 */
SPI_Status_t SPI_HW_Init(SPI_Instance_t instance);

/**
 * @brief SPI反初始化
 * @param[in] instance SPI实例索引
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_Deinit(SPI_Instance_t instance);

/**
 * @brief SPI主模式发送数据（8位）
 * @param[in] instance SPI实例索引
 * @param[in] data 要发送的数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_MasterTransmit(SPI_Instance_t instance, const uint8_t *data, uint16_t length, uint32_t timeout);

/**
 * @brief SPI主模式接收数据（8位）
 * @param[in] instance SPI实例索引
 * @param[out] data 接收数据缓冲区
 * @param[in] length 要接收的数据长度（字节数）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_MasterReceive(SPI_Instance_t instance, uint8_t *data, uint16_t length, uint32_t timeout);

/**
 * @brief SPI主模式全双工传输（8位）
 * @param[in] instance SPI实例索引
 * @param[in] tx_data 要发送的数据缓冲区
 * @param[out] rx_data 接收数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_MasterTransmitReceive(SPI_Instance_t instance, const uint8_t *tx_data, uint8_t *rx_data, uint16_t length, uint32_t timeout);

/**
 * @brief SPI主模式发送单个字节
 * @param[in] instance SPI实例索引
 * @param[in] data 要发送的数据
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_MasterTransmitByte(SPI_Instance_t instance, uint8_t data, uint32_t timeout);

/**
 * @brief SPI主模式接收单个字节
 * @param[in] instance SPI实例索引
 * @param[out] data 接收的数据
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_MasterReceiveByte(SPI_Instance_t instance, uint8_t *data, uint32_t timeout);

/**
 * @brief SPI主模式发送数据（16位）
 * @param[in] instance SPI实例索引
 * @param[in] data 要发送的数据缓冲区
 * @param[in] length 数据长度（16位字数量）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_MasterTransmit16(SPI_Instance_t instance, const uint16_t *data, uint16_t length, uint32_t timeout);

/**
 * @brief SPI主模式接收数据（16位）
 * @param[in] instance SPI实例索引
 * @param[out] data 接收数据缓冲区
 * @param[in] length 要接收的数据长度（16位字数量）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_MasterReceive16(SPI_Instance_t instance, uint16_t *data, uint16_t length, uint32_t timeout);

/**
 * @brief SPI主模式全双工传输（16位）
 * @param[in] instance SPI实例索引
 * @param[in] tx_data 要发送的数据缓冲区
 * @param[out] rx_data 接收数据缓冲区
 * @param[in] length 数据长度（16位字数量）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_MasterTransmitReceive16(SPI_Instance_t instance, const uint16_t *tx_data, uint16_t *rx_data, uint16_t length, uint32_t timeout);

/**
 * @brief 检查SPI是否已初始化
 * @param[in] instance SPI实例索引
 * @return uint8_t 1=已初始化，0=未初始化
 */
uint8_t SPI_IsInitialized(SPI_Instance_t instance);

/* ========== 从模式功能 ========== */

/**
 * @brief SPI从模式发送数据（8位）
 * @param[in] instance SPI实例索引
 * @param[in] data 要发送的数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return SPI_Status_t 错误码
 * @note 从模式需要等待主机的时钟信号
 */
SPI_Status_t SPI_SlaveTransmit(SPI_Instance_t instance, const uint8_t *data, uint16_t length, uint32_t timeout);

/**
 * @brief SPI从模式接收数据（8位）
 * @param[in] instance SPI实例索引
 * @param[out] data 接收数据缓冲区
 * @param[in] length 要接收的数据长度（字节数）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_SlaveReceive(SPI_Instance_t instance, uint8_t *data, uint16_t length, uint32_t timeout);

/**
 * @brief SPI从模式全双工传输（8位）
 * @param[in] instance SPI实例索引
 * @param[in] tx_data 要发送的数据缓冲区
 * @param[out] rx_data 接收数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_SlaveTransmitReceive(SPI_Instance_t instance, const uint8_t *tx_data, uint8_t *rx_data, uint16_t length, uint32_t timeout);

/**
 * @brief 获取SPI外设指针
 * @param[in] instance SPI实例索引
 * @return SPI_TypeDef* SPI外设指针，失败返回NULL
 */
SPI_TypeDef* SPI_GetPeriph(SPI_Instance_t instance);

/**
 * @brief SPI软件NSS控制（拉低NSS）
 * @param[in] instance SPI实例索引
 * @return SPI_Status_t 错误码
 * @note 仅用于软件NSS模式
 */
SPI_Status_t SPI_NSS_Low(SPI_Instance_t instance);

/**
 * @brief SPI软件NSS控制（拉高NSS）
 * @param[in] instance SPI实例索引
 * @return SPI_Status_t 错误码
 * @note 仅用于软件NSS模式
 */
SPI_Status_t SPI_NSS_High(SPI_Instance_t instance);

/**
 * @brief 检查SPI是否忙
 * @param[in] instance SPI实例索引
 * @return uint8_t 1=忙，0=空闲
 */
uint8_t SPI_IsBusy(SPI_Instance_t instance);

/**
 * @brief SPI软件复位
 * @param[in] instance SPI实例索引
 * @return SPI_Status_t 错误码
 * @note 用于SPI总线出错时恢复总线状态
 */
SPI_Status_t SPI_SoftwareReset(SPI_Instance_t instance);

/**
 * @brief SPI配置信息结构体
 */
typedef struct {
    SPI_TypeDef *spi_periph;        /**< SPI外设指针 */
    uint16_t mode;                  /**< SPI模式：主/从 */
    uint16_t direction;             /**< 数据方向 */
    uint16_t data_size;             /**< 数据大小：8位/16位 */
    uint16_t cpol;                  /**< 时钟极性 */
    uint16_t cpha;                  /**< 时钟相位 */
    uint16_t nss;                   /**< NSS管理方式 */
    uint16_t baudrate_prescaler;    /**< 波特率预分频器 */
    uint16_t first_bit;             /**< 首字节 */
    uint8_t enabled;                /**< 使能标志 */
} SPI_ConfigInfo_t;

/**
 * @brief 获取SPI配置信息
 * @param[in] instance SPI实例索引
 * @param[out] config_info 配置信息结构体指针
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_GetConfig(SPI_Instance_t instance, SPI_ConfigInfo_t *config_info);

/* ========== TI模式和硬件NSS管理功能 ========== */

/**
 * @brief 使能SPI TI模式（TI SSI模式）
 * @param[in] instance SPI实例索引
 * @return SPI_Status_t 错误码
 * @note TI模式用于与TI兼容的SPI设备通信
 */
SPI_Status_t SPI_EnableTIMode(SPI_Instance_t instance);

/**
 * @brief 禁用SPI TI模式
 * @param[in] instance SPI实例索引
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_DisableTIMode(SPI_Instance_t instance);

/**
 * @brief 配置SPI硬件NSS内部软件管理
 * @param[in] instance SPI实例索引
 * @param[in] nss_level NSS电平：0=低电平，1=高电平
 * @return SPI_Status_t 错误码
 * @note 硬件NSS模式下，可以通过软件控制NSS电平
 */
SPI_Status_t SPI_ConfigHardwareNSS(SPI_Instance_t instance, uint8_t nss_level);

/**
 * @brief 配置SPI CRC计算
 * @param[in] instance SPI实例索引
 * @param[in] polynomial CRC多项式（7-65535）
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_ConfigCRC(SPI_Instance_t instance, uint16_t polynomial);

/**
 * @brief 使能SPI CRC计算
 * @param[in] instance SPI实例索引
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_EnableCRC(SPI_Instance_t instance);

/**
 * @brief 禁用SPI CRC计算
 * @param[in] instance SPI实例索引
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_DisableCRC(SPI_Instance_t instance);

/**
 * @brief 获取SPI CRC值
 * @param[in] instance SPI实例索引
 * @return uint16_t CRC值
 * @note 重命名为SPI_HW_GetCRC避免与STM32标准库的SPI_GetCRC冲突
 */
uint16_t SPI_HW_GetCRC(SPI_Instance_t instance);

/* ========== 中断模式功能 ========== */

/**
 * @brief SPI中断类型枚举
 */
typedef enum {
    SPI_IT_TXE = 0,      /**< 发送缓冲区空中断 */
    SPI_IT_RXNE = 1,     /**< 接收缓冲区非空中断 */
    SPI_IT_ERR = 2,      /**< 错误中断（包含OVR、MODF、CRCERR） */
} SPI_IT_t;

/**
 * @brief SPI中断回调函数类型
 * @param[in] instance SPI实例索引
 * @param[in] it_type 中断类型
 * @param[in] user_data 用户数据指针
 */
typedef void (*SPI_IT_Callback_t)(SPI_Instance_t instance, SPI_IT_t it_type, void *user_data);

/**
 * @brief 使能SPI中断
 * @param[in] instance SPI实例索引
 * @param[in] it_type 中断类型
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_EnableIT(SPI_Instance_t instance, SPI_IT_t it_type);

/**
 * @brief 禁用SPI中断
 * @param[in] instance SPI实例索引
 * @param[in] it_type 中断类型
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_DisableIT(SPI_Instance_t instance, SPI_IT_t it_type);

/**
 * @brief 设置SPI中断回调函数
 * @param[in] instance SPI实例索引
 * @param[in] it_type 中断类型
 * @param[in] callback 回调函数指针，NULL表示禁用回调
 * @param[in] user_data 用户数据指针
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_SetITCallback(SPI_Instance_t instance, SPI_IT_t it_type,
                               SPI_IT_Callback_t callback, void *user_data);

/**
 * @brief SPI非阻塞发送（中断模式）
 * @param[in] instance SPI实例索引
 * @param[in] data 要发送的数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_MasterTransmitIT(SPI_Instance_t instance, const uint8_t *data, uint16_t length);

/**
 * @brief SPI非阻塞接收（中断模式）
 * @param[in] instance SPI实例索引
 * @param[out] data 接收数据缓冲区
 * @param[in] length 要接收的数据长度（字节数）
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_MasterReceiveIT(SPI_Instance_t instance, uint8_t *data, uint16_t length);

/**
 * @brief SPI非阻塞全双工传输（中断模式）
 * @param[in] instance SPI实例索引
 * @param[in] tx_data 要发送的数据缓冲区
 * @param[out] rx_data 接收数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @return SPI_Status_t 错误码
 */
SPI_Status_t SPI_MasterTransmitReceiveIT(SPI_Instance_t instance, const uint8_t *tx_data, uint8_t *rx_data, uint16_t length);

/**
 * @brief 获取SPI发送剩余字节数（中断模式）
 * @param[in] instance SPI实例索引
 * @return uint16_t 剩余字节数
 */
uint16_t SPI_GetTransmitRemaining(SPI_Instance_t instance);

/**
 * @brief 获取SPI接收到的字节数（中断模式）
 * @param[in] instance SPI实例索引
 * @return uint16_t 接收到的字节数
 */
uint16_t SPI_GetReceiveCount(SPI_Instance_t instance);

/**
 * @brief SPI中断服务函数（应在中断服务程序中调用）
 * @param[in] instance SPI实例索引
 */
void SPI_IRQHandler(SPI_Instance_t instance);

#ifdef __cplusplus
}
#endif

#endif /* SPI_HW_H */

