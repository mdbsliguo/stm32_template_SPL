/**
 * @file oled_ssd1306.c
 * @brief OLED驱动模块实现（SSD1306，支持软I2C和硬I2C）
 * @version 5.0.0
 * @date 2024-01-01
 * @note 完全解耦版本：OLED模块通过抽象接口使用I2C，支持软I2C和硬I2C，由配置决定
 * @details OLED模块专注于自身功能，不关心底层I2C实现方式，通过抽象接口使用I2C
 * @note 本模块专门针对SSD1306芯片，未来如需支持其他OLED型号，可创建新的模块文件
 */

#include "oled_ssd1306.h"
#include "delay.h"
#include "config.h"
#include "stm32f10x.h"
#include <stdbool.h>
#include <stddef.h>

/* ASCII字库选择：使用文件系统字库还是内置字库 */
/* 默认使用内置字库，只有显式定义OLED_USE_FILE_SYSTEM_ASCII_FONT=1时才使用文件系统字库 */
#ifdef CONFIG_MODULE_FS_WRAPPER_ENABLED
#if CONFIG_MODULE_FS_WRAPPER_ENABLED
/* 默认使用内置字库，除非显式定义OLED_USE_FILE_SYSTEM_ASCII_FONT=1 */
#ifndef OLED_USE_FILE_SYSTEM_ASCII_FONT
#define OLED_USE_FILE_SYSTEM_ASCII_FONT    0
#endif
#if OLED_USE_FILE_SYSTEM_ASCII_FONT
#include "oled_font_ascii16x16.h"
#else
#include "oled_font_ascii8x16.h"
#endif
#else
#define OLED_USE_FILE_SYSTEM_ASCII_FONT    0
#include "oled_font_ascii8x16.h"
#endif
#else
#define OLED_USE_FILE_SYSTEM_ASCII_FONT    0
#include "oled_font_ascii8x16.h"
#endif

/* 中文显示功能支持 */
#ifdef CONFIG_MODULE_FS_WRAPPER_ENABLED
#if CONFIG_MODULE_FS_WRAPPER_ENABLED
#include "oled_font_chinese16x16.h"
#include "oled_utf8_parser.h"
#endif
#endif

/* 初始化标志 */
static bool g_oled_initialized = false;

/* OLED I2C从机地址（7位地址，左移1位后为0x78） */
#define OLED_I2C_SLAVE_ADDR  0x3C  /* 7位地址：0x3C，左移1位后为0x78 */

/* ==================== I2C抽象接口定义 ==================== */

/**
 * @brief I2C传输函数指针类型
 * @param[in] slave_addr 从机地址（7位地址）
 * @param[in] data 要发送的数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @return error_code_t 错误码
 */
typedef error_code_t (*OLED_I2C_TransmitFunc_t)(uint8_t slave_addr, const uint8_t *data, uint16_t length);

/**
 * @brief I2C初始化函数指针类型
 * @return error_code_t 错误码
 */
typedef error_code_t (*OLED_I2C_InitFunc_t)(void);

/**
 * @brief I2C反初始化函数指针类型
 * @return error_code_t 错误码
 */
typedef error_code_t (*OLED_I2C_DeinitFunc_t)(void);

/**
 * @brief OLED I2C接口结构体
 */
typedef struct {
    OLED_I2C_TransmitFunc_t transmit;  /**< I2C传输函数 */
    OLED_I2C_InitFunc_t init;         /**< I2C初始化函数 */
    OLED_I2C_DeinitFunc_t deinit;     /**< I2C反初始化函数 */
} OLED_I2C_Interface_t;

/* I2C接口实例 */
static OLED_I2C_Interface_t g_oled_i2c_interface = {0};

/* ==================== 软I2C适配器 ==================== */

#if CONFIG_MODULE_SOFT_I2C_ENABLED
#include "i2c_sw.h"

/**
 * @brief 软I2C传输适配器
 */
static error_code_t OLED_SoftI2C_Transmit(uint8_t slave_addr, const uint8_t *data, uint16_t length)
{
    SoftI2C_Status_t status;
    SoftI2C_Instance_t instance = (SoftI2C_Instance_t)OLED_I2C_SOFT_INSTANCE;
    
    status = SoftI2C_MasterTransmit(instance, slave_addr, data, length, 1000);
    
    if (status == SOFT_I2C_OK)
    {
        return ERROR_OK;
    }
    else
    {
        return ERROR_BASE_OLED - 10;  /* OLED_ERROR_I2C_FAILED */
    }
}

/**
 * @brief 软I2C初始化适配器
 */
static error_code_t OLED_SoftI2C_Init(void)
{
    SoftI2C_Status_t status;
    SoftI2C_Instance_t instance = (SoftI2C_Instance_t)OLED_I2C_SOFT_INSTANCE;
    
    status = I2C_SW_Init(instance);
    
    if (status == SOFT_I2C_OK)
    {
        return ERROR_OK;
    }
    else
    {
        return ERROR_BASE_OLED - 10;  /* OLED_ERROR_I2C_FAILED */
    }
}

/**
 * @brief 软I2C反初始化适配器
 */
static error_code_t OLED_SoftI2C_Deinit(void)
{
    SoftI2C_Status_t status;
    SoftI2C_Instance_t instance = (SoftI2C_Instance_t)OLED_I2C_SOFT_INSTANCE;
    
    status = SoftI2C_Deinit(instance);
    
    if (status == SOFT_I2C_OK)
    {
        return ERROR_OK;
    }
    else
    {
        return ERROR_BASE_OLED - 10;  /* OLED_ERROR_I2C_FAILED */
    }
}
#endif /* CONFIG_MODULE_SOFT_I2C_ENABLED */

/* ==================== 硬I2C适配器 ==================== */

#if CONFIG_MODULE_I2C_ENABLED
#include "i2c_hw.h"

/**
 * @brief 硬I2C传输适配器
 */
static error_code_t OLED_HardI2C_Transmit(uint8_t slave_addr, const uint8_t *data, uint16_t length)
{
    I2C_Status_t status;
    I2C_Instance_t instance = (I2C_Instance_t)OLED_I2C_HARD_INSTANCE;
    
    status = I2C_MasterTransmit(instance, slave_addr, data, length, 1000);
    
    if (status == I2C_OK)
    {
        return ERROR_OK;
    }
    else
    {
        return ERROR_BASE_OLED - 10;  /* OLED_ERROR_I2C_FAILED */
    }
}

/**
 * @brief 硬I2C初始化适配器
 */
