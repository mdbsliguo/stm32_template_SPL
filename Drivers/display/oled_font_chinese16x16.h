/**
 * @file oled_font_chinese16x16.h
 * @brief OLED 16x16点阵中文字库模块（GB2312字符集）
 * @version 1.0.0
 * @date 2024-01-01
 * @note 中文字库存储在W25Q文件系统的/font目录下
 * @note 字库文件格式：二进制文件，按GB2312索引顺序存储字模数据（每个字符32字节）
 */

#ifndef OLED_FONT_CHINESE16X16_H
#define OLED_FONT_CHINESE16X16_H

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
 * @brief 中文字库文件名
 * @note 字库文件存储在/font目录下
 */
#ifndef OLED_CHINESE_FONT_FILENAME
#define OLED_CHINESE_FONT_FILENAME    "chinese16x16.bin"
#endif

/**
 * @brief 字模缓存开关
 * @note 默认禁用缓存以节省RAM（20KB限制）
 * @note 启用缓存可提升常用字符显示速度，但会占用32-64字节RAM
 */
#ifndef OLED_CHINESE_FONT_CACHE_ENABLED
#define OLED_CHINESE_FONT_CACHE_ENABLED   0
#endif

/**
 * @brief 缓存大小（字符数）
 * @note 仅在OLED_CHINESE_FONT_CACHE_ENABLED=1时有效
 */
#ifndef OLED_CHINESE_FONT_CACHE_SIZE
#define OLED_CHINESE_FONT_CACHE_SIZE      2
#endif

/* ==================== 常量定义 ==================== */

/**
 * @brief 中文字符字模大小（字节）
 * @note 16x16点阵 = 256位 = 32字节
 */
#define OLED_CHINESE_FONT_CHAR_SIZE       32

/**
 * @brief GB2312编码范围
 */
#define OLED_GB2312_MIN                   0xA1A1
#define OLED_GB2312_MAX                   0xFEFE

/* ==================== 错误码 ==================== */

/**
 * @brief 中文字库错误码
 */
typedef enum {
    OLED_CHINESE_FONT_OK = ERROR_OK,                                    /**< 操作成功 */
    OLED_CHINESE_FONT_ERROR_NOT_INIT = ERROR_BASE_OLED - 20,           /**< 模块未初始化 */
    OLED_CHINESE_FONT_ERROR_INVALID_PARAM = ERROR_BASE_OLED - 21,      /**< 参数非法 */
    OLED_CHINESE_FONT_ERROR_INVALID_GB2312 = ERROR_BASE_OLED - 22,     /**< 无效的GB2312编码 */
    OLED_CHINESE_FONT_ERROR_READ_FAILED = ERROR_BASE_OLED - 23,         /**< 读取失败 */
} OLED_ChineseFont_Status_t;

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化中文字库模块
 * @return OLED_ChineseFont_Status_t 错误码
 * @note 检查文件系统是否已初始化，验证字库文件是否存在
 */
OLED_ChineseFont_Status_t OLED_ChineseFont_Init(void);

/**
 * @brief 获取GB2312编码对应的字库索引
 * @param[in] gb2312_code GB2312编码（2字节，高字节在前）
 * @param[out] index 字库索引（输出）
 * @return OLED_ChineseFont_Status_t 错误码
 * @note GB2312编码范围：0xA1A1-0xFEFE
 * @note 索引计算：区码×94 + 位码
 */
OLED_ChineseFont_Status_t OLED_ChineseFont_GetIndex(uint16_t gb2312_code, uint32_t *index);

/**
 * @brief 从文件系统读取字模数据
 * @param[in] gb2312_code GB2312编码（2字节，高字节在前）
 * @param[out] font_data 字模数据缓冲区（32字节）
 * @return OLED_ChineseFont_Status_t 错误码
 * @note 字模数据存储在栈上，读取后立即使用
 * @note 如果启用缓存，会先检查缓存
 */
OLED_ChineseFont_Status_t OLED_ChineseFont_GetData(uint16_t gb2312_code, uint8_t *font_data);

/**
 * @brief 检查GB2312编码是否有效
 * @param[in] gb2312_code GB2312编码（2字节，高字节在前）
 * @return uint8_t 1-有效，0-无效
 */
uint8_t OLED_ChineseFont_IsValidGB2312(uint16_t gb2312_code);

#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */
#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */

#endif /* CONFIG_MODULE_OLED_ENABLED */
#endif /* CONFIG_MODULE_OLED_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* OLED_FONT_CHINESE16X16_H */
