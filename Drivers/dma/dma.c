/**
 * @file dma.c
 * @brief DMA驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的DMA驱动，支持DMA1/2所有通道，外设到内存、内存到外设、内存到内存传输
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_DMA_ENABLED

/* Include our header */
#include "dma.h"

#include "delay.h"
#include "nvic.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_dma.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 从board.h加载配置 */
static DMA_Config_t g_dma_configs[DMA_CHANNEL_MAX] = DMA_CONFIGS;

/* 初始化标志 */
static bool g_dma_initialized[DMA_CHANNEL_MAX] = {false};

/* 传输完成回调函数 */
static DMA_TransferCompleteCallback_t g_dma_callbacks[DMA_CHANNEL_MAX] = {NULL};
static void *g_dma_user_data[DMA_CHANNEL_MAX] = {NULL};

/* 中断回调函数数组（支持3种中断类型：TC、HT、TE） */
static DMA_IT_Callback_t g_dma_it_callbacks[DMA_CHANNEL_MAX][3] = {NULL};
static void *g_dma_it_user_data[DMA_CHANNEL_MAX][3] = {NULL};

/* 默认超时时间（毫秒） */
#define DMA_DEFAULT_TIMEOUT_MS  5000

/**
 * @brief 获取DMA通道外设指针
 * @param[in] channel DMA通道索引
 * @return DMA_Channel_TypeDef* DMA通道指针，失败返回NULL
 */
static DMA_Channel_TypeDef* DMA_GetChannelPeriph(DMA_Channel_t channel)
{
    switch (channel)
    {
        case DMA_CHANNEL_1_1: return DMA1_Channel1;
        case DMA_CHANNEL_1_2: return DMA1_Channel2;
        case DMA_CHANNEL_1_3: return DMA1_Channel3;
        case DMA_CHANNEL_1_4: return DMA1_Channel4;
        case DMA_CHANNEL_1_5: return DMA1_Channel5;
        case DMA_CHANNEL_1_6: return DMA1_Channel6;
        case DMA_CHANNEL_1_7: return DMA1_Channel7;
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
        case DMA_CHANNEL_2_1: return DMA2_Channel1;
        case DMA_CHANNEL_2_2: return DMA2_Channel2;
        case DMA_CHANNEL_2_3: return DMA2_Channel3;
        case DMA_CHANNEL_2_4: return DMA2_Channel4;
        case DMA_CHANNEL_2_5: return DMA2_Channel5;
#endif
        default: return NULL;
    }
}

/**
 * @brief 获取DMA外设时钟
 * @param[in] channel DMA通道索引
 * @return uint32_t 时钟使能值，失败返回0
 */
static uint32_t DMA_GetPeriphClock(DMA_Channel_t channel)
{
    if (channel <= DMA_CHANNEL_1_7)
    {
        return RCC_AHBPeriph_DMA1;
    }
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    else if (channel <= DMA_CHANNEL_2_5)
    {
        return RCC_AHBPeriph_DMA2;
    }
#endif
    return 0;
}

/**
 * @brief DMA初始化
 */
