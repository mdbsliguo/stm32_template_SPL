/**
 * @file can.c
 * @brief CAN总线驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的CAN总线驱动，支持CAN1/2，标准帧/扩展帧，过滤器配置
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_CAN_ENABLED

/* Include our header */
#include "can.h"

#include "gpio.h"
#include "delay.h"
#include "nvic.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_can.h"
#include "misc.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 从board.h加载配置 */
static CAN_Config_t g_can_configs[CAN_INSTANCE_MAX] = CAN_CONFIGS;

/* 初始化标志 */
static bool g_can_initialized[CAN_INSTANCE_MAX] = {false, false};

/* 默认超时时间（毫秒） */
#define CAN_DEFAULT_TIMEOUT_MS  1000

/* 中断回调函数数组（支持12种中断类型） */
static CAN_IT_Callback_t g_can_it_callbacks[CAN_INSTANCE_MAX][12] = {NULL};
static void *g_can_it_user_data[CAN_INSTANCE_MAX][12] = {NULL};

/* 中断模式接收缓冲区 */
static CAN_Message_t g_can_rx_buffer[CAN_INSTANCE_MAX][2] = {0};  /* FIFO0和FIFO1 */
static bool g_can_rx_pending[CAN_INSTANCE_MAX][2] = {false};

/**
 * @brief 获取CAN外设时钟
 * @param[in] can_periph CAN外设指针
 * @return uint32_t 时钟使能值，失败返回0
 */
static uint32_t CAN_GetPeriphClock(CAN_TypeDef *can_periph)
{
    if (can_periph == CAN1)
    {
        return RCC_APB1Periph_CAN1;
    }
    else if (can_periph == CAN2)
    {
        return RCC_APB1Periph_CAN2;
    }
    return 0;
}

/**
 * @brief 获取GPIO时钟
 * @param[in] port GPIO端口指针
 * @return uint32_t GPIO时钟使能值
 */
static uint32_t CAN_GetGPIOClock(GPIO_TypeDef *port)
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
 * @brief CAN初始化
 */
CAN_Status_t CAN_Init(CAN_Instance_t instance)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    CAN_InitTypeDef CAN_InitStructure;
    CAN_FilterInitTypeDef CAN_FilterInitStructure;
    CAN_Status_t status;
    GPIO_Status_t gpio_status;
    uint32_t can_clock;
    uint32_t gpio_clock;
    uint8_t init_status;
    
    /* 参数校验 */
    if (instance >= CAN_INSTANCE_MAX)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (!g_can_configs[instance].enabled)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (g_can_initialized[instance])
    {
        return CAN_OK;
    }
    
    /* 获取CAN外设时钟 */
    can_clock = CAN_GetPeriphClock(g_can_configs[instance].can_periph);
    if (can_clock == 0)
    {
        return CAN_ERROR_INVALID_PERIPH;
    }
    
    /* 使能CAN外设时钟 */
    RCC_APB1PeriphClockCmd(can_clock, ENABLE);
    
    /* 使能GPIO时钟 */
    gpio_clock = CAN_GetGPIOClock(g_can_configs[instance].tx_port);
    if (gpio_clock != 0)
    {
        RCC_APB2PeriphClockCmd(gpio_clock, ENABLE);
    }
    
    if (g_can_configs[instance].tx_port != g_can_configs[instance].rx_port)
    {
        gpio_clock = CAN_GetGPIOClock(g_can_configs[instance].rx_port);
        if (gpio_clock != 0)
        {
            RCC_APB2PeriphClockCmd(gpio_clock, ENABLE);
        }
    }
    
    /* 配置TX引脚为复用推挽输出 */
    GPIO_InitStructure.GPIO_Pin = g_can_configs[instance].tx_pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(g_can_configs[instance].tx_port, &GPIO_InitStructure);
    
    /* 配置RX引脚为浮空输入 */
    GPIO_InitStructure.GPIO_Pin = g_can_configs[instance].rx_pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(g_can_configs[instance].rx_port, &GPIO_InitStructure);
    
    /* 配置CAN外设 */
    CAN_InitStructure.CAN_Prescaler = g_can_configs[instance].prescaler;
    CAN_InitStructure.CAN_Mode = g_can_configs[instance].mode;
    CAN_InitStructure.CAN_SJW = g_can_configs[instance].sjw;
    CAN_InitStructure.CAN_BS1 = g_can_configs[instance].bs1;
    CAN_InitStructure.CAN_BS2 = g_can_configs[instance].bs2;
    CAN_InitStructure.CAN_TTCM = DISABLE;
    CAN_InitStructure.CAN_ABOM = ENABLE;
    CAN_InitStructure.CAN_AWUM = DISABLE;
    CAN_InitStructure.CAN_NART = DISABLE;
    CAN_InitStructure.CAN_RFLM = DISABLE;
    CAN_InitStructure.CAN_TXFP = DISABLE;
    
    /* 调用标准库的CAN_Init函数 */
    init_status = CAN_Init(g_can_configs[instance].can_periph, &CAN_InitStructure);
    if (init_status != CAN_InitStatus_Success)
    {
        return CAN_ERROR_INIT_FAILED;
    }
    
    /* 配置CAN过滤器（默认：接收所有消息） */
    CAN_FilterInitStructure.CAN_FilterNumber = 0;
    CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
    CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
    CAN_FilterInitStructure.CAN_FilterIdHigh = 0x0000;
    CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;
    CAN_FilterInitStructure.CAN_FilterMaskIdHigh = 0x0000;
    CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000;
    CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
    CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
    
    /* 对于CAN2，需要设置过滤器起始bank */
    if (g_can_configs[instance].can_periph == CAN2)
    {
        CAN_SlaveStartBank(14);
    }
    
    CAN_FilterInit(&CAN_FilterInitStructure);
    
    /* 标记为已初始化 */
    g_can_initialized[instance] = true;
    
    return CAN_OK;
}

