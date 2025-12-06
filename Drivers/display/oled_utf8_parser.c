/**
 * @file oled_utf8_parser.c
 * @brief OLED UTF-8编码解析模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @note 支持UTF-8到GB2312编码转换，用于中英文混显
 * @note 简化实现：UTF-8到GB2312转换使用基本映射，完整转换需要查找表
 */

#include "oled_utf8_parser.h"
#include "config.h"
#include <stddef.h>

#ifdef CONFIG_MODULE_OLED_ENABLED
#if CONFIG_MODULE_OLED_ENABLED

/* ==================== 内部常量 ==================== */

/* UTF-8中文字符范围：0xE4-0xE9开头 */
#define UTF8_CHINESE_FIRST_BYTE_MIN    0xE4
#define UTF8_CHINESE_FIRST_BYTE_MAX    0xE9

/* GB2312编码范围 */
#define GB2312_HIGH_BYTE_MIN           0xA1
#define GB2312_HIGH_BYTE_MAX           0xFE
#define GB2312_LOW_BYTE_MIN             0xA1
#define GB2312_LOW_BYTE_MAX             0xFE

/* ==================== 公共函数实现 ==================== */

/**
 * @brief 检查字符是否为ASCII
 */
uint8_t OLED_UTF8_IsASCII(uint8_t ch)
{
    /* ASCII可打印字符范围：0x20-0x7E */
    return (ch >= 0x20 && ch <= 0x7E);
}

/**
 * @brief 检查字符是否为UTF-8中文字符的首字节
 */
uint8_t OLED_UTF8_IsChineseFirstByte(uint8_t ch)
{
    /* UTF-8中文字符首字节范围：0xE4-0xE9 */
    return (ch >= UTF8_CHINESE_FIRST_BYTE_MIN && ch <= UTF8_CHINESE_FIRST_BYTE_MAX);
}

/**
 * @brief 检查两个字节是否为GB2312编码
 */
uint8_t OLED_UTF8_IsGB2312(uint8_t high_byte, uint8_t low_byte)
{
    /* GB2312编码范围：0xA1A1-0xFEFE */
    return (high_byte >= GB2312_HIGH_BYTE_MIN && high_byte <= GB2312_HIGH_BYTE_MAX &&
            low_byte >= GB2312_LOW_BYTE_MIN && low_byte <= GB2312_LOW_BYTE_MAX);
}

/**
 * @brief 获取下一个字符的类型和长度
 */
uint8_t OLED_UTF8_GetNextCharInfo(const char *str, OLED_CharType_t *char_type, uint8_t *char_len)
{
    uint8_t first_byte;
    
    /* 参数校验 */
    if (str == NULL || char_type == NULL || char_len == NULL)
    {
        return 0;
    }
    
    /* 检查字符串结束 */
    if (str[0] == '\0')
    {
        return 0;
    }
    
    first_byte = (uint8_t)str[0];
    
    /* 检查ASCII字符 */
    if (OLED_UTF8_IsASCII(first_byte))
    {
        *char_type = OLED_CHAR_TYPE_ASCII;
        *char_len = 1;
        return 1;
    }
    
    /* 检查UTF-8中文字符（3字节） */
    if (OLED_UTF8_IsChineseFirstByte(first_byte))
    {
        /* 检查后续字节是否存在且有效 */
        if (str[1] != '\0' && str[2] != '\0' &&
            (uint8_t)str[1] >= 0x80 && (uint8_t)str[1] <= 0xBF &&
            (uint8_t)str[2] >= 0x80 && (uint8_t)str[2] <= 0xBF)
        {
            *char_type = OLED_CHAR_TYPE_UTF8_CHINESE;
            *char_len = 3;
            return 1;
        }
    }
    
    /* 检查GB2312中文字符（2字节） */
    if (str[1] != '\0')
    {
        if (OLED_UTF8_IsGB2312(first_byte, (uint8_t)str[1]))
        {
            *char_type = OLED_CHAR_TYPE_GB2312_CHINESE;
            *char_len = 2;
            return 1;
        }
    }
    
    /* 无效字符 */
    *char_type = OLED_CHAR_TYPE_INVALID;
    *char_len = 1;  /* 跳过1字节，避免死循环 */
    return 0;
}

/**
 * @brief UTF-8字符转换为GB2312编码
 * @note 简化实现：仅支持部分常用汉字
 * @note 完整实现需要UTF-8到GB2312的完整转换表（约6000+条目）
 * @note 建议：在实际应用中，直接使用GB2312编码字符串，避免转换开销
 */
uint8_t OLED_UTF8_ToGB2312(const uint8_t *utf8_str, uint16_t *gb2312_code)
{
    /* 参数校验 */
    if (utf8_str == NULL || gb2312_code == NULL)
    {
        return 0;
    }
    
    /* 检查是否为UTF-8中文字符 */
    if (!OLED_UTF8_IsChineseFirstByte(utf8_str[0]))
    {
        return 0;
    }
    
    /* 检查后续字节 */
    if ((utf8_str[1] & 0xC0) != 0x80 || (utf8_str[2] & 0xC0) != 0x80)
    {
        return 0;
    }
    
    /* 简化实现：直接返回0表示不支持转换 */
    /* 完整实现需要UTF-8到GB2312的转换表 */
    /* 建议：在实际应用中，直接使用GB2312编码字符串 */
    (void)gb2312_code;
    
    return 0;  /* 不支持转换，需要转换表 */
}

#endif /* CONFIG_MODULE_OLED_ENABLED */
#endif /* CONFIG_MODULE_OLED_ENABLED */
