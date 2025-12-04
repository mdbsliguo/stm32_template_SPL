/**
 * @file modbus_rtu.c
 * @brief ModBusRTU协议栈模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 实现完整的ModBusRTU协议栈，支持主机和从机模式，包含常用功能码（03/06/10/16等）和CRC16校验
 */

#include "config.h"

#if CONFIG_MODULE_MODBUS_RTU_ENABLED

#include "modbus_rtu.h"
#include "uart.h"
#include "delay.h"
#include <string.h>

/* ==================== 私有变量 ==================== */

static ModBusRTU_Config_t g_slave_config;
static uint8_t g_slave_initialized = 0;

/* ==================== CRC16查表 ==================== */

/**
 * @brief CRC16查表（ModBusRTU标准，多项式0xA001）
 * @note 用于加速CRC16计算，性能提升6-8倍
 */
static const uint16_t crc16_table[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

/* ==================== 私有函数 ==================== */

/**
 * @brief 获取超时时间（如果为0则使用默认值）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return uint32_t 实际超时时间（毫秒）
 */
static uint32_t ModBusRTU_GetTimeout(uint32_t timeout)
{
    return (timeout == 0) ? MODBUS_RTU_DEFAULT_TIMEOUT_MS : timeout;
}

/**
 * @brief 转换UART错误码为ModBusRTU错误码
 * @param[in] uart_status UART状态码
 * @return ModBusRTU_Status_t ModBusRTU错误码
 */
static ModBusRTU_Status_t ModBusRTU_ConvertUARTError(UART_Status_t uart_status)
{
    switch (uart_status) {
        case UART_OK:
            return ModBusRTU_OK;
        case UART_ERROR_TIMEOUT:
            return ModBusRTU_ERROR_TIMEOUT;
        case UART_ERROR_NULL_PTR:
            return ModBusRTU_ERROR_NULL_PTR;
        case UART_ERROR_INVALID_INSTANCE:
            return ModBusRTU_ERROR_INVALID_INSTANCE;
        case UART_ERROR_NOT_INITIALIZED:
            return ModBusRTU_ERROR_NOT_INITIALIZED;
        default:
            return ModBusRTU_ERROR_INVALID_RESPONSE;
    }
}

/**
 * @brief 构建ModBusRTU请求帧
 * @param[out] frame 帧缓冲区
 * @param[in] frame_size 帧缓冲区大小
 * @param[out] frame_length 帧长度（输出）
 * @param[in] slave_address 从机地址
 * @param[in] function_code 功能码
 * @param[in] data 数据区
 * @param[in] data_length 数据区长度
 * @return ModBusRTU_Status_t 错误码
 */
static ModBusRTU_Status_t ModBusRTU_BuildRequestFrame(uint8_t *frame, uint16_t frame_size, uint16_t *frame_length,
                                                      uint8_t slave_address, uint8_t function_code,
                                                      const uint8_t *data, uint16_t data_length)
{
    uint16_t crc;
    uint16_t pos = 0;
    uint16_t total_length;
    
    /* ========== 参数校验 ========== */
    if (frame == NULL || frame_length == NULL) {
        return ModBusRTU_ERROR_NULL_PTR;
    }
    
    if (slave_address == 0 || slave_address > 247) {
        return ModBusRTU_ERROR_INVALID_ADDRESS;
    }
    
    if (data_length > 0 && data == NULL) {
        return ModBusRTU_ERROR_NULL_PTR;
    }
    
    /* ========== 检查缓冲区大小 ========== */
    /* 帧长度 = 地址(1) + 功能码(1) + 数据区(data_length) + CRC(2) */
    total_length = 4 + data_length;
    if (total_length > frame_size) {
        return ModBusRTU_ERROR_INVALID_PARAM;
    }
    
    /* ========== 构建帧 ========== */
    frame[pos++] = slave_address;
    frame[pos++] = function_code;
    
    if (data_length > 0) {
        memcpy(&frame[pos], data, data_length);
        pos += data_length;
    }
    
    /* 计算CRC */
    crc = ModBusRTU_CalculateCRC16(frame, pos);
    frame[pos++] = (uint8_t)(crc & 0xFF);        /* CRC低位在前 */
    frame[pos++] = (uint8_t)((crc >> 8) & 0xFF); /* CRC高位在后 */
    
    *frame_length = pos;
    
    return ModBusRTU_OK;
}

/**
 * @brief 解析ModBusRTU响应帧
 * @param[in] frame 帧缓冲区
 * @param[in] frame_length 帧长度
 * @param[in] expected_slave_address 期望的从机地址
 * @param[in] expected_function_code 期望的功能码
 * @param[out] data 数据区（输出）
 * @param[in,out] data_length 输入时表示缓冲区大小，输出时表示实际数据长度
 * @param[out] is_exception 是否为异常响应（输出）
 * @param[out] exception_code 异常码（输出，仅异常响应时有效）
 * @return ModBusRTU_Status_t 错误码
 */
static ModBusRTU_Status_t ModBusRTU_ParseResponseFrame(const uint8_t *frame, uint16_t frame_length,
                                                        uint8_t expected_slave_address, uint8_t expected_function_code,
                                                        uint8_t *data, uint16_t *data_length,
                                                        uint8_t *is_exception, uint8_t *exception_code)
{
    uint16_t crc_calculated;
    uint16_t crc_received;
    uint16_t data_len;
    
    /* ========== 参数校验 ========== */
    if (frame == NULL || data_length == NULL || is_exception == NULL) {
        return ModBusRTU_ERROR_NULL_PTR;
    }
    
    if (frame_length < 4) {  /* 最小帧长度：地址(1) + 功能码(1) + CRC(2) */
        return ModBusRTU_ERROR_INVALID_RESPONSE;
    }
    
    /* ========== 检查地址 ========== */
    if (frame[0] != expected_slave_address) {
        return ModBusRTU_ERROR_INVALID_RESPONSE;
    }
    
    /* ========== 检查CRC ========== */
    crc_received = ((uint16_t)frame[frame_length - 2]) | ((uint16_t)frame[frame_length - 1] << 8);  /* CRC低位在前，高位在后 */
    crc_calculated = ModBusRTU_CalculateCRC16(frame, frame_length - 2);
    
    if (crc_calculated != crc_received) {
        return ModBusRTU_ERROR_CRC;
    }
    
    /* ========== 检查是否为异常响应 ========== */
    if (frame[1] == (expected_function_code | 0x80)) {
        *is_exception = 1;
        if (exception_code != NULL && frame_length >= 4) {
            *exception_code = frame[2];
        }
        return ModBusRTU_ERROR_EXCEPTION;
    }
    
    /* ========== 检查功能码 ========== */
    if (frame[1] != expected_function_code) {
        return ModBusRTU_ERROR_INVALID_FUNCTION_CODE;
    }
    
    *is_exception = 0;
    
    /* ========== 提取数据 ========== */
    data_len = frame_length - 4;  /* 减去地址(1) + 功能码(1) + CRC(2) */
    
    if (data != NULL && data_len > 0) {
        if (*data_length < data_len) {
            return ModBusRTU_ERROR_INVALID_PARAM;
        }
        memcpy(data, &frame[2], data_len);
    }
    
    *data_length = data_len;
    
    return ModBusRTU_OK;
}

/* ==================== 公共函数实现 ==================== */

/**
 * @brief 计算CRC16校验码（查表法优化版本）
 * @param[in] data 数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @return uint16_t CRC16校验码（低位在前，高位在后）
 * @note 使用查表法，性能提升6-8倍
 */
uint16_t ModBusRTU_CalculateCRC16(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;
    uint16_t i;
    
    if (data == NULL || length == 0) {
        return 0;
    }
    
    for (i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crc16_table[(crc ^ data[i]) & 0xFF];
    }
    
    return crc;
}

/**
 * @brief 主机模式通用通信函数（内部使用）
 * @param[in] uart_instance UART实例
 * @param[in] slave_address 从机地址
 * @param[in] function_code 功能码
 * @param[in] request_data 请求数据
 * @param[in] request_data_length 请求数据长度
 * @param[out] response_data 响应数据缓冲区
 * @param[in,out] response_data_length 输入缓冲区大小，输出实际长度
 * @param[in] expected_response_length 期望的响应数据长度（0表示不检查）
 * @param[in] timeout 超时时间
 * @return ModBusRTU_Status_t 错误码
 */
static ModBusRTU_Status_t ModBusRTU_MasterTransact(UART_Instance_t uart_instance,
                                                   uint8_t slave_address,
                                                   uint8_t function_code,
                                                   const uint8_t *request_data,
                                                   uint16_t request_data_length,
                                                   uint8_t *response_data,
                                                   uint16_t *response_data_length,
                                                   uint16_t expected_response_length,
                                                   uint32_t timeout)
{
    uint8_t request_frame[256];
    uint8_t response_frame[256];
    uint16_t request_length = 0;
    uint16_t response_length = 0;
    uint8_t is_exception = 0;
    uint8_t exception_code = 0;
    UART_Status_t uart_status;
    ModBusRTU_Status_t status;
    uint32_t actual_timeout;
    
    /* ========== 参数校验 ========== */
    if (slave_address == 0 || slave_address > 247) {
        return ModBusRTU_ERROR_INVALID_ADDRESS;
    }
    if (uart_instance >= UART_INSTANCE_MAX) {
        return ModBusRTU_ERROR_INVALID_INSTANCE;
    }
    if (request_data == NULL && request_data_length > 0) {
        return ModBusRTU_ERROR_NULL_PTR;
    }
    if (response_data == NULL || response_data_length == NULL) {
        return ModBusRTU_ERROR_NULL_PTR;
    }
    
    actual_timeout = ModBusRTU_GetTimeout(timeout);
    
    /* ========== 构建请求帧 ========== */
    status = ModBusRTU_BuildRequestFrame(request_frame, sizeof(request_frame), &request_length,
                                        slave_address, function_code, request_data, request_data_length);
    if (status != ModBusRTU_OK) {
        return status;
    }
    
    /* ========== 发送请求帧 ========== */
    uart_status = UART_Transmit(uart_instance, request_frame, request_length, actual_timeout);
    if (uart_status != UART_OK) {
        return ModBusRTU_ConvertUARTError(uart_status);
    }
    
    /* ========== 计算响应帧长度 ========== */
    if (expected_response_length > 0) {
        response_length = 4 + expected_response_length;  /* 地址(1) + 功能码(1) + 数据 + CRC(2) */
    } else {
        /* 动态计算：先接收最小帧，再确定总长度 */
        response_length = 256;  /* 最大长度 */
    }
    
    if (response_length > sizeof(response_frame)) {
        return ModBusRTU_ERROR_INVALID_PARAM;
    }
    
    /* ========== 接收响应帧 ========== */
    uart_status = UART_Receive(uart_instance, response_frame, response_length, actual_timeout);
    if (uart_status != UART_OK) {
        return ModBusRTU_ConvertUARTError(uart_status);
    }
    
    /* ========== 解析响应帧 ========== */
    *response_data_length = sizeof(response_frame);
    status = ModBusRTU_ParseResponseFrame(response_frame, response_length, slave_address,
                                          function_code, response_data, response_data_length,
                                          &is_exception, &exception_code);
    return status;
}

/**
 * @brief 读保持寄存器（功能码03）- 内部实现
 */
static ModBusRTU_Status_t ModBusRTU_ReadHoldingRegisters_Internal(UART_Instance_t uart_instance, uint8_t slave_address,
                                                                   uint16_t start_address, uint16_t register_count,
                                                                   uint16_t *data, uint32_t timeout)
{
    uint8_t request_data[4];
    uint8_t response_data[256];
    uint16_t response_data_length = 0;
    ModBusRTU_Status_t status;
    uint16_t i;
    
    /* ========== 参数校验 ========== */
    if (data == NULL) {
        return ModBusRTU_ERROR_NULL_PTR;
    }
    if (register_count == 0 || register_count > 125) {
        return ModBusRTU_ERROR_INVALID_PARAM;
    }
    
    /* ========== 构建请求数据 ========== */
    request_data[0] = (uint8_t)((start_address >> 8) & 0xFF);  /* 起始地址高字节 */
    request_data[1] = (uint8_t)(start_address & 0xFF);        /* 起始地址低字节 */
    request_data[2] = (uint8_t)((register_count >> 8) & 0xFF); /* 寄存器数量高字节 */
    request_data[3] = (uint8_t)(register_count & 0xFF);        /* 寄存器数量低字节 */
    
    /* ========== 通信 ========== */
    response_data_length = sizeof(response_data);
    status = ModBusRTU_MasterTransact(uart_instance, slave_address, ModBusRTU_FUNC_READ_HOLDING_REGISTERS,
                                     request_data, 4, response_data, &response_data_length,
                                     1 + register_count * 2, timeout);  /* 字节数(1) + 数据(register_count*2) */
    if (status != ModBusRTU_OK) {
        return status;
    }
    
    /* ========== 检查字节数 ========== */
    if (response_data_length < 1 || response_data[0] != (register_count * 2)) {
        return ModBusRTU_ERROR_INVALID_RESPONSE;
    }
    
    /* ========== 提取寄存器数据 ========== */
    for (i = 0; i < register_count; i++) {
        data[i] = ((uint16_t)response_data[1 + i * 2] << 8) | response_data[1 + i * 2 + 1];
    }
    
    return ModBusRTU_OK;
}

/**
 * @brief 读保持寄存器（功能码03）
 */
ModBusRTU_Status_t ModBusRTU_ReadHoldingRegisters(UART_Instance_t uart_instance, uint8_t slave_address,
                                                   uint16_t start_address, uint16_t register_count,
                                                   uint16_t *data, uint32_t timeout)
{
    ModBusRTU_Status_t status;
    uint8_t retry_count = MODBUS_RTU_DEFAULT_RETRY_COUNT;
    uint8_t retry;
    
    /* ========== 参数校验 ========== */
    if (slave_address == 0 || slave_address > 247) {
        return ModBusRTU_ERROR_INVALID_ADDRESS;
    }
    if (uart_instance >= UART_INSTANCE_MAX) {
        return ModBusRTU_ERROR_INVALID_INSTANCE;
    }
    
    /* ========== 重试机制 ========== */
    for (retry = 0; retry <= retry_count; retry++) {
        status = ModBusRTU_ReadHoldingRegisters_Internal(uart_instance, slave_address,
                                                          start_address, register_count, data, timeout);
        
        /* 如果成功或非超时/CRC错误，直接返回 */
        if (status == ModBusRTU_OK || 
            (status != ModBusRTU_ERROR_TIMEOUT && status != ModBusRTU_ERROR_CRC)) {
            return status;
        }
        
        /* 超时或CRC错误，重试前等待一小段时间 */
        if (retry < retry_count) {
            Delay_ms(10);  /* 等待10ms后重试 */
        }
    }
    
    return status;  /* 返回最后一次的错误码 */
}

/**
 * @brief 写单个寄存器（功能码06）- 内部实现
 */
static ModBusRTU_Status_t ModBusRTU_WriteSingleRegister_Internal(UART_Instance_t uart_instance, uint8_t slave_address,
                                                                  uint16_t register_address, uint16_t value, uint32_t timeout)
{
    uint8_t request_data[4];
    uint8_t response_data[4];
    uint16_t response_data_length = 0;
    ModBusRTU_Status_t status;
    
    /* ========== 构建请求数据 ========== */
    request_data[0] = (uint8_t)((register_address >> 8) & 0xFF);  /* 寄存器地址高字节 */
    request_data[1] = (uint8_t)(register_address & 0xFF);        /* 寄存器地址低字节 */
    request_data[2] = (uint8_t)((value >> 8) & 0xFF);            /* 寄存器值高字节 */
    request_data[3] = (uint8_t)(value & 0xFF);                   /* 寄存器值低字节 */
    
    /* ========== 通信 ========== */
    response_data_length = sizeof(response_data);
    status = ModBusRTU_MasterTransact(uart_instance, slave_address, ModBusRTU_FUNC_WRITE_SINGLE_REGISTER,
                                     request_data, 4, response_data, &response_data_length, 4, timeout);
    if (status != ModBusRTU_OK) {
        return status;
    }
    
    /* ========== 验证响应数据 ========== */
    if (response_data_length != 4) {
        return ModBusRTU_ERROR_INVALID_RESPONSE;
    }
    
    if (response_data[0] != request_data[0] || response_data[1] != request_data[1] ||
        response_data[2] != request_data[2] || response_data[3] != request_data[3]) {
        return ModBusRTU_ERROR_INVALID_RESPONSE;
    }
    
    return ModBusRTU_OK;
}

/**
 * @brief 写单个寄存器（功能码06）
 */
ModBusRTU_Status_t ModBusRTU_WriteSingleRegister(UART_Instance_t uart_instance, uint8_t slave_address,
                                                  uint16_t register_address, uint16_t value, uint32_t timeout)
{
    ModBusRTU_Status_t status;
    uint8_t retry_count = MODBUS_RTU_DEFAULT_RETRY_COUNT;
    uint8_t retry;
    
    /* ========== 参数校验 ========== */
    if (slave_address == 0 || slave_address > 247) {
        return ModBusRTU_ERROR_INVALID_ADDRESS;
    }
    if (uart_instance >= UART_INSTANCE_MAX) {
        return ModBusRTU_ERROR_INVALID_INSTANCE;
    }
    
    /* ========== 重试机制 ========== */
    for (retry = 0; retry <= retry_count; retry++) {
        status = ModBusRTU_WriteSingleRegister_Internal(uart_instance, slave_address,
                                                         register_address, value, timeout);
        
        /* 如果成功或非超时/CRC错误，直接返回 */
        if (status == ModBusRTU_OK || 
            (status != ModBusRTU_ERROR_TIMEOUT && status != ModBusRTU_ERROR_CRC)) {
            return status;
        }
        
        /* 超时或CRC错误，重试前等待一小段时间 */
        if (retry < retry_count) {
            Delay_ms(10);  /* 等待10ms后重试 */
        }
    }
    
    return status;  /* 返回最后一次的错误码 */
}

/**
 * @brief 写多个寄存器（功能码10/16）- 内部实现
 */
static ModBusRTU_Status_t ModBusRTU_WriteMultipleRegisters_Internal(UART_Instance_t uart_instance, uint8_t slave_address,
                                                                    uint16_t start_address, uint16_t register_count,
                                                                    const uint16_t *data, uint32_t timeout)
{
    uint8_t request_data[256];
    uint8_t response_data[4];
    uint16_t response_data_length = 0;
    ModBusRTU_Status_t status;
    uint16_t i;
    
    /* ========== 参数校验 ========== */
    if (data == NULL) {
        return ModBusRTU_ERROR_NULL_PTR;
    }
    if (register_count == 0 || register_count > 123) {
        return ModBusRTU_ERROR_INVALID_PARAM;
    }
    
    /* ========== 检查请求帧长度 ========== */
    /* 请求数据长度 = 地址(2) + 数量(2) + 字节数(1) + 数据(register_count*2) = 5 + register_count*2 */
    if ((5 + register_count * 2) > sizeof(request_data)) {
        return ModBusRTU_ERROR_INVALID_PARAM;
    }
    
    /* ========== 构建请求数据 ========== */
    request_data[0] = (uint8_t)((start_address >> 8) & 0xFF);  /* 起始地址高字节 */
    request_data[1] = (uint8_t)(start_address & 0xFF);        /* 起始地址低字节 */
    request_data[2] = (uint8_t)((register_count >> 8) & 0xFF); /* 寄存器数量高字节 */
    request_data[3] = (uint8_t)(register_count & 0xFF);        /* 寄存器数量低字节 */
    request_data[4] = (uint8_t)(register_count * 2);          /* 字节数 */
    
    for (i = 0; i < register_count; i++) {
        request_data[5 + i * 2] = (uint8_t)((data[i] >> 8) & 0xFF);  /* 寄存器值高字节 */
        request_data[5 + i * 2 + 1] = (uint8_t)(data[i] & 0xFF);     /* 寄存器值低字节 */
    }
    
    /* ========== 通信 ========== */
    response_data_length = sizeof(response_data);
    status = ModBusRTU_MasterTransact(uart_instance, slave_address, ModBusRTU_FUNC_WRITE_MULTIPLE_REGISTERS,
                                     request_data, 5 + register_count * 2, response_data, &response_data_length, 4, timeout);
    if (status != ModBusRTU_OK) {
        return status;
    }
    
    /* ========== 验证响应数据 ========== */
    if (response_data_length != 4) {
        return ModBusRTU_ERROR_INVALID_RESPONSE;
    }
    
    if (response_data[0] != request_data[0] || response_data[1] != request_data[1] ||
        response_data[2] != request_data[2] || response_data[3] != request_data[3]) {
        return ModBusRTU_ERROR_INVALID_RESPONSE;
    }
    
    return ModBusRTU_OK;
}

/**
 * @brief 写多个寄存器（功能码10/16）
 */
ModBusRTU_Status_t ModBusRTU_WriteMultipleRegisters(UART_Instance_t uart_instance, uint8_t slave_address,
                                                     uint16_t start_address, uint16_t register_count,
                                                     const uint16_t *data, uint32_t timeout)
{
    ModBusRTU_Status_t status;
    uint8_t retry_count = MODBUS_RTU_DEFAULT_RETRY_COUNT;
    uint8_t retry;
    
    /* ========== 参数校验 ========== */
    if (slave_address == 0 || slave_address > 247) {
        return ModBusRTU_ERROR_INVALID_ADDRESS;
    }
    if (uart_instance >= UART_INSTANCE_MAX) {
        return ModBusRTU_ERROR_INVALID_INSTANCE;
    }
    
    /* ========== 重试机制 ========== */
    for (retry = 0; retry <= retry_count; retry++) {
        status = ModBusRTU_WriteMultipleRegisters_Internal(uart_instance, slave_address,
                                                            start_address, register_count, data, timeout);
        
        /* 如果成功或非超时/CRC错误，直接返回 */
        if (status == ModBusRTU_OK || 
            (status != ModBusRTU_ERROR_TIMEOUT && status != ModBusRTU_ERROR_CRC)) {
            return status;
        }
        
        /* 超时或CRC错误，重试前等待一小段时间 */
        if (retry < retry_count) {
            Delay_ms(10);  /* 等待10ms后重试 */
        }
    }
    
    return status;  /* 返回最后一次的错误码 */
}

/**
 * @brief 从机初始化
 */
ModBusRTU_Status_t ModBusRTU_SlaveInit(const ModBusRTU_Config_t *config)
{
    /* ========== 参数校验 ========== */
    if (config == NULL) {
        return ModBusRTU_ERROR_NULL_PTR;
    }
    
    if (config->uart_instance >= UART_INSTANCE_MAX) {
        return ModBusRTU_ERROR_INVALID_INSTANCE;
    }
    
    if (config->slave_address == 0 || config->slave_address > 247) {
        return ModBusRTU_ERROR_INVALID_ADDRESS;
    }
    
    /* ========== 保存配置 ========== */
    memcpy(&g_slave_config, config, sizeof(ModBusRTU_Config_t));
    g_slave_initialized = 1;
    
    return ModBusRTU_OK;
}

/**
 * @brief 从机处理请求（轮询方式）
 */
ModBusRTU_Status_t ModBusRTU_SlaveProcess(uint8_t *function_code, uint16_t *start_address,
                                          uint16_t *register_count, uint8_t *data,
                                          uint16_t *data_size, uint32_t timeout)
{
    uint8_t request_frame[256];
    uint16_t request_length = 0;
    UART_Status_t uart_status;
    uint16_t crc_calculated;
    uint16_t crc_received;
    
    /* ========== 参数校验 ========== */
    if (function_code == NULL || start_address == NULL || register_count == NULL ||
        data == NULL || data_size == NULL) {
        return ModBusRTU_ERROR_NULL_PTR;
    }
    
    if (!g_slave_initialized) {
        return ModBusRTU_ERROR_NOT_INITIALIZED;
    }
    
    /* ========== 接收请求帧（尝试接收最小帧） ========== */
    /* 先接收地址和功能码 */
    uint32_t actual_timeout = ModBusRTU_GetTimeout(timeout);
    uart_status = UART_Receive(g_slave_config.uart_instance, request_frame, 2, actual_timeout);
    if (uart_status != UART_OK) {
        return ModBusRTU_ConvertUARTError(uart_status);
    }
    
    /* 检查地址是否匹配 */
    if (request_frame[0] != g_slave_config.slave_address) {
        return ModBusRTU_ERROR_INVALID_ADDRESS;
    }
    
    *function_code = request_frame[1];
    
    /* 根据功能码确定需要接收的数据长度 */
    switch (*function_code) {
        case ModBusRTU_FUNC_READ_HOLDING_REGISTERS:
            /* 地址(2) + 数量(2) + CRC(2) */
            request_length = 6;
            break;
        case ModBusRTU_FUNC_WRITE_SINGLE_REGISTER:
            /* 地址(2) + 值(2) + CRC(2) */
            request_length = 6;
            break;
        case ModBusRTU_FUNC_WRITE_MULTIPLE_REGISTERS:
            /* 先接收地址和数量，再确定总长度 */
            uart_status = UART_Receive(g_slave_config.uart_instance, &request_frame[2], 4, actual_timeout);
            if (uart_status != UART_OK) {
                return ModBusRTU_ConvertUARTError(uart_status);
            }
            *start_address = ((uint16_t)request_frame[2] << 8) | request_frame[3];
            *register_count = ((uint16_t)request_frame[4] << 8) | request_frame[5];
            request_length = 7 + *register_count * 2;  /* 地址(2) + 数量(2) + 字节数(1) + 数据 + CRC(2) */
            
            /* 检查缓冲区溢出 */
            if (request_length > sizeof(request_frame)) {
                return ModBusRTU_ERROR_INVALID_PARAM;
            }
            
            /* 检查寄存器数量是否超出限制 */
            if (*register_count > 123) {
                return ModBusRTU_ERROR_INVALID_PARAM;
            }
            break;
        default:
            return ModBusRTU_ERROR_INVALID_FUNCTION_CODE;
    }
    
    /* 接收剩余数据 */
    if (request_length > 2) {
        /* 检查是否已经接收了部分数据（写多个寄存器时已经接收了4字节） */
        if (*function_code == ModBusRTU_FUNC_WRITE_MULTIPLE_REGISTERS) {
            /* 已经接收了地址和数量（4字节），还需要接收字节数、数据和CRC */
            /* 剩余长度 = request_length - 2（地址和功能码）- 4（已接收的地址和数量） */
            uint16_t remaining_length = request_length - 6;
            if (remaining_length > 0) {
                uart_status = UART_Receive(g_slave_config.uart_instance, &request_frame[6],
                                           remaining_length, actual_timeout);
                if (uart_status != UART_OK) {
                    return ModBusRTU_ConvertUARTError(uart_status);
                }
            }
        } else {
            /* 其他功能码：接收剩余数据 */
            uart_status = UART_Receive(g_slave_config.uart_instance, &request_frame[2],
                                       request_length - 2, actual_timeout);
            if (uart_status != UART_OK) {
                return ModBusRTU_ConvertUARTError(uart_status);
            }
        }
    }
    
    /* ========== 验证CRC ========== */
    crc_received = ((uint16_t)request_frame[request_length - 2]) | ((uint16_t)request_frame[request_length - 1] << 8);  /* CRC低位在前，高位在后 */
    crc_calculated = ModBusRTU_CalculateCRC16(request_frame, request_length - 2);
    
    if (crc_calculated != crc_received) {
        return ModBusRTU_ERROR_CRC;
    }
    
    /* ========== 解析请求数据 ========== */
    switch (*function_code) {
        case ModBusRTU_FUNC_READ_HOLDING_REGISTERS:
            *start_address = ((uint16_t)request_frame[2] << 8) | request_frame[3];
            *register_count = ((uint16_t)request_frame[4] << 8) | request_frame[5];
            *data_size = 0;  /* 读操作不需要输入数据 */
            break;
        case ModBusRTU_FUNC_WRITE_SINGLE_REGISTER:
            *start_address = ((uint16_t)request_frame[2] << 8) | request_frame[3];
            *register_count = 1;
            if (*data_size < 2) {
                return ModBusRTU_ERROR_INVALID_PARAM;
            }
            data[0] = request_frame[4];
            data[1] = request_frame[5];
            *data_size = 2;
            break;
        case ModBusRTU_FUNC_WRITE_MULTIPLE_REGISTERS:
            *start_address = ((uint16_t)request_frame[2] << 8) | request_frame[3];
            *register_count = ((uint16_t)request_frame[4] << 8) | request_frame[5];
            if (*data_size < (*register_count * 2)) {
                return ModBusRTU_ERROR_INVALID_PARAM;
            }
            memcpy(data, &request_frame[7], *register_count * 2);
            *data_size = *register_count * 2;
            break;
        default:
            return ModBusRTU_ERROR_INVALID_FUNCTION_CODE;
    }
    
    return ModBusRTU_OK;
}

/**
 * @brief 从机发送响应
 */
ModBusRTU_Status_t ModBusRTU_SlaveSendResponse(UART_Instance_t uart_instance, uint8_t slave_address,
                                                uint8_t function_code, const uint8_t *data,
                                                uint16_t data_length, uint32_t timeout)
{
    uint8_t response_frame[256];
    uint16_t response_length = 0;
    UART_Status_t uart_status;
    ModBusRTU_Status_t status;
    
    /* ========== 参数校验 ========== */
    if (data == NULL && data_length > 0) {
        return ModBusRTU_ERROR_NULL_PTR;
    }
    
    if (slave_address == 0 || slave_address > 247) {
        return ModBusRTU_ERROR_INVALID_ADDRESS;
    }
    
    if (uart_instance >= UART_INSTANCE_MAX) {
        return ModBusRTU_ERROR_INVALID_INSTANCE;
    }
    
    /* ========== 构建响应帧 ========== */
    status = ModBusRTU_BuildRequestFrame(response_frame, sizeof(response_frame), &response_length, slave_address,
                                         function_code, data, data_length);
    if (status != ModBusRTU_OK) {
        return status;
    }
    
    /* ========== 发送响应帧 ========== */
    uint32_t actual_timeout = ModBusRTU_GetTimeout(timeout);
    uart_status = UART_Transmit(uart_instance, response_frame, response_length, actual_timeout);
    if (uart_status != UART_OK) {
        return ModBusRTU_ConvertUARTError(uart_status);
    }
    
    return ModBusRTU_OK;
}

/**
 * @brief 从机发送异常响应
 */
ModBusRTU_Status_t ModBusRTU_SlaveSendException(UART_Instance_t uart_instance, uint8_t slave_address,
                                                 uint8_t function_code, ModBusRTU_ExceptionCode_t exception_code,
                                                 uint32_t timeout)
{
    uint8_t response_frame[5];
    uint16_t response_length = 0;
    uint8_t exception_data[1];
    UART_Status_t uart_status;
    ModBusRTU_Status_t status;
    
    /* ========== 参数校验 ========== */
    if (slave_address == 0 || slave_address > 247) {
        return ModBusRTU_ERROR_INVALID_ADDRESS;
    }
    
    if (uart_instance >= UART_INSTANCE_MAX) {
        return ModBusRTU_ERROR_INVALID_INSTANCE;
    }
    
    /* ========== 构建异常响应帧 ========== */
    exception_data[0] = (uint8_t)exception_code;
    status = ModBusRTU_BuildRequestFrame(response_frame, sizeof(response_frame), &response_length, slave_address,
                                         function_code | 0x80, exception_data, 1);
    if (status != ModBusRTU_OK) {
        return status;
    }
    
    /* ========== 发送异常响应帧 ========== */
    uint32_t actual_timeout = ModBusRTU_GetTimeout(timeout);
    uart_status = UART_Transmit(uart_instance, response_frame, response_length, actual_timeout);
    if (uart_status != UART_OK) {
        return ModBusRTU_ConvertUARTError(uart_status);
    }
    
    return ModBusRTU_OK;
}

#endif /* CONFIG_MODULE_MODBUS_RTU_ENABLED */

