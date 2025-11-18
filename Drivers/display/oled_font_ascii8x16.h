/**
 * @file oled_font_ascii8x16.h
 * @brief OLED 8x16点阵字模库（ASCII字符，共95个）
 * @version 1.0.0
 * @date 2024-01-01
 * @note 基于用户提供的可工作字库
 * @note 本模块专门针对ASCII字符集的8x16点阵字体，未来如需支持其他字符集或尺寸，可创建新的模块文件
 */

#ifndef OLED_FONT_ASCII8X16_H
#define OLED_FONT_ASCII8X16_H

#include <stdint.h>

/**
 * @brief OLED 8x16点阵字模数据
 * @note 字符索引从空格（' '，ASCII 32）开始，共95个字符
 * @note 每个字符16字节，前8字节为上半部分，后8字节为下半部分
 */
extern const uint8_t OLED_F8x16[][16];

#endif /* OLED_FONT_ASCII8X16_H */
