/**
 * @file uart.c
 * @brief UART驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的UART驱动，支持UART1/2/3，阻塞式发送/接收，可配置波特率、数据位、停止位、校验位
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_UART_ENABLED

/* Include our header */
#include "uart.h"

#include "gpio.h"
#include "delay.h"
#include "nvic.h"
#include "dma.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"
#include "misc.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 从board.h加载配置 */
static UART_Config_t g_uart_configs[UART_INSTANCE_MAX] = UART_CONFIGS;

/* 初始化标志 */
static bool g_uart_initialized[UART_INSTANCE_MAX] = {false, false, false};

/* 硬件流控配置 */
static UART_HW_FlowControl_t g_uart_hw_flow[UART_INSTANCE_MAX] = {UART_HW_FLOW_NONE, UART_HW_FLOW_NONE, UART_HW_FLOW_NONE};

/* 默认超时时间（毫秒） */
#define UART_DEFAULT_TIMEOUT_MS  1000

/* 中断回调函数数组 */
static UART_IT_Callback_t g_uart_it_callbacks[UART_INSTANCE_MAX][8] = {NULL};
static void *g_uart_it_user_data[UART_INSTANCE_MAX][8] = {NULL};

/* 中断模式发送/接收缓冲区 */
static const uint8_t *g_uart_tx_buffer[UART_INSTANCE_MAX] = {NULL};
static uint8_t *g_uart_rx_buffer[UART_INSTANCE_MAX] = {NULL};
static uint16_t g_uart_tx_length[UART_INSTANCE_MAX] = {0};
static uint16_t g_uart_tx_index[UART_INSTANCE_MAX] = {0};
static uint16_t g_uart_rx_length[UART_INSTANCE_MAX] = {0};
static uint16_t g_uart_rx_index[UART_INSTANCE_MAX] = {0};
static uint16_t g_uart_rx_max_length[UART_INSTANCE_MAX] = {0};

/* DMA通道映射（UART TX/RX对应的DMA通道） */
static const DMA_Channel_t uart_tx_dma_channels[UART_INSTANCE_MAX] = {
    DMA_CHANNEL_1_1,  /* UART1 TX -> DMA1_CH1 */
    DMA_CHANNEL_1_3,  /* UART2 TX -> DMA1_CH3 */
    DMA_CHANNEL_1_5,  /* UART3 TX -> DMA1_CH5 */
};

static const DMA_Channel_t uart_rx_dma_channels[UART_INSTANCE_MAX] = {
    DMA_CHANNEL_1_2,  /* UART1 RX -> DMA1_CH2 */
    DMA_CHANNEL_1_4,  /* UART2 RX -> DMA1_CH4 */
    DMA_CHANNEL_1_6,  /* UART3 RX -> DMA1_CH6 */
};

/**
 * @brief 获取UART外设时钟
 * @param[in] uart_periph UART外设指针
 * @return uint32_t 时钟使能值，失败返回0
 */
static uint32_t UART_GetPeriphClock(USART_TypeDef *uart_periph)
{
    if (uart_periph == USART1)
    {
        return RCC_APB2Periph_USART1;
    }
    else if (uart_periph == USART2)
    {
        return RCC_APB1Periph_USART2;
    }
    else if (uart_periph == USART3)
    {
        return RCC_APB1Periph_USART3;
    }
    return 0;
}

/**
 * @brief 获取UART GPIO时钟
 * @param[in] port GPIO端口指针
 * @return uint32_t GPIO时钟使能值
 */
static uint32_t UART_GetGPIOClock(GPIO_TypeDef *port)
{
    if (port == GPIOA)
    {
        return RCC_APB2Periph_GPIOA;
    }
    else if (port == GPIOB)
    {
        return RCC_APB2Periph_GPIOB;
    }
    else if (port == GPIOC)
    {
        return RCC_APB2Periph_GPIOC;
    }
    else if (port == GPIOD)
    {
        return RCC_APB2Periph_GPIOD;
    }
    return 0;
}

/**
 * @brief 等待UART标志位
 * @param[in] uart_periph UART外设指针
 * @param[in] flag 标志位
 * @param[in] timeout_ms 超时时间（毫秒）
 * @return UART_Status_t 错误码
 */
static UART_Status_t UART_WaitFlag(USART_TypeDef *uart_periph, uint16_t flag, uint32_t timeout_ms)
{
    uint32_t start_tick = Delay_GetTick();
    
    while (USART_GetFlagStatus(uart_periph, flag) == RESET)
    {
        /* 检查超时（使用Delay_GetElapsed处理溢出） */
        uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
        if (elapsed > timeout_ms)
        {
            return UART_ERROR_TIMEOUT;
        }
    }
    
    return UART_OK;
}

