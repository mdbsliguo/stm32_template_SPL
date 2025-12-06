/**
 * @file oled_font_chinese16x16.c
 * @brief OLED 16x16点阵中文字库模块实现（GB2312字符集）
 * @version 1.0.0
 * @date 2024-01-01
 * @note 中文字库存储在W25Q文件系统的/font目录下
 * @note 内存优化：默认不使用缓存，字模数据临时存储在栈上（32字节）
 */

#include "oled_font_chinese16x16.h"
#include "config.h"
#include <string.h>

#ifdef CONFIG_MODULE_OLED_ENABLED
#if CONFIG_MODULE_OLED_ENABLED

#ifdef CONFIG_MODULE_FS_WRAPPER_ENABLED
#if CONFIG_MODULE_FS_WRAPPER_ENABLED

#include "fs_wrapper.h"

/* ==================== 内部常量 ==================== */

/* 模块初始化标志 */
static uint8_t g_chinese_font_initialized = 0;

#if OLED_CHINESE_FONT_CACHE_ENABLED
/* 字模缓存结构体 */
typedef struct {
    uint16_t gb2312_code;              /**< GB2312编码 */
    uint8_t font_data[OLED_CHINESE_FONT_CHAR_SIZE];  /**< 字模数据 */
    uint8_t valid;                     /**< 缓存有效标志 */
} chinese_font_cache_t;

/* 字模缓存数组 */
static chinese_font_cache_t g_font_cache[OLED_CHINESE_FONT_CACHE_SIZE];
#endif

/* ==================== 内部函数 ==================== */

/**
 * @brief 从缓存中查找字模数据
 * @param[in] gb2312_code GB2312编码
 * @param[out] font_data 字模数据缓冲区
 * @return uint8_t 1-找到，0-未找到
 */
#if OLED_CHINESE_FONT_CACHE_ENABLED
static uint8_t chinese_font_cache_find(uint16_t gb2312_code, uint8_t *font_data)
{
    uint8_t i;
    
    for (i = 0; i < OLED_CHINESE_FONT_CACHE_SIZE; i++)
    {
        if (g_font_cache[i].valid && g_font_cache[i].gb2312_code == gb2312_code)
        {
            /* 找到缓存，复制数据 */
            memcpy(font_data, g_font_cache[i].font_data, OLED_CHINESE_FONT_CHAR_SIZE);
            return 1;
        }
    }
    
    return 0;
}

/**
 * @brief 将字模数据添加到缓存
 * @param[in] gb2312_code GB2312编码
 * @param[in] font_data 字模数据
 */
static void chinese_font_cache_add(uint16_t gb2312_code, const uint8_t *font_data)
{
    static uint8_t cache_index = 0;
    
    /* 使用循环覆盖策略（FIFO） */
    g_font_cache[cache_index].gb2312_code = gb2312_code;
    memcpy(g_font_cache[cache_index].font_data, font_data, OLED_CHINESE_FONT_CHAR_SIZE);
    g_font_cache[cache_index].valid = 1;
    
    /* 更新索引 */
    cache_index = (cache_index + 1) % OLED_CHINESE_FONT_CACHE_SIZE;
}
#endif

/* ==================== 公共函数实现 ==================== */

/**
 * @brief 初始化中文字库模块
 */
OLED_ChineseFont_Status_t OLED_ChineseFont_Init(void)
{
    /* 检查是否已初始化 */
    if (g_chinese_font_initialized)
    {
        return OLED_CHINESE_FONT_OK;
    }
    
    /* 检查文件系统是否已初始化 */
    if (!FS_IsInitialized())
    {
        return OLED_CHINESE_FONT_ERROR_NOT_INIT;
    }
    
#if OLED_CHINESE_FONT_CACHE_ENABLED
    /* 初始化缓存 */
    memset(g_font_cache, 0, sizeof(g_font_cache));
#endif
    
    g_chinese_font_initialized = 1;
    return OLED_CHINESE_FONT_OK;
}

/**
 * @brief 检查GB2312编码是否有效
 */
