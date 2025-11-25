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
#include "oled_font_ascii8x16.h"
#include "delay.h"
#include "config.h"
#include "stm32f10x.h"
#include <stdbool.h>
#include <stddef.h>

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
    
    /* 字符索引计算：支持度符号（°） */
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
    
    /* 设置光标到字符上半部分 */
    status = OLED_SetCursor((line - 1) * 2, (column - 1) * 8);
    if (status != OLED_OK)
    {
        return status;
    }
    
    /* 发送上半部分8字节字模数据 */
    for (i = 0; i < 8; i++)
    {
        status = OLED_WriteData(OLED_F8x16[char_index][i]);
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
        status = OLED_WriteData(OLED_F8x16[char_index][i + 8]);
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