/**
 * @brief UART初始化
 */
UART_Status_t UART_Init(UART_Instance_t instance)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    UART_Status_t status;
    GPIO_Status_t gpio_status;
    uint32_t uart_clock;
    uint32_t gpio_clock;
    
    /* 参数校验 */
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_configs[instance].enabled)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (g_uart_initialized[instance])
    {
        return UART_OK;
    }
    
    /* 获取UART外设时钟 */
    uart_clock = UART_GetPeriphClock(g_uart_configs[instance].uart_periph);
    if (uart_clock == 0)
    {
        return UART_ERROR_INVALID_PERIPH;
    }
    
    /* 使能UART外设时钟 */
    if (uart_clock & RCC_APB2Periph_USART1)
    {
        RCC_APB2PeriphClockCmd(uart_clock, ENABLE);
    }
    else
    {
        RCC_APB1PeriphClockCmd(uart_clock, ENABLE);
    }
    
    /* 使能GPIO时钟 */
    gpio_clock = UART_GetGPIOClock(g_uart_configs[instance].tx_port);
    if (gpio_clock != 0)
    {
        RCC_APB2PeriphClockCmd(gpio_clock, ENABLE);
    }
    
    if (g_uart_configs[instance].tx_port != g_uart_configs[instance].rx_port)
    {
        gpio_clock = UART_GetGPIOClock(g_uart_configs[instance].rx_port);
        if (gpio_clock != 0)
        {
            RCC_APB2PeriphClockCmd(gpio_clock, ENABLE);
        }
    }
    
    /* 配置TX引脚为复用推挽输出 */
    GPIO_InitStructure.GPIO_Pin = g_uart_configs[instance].tx_pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(g_uart_configs[instance].tx_port, &GPIO_InitStructure);
    
    /* 配置RX引脚为浮空输入 */
    GPIO_InitStructure.GPIO_Pin = g_uart_configs[instance].rx_pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(g_uart_configs[instance].rx_port, &GPIO_InitStructure);
    
    /* 配置UART外设 */
    USART_InitStructure.USART_BaudRate = g_uart_configs[instance].baudrate;
    USART_InitStructure.USART_WordLength = g_uart_configs[instance].word_length;
    USART_InitStructure.USART_StopBits = g_uart_configs[instance].stop_bits;
    USART_InitStructure.USART_Parity = g_uart_configs[instance].parity;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    
    /* 配置硬件流控 */
    switch (g_uart_hw_flow[instance])
    {
        case UART_HW_FLOW_RTS:
            USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_RTS;
            break;
        case UART_HW_FLOW_CTS:
            USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_CTS;
            break;
        case UART_HW_FLOW_RTS_CTS:
            USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_RTS_CTS;
            break;
        default:
            USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
            break;
    }
    
    /* 调用标准库的USART_Init函数 */
    USART_Init(g_uart_configs[instance].uart_periph, &USART_InitStructure);
    
    /* 使能UART外设 */
    USART_Cmd(g_uart_configs[instance].uart_periph, ENABLE);
    
    /* 初始化中断相关变量 */
    for (int i = 0; i < 8; i++)
    {
        g_uart_it_callbacks[instance][i] = NULL;
        g_uart_it_user_data[instance][i] = NULL;
    }
    g_uart_tx_buffer[instance] = NULL;
    g_uart_rx_buffer[instance] = NULL;
    g_uart_tx_length[instance] = 0;
    g_uart_tx_index[instance] = 0;
    g_uart_rx_length[instance] = 0;
    g_uart_rx_index[instance] = 0;
    g_uart_rx_max_length[instance] = 0;
    
    /* 标记为已初始化 */
    g_uart_initialized[instance] = true;
    
    return UART_OK;
}

/**
 * @brief UART反初始化
 */
UART_Status_t UART_Deinit(UART_Instance_t instance)
{
    uint32_t uart_clock;
    
    /* 参数校验 */
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_OK;
    }
    
    /* 禁用UART外设 */
    USART_Cmd(g_uart_configs[instance].uart_periph, DISABLE);
    
    /* 获取UART外设时钟 */
    uart_clock = UART_GetPeriphClock(g_uart_configs[instance].uart_periph);
    if (uart_clock != 0)
    {
        /* 禁用UART外设时钟 */
        if (uart_clock & RCC_APB2Periph_USART1)
        {
            RCC_APB2PeriphClockCmd(uart_clock, DISABLE);
        }
        else
        {
            RCC_APB1PeriphClockCmd(uart_clock, DISABLE);
        }
    }
    
    /* 标记为未初始化 */
    g_uart_initialized[instance] = false;
    
    return UART_OK;
}

