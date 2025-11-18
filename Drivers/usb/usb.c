/**
 * @file usb.c
 * @brief USB驱动模块实现
 * @note 本模块提供基础框架，完整的USB协议栈需要根据具体应用扩展
 */

#include "usb.h"
#include "board.h"
#include "config.h"
#include "error_handler.h"
#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include <stdbool.h>
#include <string.h>

#if CONFIG_MODULE_USB_ENABLED

#if defined(STM32F10X_MD) || defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_XL)

/* USB寄存器定义（基于CMSIS） */
#define USB_BASE                ((uint32_t)0x40005C00)
#define USB_EP0R                (*(volatile uint16_t *)(USB_BASE + 0x00))
#define USB_EP1R                (*(volatile uint16_t *)(USB_BASE + 0x04))
#define USB_EP2R                (*(volatile uint16_t *)(USB_BASE + 0x08))
#define USB_EP3R                (*(volatile uint16_t *)(USB_BASE + 0x0C))
#define USB_EP4R                (*(volatile uint16_t *)(USB_BASE + 0x10))
#define USB_EP5R                (*(volatile uint16_t *)(USB_BASE + 0x14))
#define USB_EP6R                (*(volatile uint16_t *)(USB_BASE + 0x18))
#define USB_EP7R                (*(volatile uint16_t *)(USB_BASE + 0x1C))
#define USB_CNTR                (*(volatile uint16_t *)(USB_BASE + 0x40))
#define USB_ISTR                (*(volatile uint16_t *)(USB_BASE + 0x44))
#define USB_FNR                 (*(volatile uint16_t *)(USB_BASE + 0x48))
#define USB_DADDR               (*(volatile uint8_t  *)(USB_BASE + 0x4C))
#define USB_BTABLE               (*(volatile uint16_t *)(USB_BASE + 0x50))

/* USB状态 */
static bool g_usb_initialized = false;
static bool g_usb_enabled = false;
static uint8_t g_usb_status = 0; /* 0=未初始化，1=已初始化，2=已连接，3=已配置 */

/* 事件回调 */
static USB_EventCallback_t g_usb_event_callback = NULL;
static void *g_usb_user_data = NULL;

/**
 * @brief 获取端点寄存器地址
 */
static volatile uint16_t *USB_GetEPRegister(uint8_t endpoint)
{
    switch (endpoint)
    {
        case 0: return &USB_EP0R;
        case 1: return &USB_EP1R;
        case 2: return &USB_EP2R;
        case 3: return &USB_EP3R;
        case 4: return &USB_EP4R;
        case 5: return &USB_EP5R;
        case 6: return &USB_EP6R;
        case 7: return &USB_EP7R;
        default: return NULL;
    }
}

/**
 * @brief USB初始化
 */
USB_Status_t USB_Init(void)
{
    if (g_usb_initialized)
    {
        ERROR_HANDLER_Report(ERROR_BASE_USB, __FILE__, __LINE__, "USB already initialized");
        return USB_ERROR_ALREADY_INITIALIZED;
    }
    
    /* 使能USB时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, ENABLE);
    
    /* 配置USB时钟（需要48MHz，通常由PLL提供） */
    /* 注意：USB需要外部48MHz晶振或通过PLL生成48MHz时钟 */
    /* 这里假设系统时钟已经正确配置为支持USB */
    
    /* 复位USB外设 */
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_USB, ENABLE);
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_USB, DISABLE);
    
    /* 配置USB中断 */
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    /* 使能USB中断 */
    USB_CNTR = USB_CNTR_CTRM | USB_CNTR_RESETM | USB_CNTR_SUSPM | USB_CNTR_WKUPM;
    
    g_usb_initialized = true;
    g_usb_status = 1;
    
    return USB_OK;
}

/**
 * @brief USB反初始化
 */