/**
 * @brief CAN反初始化
 */
CAN_Status_t CAN_Deinit(CAN_Instance_t instance)
{
    uint32_t can_clock;
    
    /* 参数校验 */
    if (instance >= CAN_INSTANCE_MAX)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (!g_can_initialized[instance])
    {
        return CAN_OK;
    }
    
    /* 禁用CAN外设 */
    CAN_DeInit(g_can_configs[instance].can_periph);
    
    /* 获取CAN外设时钟 */
    can_clock = CAN_GetPeriphClock(g_can_configs[instance].can_periph);
    if (can_clock != 0)
    {
        /* 禁用CAN外设时钟 */
        RCC_APB1PeriphClockCmd(can_clock, DISABLE);
    }
    
    /* 标记为未初始化 */
    g_can_initialized[instance] = false;
    
    return CAN_OK;
}

/**
 * @brief CAN发送消息（阻塞式）
 */
CAN_Status_t CAN_Transmit(CAN_Instance_t instance, const CAN_Message_t *message, uint32_t timeout)
{
    CanTxMsg TxMessage;
    uint8_t mailbox;
    uint32_t start_tick;
    uint8_t transmit_status;
    
    /* 参数校验 */
    if (instance >= CAN_INSTANCE_MAX)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (message == NULL)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (message->dlc > 8)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (!g_can_initialized[instance])
    {
        return CAN_ERROR_NOT_INITIALIZED;
    }
    
    /* 使用默认超时时间 */
    if (timeout == 0)
    {
        timeout = CAN_DEFAULT_TIMEOUT_MS;
    }
    
    /* 填充发送消息结构体 */
    if (message->type == CAN_FRAME_STANDARD)
    {
        TxMessage.StdId = message->id;
        TxMessage.ExtId = 0x00;
        TxMessage.IDE = CAN_Id_Standard;
    }
    else
    {
        TxMessage.StdId = 0x00;
        TxMessage.ExtId = message->id;
        TxMessage.IDE = CAN_Id_Extended;
    }
    
    TxMessage.RTR = (message->rtr == CAN_RTR_REMOTE) ? CAN_RTR_Remote : CAN_RTR_Data;
    TxMessage.DLC = message->dlc;
    
    memcpy(TxMessage.Data, message->data, message->dlc);
    
    /* 发送消息 */
    mailbox = CAN_Transmit(g_can_configs[instance].can_periph, &TxMessage);
    if (mailbox == CAN_TxStatus_NoMailBox)
    {
        return CAN_ERROR_NO_MAILBOX;
    }
    
    /* 等待发送完成 */
    start_tick = Delay_GetTick();
    while (1)
    {
        transmit_status = CAN_TransmitStatus(g_can_configs[instance].can_periph, mailbox);
        if (transmit_status == CAN_TxStatus_Failed)
        {
            return CAN_ERROR_BUSY;
        }
        else if (transmit_status == CAN_TxStatus_Ok)
        {
            return CAN_OK;
        }
        
        /* 检查超时 */
        uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
        if (elapsed > timeout)
        {
            return CAN_ERROR_TIMEOUT;
        }
    }
}

/**
 * @brief CAN接收消息（阻塞式）
 */
CAN_Status_t CAN_Receive(CAN_Instance_t instance, CAN_Message_t *message, uint8_t fifo_number, uint32_t timeout)
{
    CanRxMsg RxMessage;
    uint32_t start_tick;
    uint8_t pending_count;
    
    /* 参数校验 */
    if (instance >= CAN_INSTANCE_MAX)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (message == NULL)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (fifo_number > 1)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (!g_can_initialized[instance])
    {
        return CAN_ERROR_NOT_INITIALIZED;
    }
    
    /* 使用默认超时时间 */
    if (timeout == 0)
    {
        timeout = CAN_DEFAULT_TIMEOUT_MS;
    }
    
    /* 等待消息 */
    start_tick = Delay_GetTick();
    while (1)
    {
        pending_count = CAN_MessagePending(g_can_configs[instance].can_periph, fifo_number);
        if (pending_count > 0)
        {
            break;
        }
        
        /* 检查超时 */
        uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
        if (elapsed > timeout)
        {
            return CAN_ERROR_TIMEOUT;
        }
    }
    
    /* 接收消息 */
    CAN_Receive(g_can_configs[instance].can_periph, fifo_number, &RxMessage);
    
    /* 解析消息 */
    if (RxMessage.IDE == CAN_Id_Standard)
    {
        message->id = RxMessage.StdId;
        message->type = CAN_FRAME_STANDARD;
    }
    else
    {
        message->id = RxMessage.ExtId;
        message->type = CAN_FRAME_EXTENDED;
    }
    
    message->rtr = (RxMessage.RTR == CAN_RTR_Remote) ? CAN_RTR_REMOTE : CAN_RTR_DATA;
    message->dlc = RxMessage.DLC;
    memcpy(message->data, RxMessage.Data, RxMessage.DLC);
    
    /* 释放FIFO */
    CAN_FIFORelease(g_can_configs[instance].can_periph, fifo_number);
    
    return CAN_OK;
}