/**
 * @brief UART发送数据（阻塞式）
 */
UART_Status_t UART_Transmit(UART_Instance_t instance, const uint8_t *data, uint16_t length, uint32_t timeout)
{
    USART_TypeDef *uart_periph;
    UART_Status_t status;
    uint16_t i;
    
    /* 参数校验 */
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (data == NULL || length == 0)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 使用默认超时时间 */
    if (timeout == 0)
    {
        timeout = UART_DEFAULT_TIMEOUT_MS;
    }
    
    /* 发送数据 */
    for (i = 0; i < length; i++)
    {
        /* 等待发送缓冲区空 */
        status = UART_WaitFlag(uart_periph, USART_FLAG_TXE, timeout);
        if (status != UART_OK)
        {
            return status;
        }
        
        /* 发送数据 */
        USART_SendData(uart_periph, data[i]);
    }
    
    /* 等待发送完成 */
    status = UART_WaitFlag(uart_periph, USART_FLAG_TC, timeout);
    if (status != UART_OK)
    {
        return status;
    }
    
    return UART_OK;
}

/**
 * @brief UART接收数据（阻塞式）
 */
UART_Status_t UART_Receive(UART_Instance_t instance, uint8_t *data, uint16_t length, uint32_t timeout)
{
    USART_TypeDef *uart_periph;
    UART_Status_t status;
    uint16_t i;
    
    /* 参数校验 */
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (data == NULL || length == 0)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 使用默认超时时间 */
    if (timeout == 0)
    {
        timeout = UART_DEFAULT_TIMEOUT_MS;
    }
    
    /* 接收数据 */
    for (i = 0; i < length; i++)
    {
        /* 等待接收数据就绪 */
        status = UART_WaitFlag(uart_periph, USART_FLAG_RXNE, timeout);
        if (status != UART_OK)
        {
            return status;
        }
        
        /* 读取数据 */
        data[i] = (uint8_t)USART_ReceiveData(uart_periph);
    }
    
    return UART_OK;
}

/**
 * @brief UART发送单个字节（阻塞式）
 */
UART_Status_t UART_TransmitByte(UART_Instance_t instance, uint8_t byte, uint32_t timeout)
{
    return UART_Transmit(instance, &byte, 1, timeout);
}

/**
 * @brief UART接收单个字节（阻塞式）
 */
UART_Status_t UART_ReceiveByte(UART_Instance_t instance, uint8_t *byte, uint32_t timeout)
{
    if (byte == NULL)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    return UART_Receive(instance, byte, 1, timeout);
}

/**
 * @brief UART发送字符串（阻塞式）
 */
UART_Status_t UART_TransmitString(UART_Instance_t instance, const char *str, uint32_t timeout)
{
    if (str == NULL)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    return UART_Transmit(instance, (const uint8_t *)str, strlen(str), timeout);
}

/**
 * @brief 检查UART是否已初始化
 */
uint8_t UART_IsInitialized(UART_Instance_t instance)
{
    if (instance >= UART_INSTANCE_MAX)
    {
        return 0;
    }
    
    return g_uart_initialized[instance] ? 1 : 0;
}

/**
 * @brief 获取UART外设指针
 */
USART_TypeDef* UART_GetPeriph(UART_Instance_t instance)
{
    if (instance >= UART_INSTANCE_MAX)
    {
        return NULL;
    }
    
    if (!g_uart_initialized[instance])
    {
        return NULL;
    }
    
    return g_uart_configs[instance].uart_periph;
}

/* ========== 中断模式功能实现 ========== */

/**
 * @brief 获取UART中断类型对应的SPL库中断值
 */
static uint16_t UART_GetITValue(UART_IT_t it_type)
{
    switch (it_type)
    {
        case UART_IT_TXE:  return USART_IT_TXE;
        case UART_IT_TC:   return USART_IT_TC;
        case UART_IT_RXNE: return USART_IT_RXNE;
        case UART_IT_IDLE: return USART_IT_IDLE;
        case UART_IT_PE:   return USART_IT_PE;
        case UART_IT_ERR:  return USART_IT_ERR;
        case UART_IT_LBD:  return USART_IT_LBD;
        case UART_IT_CTS:  return USART_IT_CTS;
        default: return 0;
    }
}