static error_code_t OLED_HardI2C_Init(void)
{
    I2C_Status_t status;
    I2C_Instance_t instance = (I2C_Instance_t)OLED_I2C_HARD_INSTANCE;
    
    status = I2C_HW_Init(instance);
    
    if (status == I2C_OK)
    {
        return ERROR_OK;
    }
    else
    {
        return ERROR_BASE_OLED - 10;  /* OLED_ERROR_I2C_FAILED */
    }
}

/**
 * @brief 硬I2C反初始化适配器
 */
static error_code_t OLED_HardI2C_Deinit(void)
{
    I2C_Status_t status;
    I2C_Instance_t instance = (I2C_Instance_t)OLED_I2C_HARD_INSTANCE;
    
    status = I2C_Deinit(instance);
    
    if (status == I2C_OK)
    {
        return ERROR_OK;
    }
    else
    {
        return ERROR_BASE_OLED - 10;  /* OLED_ERROR_I2C_FAILED */
    }
}
#endif /* CONFIG_MODULE_I2C_ENABLED */

/**
 * @brief OLED写命令
 * @param[in] command 命令字节（0~0xFF）
 * @return OLED_Status_t 错误码
 */
static OLED_Status_t OLED_WriteCommand(uint8_t command)
{
    error_code_t status;
    uint8_t data[2];
    
    /* ========== 参数校验 ========== */
    if (!g_oled_initialized)
    {
        return OLED_ERROR_NOT_INITIALIZED;
    }
    
    if (g_oled_i2c_interface.transmit == NULL)
    {
        return OLED_ERROR_GPIO_FAILED;  /* I2C接口未初始化 */
    }
    
    /* 构建数据包：控制字节(0x00) + 命令字节 */
    data[0] = 0x00;  /* 控制字节：0x00表示写命令 */
    data[1] = command;
    
    /* 通过接口发送 */
    status = g_oled_i2c_interface.transmit(OLED_I2C_SLAVE_ADDR, data, 2);
    if (status != ERROR_OK)
    {
        return OLED_ERROR_GPIO_FAILED;  /* 使用GPIO错误码表示I2C失败 */
    }
    
    return OLED_OK;
}

/**
 * @brief OLED写数据
 * @param[in] data 数据字节（0~0xFF）
 * @return OLED_Status_t 错误码
 */
static OLED_Status_t OLED_WriteData(uint8_t data)
{
    error_code_t status;
    uint8_t data_packet[2];
    
    /* ========== 参数校验 ========== */
    if (!g_oled_initialized)
    {
        return OLED_ERROR_NOT_INITIALIZED;
    }
    
    if (g_oled_i2c_interface.transmit == NULL)
    {
        return OLED_ERROR_GPIO_FAILED;  /* I2C接口未初始化 */
    }
    
    /* 构建数据包：控制字节(0x40) + 数据字节 */
    data_packet[0] = 0x40;  /* 控制字节：0x40表示写数据 */
    data_packet[1] = data;
    
    /* 通过接口发送 */
    status = g_oled_i2c_interface.transmit(OLED_I2C_SLAVE_ADDR, data_packet, 2);
    if (status != ERROR_OK)
    {
        return OLED_ERROR_GPIO_FAILED;  /* 使用GPIO错误码表示I2C失败 */
    }
    
    return OLED_OK;
}

/**
 * @brief OLED次方函数（返回X的Y次方）
 * @param[in] x 底数
 * @param[in] y 指数
 * @return uint32_t 结果
 */
static uint32_t OLED_Pow(uint32_t x, uint32_t y)
{
    uint32_t result = 1;
    while (y--)
    {
        result *= x;
    }
    return result;
}

/**
 * @brief 执行SSD1306初始化序列
 * @return OLED_Status_t 错误码
 * @note 内部辅助函数，执行SSD1306的初始化命令序列
 */
static OLED_Status_t OLED_InitSequence(void)
{
    /* SSD1306初始化序列（保持用户代码的序列，不添加额外延时） */
    OLED_WriteCommand(0xAE);  /* 关闭显示 */
    
    OLED_WriteCommand(0xD5);  /* 设置显示时钟分频比/振荡器频率 */
    OLED_WriteCommand(0x80);
    
    OLED_WriteCommand(0xA8);  /* 设置多路复用率 */
    OLED_WriteCommand(0x3F);  /* 0x3F = 1/64 */
    
    OLED_WriteCommand(0xD3);  /* 设置显示偏移 */
    OLED_WriteCommand(0x00);
    
    OLED_WriteCommand(0x40);  /* 设置显示开始行 */
    
    OLED_WriteCommand(0xA1);  /* 设置左右方向（0xA1=正常，0xA0=左右反置） */
    OLED_WriteCommand(0xC8);  /* 设置上下方向（0xC8=正常，0xC0=上下反置） */
    
    OLED_WriteCommand(0xDA);  /* 设置COM引脚硬件配置 */
    OLED_WriteCommand(0x12);
    
    OLED_WriteCommand(0x81);  /* 设置对比度控制 */
    OLED_WriteCommand(0xCF);
    
    OLED_WriteCommand(0xD9);  /* 设置预充电周期 */
    OLED_WriteCommand(0xF1);
    
    OLED_WriteCommand(0xDB);  /* 设置VCOMH取消选择级别 */
    OLED_WriteCommand(0x30);
    
    OLED_WriteCommand(0xA4);  /* 设置整个显示打开/关闭 */
    OLED_WriteCommand(0xA6);  /* 设置正常/倒转显示 */
    
    OLED_WriteCommand(0x8D);  /* 设置电荷泵 */
    OLED_WriteCommand(0x14);  /* 0x14=启用，0x10=禁用 */
    
    OLED_WriteCommand(0xAF);  /* 开启显示 */
    
    /* 清屏（失败不影响初始化成功） */
    (void)OLED_Clear();
    
    return OLED_OK;
}

/**
 * @brief OLED初始化
 * @return OLED_Status_t 错误码
 * @note 根据board.h中的OLED_I2C_TYPE配置选择I2C接口（软I2C或硬I2C）
 * @note 基于用户提供的可工作代码，保持初始化序列不变
 */