/**
 * @brief 检查是否有消息待接收
 */
uint8_t CAN_GetPendingMessageCount(CAN_Instance_t instance, uint8_t fifo_number)
{
    if (instance >= CAN_INSTANCE_MAX)
    {
        return 0;
    }
    
    if (fifo_number > 1)
    {
        return 0;
    }
    
    if (!g_can_initialized[instance])
    {
        return 0;
    }
    
    return CAN_MessagePending(g_can_configs[instance].can_periph, fifo_number);
}

/**
 * @brief 配置CAN过滤器
 */
CAN_Status_t CAN_ConfigFilter(CAN_Instance_t instance, uint8_t filter_number,
                               uint32_t filter_id, uint32_t filter_mask,
                               CAN_FrameType_t filter_type, uint8_t fifo_number)
{
    CAN_FilterInitTypeDef CAN_FilterInitStructure;
    
    /* 参数校验 */
    if (instance >= CAN_INSTANCE_MAX)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (filter_number > 13)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (fifo_number > 1)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (!g_can_initialized[instance])
    {
        return CAN_ERROR_NOT_INITIALIZED;
    }
    
    /* 配置过滤器 */
    CAN_FilterInitStructure.CAN_FilterNumber = filter_number;
    CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
    CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
    
    if (filter_type == CAN_FRAME_STANDARD)
    {
        /* 标准帧：ID在低11位 */
        CAN_FilterInitStructure.CAN_FilterIdHigh = (filter_id << 5) >> 16;
        CAN_FilterInitStructure.CAN_FilterIdLow = ((filter_id << 5) & 0xFFFF) | CAN_Id_Standard;
        CAN_FilterInitStructure.CAN_FilterMaskIdHigh = (filter_mask << 5) >> 16;
        CAN_FilterInitStructure.CAN_FilterMaskIdLow = ((filter_mask << 5) & 0xFFFF) | CAN_Id_Standard;
    }
    else
    {
        /* 扩展帧：ID在29位 */
        CAN_FilterInitStructure.CAN_FilterIdHigh = filter_id >> 16;
        CAN_FilterInitStructure.CAN_FilterIdLow = (filter_id & 0xFFFF) | CAN_Id_Extended;
        CAN_FilterInitStructure.CAN_FilterMaskIdHigh = filter_mask >> 16;
        CAN_FilterInitStructure.CAN_FilterMaskIdLow = (filter_mask & 0xFFFF) | CAN_Id_Extended;
    }
    
    CAN_FilterInitStructure.CAN_FilterFIFOAssignment = (fifo_number == 0) ? CAN_Filter_FIFO0 : CAN_Filter_FIFO1;
    CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
    
    CAN_FilterInit(&CAN_FilterInitStructure);
    
    return CAN_OK;
}

/**
 * @brief 检查CAN是否已初始化
 */
uint8_t CAN_IsInitialized(CAN_Instance_t instance)
{
    if (instance >= CAN_INSTANCE_MAX)
    {
        return 0;
    }
    
    return g_can_initialized[instance] ? 1 : 0;
}

/**
 * @brief 获取CAN外设指针
 */
CAN_TypeDef* CAN_GetPeriph(CAN_Instance_t instance)
{
    if (instance >= CAN_INSTANCE_MAX)
    {
        return NULL;
    }
    
    if (!g_can_initialized[instance])
    {
        return NULL;
    }
    
    return g_can_configs[instance].can_periph;
}

/* ========== 中断模式功能实现 ========== */

/**
 * @brief 获取CAN中断类型对应的SPL库中断值
 */
static uint32_t CAN_GetITValue(CAN_IT_t it_type)
{
    switch (it_type)
    {
        case CAN_IT_TX:    return CAN_IT_TME;
        case CAN_IT_RX0:   return CAN_IT_FMP0;
        case CAN_IT_RX1:   return CAN_IT_FMP1;
        case CAN_IT_ERROR: return CAN_IT_ERR;
        case CAN_IT_EWG:   return CAN_IT_EWG;
        case CAN_IT_EPV:   return CAN_IT_EPV;
        case CAN_IT_BOF:   return CAN_IT_BOF;
        case CAN_IT_LEC:   return CAN_IT_LEC;
        case CAN_IT_FF0:   return CAN_IT_FF0;
        case CAN_IT_FF1:   return CAN_IT_FF1;
        case CAN_IT_FOV0:  return CAN_IT_FOV0;
        case CAN_IT_FOV1:  return CAN_IT_FOV1;
        default: return 0;
    }
}

/**
 * @brief 获取CAN中断向量
 */
