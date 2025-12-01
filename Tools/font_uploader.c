/**
 * @file font_uploader.c
 * @brief 字库文件UART传输模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @note 用于通过UART传输大文件（如字库）到STM32并写入文件系统
 */

#include "font_uploader.h"
#include "config.h"
#include <string.h>

#ifdef CONFIG_MODULE_UART_ENABLED
#if CONFIG_MODULE_UART_ENABLED

#ifdef CONFIG_MODULE_FS_WRAPPER_ENABLED
#if CONFIG_MODULE_FS_WRAPPER_ENABLED

#include "fs_wrapper.h"
#include "uart.h"
#include "delay.h"
#include "board.h"
#include "stm32f10x_usart.h"
#include <stdio.h>

/* ==================== 公共函数实现 ==================== */

/**
 * @brief 接收字库文件并写入文件系统
 */
FontUpload_Status_t FontUpload_ReceiveFile(UART_Instance_t uart_instance, 
                                           const char* font_filename)
{
    uint8_t cmd;
    uint8_t buffer[FONT_UPLOAD_CHUNK_SIZE];
    uint32_t file_size = 0;
    uint32_t total_received = 0;
    uint16_t packet_size;
    UART_Status_t uart_status;
    error_code_t fs_status;
    
    /* 参数校验 */
    if (font_filename == NULL) {
        return FONT_UPLOAD_ERROR_WRITE_FAILED;
    }
    
    /* 检查文件系统是否已初始化 */
    if (!FS_IsInitialized()) {
        /* 尝试初始化文件系统 */
        fs_status = FS_Init();
        if (fs_status != FS_WRAPPER_OK) {
            return FONT_UPLOAD_ERROR_FS_NOT_INIT;
        }
    }
    
    /* 清空UART接收缓冲区（直接读取USART寄存器，不等待） */
    USART_TypeDef *uart_periph = UART_GetPeriph(uart_instance);
    if (uart_periph != NULL) {
        /* 快速读取所有待接收的数据 */
        uint16_t count = 0;
        while (USART_GetFlagStatus(uart_periph, USART_FLAG_RXNE) != RESET && count < 100) {
            (void)USART_ReceiveData(uart_periph);  /* 读取并丢弃数据 */
            count++;
        }
    }
    
    /* 注意：不再发送就绪信号，等待PC端发送命令 */
    
    /* 步骤1：循环等待CMD_START命令，忽略所有非协议字节（调试信息等） */
    /* 无限循环等待，直到收到CMD_START命令 */
    uint8_t byte_received;
    
    while (1) {
        /* 尝试接收一个字节（短超时，避免阻塞太久） */
        uart_status = UART_ReceiveByte(uart_instance, &byte_received, 100);  /* 100ms超时 */
        if (uart_status == UART_OK) {
            /* 收到字节，检查是否是CMD_START */
            if (byte_received == FONT_UPLOAD_CMD_START) {
                cmd = byte_received;
                /* 收到CMD_START后，稍作延迟，确保处理完成 */
                Delay_ms(10);
                break;  /* 找到CMD_START，退出循环 */
            }
            /* 不是CMD_START，丢弃（可能是调试信息），继续等待 */
        }
        /* 超时或错误，继续循环等待（无限等待，直到收到CMD_START） */
    }
    
    /* 接收文件大小（4字节，小端序） */
    /* 增加超时时间，给PC端更多时间发送 */
    uart_status = UART_Receive(uart_instance, (uint8_t*)&file_size, 4, 10000);  /* 10秒超时 */
    if (uart_status != UART_OK) {
        return FONT_UPLOAD_ERROR_TIMEOUT;
    }
    
    /* 验证文件大小（防止异常值） */
    if (file_size == 0 || file_size > (10 * 1024 * 1024)) {  /* 最大10MB */
        return FONT_UPLOAD_ERROR_INVALID_CMD;
    }
    
    /* 发送确认（确保发送完成） */
    cmd = FONT_UPLOAD_CMD_ACK;
    uart_status = UART_Transmit(uart_instance, &cmd, 1, 1000);
    if (uart_status != UART_OK) {
        return FONT_UPLOAD_ERROR_TIMEOUT;
    }
    
    /* 确保ACK已发送完成（等待发送完成） */
    Delay_ms(10);
    
    /* 步骤2：分段接收数据并写入 */
    while (total_received < file_size) {
        /* 接收命令 */
        uart_status = UART_Receive(uart_instance, &cmd, 1, 10000);  /* 数据包超时10秒 */
        if (uart_status != UART_OK) {
            return FONT_UPLOAD_ERROR_TIMEOUT;
        }
        
        if (cmd == FONT_UPLOAD_CMD_END) {
            break;  /* 传输结束 */
        }
        
        if (cmd != FONT_UPLOAD_CMD_DATA) {
            return FONT_UPLOAD_ERROR_INVALID_CMD;
        }
        
        /* 接收数据包大小（2字节，小端序） */
        uart_status = UART_Receive(uart_instance, (uint8_t*)&packet_size, 2, 5000);
        if (uart_status != UART_OK) {
            return FONT_UPLOAD_ERROR_TIMEOUT;
        }
        
        /* 验证数据包大小 */
        if (packet_size == 0 || packet_size > FONT_UPLOAD_CHUNK_SIZE) {
            return FONT_UPLOAD_ERROR_INVALID_CMD;
        }
        
        /* 计算实际需要接收的数据量 */
        uint32_t remaining = file_size - total_received;
        uint16_t chunk_size = (packet_size > remaining) ? (uint16_t)remaining : packet_size;
        
        /* 接收数据 */
        uart_status = UART_Receive(uart_instance, buffer, chunk_size, 5000);
        if (uart_status != UART_OK) {
            return FONT_UPLOAD_ERROR_TIMEOUT;
        }
        
        /* 写入文件系统 */
        if (total_received == 0) {
            /* 第一次：创建文件（覆盖旧文件） */
            fs_status = FS_WriteFile(FS_DIR_FONT, font_filename, buffer, chunk_size);
        } else {
            /* 后续：追加数据 */
            fs_status = FS_AppendFile(FS_DIR_FONT, font_filename, buffer, chunk_size);
        }
        
        if (fs_status != FS_WRAPPER_OK) {
            return FONT_UPLOAD_ERROR_WRITE_FAILED;
        }
        
        total_received += chunk_size;
        
        /* 发送确认 */
        cmd = FONT_UPLOAD_CMD_ACK;
        UART_Transmit(uart_instance, &cmd, 1, 1000);
    }
    
    /* 验证文件大小 */
    if (total_received != file_size) {
        return FONT_UPLOAD_ERROR_WRITE_FAILED;
    }
    
    return FONT_UPLOAD_OK;
}