/**
 * @brief 获取UART中断向量
 */
static IRQn_Type UART_GetIRQn(UART_Instance_t instance)
{
    switch (instance)
    {
        case UART_INSTANCE_1: return USART1_IRQn;
        case UART_INSTANCE_2: return USART2_IRQn;
        case UART_INSTANCE_3: return USART3_IRQn;
        default: return (IRQn_Type)0;
    }
}

/**
 * @brief 使能UART中断
 */
UART_Status_t UART_EnableIT(UART_Instance_t instance, UART_IT_t it_type)
{
    USART_TypeDef *uart_periph;
    uint16_t it_value;
    IRQn_Type irqn;
    
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 8) /* UART_IT_MAX */
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    it_value = UART_GetITValue(it_type);
    if (it_value == 0)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    /* 使能UART中断 */
    USART_ITConfig(uart_periph, it_value, ENABLE);
    
    /* 使能NVIC中断 */
    irqn = UART_GetIRQn(instance);
    if (irqn != 0)
    {
        NVIC_HW_EnableIRQ(irqn);
    }
    
    return UART_OK;
}

/**
 * @brief 禁用UART中断
 */
UART_Status_t UART_DisableIT(UART_Instance_t instance, UART_IT_t it_type)
{
    USART_TypeDef *uart_periph;
    uint16_t it_value;
    
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 8)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    it_value = UART_GetITValue(it_type);
    if (it_value == 0)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    /* 禁用UART中断 */
    USART_ITConfig(uart_periph, it_value, DISABLE);
    
    return UART_OK;
}

/**
 * @brief 设置UART中断回调函数
 */
UART_Status_t UART_SetITCallback(UART_Instance_t instance, UART_IT_t it_type,
                                  UART_IT_Callback_t callback, void *user_data)
{
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 8)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    g_uart_it_callbacks[instance][it_type] = callback;
    g_uart_it_user_data[instance][it_type] = user_data;
    
    return UART_OK;
}

/**
 * @brief 获取UART中断状态
 */
uint8_t UART_GetITStatus(UART_Instance_t instance, UART_IT_t it_type)
{
    USART_TypeDef *uart_periph;
    uint16_t it_value;
    
    if (instance >= UART_INSTANCE_MAX || !g_uart_initialized[instance])
    {
        return 0;
    }
    
    if (it_type >= 8)
    {
        return 0;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    it_value = UART_GetITValue(it_type);
    if (it_value == 0)
    {
        return 0;
    }
    
    return (USART_GetITStatus(uart_periph, it_value) == SET) ? 1 : 0;
}

/**
 * @brief 清除UART中断挂起标志
 */
UART_Status_t UART_ClearITPendingBit(UART_Instance_t instance, UART_IT_t it_type)
{
    USART_TypeDef *uart_periph;
    uint16_t it_value;
    
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 8)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    it_value = UART_GetITValue(it_type);
    if (it_value == 0)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    USART_ClearITPendingBit(uart_periph, it_value);
    
    return UART_OK;
}

/**
 * @brief UART非阻塞发送（中断模式）
 */
UART_Status_t UART_TransmitIT(UART_Instance_t instance, const uint8_t *data, uint16_t length)
{
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (g_uart_tx_buffer[instance] != NULL)
    {
        return UART_ERROR_BUSY; /* 正在发送 */
    }
    
    /* 保存发送缓冲区信息 */
    g_uart_tx_buffer[instance] = data;
    g_uart_tx_length[instance] = length;
    g_uart_tx_index[instance] = 0;
    
    /* 使能TXE中断，开始发送 */
    UART_EnableIT(instance, UART_IT_TXE);
    
    return UART_OK;
}

/**
 * @brief UART非阻塞接收（中断模式）
 */
UART_Status_t UART_ReceiveIT(UART_Instance_t instance, uint8_t *data, uint16_t max_length)
{
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || max_length == 0)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (g_uart_rx_buffer[instance] != NULL)
    {
        return UART_ERROR_BUSY; /* 正在接收 */
    }
    
    /* 保存接收缓冲区信息 */
    g_uart_rx_buffer[instance] = data;
    g_uart_rx_max_length[instance] = max_length;
    g_uart_rx_index[instance] = 0;
    g_uart_rx_length[instance] = 0;
    
    /* 使能RXNE和IDLE中断，开始接收 */
    UART_EnableIT(instance, UART_IT_RXNE);
    UART_EnableIT(instance, UART_IT_IDLE);
    
    return UART_OK;
}