static void CAN_GetIRQn(CAN_Instance_t instance, IRQn_Type *tx_irqn, IRQn_Type *rx0_irqn, IRQn_Type *rx1_irqn, IRQn_Type *sce_irqn)
{
    if (instance == CAN_INSTANCE_1)
    {
        *tx_irqn = USB_HP_CAN1_TX_IRQn;
        *rx0_irqn = USB_LP_CAN1_RX0_IRQn;
        *rx1_irqn = USB_LP_CAN1_RX0_IRQn;  /* CAN1 RX1也使用RX0中断 */
        *sce_irqn = USB_LP_CAN1_RX0_IRQn;   /* CAN1 SCE也使用RX0中断 */
    }
    else
    {
        *tx_irqn = USB_HP_CAN1_TX_IRQn;    /* CAN2 TX使用CAN1 TX中断 */
        *rx0_irqn = CAN2_RX0_IRQn;
        *rx1_irqn = CAN2_RX1_IRQn;
        *sce_irqn = CAN2_SCE_IRQn;
    }
}

/**
 * @brief 使能CAN中断
 */
CAN_Status_t CAN_EnableIT(CAN_Instance_t instance, CAN_IT_t it_type)
{
    CAN_TypeDef *can_periph;
    uint32_t it_value;
    IRQn_Type tx_irqn, rx0_irqn, rx1_irqn, sce_irqn;
    
    if (instance >= CAN_INSTANCE_MAX)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (!g_can_initialized[instance])
    {
        return CAN_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 12)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    can_periph = g_can_configs[instance].can_periph;
    it_value = CAN_GetITValue(it_type);
    if (it_value == 0)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    /* 使能CAN中断 */
    CAN_ITConfig(can_periph, it_value, ENABLE);
    
    /* 使能NVIC中断 */
    CAN_GetIRQn(instance, &tx_irqn, &rx0_irqn, &rx1_irqn, &sce_irqn);
    
    if (it_type == CAN_IT_TX)
    {
        NVIC_EnableIRQ(tx_irqn);
    }
    else if (it_type == CAN_IT_RX0 || it_type == CAN_IT_FF0 || it_type == CAN_IT_FOV0)
    {
        NVIC_EnableIRQ(rx0_irqn);
    }
    else if (it_type == CAN_IT_RX1 || it_type == CAN_IT_FF1 || it_type == CAN_IT_FOV1)
    {
        NVIC_EnableIRQ(rx1_irqn);
    }
    else if (it_type == CAN_IT_ERROR || it_type == CAN_IT_EWG || it_type == CAN_IT_EPV || 
             it_type == CAN_IT_BOF || it_type == CAN_IT_LEC)
    {
        NVIC_EnableIRQ(sce_irqn);
    }
    
    return CAN_OK;
}

/**
 * @brief 禁用CAN中断
 */
CAN_Status_t CAN_DisableIT(CAN_Instance_t instance, CAN_IT_t it_type)
{
    CAN_TypeDef *can_periph;
    uint32_t it_value;
    
    if (instance >= CAN_INSTANCE_MAX)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (!g_can_initialized[instance])
    {
        return CAN_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 12)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    can_periph = g_can_configs[instance].can_periph;
    it_value = CAN_GetITValue(it_type);
    if (it_value == 0)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    /* 禁用CAN中断 */
    CAN_ITConfig(can_periph, it_value, DISABLE);
    
    return CAN_OK;
}

/**
 * @brief 设置CAN中断回调函数
 */
CAN_Status_t CAN_SetITCallback(CAN_Instance_t instance, CAN_IT_t it_type,
                                CAN_IT_Callback_t callback, void *user_data)
{
    if (instance >= CAN_INSTANCE_MAX)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (!g_can_initialized[instance])
    {
        return CAN_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 12)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    g_can_it_callbacks[instance][it_type] = callback;
    g_can_it_user_data[instance][it_type] = user_data;
    
    return CAN_OK;
}

/**
 * @brief CAN非阻塞发送（中断模式）
 */
CAN_Status_t CAN_TransmitIT(CAN_Instance_t instance, const CAN_Message_t *message)
{
    CanTxMsg TxMessage;
    uint8_t mailbox;
    
    if (instance >= CAN_INSTANCE_MAX)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (!g_can_initialized[instance])
    {
        return CAN_ERROR_NOT_INITIALIZED;
    }
    
    if (message == NULL)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否有可用邮箱 */
    mailbox = CAN_TransmitStatus(g_can_configs[instance].can_periph, CAN_TxStatus_NoMailBox);
    if (mailbox == CAN_TxStatus_NoMailBox)
    {
        return CAN_ERROR_NO_MAILBOX;
    }
    
    /* 填充发送消息结构 */
    TxMessage.StdId = (message->type == CAN_FRAME_STANDARD) ? message->id : 0;
    TxMessage.ExtId = (message->type == CAN_FRAME_EXTENDED) ? message->id : 0;
    TxMessage.RTR = (message->rtr == CAN_RTR_REMOTE) ? CAN_RTR_Remote : CAN_RTR_Data;
    TxMessage.IDE = (message->type == CAN_FRAME_EXTENDED) ? CAN_ID_EXT : CAN_ID_STD;
    TxMessage.DLC = message->dlc;
    memcpy(TxMessage.Data, message->data, message->dlc);
    
    /* 发送消息 */
    CAN_Transmit(g_can_configs[instance].can_periph, &TxMessage);
    
    return CAN_OK;
}

/**
 * @brief CAN中断服务函数
 */