USB_Status_t USB_Deinit(void)
{
    if (!g_usb_initialized)
    {
        return USB_ERROR_NOT_INITIALIZED;
    }
    
    /* 禁用USB */
    USB_Disable();
    
    /* 禁用USB中断 */
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = DISABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    /* 禁用USB时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, DISABLE);
    
    g_usb_initialized = false;
    g_usb_status = 0;
    
    return USB_OK;
}

/**
 * @brief 配置USB端点
 */
USB_Status_t USB_ConfigEndpoint(const USB_EP_Config_t *config)
{
    volatile uint16_t *ep_register;
    
    if (!g_usb_initialized)
    {
        return USB_ERROR_NOT_INITIALIZED;
    }
    
    if (config == NULL)
    {
        return USB_ERROR_INVALID_PARAM;
    }
    
    if (config->endpoint > 7)
    {
        return USB_ERROR_INVALID_ENDPOINT;
    }
    
    ep_register = USB_GetEPRegister(config->endpoint);
    if (ep_register == NULL)
    {
        return USB_ERROR_INVALID_ENDPOINT;
    }
    
    /* 配置端点寄存器 */
    /* 注意：这里只是基础框架，完整的端点配置需要根据USB协议栈实现 */
    /* 包括端点类型、缓冲区地址、数据包大小等 */
    
    /* 设置端点地址和类型 */
    uint16_t ep_value = config->endpoint | (config->type << 9);
    *ep_register = ep_value;
    
    return USB_OK;
}

/**
 * @brief 发送数据到USB端点
 */
USB_Status_t USB_Send(uint8_t endpoint, const uint8_t *data, uint16_t length)
{
    volatile uint16_t *ep_register;
    
    if (!g_usb_initialized || !g_usb_enabled)
    {
        return USB_ERROR_NOT_INITIALIZED;
    }
    
    if (endpoint > 7 || data == NULL || length == 0)
    {
        return USB_ERROR_INVALID_PARAM;
    }
    
    ep_register = USB_GetEPRegister(endpoint);
    if (ep_register == NULL)
    {
        return USB_ERROR_INVALID_ENDPOINT;
    }
    
    /* 注意：这里只是基础框架，完整的数据发送需要：
     * 1. 配置端点缓冲区地址
     * 2. 将数据复制到USB缓冲区
     * 3. 设置数据包长度
     * 4. 使能端点传输
     */
    
    /* 这里需要根据具体的USB协议栈实现 */
    
    return USB_OK;
}

/**
 * @brief 从USB端点接收数据
 */
USB_Status_t USB_Receive(uint8_t endpoint, uint8_t *data, uint16_t *length)
{
    volatile uint16_t *ep_register;
    
    if (!g_usb_initialized || !g_usb_enabled)
    {
        return USB_ERROR_NOT_INITIALIZED;
    }
    
    if (endpoint > 7 || data == NULL || length == NULL)
    {
        return USB_ERROR_INVALID_PARAM;
    }
    
    ep_register = USB_GetEPRegister(endpoint);
    if (ep_register == NULL)
    {
        return USB_ERROR_INVALID_ENDPOINT;
    }
    
    /* 注意：这里只是基础框架，完整的数据接收需要：
     * 1. 检查端点状态
     * 2. 从USB缓冲区读取数据
     * 3. 返回实际接收的数据长度
     */
    
    /* 这里需要根据具体的USB协议栈实现 */
    
    return USB_OK;
}

/**
 * @brief 设置USB事件回调函数
 */
USB_Status_t USB_SetEventCallback(USB_EventCallback_t callback, void *user_data)
{
    g_usb_event_callback = callback;
    g_usb_user_data = user_data;
    return USB_OK;
}

/**
 * @brief 使能USB
 */
USB_Status_t USB_Enable(void)
{
    if (!g_usb_initialized)
    {
        return USB_ERROR_NOT_INITIALIZED;
    }
    
    /* 使能USB功能 */
    USB_DADDR |= USB_DADDR_EF;
    
    g_usb_enabled = true;
    
    return USB_OK;
}

/**
 * @brief 禁用USB
 */
USB_Status_t USB_Disable(void)
{
    if (!g_usb_initialized)
    {
        return USB_ERROR_NOT_INITIALIZED;
    }
    
    /* 禁用USB功能 */
    USB_DADDR &= ~USB_DADDR_EF;
    
    g_usb_enabled = false;
    
    return USB_OK;
}

/**
 * @brief 检查USB是否已连接
 */
uint8_t USB_IsConnected(void)
{
    if (!g_usb_initialized)
    {
        return 0;
    }
    
    /* 检查USB连接状态 */
    /* 这里需要根据具体的USB协议栈实现 */
    
    return (g_usb_status >= 2) ? 1 : 0;
}

/**
 * @brief 获取USB当前状态
 */
uint8_t USB_GetStatus(void)
{
    return g_usb_status;
}

/**
 * @brief USB中断服务函数
 */
void USB_LP_CAN1_RX0_IRQHandler(void)
{
    uint16_t istr = USB_ISTR;
    
    /* 处理USB中断 */
    if (istr & USB_ISTR_CTR)
    {
        /* 正确传输中断 */
        uint8_t ep_num = istr & USB_ISTR_EP_ID;
        volatile uint16_t *ep_register = USB_GetEPRegister(ep_num);
        
        if (ep_register != NULL)
        {
            /* 清除CTR标志 */
            if (istr & USB_ISTR_DIR)
            {
                /* IN传输 */
                *ep_register &= ~USB_EP0R_CTR_TX;
            }
            else
            {
                /* OUT传输 */
                *ep_register &= ~USB_EP0R_CTR_RX;
            }
        }
        
        /* 调用事件回调 */
        if (g_usb_event_callback != NULL)
        {
            g_usb_event_callback(g_usb_user_data);
        }
    }
    
    if (istr & USB_ISTR_RESET)
    {
        /* USB复位中断 */
        USB_CNTR &= ~USB_CNTR_RESETM;
        g_usb_status = 2; /* 已连接 */
        
        /* 调用事件回调 */
        if (g_usb_event_callback != NULL)
        {
            g_usb_event_callback(g_usb_user_data);
        }
    }
    
    if (istr & USB_ISTR_SUSP)
    {
        /* USB挂起中断 */
        USB_CNTR &= ~USB_CNTR_SUSPM;
        
        /* 调用事件回调 */
        if (g_usb_event_callback != NULL)
        {
            g_usb_event_callback(g_usb_user_data);
        }
    }
    
    if (istr & USB_ISTR_WKUP)
    {
        /* USB唤醒中断 */
        USB_CNTR &= ~USB_CNTR_WKUPM;
        
        /* 调用事件回调 */
        if (g_usb_event_callback != NULL)
        {
            g_usb_event_callback(g_usb_user_data);
        }
    }
}

#endif /* STM32F10X_MD || STM32F10X_HD || STM32F10X_CL || STM32F10X_XL */

#endif /* CONFIG_MODULE_USB_ENABLED */

