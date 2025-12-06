/**
 * @file i2c_hw.h
 * @brief 硬件I2C驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的硬件I2C驱动，支持主模式（轮询/中断/DMA）、7位/10位地址、总线扫描等功能
 */

#ifndef I2C_HW_H
#define I2C_HW_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include <stdint.h>

/* Note: We use I2C_HW_Init instead of I2C_Init to avoid conflict with STM32 standard library's I2C_Init */
/* Standard library: void I2C_Init(I2C_TypeDef *, I2C_InitTypeDef *) */
/* Our function: I2C_Status_t I2C_HW_Init(I2C_Instance_t instance) */

/**
 * @brief I2C错误码
 */
typedef enum {
    I2C_OK = ERROR_OK,                                    /**< 操作成功 */
    I2C_ERROR_NOT_INITIALIZED = ERROR_BASE_I2C - 1,      /**< 未初始化 */
    I2C_ERROR_INVALID_PARAM = ERROR_BASE_I2C - 2,        /**< 参数非法 */
    I2C_ERROR_INVALID_PERIPH = ERROR_BASE_I2C - 3,        /**< 无效的外设 */
    I2C_ERROR_GPIO_FAILED = ERROR_BASE_I2C - 4,          /**< GPIO配置失败 */
    I2C_ERROR_BUSY = ERROR_BASE_I2C - 5,                 /**< I2C总线忙 */
    I2C_ERROR_TIMEOUT = ERROR_BASE_I2C - 6,              /**< 操作超时 */
    I2C_ERROR_NACK = ERROR_BASE_I2C - 7,                 /**< 从机无应答 */
    I2C_ERROR_ARBITRATION_LOST = ERROR_BASE_I2C - 8,     /**< 仲裁丢失 */
    I2C_ERROR_BUS_ERROR = ERROR_BASE_I2C - 9,            /**< 总线错误 */
} I2C_Status_t;

/**
 * @brief I2C实例索引（用于多I2C支持）
 */
typedef enum {
    I2C_INSTANCE_1 = 0,   /**< I2C1实例 */
    I2C_INSTANCE_2 = 1,   /**< I2C2实例 */
    I2C_INSTANCE_MAX      /**< 最大实例数 */
} I2C_Instance_t;

/* Include board.h, which includes stm32f10x.h and stm32f10x_i2c.h */
#include "board.h"

/**
 * @brief 硬件I2C初始化
 * @param[in] instance I2C实例索引（I2C_INSTANCE_1或I2C_INSTANCE_2）
 * @return I2C_Status_t 错误码
 * @note 根据board.h中的配置初始化I2C外设和GPIO引脚
 * @note 使用I2C_HW_Init而非I2C_Init，避免与STM32标准库的I2C_Init函数冲突
 */
I2C_Status_t I2C_HW_Init(I2C_Instance_t instance);

/**
 * @brief I2C反初始化
 * @param[in] instance I2C实例索引
 * @return I2C_Status_t 错误码
 */
I2C_Status_t I2C_Deinit(I2C_Instance_t instance);

/**
 * @brief I2C主模式发送数据
 * @param[in] instance I2C实例索引
 * @param[in] slave_addr 从机地址（7位地址）
 * @param[in] data 要发送的数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return I2C_Status_t 错误码
 * @note 发送格式：START + SLAVE_ADDR(W) + DATA[0] + DATA[1] + ... + STOP
 */
I2C_Status_t I2C_MasterTransmit(I2C_Instance_t instance, uint8_t slave_addr, 
                                 const uint8_t *data, uint16_t length, uint32_t timeout);

/**
 * @brief I2C主模式接收数据
 * @param[in] instance I2C实例索引
 * @param[in] slave_addr 从机地址（7位地址）
 * @param[out] data 接收数据缓冲区
 * @param[in] length 要接收的数据长度（字节数）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return I2C_Status_t 错误码
 * @note 接收格式：START + SLAVE_ADDR(R) + DATA[0] + DATA[1] + ... + STOP
 */
I2C_Status_t I2C_MasterReceive(I2C_Instance_t instance, uint8_t slave_addr, 
                                uint8_t *data, uint16_t length, uint32_t timeout);

/**
 * @brief I2C主模式写寄存器（常用封装）
 * @param[in] instance I2C实例索引
 * @param[in] slave_addr 从机地址（7位地址）
 * @param[in] reg_addr 寄存器地址
 * @param[in] reg_value 寄存器值
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return I2C_Status_t 错误码
 * @note 发送格式：START + SLAVE_ADDR(W) + REG_ADDR + REG_VALUE + STOP
 */