void CAN_IRQHandler(CAN_Instance_t instance, uint8_t fifo)
{
    CAN_TypeDef *can_periph;
    CanRxMsg RxMessage;
    
    if (instance >= CAN_INSTANCE_MAX || !g_can_initialized[instance])
    {
        return;
    }
    
    can_periph = g_can_configs[instance].can_periph;
    
    /* 处理发送完成中断 */
    if (CAN_GetITStatus(can_periph, CAN_IT_TME) != RESET)
    {
        CAN_ClearITPendingBit(can_periph, CAN_IT_TME);
        
        /* 调用回调函数 */
        if (g_can_it_callbacks[instance][CAN_IT_TX] != NULL)
        {
            g_can_it_callbacks[instance][CAN_IT_TX](instance, CAN_IT_TX, g_can_it_user_data[instance][CAN_IT_TX]);
        }
    }
    
    /* 处理FIFO0接收中断 */
    if (CAN_GetITStatus(can_periph, CAN_IT_FMP0) != RESET)
    {
        CAN_Receive(can_periph, CAN_FIFO0, &RxMessage);
        
        /* 保存接收到的消息 */
        g_can_rx_buffer[instance][0].id = (RxMessage.IDE == CAN_ID_STD) ? RxMessage.StdId : RxMessage.ExtId;
        g_can_rx_buffer[instance][0].type = (RxMessage.IDE == CAN_ID_STD) ? CAN_FRAME_STANDARD : CAN_FRAME_EXTENDED;
        g_can_rx_buffer[instance][0].rtr = (RxMessage.RTR == CAN_RTR_Remote) ? CAN_RTR_REMOTE : CAN_RTR_DATA;
        g_can_rx_buffer[instance][0].dlc = RxMessage.DLC;
        memcpy(g_can_rx_buffer[instance][0].data, RxMessage.Data, RxMessage.DLC);
        g_can_rx_pending[instance][0] = true;
        
        CAN_ClearITPendingBit(can_periph, CAN_IT_FMP0);
        
        /* 调用回调函数 */
        if (g_can_it_callbacks[instance][CAN_IT_RX0] != NULL)
        {
            g_can_it_callbacks[instance][CAN_IT_RX0](instance, CAN_IT_RX0, g_can_it_user_data[instance][CAN_IT_RX0]);
        }
    }
    
    /* 处理FIFO1接收中断 */
    if (CAN_GetITStatus(can_periph, CAN_IT_FMP1) != RESET)
    {
        CAN_Receive(can_periph, CAN_FIFO1, &RxMessage);
        
        /* 保存接收到的消息 */
        g_can_rx_buffer[instance][1].id = (RxMessage.IDE == CAN_ID_STD) ? RxMessage.StdId : RxMessage.ExtId;
        g_can_rx_buffer[instance][1].type = (RxMessage.IDE == CAN_ID_STD) ? CAN_FRAME_STANDARD : CAN_FRAME_EXTENDED;
        g_can_rx_buffer[instance][1].rtr = (RxMessage.RTR == CAN_RTR_Remote) ? CAN_RTR_REMOTE : CAN_RTR_DATA;
        g_can_rx_buffer[instance][1].dlc = RxMessage.DLC;
        memcpy(g_can_rx_buffer[instance][1].data, RxMessage.Data, RxMessage.DLC);
        g_can_rx_pending[instance][1] = true;
        
        CAN_ClearITPendingBit(can_periph, CAN_IT_FMP1);
        
        /* 调用回调函数 */
        if (g_can_it_callbacks[instance][CAN_IT_RX1] != NULL)
        {
            g_can_it_callbacks[instance][CAN_IT_RX1](instance, CAN_IT_RX1, g_can_it_user_data[instance][CAN_IT_RX1]);
        }
    }
    
    /* 处理FIFO0满中断 */
    if (CAN_GetITStatus(can_periph, CAN_IT_FF0) != RESET)
    {
        CAN_ClearITPendingBit(can_periph, CAN_IT_FF0);
        if (g_can_it_callbacks[instance][CAN_IT_FF0] != NULL)
        {
            g_can_it_callbacks[instance][CAN_IT_FF0](instance, CAN_IT_FF0, g_can_it_user_data[instance][CAN_IT_FF0]);
        }
    }
    
    /* 处理FIFO1满中断 */
    if (CAN_GetITStatus(can_periph, CAN_IT_FF1) != RESET)
    {
        CAN_ClearITPendingBit(can_periph, CAN_IT_FF1);
        if (g_can_it_callbacks[instance][CAN_IT_FF1] != NULL)
        {
            g_can_it_callbacks[instance][CAN_IT_FF1](instance, CAN_IT_FF1, g_can_it_user_data[instance][CAN_IT_FF1]);
        }
    }
    
    /* 处理FIFO0溢出中断 */
    if (CAN_GetITStatus(can_periph, CAN_IT_FOV0) != RESET)
    {
        CAN_ClearITPendingBit(can_periph, CAN_IT_FOV0);
        if (g_can_it_callbacks[instance][CAN_IT_FOV0] != NULL)
        {
            g_can_it_callbacks[instance][CAN_IT_FOV0](instance, CAN_IT_FOV0, g_can_it_user_data[instance][CAN_IT_FOV0]);
        }
    }
    
    /* 处理FIFO1溢出中断 */
    if (CAN_GetITStatus(can_periph, CAN_IT_FOV1) != RESET)
    {
        CAN_ClearITPendingBit(can_periph, CAN_IT_FOV1);
        if (g_can_it_callbacks[instance][CAN_IT_FOV1] != NULL)
        {
            g_can_it_callbacks[instance][CAN_IT_FOV1](instance, CAN_IT_FOV1, g_can_it_user_data[instance][CAN_IT_FOV1]);
        }
    }
    
    /* 处理错误警告中断（EWG） */
    if (CAN_GetITStatus(can_periph, CAN_IT_EWG) != RESET)
    {
        CAN_ClearITPendingBit(can_periph, CAN_IT_EWG);
        if (g_can_it_callbacks[instance][CAN_IT_EWG] != NULL)
        {
            g_can_it_callbacks[instance][CAN_IT_EWG](instance, CAN_IT_EWG, g_can_it_user_data[instance][CAN_IT_EWG]);
        }
        /* 同时调用通用错误回调 */
        if (g_can_it_callbacks[instance][CAN_IT_ERROR] != NULL)
        {
            g_can_it_callbacks[instance][CAN_IT_ERROR](instance, CAN_IT_ERROR, g_can_it_user_data[instance][CAN_IT_ERROR]);
        }
    }
    
    /* 处理错误被动中断（EPV） */
    if (CAN_GetITStatus(can_periph, CAN_IT_EPV) != RESET)
    {
        CAN_ClearITPendingBit(can_periph, CAN_IT_EPV);
        if (g_can_it_callbacks[instance][CAN_IT_EPV] != NULL)
        {
            g_can_it_callbacks[instance][CAN_IT_EPV](instance, CAN_IT_EPV, g_can_it_user_data[instance][CAN_IT_EPV]);
        }
        /* 同时调用通用错误回调 */
        if (g_can_it_callbacks[instance][CAN_IT_ERROR] != NULL)
        {
            g_can_it_callbacks[instance][CAN_IT_ERROR](instance, CAN_IT_ERROR, g_can_it_user_data[instance][CAN_IT_ERROR]);
        }
    }
    
    /* 处理总线关闭中断（BOF） */
    if (CAN_GetITStatus(can_periph, CAN_IT_BOF) != RESET)
    {
        CAN_ClearITPendingBit(can_periph, CAN_IT_BOF);
        if (g_can_it_callbacks[instance][CAN_IT_BOF] != NULL)
        {
            g_can_it_callbacks[instance][CAN_IT_BOF](instance, CAN_IT_BOF, g_can_it_user_data[instance][CAN_IT_BOF]);
        }
        /* 同时调用通用错误回调 */
        if (g_can_it_callbacks[instance][CAN_IT_ERROR] != NULL)
        {
            g_can_it_callbacks[instance][CAN_IT_ERROR](instance, CAN_IT_ERROR, g_can_it_user_data[instance][CAN_IT_ERROR]);
        }
    }
    
    /* 处理最后错误码中断（LEC） */
    if (CAN_GetITStatus(can_periph, CAN_IT_LEC) != RESET)
    {
        CAN_ClearITPendingBit(can_periph, CAN_IT_LEC);
        if (g_can_it_callbacks[instance][CAN_IT_LEC] != NULL)
        {
            g_can_it_callbacks[instance][CAN_IT_LEC](instance, CAN_IT_LEC, g_can_it_user_data[instance][CAN_IT_LEC]);
        }
        /* 同时调用通用错误回调 */
        if (g_can_it_callbacks[instance][CAN_IT_ERROR] != NULL)
        {
            g_can_it_callbacks[instance][CAN_IT_ERROR](instance, CAN_IT_ERROR, g_can_it_user_data[instance][CAN_IT_ERROR]);
        }
    }
    
    /* 处理通用错误中断（ERR） */
    if (CAN_GetITStatus(can_periph, CAN_IT_ERR) != RESET)
    {
        CAN_ClearITPendingBit(can_periph, CAN_IT_ERR);
        if (g_can_it_callbacks[instance][CAN_IT_ERROR] != NULL)
        {
            g_can_it_callbacks[instance][CAN_IT_ERROR](instance, CAN_IT_ERROR, g_can_it_user_data[instance][CAN_IT_ERROR]);
        }
    }
}