uint8_t OLED_ChineseFont_IsValidGB2312(uint16_t gb2312_code)
{
    uint8_t high_byte = (uint8_t)(gb2312_code >> 8);
    uint8_t low_byte = (uint8_t)(gb2312_code & 0xFF);
    
    /* GB2312编码范围：0xA1A1-0xFEFE */
    if (high_byte >= 0xA1 && high_byte <= 0xFE && 
        low_byte >= 0xA1 && low_byte <= 0xFE)
    {
        return 1;
    }
    
    return 0;
}

/**
 * @brief 获取GB2312编码对应的字库索引
 */
OLED_ChineseFont_Status_t OLED_ChineseFont_GetIndex(uint16_t gb2312_code, uint32_t *index)
{
    uint8_t high_byte, low_byte;
    uint8_t qu_code, wei_code;
    
    /* 参数校验 */
    if (index == NULL)
    {
        return OLED_CHINESE_FONT_ERROR_INVALID_PARAM;
    }
    
    /* 检查GB2312编码是否有效 */
    if (!OLED_ChineseFont_IsValidGB2312(gb2312_code))
    {
        return OLED_CHINESE_FONT_ERROR_INVALID_GB2312;
    }
    
    /* 提取高字节和低字节 */
    high_byte = (uint8_t)(gb2312_code >> 8);
    low_byte = (uint8_t)(gb2312_code & 0xFF);
    
    /* 计算区码和位码 */
    qu_code = high_byte - 0xA1;   /* 区码：0-93 */
    wei_code = low_byte - 0xA1;   /* 位码：0-93 */
    
    /* 计算索引：区码×94 + 位码 */
    *index = (uint32_t)qu_code * 94 + (uint32_t)wei_code;
    
    return OLED_CHINESE_FONT_OK;
}

/**
 * @brief 从文件系统读取字模数据
 */
OLED_ChineseFont_Status_t OLED_ChineseFont_GetData(uint16_t gb2312_code, uint8_t *font_data)
{
    OLED_ChineseFont_Status_t status;
    uint32_t index;
    uint32_t offset;
    error_code_t fs_status;
    
    /* 参数校验 */
    if (font_data == NULL)
    {
        return OLED_CHINESE_FONT_ERROR_INVALID_PARAM;
    }
    
    /* 检查模块是否已初始化 */
    if (!g_chinese_font_initialized)
    {
        return OLED_CHINESE_FONT_ERROR_NOT_INIT;
    }
    
    /* 检查GB2312编码是否有效 */
    if (!OLED_ChineseFont_IsValidGB2312(gb2312_code))
    {
        return OLED_CHINESE_FONT_ERROR_INVALID_GB2312;
    }
    
#if OLED_CHINESE_FONT_CACHE_ENABLED
    /* 先检查缓存 */
    if (chinese_font_cache_find(gb2312_code, font_data))
    {
        return OLED_CHINESE_FONT_OK;  /* 缓存命中 */
    }
#endif
    
    /* 获取字库索引 */
    status = OLED_ChineseFont_GetIndex(gb2312_code, &index);
    if (status != OLED_CHINESE_FONT_OK)
    {
        return status;
    }
    
    /* 计算文件偏移量：索引 × 32字节 */
    offset = index * OLED_CHINESE_FONT_CHAR_SIZE;
    
    /* 从文件系统读取字模数据 */
    fs_status = FS_ReadFile(FS_DIR_FONT, OLED_CHINESE_FONT_FILENAME, 
                           offset, font_data, OLED_CHINESE_FONT_CHAR_SIZE);
    if (fs_status != FS_WRAPPER_OK)
    {
        return OLED_CHINESE_FONT_ERROR_READ_FAILED;
    }
    
#if OLED_CHINESE_FONT_CACHE_ENABLED
    /* 添加到缓存 */
    chinese_font_cache_add(gb2312_code, font_data);
#endif
    
    return OLED_CHINESE_FONT_OK;
}

#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */
#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */

#endif /* CONFIG_MODULE_OLED_ENABLED */
#endif /* CONFIG_MODULE_OLED_ENABLED */
