/**
 * @file font_uploader.h
 * @brief 字库文件UART传输模块
 * @version 1.0.0
 * @date 2024-01-01
 * @note 用于通过UART传输大文件（如字库）到STM32并写入文件系统
 * @note 支持分段传输，适用于200KB+的大文件
 */

#ifndef FONT_UPLOADER_H
#define FONT_UPLOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include "config.h"
#include <stdint.h>

/* 模块开关检查 */
#ifdef CONFIG_MODULE_UART_ENABLED
#if CONFIG_MODULE_UART_ENABLED

#ifdef CONFIG_MODULE_FS_WRAPPER_ENABLED
#if CONFIG_MODULE_FS_WRAPPER_ENABLED

#include "uart.h"

/* ==================== 协议定义 ==================== */

/**
 * @brief 传输协议命令定义
 */
#define FONT_UPLOAD_CMD_START       0xAA    /**< 开始传输命令 */
#define FONT_UPLOAD_CMD_DATA        0xBB    /**< 数据包命令 */
#define FONT_UPLOAD_CMD_END         0xCC    /**< 结束传输命令 */
#define FONT_UPLOAD_CMD_ACK         0xDD    /**< 确认命令 */

/**
 * @brief 每包数据大小（字节）
 * @note 建议值：256字节，平衡传输效率和可靠性
 */
#define FONT_UPLOAD_CHUNK_SIZE      256

/* ==================== 错误码 ==================== */

/**
 * @brief 字库上传状态
 */
typedef enum {
    FONT_UPLOAD_OK = ERROR_OK,                                    /**< 操作成功 */
    FONT_UPLOAD_ERROR_TIMEOUT = ERROR_BASE_UART - 100,          /**< 接收超时 */
    FONT_UPLOAD_ERROR_INVALID_CMD = ERROR_BASE_UART - 101,      /**< 无效命令 */
    FONT_UPLOAD_ERROR_WRITE_FAILED = ERROR_BASE_UART - 102,     /**< 写入失败 */
    FONT_UPLOAD_ERROR_FS_NOT_INIT = ERROR_BASE_UART - 103,      /**< 文件系统未初始化 */
} FontUpload_Status_t;

/* ==================== 函数声明 ==================== */

/**
 * @brief 等待上传命令（'A'=ASCII字库，'C'=中文字库）并返回对应文件名
 * @param[in] uart_instance UART实例（UART_INSTANCE_1/2/3）
 * @param[out] font_filename 返回的字库文件名（缓冲区，至少32字节）
 * @return FontUpload_Status_t 错误码
 * @note 阻塞式函数，等待PC端发送'A'或'C'字符
 * @note 收到命令后回复"OK\r\n"字符串，然后可以开始上传
 * @note 'A'或'a' -> "ASCII16.bin"
 * @note 'C'或'c' -> "chinese16x16.bin"
 * @note 需要先初始化UART
 */
FontUpload_Status_t FontUpload_WaitForCommand(UART_Instance_t uart_instance, 
                                               char* font_filename);

/**
 * @brief 接收字库文件并写入文件系统
 * @param[in] uart_instance UART实例（UART_INSTANCE_1/2/3）
 * @param[in] font_filename 字库文件名（如"chinese16x16.bin"）
 * @return FontUpload_Status_t 错误码
 * @note 阻塞式函数，等待PC端发送完整文件
 * @note 传输协议：
 *   1. PC发送：CMD_START (0xAA)
 *   2. PC发送：文件大小（4字节，小端序）
 *   3. STM32回复：CMD_ACK (0xDD)
 *   4. 循环：
 *      - PC发送：CMD_DATA (0xBB)
 *      - PC发送：数据包大小（2字节，小端序）
 *      - PC发送：数据（最多256字节）
 *      - STM32回复：CMD_ACK (0xDD)
 *   5. PC发送：CMD_END (0xCC)
 * @note 需要先初始化UART和文件系统
 */
FontUpload_Status_t FontUpload_ReceiveFile(UART_Instance_t uart_instance, 
                                           const char* font_filename);

#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */
#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */

#endif /* CONFIG_MODULE_UART_ENABLED */
#endif /* CONFIG_MODULE_UART_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* FONT_UPLOADER_H */
