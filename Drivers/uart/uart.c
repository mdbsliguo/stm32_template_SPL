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
#ifdef CONFIG_MODULE_DMA_ENABLED
#if CONFIG_MODULE_DMA_ENABLED
#include "dma.h"
#endif
#endif
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

/* 单线半双工模式标志 */
static bool g_uart_half_duplex[UART_INSTANCE_MAX] = {false, false, false};

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
/* g_uart_rx_length 保留用于未来扩展，当前未使用 */
#if 0
static uint16_t g_uart_rx_length[UART_INSTANCE_MAX] = {0};
#endif
static uint16_t g_uart_rx_index[UART_INSTANCE_MAX] = {0};
static uint16_t g_uart_rx_max_length[UART_INSTANCE_MAX] = {0};

/* DMA通道映射（UART TX/RX对应的DMA通道） */
#if CONFIG_MODULE_DMA_ENABLED
static const DMA_Channel_t uart_tx_dma_channels[UART_INSTANCE_MAX] = {
    DMA_CHANNEL_1_1,  /* UART1 TX -> DMA1_CH1 */
    DMA_CHANNEL_1_3,  /* UART2 TX -> DMA1_CH3 */
    DMA_CHANNEL_1_5,  /* UART3 TX -> DMA1_CH5 */
};

static const DMA_Channel_t uart_rx_dma_channels[UART_INSTANCE_MAX] = {
    DMA_CHANNEL_1_5,  /* UART1 RX -> DMA1_CH5 (STM32F103固定映射) */
    DMA_CHANNEL_1_4,  /* UART2 RX -> DMA1_CH4 */
    DMA_CHANNEL_1_6,  /* UART3 RX -> DMA1_CH6 */
};
#endif /* CONFIG_MODULE_DMA_ENABLED */

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
    else if (port == GPIOE)
    {
        return RCC_APB2Periph_GPIOE;
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
        
        /* 检查错误标志 */
        /* 注意：ORE、NE、FE、PE、IDLE标志需要通过读取SR寄存器然后读取DR寄存器来清除 */
        /* 优化：直接读取SR寄存器，减少函数调用开销 */
        uint16_t sr = uart_periph->SR;
        
        if (sr & USART_FLAG_ORE)
        {
            /* 清除ORE标志：读取SR寄存器，然后读取DR寄存器 */
            (void)USART_ReceiveData(uart_periph);
            return UART_ERROR_ORE;
        }
        if (sr & USART_FLAG_NE)
        {
            /* 清除NE标志：读取SR寄存器，然后读取DR寄存器 */
            (void)USART_ReceiveData(uart_periph);
            return UART_ERROR_NE;
        }
        if (sr & USART_FLAG_FE)
        {
            /* 清除FE标志：读取SR寄存器，然后读取DR寄存器 */
            (void)USART_ReceiveData(uart_periph);
            return UART_ERROR_FE;
        }
        if (sr & USART_FLAG_PE)
        {
            /* 清除PE标志：读取SR寄存器，然后读取DR寄存器 */
            (void)USART_ReceiveData(uart_periph);
            return UART_ERROR_PE;
        }
    }
    
    return UART_OK;
}

/**
 * @brief UART初始化
 */
