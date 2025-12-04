/**
 * @file modbus_rtu.h
 * @brief ModBusRTU协议栈模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 实现完整的ModBusRTU协议栈，支持主机和从机模式，包含常用功能码（03/06/10/16等）和CRC16校验
 */

#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"

#ifdef CONFIG_MODULE_MODBUS_RTU_ENABLED
#if CONFIG_MODULE_MODBUS_RTU_ENABLED

#include "error_code.h"
#include "uart.h"
#include <stdint.h>

/* ==================== 常量定义 ==================== */

#define MODBUS_RTU_DEFAULT_TIMEOUT_MS  1000  /**< 默认超时时间（毫秒） */
#define MODBUS_RTU_DEFAULT_RETRY_COUNT 3     /**< 默认重试次数 */

/**
 * @brief ModBusRTU错误码
 */
typedef enum {
    ModBusRTU_OK = ERROR_OK,                                    /**< 操作成功 */
    ModBusRTU_ERROR_NOT_IMPLEMENTED = ERROR_BASE_MODBUS_RTU - 99,  /**< 功能未实现（占位空函数） */
    ModBusRTU_ERROR_NULL_PTR = ERROR_BASE_MODBUS_RTU - 1,      /**< 空指针错误 */
    ModBusRTU_ERROR_INVALID_PARAM = ERROR_BASE_MODBUS_RTU - 2,  /**< 参数非法（通用） */
    ModBusRTU_ERROR_INVALID_INSTANCE = ERROR_BASE_MODBUS_RTU - 3,  /**< 无效实例编号 */
    ModBusRTU_ERROR_NOT_INITIALIZED = ERROR_BASE_MODBUS_RTU - 4,  /**< 未初始化 */
    ModBusRTU_ERROR_TIMEOUT = ERROR_BASE_MODBUS_RTU - 5,       /**< 操作超时 */
    ModBusRTU_ERROR_CRC = ERROR_BASE_MODBUS_RTU - 6,            /**< CRC校验错误 */
    ModBusRTU_ERROR_INVALID_RESPONSE = ERROR_BASE_MODBUS_RTU - 7,  /**< 无效响应 */
    ModBusRTU_ERROR_INVALID_ADDRESS = ERROR_BASE_MODBUS_RTU - 8,  /**< 无效地址 */
    ModBusRTU_ERROR_INVALID_FUNCTION_CODE = ERROR_BASE_MODBUS_RTU - 9,  /**< 无效功能码 */
    ModBusRTU_ERROR_EXCEPTION = ERROR_BASE_MODBUS_RTU - 10,     /**< 异常响应 */
} ModBusRTU_Status_t;

/**
 * @brief ModBusRTU功能码枚举
 */
typedef enum {
    ModBusRTU_FUNC_READ_HOLDING_REGISTERS = 0x03,   /**< 读保持寄存器 */
    ModBusRTU_FUNC_WRITE_SINGLE_REGISTER = 0x06,    /**< 写单个寄存器 */
    ModBusRTU_FUNC_WRITE_MULTIPLE_REGISTERS = 0x10, /**< 写多个寄存器 */
} ModBusRTU_FunctionCode_t;

/**
 * @brief ModBusRTU异常码枚举
 */
typedef enum {
    ModBusRTU_EXCEPTION_ILLEGAL_FUNCTION = 0x01,        /**< 非法功能码 */
    ModBusRTU_EXCEPTION_ILLEGAL_DATA_ADDRESS = 0x02,   /**< 非法数据地址 */
    ModBusRTU_EXCEPTION_ILLEGAL_DATA_VALUE = 0x03,     /**< 非法数据值 */
    ModBusRTU_EXCEPTION_SLAVE_DEVICE_FAILURE = 0x04,    /**< 从机设备故障 */
    ModBusRTU_EXCEPTION_ACKNOWLEDGE = 0x05,             /**< 确认 */
    ModBusRTU_EXCEPTION_SLAVE_DEVICE_BUSY = 0x06,       /**< 从机设备忙 */
    ModBusRTU_EXCEPTION_NEGATIVE_ACKNOWLEDGE = 0x07,   /**< 否定确认 */
    ModBusRTU_EXCEPTION_MEMORY_PARITY_ERROR = 0x08,    /**< 内存奇偶校验错误 */
} ModBusRTU_ExceptionCode_t;

/**
 * @brief ModBusRTU配置结构体
 */
typedef struct {
    UART_Instance_t uart_instance;    /**< UART实例索引 */
    uint8_t slave_address;            /**< 从机地址（从机模式使用） */
    uint32_t timeout_ms;              /**< 超时时间（毫秒），0表示使用默认超时 */
    uint8_t retry_count;              /**< 重试次数 */
} ModBusRTU_Config_t;

