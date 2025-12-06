/**
 * @file i2c_sw.h
 * @brief 软件I2C驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于GPIO的软件I2C驱动，支持主模式、7位/10位地址、总线扫描等功能
 *          适用于任意GPIO引脚配置，不依赖硬件I2C外设
 */

#ifndef I2C_SW_H
#define I2C_SW_H

#ifdef __cplusplus
extern "C" {
#endif

#include "board.h"
#include "error_code.h"
#include "gpio.h"
#include <stdint.h>

/**
 * @brief 软件I2C错误码
 */
typedef enum {
    SOFT_I2C_OK = ERROR_OK,                                    /**< 操作成功 */
    SOFT_I2C_ERROR_NOT_INITIALIZED = ERROR_BASE_SOFT_I2C - 1, /**< 未初始化 */
    SOFT_I2C_ERROR_INVALID_PARAM = ERROR_BASE_SOFT_I2C - 2,   /**< 参数非法 */
    SOFT_I2C_ERROR_GPIO_FAILED = ERROR_BASE_SOFT_I2C - 3,      /**< GPIO配置失败 */
    SOFT_I2C_ERROR_NACK = ERROR_BASE_SOFT_I2C - 4,            /**< 从机无应答 */
    SOFT_I2C_ERROR_TIMEOUT = ERROR_BASE_SOFT_I2C - 5,          /**< 操作超时 */
    SOFT_I2C_ERROR_BUS_BUSY = ERROR_BASE_SOFT_I2C - 6,        /**< 总线忙 */
} SoftI2C_Status_t;

/**
 * @brief 软件I2C实例索引（用于多I2C支持）
 */
typedef enum {
    SOFT_I2C_INSTANCE_1 = 0,   /**< 软件I2C实例1 */
    SOFT_I2C_INSTANCE_2 = 1,   /**< 软件I2C实例2 */
    SOFT_I2C_INSTANCE_3 = 2,   /**< 软件I2C实例3 */
    SOFT_I2C_INSTANCE_4 = 3,   /**< 软件I2C实例4 */
    SOFT_I2C_INSTANCE_MAX      /**< 最大实例数 */
} SoftI2C_Instance_t;

/**
 * @brief 软件I2C初始化
 * @param[in] instance 软件I2C实例索引
 * @return SoftI2C_Status_t 错误码
 * @note 根据board.h中的配置初始化GPIO引脚
 */
SoftI2C_Status_t I2C_SW_Init(SoftI2C_Instance_t instance);

/**
 * @brief 软件I2C反初始化
 * @param[in] instance 软件I2C实例索引
 * @return SoftI2C_Status_t 错误码
 */
SoftI2C_Status_t SoftI2C_Deinit(SoftI2C_Instance_t instance);

/**
 * @brief 检查软件I2C是否已初始化
 * @param[in] instance 软件I2C实例索引
 * @return uint8_t 1=已初始化，0=未初始化
 */
uint8_t SoftI2C_IsInitialized(SoftI2C_Instance_t instance);

/**
 * @brief 软件I2C主模式发送数据
 * @param[in] instance 软件I2C实例索引
 * @param[in] slave_addr 从机地址（7位地址）
 * @param[in] data 要发送的数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @param[in] timeout_ms 超时时间（毫秒），0表示不超时
 * @return SoftI2C_Status_t 错误码
 * @note 发送格式：START + SLAVE_ADDR(W) + DATA[0] + DATA[1] + ... + STOP
 */
SoftI2C_Status_t SoftI2C_MasterTransmit(SoftI2C_Instance_t instance, uint8_t slave_addr, 
                                         const uint8_t *data, uint16_t length, uint32_t timeout_ms);

/**
 * @brief 软件I2C主模式接收数据
 * @param[in] instance 软件I2C实例索引
 * @param[in] slave_addr 从机地址（7位地址）
 * @param[out] data 接收数据缓冲区
 * @param[in] length 要接收的数据长度（字节数）
 * @param[in] timeout_ms 超时时间（毫秒），0表示不超时
 * @return SoftI2C_Status_t 错误码
 * @note 接收格式：START + SLAVE_ADDR(R) + DATA[0] + DATA[1] + ... + STOP
 */
SoftI2C_Status_t SoftI2C_MasterReceive(SoftI2C_Instance_t instance, uint8_t slave_addr, 
                                        uint8_t *data, uint16_t length, uint32_t timeout_ms);

/**
 * @brief 软件I2C主模式写寄存器（常用封装）
 * @param[in] instance 软件I2C实例索引
 * @param[in] slave_addr 从机地址（7位地址）
 * @param[in] reg_addr 寄存器地址
 * @param[in] reg_value 寄存器值
 * @param[in] timeout_ms 超时时间（毫秒），0表示不超时
 * @return SoftI2C_Status_t 错误码
 * @note 发送格式：START + SLAVE_ADDR(W) + REG_ADDR + REG_VALUE + STOP
 */
SoftI2C_Status_t SoftI2C_MasterWriteReg(SoftI2C_Instance_t instance, uint8_t slave_addr, 
                                         uint8_t reg_addr, uint8_t reg_value, uint32_t timeout_ms);

/**
 * @brief 软件I2C主模式读寄存器（常用封装）
 * @param[in] instance 软件I2C实例索引
 * @param[in] slave_addr 从机地址（7位地址）
 * @param[in] reg_addr 寄存器地址
 * @param[out] reg_value 寄存器值
 * @param[in] timeout_ms 超时时间（毫秒），0表示不超时
 * @return SoftI2C_Status_t 错误码
 * @note 发送格式：START + SLAVE_ADDR(W) + REG_ADDR + RESTART + SLAVE_ADDR(R) + DATA + STOP
 */
SoftI2C_Status_t SoftI2C_MasterReadReg(SoftI2C_Instance_t instance, uint8_t slave_addr, 
                                        uint8_t reg_addr, uint8_t *reg_value, uint32_t timeout_ms);

/**
 * @brief 软件I2C主模式写多个寄存器（常用封装）
 * @param[in] instance 软件I2C实例索引
 * @param[in] slave_addr 从机地址（7位地址）
 * @param[in] reg_addr 起始寄存器地址
 * @param[in] data 要写入的数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @param[in] timeout_ms 超时时间（毫秒），0表示不超时
 * @return SoftI2C_Status_t 错误码
 * @note 发送格式：START + SLAVE_ADDR(W) + REG_ADDR + DATA[0] + DATA[1] + ... + STOP
 */
SoftI2C_Status_t SoftI2C_MasterWriteRegs(SoftI2C_Instance_t instance, uint8_t slave_addr, 
                                          uint8_t reg_addr, const uint8_t *data, uint16_t length, uint32_t timeout_ms);

/**
 * @brief 软件I2C主模式读多个寄存器（常用封装）
 * @param[in] instance 软件I2C实例索引
 * @param[in] slave_addr 从机地址（7位地址）
 * @param[in] reg_addr 起始寄存器地址
 * @param[out] data 接收数据缓冲区
 * @param[in] length 要接收的数据长度（字节数）
 * @param[in] timeout_ms 超时时间（毫秒），0表示不超时
 * @return SoftI2C_Status_t 错误码
 * @note 发送格式：START + SLAVE_ADDR(W) + REG_ADDR + RESTART + SLAVE_ADDR(R) + DATA[0] + DATA[1] + ... + STOP
 */
SoftI2C_Status_t SoftI2C_MasterReadRegs(SoftI2C_Instance_t instance, uint8_t slave_addr, 
                                         uint8_t reg_addr, uint8_t *data, uint16_t length, uint32_t timeout_ms);

/**
 * @brief 软件I2C总线扫描
 * @param[in] instance 软件I2C实例索引
 * @param[out] found_addr 找到的设备地址数组（7位地址）
 * @param[in] max_count 最大扫描数量
 * @param[out] count 实际找到的设备数量
 * @param[in] timeout_ms 每个地址的超时时间（毫秒），0表示使用默认值（10ms）
 * @return SoftI2C_Status_t 错误码
 * @note 扫描地址范围：0x08-0x77（7位地址，排除保留地址）
 */
SoftI2C_Status_t SoftI2C_ScanBus(SoftI2C_Instance_t instance, uint8_t *found_addr, 
                                  uint8_t max_count, uint8_t *count, uint32_t timeout_ms);

/**
 * @brief 软件I2C主模式发送数据（10位地址）
 * @param[in] instance 软件I2C实例索引
 * @param[in] slave_addr 从机地址（10位地址，0x000-0x3FF）
 * @param[in] data 要发送的数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @param[in] timeout_ms 超时时间（毫秒），0表示不超时
 * @return SoftI2C_Status_t 错误码
 */
SoftI2C_Status_t SoftI2C_MasterTransmit10bit(SoftI2C_Instance_t instance, uint16_t slave_addr, 
                                               const uint8_t *data, uint16_t length, uint32_t timeout_ms);

/**
 * @brief 软件I2C主模式接收数据（10位地址）
 * @param[in] instance 软件I2C实例索引
 * @param[in] slave_addr 从机地址（10位地址，0x000-0x3FF）
 * @param[out] data 接收数据缓冲区
 * @param[in] length 要接收的数据长度（字节数）
 * @param[in] timeout_ms 超时时间（毫秒），0表示不超时
 * @return SoftI2C_Status_t 错误码
 */
SoftI2C_Status_t SoftI2C_MasterReceive10bit(SoftI2C_Instance_t instance, uint16_t slave_addr, 
                                             uint8_t *data, uint16_t length, uint32_t timeout_ms);

/**
 * @brief 软件I2C软件复位
 * @param[in] instance 软件I2C实例索引
 * @return SoftI2C_Status_t 错误码
 * @note 用于I2C总线出错时恢复总线状态（发送9个时钟脉冲）
 */
SoftI2C_Status_t SoftI2C_SoftwareReset(SoftI2C_Instance_t instance);

/**
 * @brief 检查软件I2C总线是否忙
 * @param[in] instance 软件I2C实例索引
 * @return uint8_t 1=总线忙，0=总线空闲
 * @note 通过检测SDA线是否被拉低来判断总线是否忙
 */
uint8_t SoftI2C_IsBusBusy(SoftI2C_Instance_t instance);

/**
 * @brief 软件I2C主模式写寄存器（10位地址）
 * @param[in] instance 软件I2C实例索引
 * @param[in] slave_addr 从机地址（10位地址，0x000-0x3FF）
 * @param[in] reg_addr 寄存器地址
 * @param[in] reg_value 寄存器值
 * @param[in] timeout_ms 超时时间（毫秒），0表示不超时
 * @return SoftI2C_Status_t 错误码
 * @note 发送格式：START + SLAVE_ADDR_10BIT(W) + REG_ADDR + REG_VALUE + STOP
 */
SoftI2C_Status_t SoftI2C_MasterWriteReg10bit(SoftI2C_Instance_t instance, uint16_t slave_addr, 
                                              uint8_t reg_addr, uint8_t reg_value, uint32_t timeout_ms);

/**
 * @brief 软件I2C主模式读寄存器（10位地址）
 * @param[in] instance 软件I2C实例索引
 * @param[in] slave_addr 从机地址（10位地址，0x000-0x3FF）
 * @param[in] reg_addr 寄存器地址
 * @param[out] reg_value 寄存器值
 * @param[in] timeout_ms 超时时间（毫秒），0表示不超时
 * @return SoftI2C_Status_t 错误码
 * @note 发送格式：START + SLAVE_ADDR_10BIT(W) + REG_ADDR + RESTART + SLAVE_ADDR_10BIT(R) + DATA + STOP
 */
SoftI2C_Status_t SoftI2C_MasterReadReg10bit(SoftI2C_Instance_t instance, uint16_t slave_addr, 
                                             uint8_t reg_addr, uint8_t *reg_value, uint32_t timeout_ms);

/**
 * @brief 软件I2C配置信息结构体
 */
typedef struct {
    GPIO_TypeDef *scl_port;         /**< SCL引脚端口 */
    uint16_t scl_pin;               /**< SCL引脚号 */
    GPIO_TypeDef *sda_port;         /**< SDA引脚端口 */
    uint16_t sda_pin;               /**< SDA引脚号 */
    uint32_t delay_us;              /**< I2C时序延时（微秒） */
    uint8_t enabled;                /**< 使能标志 */
} SoftI2C_ConfigInfo_t;

/**
 * @brief 获取软件I2C配置信息
 * @param[in] instance 软件I2C实例索引
 * @param[out] config_info 配置信息结构体指针
 * @return SoftI2C_Status_t 错误码
 */
SoftI2C_Status_t SoftI2C_GetConfig(SoftI2C_Instance_t instance, SoftI2C_ConfigInfo_t *config_info);

#ifdef __cplusplus
}
#endif

#endif /* I2C_SW_H */