I2C_Status_t I2C_MasterWriteReg(I2C_Instance_t instance, uint8_t slave_addr, 
                                 uint8_t reg_addr, uint8_t reg_value, uint32_t timeout);

/**
 * @brief I2C主模式读寄存器（常用封装）
 * @param[in] instance I2C实例索引
 * @param[in] slave_addr 从机地址（7位地址）
 * @param[in] reg_addr 寄存器地址
 * @param[out] reg_value 寄存器值
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return I2C_Status_t 错误码
 * @note 发送格式：START + SLAVE_ADDR(W) + REG_ADDR + RESTART + SLAVE_ADDR(R) + DATA + STOP
 */
I2C_Status_t I2C_MasterReadReg(I2C_Instance_t instance, uint8_t slave_addr, 
                                 uint8_t reg_addr, uint8_t *reg_value, uint32_t timeout);

/**
 * @brief I2C主模式写多个寄存器（常用封装）
 * @param[in] instance I2C实例索引
 * @param[in] slave_addr 从机地址（7位地址）
 * @param[in] reg_addr 起始寄存器地址
 * @param[in] data 要写入的数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return I2C_Status_t 错误码
 * @note 发送格式：START + SLAVE_ADDR(W) + REG_ADDR + DATA[0] + DATA[1] + ... + STOP
 */
I2C_Status_t I2C_MasterWriteRegs(I2C_Instance_t instance, uint8_t slave_addr, 
                                  uint8_t reg_addr, const uint8_t *data, uint16_t length, uint32_t timeout);

/**
 * @brief I2C主模式读多个寄存器（常用封装）
 * @param[in] instance I2C实例索引
 * @param[in] slave_addr 从机地址（7位地址）
 * @param[in] reg_addr 起始寄存器地址
 * @param[out] data 接收数据缓冲区
 * @param[in] length 要接收的数据长度（字节数）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return I2C_Status_t 错误码
 * @note 发送格式：START + SLAVE_ADDR(W) + REG_ADDR + RESTART + SLAVE_ADDR(R) + DATA[0] + DATA[1] + ... + STOP
 */
I2C_Status_t I2C_MasterReadRegs(I2C_Instance_t instance, uint8_t slave_addr, 
                                  uint8_t reg_addr, uint8_t *data, uint16_t length, uint32_t timeout);

/**
 * @brief 检查I2C是否已初始化
 * @param[in] instance I2C实例索引
 * @return uint8_t 1=已初始化，0=未初始化
 */
uint8_t I2C_IsInitialized(I2C_Instance_t instance);

/**
 * @brief 获取I2C外设指针
 * @param[in] instance I2C实例索引
 * @return I2C_TypeDef* I2C外设指针，失败返回NULL
 */
I2C_TypeDef* I2C_GetPeriph(I2C_Instance_t instance);

/**
 * @brief I2C软件复位
 * @param[in] instance I2C实例索引
 * @return I2C_Status_t 错误码
 * @note 用于I2C总线出错时恢复总线状态
 */
I2C_Status_t I2C_SoftwareReset(I2C_Instance_t instance);

/**
 * @brief 检查I2C总线是否忙
 * @param[in] instance I2C实例索引
 * @return uint8_t 1=总线忙，0=总线空闲
 */
uint8_t I2C_IsBusBusy(I2C_Instance_t instance);

/**
 * @brief I2C总线扫描
 * @param[in] instance I2C实例索引
 * @param[out] found_addr 找到的设备地址数组（7位地址）
 * @param[in] max_count 最大扫描数量
 * @param[out] count 实际找到的设备数量
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return I2C_Status_t 错误码
 * @note 扫描地址范围：0x08-0x77（7位地址，排除保留地址）
 */
I2C_Status_t I2C_ScanBus(I2C_Instance_t instance, uint8_t *found_addr, uint8_t max_count, uint8_t *count, uint32_t timeout);

/**
 * @brief I2C主模式发送数据（10位地址）
 * @param[in] instance I2C实例索引
 * @param[in] slave_addr 从机地址（10位地址，0x000-0x3FF）
 * @param[in] data 要发送的数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return I2C_Status_t 错误码
 */
I2C_Status_t I2C_MasterTransmit10bit(I2C_Instance_t instance, uint16_t slave_addr, 
                                      const uint8_t *data, uint16_t length, uint32_t timeout);

