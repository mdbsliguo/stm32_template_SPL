/**
 * @file oled_utf8_parser.h
 * @brief OLED UTF-8编码解析模块
 * @version 1.0.0
 * @date 2024-01-01
 * @note 支持UTF-8到GB2312编码转换，用于中英文混显
 */

#ifndef OLED_UTF8_PARSER_H
#define OLED_UTF8_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include "config.h"
#include <stdint.h>

/* 模块开关检查 */
#ifdef CONFIG_MODULE_OLED_ENABLED
#if CONFIG_MODULE_OLED_ENABLED

/* ==================== 字符类型枚举 ==================== */

/**
 * @brief 字符类型
 */
typedef enum {
    OLED_CHAR_TYPE_ASCII = 0,      /**< ASCII字符（单字节） */
    OLED_CHAR_TYPE_UTF8_CHINESE,   /**< UTF-8中文字符（3字节） */
    OLED_CHAR_TYPE_GB2312_CHINESE, /**< GB2312中文字符（2字节） */
    OLED_CHAR_TYPE_INVALID         /**< 无效字符 */
} OLED_CharType_t;

/* ==================== 函数声明 ==================== */

/**
 * @brief 获取下一个字符的类型和长度
 * @param[in] str 字符串指针
 * @param[out] char_type 字符类型（输出）
 * @param[out] char_len 字符长度（字节数，输出）
 * @return uint8_t 1-成功，0-失败（字符串结束或无效字符）
 * @note 自动识别ASCII、UTF-8中文、GB2312中文
 */
uint8_t OLED_UTF8_GetNextCharInfo(const char *str, OLED_CharType_t *char_type, uint8_t *char_len);

/**
 * @brief UTF-8字符转换为GB2312编码
 * @param[in] utf8_str UTF-8字符（3字节）
 * @param[out] gb2312_code GB2312编码（2字节，高字节在前）
 * @return uint8_t 1-成功，0-失败（不支持该字符）
 * @note 仅支持常用汉字，不支持所有UTF-8字符
 * @note 简化实现：使用查找表或在线转换算法
 */
uint8_t OLED_UTF8_ToGB2312(const uint8_t *utf8_str, uint16_t *gb2312_code);

/**
 * @brief 检查字符是否为ASCII
 * @param[in] ch 字符（单字节）
 * @return uint8_t 1-是ASCII，0-不是
 */
uint8_t OLED_UTF8_IsASCII(uint8_t ch);

/**
 * @brief 检查字符是否为UTF-8中文字符的首字节
 * @param[in] ch 字符（单字节）
 * @return uint8_t 1-是，0-不是
 */
uint8_t OLED_UTF8_IsChineseFirstByte(uint8_t ch);

/**
 * @brief 检查两个字节是否为GB2312编码
 * @param[in] high_byte 高字节
 * @param[in] low_byte 低字节
 * @return uint8_t 1-是GB2312，0-不是
 */
uint8_t OLED_UTF8_IsGB2312(uint8_t high_byte, uint8_t low_byte);

#endif /* CONFIG_MODULE_OLED_ENABLED */
#endif /* CONFIG_MODULE_OLED_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* OLED_UTF8_PARSER_H */



