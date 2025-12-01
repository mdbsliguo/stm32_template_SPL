/**
 * @file printf_wrapper.h
 * @brief printf封装模块，提供便捷的输出函数
 * @version 1.0.0
 * @date 2024-01-01
 * @details 提供格式化输出到UART和OLED的便捷函数，支持printf风格的格式化字符串
 */

#ifndef PRINTF_WRAPPER_H
#define PRINTF_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 输出格式化字符串到UART1
 * @param[in] format 格式化字符串（printf风格）
 * @param[in] ... 可变参数列表
 * @note 使用标准printf格式化语法，如：Printf_UART1("Value: %d\n", value);
 */
void Printf_UART1(const char *format, ...);

/**
 * @brief 输出格式化字符串到UART2
 * @param[in] format 格式化字符串（printf风格）
 * @param[in] ... 可变参数列表
 * @note 使用标准printf格式化语法，如：Printf_UART2("Debug: %s\n", info);
 */
void Printf_UART2(const char *format, ...);

/**
 * @brief 输出格式化字符串到UART3
 * @param[in] format 格式化字符串（printf风格）
 * @param[in] ... 可变参数列表
 * @note 使用标准printf格式化语法，如：Printf_UART3("Data: %d\n", data);
 */
void Printf_UART3(const char *format, ...);

/**
 * @brief 输出格式化字符串到OLED第1行
 * @param[in] format 格式化字符串（printf风格）
 * @param[in] ... 可变参数列表
 * @note 字符串会自动截断到16字符（OLED每行最多16个字符）
 * @note 使用标准printf格式化语法，如：Printf_OLED1("Temp:%.1fC", temp);
 */
void Printf_OLED1(const char *format, ...);

/**
 * @brief 输出格式化字符串到OLED第2行
 * @param[in] format 格式化字符串（printf风格）
 * @param[in] ... 可变参数列表
 * @note 字符串会自动截断到16字符（OLED每行最多16个字符）
 * @note 使用标准printf格式化语法，如：Printf_OLED2("Humidity:%d%%", hum);
 */
void Printf_OLED2(const char *format, ...);

/**
 * @brief 输出格式化字符串到OLED第3行
 * @param[in] format 格式化字符串（printf风格）
 * @param[in] ... 可变参数列表
 * @note 字符串会自动截断到16字符（OLED每行最多16个字符）
 * @note 使用标准printf格式化语法，如：Printf_OLED3("Status:OK");
 */
void Printf_OLED3(const char *format, ...);

/**
 * @brief 输出格式化字符串到OLED第4行
 * @param[in] format 格式化字符串（printf风格）
 * @param[in] ... 可变参数列表
 * @note 字符串会自动截断到16字符（OLED每行最多16个字符）
 * @note 使用标准printf格式化语法，如：Printf_OLED4("Count:%d", count);
 */
void Printf_OLED4(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* PRINTF_WRAPPER_H */


















