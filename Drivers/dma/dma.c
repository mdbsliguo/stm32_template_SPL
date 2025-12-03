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
#include "error_handler.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_dma.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 确保DMA_SetCurrDataCounter函数可用 */
#ifndef DMA_SetCurrDataCounter
/* 如果标准库中没有定义，我们需要自己定义 */
/* 但通常stm32f10x_dma.h中已经定义了，这里只是确保 */
#endif

/* 从board.h加载配置 */
#pragma diag_suppress 1296  /* 抑制扩展常量初始化器警告 */
static DMA_Config_t g_dma_configs[DMA_CHANNEL_MAX] = DMA_CONFIGS;
#pragma diag_default 1296   /* 恢复警告 */

/* 初始化标志 */
static bool g_dma_initialized[DMA_CHANNEL_MAX] = {false};

/* 传输完成回调函数 */
static DMA_TransferCompleteCallback_t g_dma_callbacks[DMA_CHANNEL_MAX] = {NULL};
static void *g_dma_user_data[DMA_CHANNEL_MAX] = {NULL};

/* 中断回调函数数组（支持3种中断类型：TC、HT、TE） */
static DMA_IT_Callback_t g_dma_it_callbacks[DMA_CHANNEL_MAX][3] = {NULL};
static void *g_dma_it_user_data[DMA_CHANNEL_MAX][3] = {NULL};

/* 当前传输配置（用于重新启动传输） */
static struct {
    uint32_t peripheral_addr;
    uint32_t memory_addr;
    uint16_t data_size;
    DMA_Direction_t direction;
    uint8_t data_width;
    uint8_t is_m2m;
} g_dma_transfer_config[DMA_CHANNEL_MAX];

/* 默认超时时间（毫秒） */
#define DMA_DEFAULT_TIMEOUT_MS  5000

/* ==================== 辅助宏和函数 ==================== */

/* 参数校验宏 */
#define DMA_CHECK_CHANNEL(ch) \
    do { \
        if ((ch) >= DMA_CHANNEL_MAX) { \
            return DMA_ERROR_INVALID_CHANNEL; \
        } \
    } while(0)

#define DMA_CHECK_INITIALIZED(ch) \
    do { \
        if (!g_dma_initialized[(ch)]) { \
            return DMA_ERROR_NOT_INITIALIZED; \
        } \
    } while(0)

/* 中断/标志位查找表（用于优化性能） */
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
/* HD/CL/HD_VL型号：支持DMA1和DMA2，共12个通道 */
static const uint32_t dma_it_tc_table[DMA_CHANNEL_MAX] = {
    DMA1_IT_GL1, DMA1_IT_GL2, DMA1_IT_GL3, DMA1_IT_GL4, 
    DMA1_IT_GL5, DMA1_IT_GL6, DMA1_IT_GL7,
    DMA2_IT_GL1, DMA2_IT_GL2, DMA2_IT_GL3, DMA2_IT_GL4, DMA2_IT_GL5,
};

static const uint32_t dma_it_ht_table[DMA_CHANNEL_MAX] = {
    DMA1_IT_HT1, DMA1_IT_HT2, DMA1_IT_HT3, DMA1_IT_HT4,
    DMA1_IT_HT5, DMA1_IT_HT6, DMA1_IT_HT7,
    DMA2_IT_HT1, DMA2_IT_HT2, DMA2_IT_HT3, DMA2_IT_HT4, DMA2_IT_HT5,
};

static const uint32_t dma_it_te_table[DMA_CHANNEL_MAX] = {
    DMA1_IT_TE1, DMA1_IT_TE2, DMA1_IT_TE3, DMA1_IT_TE4,
    DMA1_IT_TE5, DMA1_IT_TE6, DMA1_IT_TE7,
    DMA2_IT_TE1, DMA2_IT_TE2, DMA2_IT_TE3, DMA2_IT_TE4, DMA2_IT_TE5,
};

static const uint32_t dma_flag_tc_table[DMA_CHANNEL_MAX] = {
    DMA1_FLAG_TC1, DMA1_FLAG_TC2, DMA1_FLAG_TC3, DMA1_FLAG_TC4,
    DMA1_FLAG_TC5, DMA1_FLAG_TC6, DMA1_FLAG_TC7,
    DMA2_FLAG_TC1, DMA2_FLAG_TC2, DMA2_FLAG_TC3, DMA2_FLAG_TC4, DMA2_FLAG_TC5,
};

