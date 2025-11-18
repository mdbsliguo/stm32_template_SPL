/**
 * @file debug.c
 * @brief 调试输出模块实现
 * @details 当前实现为占位实现，实际使用时需要配置UART或SWO
 */

#include "debug.h"
#include "stm32f10x.h"

static debug_mode_t g_debug_mode = DEBUG_MODE_NONE;

int Debug_Init(debug_mode_t mode, uint32_t baudrate)
{
    (void)baudrate; /* 未使用参数警告消除 */
    
    g_debug_mode = mode;
    
    /* TODO: 实际使用时需要配置UART或SWO */
    /* 示例：UART配置
     * USART_InitTypeDef USART_InitStructure;
     * // ... 配置UART
     * USART_Init(USART1, &USART_InitStructure);
     * USART_Cmd(USART1, ENABLE);
     */
    
    return 0;
}

void Debug_Deinit(void)
{
    g_debug_mode = DEBUG_MODE_NONE;
}

int Debug_PutChar(int ch)
{
    if (g_debug_mode == DEBUG_MODE_NONE)
    {
        return ch;
    }
    
    /* TODO: 实际使用时需要实现UART发送或SWO输出 */
    /* 示例：UART发送
     * while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
     * USART_SendData(USART1, (uint8_t)ch);
     */
    
    /* 当前实现：空操作（避免编译警告） */
    (void)ch;
    
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