OLED_Status_t OLED_Init(void)
{
    uint32_t i, j;
    
    /* 防止重复初始化 */
    if (g_oled_initialized)
    {
        return OLED_OK;
    }
    
    /* 上电延时（约1秒，使用空循环，保持用户代码的延时方式） */
    for (i = 0; i < 1000; i++)
    {
        for (j = 0; j < 1000; j++);
    }
    
    /* 根据配置选择I2C接口 */
    if (OLED_I2C_TYPE == OLED_I2C_TYPE_SOFTWARE)
    {
#if CONFIG_MODULE_SOFT_I2C_ENABLED
        OLED_Status_t status;
        
        /* 使用软I2C接口 */
        g_oled_i2c_interface.transmit = OLED_SoftI2C_Transmit;
        g_oled_i2c_interface.init = OLED_SoftI2C_Init;
        g_oled_i2c_interface.deinit = OLED_SoftI2C_Deinit;
        
        /* 初始化软I2C */
        status = (OLED_Status_t)g_oled_i2c_interface.init();
        if (status != OLED_OK)
        {
            return status;
        }
        /* 如果执行到这里，说明I2C初始化成功 */
        /* 设置初始化标志，允许后续写命令操作 */
        g_oled_initialized = true;
        
        /* 执行SSD1306初始化序列 */
        return OLED_InitSequence();
#else
        /* 软I2C模块未启用 */
        return OLED_ERROR_GPIO_FAILED;
#endif
    }
    else if (OLED_I2C_TYPE == OLED_I2C_TYPE_HARDWARE)
    {
#if CONFIG_MODULE_I2C_ENABLED
        OLED_Status_t status;
        
        /* 使用硬I2C接口 */
        g_oled_i2c_interface.transmit = OLED_HardI2C_Transmit;
        g_oled_i2c_interface.init = OLED_HardI2C_Init;
        g_oled_i2c_interface.deinit = OLED_HardI2C_Deinit;
        
        /* 初始化硬I2C */
        status = (OLED_Status_t)g_oled_i2c_interface.init();
        if (status != OLED_OK)
        {
            return status;
        }
        /* 如果执行到这里，说明I2C初始化成功 */
        /* 设置初始化标志，允许后续写命令操作 */
        g_oled_initialized = true;
        
        /* 执行SSD1306初始化序列 */
        return OLED_InitSequence();
#else
        /* 硬I2C模块未启用 */
        return OLED_ERROR_GPIO_FAILED;
#endif
    }
    else
    {
        /* 不支持的接口类型 */
        return OLED_ERROR_INVALID_PARAM;
    }
}

/**
 * @brief OLED反初始化
 * @return OLED_Status_t 错误码
 */
OLED_Status_t OLED_Deinit(void)
{
    if (!g_oled_initialized)
    {
        return OLED_OK;
    }
    
    /* 关闭显示 */
    OLED_WriteCommand(0xAE);
    
    /* 反初始化I2C接口 */
    if (g_oled_i2c_interface.deinit != NULL)
    {
        g_oled_i2c_interface.deinit();
    }
    
    /* 清空接口 */
    g_oled_i2c_interface.transmit = NULL;
    g_oled_i2c_interface.init = NULL;
    g_oled_i2c_interface.deinit = NULL;
    
    g_oled_initialized = false;
    return OLED_OK;
}

/**
 * @brief OLED设置光标位置
 * @param[in] y 页地址（0~7）
 * @param[in] x 列地址（0~127）
 * @return OLED_Status_t 错误码
 */
OLED_Status_t OLED_SetCursor(uint8_t y, uint8_t x)
{
    OLED_Status_t status;
    
    /* ========== 参数校验 ========== */
    if (!g_oled_initialized)
    {
        return OLED_ERROR_NOT_INITIALIZED;
    }
    
    if (y > 7 || x > 127)
    {
        return OLED_ERROR_INVALID_PARAM;
    }
    
    /* 设置页地址（Y位置） */
    status = OLED_WriteCommand(0xB0 | y);
    if (status != OLED_OK)
    {
        return status;
    }
    
    /* 设置列地址高4位（X位置高4位） */
    status = OLED_WriteCommand(0x10 | ((x & 0xF0) >> 4));
    if (status != OLED_OK)
    {
        return status;
    }
    
    /* 设置列地址低4位（X位置低4位） */
    status = OLED_WriteCommand(0x00 | (x & 0x0F));
    if (status != OLED_OK)
    {
        return status;
    }
    
    return OLED_OK;
}

/**
 * @brief OLED清屏
 * @return OLED_Status_t 错误码
 * @note 将整个屏幕清空为黑色
 */
OLED_Status_t OLED_Clear(void)
{
    uint8_t i, j;
    OLED_Status_t status;
    
    /* ========== 参数校验 ========== */
    if (!g_oled_initialized)
    {
        return OLED_ERROR_NOT_INITIALIZED;
    }
    
    /* 遍历8个页 */
    for (j = 0; j < 8; j++)
    {
        /* 设置光标到当前页起始位置 */
        status = OLED_SetCursor(j, 0);
        if (status != OLED_OK)
        {
            return status;
        }
        
        /* 遍历128列，写入0 */
        for (i = 0; i < 128; i++)
        {
            status = OLED_WriteData(0x00);
            if (status != OLED_OK)
            {
                return status;
            }
        }
    }
    
    return OLED_OK;
}

/**
 * @brief OLED显示一个字符
 * @param[in] line 行号（1~4）
 * @param[in] column 列号（1~16）
 * @param[in] ch ASCII字符
 * @return OLED_Status_t 错误码
 */
OLED_Status_t OLED_ShowChar(uint8_t line, uint8_t column, char ch)
{
    uint8_t i;
    uint8_t char_index;  /* 字符在字库中的索引 */
    OLED_Status_t status;
    
    /* ========== 参数校验 ========== */
    if (!g_oled_initialized)
    {
        return OLED_ERROR_NOT_INITIALIZED;
    }
    
    if (line < 1 || line > 4 || column < 1 || column > 16)
    {
        return OLED_ERROR_INVALID_PARAM;
    }
    
#if OLED_USE_FILE_SYSTEM_ASCII_FONT
    uint8_t font_data[OLED_ASCII_FONT_CHAR_SIZE];
    OLED_AsciiFont_Status_t font_status;
    
    /* 使用文件系统字库：获取字符索引 */
    font_status = OLED_AsciiFont_GetIndex(ch, &char_index);
    if (font_status != OLED_ASCII_FONT_OK)
    {
        return OLED_ERROR_INVALID_PARAM;  /* 不支持的字符 */
    }
    
    /* 从文件系统读取字模数据 */
    font_status = OLED_AsciiFont_GetData(char_index, font_data);
    if (font_status != OLED_ASCII_FONT_OK)
    {
        return OLED_ERROR_INVALID_PARAM;  /* 读取失败 */
    }
#else
    /* 使用内置字库：字符索引计算 */
    if ((unsigned char)ch == 0xB0 || (unsigned char)ch == 176)  /* 度符号的多种编码 */
    {
        char_index = 95;  /* 度符号在字库中的索引 */
    }
    else if (ch >= ' ' && ch <= '~')
    {
        char_index = ch - ' ';  /* 标准ASCII字符（索引0-94） */
    }
    else
    {
        return OLED_ERROR_INVALID_PARAM;  /* 不支持的字符 */
    }
#endif
    
    /* 设置光标到字符上半部分 */
    status = OLED_SetCursor((line - 1) * 2, (column - 1) * 8);
    if (status != OLED_OK)
    {
        return status;
    }
    
    /* 发送上半部分8字节字模数据 */
    for (i = 0; i < 8; i++)
    {
#if OLED_USE_FILE_SYSTEM_ASCII_FONT
        status = OLED_WriteData(font_data[i]);
#else
        status = OLED_WriteData(OLED_F8x16[char_index][i]);
#endif
        if (status != OLED_OK)
        {
            return status;
        }
    }
    
    /* 设置光标到字符下半部分 */
    status = OLED_SetCursor((line - 1) * 2 + 1, (column - 1) * 8);
    if (status != OLED_OK)
    {
        return status;
    }
    
    /* 发送下半部分8字节字模数据 */
    for (i = 0; i < 8; i++)
    {
#if OLED_USE_FILE_SYSTEM_ASCII_FONT
        status = OLED_WriteData(font_data[i + 8]);
#else
        status = OLED_WriteData(OLED_F8x16[char_index][i + 8]);
#endif
        if (status != OLED_OK)
        {
            return status;
        }
    }
    
    return OLED_OK;
}