/**
 * @brief 等待上传命令（'A'=ASCII字库，'C'=中文字库）并返回对应文件名
 */
FontUpload_Status_t FontUpload_WaitForCommand(UART_Instance_t uart_instance, 
                                               char* font_filename)
{
    uint8_t cmd;
    UART_Status_t uart_status;
    const char* ok_response = "OK\r\n";
    
    /* 参数校验 */
    if (font_filename == NULL) {
        return FONT_UPLOAD_ERROR_INVALID_CMD;
    }
    
    /* 清空UART接收缓冲区 */
    USART_TypeDef *uart_periph = UART_GetPeriph(uart_instance);
    if (uart_periph != NULL) {
        uint16_t count = 0;
        while (USART_GetFlagStatus(uart_periph, USART_FLAG_RXNE) != RESET && count < 100) {
            (void)USART_ReceiveData(uart_periph);
            count++;
        }
    }
    
    /* 循环等待'A'或'C'命令 */
    while (1) {
        /* 尝试接收一个字节 */
        uart_status = UART_ReceiveByte(uart_instance, &cmd, 100);  /* 100ms超时 */
        if (uart_status == UART_OK) {
            if (cmd == 'A' || cmd == 'a') {
                /* ASCII字库 */
                strcpy(font_filename, "ASCII16.bin");
                /* 回复OK */
                UART_TransmitString(uart_instance, ok_response, 1000);
                return FONT_UPLOAD_OK;
            }
            else if (cmd == 'C' || cmd == 'c') {
                /* 中文字库 */
                strcpy(font_filename, "chinese16x16.bin");
                /* 回复OK */
                UART_TransmitString(uart_instance, ok_response, 1000);
                return FONT_UPLOAD_OK;
            }
            /* 其他字符，忽略，继续等待 */
        }
        /* 超时或错误，继续循环等待 */
    }
}

#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */
#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */

#endif /* CONFIG_MODULE_UART_ENABLED */
#endif /* CONFIG_MODULE_UART_ENABLED */