/**
 * @brief 获取UART发送剩余字节数（中断模式）
 */
uint16_t UART_GetTransmitRemaining(UART_Instance_t instance)
{
    if (instance >= UART_INSTANCE_MAX || !g_uart_initialized[instance])
    {
        return 0;
    }
    
    if (g_uart_tx_buffer[instance] == NULL)
    {
        return 0;
    }
    
    return g_uart_tx_length[instance] - g_uart_tx_index[instance];
}

/**
 * @brief 获取UART接收到的字节数（中断模式）
 */
uint16_t UART_GetReceiveCount(UART_Instance_t instance)
{
    if (instance >= UART_INSTANCE_MAX || !g_uart_initialized[instance])
    {
        return 0;
    }
    
    return g_uart_rx_length[instance];
}

/**
 * @brief UART中断服务函数
 */
void UART_IRQHandler(UART_Instance_t instance)
{
    USART_TypeDef *uart_periph;
    uint8_t byte;
    
    if (instance >= UART_INSTANCE_MAX || !g_uart_initialized[instance])
    {
        return;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 处理TXE中断（发送缓冲区空） */
    if (USART_GetITStatus(uart_periph, USART_IT_TXE) != RESET)
    {
        if (g_uart_tx_buffer[instance] != NULL && g_uart_tx_index[instance] < g_uart_tx_length[instance])
        {
            /* 发送下一个字节 */
            USART_SendData(uart_periph, g_uart_tx_buffer[instance][g_uart_tx_index[instance]++]);
        }
        else
        {
            /* 发送完成，禁用TXE中断，使能TC中断 */
            USART_ITConfig(uart_periph, USART_IT_TXE, DISABLE);
            USART_ITConfig(uart_periph, USART_IT_TC, ENABLE);
        }
        
        /* 调用回调函数 */
        if (g_uart_it_callbacks[instance][UART_IT_TXE] != NULL)
        {
            g_uart_it_callbacks[instance][UART_IT_TXE](instance, UART_IT_TXE, g_uart_it_user_data[instance][UART_IT_TXE]);
        }
    }
    
    /* 处理TC中断（发送完成） */
    if (USART_GetITStatus(uart_periph, USART_IT_TC) != RESET)
    {
        USART_ClearITPendingBit(uart_periph, USART_IT_TC);
        USART_ITConfig(uart_periph, USART_IT_TC, DISABLE);
        
        /* 清除发送缓冲区 */
        g_uart_tx_buffer[instance] = NULL;
        g_uart_tx_length[instance] = 0;
        g_uart_tx_index[instance] = 0;
        
        /* 调用回调函数 */
        if (g_uart_it_callbacks[instance][UART_IT_TC] != NULL)
        {
            g_uart_it_callbacks[instance][UART_IT_TC](instance, UART_IT_TC, g_uart_it_user_data[instance][UART_IT_TC]);
        }
    }
    
    /* 处理RXNE中断（接收数据就绪） */
    if (USART_GetITStatus(uart_periph, USART_IT_RXNE) != RESET)
    {
        if (g_uart_rx_buffer[instance] != NULL && g_uart_rx_index[instance] < g_uart_rx_max_length[instance])
        {
            /* 读取数据 */
            byte = (uint8_t)USART_ReceiveData(uart_periph);
            g_uart_rx_buffer[instance][g_uart_rx_index[instance]++] = byte;
            g_uart_rx_length[instance] = g_uart_rx_index[instance];
        }
        else
        {
            /* 缓冲区满，读取并丢弃 */
            USART_ReceiveData(uart_periph);
        }
        
        /* 调用回调函数 */
        if (g_uart_it_callbacks[instance][UART_IT_RXNE] != NULL)
        {
            g_uart_it_callbacks[instance][UART_IT_RXNE](instance, UART_IT_RXNE, g_uart_it_user_data[instance][UART_IT_RXNE]);
        }
    }
    
    /* 处理IDLE中断（空闲） */
    if (USART_GetITStatus(uart_periph, USART_IT_IDLE) != RESET)
    {
        USART_ClearITPendingBit(uart_periph, USART_IT_IDLE);
        
        /* 调用回调函数 */
        if (g_uart_it_callbacks[instance][UART_IT_IDLE] != NULL)
        {
            g_uart_it_callbacks[instance][UART_IT_IDLE](instance, UART_IT_IDLE, g_uart_it_user_data[instance][UART_IT_IDLE]);
        }
    }
    
    /* 处理错误中断 */
    if (USART_GetITStatus(uart_periph, USART_IT_PE) != RESET)
    {
        USART_ClearITPendingBit(uart_periph, USART_IT_PE);
        
        /* 调用回调函数 */
        if (g_uart_it_callbacks[instance][UART_IT_PE] != NULL)
        {
            g_uart_it_callbacks[instance][UART_IT_PE](instance, UART_IT_PE, g_uart_it_user_data[instance][UART_IT_PE]);
        }
    }
    
    if (USART_GetITStatus(uart_periph, USART_IT_ORE) != RESET)
    {
        USART_ClearITPendingBit(uart_periph, USART_IT_ORE);
        
        /* 调用回调函数 */
        if (g_uart_it_callbacks[instance][UART_IT_ERR] != NULL)
        {
            g_uart_it_callbacks[instance][UART_IT_ERR](instance, UART_IT_ERR, g_uart_it_user_data[instance][UART_IT_ERR]);
        }
    }
    
    if (USART_GetITStatus(uart_periph, USART_IT_NE) != RESET)
    {
        USART_ClearITPendingBit(uart_periph, USART_IT_NE);
        
        /* 调用回调函数 */
        if (g_uart_it_callbacks[instance][UART_IT_ERR] != NULL)
        {
            g_uart_it_callbacks[instance][UART_IT_ERR](instance, UART_IT_ERR, g_uart_it_user_data[instance][UART_IT_ERR]);
        }
    }
    
    if (USART_GetITStatus(uart_periph, USART_IT_FE) != RESET)
    {
        USART_ClearITPendingBit(uart_periph, USART_IT_FE);
        
        /* 调用回调函数 */
        if (g_uart_it_callbacks[instance][UART_IT_ERR] != NULL)
        {
            g_uart_it_callbacks[instance][UART_IT_ERR](instance, UART_IT_ERR, g_uart_it_user_data[instance][UART_IT_ERR]);
        }
    }
}

/* UART中断服务程序入口（需要在启动文件中定义，或用户自己实现） */
void USART1_IRQHandler(void)
{
    UART_IRQHandler(UART_INSTANCE_1);
}

void USART2_IRQHandler(void)
{
    UART_IRQHandler(UART_INSTANCE_2);
}

void USART3_IRQHandler(void)
{
    UART_IRQHandler(UART_INSTANCE_3);
}

/* ========== DMA模式功能实现 ========== */

/**
 * @brief UART DMA发送
 */
UART_Status_t UART_TransmitDMA(UART_Instance_t instance, const uint8_t *data, uint16_t length)
{
    USART_TypeDef *uart_periph;
    DMA_Channel_t dma_channel;
    DMA_Status_t dma_status;
    
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    dma_channel = uart_tx_dma_channels[instance];
    
    /* 检查DMA通道是否已初始化 */
    if (!DMA_IsInitialized(dma_channel))
    {
        /* 初始化DMA通道 */
        dma_status = DMA_HW_Init(dma_channel);
        if (dma_status != DMA_OK)
        {
            return UART_ERROR_INVALID_PARAM;
        }
    }
    
    /* 停止之前的传输 */
    DMA_Stop(dma_channel);
    
    /* 配置DMA传输（内存到外设） */
    dma_status = DMA_ConfigTransfer(dma_channel, (uint32_t)&uart_periph->DR,
                                    (uint32_t)data, length,
                                    DMA_DIR_MEMORY_TO_PERIPHERAL, 1);
    if (dma_status != DMA_OK)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    /* 使能UART DMA发送请求 */
    USART_DMACmd(uart_periph, USART_DMAReq_Tx, ENABLE);
    
    /* 启动DMA传输 */
    dma_status = DMA_Start(dma_channel);
    if (dma_status != DMA_OK)
    {
        USART_DMACmd(uart_periph, USART_DMAReq_Tx, DISABLE);
        return UART_ERROR_INVALID_PARAM;
    }
    
    return UART_OK;
}

/**
 * @brief UART DMA接收
 */
UART_Status_t UART_ReceiveDMA(UART_Instance_t instance, uint8_t *data, uint16_t length)
{
    USART_TypeDef *uart_periph;
    DMA_Channel_t dma_channel;
    DMA_Status_t dma_status;
    
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    dma_channel = uart_rx_dma_channels[instance];
    
    /* 检查DMA通道是否已初始化 */
    if (!DMA_IsInitialized(dma_channel))
    {
        /* 初始化DMA通道 */
        dma_status = DMA_HW_Init(dma_channel);
        if (dma_status != DMA_OK)
        {
            return UART_ERROR_INVALID_PARAM;
        }
    }
    
    /* 停止之前的传输 */
    DMA_Stop(dma_channel);
    
    /* 配置DMA传输（外设到内存） */
    dma_status = DMA_ConfigTransfer(dma_channel, (uint32_t)&uart_periph->DR,
                                    (uint32_t)data, length,
                                    DMA_DIR_PERIPHERAL_TO_MEMORY, 1);
    if (dma_status != DMA_OK)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    /* 使能UART DMA接收请求 */
    USART_DMACmd(uart_periph, USART_DMAReq_Rx, ENABLE);
    
    /* 启动DMA传输 */
    dma_status = DMA_Start(dma_channel);
    if (dma_status != DMA_OK)
    {
        USART_DMACmd(uart_periph, USART_DMAReq_Rx, DISABLE);
        return UART_ERROR_INVALID_PARAM;
    }
    
    return UART_OK;
}

/**
 * @brief 停止UART DMA发送
 */
UART_Status_t UART_StopTransmitDMA(UART_Instance_t instance)
{
    USART_TypeDef *uart_periph;
    DMA_Channel_t dma_channel;
    
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    dma_channel = uart_tx_dma_channels[instance];
    
    /* 禁用UART DMA发送请求 */
    USART_DMACmd(uart_periph, USART_DMAReq_Tx, DISABLE);
    
    /* 停止DMA传输 */
    DMA_Stop(dma_channel);
    
    return UART_OK;
}

/**
 * @brief 停止UART DMA接收
 */
UART_Status_t UART_StopReceiveDMA(UART_Instance_t instance)
{
    USART_TypeDef *uart_periph;
    DMA_Channel_t dma_channel;
    
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    dma_channel = uart_rx_dma_channels[instance];
    
    /* 禁用UART DMA接收请求 */
    USART_DMACmd(uart_periph, USART_DMAReq_Rx, DISABLE);
    
    /* 停止DMA传输 */
    DMA_Stop(dma_channel);
    
    return UART_OK;
}

/**
 * @brief 获取UART DMA发送剩余字节数
 */
uint16_t UART_GetTransmitDMARemaining(UART_Instance_t instance)
{
    DMA_Channel_t dma_channel;
    
    if (instance >= UART_INSTANCE_MAX || !g_uart_initialized[instance])
    {
        return 0;
    }
    
    dma_channel = uart_tx_dma_channels[instance];
    return DMA_GetRemainingDataSize(dma_channel);
}

/**
 * @brief 获取UART DMA接收剩余字节数
 */
uint16_t UART_GetReceiveDMARemaining(UART_Instance_t instance)
{
    DMA_Channel_t dma_channel;
    
    if (instance >= UART_INSTANCE_MAX || !g_uart_initialized[instance])
    {
        return 0;
    }
    
    dma_channel = uart_rx_dma_channels[instance];
    return DMA_GetRemainingDataSize(dma_channel);
}

/* ========== 硬件流控功能实现 ========== */

/**
 * @brief 配置UART硬件流控
 */
UART_Status_t UART_SetHardwareFlowControl(UART_Instance_t instance, UART_HW_FlowControl_t flow_control)
{
    USART_InitTypeDef USART_InitStructure;
    
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    if (flow_control > UART_HW_FLOW_RTS_CTS)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    /* 保存硬件流控配置 */
    g_uart_hw_flow[instance] = flow_control;
    
    /* 重新配置UART（需要先禁用UART） */
    USART_Cmd(g_uart_configs[instance].uart_periph, DISABLE);
    
    /* 读取当前配置 */
    USART_StructInit(&USART_InitStructure);
    USART_InitStructure.USART_BaudRate = g_uart_configs[instance].baudrate;
    USART_InitStructure.USART_WordLength = g_uart_configs[instance].word_length;
    USART_InitStructure.USART_StopBits = g_uart_configs[instance].stop_bits;
    USART_InitStructure.USART_Parity = g_uart_configs[instance].parity;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    
    /* 配置硬件流控 */
    switch (flow_control)
    {
        case UART_HW_FLOW_RTS:
            USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_RTS;
            break;
        case UART_HW_FLOW_CTS:
            USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_CTS;
            break;
        case UART_HW_FLOW_RTS_CTS:
            USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_RTS_CTS;
            break;
        default:
            USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
            break;
    }
    
    /* 重新初始化UART */
    USART_Init(g_uart_configs[instance].uart_periph, &USART_InitStructure);
    
    /* 重新使能UART */
    USART_Cmd(g_uart_configs[instance].uart_periph, ENABLE);
    
    return UART_OK;
}

/**
 * @brief 获取UART硬件流控配置
 */
UART_Status_t UART_GetHardwareFlowControl(UART_Instance_t instance, UART_HW_FlowControl_t *flow_control)
{
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    if (flow_control == NULL)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    *flow_control = g_uart_hw_flow[instance];
    
    return UART_OK;
}

/* ========== 单线半双工模式功能实现 ========== */

/**
 * @brief 使能UART单线半双工模式
 */
UART_Status_t UART_EnableHalfDuplex(UART_Instance_t instance)
{
    USART_TypeDef *uart_periph;
    
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 使能单线半双工模式 */
    USART_HalfDuplexCmd(uart_periph, ENABLE);
    
    return UART_OK;
}

/**
 * @brief 禁用UART单线半双工模式
 */
UART_Status_t UART_DisableHalfDuplex(UART_Instance_t instance)
{
    USART_TypeDef *uart_periph;
    
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 禁用单线半双工模式 */
    USART_HalfDuplexCmd(uart_periph, DISABLE);
    
    return UART_OK;
}

/**
 * @brief 检查UART是否处于单线半双工模式
 */
uint8_t UART_IsHalfDuplex(UART_Instance_t instance)
{
    USART_TypeDef *uart_periph;
    
    if (instance >= UART_INSTANCE_MAX)
    {
        return 0;
    }
    
    if (!g_uart_initialized[instance])
    {
        return 0;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 检查HDSEL位（USART_CR3寄存器） */
    if ((uart_periph->CR3 & USART_CR3_HDSEL) != RESET)
    {
        return 1;
    }
    
    return 0;
}

/* ========== LIN/IrDA/智能卡模式功能实现 ========== */

/**
 * @brief 使能UART LIN模式
 */
UART_Status_t UART_EnableLINMode(UART_Instance_t instance)
{
    USART_TypeDef *uart_periph;
    
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 使能LIN模式 */
    USART_LINCmd(uart_periph, ENABLE);
    
    return UART_OK;
}

/**
 * @brief 禁用UART LIN模式
 */
UART_Status_t UART_DisableLINMode(UART_Instance_t instance)
{
    USART_TypeDef *uart_periph;
    
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 禁用LIN模式 */
    USART_LINCmd(uart_periph, DISABLE);
    
    return UART_OK;
}

/**
 * @brief 发送LIN断点字符
 */
UART_Status_t UART_SendBreak(UART_Instance_t instance)
{
    USART_TypeDef *uart_periph;
    
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 发送LIN断点字符 */
    USART_SendBreak(uart_periph);
    
    return UART_OK;
}

/**
 * @brief 使能UART IrDA模式
 */
UART_Status_t UART_EnableIrDAMode(UART_Instance_t instance, uint8_t prescaler)
{
    USART_TypeDef *uart_periph;
    
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    if (prescaler < 1 || prescaler > 31)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 配置IrDA预分频器 */
    USART_SetPrescaler(uart_periph, prescaler);
    
    /* 使能IrDA模式 */
    USART_IrDACmd(uart_periph, ENABLE);
    
    return UART_OK;
}

/**
 * @brief 禁用UART IrDA模式
 */
UART_Status_t UART_DisableIrDAMode(UART_Instance_t instance)
{
    USART_TypeDef *uart_periph;
    
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 禁用IrDA模式 */
    USART_IrDACmd(uart_periph, DISABLE);
    
    return UART_OK;
}

/**
 * @brief 使能UART智能卡模式
 */
UART_Status_t UART_EnableSmartCardMode(UART_Instance_t instance)
{
    USART_TypeDef *uart_periph;
    
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 使能智能卡模式 */
    USART_SmartCardCmd(uart_periph, ENABLE);
    
    return UART_OK;
}

/**
 * @brief 禁用UART智能卡模式
 */
UART_Status_t UART_DisableSmartCardMode(UART_Instance_t instance)
{
    USART_TypeDef *uart_periph;
    
    if (instance >= UART_INSTANCE_MAX)
    {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (!g_uart_initialized[instance])
    {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 禁用智能卡模式 */
    USART_SmartCardCmd(uart_periph, DISABLE);
    
    return UART_OK;
}

#endif /* CONFIG_MODULE_UART_ENABLED */

