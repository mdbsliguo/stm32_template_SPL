/**
 * @file printf_wrapper.c
 * @brief printf封装模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 提供格式化输出到UART和OLED的便捷函数实现
 */

#include "printf_wrapper.h"
#include "uart.h"
#include "oled_ssd1306.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* 缓冲区大小定义 */
#define PRINTF_BUFFER_SIZE  256  /* 格式化缓冲区大小（足够大以容纳大多数格式化字符串） */
#define OLED_MAX_LENGTH     16   /* OLED每行最大字符数 */

/**
 * @brief 内部函数：输出格式化字符串到UART
 * @param[in] instance UART实例索引
 * @param[in] format 格式化字符串
 * @param[in] args 可变参数列表
 */
static void Printf_UART_Internal(UART_Instance_t instance, const char *format, va_list args)
{
    char buffer[PRINTF_BUFFER_SIZE];
    int len;
    
    /* 参数校验 */
    if (format == NULL)
    {
        return;
    }
    
    /* 检查UART是否已初始化 */
    if (!UART_IsInitialized(instance))
    {
        return;  /* UART未初始化，静默失败 */
    }
    
    /* 格式化字符串 */
    len = vsnprintf(buffer, sizeof(buffer), format, args);
    
    /* 检查格式化结果 */
    if (len < 0)
    {
        return;  /* 格式化失败 */
    }
    
    /* 确保字符串以'\0'结尾 */
    if (len >= (int)sizeof(buffer))
    {
        buffer[sizeof(buffer) - 1] = '\0';
        len = sizeof(buffer) - 1;
    }
    
    /* 发送到UART（超时时间0表示使用默认超时） */
    (void)UART_TransmitString(instance, buffer, 0);
}

/**
 * @brief 内部函数：输出格式化字符串到OLED
 * @param[in] line OLED行号（1-4）
 * @param[in] format 格式化字符串
 * @param[in] args 可变参数列表
 */
static void Printf_OLED_Internal(uint8_t line, const char *format, va_list args)
{
    char buffer[PRINTF_BUFFER_SIZE];
    int len;
    
    /* 参数校验 */
    if (format == NULL)
    {
        return;
    }
    
    if (line < 1 || line > 4)
    {
        return;  /* 无效的行号 */
    }
    
    /* 格式化字符串 */
    len = vsnprintf(buffer, sizeof(buffer), format, args);
    
    /* 检查格式化结果 */
    if (len < 0)
    {
        return;  /* 格式化失败 */
    }
    
    /* 确保字符串以'\0'结尾 */
    if (len >= (int)sizeof(buffer))
    {
        buffer[sizeof(buffer) - 1] = '\0';
        len = sizeof(buffer) - 1;
    }
    
    /* 截断到OLED最大长度（16字符） */
    if (len > OLED_MAX_LENGTH)
    {
        buffer[OLED_MAX_LENGTH] = '\0';
        len = OLED_MAX_LENGTH;
    }
    
    /* 显示到OLED指定行，从第1列开始 */
    (void)OLED_ShowString(line, 1, buffer);
}

/**
 * @brief 输出格式化字符串到UART1
 */
void Printf_UART1(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    Printf_UART_Internal(UART_INSTANCE_1, format, args);
    va_end(args);
}

/**
 * @brief 输出格式化字符串到UART2
 */
void Printf_UART2(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    Printf_UART_Internal(UART_INSTANCE_2, format, args);
    va_end(args);
}

/**
 * @brief 输出格式化字符串到UART3
 */
void Printf_UART3(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    Printf_UART_Internal(UART_INSTANCE_3, format, args);
    va_end(args);
}

/**
 * @brief 输出格式化字符串到OLED第1行
 */
void Printf_OLED1(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    Printf_OLED_Internal(1, format, args);
    va_end(args);
}

/**
 * @brief 输出格式化字符串到OLED第2行
 */
void Printf_OLED2(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    Printf_OLED_Internal(2, format, args);
    va_end(args);
}

/**
 * @brief 输出格式化字符串到OLED第3行
 */
void Printf_OLED3(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    Printf_OLED_Internal(3, format, args);
    va_end(args);
}

/**
 * @brief 输出格式化字符串到OLED第4行
 */
void Printf_OLED4(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    Printf_OLED_Internal(4, format, args);
    va_end(args);
}

