/**
 * @brief OLED显示字符串
 * @param[in] line 行号（1~4）
 * @param[in] column 列号（1~16）
 * @param[in] str 字符串指针
 * @return OLED_Status_t 错误码
 */
OLED_Status_t OLED_ShowString(uint8_t line, uint8_t column, const char *str)
{
    uint8_t i;
    OLED_Status_t status;
    
    /* ========== 参数校验 ========== */
    if (!g_oled_initialized)
    {
        return OLED_ERROR_NOT_INITIALIZED;
    }
    
    if (str == NULL)
    {
        return OLED_ERROR_INVALID_PARAM;
    }
    
    /* 遍历字符串直到遇到结束符'\0' */
    for (i = 0; str[i] != '\0'; i++)
    {
        /* 检查是否超出显示范围 */
        if (column + i > 16)
        {
            break;  /* 超出范围，停止显示 */
        }
        
        status = OLED_ShowChar(line, column + i, str[i]);
        if (status != OLED_OK)
        {
            return status;
        }
    }
    
    return OLED_OK;
}

/**
 * @brief OLED显示无符号十进制数
 * @param[in] line 行号（1~4）
 * @param[in] column 列号（1~16）
 * @param[in] number 数字（0~4294967295）
 * @param[in] length 显示长度（1~10）
 * @return OLED_Status_t 错误码
 */