/* CAN中断服务程序入口 */
void USB_HP_CAN1_TX_IRQHandler(void)
{
    CAN_IRQHandler(CAN_INSTANCE_1, 0);
    CAN_IRQHandler(CAN_INSTANCE_2, 0);  /* CAN2 TX也使用此中断 */
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
    CAN_IRQHandler(CAN_INSTANCE_1, 0);
}

void CAN2_RX0_IRQHandler(void)
{
    CAN_IRQHandler(CAN_INSTANCE_2, 0);
}

void CAN2_RX1_IRQHandler(void)
{
    CAN_IRQHandler(CAN_INSTANCE_2, 1);
}

void CAN2_SCE_IRQHandler(void)
{
    CAN_IRQHandler(CAN_INSTANCE_2, 0);
}

/**
 * @brief 获取CAN最后错误码
 */
uint8_t CAN_GetInstanceLastErrorCode(CAN_Instance_t instance)
{
    CAN_TypeDef *can_periph;
    
    if (instance >= CAN_INSTANCE_MAX)
    {
        return CAN_LastErrorCode_NoError;
    }
    
    if (!g_can_initialized[instance])
    {
        return CAN_LastErrorCode_NoError;
    }
    
    can_periph = g_can_configs[instance].can_periph;
    
    return CAN_GetLastErrorCode(can_periph);
}