DMA_Status_t DMA_HW_Init(DMA_Channel_t channel)
{
    DMA_InitTypeDef DMA_InitStructure;
    uint32_t dma_clock;
    DMA_Channel_TypeDef *dma_channel;
    
    /* 参数校验 */
    if (channel >= DMA_CHANNEL_MAX)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    if (!g_dma_configs[channel].enabled)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    if (g_dma_initialized[channel])
    {
        return DMA_OK;
    }
    
    /* 获取DMA通道外设指针 */
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL)
    {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    /* 获取DMA外设时钟 */
    dma_clock = DMA_GetPeriphClock(channel);
    if (dma_clock == 0)
    {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    /* 使能DMA外设时钟 */
    RCC_AHBPeriphClockCmd(dma_clock, ENABLE);
    
    /* 配置DMA通道 */
    DMA_InitStructure.DMA_PeripheralBaseAddr = g_dma_configs[channel].peripheral_addr;
    DMA_InitStructure.DMA_MemoryBaseAddr = 0;  /* 稍后配置 */
    DMA_InitStructure.DMA_DIR = g_dma_configs[channel].direction;
    DMA_InitStructure.DMA_BufferSize = 0;  /* 稍后配置 */
    DMA_InitStructure.DMA_PeripheralInc = g_dma_configs[channel].peripheral_inc;
    DMA_InitStructure.DMA_MemoryInc = g_dma_configs[channel].memory_inc;
    DMA_InitStructure.DMA_PeripheralDataSize = g_dma_configs[channel].data_size;
    DMA_InitStructure.DMA_MemoryDataSize = g_dma_configs[channel].data_size;
    DMA_InitStructure.DMA_Mode = g_dma_configs[channel].mode;
    DMA_InitStructure.DMA_Priority = g_dma_configs[channel].priority;
    DMA_InitStructure.DMA_M2M = (g_dma_configs[channel].direction == DMA_DIR_MEMORY_TO_MEMORY) ? DMA_M2M_Enable : DMA_M2M_Disable;
    
    DMA_Init(dma_channel, &DMA_InitStructure);  /* 调用标准库函数 */
    
    /* 标记为已初始化 */
    g_dma_initialized[channel] = true;
    
    return DMA_OK;
}

/**
 * @brief DMA反初始化
 */
DMA_Status_t DMA_Deinit(DMA_Channel_t channel)
{
    DMA_Channel_TypeDef *dma_channel;
    uint32_t dma_clock;
    
    /* 参数校验 */
    if (channel >= DMA_CHANNEL_MAX)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    if (!g_dma_initialized[channel])
    {
        return DMA_OK;
    }
    
    /* 停止DMA传输 */
    DMA_Stop(channel);
    
    /* 禁用DMA通道 */
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel != NULL)
    {
        DMA_Cmd(dma_channel, DISABLE);
    }
    
    /* 获取DMA外设时钟 */
    dma_clock = DMA_GetPeriphClock(channel);
    if (dma_clock != 0)
    {
        /* 禁用DMA外设时钟 */
        RCC_AHBPeriphClockCmd(dma_clock, DISABLE);
    }
    
    /* 清除回调函数 */
    g_dma_callbacks[channel] = NULL;
    g_dma_user_data[channel] = NULL;
    
    /* 标记为未初始化 */
    g_dma_initialized[channel] = false;
    
    return DMA_OK;
}

/**
 * @brief 配置DMA传输（外设到内存或内存到外设）
 */