static const uint32_t dma_flag_ht_table[DMA_CHANNEL_MAX] = {
    DMA1_FLAG_HT1, DMA1_FLAG_HT2, DMA1_FLAG_HT3, DMA1_FLAG_HT4,
    DMA1_FLAG_HT5, DMA1_FLAG_HT6, DMA1_FLAG_HT7,
    DMA2_FLAG_HT1, DMA2_FLAG_HT2, DMA2_FLAG_HT3, DMA2_FLAG_HT4, DMA2_FLAG_HT5,
};

static const uint32_t dma_flag_te_table[DMA_CHANNEL_MAX] = {
    DMA1_FLAG_TE1, DMA1_FLAG_TE2, DMA1_FLAG_TE3, DMA1_FLAG_TE4,
    DMA1_FLAG_TE5, DMA1_FLAG_TE6, DMA1_FLAG_TE7,
    DMA2_FLAG_TE1, DMA2_FLAG_TE2, DMA2_FLAG_TE3, DMA2_FLAG_TE4, DMA2_FLAG_TE5,
};
#else
/* MD/LD型号：只支持DMA1，共7个通道 */
static const uint32_t dma_it_tc_table[DMA_CHANNEL_MAX] = {
    DMA1_IT_GL1, DMA1_IT_GL2, DMA1_IT_GL3, DMA1_IT_GL4, 
    DMA1_IT_GL5, DMA1_IT_GL6, DMA1_IT_GL7,
};

static const uint32_t dma_it_ht_table[DMA_CHANNEL_MAX] = {
    DMA1_IT_HT1, DMA1_IT_HT2, DMA1_IT_HT3, DMA1_IT_HT4,
    DMA1_IT_HT5, DMA1_IT_HT6, DMA1_IT_HT7,
};

static const uint32_t dma_it_te_table[DMA_CHANNEL_MAX] = {
    DMA1_IT_TE1, DMA1_IT_TE2, DMA1_IT_TE3, DMA1_IT_TE4,
    DMA1_IT_TE5, DMA1_IT_TE6, DMA1_IT_TE7,
};

static const uint32_t dma_flag_tc_table[DMA_CHANNEL_MAX] = {
    DMA1_FLAG_TC1, DMA1_FLAG_TC2, DMA1_FLAG_TC3, DMA1_FLAG_TC4,
    DMA1_FLAG_TC5, DMA1_FLAG_TC6, DMA1_FLAG_TC7,
};

static const uint32_t dma_flag_ht_table[DMA_CHANNEL_MAX] = {
    DMA1_FLAG_HT1, DMA1_FLAG_HT2, DMA1_FLAG_HT3, DMA1_FLAG_HT4,
    DMA1_FLAG_HT5, DMA1_FLAG_HT6, DMA1_FLAG_HT7,
};

static const uint32_t dma_flag_te_table[DMA_CHANNEL_MAX] = {
    DMA1_FLAG_TE1, DMA1_FLAG_TE2, DMA1_FLAG_TE3, DMA1_FLAG_TE4,
    DMA1_FLAG_TE5, DMA1_FLAG_TE6, DMA1_FLAG_TE7,
};
#endif

/* ==================== 辅助函数 ==================== */

/**
 * @brief 获取DMA通道外设指针
 * @param[in] channel DMA通道索引
 * @return DMA_Channel_TypeDef* DMA通道指针，失败返回NULL
 */