/**
 * @brief CAN总线恢复（从总线关闭状态恢复）
 */
CAN_Status_t CAN_Recovery(CAN_Instance_t instance)
{
    CAN_TypeDef *can_periph;
    CAN_InitTypeDef CAN_InitStructure;
    CAN_FilterInitTypeDef CAN_FilterInitStructure;
    uint8_t init_status;
    uint32_t timeout;
    
    if (instance >= CAN_INSTANCE_MAX)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (!g_can_initialized[instance])
    {
        return CAN_ERROR_NOT_INITIALIZED;
    }
    
    can_periph = g_can_configs[instance].can_periph;
    
    /* 进入初始化模式 */
    can_periph->MCR |= CAN_MCR_INRQ;
    
    /* 等待进入初始化模式 */
    timeout = 0xFFFF;
    while (((can_periph->MSR & CAN_MSR_INAK) != CAN_MSR_INAK) && (timeout != 0))
    {
        timeout--;
    }
    
    if ((can_periph->MSR & CAN_MSR_INAK) != CAN_MSR_INAK)
    {
        return CAN_ERROR_INIT_FAILED;
    }
    
    /* 重新配置CAN外设 */
    CAN_InitStructure.CAN_Prescaler = g_can_configs[instance].prescaler;
    CAN_InitStructure.CAN_Mode = g_can_configs[instance].mode;
    CAN_InitStructure.CAN_SJW = g_can_configs[instance].sjw;
    CAN_InitStructure.CAN_BS1 = g_can_configs[instance].bs1;
    CAN_InitStructure.CAN_BS2 = g_can_configs[instance].bs2;
    CAN_InitStructure.CAN_TTCM = DISABLE;
    CAN_InitStructure.CAN_ABOM = ENABLE;  /* 使能自动总线关闭管理 */
    CAN_InitStructure.CAN_AWUM = DISABLE;
    CAN_InitStructure.CAN_NART = DISABLE;
    CAN_InitStructure.CAN_RFLM = DISABLE;
    CAN_InitStructure.CAN_TXFP = DISABLE;
    
    /* 调用标准库的CAN_Init函数 */
    init_status = CAN_Init(can_periph, &CAN_InitStructure);
    if (init_status != CAN_InitStatus_Success)
    {
        return CAN_ERROR_INIT_FAILED;
    }
    
    /* 重新配置CAN过滤器（默认：接收所有消息） */
    CAN_FilterInitStructure.CAN_FilterNumber = 0;
    CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
    CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
    CAN_FilterInitStructure.CAN_FilterIdHigh = 0x0000;
    CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;
    CAN_FilterInitStructure.CAN_FilterMaskIdHigh = 0x0000;
    CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000;
    CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
    CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
    
    /* 对于CAN2，需要设置过滤器起始bank */
    if (can_periph == CAN2)
    {
        CAN_SlaveStartBank(14);
    }
    
    CAN_FilterInit(&CAN_FilterInitStructure);
    
    return CAN_OK;
}

/* ========== CAN测试模式功能实现 ========== */

/**
 * @brief 获取CAN模式对应的SPL库模式值
 */
static uint8_t CAN_GetModeValue(CAN_Mode_t mode)
{
    switch (mode)
    {
        case CAN_MODE_NORMAL: return CAN_Mode_Normal;
        case CAN_MODE_LOOPBACK: return CAN_Mode_LoopBack;
        case CAN_MODE_SILENT: return CAN_Mode_Silent;
        case CAN_MODE_SILENT_LOOPBACK: return CAN_Mode_Silent_LoopBack;
        default: return CAN_Mode_Normal;
    }
}

/**
 * @brief 配置CAN工作模式
 */
CAN_Status_t CAN_SetMode(CAN_Instance_t instance, CAN_Mode_t mode)
{
    CAN_TypeDef *can_periph;
    CAN_InitTypeDef CAN_InitStructure;
    CAN_FilterInitTypeDef CAN_FilterInitStructure;
    uint8_t init_status;
    uint8_t can_mode_value;
    
    if (instance >= CAN_INSTANCE_MAX)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (!g_can_initialized[instance])
    {
        return CAN_ERROR_NOT_INITIALIZED;
    }
    
    if (mode >= 4)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    can_periph = g_can_configs[instance].can_periph;
    can_mode_value = CAN_GetModeValue(mode);
    
    /* 进入初始化模式 */
    can_periph->MCR |= CAN_MCR_INRQ;
    uint32_t timeout = Delay_GetTick();
    while ((can_periph->MSR & CAN_MSR_INAK) == 0)
    {
        if (Delay_GetElapsed(Delay_GetTick(), timeout) > 1000)
        {
            return CAN_ERROR_TIMEOUT;
        }
    }
    
    /* 读取当前配置 */
    CAN_InitStructure.CAN_TTCM = DISABLE;
    CAN_InitStructure.CAN_ABOM = ENABLE;
    CAN_InitStructure.CAN_AWUM = DISABLE;
    CAN_InitStructure.CAN_NART = DISABLE;
    CAN_InitStructure.CAN_RFLM = DISABLE;
    CAN_InitStructure.CAN_TXFP = DISABLE;
    CAN_InitStructure.CAN_Mode = can_mode_value;
    CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;
    CAN_InitStructure.CAN_BS1 = CAN_BS1_8tq;
    CAN_InitStructure.CAN_BS2 = CAN_BS2_7tq;
    CAN_InitStructure.CAN_Prescaler = 5;
    
    /* 重新初始化CAN */
    init_status = CAN_Init(can_periph, &CAN_InitStructure);
    if (init_status != CAN_InitStatus_Success)
    {
        return CAN_ERROR_INIT_FAILED;
    }
    
    /* 重新配置过滤器 */
    CAN_FilterInitStructure.CAN_FilterNumber = 0;
    CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
    CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
    CAN_FilterInitStructure.CAN_FilterIdHigh = 0x0000;
    CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;
    CAN_FilterInitStructure.CAN_FilterMaskIdHigh = 0x0000;
    CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000;
    CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
    CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
    CAN_FilterInit(&CAN_FilterInitStructure);
    
    return CAN_OK;
}

