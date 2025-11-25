/**
 * @file oled_font_ascii8x16.h
 * @brief OLED 8x16点阵字模库（ASCII字符 + 度符号，共96个）
 * @version 1.1.0
 * @date 2024-01-01
 * @note 基于用户提供的可工作字库
 * @note 本模块专门针对ASCII字符集的8x16点阵字体，未来如需支持其他字符集或尺寸，可创建新的模块文件
 * @note v1.1.0: 添加度符号（°）支持，索引95
 */

#ifndef OLED_FONT_ASCII8X16_H
#define OLED_FONT_ASCII8X16_H

#include <stdint.h>

/**
 * @brief OLED 8x16点阵字模数据
 * @note 字符索引从空格（' '，ASCII 32）开始，共96个字符（95个ASCII字符 + 1个度符号）
 * @note 每个字符16字节，前8字节为上半部分，后8字节为下半部分
 * @note 度符号（°）使用索引95，可通过字符码0xB0或176访问
 */
extern const uint8_t OLED_F8x16[][16];

#endif /* OLED_FONT_ASCII8X16_H */