DMA_Status_t DMA_ConfigTransfer(DMA_Channel_t channel, uint32_t peripheral_addr,
                                uint32_t memory_addr, uint16_t data_size,
                                DMA_Direction_t direction, uint8_t data_width)
{
    DMA_InitTypeDef DMA_InitStructure;
    DMA_Channel_TypeDef *dma_channel;
    uint32_t peripheral_data_size, memory_data_size;
    
    /* 参数校验 */
    if (channel >= DMA_CHANNEL_MAX)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    if (memory_addr == 0 || data_size == 0)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    if (direction == DMA_DIR_MEMORY_TO_MEMORY)
    {
        return DMA_ERROR_INVALID_PARAM;  /* 使用DMA_ConfigMemoryToMemory */
    }
    
    if (!g_dma_initialized[channel])
    {
        return DMA_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取DMA通道外设指针 */
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL)
    {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    /* 停止DMA传输 */
    DMA_Cmd(dma_channel, DISABLE);
    
    /* 根据数据宽度设置数据大小 */
    switch (data_width)
    {
        case 1:  /* 字节 */
            peripheral_data_size = DMA_PeripheralDataSize_Byte;
            memory_data_size = DMA_MemoryDataSize_Byte;
            break;
        case 2:  /* 半字 */
            peripheral_data_size = DMA_PeripheralDataSize_HalfWord;
            memory_data_size = DMA_MemoryDataSize_HalfWord;
            break;
        case 4:  /* 字 */
            peripheral_data_size = DMA_PeripheralDataSize_Word;
            memory_data_size = DMA_MemoryDataSize_Word;
            /* 数据大小需要按字对齐 */
            data_size = (data_size + 3) / 4;
            break;
        default:
            return DMA_ERROR_INVALID_PARAM;
    }
    
    /* 配置DMA通道 */
    DMA_InitStructure.DMA_PeripheralBaseAddr = peripheral_addr;
    DMA_InitStructure.DMA_MemoryBaseAddr = memory_addr;
    if (direction == DMA_DIR_PERIPHERAL_TO_MEMORY)
    {
        DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    }
    else if (direction == DMA_DIR_MEMORY_TO_PERIPHERAL)
    {
        DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    }
    else
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    DMA_InitStructure.DMA_BufferSize = data_size;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = peripheral_data_size;
    DMA_InitStructure.DMA_MemoryDataSize = memory_data_size;
    DMA_InitStructure.DMA_Mode = g_dma_configs[channel].mode;
    DMA_InitStructure.DMA_Priority = g_dma_configs[channel].priority;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    
    DMA_Init(dma_channel, &DMA_InitStructure);  /* 调用标准库函数 */
    
    return DMA_OK;
}

/**
 * @brief 配置DMA内存到内存传输
 */
DMA_Status_t DMA_ConfigMemoryToMemory(DMA_Channel_t channel, uint32_t src_addr,
                                      uint32_t dst_addr, uint16_t data_size, uint8_t data_width)
{
    DMA_InitTypeDef DMA_InitStructure;
    DMA_Channel_TypeDef *dma_channel;
    uint32_t data_size_type;
    
    /* 参数校验 */
    if (channel >= DMA_CHANNEL_MAX)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    if (src_addr == 0 || dst_addr == 0 || data_size == 0)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    if (!g_dma_initialized[channel])
    {
        return DMA_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取DMA通道外设指针 */
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL)
    {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    /* 停止DMA传输 */
    DMA_Cmd(dma_channel, DISABLE);
    
    /* 根据数据宽度设置数据大小 */
    switch (data_width)
    {
        case 1:  /* 字节 */
            data_size_type = DMA_PeripheralDataSize_Byte;
            break;
        case 2:  /* 半字 */
            data_size_type = DMA_PeripheralDataSize_HalfWord;
            break;
        case 4:  /* 字 */
            data_size_type = DMA_PeripheralDataSize_Word;
            /* 数据大小需要按字对齐 */
            data_size = (data_size + 3) / 4;
            break;
        default:
            return DMA_ERROR_INVALID_PARAM;
    }
    
    /* 配置DMA通道 */
    DMA_InitStructure.DMA_PeripheralBaseAddr = src_addr;
    DMA_InitStructure.DMA_MemoryBaseAddr = dst_addr;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = data_size;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Enable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = data_size_type;
    DMA_InitStructure.DMA_MemoryDataSize = data_size_type;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;  /* 内存到内存不支持循环模式 */
    DMA_InitStructure.DMA_Priority = g_dma_configs[channel].priority;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Enable;
    
    DMA_Init(dma_channel, &DMA_InitStructure);  /* 调用标准库函数 */
    
    return DMA_OK;
}

/**
 * @brief 启动DMA传输
 */
DMA_Status_t DMA_Start(DMA_Channel_t channel)
{
    DMA_Channel_TypeDef *dma_channel;
    
    /* 参数校验 */
    if (channel >= DMA_CHANNEL_MAX)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    if (!g_dma_initialized[channel])
    {
        return DMA_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取DMA通道外设指针 */
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL)
    {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    /* 清除传输完成标志 */
    if (channel <= DMA_CHANNEL_1_7)
    {
        DMA1->IFCR |= (DMA_IFCR_CTCIF1 << (channel * 4));
    }
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    else if (channel <= DMA_CHANNEL_2_5)
    {
        DMA2->IFCR |= (DMA_IFCR_CTCIF1 << ((channel - DMA_CHANNEL_2_1) * 4));
    }
#endif
    
    /* 使能DMA通道 */
    DMA_Cmd(dma_channel, ENABLE);
    
    return DMA_OK;
}

/**
 * @brief 停止DMA传输
 */
DMA_Status_t DMA_Stop(DMA_Channel_t channel)
{
    DMA_Channel_TypeDef *dma_channel;
    
    /* 参数校验 */
    if (channel >= DMA_CHANNEL_MAX)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    if (!g_dma_initialized[channel])
    {
        return DMA_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取DMA通道外设指针 */
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL)
    {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    /* 禁用DMA通道 */
    DMA_Cmd(dma_channel, DISABLE);
    
    return DMA_OK;
}

/**
 * @brief 等待DMA传输完成（阻塞式）
 */
DMA_Status_t DMA_WaitComplete(DMA_Channel_t channel, uint32_t timeout)
{
    uint32_t start_tick;
    uint32_t flag;
    DMA_TypeDef *dma;
    
    /* 参数校验 */
    if (channel >= DMA_CHANNEL_MAX)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    if (!g_dma_initialized[channel])
    {
        return DMA_ERROR_NOT_INITIALIZED;
    }
    
    /* 使用默认超时时间 */
    if (timeout == 0)
    {
        timeout = DMA_DEFAULT_TIMEOUT_MS;
    }
    
    /* 确定DMA和标志位 */
    if (channel <= DMA_CHANNEL_1_7)
    {
        dma = DMA1;
        flag = DMA1_FLAG_TC1 << (channel * 4);
    }
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    else if (channel <= DMA_CHANNEL_2_5)
    {
        dma = DMA2;
        flag = DMA_FLAG_TC1 << ((channel - DMA_CHANNEL_2_1) * 4);
    }
#endif
    else
    {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    start_tick = Delay_GetTick();
    
    /* 等待传输完成 */
    while (DMA_GetFlagStatus(flag) == RESET)
    {
        /* 检查超时 */
        uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
        if (elapsed > timeout)
        {
            return DMA_ERROR_TIMEOUT;
        }
    }
    
    /* 清除标志位 */
    if (channel <= DMA_CHANNEL_1_7)
    {
        DMA1->IFCR |= (DMA_IFCR_CTCIF1 << (channel * 4));
    }
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    else if (channel <= DMA_CHANNEL_2_5)
    {
        DMA2->IFCR |= (DMA_IFCR_CTCIF1 << ((channel - DMA_CHANNEL_2_1) * 4));
    }
#endif
    
    /* 调用回调函数 */
    if (g_dma_callbacks[channel] != NULL)
    {
        g_dma_callbacks[channel](channel, g_dma_user_data[channel]);
    }
    
    return DMA_OK;
}

/**
 * @brief 检查DMA传输是否完成
 */
uint8_t DMA_IsComplete(DMA_Channel_t channel)
{
    uint32_t flag;
    DMA_TypeDef *dma;
    
    if (channel >= DMA_CHANNEL_MAX)
    {
        return 0;
    }
    
    if (!g_dma_initialized[channel])
    {
        return 0;
    }
    
    /* 确定DMA和标志位 */
    if (channel <= DMA_CHANNEL_1_7)
    {
        dma = DMA1;
        flag = DMA1_FLAG_TC1 << (channel * 4);
    }
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    else if (channel <= DMA_CHANNEL_2_5)
    {
        dma = DMA2;
        flag = DMA_FLAG_TC1 << ((channel - DMA_CHANNEL_2_1) * 4);
    }
#endif
    else
    {
        return 0;
    }
    
    if (DMA_GetFlagStatus(flag) != RESET)
    {
        /* 清除标志位 */
        if (channel <= DMA_CHANNEL_1_7)
        {
            DMA1->IFCR |= (DMA_IFCR_CTCIF1 << (channel * 4));
        }
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
        else if (channel <= DMA_CHANNEL_2_5)
        {
            DMA2->IFCR |= (DMA_IFCR_CTCIF1 << ((channel - DMA_CHANNEL_2_1) * 4));
        }
#endif
        
        /* 调用回调函数 */
        if (g_dma_callbacks[channel] != NULL)
        {
            g_dma_callbacks[channel](channel, g_dma_user_data[channel]);
        }
        
        return 1;
    }
    
    return 0;
}

/**
 * @brief 获取剩余传输数据量
 */
uint16_t DMA_GetRemainingDataSize(DMA_Channel_t channel)
{
    DMA_Channel_TypeDef *dma_channel;
    
    if (channel >= DMA_CHANNEL_MAX)
    {
        return 0;
    }
    
    if (!g_dma_initialized[channel])
    {
        return 0;
    }
    
    /* 获取DMA通道外设指针 */
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL)
    {
        return 0;
    }
    
    return DMA_GetCurrDataCounter(dma_channel);
}

/**
 * @brief 设置DMA传输完成回调函数
 */
DMA_Status_t DMA_SetTransferCompleteCallback(DMA_Channel_t channel,
                                             DMA_TransferCompleteCallback_t callback,
                                             void *user_data)
{
    /* 参数校验 */
    if (channel >= DMA_CHANNEL_MAX)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    if (!g_dma_initialized[channel])
    {
        return DMA_ERROR_NOT_INITIALIZED;
    }
    
    g_dma_callbacks[channel] = callback;
    g_dma_user_data[channel] = user_data;
    
    return DMA_OK;
}

/**
 * @brief 检查DMA是否已初始化
 */
uint8_t DMA_IsInitialized(DMA_Channel_t channel)
{
    if (channel >= DMA_CHANNEL_MAX)
    {
        return 0;
    }
    
    return g_dma_initialized[channel] ? 1 : 0;
}

/**
 * @brief 获取DMA通道外设指针
 */
DMA_Channel_TypeDef* DMA_GetChannel(DMA_Channel_t channel)
{
    if (channel >= DMA_CHANNEL_MAX)
    {
        return NULL;
    }
    
    if (!g_dma_initialized[channel])
    {
        return NULL;
    }
    
    return DMA_GetChannelPeriph(channel);
}

/**
 * @brief 设置DMA传输模式（正常/循环）
 */
DMA_Status_t DMA_SetMode(DMA_Channel_t channel, uint8_t mode)
{
    DMA_Channel_TypeDef *dma_channel;
    DMA_InitTypeDef DMA_InitStructure;
    
    if (channel >= DMA_CHANNEL_MAX)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    if (!g_dma_initialized[channel])
    {
        return DMA_ERROR_NOT_INITIALIZED;
    }
    
    if (mode > 1)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL)
    {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    /* 停止DMA传输 */
    DMA_Cmd(dma_channel, DISABLE);
    
    /* 读取当前配置 */
    DMA_InitStructure.DMA_PeripheralBaseAddr = dma_channel->CPAR;
    DMA_InitStructure.DMA_MemoryBaseAddr = dma_channel->CMAR;
    DMA_InitStructure.DMA_DIR = (dma_channel->CCR & DMA_CCR1_DIR) ? DMA_DIR_PeripheralSRC : DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = dma_channel->CNDTR;
    DMA_InitStructure.DMA_PeripheralInc = (dma_channel->CCR & DMA_CCR1_PINC) ? DMA_PeripheralInc_Enable : DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = (dma_channel->CCR & DMA_CCR1_MINC) ? DMA_MemoryInc_Enable : DMA_MemoryInc_Disable;
    DMA_InitStructure.DMA_PeripheralDataSize = (dma_channel->CCR & DMA_CCR1_PSIZE) >> 8;
    DMA_InitStructure.DMA_MemoryDataSize = (dma_channel->CCR & DMA_CCR1_MSIZE) >> 10;
    DMA_InitStructure.DMA_Mode = (mode == 1) ? DMA_Mode_Circular : DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = (dma_channel->CCR & DMA_CCR1_PL) >> 12;
    DMA_InitStructure.DMA_M2M = (dma_channel->CCR & DMA_CCR1_MEM2MEM) ? DMA_M2M_Enable : DMA_M2M_Disable;
    
    /* 内存到内存不支持循环模式 */
    if (DMA_InitStructure.DMA_M2M == DMA_M2M_Enable && mode == 1)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    /* 重新配置DMA通道 */
    DMA_Init(dma_channel, &DMA_InitStructure);  /* 调用标准库函数 */
    
    /* 更新配置表中的模式 */
    g_dma_configs[channel].mode = DMA_InitStructure.DMA_Mode;
    
    return DMA_OK;
}

/* ========== 中断模式功能实现 ========== */

/**
 * @brief 获取DMA中断类型对应的SPL库中断值
 */
static uint32_t DMA_GetITValue(DMA_IT_t it_type)
{
    switch (it_type)
    {
        case DMA_IT_TYPE_TC: return DMA_IT_TYPE_TC;
        case DMA_IT_TYPE_HT: return DMA_IT_TYPE_HT;
        case DMA_IT_TYPE_TE: return DMA_IT_TYPE_TE;
        default: return 0;
    }
}

/**
 * @brief 获取DMA通道对应的IRQn
 */
static IRQn_Type DMA_GetIRQn(DMA_Channel_t channel)
{
    switch (channel)
    {
        case DMA_CHANNEL_1_1: return DMA1_Channel1_IRQn;
        case DMA_CHANNEL_1_2: return DMA1_Channel2_IRQn;
        case DMA_CHANNEL_1_3: return DMA1_Channel3_IRQn;
        case DMA_CHANNEL_1_4: return DMA1_Channel4_IRQn;
        case DMA_CHANNEL_1_5: return DMA1_Channel5_IRQn;
        case DMA_CHANNEL_1_6: return DMA1_Channel6_IRQn;
        case DMA_CHANNEL_1_7: return DMA1_Channel7_IRQn;
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
        case DMA_CHANNEL_2_1: return DMA2_Channel1_IRQn;
        case DMA_CHANNEL_2_2: return DMA2_Channel2_IRQn;
        case DMA_CHANNEL_2_3: return DMA2_Channel3_IRQn;
        case DMA_CHANNEL_2_4: return DMA2_Channel4_IRQn;
        case DMA_CHANNEL_2_5: return DMA2_Channel5_IRQn;
#endif
        default: return (IRQn_Type)0;
    }
}

/**
 * @brief 使能DMA中断
 */
DMA_Status_t DMA_EnableIT(DMA_Channel_t channel, DMA_IT_t it_type)
{
    DMA_Channel_TypeDef *dma_channel;
    uint32_t it_value;
    IRQn_Type irqn;
    
    if (channel >= DMA_CHANNEL_MAX)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    if (!g_dma_initialized[channel])
    {
        return DMA_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 3)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL)
    {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    it_value = DMA_GetITValue(it_type);
    if (it_value == 0)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    /* 使能DMA中断 */
    DMA_ITConfig(dma_channel, it_value, ENABLE);
    
    /* 使能NVIC中断 */
    irqn = DMA_GetIRQn(channel);
    if (irqn != 0)
    {
        NVIC_HW_EnableIRQ(irqn);
    }
    
    return DMA_OK;
}

/**
 * @brief 禁用DMA中断
 */
DMA_Status_t DMA_DisableIT(DMA_Channel_t channel, DMA_IT_t it_type)
{
    DMA_Channel_TypeDef *dma_channel;
    uint32_t it_value;
    
    if (channel >= DMA_CHANNEL_MAX)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    if (!g_dma_initialized[channel])
    {
        return DMA_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 3)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL)
    {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    it_value = DMA_GetITValue(it_type);
    if (it_value == 0)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    /* 禁用DMA中断 */
    DMA_ITConfig(dma_channel, it_value, DISABLE);
    
    return DMA_OK;
}

/**
 * @brief 设置DMA中断回调函数
 */
DMA_Status_t DMA_SetITCallback(DMA_Channel_t channel, DMA_IT_t it_type,
                                DMA_IT_Callback_t callback, void *user_data)
{
    if (channel >= DMA_CHANNEL_MAX)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    if (!g_dma_initialized[channel])
    {
        return DMA_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 3)
    {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    g_dma_it_callbacks[channel][it_type] = callback;
    g_dma_it_user_data[channel][it_type] = user_data;
    
    return DMA_OK;
}

/**
 * @brief DMA中断服务函数
 */
void DMA_IRQHandler(DMA_Channel_t channel)
{
    DMA_Channel_TypeDef *dma_channel;
    DMA_TypeDef *dma;
    uint32_t flag_tc, flag_ht, flag_te;
    
    if (channel >= DMA_CHANNEL_MAX || !g_dma_initialized[channel])
    {
        return;
    }
    
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL)
    {
        return;
    }
    
    /* 确定DMA和标志位 */
    if (channel <= DMA_CHANNEL_1_7)
    {
        dma = DMA1;
        flag_tc = DMA1_FLAG_TC1 << (channel * 4);
        flag_ht = DMA1_FLAG_HT1 << (channel * 4);
        flag_te = DMA1_FLAG_TE1 << (channel * 4);
    }
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    else if (channel <= DMA_CHANNEL_2_5)
    {
        dma = DMA2;
        uint8_t ch_idx = channel - DMA_CHANNEL_2_1;
        flag_tc = DMA2_FLAG_TC1 << (ch_idx * 4);
        flag_ht = DMA2_FLAG_HT1 << (ch_idx * 4);
        flag_te = DMA2_FLAG_TE1 << (ch_idx * 4);
    }
#endif
    else
    {
        return;
    }
    
    /* 处理传输完成中断（TC） */
    uint32_t it_tc = (channel <= DMA_CHANNEL_1_7) ? (DMA1_IT_TC1 << (channel * 4)) : 
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
                     (DMA2_IT_TC1 << ((channel - DMA_CHANNEL_2_1) * 4));
#else
                     0;
#endif
    if (DMA_GetITStatus(it_tc) != RESET)
    {
        DMA_ClearITPendingBit(it_tc);
        
        /* 调用回调函数 */
        if (g_dma_it_callbacks[channel][DMA_IT_TYPE_TC] != NULL)
        {
            g_dma_it_callbacks[channel][DMA_IT_TYPE_TC](channel, DMA_IT_TYPE_TC, g_dma_it_user_data[channel][DMA_IT_TYPE_TC]);
        }
        
        /* 同时调用传输完成回调（兼容旧接口） */
        if (g_dma_callbacks[channel] != NULL)
        {
            g_dma_callbacks[channel](channel, g_dma_user_data[channel]);
        }
    }
    
    /* 处理半传输中断（HT） */
    uint32_t it_ht = (channel <= DMA_CHANNEL_1_7) ? (DMA1_IT_HT1 << (channel * 4)) : 
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
                     (DMA2_IT_HT1 << ((channel - DMA_CHANNEL_2_1) * 4));
#else
                     0;
#endif
    if (DMA_GetITStatus(it_ht) != RESET)
    {
        DMA_ClearITPendingBit(it_ht);
        
        /* 调用回调函数 */
        if (g_dma_it_callbacks[channel][DMA_IT_TYPE_HT] != NULL)
        {
            g_dma_it_callbacks[channel][DMA_IT_TYPE_HT](channel, DMA_IT_TYPE_HT, g_dma_it_user_data[channel][DMA_IT_TYPE_HT]);
        }
    }
    
    /* 处理传输错误中断（TE） */
    uint32_t it_te = (channel <= DMA_CHANNEL_1_7) ? (DMA1_IT_TE1 << (channel * 4)) : 
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
                     (DMA2_IT_TE1 << ((channel - DMA_CHANNEL_2_1) * 4));
#else
                     0;
#endif
    if (DMA_GetITStatus(it_te) != RESET)
    {
        DMA_ClearITPendingBit(it_te);
        
        /* 调用回调函数 */
        if (g_dma_it_callbacks[channel][DMA_IT_TYPE_TE] != NULL)
        {
            g_dma_it_callbacks[channel][DMA_IT_TYPE_TE](channel, DMA_IT_TYPE_TE, g_dma_it_user_data[channel][DMA_IT_TYPE_TE]);
        }
    }
}

/* DMA中断服务程序入口 */
void DMA1_Channel1_IRQHandler(void) { DMA_IRQHandler(DMA_CHANNEL_1_1); }
void DMA1_Channel2_IRQHandler(void) { DMA_IRQHandler(DMA_CHANNEL_1_2); }
void DMA1_Channel3_IRQHandler(void) { DMA_IRQHandler(DMA_CHANNEL_1_3); }
void DMA1_Channel4_IRQHandler(void) { DMA_IRQHandler(DMA_CHANNEL_1_4); }
void DMA1_Channel5_IRQHandler(void) { DMA_IRQHandler(DMA_CHANNEL_1_5); }
void DMA1_Channel6_IRQHandler(void) { DMA_IRQHandler(DMA_CHANNEL_1_6); }
void DMA1_Channel7_IRQHandler(void) { DMA_IRQHandler(DMA_CHANNEL_1_7); }

#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
void DMA2_Channel1_IRQHandler(void) { DMA_IRQHandler(DMA_CHANNEL_2_1); }
void DMA2_Channel2_IRQHandler(void) { DMA_IRQHandler(DMA_CHANNEL_2_2); }
void DMA2_Channel3_IRQHandler(void) { DMA_IRQHandler(DMA_CHANNEL_2_3); }
void DMA2_Channel4_IRQHandler(void) { DMA_IRQHandler(DMA_CHANNEL_2_4); }
void DMA2_Channel5_IRQHandler(void) { DMA_IRQHandler(DMA_CHANNEL_2_5); }
#endif

#endif /* CONFIG_MODULE_DMA_ENABLED */