/* ==================== 主机模式函数 ==================== */

/**
 * @brief 读保持寄存器（功能码03）
 * @param[in] uart_instance UART实例索引
 * @param[in] slave_address 从机地址（1-247）
 * @param[in] start_address 起始寄存器地址
 * @param[in] register_count 寄存器数量（1-125）
 * @param[out] data 接收数据缓冲区（每个寄存器2字节，高字节在前）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return ModBusRTU_Status_t 错误码
 */
ModBusRTU_Status_t ModBusRTU_ReadHoldingRegisters(UART_Instance_t uart_instance, uint8_t slave_address,
                                                   uint16_t start_address, uint16_t register_count,
                                                   uint16_t *data, uint32_t timeout);

/**
 * @brief 写单个寄存器（功能码06）
 * @param[in] uart_instance UART实例索引
 * @param[in] slave_address 从机地址（1-247）
 * @param[in] register_address 寄存器地址
 * @param[in] value 寄存器值（16位）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return ModBusRTU_Status_t 错误码
 */
ModBusRTU_Status_t ModBusRTU_WriteSingleRegister(UART_Instance_t uart_instance, uint8_t slave_address,
                                                  uint16_t register_address, uint16_t value, uint32_t timeout);

/**
 * @brief 写多个寄存器（功能码10/16）
 * @param[in] uart_instance UART实例索引
 * @param[in] slave_address 从机地址（1-247）
 * @param[in] start_address 起始寄存器地址
 * @param[in] register_count 寄存器数量（1-123）
 * @param[in] data 要写入的数据（每个寄存器2字节，高字节在前）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return ModBusRTU_Status_t 错误码
 */
ModBusRTU_Status_t ModBusRTU_WriteMultipleRegisters(UART_Instance_t uart_instance, uint8_t slave_address,
                                                     uint16_t start_address, uint16_t register_count,
                                                     const uint16_t *data, uint32_t timeout);

/* ==================== 从机模式函数 ==================== */

/**
 * @brief 从机初始化
 * @param[in] config 配置结构体指针
 * @return ModBusRTU_Status_t 错误码
 */
ModBusRTU_Status_t ModBusRTU_SlaveInit(const ModBusRTU_Config_t *config);

/**
 * @brief 从机处理请求（轮询方式）
 * @param[out] function_code 接收到的功能码
 * @param[out] start_address 接收到的起始地址
 * @param[out] register_count 接收到的寄存器数量
 * @param[out] data 接收/发送数据缓冲区
 * @param[in,out] data_size 输入时表示缓冲区大小，输出时表示实际数据长度
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return ModBusRTU_Status_t 错误码
 * @note 此函数需要周期性调用，用于接收和处理ModBusRTU请求
 */
ModBusRTU_Status_t ModBusRTU_SlaveProcess(uint8_t *function_code, uint16_t *start_address,
                                          uint16_t *register_count, uint8_t *data,
                                          uint16_t *data_size, uint32_t timeout);

/**
 * @brief 从机发送响应
 * @param[in] uart_instance UART实例索引
 * @param[in] slave_address 从机地址
 * @param[in] function_code 功能码
 * @param[in] data 响应数据缓冲区
 * @param[in] data_length 数据长度（字节数）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return ModBusRTU_Status_t 错误码
 */
ModBusRTU_Status_t ModBusRTU_SlaveSendResponse(UART_Instance_t uart_instance, uint8_t slave_address,
                                                uint8_t function_code, const uint8_t *data,
                                                uint16_t data_length, uint32_t timeout);

/**
 * @brief 从机发送异常响应
 * @param[in] uart_instance UART实例索引
 * @param[in] slave_address 从机地址
 * @param[in] function_code 功能码
 * @param[in] exception_code 异常码
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return ModBusRTU_Status_t 错误码
 */
ModBusRTU_Status_t ModBusRTU_SlaveSendException(UART_Instance_t uart_instance, uint8_t slave_address,
                                                 uint8_t function_code, ModBusRTU_ExceptionCode_t exception_code,
                                                 uint32_t timeout);

/* ==================== 工具函数 ==================== */

/**
 * @brief 计算CRC16校验码
 * @param[in] data 数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @return uint16_t CRC16校验码（低位在前，高位在后）
 */
uint16_t ModBusRTU_CalculateCRC16(const uint8_t *data, uint16_t length);

#endif /* CONFIG_MODULE_MODBUS_RTU_ENABLED */
#endif /* CONFIG_MODULE_MODBUS_RTU_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_RTU_H */