/**
 * @brief I2C主模式接收数据（10位地址）
 * @param[in] instance I2C实例索引
 * @param[in] slave_addr 从机地址（10位地址，0x000-0x3FF）
 * @param[out] data 接收数据缓冲区
 * @param[in] length 要接收的数据长度（字节数）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return I2C_Status_t 错误码
 */
I2C_Status_t I2C_MasterReceive10bit(I2C_Instance_t instance, uint16_t slave_addr, 
                                     uint8_t *data, uint16_t length, uint32_t timeout);

/**
 * @brief I2C配置信息结构体
 */
typedef struct {
    I2C_TypeDef *i2c_periph;        /**< I2C外设指针 */
    uint32_t clock_speed;            /**< 时钟速度（Hz） */
    uint16_t own_address;            /**< 本设备地址 */
    uint8_t address_mode;            /**< 地址模式：0=7位，1=10位 */
    uint8_t enabled;                 /**< 使能标志 */
} I2C_ConfigInfo_t;

/**
 * @brief 获取I2C配置信息
 * @param[in] instance I2C实例索引
 * @param[out] config_info 配置信息结构体指针
 * @return I2C_Status_t 错误码
 */
I2C_Status_t I2C_GetConfig(I2C_Instance_t instance, I2C_ConfigInfo_t *config_info);

/* ========== 从模式功能 ========== */

/**
 * @brief I2C从模式回调函数类型
 * @param[in] instance I2C实例索引
 * @param[in] event 从模式事件
 * @param[in] data 数据指针（接收时为接收数据，发送时为NULL）
 * @param[in] user_data 用户数据指针
 */
typedef void (*I2C_SlaveCallback_t)(I2C_Instance_t instance, uint32_t event, uint8_t data, void *user_data);

/**
 * @brief I2C从模式初始化
 * @param[in] instance I2C实例索引
 * @param[in] slave_address 从机地址（7位地址）
 * @param[in] callback 从模式事件回调函数
 * @param[in] user_data 用户数据指针
 * @return I2C_Status_t 错误码
 */
I2C_Status_t I2C_SlaveInit(I2C_Instance_t instance, uint8_t slave_address,
                           I2C_SlaveCallback_t callback, void *user_data);

/**
 * @brief I2C从模式反初始化
 * @param[in] instance I2C实例索引
 * @return I2C_Status_t 错误码
 */
I2C_Status_t I2C_SlaveDeinit(I2C_Instance_t instance);

/**
 * @brief I2C从模式发送数据
 * @param[in] instance I2C实例索引
 * @param[in] data 要发送的数据
 * @return I2C_Status_t 错误码
 * @note 应在从模式发送事件回调中调用
 */
I2C_Status_t I2C_SlaveTransmit(I2C_Instance_t instance, uint8_t data);

/**
 * @brief I2C从模式接收数据
 * @param[in] instance I2C实例索引
 * @param[out] data 接收的数据
 * @return I2C_Status_t 错误码
 * @note 应在从模式接收事件回调中调用
 */
I2C_Status_t I2C_SlaveReceive(I2C_Instance_t instance, uint8_t *data);

/**
 * @brief I2C传输模式
 */
typedef enum {
    I2C_MODE_POLLING = 0,   /**< 轮询模式 */
    I2C_MODE_INTERRUPT = 1, /**< 中断模式 */
    I2C_MODE_DMA = 2        /**< DMA模式 */
} I2C_TransferMode_t;

/**
 * @brief I2C中断回调函数类型
 */
typedef void (*I2C_Callback_t)(I2C_Instance_t instance, I2C_Status_t status);

/**
 * @brief 设置I2C传输模式
 * @param[in] instance I2C实例索引
 * @param[in] mode 传输模式
 * @return I2C_Status_t 错误码
 * @note 当前版本仅支持轮询模式，中断和DMA模式为预留接口
 */
I2C_Status_t I2C_SetTransferMode(I2C_Instance_t instance, I2C_TransferMode_t mode);

/**
 * @brief 设置I2C中断回调函数
 * @param[in] instance I2C实例索引
 * @param[in] callback 回调函数指针，NULL表示禁用回调
 * @return I2C_Status_t 错误码
 * @note 当前版本为预留接口
 */
I2C_Status_t I2C_SetCallback(I2C_Instance_t instance, I2C_Callback_t callback);

/**
 * @brief I2C主模式发送数据（DMA模式）
 * @param[in] instance I2C实例索引
 * @param[in] slave_addr 从机地址（7位地址）
 * @param[in] data 要发送的数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @return I2C_Status_t 错误码
 * @note 当前版本为预留接口，需要配置DMA通道
 */
