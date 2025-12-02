/**
 * @file oled_font_ascii16x16.h
 * @brief OLED 8x16点阵ASCII字库模块（从文件系统读取）
 * @version 1.0.0
 * @date 2024-01-01
 * @note ASCII字库存储在W25Q文件系统的/font目录下
 * @note 字库文件格式：按字符索引顺序存储字模数据，每个字符16字节（8x16点阵）
 */

#ifndef OLED_FONT_ASCII16X16_H
#define OLED_FONT_ASCII16X16_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include "config.h"
#include <stdint.h>

/* 模块开关检查 */
#ifdef CONFIG_MODULE_OLED_ENABLED
#if CONFIG_MODULE_OLED_ENABLED

#ifdef CONFIG_MODULE_FS_WRAPPER_ENABLED
#if CONFIG_MODULE_FS_WRAPPER_ENABLED

/* ==================== 配置选项 ==================== */

/**
 * @brief ASCII字库文件名
 * @note 字库文件存储在/font目录下
 */
#ifndef OLED_ASCII_FONT_FILENAME
#define OLED_ASCII_FONT_FILENAME    "ASCII16.bin"
#endif

/**
 * @brief 字模缓存开关
 * @note 默认关闭缓存以节省RAM（约16字节）
 * @note 启用缓存可提高字符显示速度，但会占用16字节RAM
 */
#ifndef OLED_ASCII_FONT_CACHE_ENABLED
#define OLED_ASCII_FONT_CACHE_ENABLED   0
#endif

/**
 * @brief 缓存大小（字符个数）
 * @note 仅在OLED_ASCII_FONT_CACHE_ENABLED=1时有效
 */
#ifndef OLED_ASCII_FONT_CACHE_SIZE
#define OLED_ASCII_FONT_CACHE_SIZE      2
#endif

/* ==================== 常量定义 ==================== */

/**
 * @brief ASCII字符字模大小（字节）
 * @note 8x16点阵 = 128位 = 16字节
 */
#define OLED_ASCII_FONT_CHAR_SIZE       16

/**
 * @brief ASCII字符索引范围
 */
#define OLED_ASCII_CHAR_INDEX_MIN       0   /* 对应字符' ' */
#define OLED_ASCII_CHAR_INDEX_MAX       95   /* 对应字符'~'和度符号 */

/**
 * @brief ASCII字符总数
 */
#define OLED_ASCII_FONT_CHAR_COUNT      96   /* 0-95，共96个字符 */

/* ==================== 错误码 ==================== */

/**
 * @brief ASCII字库状态
 */
typedef enum {
    OLED_ASCII_FONT_OK = ERROR_OK,                                    /**< 操作成功 */
    OLED_ASCII_FONT_ERROR_NOT_INIT = ERROR_BASE_OLED - 30,           /**< 模块未初始化 */
    OLED_ASCII_FONT_ERROR_INVALID_PARAM = ERROR_BASE_OLED - 31,       /**< 参数非法 */
    OLED_ASCII_FONT_ERROR_INVALID_INDEX = ERROR_BASE_OLED - 32,      /**< 无效的字符索引 */
    OLED_ASCII_FONT_ERROR_READ_FAILED = ERROR_BASE_OLED - 33,         /**< 读取失败 */
} OLED_AsciiFont_Status_t;

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化ASCII字库模块
 * @return OLED_AsciiFont_Status_t 错误码
 * @note 检查文件系统是否已初始化，并验证字库文件是否存在
 */
OLED_AsciiFont_Status_t OLED_AsciiFont_Init(void);

/**
 * @brief 从文件系统读取字模数据
 * @param[in] char_index 字符索引（0-95，0=' ', 95=度符号）
 * @param[out] font_data 字模数据缓冲区（16字节）
 * @return OLED_AsciiFont_Status_t 错误码
 * @note 字模数据临时存储在栈上，读取后立即使用
 * @note 如果启用缓存，会先检查缓存
 */
OLED_AsciiFont_Status_t OLED_AsciiFont_GetData(uint8_t char_index, uint8_t *font_data);

/**
 * @brief 将ASCII字符转换为字符索引
 * @param[in] ch ASCII字符
 * @param[out] char_index 字符索引（0-95）
 * @return OLED_AsciiFont_Status_t 错误码
 * @note 支持标准ASCII字符（' '到'~'）和度符号（0xB0或176）
 */
OLED_AsciiFont_Status_t OLED_AsciiFont_GetIndex(char ch, uint8_t *char_index);

#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */
#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */

#endif /* CONFIG_MODULE_OLED_ENABLED */
#endif /* CONFIG_MODULE_OLED_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* OLED_FONT_ASCII16X16_H */