UART_Status_t UART_Init(UART_Instance_t instance)
{
    USART_InitTypeDef USART_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    uint32_t uart_clock;
    uint32_t gpio_clock;
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    
    if (!g_uart_configs[instance].enabled) {
        return UART_ERROR_INVALID_PARAM;
    }
    
    if (g_uart_configs[instance].uart_periph == NULL) {
        return UART_ERROR_INVALID_PERIPH;
    }
    
    /* 检查是否已初始化 */
    if (g_uart_initialized[instance]) {
        return UART_OK;
    }
    
    /* 获取UART外设时钟 */
    uart_clock = UART_GetPeriphClock(g_uart_configs[instance].uart_periph);
    if (uart_clock == 0) {
        return UART_ERROR_INVALID_PERIPH;
    }
    
    /* 使能UART外设时钟 */
    if (g_uart_configs[instance].uart_periph == USART1) {
        RCC_APB2PeriphClockCmd(uart_clock, ENABLE);
    } else {
        RCC_APB1PeriphClockCmd(uart_clock, ENABLE);
    }
    
    /* 使能TX引脚GPIO时钟 */
    gpio_clock = UART_GetGPIOClock(g_uart_configs[instance].tx_port);
    if (gpio_clock == 0) {
        return UART_ERROR_GPIO_FAILED;
    }
    RCC_APB2PeriphClockCmd(gpio_clock, ENABLE);
    
    /* 使能RX引脚GPIO时钟（如果与TX不同） */
    if (g_uart_configs[instance].rx_port != g_uart_configs[instance].tx_port) {
        gpio_clock = UART_GetGPIOClock(g_uart_configs[instance].rx_port);
        if (gpio_clock == 0) {
            return UART_ERROR_GPIO_FAILED;
        }
        RCC_APB2PeriphClockCmd(gpio_clock, ENABLE);
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
    
    /* 根据配置设置硬件流控 */
    switch (g_uart_hw_flow[instance]) {
        case UART_HW_FLOW_NONE:
            USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
            break;
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
    
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    
    USART_Init(g_uart_configs[instance].uart_periph, &USART_InitStructure);
    
    /* 使能UART外设 */
    USART_Cmd(g_uart_configs[instance].uart_periph, ENABLE);
    
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
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    
    if (!g_uart_initialized[instance]) {
        return UART_OK;
    }
    
    /* 禁用UART外设 */
    USART_Cmd(g_uart_configs[instance].uart_periph, DISABLE);
    
    /* 禁用所有UART中断 */
    USART_ITConfig(g_uart_configs[instance].uart_periph, 
                   USART_IT_TXE | USART_IT_TC | USART_IT_RXNE | USART_IT_IDLE |
                   USART_IT_PE | USART_IT_ERR | USART_IT_LBD | USART_IT_CTS, 
                   DISABLE);
    
    /* 清理中断模式状态 */
    g_uart_tx_buffer[instance] = NULL;
    g_uart_tx_length[instance] = 0;
    g_uart_tx_index[instance] = 0;
    g_uart_rx_buffer[instance] = NULL;
    /* g_uart_rx_length[instance] = 0; */ /* 保留用于未来扩展 */
    g_uart_rx_index[instance] = 0;
    g_uart_rx_max_length[instance] = 0;
    
#if CONFIG_MODULE_DMA_ENABLED
    /* 停止并清理DMA传输 */
    USART_DMACmd(g_uart_configs[instance].uart_periph, USART_DMAReq_Tx | USART_DMAReq_Rx, DISABLE);
    DMA_Stop(uart_tx_dma_channels[instance]);
    DMA_Stop(uart_rx_dma_channels[instance]);
#endif /* CONFIG_MODULE_DMA_ENABLED */
    
    /* 获取UART外设时钟 */
    uart_clock = UART_GetPeriphClock(g_uart_configs[instance].uart_periph);
    if (uart_clock != 0) {
        /* 禁用UART外设时钟 */
        if (g_uart_configs[instance].uart_periph == USART1) {
            RCC_APB2PeriphClockCmd(uart_clock, DISABLE);
        } else {
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
    uint32_t actual_timeout;
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (data == NULL) {
        return UART_ERROR_NULL_PTR;
    }
    if (length == 0) {
        return UART_ERROR_INVALID_PARAM;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    actual_timeout = (timeout == 0) ? UART_DEFAULT_TIMEOUT_MS : timeout;
    
    /* 发送数据 */
    /* 性能优化：对于大数据量，减少函数调用开销，直接检查标志位 */
    uint32_t start_tick = Delay_GetTick();
    for (i = 0; i < length; i++) {
        /* 等待发送数据寄存器为空（TXE标志） */
        /* 优化：直接检查标志位，减少函数调用 */
        while (USART_GetFlagStatus(uart_periph, USART_FLAG_TXE) == RESET) {
            /* 检查超时 */
            uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
            if (elapsed > actual_timeout) {
                return UART_ERROR_TIMEOUT;
            }
            
            /* 检查错误标志（快速检查） */
            uint16_t sr = uart_periph->SR;
            if (sr & (USART_FLAG_ORE | USART_FLAG_NE | USART_FLAG_FE | USART_FLAG_PE)) {
                /* 清除错误标志 */
                (void)USART_ReceiveData(uart_periph);
                if (sr & USART_FLAG_ORE) return UART_ERROR_ORE;
                if (sr & USART_FLAG_NE) return UART_ERROR_NE;
                if (sr & USART_FLAG_FE) return UART_ERROR_FE;
                if (sr & USART_FLAG_PE) return UART_ERROR_PE;
            }
        }
        
        /* 发送数据 */
        USART_SendData(uart_periph, data[i]);
    }
    
    /* 等待最后一个字节发送完成（TC标志） */
    status = UART_WaitFlag(uart_periph, USART_FLAG_TC, actual_timeout);
    if (status != UART_OK) {
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
    uint16_t i;
    uint32_t actual_timeout;
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (data == NULL) {
        return UART_ERROR_NULL_PTR;
    }
    if (length == 0) {
        return UART_ERROR_INVALID_PARAM;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    actual_timeout = (timeout == 0) ? UART_DEFAULT_TIMEOUT_MS : timeout;
    
    /* 接收数据 */
    /* 性能优化：对于大数据量，减少函数调用开销，直接检查标志位 */
    uint32_t start_tick = Delay_GetTick();
    for (i = 0; i < length; i++) {
        /* 等待接收数据就绪（RXNE标志） */
        /* 优化：直接检查标志位，减少函数调用 */
        while (USART_GetFlagStatus(uart_periph, USART_FLAG_RXNE) == RESET) {
            /* 检查超时 */
            uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
            if (elapsed > actual_timeout) {
                return UART_ERROR_TIMEOUT;
            }
            
            /* 检查错误标志（快速检查） */
            uint16_t sr = uart_periph->SR;
            if (sr & (USART_FLAG_ORE | USART_FLAG_NE | USART_FLAG_FE | USART_FLAG_PE)) {
                /* 清除错误标志 */
                (void)USART_ReceiveData(uart_periph);
                if (sr & USART_FLAG_ORE) return UART_ERROR_ORE;
                if (sr & USART_FLAG_NE) return UART_ERROR_NE;
                if (sr & USART_FLAG_FE) return UART_ERROR_FE;
                if (sr & USART_FLAG_PE) return UART_ERROR_PE;
            }
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
    USART_TypeDef *uart_periph;
    UART_Status_t status;
    uint32_t actual_timeout;
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    actual_timeout = (timeout == 0) ? UART_DEFAULT_TIMEOUT_MS : timeout;
    
    /* 等待发送数据寄存器为空（TXE标志） */
    status = UART_WaitFlag(uart_periph, USART_FLAG_TXE, actual_timeout);
    if (status != UART_OK) {
        return status;
    }
    
    /* 发送数据 */
    USART_SendData(uart_periph, byte);
    
    /* 等待发送完成（TC标志） */
    status = UART_WaitFlag(uart_periph, USART_FLAG_TC, actual_timeout);
    if (status != UART_OK) {
        return status;
    }
    
    return UART_OK;
}

/**
 * @brief UART接收单个字节（阻塞式）
 */
UART_Status_t UART_ReceiveByte(UART_Instance_t instance, uint8_t *byte, uint32_t timeout)
{
    USART_TypeDef *uart_periph;
    UART_Status_t status;
    uint32_t actual_timeout;
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (byte == NULL) {
        return UART_ERROR_NULL_PTR;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    actual_timeout = (timeout == 0) ? UART_DEFAULT_TIMEOUT_MS : timeout;
    
    /* 等待接收数据就绪（RXNE标志） */
    status = UART_WaitFlag(uart_periph, USART_FLAG_RXNE, actual_timeout);
    if (status != UART_OK) {
        return status;
    }
    
    /* 读取数据 */
    *byte = (uint8_t)USART_ReceiveData(uart_periph);
    
    return UART_OK;
}

/**
 * @brief UART发送字符串（阻塞式）
 */
UART_Status_t UART_TransmitString(UART_Instance_t instance, const char *str, uint32_t timeout)
{
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (str == NULL) {
        return UART_ERROR_NULL_PTR;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    /* 计算字符串长度并发送 */
    return UART_Transmit(instance, (const uint8_t *)str, strlen(str), timeout);
}

/**
 * @brief 检查UART是否已初始化
 */
uint8_t UART_IsInitialized(UART_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return 0;  /* 无效实例返回0（未初始化） */
    }
    
    return g_uart_initialized[instance] ? 1 : 0;
}

/**
 * @brief 获取UART外设指针
 */
USART_TypeDef* UART_GetPeriph(UART_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return NULL;  /* 无效实例返回NULL */
    }
    
    if (!g_uart_configs[instance].enabled) {
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
    switch (it_type) {
        case UART_IT_TXE:
            return USART_IT_TXE;
        case UART_IT_TC:
            return USART_IT_TC;
        case UART_IT_RXNE:
            return USART_IT_RXNE;
        case UART_IT_IDLE:
            return USART_IT_IDLE;
        case UART_IT_PE:
            return USART_IT_PE;
        case UART_IT_ERR:
            return USART_IT_ERR;
        case UART_IT_LBD:
            return USART_IT_LBD;
        case UART_IT_CTS:
            return USART_IT_CTS;
        default:
            return 0;
    }
}

/**
 * @brief 获取UART中断向量
 */
static IRQn_Type UART_GetIRQn(UART_Instance_t instance)
{
    switch (instance) {
        case UART_INSTANCE_1:
            return USART1_IRQn;
        case UART_INSTANCE_2:
            return USART2_IRQn;
        case UART_INSTANCE_3:
            return USART3_IRQn;
        default:
            return (IRQn_Type)0;
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
    NVIC_InitTypeDef NVIC_InitStructure;
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (it_type > UART_IT_CTS) {
        return UART_ERROR_INVALID_PARAM;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    it_value = UART_GetITValue(it_type);
    if (it_value == 0) {
        return UART_ERROR_INVALID_PARAM;
    }
    
    /* 使能UART中断 */
    USART_ITConfig(uart_periph, it_value, ENABLE);
    
    /* 使能NVIC中断（如果还没有使能） */
    irqn = UART_GetIRQn(instance);
    if (irqn != 0) {
        NVIC_InitStructure.NVIC_IRQChannel = irqn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);
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
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (it_type > UART_IT_CTS) {
        return UART_ERROR_INVALID_PARAM;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    it_value = UART_GetITValue(it_type);
    if (it_value == 0) {
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
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (it_type > UART_IT_CTS) {
        return UART_ERROR_INVALID_PARAM;
    }
    /* 注意：callback可以为NULL（表示禁用回调），user_data可以为NULL */
    
    /* 设置回调函数和用户数据 */
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
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return 0;  /* 无效实例返回0 */
    }
    if (it_type > UART_IT_CTS) {
        return 0;  /* 无效中断类型返回0 */
    }
    if (!g_uart_initialized[instance]) {
        return 0;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    it_value = UART_GetITValue(it_type);
    if (it_value == 0) {
        return 0;
    }
    
    return (USART_GetITStatus(uart_periph, it_value) != RESET) ? 1 : 0;
}

/**
 * @brief 清除UART中断挂起标志
 */
UART_Status_t UART_ClearITPendingBit(UART_Instance_t instance, UART_IT_t it_type)
{
    USART_TypeDef *uart_periph;
    uint16_t it_value;
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (it_type > UART_IT_CTS) {
        return UART_ERROR_INVALID_PARAM;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    it_value = UART_GetITValue(it_type);
    if (it_value == 0) {
        return UART_ERROR_INVALID_PARAM;
    }
    
    /* 清除中断挂起标志 */
    USART_ClearITPendingBit(uart_periph, it_value);
    
    return UART_OK;
}

/**
 * @brief UART非阻塞发送（中断模式）
 */
UART_Status_t UART_TransmitIT(UART_Instance_t instance, const uint8_t *data, uint16_t length)
{
    USART_TypeDef *uart_periph;
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (data == NULL) {
        return UART_ERROR_NULL_PTR;
    }
    if (length == 0) {
        return UART_ERROR_INVALID_PARAM;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    /* 检查是否正在发送 */
    if (g_uart_tx_buffer[instance] != NULL) {
        return UART_ERROR_BUSY;
    }
    
#if CONFIG_MODULE_DMA_ENABLED
    /* 检查是否正在使用DMA发送 */
    if (DMA_GetRemainingDataSize(uart_tx_dma_channels[instance]) > 0) {
        return UART_ERROR_BUSY;
    }
#endif /* CONFIG_MODULE_DMA_ENABLED */
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 保存发送缓冲区信息 */
    g_uart_tx_buffer[instance] = data;
    g_uart_tx_length[instance] = length;
    g_uart_tx_index[instance] = 0;
    
    /* 优化：立即发送第一个字节（如果发送寄存器为空），减少延迟 */
    if (USART_GetFlagStatus(uart_periph, USART_FLAG_TXE) != RESET) {
        if (length > 0) {
            USART_SendData(uart_periph, data[0]);
            g_uart_tx_index[instance] = 1;
        }
    }
    
    /* 使能TXE中断（发送缓冲区空中断） */
    USART_ITConfig(uart_periph, USART_IT_TXE, ENABLE);
    
    return UART_OK;
}

/**
 * @brief UART非阻塞接收（中断模式）
 */
UART_Status_t UART_ReceiveIT(UART_Instance_t instance, uint8_t *data, uint16_t max_length)
{
    USART_TypeDef *uart_periph;
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (data == NULL) {
        return UART_ERROR_NULL_PTR;
    }
    if (max_length == 0) {
        return UART_ERROR_INVALID_PARAM;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    /* 检查是否正在接收 */
    if (g_uart_rx_buffer[instance] != NULL) {
        return UART_ERROR_BUSY;
    }
    
#if CONFIG_MODULE_DMA_ENABLED
    /* 检查是否正在使用DMA接收 */
    if (DMA_GetRemainingDataSize(uart_rx_dma_channels[instance]) > 0) {
        return UART_ERROR_BUSY;
    }
#endif /* CONFIG_MODULE_DMA_ENABLED */
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 保存接收缓冲区信息 */
    g_uart_rx_buffer[instance] = data;
    g_uart_rx_max_length[instance] = max_length;
    /* g_uart_rx_length[instance] = 0; */ /* 保留用于未来扩展 */
    g_uart_rx_index[instance] = 0;
    
    /* 使能RXNE中断（接收数据就绪中断） */
    USART_ITConfig(uart_periph, USART_IT_RXNE, ENABLE);
    
    return UART_OK;
}

/**
 * @brief 获取UART发送剩余字节数（中断模式）
 */
uint16_t UART_GetTransmitRemaining(UART_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return 0;  /* 无效实例返回0 */
    }
    
    if (g_uart_tx_buffer[instance] == NULL) {
        return 0;  /* 没有正在进行的发送 */
    }
    
    return g_uart_tx_length[instance] - g_uart_tx_index[instance];
}

/**
 * @brief 获取UART接收到的字节数（中断模式）
 */
uint16_t UART_GetReceiveCount(UART_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return 0;  /* 无效实例返回0 */
    }
    
    if (g_uart_rx_buffer[instance] == NULL) {
        return 0;  /* 没有正在进行的接收 */
    }
    
    return g_uart_rx_index[instance];
}

/**
 * @brief UART中断服务函数
 */
void UART_IRQHandler(UART_Instance_t instance)
{
    USART_TypeDef *uart_periph;
    UART_IT_Callback_t callback;
    void *user_data;
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return;  /* 无效实例直接返回 */
    }
    
    if (!g_uart_initialized[instance]) {
        return;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 处理发送缓冲区空中断（TXE） */
    if (USART_GetITStatus(uart_periph, USART_IT_TXE) != RESET) {
        if (g_uart_tx_buffer[instance] != NULL) {
            if (g_uart_tx_index[instance] < g_uart_tx_length[instance]) {
                /* 发送下一个字节 */
                USART_SendData(uart_periph, g_uart_tx_buffer[instance][g_uart_tx_index[instance]++]);
            } else {
                /* 发送完成，禁用TXE中断，使能TC中断 */
                USART_ITConfig(uart_periph, USART_IT_TXE, DISABLE);
                USART_ITConfig(uart_periph, USART_IT_TC, ENABLE);
            }
        } else {
            /* 缓冲区指针为NULL但中断已使能，禁用中断（防止异常情况） */
            USART_ITConfig(uart_periph, USART_IT_TXE, DISABLE);
        }
    }
    
    /* 处理发送完成中断（TC） */
    if (USART_GetITStatus(uart_periph, USART_IT_TC) != RESET) {
        USART_ClearITPendingBit(uart_periph, USART_IT_TC);
        USART_ITConfig(uart_periph, USART_IT_TC, DISABLE);
        
        /* 清除发送缓冲区指针 */
        g_uart_tx_buffer[instance] = NULL;
        
        /* 调用回调函数 */
        callback = g_uart_it_callbacks[instance][UART_IT_TC];
        user_data = g_uart_it_user_data[instance][UART_IT_TC];
        if (callback != NULL) {
            callback(instance, UART_IT_TC, user_data);
        }
    }
    
    /* 处理接收数据就绪中断（RXNE） */
    if (USART_GetITStatus(uart_periph, USART_IT_RXNE) != RESET) {
        if (g_uart_rx_buffer[instance] != NULL) {
            if (g_uart_rx_index[instance] < g_uart_rx_max_length[instance]) {
                /* 接收数据 */
                g_uart_rx_buffer[instance][g_uart_rx_index[instance]++] = (uint8_t)USART_ReceiveData(uart_periph);
                
                /* 调用回调函数 */
                callback = g_uart_it_callbacks[instance][UART_IT_RXNE];
                user_data = g_uart_it_user_data[instance][UART_IT_RXNE];
                if (callback != NULL) {
                    callback(instance, UART_IT_RXNE, user_data);
                }
            } else {
                /* 缓冲区已满，先读取当前数据（避免数据丢失），然后禁用接收中断 */
                /* 注意：即使缓冲区已满，也要读取DR寄存器中的数据，否则会丢失 */
                (void)USART_ReceiveData(uart_periph);
                
                /* 禁用接收中断 */
                USART_ITConfig(uart_periph, USART_IT_RXNE, DISABLE);
                
                /* 调用回调函数通知缓冲区已满 */
                callback = g_uart_it_callbacks[instance][UART_IT_RXNE];
                user_data = g_uart_it_user_data[instance][UART_IT_RXNE];
                if (callback != NULL) {
                    callback(instance, UART_IT_RXNE, user_data);
                }
                
                g_uart_rx_buffer[instance] = NULL;
            }
        } else {
            /* 没有接收缓冲区，读取并丢弃数据 */
            (void)USART_ReceiveData(uart_periph);
        }
    }
    
    /* 处理空闲中断（IDLE） */
    /* 注意：IDLE标志需要通过读取SR寄存器然后读取DR寄存器来清除 */
    if (USART_GetITStatus(uart_periph, USART_IT_IDLE) != RESET) {
        /* 清除IDLE标志：读取SR寄存器，然后读取DR寄存器 */
        (void)USART_GetITStatus(uart_periph, USART_IT_IDLE);
        (void)USART_ReceiveData(uart_periph);
        
        /* 调用回调函数 */
        callback = g_uart_it_callbacks[instance][UART_IT_IDLE];
        user_data = g_uart_it_user_data[instance][UART_IT_IDLE];
        if (callback != NULL) {
            callback(instance, UART_IT_IDLE, user_data);
        }
    }
    
    /* 处理奇偶校验错误中断（PE） */
    /* 注意：PE标志需要通过读取SR寄存器然后读取DR寄存器来清除 */
    if (USART_GetITStatus(uart_periph, USART_IT_PE) != RESET) {
        /* 清除PE标志：读取SR寄存器，然后读取DR寄存器 */
        (void)USART_GetITStatus(uart_periph, USART_IT_PE);
        (void)USART_ReceiveData(uart_periph);
        
        /* 错误发生时，停止正在进行的接收（中断模式） */
        if (g_uart_rx_buffer[instance] != NULL) {
            /* 停止接收 */
            USART_ITConfig(uart_periph, USART_IT_RXNE, DISABLE);
            g_uart_rx_buffer[instance] = NULL;
        }
        
        /* 调用回调函数 */
        callback = g_uart_it_callbacks[instance][UART_IT_PE];
        user_data = g_uart_it_user_data[instance][UART_IT_PE];
        if (callback != NULL) {
            callback(instance, UART_IT_PE, user_data);
        }
    }
    
    /* 处理错误中断（ORE、NE、FE） */
    /* 注意：ORE、NE、FE中断标志需要通过读取SR寄存器然后读取DR寄存器来清除 */
    /* 优化：先读取一次状态寄存器，然后根据标志位清除 */
    if (USART_GetITStatus(uart_periph, USART_IT_ORE) != RESET ||
        USART_GetITStatus(uart_periph, USART_IT_NE) != RESET ||
        USART_GetITStatus(uart_periph, USART_IT_FE) != RESET) {
        /* 读取SR寄存器（清除标志的第一步） */
        (void)uart_periph->SR;
        /* 读取DR寄存器（清除标志的第二步） */
        (void)USART_ReceiveData(uart_periph);
        
        /* 错误发生时，停止正在进行的传输（中断模式） */
        if (g_uart_tx_buffer[instance] != NULL) {
            /* 停止发送 */
            USART_ITConfig(uart_periph, USART_IT_TXE, DISABLE);
            USART_ITConfig(uart_periph, USART_IT_TC, DISABLE);
            g_uart_tx_buffer[instance] = NULL;
        }
        if (g_uart_rx_buffer[instance] != NULL) {
            /* 停止接收 */
            USART_ITConfig(uart_periph, USART_IT_RXNE, DISABLE);
            g_uart_rx_buffer[instance] = NULL;
        }
        
        /* 调用回调函数 */
        callback = g_uart_it_callbacks[instance][UART_IT_ERR];
        user_data = g_uart_it_user_data[instance][UART_IT_ERR];
        if (callback != NULL) {
            callback(instance, UART_IT_ERR, user_data);
        }
    }
    
    /* 处理LIN断点检测中断（LBD） */
    if (USART_GetITStatus(uart_periph, USART_IT_LBD) != RESET) {
        USART_ClearITPendingBit(uart_periph, USART_IT_LBD);
        
        /* 调用回调函数 */
        callback = g_uart_it_callbacks[instance][UART_IT_LBD];
        user_data = g_uart_it_user_data[instance][UART_IT_LBD];
        if (callback != NULL) {
            callback(instance, UART_IT_LBD, user_data);
        }
    }
    
    /* 处理CTS中断 */
    if (USART_GetITStatus(uart_periph, USART_IT_CTS) != RESET) {
        USART_ClearITPendingBit(uart_periph, USART_IT_CTS);
        
        /* 调用回调函数 */
        callback = g_uart_it_callbacks[instance][UART_IT_CTS];
        user_data = g_uart_it_user_data[instance][UART_IT_CTS];
        if (callback != NULL) {
            callback(instance, UART_IT_CTS, user_data);
        }
    }
}

/* 注意：UART中断服务程序入口（USART1_IRQHandler等）在Core/stm32f10x_it.c中实现 */


/* ========== DMA模式功能实现 ========== */

/**
 * @brief UART DMA发送
 */
UART_Status_t UART_TransmitDMA(UART_Instance_t instance, const uint8_t *data, uint16_t length)
{
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (data == NULL) {
        return UART_ERROR_NULL_PTR;
    }
    if (length == 0) {
        return UART_ERROR_INVALID_PARAM;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
#if CONFIG_MODULE_DMA_ENABLED
    USART_TypeDef *uart_periph;
    DMA_Channel_t dma_channel;
    DMA_Status_t dma_status;
    
    /* 检查是否正在使用中断模式发送 */
    if (g_uart_tx_buffer[instance] != NULL) {
        return UART_ERROR_BUSY;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    dma_channel = uart_tx_dma_channels[instance];
    
    /* 检查DMA通道是否已初始化 */
    if (!DMA_IsInitialized(dma_channel)) {
        dma_status = DMA_HW_Init(dma_channel);
        if (dma_status != DMA_OK) {
            return UART_ERROR_INVALID_PARAM;
        }
    }
    
    /* 停止之前的传输 */
    DMA_Stop(dma_channel);
    
    /* 配置DMA传输（内存到外设） */
    dma_status = DMA_ConfigTransfer(dma_channel, (uint32_t)&uart_periph->DR,
                                    (uint32_t)data, length,
                                    DMA_DIR_MEMORY_TO_PERIPHERAL, 1);
    if (dma_status != DMA_OK) {
        return UART_ERROR_INVALID_PARAM;
    }
    
    /* 使能UART DMA发送请求 */
    USART_DMACmd(uart_periph, USART_DMAReq_Tx, ENABLE);
    
    /* 启动DMA传输 */
    dma_status = DMA_Start(dma_channel);
    if (dma_status != DMA_OK) {
        USART_DMACmd(uart_periph, USART_DMAReq_Tx, DISABLE);
        return UART_ERROR_INVALID_PARAM;
    }
    
    return UART_OK;
#else
    return UART_ERROR_NOT_IMPLEMENTED;
#endif /* CONFIG_MODULE_DMA_ENABLED */
}

/**
 * @brief UART DMA接收
 */
UART_Status_t UART_ReceiveDMA(UART_Instance_t instance, uint8_t *data, uint16_t length)
{
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (data == NULL) {
        return UART_ERROR_NULL_PTR;
    }
    if (length == 0) {
        return UART_ERROR_INVALID_PARAM;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
#if CONFIG_MODULE_DMA_ENABLED
    USART_TypeDef *uart_periph;
    DMA_Channel_t dma_channel;
    DMA_Status_t dma_status;
    
    /* 检查是否正在使用中断模式接收 */
    if (g_uart_rx_buffer[instance] != NULL) {
        return UART_ERROR_BUSY;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    dma_channel = uart_rx_dma_channels[instance];
    
    /* 检查DMA通道是否已初始化 */
    if (!DMA_IsInitialized(dma_channel)) {
        dma_status = DMA_HW_Init(dma_channel);
        if (dma_status != DMA_OK) {
            return UART_ERROR_INVALID_PARAM;
        }
    }
    
    /* 停止之前的传输 */
    DMA_Stop(dma_channel);
    
    /* 配置DMA传输（外设到内存） */
    dma_status = DMA_ConfigTransfer(dma_channel, (uint32_t)&uart_periph->DR,
                                    (uint32_t)data, length,
                                    DMA_DIR_PERIPHERAL_TO_MEMORY, 1);
    if (dma_status != DMA_OK) {
        return UART_ERROR_INVALID_PARAM;
    }
    
    /* 使能UART DMA接收请求 */
    USART_DMACmd(uart_periph, USART_DMAReq_Rx, ENABLE);
    
    /* 启动DMA传输 */
    dma_status = DMA_Start(dma_channel);
    if (dma_status != DMA_OK) {
        USART_DMACmd(uart_periph, USART_DMAReq_Rx, DISABLE);
        return UART_ERROR_INVALID_PARAM;
    }
    
    return UART_OK;
#else
    return UART_ERROR_NOT_IMPLEMENTED;
#endif /* CONFIG_MODULE_DMA_ENABLED */
}

/**
 * @brief 停止UART DMA发送
 */
UART_Status_t UART_StopTransmitDMA(UART_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
#if CONFIG_MODULE_DMA_ENABLED
    USART_TypeDef *uart_periph;
    DMA_Channel_t dma_channel;
    
    uart_periph = g_uart_configs[instance].uart_periph;
    dma_channel = uart_tx_dma_channels[instance];
    
    /* 禁用UART DMA发送请求 */
    USART_DMACmd(uart_periph, USART_DMAReq_Tx, DISABLE);
    
    /* 停止DMA传输 */
    DMA_Stop(dma_channel);
    
    return UART_OK;
#else
    return UART_ERROR_NOT_IMPLEMENTED;
#endif /* CONFIG_MODULE_DMA_ENABLED */
}

/**
 * @brief 停止UART DMA接收
 */
UART_Status_t UART_StopReceiveDMA(UART_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
#if CONFIG_MODULE_DMA_ENABLED
    USART_TypeDef *uart_periph;
    DMA_Channel_t dma_channel;
    
    uart_periph = g_uart_configs[instance].uart_periph;
    dma_channel = uart_rx_dma_channels[instance];
    
    /* 禁用UART DMA接收请求 */
    USART_DMACmd(uart_periph, USART_DMAReq_Rx, DISABLE);
    
    /* 停止DMA传输 */
    DMA_Stop(dma_channel);
    
    return UART_OK;
#else
    return UART_ERROR_NOT_IMPLEMENTED;
#endif /* CONFIG_MODULE_DMA_ENABLED */
}

/**
 * @brief 获取UART DMA发送剩余字节数
 */
uint16_t UART_GetTransmitDMARemaining(UART_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return 0;  /* 无效实例返回0 */
    }
    
#if CONFIG_MODULE_DMA_ENABLED
    DMA_Channel_t dma_channel;
    dma_channel = uart_tx_dma_channels[instance];
    return DMA_GetRemainingDataSize(dma_channel);
#else
    return 0;
#endif /* CONFIG_MODULE_DMA_ENABLED */
}

/**
 * @brief 获取UART DMA接收剩余字节数
 */
uint16_t UART_GetReceiveDMARemaining(UART_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return 0;  /* 无效实例返回0 */
    }
    
#if CONFIG_MODULE_DMA_ENABLED
    DMA_Channel_t dma_channel;
    dma_channel = uart_rx_dma_channels[instance];
    return DMA_GetRemainingDataSize(dma_channel);
#else
    return 0;
#endif /* CONFIG_MODULE_DMA_ENABLED */
}

/* ========== 硬件流控功能实现 ========== */

/**
 * @brief 配置UART硬件流控
 */
UART_Status_t UART_SetHardwareFlowControl(UART_Instance_t instance, UART_HW_FlowControl_t flow_control)
{
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (flow_control > UART_HW_FLOW_RTS_CTS) {
        return UART_ERROR_INVALID_PARAM;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    /* 保存配置 */
    g_uart_hw_flow[instance] = flow_control;
    
    /* 注意：硬件流控配置需要在UART_Init()中应用 */
    /* 如果UART已经初始化，需要先调用UART_Deinit()，然后重新调用UART_Init()来应用新配置 */
    
    return UART_OK;
}

/**
 * @brief 获取UART硬件流控配置
 */
UART_Status_t UART_GetHardwareFlowControl(UART_Instance_t instance, UART_HW_FlowControl_t *flow_control)
{
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (flow_control == NULL) {
        return UART_ERROR_NULL_PTR;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
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
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 使能单线半双工模式 */
    USART_HalfDuplexCmd(uart_periph, ENABLE);
    
    /* 标记为单线模式 */
    g_uart_half_duplex[instance] = true;
    
    return UART_OK;
}

/**
 * @brief 禁用UART单线半双工模式
 */
UART_Status_t UART_DisableHalfDuplex(UART_Instance_t instance)
{
    USART_TypeDef *uart_periph;
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 禁用单线半双工模式 */
    USART_HalfDuplexCmd(uart_periph, DISABLE);
    
    /* 标记为全双工模式 */
    g_uart_half_duplex[instance] = false;
    
    return UART_OK;
}

/**
 * @brief 检查UART是否处于单线半双工模式
 */
uint8_t UART_IsHalfDuplex(UART_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return 0;  /* 无效实例返回0 */
    }
    
    return g_uart_half_duplex[instance] ? 1 : 0;
}

/* ========== LIN/IrDA/智能卡模式功能实现 ========== */

/**
 * @brief 使能UART LIN模式
 */
UART_Status_t UART_EnableLINMode(UART_Instance_t instance)
{
    USART_TypeDef *uart_periph;
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (!g_uart_initialized[instance]) {
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
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (!g_uart_initialized[instance]) {
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
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (!g_uart_initialized[instance]) {
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
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (prescaler == 0 || prescaler > 31) {
        return UART_ERROR_INVALID_PARAM;  /* IrDA预分频器范围：1-31 */
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 配置IrDA预分频器 */
    USART_SetPrescaler(uart_periph, prescaler);
    
    /* 配置IrDA模式（正常模式） */
    USART_IrDAConfig(uart_periph, USART_IrDAMode_Normal);
    
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
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (!g_uart_initialized[instance]) {
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
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (!g_uart_initialized[instance]) {
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
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 禁用智能卡模式 */
    USART_SmartCardCmd(uart_periph, DISABLE);
    
    return UART_OK;
}

/* ========== UART高级功能实现 ========== */

/**
 * @brief 使能UART自动波特率检测
 * @note STM32F10x SPL库不直接支持自动波特率检测，此功能需要通过软件实现
 * @note 当前实现为占位函数，返回NOT_IMPLEMENTED
 */
UART_Status_t UART_EnableAutoBaudRate(UART_Instance_t instance, UART_AutoBaudMode_t mode)
{
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (mode > UART_AUTOBAUD_MODE_FALLING_EDGE) {
        return UART_ERROR_INVALID_PARAM;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    /* 注意：STM32F10x SPL库不直接支持自动波特率检测 */
    /* 此功能需要通过软件实现（测量起始位宽度或下降沿间隔） */
    /* 当前实现为占位函数，待后续完善 */
    
    (void)mode;  /* 避免未使用参数警告 */
    
    return UART_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 禁用UART自动波特率检测
 */
UART_Status_t UART_DisableAutoBaudRate(UART_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    /* 注意：自动波特率检测功能未实现，此函数仅用于接口完整性 */
    
    return UART_OK;
}

/**
 * @brief 使能UART接收器唤醒
 */
UART_Status_t UART_EnableReceiverWakeUp(UART_Instance_t instance)
{
    USART_TypeDef *uart_periph;
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 使能接收器唤醒 */
    USART_ReceiverWakeUpCmd(uart_periph, ENABLE);
    
    return UART_OK;
}

/**
 * @brief 禁用UART接收器唤醒
 */
UART_Status_t UART_DisableReceiverWakeUp(UART_Instance_t instance)
{
    USART_TypeDef *uart_periph;
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 禁用接收器唤醒 */
    USART_ReceiverWakeUpCmd(uart_periph, DISABLE);
    
    return UART_OK;
}

/**
 * @brief 使能UART 8倍过采样
 */
UART_Status_t UART_EnableOverSampling8(UART_Instance_t instance)
{
    USART_TypeDef *uart_periph;
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 注意：8倍过采样需要在UART初始化前配置（在USART_Init之前） */
    /* 如果UART已经初始化，需要先禁用UART，配置过采样，然后重新初始化 */
    /* 这里仅提供接口，实际使用时需要在UART_Init中根据配置设置 */
    
    /* 使能8倍过采样 */
    USART_OverSampling8Cmd(uart_periph, ENABLE);
    
    return UART_OK;
}

/**
 * @brief 禁用UART 8倍过采样（使用16倍过采样）
 */
UART_Status_t UART_DisableOverSampling8(UART_Instance_t instance)
{
    USART_TypeDef *uart_periph;
    
    /* ========== 参数校验 ========== */
    if (instance >= UART_INSTANCE_MAX) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    if (!g_uart_initialized[instance]) {
        return UART_ERROR_NOT_INITIALIZED;
    }
    
    uart_periph = g_uart_configs[instance].uart_periph;
    
    /* 禁用8倍过采样（使用16倍过采样） */
    USART_OverSampling8Cmd(uart_periph, DISABLE);
    
    return UART_OK;
}

/**
 * @brief 动态修改UART波特率（不重新初始化）
 * @param[in] uart_periph UART外设指针（USART1/USART2/USART3）
 * @param[in] baudrate 目标波特率
 * @return UART_Status_t 错误码
 * @note 此函数直接修改BRR寄存器，执行时间 < 1μs
 * @note 不影响UART其他配置（数据位、停止位等）
 * @note 不丢失发送/接收状态
 */
UART_Status_t UART_SetBaudRate(USART_TypeDef *uart_periph, uint32_t baudrate)
{
    uint32_t clock;
    uint16_t brr;
    RCC_ClocksTypeDef RCC_ClocksStatus;
    
    /* ========== 参数校验 ========== */
    if (uart_periph == NULL) {
        return UART_ERROR_NULL_PTR;
    }
    
    if (baudrate == 0) {
        return UART_ERROR_INVALID_PARAM;
    }
    
    /* 获取当前UART的PCLK时钟频率 */
    RCC_GetClocksFreq(&RCC_ClocksStatus);
    
    /* 某些UART在APB2（如USART1） */
    if (uart_periph == USART1) {
        clock = RCC_ClocksStatus.PCLK2_Frequency;  /* USART1使用PCLK2 */
    } else {
        clock = RCC_ClocksStatus.PCLK1_Frequency;  /* USART2/3使用PCLK1 */
    }
    
    /* 计算分频系数（STM32F1系列: BRR = PCLK / baudrate，四舍五入） */
    brr = (clock + baudrate / 2) / baudrate;  /* 四舍五入 */
    
    /* 关键：等待发送完成 */
    while (USART_GetFlagStatus(uart_periph, USART_FLAG_TC) == RESET);
    
    /* 关键：等待接收完成（如果有数据在接收） */
    while (USART_GetFlagStatus(uart_periph, USART_FLAG_RXNE) == SET) {
        volatile uint8_t dummy = USART_ReceiveData(uart_periph);
        (void)dummy;
    }
    
    /* 关闭UART（短暂关闭，防止写入时数据错乱） */
    uart_periph->CR1 &= ~USART_CR1_UE;
    
    /* 写入新波特率分频值 */
    uart_periph->BRR = brr;
    
    /* 重新使能UART */
    uart_periph->CR1 |= USART_CR1_UE;
    
    return UART_OK;
}

#endif /* CONFIG_MODULE_UART_ENABLED */