/**
 * @brief 获取CAN当前工作模式
 */
CAN_Status_t CAN_GetMode(CAN_Instance_t instance, CAN_Mode_t *mode)
{
    CAN_TypeDef *can_periph;
    uint8_t can_mode_reg;
    
    if (instance >= CAN_INSTANCE_MAX)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (!g_can_initialized[instance])
    {
        return CAN_ERROR_NOT_INITIALIZED;
    }
    
    if (mode == NULL)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    can_periph = g_can_configs[instance].can_periph;
    
    /* 读取CAN模式寄存器（CAN_BTR寄存器的MODE位） */
    can_mode_reg = (can_periph->BTR & CAN_BTR_MODE) >> 30;
    
    /* 将寄存器值转换为枚举值 */
    switch (can_mode_reg)
    {
        case 0: *mode = CAN_MODE_NORMAL; break;
        case 1: *mode = CAN_MODE_LOOPBACK; break;
        case 2: *mode = CAN_MODE_SILENT; break;
        case 3: *mode = CAN_MODE_SILENT_LOOPBACK; break;
        default: *mode = CAN_MODE_NORMAL; break;
    }
    
    return CAN_OK;
}

/**
 * @brief 请求CAN操作模式
 */
CAN_Status_t CAN_RequestOperatingMode(CAN_Instance_t instance, CAN_OperatingMode_t op_mode)
{
    CAN_TypeDef *can_periph;
    uint8_t can_op_mode;
    uint8_t result;
    
    if (instance >= CAN_INSTANCE_MAX)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (!g_can_initialized[instance])
    {
        return CAN_ERROR_NOT_INITIALIZED;
    }
    
    if (op_mode >= 3)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    can_periph = g_can_configs[instance].can_periph;
    
    /* 转换操作模式枚举值 */
    switch (op_mode)
    {
        case CAN_OP_MODE_NORMAL: can_op_mode = CAN_OperatingMode_Normal; break;
        case CAN_OP_MODE_SLEEP: can_op_mode = CAN_OperatingMode_Sleep; break;
        case CAN_OP_MODE_INIT: can_op_mode = CAN_OperatingMode_Initialization; break;
        default: return CAN_ERROR_INVALID_PARAM;
    }
    
    /* 请求操作模式 */
    result = CAN_OperatingModeRequest(can_periph, can_op_mode);
    if (result == CAN_ModeStatus_Failed)
    {
        return CAN_ERROR_INIT_FAILED;
    }
    
    return CAN_OK;
}

/**
 * @brief CAN进入睡眠模式
 */
CAN_Status_t CAN_Sleep(CAN_Instance_t instance)
{
    CAN_TypeDef *can_periph;
    uint8_t result;
    
    if (instance >= CAN_INSTANCE_MAX)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (!g_can_initialized[instance])
    {
        return CAN_ERROR_NOT_INITIALIZED;
    }
    
    can_periph = g_can_configs[instance].can_periph;
    
    /* 进入睡眠模式（直接调用SPL库函数） */
    result = CAN_Sleep(can_periph);
    if (result == CAN_Sleep_Failed)
    {
        return CAN_ERROR_INIT_FAILED;
    }
    
    return CAN_OK;
}

/**
 * @brief CAN从睡眠模式唤醒
 */
CAN_Status_t CAN_WakeUp(CAN_Instance_t instance)
{
    CAN_TypeDef *can_periph;
    uint8_t result;
    
    if (instance >= CAN_INSTANCE_MAX)
    {
        return CAN_ERROR_INVALID_PARAM;
    }
    
    if (!g_can_initialized[instance])
    {
        return CAN_ERROR_NOT_INITIALIZED;
    }
    
    can_periph = g_can_configs[instance].can_periph;
    
    /* 从睡眠模式唤醒（直接调用SPL库函数） */
    result = CAN_WakeUp(can_periph);
    if (result == CAN_WakeUp_Failed)
    {
        return CAN_ERROR_INIT_FAILED;
    }
    
    return CAN_OK;
}

#endif /* CONFIG_MODULE_CAN_ENABLED */