OLED_Status_t OLED_ShowNum(uint8_t line, uint8_t column, uint32_t number, uint8_t length)
{
    uint8_t i;
    OLED_Status_t status;
    
    /* ========== 参数校验 ========== */
    if (!g_oled_initialized)
    {
        return OLED_ERROR_NOT_INITIALIZED;
    }
    
    if (length < 1 || length > 10)
    {
        return OLED_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否超出显示范围 */
    if (column + length > 16)
    {
        return OLED_ERROR_INVALID_PARAM;
    }
    
    /* 从高位到低位依次显示 */
    for (i = 0; i < length; i++)
    {
        status = OLED_ShowChar(line, column + i, 
                              (char)(number / OLED_Pow(10, length - i - 1) % 10 + '0'));
        if (status != OLED_OK)
        {
            return status;
        }
    }
    
    return OLED_OK;
}

/**
 * @brief OLED显示有符号十进制数
 * @param[in] line 行号（1~4）
 * @param[in] column 列号（1~16）
 * @param[in] number 数字（-2147483648~2147483647）
 * @param[in] length 显示长度（1~10）
 * @return OLED_Status_t 错误码
 */
OLED_Status_t OLED_ShowSignedNum(uint8_t line, uint8_t column, int32_t number, uint8_t length)
{
    uint8_t i;
    uint32_t number1;
    OLED_Status_t status;
    
    /* ========== 参数校验 ========== */
    if (!g_oled_initialized)
    {
        return OLED_ERROR_NOT_INITIALIZED;
    }
    
    if (length < 1 || length > 10)
    {
        return OLED_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否超出显示范围（符号位+数字位） */
    if (column + length + 1 > 16)
    {
        return OLED_ERROR_INVALID_PARAM;
    }
    
    /* 处理符号位 */
    if (number >= 0)
    {
        status = OLED_ShowChar(line, column, '+');
        number1 = (uint32_t)number;
    }
    else
    {
        status = OLED_ShowChar(line, column, '-');
        number1 = (uint32_t)(-number);
    }
    
    if (status != OLED_OK)
    {
        return status;
    }
    
    /* 显示数值部分 */
    for (i = 0; i < length; i++)
    {
        status = OLED_ShowChar(line, column + i + 1, 
                              (char)(number1 / OLED_Pow(10, length - i - 1) % 10 + '0'));
        if (status != OLED_OK)
        {
            return status;
        }
    }
    
    return OLED_OK;
}

/**
 * @brief OLED显示十六进制数
 * @param[in] line 行号（1~4）
 * @param[in] column 列号（1~16）
 * @param[in] number 数字（0~0xFFFFFFFF）
 * @param[in] length 显示长度（1~8）
 * @return OLED_Status_t 错误码
 */
OLED_Status_t OLED_ShowHexNum(uint8_t line, uint8_t column, uint32_t number, uint8_t length)
{
    uint8_t i, single_number;
    OLED_Status_t status;
    
    /* ========== 参数校验 ========== */
    if (!g_oled_initialized)
    {
        return OLED_ERROR_NOT_INITIALIZED;
    }
    
    if (length < 1 || length > 8)
    {
        return OLED_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否超出显示范围 */
    if (column + length > 16)
    {
        return OLED_ERROR_INVALID_PARAM;
    }
    
    /* 从高位到低位依次显示 */
    for (i = 0; i < length; i++)
    {
        /* 提取当前位的值（0~15） */
        single_number = (uint8_t)(number / OLED_Pow(16, length - i - 1) % 16);
        
        /* 0~9显示数字，10~15显示A~F */
        if (single_number < 10)
        {
            status = OLED_ShowChar(line, column + i, (char)(single_number + '0'));
        }
        else
        {
            status = OLED_ShowChar(line, column + i, (char)(single_number - 10 + 'A'));
        }
        
        if (status != OLED_OK)
        {
            return status;
        }
    }
    
    return OLED_OK;
}

/**
 * @brief OLED显示二进制数
 * @param[in] line 行号（1~4）
 * @param[in] column 列号（1~16）
 * @param[in] number 数字（0~0xFFFF，length<=16）
 * @param[in] length 显示长度（1~16）
 * @return OLED_Status_t 错误码
 */
OLED_Status_t OLED_ShowBinNum(uint8_t line, uint8_t column, uint32_t number, uint8_t length)
{
    uint8_t i;
    OLED_Status_t status;
    
    /* ========== 参数校验 ========== */
    if (!g_oled_initialized)
    {
        return OLED_ERROR_NOT_INITIALIZED;
    }
    
    if (length < 1 || length > 16)
    {
        return OLED_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否超出显示范围 */
    if (column + length > 16)
    {
        return OLED_ERROR_INVALID_PARAM;
    }
    
    /* 从高位到低位依次显示 */
    for (i = 0; i < length; i++)
    {
        status = OLED_ShowChar(line, column + i, 
                              (char)(number / OLED_Pow(2, length - i - 1) % 2 + '0'));
        if (status != OLED_OK)
        {
            return status;
        }
    }
    
    return OLED_OK;
}

/* ==================== 中文显示功能实现 ==================== */

#ifdef CONFIG_MODULE_FS_WRAPPER_ENABLED
#if CONFIG_MODULE_FS_WRAPPER_ENABLED

/**
 * @brief OLED显示单个中文字符（16x16点阵）
 */
OLED_Status_t OLED_ShowChineseChar(uint8_t line, uint8_t column, uint16_t gb2312_code)
{
    uint8_t i, j;
    uint8_t font_data[OLED_CHINESE_FONT_CHAR_SIZE];  /* 字模数据缓冲区（栈上） */
    OLED_Status_t status;
    OLED_ChineseFont_Status_t font_status;
    
    /* ========== 参数校验 ========== */
    if (!g_oled_initialized)
    {
        return OLED_ERROR_NOT_INITIALIZED;
    }
    
    if (line < 1 || line > 4 || column < 1 || column > 8)
    {
        return OLED_ERROR_INVALID_PARAM;
    }
    
    /* ========== 禁止在第1行显示中文（第1行保留用于显示方法编号） ========== */
    #ifdef OLED_CHINESE_METHOD_AUTO_CYCLE
    if (line == 1)
    {
        return OLED_OK;  /* 直接返回，禁止在第1行显示中文 */
    }
    #endif
    
    /* 检查GB2312编码是否有效 */
    if (!OLED_ChineseFont_IsValidGB2312(gb2312_code))
    {
        return OLED_ERROR_INVALID_PARAM;
    }
    
    /* 从文件系统读取字模数据 */
    font_status = OLED_ChineseFont_GetData(gb2312_code, font_data);
    if (font_status != OLED_CHINESE_FONT_OK)
    {
        return OLED_ERROR_INVALID_PARAM;  /* 读取失败 */
    }
    
    /* 计算显示位置（中文字符占用2列，16像素宽度） */
    uint8_t x_pos = (column - 1) * 16;  /* 每列16像素 */
    
    /* 16x16点阵按页显示：每页8行，共2页 */
    /* 
     * 字模数据格式说明（常见格式）：
     * 格式1：按列存储，32字节 = 16列 × 2字节（每列：页0字节+页1字节）
     *   字节0=列0页0, 字节1=列0页1, 字节2=列1页0, 字节3=列1页1, ...
     *   这种格式可以直接发送，无需重组
     * 
     * 格式2：按行存储，32字节 = 16行 × 2字节（每行：左字节+右字节）
     *   字节0,1=行0, 字节2,3=行1, ...
     *   需要按列重组
     * 
     * 先尝试格式1（按列存储），如果显示不对，再尝试格式2
     */
    for (j = 0; j < 2; j++)
    {
        /* 设置光标到当前页的起始位置 */
        status = OLED_SetCursor((line - 1) * 2 + j, x_pos);
        if (status != OLED_OK)
        {
            return status;
        }
        
        /* ========== 多种显示方法，通过修改 METHOD_NUM 切换测试 ========== */
        /* 方法说明：
         * 方法1-4：按行存储格式的各种变体
         * 方法5-7：按列存储格式的各种变体
         * 方法8-13：组合反转方法
         * 方法14：按列存储 + 反转页顺序 + 反转列顺序
         * 方法15：方法11的左右翻转版本
         * 
         * 如果定义了 OLED_CHINESE_METHOD_AUTO_CYCLE，则使用静态变量自动循环
         * 否则使用 METHOD_NUM 宏定义的方法
         */
        #ifndef OLED_CHINESE_METHOD_AUTO_CYCLE
        #define METHOD_NUM  11  /* 修改这个数字来切换方法（1-15），方法11已确认正确 */
        #endif
        
        #ifdef OLED_CHINESE_METHOD_AUTO_CYCLE
        /* 自动循环模式：使用静态变量存储当前方法编号 */
        static uint8_t g_current_method = 1;
        static uint32_t g_method_switch_timer = 0;
        static uint8_t g_method_initialized = 0;
        static uint8_t g_last_displayed_method = 0;
        uint32_t current_tick = Delay_GetTick();
        
        /* 初始化定时器 */
        if (!g_method_initialized)
        {
            g_method_switch_timer = current_tick;
            g_method_initialized = 1;
            g_last_displayed_method = 0;  /* 强制第一次显示 */
        }
        
        /* 每3秒切换一次方法 */
        if (Delay_GetElapsed(current_tick, g_method_switch_timer) >= 3000)
        {
            g_current_method++;
            if (g_current_method > 15)
            {
                g_current_method = 1;  /* 循环回到方法1 */
            }
            g_method_switch_timer = current_tick;
            g_last_displayed_method = 0;  /* 标记需要更新显示 */
        }
        
        /* 如果方法切换了，更新第1行显示 */
        if (g_last_displayed_method != g_current_method)
            {
                /* 第1行：先清除整行的所有像素（清除页0和页1的所有128列） */
                uint8_t page, col;
                for (page = 0; page < 2; page++)  /* 第1行对应页0和页1 */
                {
                    status = OLED_SetCursor(page, 0);
                    if (status != OLED_OK) return status;
                    for (col = 0; col < 128; col++)  /* 清除所有128列 */
                    {
                        status = OLED_WriteData(0x00);  /* 写入0清除像素 */
                        if (status != OLED_OK) return status;
                    }
                }
                
                /* 显示方法编号 */
                OLED_ShowString(1, 1, "Method: ");
                if (g_current_method < 10)
                {
                    OLED_ShowNum(1, 9, g_current_method, 1);
                }
                else
                {
                    OLED_ShowNum(1, 9, g_current_method, 2);
                }
            
                g_last_displayed_method = g_current_method;
                
                /* 重要：如果当前调用是要显示在第1行，清除后直接返回，避免覆盖方法编号 */
                if (line == 1)
                {
                    return OLED_OK;
                }
            }
        #endif
        
        /* 确定使用哪个方法（编译时或运行时） */
        #ifdef OLED_CHINESE_METHOD_AUTO_CYCLE
        uint8_t method_num = g_current_method;
        #else
        uint8_t method_num = METHOD_NUM;
        #endif
        
        /* 使用运行时判断而不是预处理器判断 */
        if (method_num == 1)
        /* 方法1：方法15的左右翻转（按行存储 + 反转字节顺序 + 反转位顺序 + 反转列顺序） */
        for (i = 0; i < 16; i++)
        {
            uint8_t col_index = 15 - i;  /* 反转列顺序：左右翻转 */
            uint8_t byte_val = 0;
            uint8_t row;
            for (row = 0; row < 8; row++)
            {
                uint8_t row_index = j * 8 + row;
                uint8_t byte_offset = row_index * 2;
                uint8_t src_byte = (col_index < 8) ? (byte_offset + 1) : byte_offset;
                uint8_t src_bit = (col_index < 8) ? col_index : (col_index - 8);
                uint8_t dst_bit = row;
                if (src_byte < OLED_CHINESE_FONT_CHAR_SIZE)
                {
                    if (font_data[src_byte] & (1 << src_bit))
                        byte_val |= (1 << dst_bit);
                }
            }
            status = OLED_WriteData(byte_val);
            if (status != OLED_OK) return status;
        }
        
        else if (method_num == 2)
        /* 方法2：按列存储 + 反转页顺序（上下颠倒） */
        for (i = 0; i < 16; i++)
        {
            uint8_t page_index = 1 - j;
            uint8_t byte_index = i * 2 + page_index;
            if (byte_index < OLED_CHINESE_FONT_CHAR_SIZE)
            {
                status = OLED_WriteData(font_data[byte_index]);
                if (status != OLED_OK) return status;
            }
        }
        
        else if (method_num == 3)
        /* 方法3：按列存储 + 反转列顺序 + 反转字节内的位顺序（完整字符左右翻转） */
        for (i = 0; i < 16; i++)
        {
            uint8_t col_index = 15 - i;  /* 反转列顺序：从右到左（左右翻转） */
            uint8_t byte_index = col_index * 2 + j;  /* 保持页顺序不变 */
            if (byte_index < OLED_CHINESE_FONT_CHAR_SIZE)
            {
                uint8_t byte_val = font_data[byte_index];
                uint8_t reversed = 0;
                uint8_t k;
                /* 反转字节内的位顺序（完整字符左右翻转） */
                for (k = 0; k < 8; k++)
                {
                    if (byte_val & (1 << k))
                        reversed |= (1 << (7 - k));
                }
                status = OLED_WriteData(reversed);
                if (status != OLED_OK) return status;
            }
        }
        
        else if (method_num == 4)
        /* 方法4：按行存储 + 反转字节顺序 + 反转位顺序（组合） */
        for (i = 0; i < 16; i++)
        {
            uint8_t byte_val = 0;
            uint8_t row;
            for (row = 0; row < 8; row++)
            {
                uint8_t row_index = j * 8 + row;
                uint8_t byte_offset = row_index * 2;
                uint8_t src_byte = (i < 8) ? (byte_offset + 1) : byte_offset;
                uint8_t src_bit = (i < 8) ? i : (i - 8);
                uint8_t dst_bit = row;
                if (src_byte < OLED_CHINESE_FONT_CHAR_SIZE)
                {
                    if (font_data[src_byte] & (1 << src_bit))
                        byte_val |= (1 << dst_bit);
                }
            }
            status = OLED_WriteData(byte_val);
            if (status != OLED_OK) return status;
        }
        
        else if (method_num == 5)
        /* 方法5：按列存储 + 反转字节内的位顺序 */
        for (i = 0; i < 16; i++)
        {
            uint8_t byte_index = i * 2 + j;
            if (byte_index < OLED_CHINESE_FONT_CHAR_SIZE)
            {
                uint8_t byte_val = font_data[byte_index];
                uint8_t reversed = 0;
                uint8_t k;
                for (k = 0; k < 8; k++)
                {
                    if (byte_val & (1 << k))
                        reversed |= (1 << (7 - k));
                }
                status = OLED_WriteData(reversed);
                if (status != OLED_OK) return status;
            }
        }
        
        else if (method_num == 6)
        /* 方法6：按行存储 + 正常按列重组 */
        for (i = 0; i < 16; i++)
        {
            uint8_t byte_val = 0;
            uint8_t row;
            for (row = 0; row < 8; row++)
            {
                uint8_t row_index = j * 8 + row;
                uint8_t byte_offset = row_index * 2;
                uint8_t src_byte = (i < 8) ? byte_offset : (byte_offset + 1);
                uint8_t src_bit = (i < 8) ? (7 - i) : (7 - (i - 8));
                uint8_t dst_bit = 7 - row;
                if (src_byte < OLED_CHINESE_FONT_CHAR_SIZE)
                {
                    if (font_data[src_byte] & (1 << src_bit))
                        byte_val |= (1 << dst_bit);
                }
            }
            status = OLED_WriteData(byte_val);
            if (status != OLED_OK) return status;
        }
        
        else if (method_num == 7)
        /* 方法7：按行存储 + 反转列顺序（从右到左） */
        for (i = 0; i < 16; i++)
        {
            uint8_t byte_val = 0;
            uint8_t row;
            uint8_t col_index = 15 - i;
            for (row = 0; row < 8; row++)
            {
                uint8_t row_index = j * 8 + row;
                uint8_t byte_offset = row_index * 2;
                uint8_t src_byte = (col_index < 8) ? byte_offset : (byte_offset + 1);
                uint8_t src_bit = (col_index < 8) ? (7 - col_index) : (7 - (col_index - 8));
                uint8_t dst_bit = 7 - row;
                if (src_byte < OLED_CHINESE_FONT_CHAR_SIZE)
                {
                    if (font_data[src_byte] & (1 << src_bit))
                        byte_val |= (1 << dst_bit);
                }
            }
            status = OLED_WriteData(byte_val);
            if (status != OLED_OK) return status;
        }
        
        else if (method_num == 14)
        /* 方法14：按行存储 + 反转列和行顺序（同时反转） */
        for (i = 0; i < 16; i++)
        {
            uint8_t byte_val = 0;
            uint8_t row;
            uint8_t col_index = 15 - i;
            for (row = 0; row < 8; row++)
            {
                uint8_t row_in_page = 7 - row;
                uint8_t row_index = j * 8 + row_in_page;
                uint8_t byte_offset = row_index * 2;
                uint8_t src_byte = (col_index < 8) ? byte_offset : (byte_offset + 1);
                uint8_t src_bit = (col_index < 8) ? (7 - col_index) : (7 - (col_index - 8));
                uint8_t dst_bit = row;
                if (src_byte < OLED_CHINESE_FONT_CHAR_SIZE)
                {
                    if (font_data[src_byte] & (1 << src_bit))
                        byte_val |= (1 << dst_bit);
                }
            }
            status = OLED_WriteData(byte_val);
            if (status != OLED_OK) return status;
        }
        
        else if (method_num == 8)
        /* 方法8：按行存储 + 反转行顺序（上下颠倒） */
        for (i = 0; i < 16; i++)
        {
            uint8_t byte_val = 0;
            uint8_t row;
            for (row = 0; row < 8; row++)
            {
                uint8_t row_in_page = 7 - row;
                uint8_t row_index = j * 8 + row_in_page;
                uint8_t byte_offset = row_index * 2;
                uint8_t src_byte = (i < 8) ? byte_offset : (byte_offset + 1);
                uint8_t src_bit = (i < 8) ? (7 - i) : (7 - (i - 8));
                uint8_t dst_bit = row;
                if (src_byte < OLED_CHINESE_FONT_CHAR_SIZE)
                {
                    if (font_data[src_byte] & (1 << src_bit))
                        byte_val |= (1 << dst_bit);
                }
            }
            status = OLED_WriteData(byte_val);
            if (status != OLED_OK) return status;
        }
        
        else if (method_num == 9)
        /* 方法9：按行存储 + 反转字节内的位顺序（左右颠倒每个字节） */
        for (i = 0; i < 16; i++)
        {
            uint8_t byte_val = 0;
            uint8_t row;
            for (row = 0; row < 8; row++)
            {
                uint8_t row_index = j * 8 + row;
                uint8_t byte_offset = row_index * 2;
                uint8_t src_byte = (i < 8) ? byte_offset : (byte_offset + 1);
                uint8_t src_bit = (i < 8) ? i : (i - 8);
                uint8_t dst_bit = 7 - row;
                if (src_byte < OLED_CHINESE_FONT_CHAR_SIZE)
                {
                    if (font_data[src_byte] & (1 << src_bit))
                        byte_val |= (1 << dst_bit);
                }
            }
            status = OLED_WriteData(byte_val);
            if (status != OLED_OK) return status;
        }
        
        else if (method_num == 10)
        /* 方法10：按列存储格式（直接发送） */
        for (i = 0; i < 16; i++)
        {
            uint8_t byte_index = i * 2 + j;
            if (byte_index < OLED_CHINESE_FONT_CHAR_SIZE)
            {
                status = OLED_WriteData(font_data[byte_index]);
                if (status != OLED_OK) return status;
            }
        }
        
        else if (method_num == 11)
        /* 方法11：按列存储 + 反转列顺序（从右到左） */
        for (i = 0; i < 16; i++)
        {
            uint8_t col_index = 15 - i;
            uint8_t byte_index = col_index * 2 + j;
            if (byte_index < OLED_CHINESE_FONT_CHAR_SIZE)
            {
                status = OLED_WriteData(font_data[byte_index]);
                if (status != OLED_OK) return status;
            }
        }
        
        else if (method_num == 15)
        /* 方法15：按行存储 + 正常重组（使用不同的位提取方式） */
        for (i = 0; i < 16; i++)
        {
            uint8_t byte_val = 0;
            uint8_t row;
            for (row = 0; row < 8; row++)
            {
                uint8_t row_index = j * 8 + row;
                uint8_t byte_offset = row_index * 2;
                uint8_t src_byte = (i < 8) ? byte_offset : (byte_offset + 1);
                uint8_t src_bit = (i < 8) ? i : (i - 8);
                uint8_t dst_bit = row;
                if (src_byte < OLED_CHINESE_FONT_CHAR_SIZE)
                {
                    if (font_data[src_byte] & (1 << src_bit))
                        byte_val |= (1 << dst_bit);
                }
            }
            status = OLED_WriteData(byte_val);
            if (status != OLED_OK) return status;
        }
        
        else if (method_num == 12)
        /* 方法12：按行存储 + 反转字节顺序（左右字节互换） */
        for (i = 0; i < 16; i++)
        {
            uint8_t byte_val = 0;
            uint8_t row;
            for (row = 0; row < 8; row++)
            {
                uint8_t row_index = j * 8 + row;
                uint8_t byte_offset = row_index * 2;
                uint8_t src_byte = (i < 8) ? (byte_offset + 1) : byte_offset;
                uint8_t src_bit = (i < 8) ? (7 - i) : (7 - (i - 8));
                uint8_t dst_bit = 7 - row;
                if (src_byte < OLED_CHINESE_FONT_CHAR_SIZE)
                {
                    if (font_data[src_byte] & (1 << src_bit))
                        byte_val |= (1 << dst_bit);
                }
            }
            status = OLED_WriteData(byte_val);
            if (status != OLED_OK) return status;
        }
        
        else if (method_num == 13)
        /* 方法13：按行存储 + 反转位顺序（每个字节内的位左右颠倒） */
        for (i = 0; i < 16; i++)
        {
            uint8_t byte_val = 0;
            uint8_t row;
            for (row = 0; row < 8; row++)
            {
                uint8_t row_index = j * 8 + row;
                uint8_t byte_offset = row_index * 2;
                uint8_t src_byte = (i < 8) ? byte_offset : (byte_offset + 1);
                uint8_t src_bit = (i < 8) ? i : (i - 8);
                uint8_t dst_bit = row;
                if (src_byte < OLED_CHINESE_FONT_CHAR_SIZE)
                {
                    if (font_data[src_byte] & (1 << src_bit))
                        byte_val |= (1 << dst_bit);
                }
            }
            status = OLED_WriteData(byte_val);
            if (status != OLED_OK) return status;
        }
        
        else
        {
            /* 无效的方法编号，使用方法1作为默认 */
            /* 方法1：按行存储 + 正常按列重组 */
            for (i = 0; i < 16; i++)
            {
                uint8_t byte_val = 0;
                uint8_t row;
                for (row = 0; row < 8; row++)
                {
                    uint8_t row_index = j * 8 + row;
                    uint8_t byte_offset = row_index * 2;
                    uint8_t src_byte = (i < 8) ? byte_offset : (byte_offset + 1);
                    uint8_t src_bit = (i < 8) ? (7 - i) : (7 - (i - 8));
                    uint8_t dst_bit = 7 - row;
                    if (src_byte < OLED_CHINESE_FONT_CHAR_SIZE)
                    {
                        if (font_data[src_byte] & (1 << src_bit))
                            byte_val |= (1 << dst_bit);
                    }
                }
                status = OLED_WriteData(byte_val);
                if (status != OLED_OK) return status;
            }
        }
    }
    
    return OLED_OK;
}

/**
 * @brief OLED显示GB2312编码字符串（支持中英文混显）
 */
OLED_Status_t OLED_ShowStringGB2312(uint8_t line, uint8_t column, const char *str)
{
    const char *p = str;
    uint8_t current_column = column;
    uint8_t current_line = line;
    OLED_Status_t status;
    OLED_CharType_t char_type;
    uint8_t char_len;
    uint16_t gb2312_code;
    
    /* ========== 参数校验 ========== */
    if (!g_oled_initialized)
    {
        return OLED_ERROR_NOT_INITIALIZED;
    }
    
    if (str == NULL)
    {
        return OLED_ERROR_INVALID_PARAM;
    }
    
    if (line < 1 || line > 4 || column < 1 || column > 16)
    {
        return OLED_ERROR_INVALID_PARAM;
    }
    
    /* 遍历字符串 */
    while (*p != '\0')
    {
        /* 获取下一个字符信息 */
        if (!OLED_UTF8_GetNextCharInfo(p, &char_type, &char_len))
        {
            break;  /* 字符串结束或无效字符 */
        }
        
        /* 检查是否超出显示范围 */
        if (current_line > 4)
        {
            break;  /* 超出显示范围 */
        }
        
        /* 根据字符类型处理 */
        if (char_type == OLED_CHAR_TYPE_ASCII)
        {
            /* ASCII字符：占用1列 */
            if (current_column > 16)
            {
                /* 换行 */
                current_line++;
                current_column = 1;
                if (current_line > 4)
                {
                    break;
                }
            }
            
            status = OLED_ShowChar(current_line, current_column, *p);
            if (status != OLED_OK)
            {
                return status;
            }
            
            current_column++;
        }
        else if (char_type == OLED_CHAR_TYPE_GB2312_CHINESE)
        {
            /* GB2312中文字符：占用2列 */
            if (current_column > 15)  /* 中文字符需要2列，所以最多到15 */
            {
                /* 换行 */
                current_line++;
                current_column = 1;
                if (current_line > 4)
                {
                    break;
                }
            }
            
            /* 提取GB2312编码（高字节在前） */
            gb2312_code = ((uint16_t)(uint8_t)p[0] << 8) | (uint8_t)p[1];
            
            /* 转换为列号（中文字符列号范围1-8） */
            uint8_t chinese_column = (current_column + 1) / 2;  /* 转换为1-8范围 */
            if (chinese_column < 1) chinese_column = 1;
            if (chinese_column > 8) chinese_column = 8;
            
            status = OLED_ShowChineseChar(current_line, chinese_column, gb2312_code);
            if (status != OLED_OK)
            {
                return status;
            }
            
            current_column += 2;  /* 中文字符占用2列 */
        }
        else
        {
            /* 其他类型字符：跳过 */
        }
        
        /* 移动到下一个字符 */
        p += char_len;
    }
    
    return OLED_OK;
}

/**
 * @brief OLED显示UTF-8编码字符串（支持中英文混显）
 */
OLED_Status_t OLED_ShowStringUTF8(uint8_t line, uint8_t column, const char *str)
{
    const char *p = str;
    uint8_t current_column = column;
    uint8_t current_line = line;
    OLED_Status_t status;
    OLED_CharType_t char_type;
    uint8_t char_len;
    uint16_t gb2312_code;
    
    /* ========== 参数校验 ========== */
    if (!g_oled_initialized)
    {
        return OLED_ERROR_NOT_INITIALIZED;
    }
    
    if (str == NULL)
    {
        return OLED_ERROR_INVALID_PARAM;
    }
    
    if (line < 1 || line > 4 || column < 1 || column > 16)
    {
        return OLED_ERROR_INVALID_PARAM;
    }
    
    /* 遍历字符串 */
    while (*p != '\0')
    {
        /* 获取下一个字符信息 */
        if (!OLED_UTF8_GetNextCharInfo(p, &char_type, &char_len))
        {
            break;  /* 字符串结束或无效字符 */
        }
        
        /* 检查是否超出显示范围 */
        if (current_line > 4)
        {
            break;  /* 超出显示范围 */
        }
        
        /* 根据字符类型处理 */
        if (char_type == OLED_CHAR_TYPE_ASCII)
        {
            /* ASCII字符：占用1列 */
            if (current_column > 16)
            {
                /* 换行 */
                current_line++;
                current_column = 1;
                if (current_line > 4)
                {
                    break;
                }
            }
            
            status = OLED_ShowChar(current_line, current_column, *p);
            if (status != OLED_OK)
            {
                return status;
            }
            
            current_column++;
        }
        else if (char_type == OLED_CHAR_TYPE_UTF8_CHINESE)
        {
            /* UTF-8中文字符：需要转换为GB2312 */
            if (OLED_UTF8_ToGB2312((const uint8_t *)p, &gb2312_code))
            {
                /* 转换成功：占用2列 */
                if (current_column > 15)  /* 中文字符需要2列，所以最多到15 */
                {
                    /* 换行 */
                    current_line++;
                    current_column = 1;
                    if (current_line > 4)
                    {
                        break;
                    }
                }
                
                /* 转换为列号（中文字符列号范围1-8） */
                uint8_t chinese_column = (current_column + 1) / 2;  /* 转换为1-8范围 */
                if (chinese_column < 1) chinese_column = 1;
                if (chinese_column > 8) chinese_column = 8;
                
                status = OLED_ShowChineseChar(current_line, chinese_column, gb2312_code);
                if (status != OLED_OK)
                {
                    return status;
                }
                
                current_column += 2;  /* 中文字符占用2列 */
            }
            /* 转换失败：跳过该字符 */
        }
        else if (char_type == OLED_CHAR_TYPE_GB2312_CHINESE)
        {
            /* GB2312中文字符：直接使用 */
            if (current_column > 15)  /* 中文字符需要2列，所以最多到15 */
            {
                /* 换行 */
                current_line++;
                current_column = 1;
                if (current_line > 4)
                {
                    break;
                }
            }
            
            /* 提取GB2312编码（高字节在前） */
            gb2312_code = ((uint16_t)(uint8_t)p[0] << 8) | (uint8_t)p[1];
            
            /* 转换为列号（中文字符列号范围1-8） */
            uint8_t chinese_column = (current_column + 1) / 2;  /* 转换为1-8范围 */
            if (chinese_column < 1) chinese_column = 1;
            if (chinese_column > 8) chinese_column = 8;
            
            status = OLED_ShowChineseChar(current_line, chinese_column, gb2312_code);
            if (status != OLED_OK)
            {
                return status;
            }
            
            current_column += 2;  /* 中文字符占用2列 */
        }
        else
        {
            /* 无效字符：跳过 */
        }
        
        /* 移动到下一个字符 */
        p += char_len;
    }
    
    return OLED_OK;
}

#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */
#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */
