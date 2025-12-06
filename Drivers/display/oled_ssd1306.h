/**
 * @file oled_ssd1306.h
 * @brief OLED驱动模块（SSD1306，支持软I2C和硬I2C）
 * @version 3.0.0
 * @date 2024-01-01
 * @note 完全解耦版本：OLED模块通过抽象接口使用I2C，支持软I2C和硬I2C，由配置决定
 * @details OLED模块专注于自身功能，不关心底层I2C实现方式，通过抽象接口使用I2C
 * @note 本模块专门针对SSD1306芯片，未来如需支持其他OLED型号，可创建新的模块文件
 */

#ifndef OLED_SSD1306_H
#define OLED_SSD1306_H

#ifdef __cplusplus
extern "C" {
#endif

#include "board.h"
#include "error_code.h"
#include <stdint.h>

/**
 * @brief OLED错误码
 */
typedef enum {
    OLED_OK = ERROR_OK,                                    /**< 操作成功 */
    OLED_ERROR_NOT_INITIALIZED = ERROR_BASE_OLED - 1,      /**< 未初始化 */
    OLED_ERROR_INVALID_PARAM = ERROR_BASE_OLED - 2,        /**< 参数非法 */
    OLED_ERROR_GPIO_FAILED = ERROR_BASE_OLED - 3,          /**< GPIO操作失败 */
} OLED_Status_t;

/**
 * @brief OLED初始化
 * @return OLED_Status_t 错误码
 * @note 根据board.h中的OLED_I2C_TYPE配置选择I2C接口（软I2C或硬I2C）
 * @note 初始化I2C接口和SSD1306芯片
 */
OLED_Status_t OLED_Init(void);

/**
 * @brief OLED反初始化
 * @return OLED_Status_t 错误码
 */
OLED_Status_t OLED_Deinit(void);

/**
 * @brief OLED清屏
 * @return OLED_Status_t 错误码
 */
OLED_Status_t OLED_Clear(void);

/**
 * @brief OLED设置光标位置
 * @param[in] y 页地址（0~7）
 * @param[in] x 列地址（0~127）
 * @return OLED_Status_t 错误码
 */
OLED_Status_t OLED_SetCursor(uint8_t y, uint8_t x);

/**
 * @brief OLED显示一个字符
 * @param[in] line 行号（1~4）
 * @param[in] column 列号（1~16）
 * @param[in] ch ASCII字符或度符号（°）
 * @return OLED_Status_t 错误码
 * @note 支持ASCII字符（' '到'~'）和度符号（°）
 * @note 度符号可通过字符码0xB0或176访问，例如：OLED_ShowChar(1, 1, 0xB0)
 */
OLED_Status_t OLED_ShowChar(uint8_t line, uint8_t column, char ch);

/**
 * @brief OLED显示字符串
 * @param[in] line 行号（1~4）
 * @param[in] column 列号（1~16）
 * @param[in] str 字符串指针
 * @return OLED_Status_t 错误码
 */
OLED_Status_t OLED_ShowString(uint8_t line, uint8_t column, const char *str);

/**
 * @brief OLED显示无符号十进制数
 * @param[in] line 行号（1~4）
 * @param[in] column 列号（1~16）
 * @param[in] number 数字（0~4294967295）
 * @param[in] length 显示长度（1~10）
 * @return OLED_Status_t 错误码
 */
OLED_Status_t OLED_ShowNum(uint8_t line, uint8_t column, uint32_t number, uint8_t length);

/**
 * @brief OLED显示有符号十进制数
 * @param[in] line 行号（1~4）
 * @param[in] column 列号（1~16）
 * @param[in] number 数字（-2147483648~2147483647）
 * @param[in] length 显示长度（1~10）
 * @return OLED_Status_t 错误码
 */
OLED_Status_t OLED_ShowSignedNum(uint8_t line, uint8_t column, int32_t number, uint8_t length);

/**
 * @brief OLED显示十六进制数
 * @param[in] line 行号（1~4）
 * @param[in] column 列号（1~16）
 * @param[in] number 数字（0~0xFFFFFFFF）
 * @param[in] length 显示长度（1~8）
 * @return OLED_Status_t 错误码
 */
OLED_Status_t OLED_ShowHexNum(uint8_t line, uint8_t column, uint32_t number, uint8_t length);

/**
 * @brief OLED显示二进制数
 * @param[in] line 行号（1~4）
 * @param[in] column 列号（1~16）
 * @param[in] number 数字（0~0xFFFF，length<=16）
 * @param[in] length 显示长度（1~16）
 * @return OLED_Status_t 错误码
 */
OLED_Status_t OLED_ShowBinNum(uint8_t line, uint8_t column, uint32_t number, uint8_t length);

/* ==================== 中文显示功能（需要fs_wrapper模块） ==================== */

#ifdef CONFIG_MODULE_FS_WRAPPER_ENABLED
#if CONFIG_MODULE_FS_WRAPPER_ENABLED

/**
 * @brief OLED显示单个中文字符（16x16点阵）
 * @param[in] line 行号（1~4）
 * @param[in] column 列号（1~8，中文字符占用2列）
 * @param[in] gb2312_code GB2312编码（2字节，高字节在前）
 * @return OLED_Status_t 错误码
 * @note 中文字符占用2列（16像素宽度）
 * @note 需要先初始化中文字库模块（OLED_ChineseFont_Init()）
 */
OLED_Status_t OLED_ShowChineseChar(uint8_t line, uint8_t column, uint16_t gb2312_code);

/**
 * @brief OLED显示GB2312编码字符串（支持中英文混显）
 * @param[in] line 行号（1~4）
 * @param[in] column 列号（1~16，ASCII字符）或（1~8，中文字符）
 * @param[in] str GB2312编码字符串指针
 * @return OLED_Status_t 错误码
 * @note 自动识别ASCII和GB2312中文字符
 * @note ASCII字符占用1列，中文字符占用2列
 * @note 需要先初始化中文字库模块（OLED_ChineseFont_Init()）
 */
OLED_Status_t OLED_ShowStringGB2312(uint8_t line, uint8_t column, const char *str);

/**
 * @brief OLED显示UTF-8编码字符串（支持中英文混显）
 * @param[in] line 行号（1~4）
 * @param[in] column 列号（1~16，ASCII字符）或（1~8，中文字符）
 * @param[in] str UTF-8编码字符串指针
 * @return OLED_Status_t 错误码
 * @note 自动识别ASCII和UTF-8中文字符
 * @note ASCII字符占用1列，中文字符占用2列
 * @note UTF-8中文需要转换为GB2312（简化实现可能不支持所有字符）
 * @note 需要先初始化中文字库模块（OLED_ChineseFont_Init()）
 * @warning UTF-8到GB2312转换功能有限，建议直接使用GB2312编码字符串
 */
OLED_Status_t OLED_ShowStringUTF8(uint8_t line, uint8_t column, const char *str);

#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */
#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* OLED_SSD1306_H */
