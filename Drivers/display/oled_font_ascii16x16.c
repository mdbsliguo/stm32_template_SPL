/**
 * @file oled_font_ascii16x16.c
 * @brief OLED 8x16点阵ASCII字库模块实现（从文件系统读取）
 * @version 1.0.0
 * @date 2024-01-01
 * @note ASCII字库存储在W25Q文件系统的/font目录下
 * @note 内存优化：默认不使用缓存，字模数据临时存储在栈上（16字节）
 */

#include "oled_font_ascii16x16.h"
#include "config.h"
#include <string.h>

#ifdef CONFIG_MODULE_OLED_ENABLED
#if CONFIG_MODULE_OLED_ENABLED

#ifdef CONFIG_MODULE_FS_WRAPPER_ENABLED
#if CONFIG_MODULE_FS_WRAPPER_ENABLED

#include "fs_wrapper.h"

/* ==================== 内部变量 ==================== */

/* 模块初始化标志 */
static uint8_t g_ascii_font_initialized = 0;

#if OLED_ASCII_FONT_CACHE_ENABLED
/* 字模缓存结构体 */
typedef struct {
    uint8_t char_index;                      /**< 字符索引 */
    uint8_t font_data[OLED_ASCII_FONT_CHAR_SIZE];  /**< 字模数据 */
    uint8_t valid;                           /**< 缓存有效标志 */
} ascii_font_cache_t;

/* 字模缓存数组 */
static ascii_font_cache_t g_font_cache[OLED_ASCII_FONT_CACHE_SIZE];
#endif

/* ==================== 内部函数 ==================== */

/**
 * @brief 从缓存中查找字模数据
 * @param[in] char_index 字符索引
 * @param[out] font_data 字模数据缓冲区
 * @return uint8_t 1-找到，0-未找到
 */
#if OLED_ASCII_FONT_CACHE_ENABLED
static uint8_t ascii_font_cache_find(uint8_t char_index, uint8_t *font_data)
{
    uint8_t i;
    
    for (i = 0; i < OLED_ASCII_FONT_CACHE_SIZE; i++)
    {
        if (g_font_cache[i].valid && g_font_cache[i].char_index == char_index)
        {
            /* 找到缓存，复制数据 */
            memcpy(font_data, g_font_cache[i].font_data, OLED_ASCII_FONT_CHAR_SIZE);
            return 1;
        }
    }
    
    return 0;
}

/**
 * @brief 将字模数据添加到缓存
 * @param[in] char_index 字符索引
 * @param[in] font_data 字模数据
 */
static void ascii_font_cache_add(uint8_t char_index, const uint8_t *font_data)
{
    static uint8_t cache_index = 0;
    
    /* 使用循环覆盖策略（FIFO） */
    g_font_cache[cache_index].char_index = char_index;
    memcpy(g_font_cache[cache_index].font_data, font_data, OLED_ASCII_FONT_CHAR_SIZE);
    g_font_cache[cache_index].valid = 1;
    
    /* 更新索引 */
    cache_index = (cache_index + 1) % OLED_ASCII_FONT_CACHE_SIZE;
}
#endif

/* ==================== 公共函数实现 ==================== */

/**
 * @brief 初始化ASCII字库模块
 */
OLED_AsciiFont_Status_t OLED_AsciiFont_Init(void)
{
    /* 检查是否已初始化 */
    if (g_ascii_font_initialized)
    {
        return OLED_ASCII_FONT_OK;
    }
    
    /* 检查文件系统是否已初始化 */
    if (!FS_IsInitialized())
    {
        return OLED_ASCII_FONT_ERROR_NOT_INIT;
    }
    
#if OLED_ASCII_FONT_CACHE_ENABLED
    /* 初始化缓存 */
    memset(g_font_cache, 0, sizeof(g_font_cache));
#endif
    
    g_ascii_font_initialized = 1;
    return OLED_ASCII_FONT_OK;
}

/**
 * @brief 将ASCII字符转换为字符索引
 */
OLED_AsciiFont_Status_t OLED_AsciiFont_GetIndex(char ch, uint8_t *char_index)
{
    /* 参数校验 */
    if (char_index == NULL)
    {
        return OLED_ASCII_FONT_ERROR_INVALID_PARAM;
    }
    
    /* 字符索引计算：支持度符号（°） */
    if ((unsigned char)ch == 0xB0 || (unsigned char)ch == 176)  /* 度符号的两种表示 */
    {
        *char_index = 95;  /* 度符号在字库中的索引 */
    }
    else if (ch >= ' ' && ch <= '~')
    {
        *char_index = ch - ' ';  /* 标准ASCII字符索引（0-94） */
    }
    else
    {
        return OLED_ASCII_FONT_ERROR_INVALID_INDEX;  /* 不支持的字符 */
    }
    
    return OLED_ASCII_FONT_OK;
}

/**
 * @brief 从文件系统读取字模数据
 */
OLED_AsciiFont_Status_t OLED_AsciiFont_GetData(uint8_t char_index, uint8_t *font_data)
{
    uint32_t offset;
    error_code_t fs_status;
    
    /* 参数校验 */
    if (font_data == NULL)
    {
        return OLED_ASCII_FONT_ERROR_INVALID_PARAM;
    }
    
    /* 检查模块是否已初始化 */
    if (!g_ascii_font_initialized)
    {
        return OLED_ASCII_FONT_ERROR_NOT_INIT;
    }
    
    /* 检查字符索引是否有效 */
    if (char_index > OLED_ASCII_CHAR_INDEX_MAX)
    {
        return OLED_ASCII_FONT_ERROR_INVALID_INDEX;
    }
    
#if OLED_ASCII_FONT_CACHE_ENABLED
    /* 先检查缓存 */
    if (ascii_font_cache_find(char_index, font_data))
    {
        return OLED_ASCII_FONT_OK;  /* 缓存命中 */
    }
#endif
    
    /* 计算文件偏移量：字符索引 × 16字节 */
    offset = (uint32_t)char_index * OLED_ASCII_FONT_CHAR_SIZE;
    
    /* 从文件系统读取字模数据 */
    fs_status = FS_ReadFile(FS_DIR_FONT, OLED_ASCII_FONT_FILENAME, 
                           offset, font_data, OLED_ASCII_FONT_CHAR_SIZE);
    if (fs_status != FS_WRAPPER_OK)
    {
        return OLED_ASCII_FONT_ERROR_READ_FAILED;
    }
    
#if OLED_ASCII_FONT_CACHE_ENABLED
    /* 添加到缓存 */
    ascii_font_cache_add(char_index, font_data);
#endif
    
    return OLED_ASCII_FONT_OK;
}

#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */
#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */

#endif /* CONFIG_MODULE_OLED_ENABLED */
#endif /* CONFIG_MODULE_OLED_ENABLED */



