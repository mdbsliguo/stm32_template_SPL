/**
 * @file debug.h
 * @brief 调试输出模块（printf重定向）
 * @version 1.0.0
 * @date 2024-01-01
 * @details 提供printf重定向到UART或SWO输出
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 调试输出模式
 */
typedef enum {
    DEBUG_MODE_NONE = 0,    /**< 禁用调试输出 */
    DEBUG_MODE_UART,        /**< 通过UART输出（需要配置UART） */
    DEBUG_MODE_SWO,         /**< 通过SWO输出（需要SWO配置） */
} debug_mode_t;

/**
 * @brief 初始化调试输出
 * @param[in] mode 输出模式
 * @param[in] baudrate UART波特率（UART模式时有效）
 * @return 0-成功，非0-失败
 */
int Debug_Init(debug_mode_t mode, uint32_t baudrate);

/**
 * @brief 反初始化调试输出
 */
void Debug_Deinit(void);

/**
 * @brief 发送单个字符（用于printf重定向）
 * @param[in] ch 字符
 * @return 发送的字符
 */
int Debug_PutChar(int ch);

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_H */