static DMA_Channel_TypeDef* DMA_GetChannelPeriph(DMA_Channel_t channel)
{
    switch (channel) {
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
    if (channel <= DMA_CHANNEL_1_7) {
        return RCC_AHBPeriph_DMA1;  /* DMA1通道 */
    }
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    else if (channel >= DMA_CHANNEL_2_1 && channel <= DMA_CHANNEL_2_5) {
        return RCC_AHBPeriph_DMA2;  /* DMA2通道 */
    }
#endif
    return 0;
}

/**
 * @brief 获取DMA通道对应的IRQn
 * @param[in] channel DMA通道索引
 * @return IRQn_Type IRQ号
 */
static IRQn_Type DMA_GetIRQn(DMA_Channel_t channel)
{
    switch (channel) {
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
 * @brief 获取DMA中断类型对应的SPL库中断值
 * @param[in] channel DMA通道索引
 * @param[in] it_type 中断类型
 * @return uint32_t SPL库中断值
 */
static uint32_t DMA_GetITValue(DMA_Channel_t channel, DMA_IT_t it_type)
{
    if (channel >= DMA_CHANNEL_MAX) {
        return 0;
    }
    
    /* 使用查找表优化性能 */
    switch (it_type) {
        case DMA_IT_TYPE_TC: return dma_it_tc_table[channel];
        case DMA_IT_TYPE_HT: return dma_it_ht_table[channel];
        case DMA_IT_TYPE_TE: return dma_it_te_table[channel];
        default: return 0;
    }
}

/**
 * @brief 获取DMA标志位
 * @param[in] channel DMA通道索引
 * @param[in] it_type 中断类型
 * @return uint32_t 标志位值
 */
static uint32_t DMA_GetFlagValue(DMA_Channel_t channel, DMA_IT_t it_type)
{
    if (channel >= DMA_CHANNEL_MAX) {
        return 0;
    }
    
    /* 使用查找表优化性能 */
    switch (it_type) {
        case DMA_IT_TYPE_TC: return dma_flag_tc_table[channel];
        case DMA_IT_TYPE_HT: return dma_flag_ht_table[channel];
        case DMA_IT_TYPE_TE: return dma_flag_te_table[channel];
        default: return 0;
    }
}

/* ==================== 主要功能实现 ==================== */

/**
 * @brief 转换数据宽度为SPL库值
 * @param[in] data_width 数据宽度（1=字节，2=半字，4=字）
 * @param[out] peripheral_size 外设数据宽度（输出）
 * @param[out] memory_size 内存数据宽度（输出）
 * @return DMA_Status_t 错误码
 */
static DMA_Status_t DMA_ConvertDataWidth(uint8_t data_width, uint32_t *peripheral_size, uint32_t *memory_size)
{
    switch (data_width) {
        case 1:
            *peripheral_size = DMA_PeripheralDataSize_Byte;
            *memory_size = DMA_MemoryDataSize_Byte;
            return DMA_OK;
        case 2:
            *peripheral_size = DMA_PeripheralDataSize_HalfWord;
            *memory_size = DMA_MemoryDataSize_HalfWord;
            return DMA_OK;
        case 4:
            *peripheral_size = DMA_PeripheralDataSize_Word;
            *memory_size = DMA_MemoryDataSize_Word;
            return DMA_OK;
        default:
            return DMA_ERROR_INVALID_PARAM;
    }
}

/**
 * @brief 处理DMA中断（内部辅助函数）
 * @param[in] channel DMA通道索引
 * @param[in] it_type 中断类型
 */
static void DMA_ProcessIT(DMA_Channel_t channel, DMA_IT_t it_type)
{
    uint32_t it_value = DMA_GetITValue(channel, it_type);
    if (it_value == 0) {
        return;
    }
    
    if (DMA_GetITStatus(it_value) != RESET) {
        /* 清除中断挂起位 */
        DMA_ClearITPendingBit(it_value);
        
        /* 对于TC中断，调用传输完成回调 */
        if (it_type == DMA_IT_TYPE_TC && g_dma_callbacks[channel] != NULL) {
            g_dma_callbacks[channel](channel, g_dma_user_data[channel]);
        }
        
        /* 调用中断回调 */
        DMA_IT_Callback_t callback = g_dma_it_callbacks[channel][it_type];
        void *user_data = g_dma_it_user_data[channel][it_type];
        if (callback != NULL) {
            callback(channel, it_type, user_data);
        }
    }
}

/**
 * @brief DMA初始化
 */
DMA_Status_t DMA_HW_Init(DMA_Channel_t channel)
{
    DMA_Channel_TypeDef *dma_channel;
    uint32_t dma_clock;
    
    /* ========== 参数校验 ========== */
    DMA_CHECK_CHANNEL(channel);
    
    if (!g_dma_configs[channel].enabled) {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已初始化 */
    if (g_dma_initialized[channel]) {
        return DMA_OK;
    }
    
    /* 获取DMA通道外设指针 */
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL) {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    /* 获取DMA外设时钟 */
    dma_clock = DMA_GetPeriphClock(channel);
    if (dma_clock == 0) {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    /* 使能DMA外设时钟 */
    RCC_AHBPeriphClockCmd(dma_clock, ENABLE);
    
    /* 反初始化DMA通道（确保干净状态） */
    DMA_DeInit(dma_channel);
    
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
    
    /* ========== 参数校验 ========== */
    DMA_CHECK_CHANNEL(channel);
    
    if (!g_dma_initialized[channel]) {
        return DMA_OK;
    }
    
    /* 获取DMA通道外设指针 */
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL) {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    /* 停止DMA传输 */
    DMA_Cmd(dma_channel, DISABLE);
    
    /* 禁用所有中断 */
    DMA_ITConfig(dma_channel, DMA_IT_TC | DMA_IT_HT | DMA_IT_TE, DISABLE);
    
    /* 清除中断标志（清除所有可能的标志位） */
    uint32_t clear_flag = DMA_GetFlagValue(channel, DMA_IT_TYPE_TC);
    if (clear_flag != 0) {
        DMA_ClearFlag(clear_flag | DMA_GetFlagValue(channel, DMA_IT_TYPE_HT) | DMA_GetFlagValue(channel, DMA_IT_TYPE_TE));
    }
    
    /* 反初始化DMA通道 */
    DMA_DeInit(dma_channel);
    
    /* 获取DMA外设时钟 */
    dma_clock = DMA_GetPeriphClock(channel);
    if (dma_clock != 0) {
        /* 禁用DMA外设时钟 */
        RCC_AHBPeriphClockCmd(dma_clock, DISABLE);
    }
    
    /* 清除回调函数 */
    g_dma_callbacks[channel] = NULL;
    g_dma_user_data[channel] = NULL;
    for (int i = 0; i < 3; i++) {
        g_dma_it_callbacks[channel][i] = NULL;
        g_dma_it_user_data[channel][i] = NULL;
    }
    
    /* 清除传输配置 */
    memset(&g_dma_transfer_config[channel], 0, sizeof(g_dma_transfer_config[channel]));
    
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
    DMA_Channel_TypeDef *dma_channel;
    DMA_InitTypeDef DMA_InitStructure;
    uint32_t peripheral_data_size;
    uint32_t memory_data_size;
    DMA_Status_t status;
    
    /* ========== 参数校验 ========== */
    DMA_CHECK_CHANNEL(channel);
    if (peripheral_addr == 0 || memory_addr == 0 || data_size == 0) {
        return DMA_ERROR_INVALID_PARAM;
    }
    /* 不允许内存到内存传输（应使用DMA_ConfigMemoryToMemory） */
    if (direction == DMA_DIR_MEMORY_TO_MEMORY || direction > DMA_DIR_MEMORY_TO_MEMORY) {
        return DMA_ERROR_INVALID_PARAM;
    }
    if (data_width != 1 && data_width != 2 && data_width != 4) {
        return DMA_ERROR_INVALID_PARAM;  /* 数据宽度：1=字节，2=半字，4=字 */
    }
    
    DMA_CHECK_INITIALIZED(channel);
    
    /* 获取DMA通道外设指针 */
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL) {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    /* 停止当前传输 */
    DMA_Cmd(dma_channel, DISABLE);
    
    /* 等待DMA完全停止（重要：确保DMA停止后再重新配置） */
    {
        uint32_t wait_start = Delay_GetTick();
        while ((dma_channel->CCR & 0x0001) != 0) {  /* 等待EN位清除 */
            if (Delay_GetElapsed(Delay_GetTick(), wait_start) > 10) {
                break;  /* 超时退出 */
            }
        }
    }
    
    /* 转换数据宽度 */
    status = DMA_ConvertDataWidth(data_width, &peripheral_data_size, &memory_data_size);
    if (status != DMA_OK) {
        return status;
    }
    
    /* 计算传输数据单位数（根据数据宽度） */
    uint16_t transfer_count = data_size / data_width;
    if (transfer_count == 0 || (data_size % data_width != 0)) {
        return DMA_ERROR_INVALID_PARAM;  /* 数据大小必须是数据宽度的整数倍 */
    }
    
    /* 配置DMA初始化结构体 */
    DMA_InitStructure.DMA_PeripheralBaseAddr = peripheral_addr;
    DMA_InitStructure.DMA_MemoryBaseAddr = memory_addr;
    DMA_InitStructure.DMA_DIR = (direction == DMA_DIR_PERIPHERAL_TO_MEMORY) ? 
                                 DMA_DIR_PeripheralSRC : DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = transfer_count;
    DMA_InitStructure.DMA_PeripheralInc = g_dma_configs[channel].peripheral_inc;
    DMA_InitStructure.DMA_MemoryInc = g_dma_configs[channel].memory_inc;
    DMA_InitStructure.DMA_PeripheralDataSize = peripheral_data_size;
    DMA_InitStructure.DMA_MemoryDataSize = memory_data_size;
    DMA_InitStructure.DMA_Mode = g_dma_configs[channel].mode;
    DMA_InitStructure.DMA_Priority = g_dma_configs[channel].priority;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;  /* 外设传输不使用M2M模式 */
    
    /* 初始化DMA通道 */
    DMA_Init(dma_channel, &DMA_InitStructure);
    
    /* 保存传输配置（用于重新启动） */
    g_dma_transfer_config[channel].peripheral_addr = peripheral_addr;
    g_dma_transfer_config[channel].memory_addr = memory_addr;
    g_dma_transfer_config[channel].data_size = data_size;
    g_dma_transfer_config[channel].direction = direction;
    g_dma_transfer_config[channel].data_width = data_width;
    g_dma_transfer_config[channel].is_m2m = 0;
    
    return DMA_OK;
}

/**
 * @brief 配置DMA内存到内存传输
 */
DMA_Status_t DMA_ConfigMemoryToMemory(DMA_Channel_t channel, uint32_t src_addr,
                                      uint32_t dst_addr, uint16_t data_size, uint8_t data_width)
{
    DMA_Channel_TypeDef *dma_channel;
    DMA_InitTypeDef DMA_InitStructure;
    uint32_t data_size_value;
    uint32_t unused;
    DMA_Status_t status;
    
    /* ========== 参数校验 ========== */
    DMA_CHECK_CHANNEL(channel);
    if (src_addr == 0 || dst_addr == 0 || data_size == 0) {
        return DMA_ERROR_INVALID_PARAM;
    }
    if (data_width != 1 && data_width != 2 && data_width != 4) {
        return DMA_ERROR_INVALID_PARAM;  /* 数据宽度：1=字节，2=半字，4=字 */
    }
    
    DMA_CHECK_INITIALIZED(channel);
    
    /* 获取DMA通道外设指针 */
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL) {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    /* 停止当前传输 */
    DMA_Cmd(dma_channel, DISABLE);
    
    /* 转换数据宽度（内存到内存，外设和内存数据宽度相同） */
    status = DMA_ConvertDataWidth(data_width, &data_size_value, &unused);
    if (status != DMA_OK) {
        return status;
    }
    
    /* 计算传输数据单位数（根据数据宽度） */
    uint16_t transfer_count = data_size / data_width;
    if (transfer_count == 0 || (data_size % data_width != 0)) {
        return DMA_ERROR_INVALID_PARAM;  /* 数据大小必须是数据宽度的整数倍 */
    }
    
    /* 配置DMA初始化结构体 */
    DMA_InitStructure.DMA_PeripheralBaseAddr = src_addr;
    DMA_InitStructure.DMA_MemoryBaseAddr = dst_addr;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;  /* 内存到内存使用PeripheralSRC */
    DMA_InitStructure.DMA_BufferSize = transfer_count;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Enable;  /* 内存到内存，源地址递增 */
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;  /* 内存到内存，目标地址递增 */
    DMA_InitStructure.DMA_PeripheralDataSize = data_size_value;
    DMA_InitStructure.DMA_MemoryDataSize = data_size_value;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;  /* 内存到内存只能使用正常模式 */
    DMA_InitStructure.DMA_Priority = g_dma_configs[channel].priority;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Enable;  /* 使能内存到内存模式 */
    
    /* 初始化DMA通道 */
    DMA_Init(dma_channel, &DMA_InitStructure);
    
    /* 保存传输配置（用于重新启动） */
    g_dma_transfer_config[channel].peripheral_addr = src_addr;
    g_dma_transfer_config[channel].memory_addr = dst_addr;
    g_dma_transfer_config[channel].data_size = data_size;
    g_dma_transfer_config[channel].direction = DMA_DIR_MEMORY_TO_MEMORY;
    g_dma_transfer_config[channel].data_width = data_width;
    g_dma_transfer_config[channel].is_m2m = 1;
    
    return DMA_OK;
}

/**
 * @brief 启动DMA传输
 */
DMA_Status_t DMA_Start(DMA_Channel_t channel)
{
    DMA_Channel_TypeDef *dma_channel;
    
    /* ========== 参数校验 ========== */
    if (channel >= DMA_CHANNEL_MAX) {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    if (!g_dma_initialized[channel]) {
        return DMA_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取DMA通道外设指针 */
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL) {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    /* 重要：确保DMA已禁用，然后重新设置计数器（参考案例的做法） */
    DMA_Cmd(dma_channel, DISABLE);
    
    /* 等待DMA完全停止 */
    {
        uint32_t wait_start = Delay_GetTick();
        while ((dma_channel->CCR & 0x0001) != 0) {  /* 等待EN位清除 */
            if (Delay_GetElapsed(Delay_GetTick(), wait_start) > 10) {
                break;  /* 超时退出 */
            }
        }
    }
    
    /* 重新设置传输计数器（重要：确保计数器正确） */
    if (g_dma_transfer_config[channel].data_width != 0) {
        uint16_t transfer_count = g_dma_transfer_config[channel].data_size / g_dma_transfer_config[channel].data_width;
        if (transfer_count > 0) {
            DMA_SetCurrDataCounter(dma_channel, transfer_count);
        }
    }
    
    /* 清除所有可能的标志位（TC、HT、TE），避免误判 */
    uint32_t tc_flag = DMA_GetFlagValue(channel, DMA_IT_TYPE_TC);
    uint32_t ht_flag = DMA_GetFlagValue(channel, DMA_IT_TYPE_HT);
    uint32_t te_flag = DMA_GetFlagValue(channel, DMA_IT_TYPE_TE);
    if (tc_flag != 0) {
        DMA_ClearFlag(tc_flag | (ht_flag != 0 ? ht_flag : 0) | (te_flag != 0 ? te_flag : 0));
    }
    
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
    
    /* ========== 参数校验 ========== */
    if (channel >= DMA_CHANNEL_MAX) {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    if (!g_dma_initialized[channel]) {
        return DMA_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取DMA通道外设指针 */
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL) {
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
    DMA_Channel_TypeDef *dma_channel;
    uint32_t start_tick;
    uint32_t actual_timeout;
    uint32_t flag;
    uint8_t is_circular;
    
    /* ========== 参数校验 ========== */
    if (channel >= DMA_CHANNEL_MAX) {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    if (!g_dma_initialized[channel]) {
        return DMA_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取DMA通道外设指针 */
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL) {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    /* 检查DMA是否已启动 */
    if ((dma_channel->CCR & 0x0001) == 0) {  /* EN位为0表示未启动 */
        return DMA_ERROR_BUSY;  /* DMA未启动，无法等待完成 */
    }
    
    /* 检查是否为循环模式 */
    is_circular = (dma_channel->CCR & 0x0020) != 0;  /* CIRC位 */
    if (is_circular) {
        /* 循环模式下，WaitComplete不适用（会一直等待），返回错误 */
        return DMA_ERROR_INVALID_PARAM;
    }
    
    actual_timeout = (timeout == 0) ? DMA_DEFAULT_TIMEOUT_MS : timeout;
    start_tick = Delay_GetTick();
    flag = DMA_GetFlagValue(channel, DMA_IT_TYPE_TC);
    
    /* 等待传输完成标志 */
    while (DMA_GetFlagStatus(flag) == RESET) {
        /* 检查超时 */
        uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
        if (elapsed > actual_timeout) {
            return DMA_ERROR_TIMEOUT;
        }
        
        /* 检查传输错误标志 */
        uint32_t te_flag = DMA_GetFlagValue(channel, DMA_IT_TYPE_TE);
        if (te_flag != 0 && DMA_GetFlagStatus(te_flag) != RESET) {
            DMA_ClearFlag(te_flag);
            return DMA_ERROR_TRANSFER_FAILED;
        }
        
        /* 检查DMA是否已停止（可能被外部停止） */
        if ((dma_channel->CCR & 0x0001) == 0) {
            return DMA_ERROR_BUSY;
        }
    }
    
    /* 清除传输完成标志 */
    DMA_ClearFlag(flag);
    
    return DMA_OK;
}

/**
 * @brief 检查DMA传输是否完成
 */
uint8_t DMA_IsComplete(DMA_Channel_t channel)
{
    uint32_t flag;
    
    /* ========== 参数校验 ========== */
    if (channel >= DMA_CHANNEL_MAX) {
        return 0;  /* 无效通道返回0（未完成） */
    }
    
    if (!g_dma_initialized[channel]) {
        return 0;
    }
    
    flag = DMA_GetFlagValue(channel, DMA_IT_TYPE_TC);
    if (flag == 0) {
        return 0;
    }
    
    return (DMA_GetFlagStatus(flag) != RESET) ? 1 : 0;
}

/**
 * @brief 获取剩余传输数据量
 */
uint16_t DMA_GetRemainingDataSize(DMA_Channel_t channel)
{
    DMA_Channel_TypeDef *dma_channel;
    
    /* ========== 参数校验 ========== */
    if (channel >= DMA_CHANNEL_MAX) {
        return 0;  /* 无效通道返回0 */
    }
    
    if (!g_dma_initialized[channel]) {
        return 0;
    }
    
    /* 获取DMA通道外设指针 */
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL) {
        return 0;
    }
    
    /* 检查是否已配置传输 */
    if (g_dma_transfer_config[channel].data_width == 0) {
        return 0;  /* 未配置传输，返回0 */
    }
    
    /* 获取当前剩余数据计数器 */
    uint16_t remaining = DMA_GetCurrDataCounter(dma_channel);
    
    /* 转换为字节数 */
    uint8_t data_width = g_dma_transfer_config[channel].data_width;
    
    return remaining * data_width;
}

/**
 * @brief 设置DMA传输完成回调函数
 */
DMA_Status_t DMA_SetTransferCompleteCallback(DMA_Channel_t channel,
                                             DMA_TransferCompleteCallback_t callback,
                                             void *user_data)
{
    /* ========== 参数校验 ========== */
    if (channel >= DMA_CHANNEL_MAX) {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    /* 注意：callback可以为NULL（表示禁用回调），user_data可以为NULL */
    
    if (!g_dma_initialized[channel]) {
        return DMA_ERROR_NOT_INITIALIZED;
    }
    
    /* 保存回调函数 */
    g_dma_callbacks[channel] = callback;
    g_dma_user_data[channel] = user_data;
    
    return DMA_OK;
}

/**
 * @brief 检查DMA是否已初始化
 */
uint8_t DMA_IsInitialized(DMA_Channel_t channel)
{
    /* ========== 参数校验 ========== */
    if (channel >= DMA_CHANNEL_MAX) {
        return 0;  /* 无效通道返回0（未初始化） */
    }
    
    return g_dma_initialized[channel] ? 1 : 0;
}

/**
 * @brief 获取DMA通道外设指针
 */
DMA_Channel_TypeDef* DMA_GetChannel(DMA_Channel_t channel)
{
    /* ========== 参数校验 ========== */
    if (channel >= DMA_CHANNEL_MAX) {
        return NULL;  /* 无效通道返回NULL */
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
    
    /* ========== 参数校验 ========== */
    if (channel >= DMA_CHANNEL_MAX) {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    if (mode > 1) {
        return DMA_ERROR_INVALID_PARAM;  /* 模式：0=正常，1=循环 */
    }
    
    if (!g_dma_initialized[channel]) {
        return DMA_ERROR_NOT_INITIALIZED;
    }
    
    /* 内存到内存传输不支持循环模式 */
    if (g_dma_transfer_config[channel].is_m2m && mode == 1) {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    /* 获取DMA通道外设指针 */
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL) {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    /* 停止当前传输 */
    DMA_Cmd(dma_channel, DISABLE);
    
    /* 等待DMA停止（确保配置稳定） */
    uint32_t wait_start = Delay_GetTick();
    while ((dma_channel->CCR & 0x0001) != 0) {  /* 等待EN位清零 */
        if (Delay_GetElapsed(Delay_GetTick(), wait_start) > 10) {
            break;  /* 超时保护 */
        }
    }
    
    /* 读取当前配置（使用保存的配置更可靠） */
    if (g_dma_transfer_config[channel].data_width != 0) {
        /* 使用保存的配置 */
        uint32_t peripheral_data_size, memory_data_size;
        DMA_ConvertDataWidth(g_dma_transfer_config[channel].data_width, &peripheral_data_size, &memory_data_size);
        
        DMA_InitStructure.DMA_PeripheralBaseAddr = g_dma_transfer_config[channel].peripheral_addr;
        DMA_InitStructure.DMA_MemoryBaseAddr = g_dma_transfer_config[channel].memory_addr;
        DMA_InitStructure.DMA_DIR = (g_dma_transfer_config[channel].direction == DMA_DIR_PERIPHERAL_TO_MEMORY) ? 
                                     DMA_DIR_PeripheralSRC : DMA_DIR_PeripheralDST;
        DMA_InitStructure.DMA_BufferSize = g_dma_transfer_config[channel].data_size / g_dma_transfer_config[channel].data_width;
        DMA_InitStructure.DMA_PeripheralInc = g_dma_configs[channel].peripheral_inc;
        DMA_InitStructure.DMA_MemoryInc = g_dma_configs[channel].memory_inc;
        DMA_InitStructure.DMA_PeripheralDataSize = peripheral_data_size;
        DMA_InitStructure.DMA_MemoryDataSize = memory_data_size;
        DMA_InitStructure.DMA_Mode = mode ? DMA_Mode_Circular : DMA_Mode_Normal;
        DMA_InitStructure.DMA_Priority = g_dma_configs[channel].priority;
        DMA_InitStructure.DMA_M2M = g_dma_transfer_config[channel].is_m2m ? DMA_M2M_Enable : DMA_M2M_Disable;
    } else {
        /* 如果没有保存的配置，从寄存器读取 */
        uint32_t ccr = dma_channel->CCR;
        DMA_InitStructure.DMA_PeripheralBaseAddr = dma_channel->CPAR;
        DMA_InitStructure.DMA_MemoryBaseAddr = dma_channel->CMAR;
        DMA_InitStructure.DMA_DIR = (ccr & 0x0010) ? DMA_DIR_PeripheralDST : DMA_DIR_PeripheralSRC;  /* DIR bit */
        DMA_InitStructure.DMA_BufferSize = dma_channel->CNDTR;
        DMA_InitStructure.DMA_PeripheralInc = (ccr & 0x0040) ? DMA_PeripheralInc_Enable : DMA_PeripheralInc_Disable;  /* PINC bit */
        DMA_InitStructure.DMA_MemoryInc = (ccr & 0x0080) ? DMA_MemoryInc_Enable : DMA_MemoryInc_Disable;  /* MINC bit */
        DMA_InitStructure.DMA_PeripheralDataSize = ccr & 0x0300;  /* PSIZE bits */
        DMA_InitStructure.DMA_MemoryDataSize = ccr & 0x0C00;  /* MSIZE bits */
        DMA_InitStructure.DMA_Mode = mode ? DMA_Mode_Circular : DMA_Mode_Normal;
        DMA_InitStructure.DMA_Priority = ccr & 0x3000;  /* PL bits */
        DMA_InitStructure.DMA_M2M = (ccr & 0x4000) ? DMA_M2M_Enable : DMA_M2M_Disable;  /* MEM2MEM bit */
    }
    
    /* 重新初始化DMA通道 */
    DMA_Init(dma_channel, &DMA_InitStructure);
    
    /* 更新配置 */
    g_dma_configs[channel].mode = DMA_InitStructure.DMA_Mode;
    
    return DMA_OK;
}

/* ========== 中断模式功能实现 ========== */

/**
 * @brief 使能DMA中断
 */
DMA_Status_t DMA_EnableIT(DMA_Channel_t channel, DMA_IT_t it_type)
{
    DMA_Channel_TypeDef *dma_channel;
    uint32_t it_value;
    IRQn_Type irqn;
    
    /* ========== 参数校验 ========== */
    if (channel >= DMA_CHANNEL_MAX) {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    if (it_type > DMA_IT_TYPE_TE) {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    if (!g_dma_initialized[channel]) {
        return DMA_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取DMA通道外设指针 */
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL) {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    /* 获取中断值 */
    it_value = DMA_GetITValue(channel, it_type);
    if (it_value == 0) {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    /* 使能DMA中断 */
    DMA_ITConfig(dma_channel, it_value, ENABLE);
    
    /* 使能NVIC中断 */
    irqn = DMA_GetIRQn(channel);
    if (irqn != 0) {
        NVIC_ConfigIRQ(irqn, 2, 2, 1);  /* 默认优先级：抢占优先级2，子优先级2 */
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
    
    /* ========== 参数校验 ========== */
    if (channel >= DMA_CHANNEL_MAX) {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    if (it_type > DMA_IT_TYPE_TE) {
        return DMA_ERROR_INVALID_PARAM;
    }
    
    if (!g_dma_initialized[channel]) {
        return DMA_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取DMA通道外设指针 */
    dma_channel = DMA_GetChannelPeriph(channel);
    if (dma_channel == NULL) {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    
    /* 获取中断值 */
    it_value = DMA_GetITValue(channel, it_type);
    if (it_value == 0) {
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
    /* ========== 参数校验 ========== */
    if (channel >= DMA_CHANNEL_MAX) {
        return DMA_ERROR_INVALID_CHANNEL;
    }
    if (it_type > DMA_IT_TYPE_TE) {
        return DMA_ERROR_INVALID_PARAM;
    }
    /* 注意：callback可以为NULL（表示禁用回调），user_data可以为NULL */
    
    if (!g_dma_initialized[channel]) {
        return DMA_ERROR_NOT_INITIALIZED;
    }
    
    /* 保存回调函数 */
    g_dma_it_callbacks[channel][it_type] = callback;
    g_dma_it_user_data[channel][it_type] = user_data;
    
    return DMA_OK;
}

/**
 * @brief DMA中断服务函数
 */
void DMA_IRQHandler(DMA_Channel_t channel)
{
    /* ========== 参数校验 ========== */
    if (channel >= DMA_CHANNEL_MAX) {
        return;  /* 无效通道直接返回 */
    }
    
    if (!g_dma_initialized[channel]) {
        return;
    }
    
    /* 处理三种中断类型（使用统一的处理函数，减少代码重复） */
    DMA_ProcessIT(channel, DMA_IT_TYPE_TC);
    DMA_ProcessIT(channel, DMA_IT_TYPE_HT);
    DMA_ProcessIT(channel, DMA_IT_TYPE_TE);
}

/* 注意：DMA中断服务程序入口在Core/stm32f10x_it.c中实现，遵循项目规范 */

#endif /* CONFIG_MODULE_DMA_ENABLED */

