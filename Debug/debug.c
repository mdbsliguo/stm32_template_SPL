/**
 * @file debug.c
 * @brief 调试输出模块实现
 * @details 提供UART和SWO输出模式的实现
 * @note UART模式需要启用UART模块（CONFIG_MODULE_UART_ENABLED）
 */

#include "debug.h"
#include "stm32f10x.h"

/* 系统配置（可选依赖） */
/* 使用 <config.h> 而不是 "../System/config.h"，这样会优先在包含路径中查找 */
/* 案例工程的包含路径中，案例目录在最前面，所以会优先找到案例的config.h */
#include <config.h>

#if defined(CONFIG_MODULE_UART_ENABLED) && CONFIG_MODULE_UART_ENABLED
#include "uart.h"
#endif

/* Debug模块状态 */
static debug_mode_t g_debug_mode = DEBUG_MODE_NONE;
static uint8_t g_debug_initialized = 0;

/**
 * @brief 初始化调试输出
 * @param[in] mode 输出模式
 * @param[in] baudrate UART波特率（UART模式时有效）
 * @return 0-成功，非0-失败
 */
int Debug_Init(debug_mode_t mode, uint32_t baudrate)
{
    if (mode == DEBUG_MODE_UART)
    {
#if defined(CONFIG_MODULE_UART_ENABLED) && CONFIG_MODULE_UART_ENABLED
        UART_Status_t status;
        
        /* 初始化UART1实例 */
        status = UART_Init(UART_INSTANCE_1);
        if (status != UART_OK)
        {
            return -1;  /* UART初始化失败 */
        }
        
        g_debug_mode = mode;
        g_debug_initialized = 1;
        
        /* 注意：baudrate参数用于验证，实际波特率由board.h中的UART_CONFIGS配置 */
        (void)baudrate;  /* 避免未使用参数警告 */
        
        return 0;
#else
        /* UART模块未启用 */
        return -2;  /* UART模块未启用 */
#endif
    }
    
    /* SWO模式或其他模式（暂未实现） */
    if (mode == DEBUG_MODE_SWO)
    {
        /* TODO: 实现SWO模式 */
        g_debug_mode = mode;
        g_debug_initialized = 1;
        (void)baudrate;
        return 0;
    }
    
    /* 不支持的模式 */
    if (mode == DEBUG_MODE_NONE)
    {
        g_debug_mode = mode;
        g_debug_initialized = 0;
        return 0;
    }
    
    return -1;  /* 不支持的模式或UART模块未启用 */
}

/**
 * @brief 反初始化调试输出
 */
void Debug_Deinit(void)
{
    if (g_debug_initialized)
    {
#if defined(CONFIG_MODULE_UART_ENABLED) && CONFIG_MODULE_UART_ENABLED
        if (g_debug_mode == DEBUG_MODE_UART)
        {
            /* 反初始化UART */
            UART_Deinit(UART_INSTANCE_1);
        }
#endif
        g_debug_mode = DEBUG_MODE_NONE;
        g_debug_initialized = 0;
    }
}

/**
 * @brief 发送单个字符（用于printf重定向）
 * @param[in] ch 字符
 * @return 发送的字符
 */
int Debug_PutChar(int ch)
{
    if (!g_debug_initialized || g_debug_mode == DEBUG_MODE_NONE)
    {
        return ch;  /* 未初始化或模式不正确，直接返回 */
    }
    
#if defined(CONFIG_MODULE_UART_ENABLED) && CONFIG_MODULE_UART_ENABLED
    if (g_debug_mode == DEBUG_MODE_UART)
    {
        UART_Status_t status;
        
        /* 通过UART发送字符（阻塞式，超时时间0表示使用默认超时） */
        status = UART_TransmitByte(UART_INSTANCE_1, (uint8_t)ch, 0);
        
        /* 如果发送失败，仍然返回字符（避免printf中断） */
        (void)status;  /* 避免未使用变量警告 */
        
        return ch;
    }
#endif
    
    /* SWO模式或其他模式（暂未实现） */
    if (g_debug_mode == DEBUG_MODE_SWO)
    {
        /* TODO: 实现SWO输出 */
        (void)ch;
        return ch;
    }
    
    return ch;
}

/* printf重定向（可选，需要链接时包含） */
#ifdef USE_DEBUG_PRINTF
int fputc(int ch, FILE *f)
{
    (void)f; /* 未使用参数 */
    return Debug_PutChar(ch);
}
#endif