I2C_Status_t I2C_MasterTransmitDMA(I2C_Instance_t instance, uint8_t slave_addr, 
                                    const uint8_t *data, uint16_t length);

/**
 * @brief I2C主模式接收数据（DMA模式）
 * @param[in] instance I2C实例索引
 * @param[in] slave_addr 从机地址（7位地址）
 * @param[out] data 接收数据缓冲区
 * @param[in] length 要接收的数据长度（字节数）
 * @return I2C_Status_t 错误码
 * @note 当前版本为预留接口，需要配置DMA通道
 */
I2C_Status_t I2C_MasterReceiveDMA(I2C_Instance_t instance, uint8_t slave_addr, 
                                   uint8_t *data, uint16_t length);

/**
 * @brief I2C主模式发送数据（中断模式）
 * @param[in] instance I2C实例索引
 * @param[in] slave_addr 从机地址（7位地址）
 * @param[in] data 要发送的数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @return I2C_Status_t 错误码
 * @note 需要先调用I2C_SetTransferMode设置为中断模式
 */
I2C_Status_t I2C_MasterTransmitIT(I2C_Instance_t instance, uint8_t slave_addr, 
                                  const uint8_t *data, uint16_t length);

/**
 * @brief I2C主模式接收数据（中断模式）
 * @param[in] instance I2C实例索引
 * @param[in] slave_addr 从机地址（7位地址）
 * @param[out] data 接收数据缓冲区
 * @param[in] length 要接收的数据长度（字节数）
 * @return I2C_Status_t 错误码
 * @note 需要先调用I2C_SetTransferMode设置为中断模式
 */
I2C_Status_t I2C_MasterReceiveIT(I2C_Instance_t instance, uint8_t slave_addr, 
                                 uint8_t *data, uint16_t length);

/* ========== SMBus/PEC功能 ========== */

/**
 * @brief I2C PEC位置枚举
 */
typedef enum {
    I2C_PEC_POSITION_NEXT = 0,     /**< PEC在下一个字节 */
    I2C_PEC_POSITION_CURRENT = 1,   /**< PEC在当前字节 */
} I2C_PECPosition_t;

/**
 * @brief I2C SMBus Alert引脚电平枚举
 */
typedef enum {
    I2C_SMBUS_ALERT_LOW = 0,        /**< SMBus Alert引脚低电平 */
    I2C_SMBUS_ALERT_HIGH = 1,       /**< SMBus Alert引脚高电平 */
} I2C_SMBusAlert_t;

/**
 * @brief 使能I2C PEC计算
 * @param[in] instance I2C实例索引
 * @return I2C_Status_t 错误码
 */
I2C_Status_t I2C_EnablePEC(I2C_Instance_t instance);

/**
 * @brief 禁用I2C PEC计算
 * @param[in] instance I2C实例索引
 * @return I2C_Status_t 错误码
 */
I2C_Status_t I2C_DisablePEC(I2C_Instance_t instance);

/**
 * @brief 使能I2C PEC传输
 * @param[in] instance I2C实例索引
 * @return I2C_Status_t 错误码
 */
I2C_Status_t I2C_EnablePECTransmission(I2C_Instance_t instance);

/**
 * @brief 禁用I2C PEC传输
 * @param[in] instance I2C实例索引
 * @return I2C_Status_t 错误码
 */
I2C_Status_t I2C_DisablePECTransmission(I2C_Instance_t instance);

/**
 * @brief 配置I2C PEC位置
 * @param[in] instance I2C实例索引
 * @param[in] position PEC位置
 * @return I2C_Status_t 错误码
 */
I2C_Status_t I2C_ConfigPECPosition(I2C_Instance_t instance, I2C_PECPosition_t position);

/**
 * @brief 获取I2C PEC值
 * @param[in] instance I2C实例索引
 * @return uint8_t PEC值
 */
uint8_t I2C_HW_GetPEC(I2C_Instance_t instance);

/**
 * @brief 配置I2C SMBus Alert引脚
 * @param[in] instance I2C实例索引
 * @param[in] alert_level SMBus Alert引脚电平
 * @return I2C_Status_t 错误码
 */
I2C_Status_t I2C_ConfigSMBusAlert(I2C_Instance_t instance, I2C_SMBusAlert_t alert_level);

#ifdef __cplusplus
}
#endif

#endif /* I2C_HW_H */
