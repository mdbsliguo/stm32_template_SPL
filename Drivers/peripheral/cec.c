/**
 * @file cec.c
 * @brief CEC驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的CEC驱动，支持消费电子控制协议、HDMI CEC支持
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_CEC_ENABLED

/* Include our header */
#include "cec.h"

#include "stm32f10x_rcc.h"
#include "stm32f10x_cec.h"
#include <stdbool.h>
#include <stddef.h>

/* 初始化标志 */
static bool g_cec_initialized = false;

/**
 * @brief CEC初始化
 */
CEC_Status_t CEC_Init(CEC_BitTimingMode_t bit_timing_mode, CEC_BitPeriodMode_t bit_period_mode,
                      uint8_t own_address, uint16_t prescaler)
{
    CEC_InitTypeDef CEC_InitStructure;
    uint16_t bit_timing_value, bit_period_value;
    
    if (own_address >= 16)
    {
        return CEC_ERROR_INVALID_PARAM;
    }
    
    if (prescaler > 16383)
    {
        return CEC_ERROR_INVALID_PARAM;
    }
    
    if (bit_timing_mode >= 2 || bit_period_mode >= 2)
    {
        return CEC_ERROR_INVALID_PARAM;
    }
    
    /* 使能CEC时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CEC, ENABLE);
    
    /* 反初始化CEC */
    CEC_DeInit();
    
    /* 转换位时序错误模式 */
    bit_timing_value = (bit_timing_mode == CEC_BIT_TIMING_STD) ? CEC_BitTimingStdMode : CEC_BitTimingErrFreeMode;
    
    /* 转换位周期错误模式 */
    bit_period_value = (bit_period_mode == CEC_BIT_PERIOD_STD) ? CEC_BitPeriodStdMode : CEC_BitPeriodFlexibleMode;
    
    /* 配置CEC */
    CEC_InitStructure.CEC_BitTimingMode = bit_timing_value;
    CEC_InitStructure.CEC_BitPeriodMode = bit_period_value;
    CEC_Init(&CEC_InitStructure);
    
    /* 配置本机地址 */
    CEC_OwnAddressConfig(own_address);
    
    /* 设置预分频器 */
    CEC_SetPrescaler(prescaler);
    
    g_cec_initialized = true;
    
    return CEC_OK;
}

/**
 * @brief CEC反初始化
 */
CEC_Status_t CEC_Deinit(void)
{
    CEC_DeInit();
    g_cec_initialized = false;
    return CEC_OK;
}

/**
 * @brief 使能CEC
 */
CEC_Status_t CEC_Enable(void)
{
    if (!g_cec_initialized)
    {
        return CEC_ERROR_NOT_INITIALIZED;
    }
    
    CEC_Cmd(ENABLE);
    return CEC_OK;
}

/**
 * @brief 禁用CEC
 */
CEC_Status_t CEC_Disable(void)
{
    if (!g_cec_initialized)
    {
        return CEC_ERROR_NOT_INITIALIZED;
    }
    
    CEC_Cmd(DISABLE);
    return CEC_OK;
}

/**
 * @brief 使能CEC中断
 */
CEC_Status_t CEC_EnableIT(void)
{
    if (!g_cec_initialized)
    {
        return CEC_ERROR_NOT_INITIALIZED;
    }
    
    CEC_ITConfig(ENABLE);
    return CEC_OK;
}

/**
 * @brief 禁用CEC中断
 */
CEC_Status_t CEC_DisableIT(void)
{
    if (!g_cec_initialized)
    {
        return CEC_ERROR_NOT_INITIALIZED;
    }
    
    CEC_ITConfig(DISABLE);
    return CEC_OK;
}

/**
 * @brief 配置CEC本机地址
 */
CEC_Status_t CEC_SetOwnAddress(uint8_t own_address)
{
    if (!g_cec_initialized)
    {
        return CEC_ERROR_NOT_INITIALIZED;
    }
    
    if (own_address >= 16)
    {
        return CEC_ERROR_INVALID_PARAM;
    }
    
    CEC_OwnAddressConfig(own_address);
    return CEC_OK;
}

/**
 * @brief 设置CEC预分频器
 */
CEC_Status_t CEC_SetPrescaler(uint16_t prescaler)
{
    if (!g_cec_initialized)
    {
        return CEC_ERROR_NOT_INITIALIZED;
    }
    
    if (prescaler > 16383)
    {
        return CEC_ERROR_INVALID_PARAM;
    }
    
    CEC_SetPrescaler(prescaler);
    return CEC_OK;
}

/**
 * @brief 发送CEC数据字节
 */
CEC_Status_t CEC_SendDataByte(uint8_t data)
{
    if (!g_cec_initialized)
    {
        return CEC_ERROR_NOT_INITIALIZED;
    }
    
    CEC_SendDataByte(data);
    return CEC_OK;
}

/**
 * @brief 接收CEC数据字节（直接调用SPL库函数）
 */
uint8_t CEC_ReceiveDataByte(void)
{
    return CEC_ReceiveDataByte();
}

/**
 * @brief 启动CEC消息
 */
CEC_Status_t CEC_StartOfMessage(void)
{
    if (!g_cec_initialized)
    {
        return CEC_ERROR_NOT_INITIALIZED;
    }
    
    CEC_StartOfMessage();
    return CEC_OK;
}

/**
 * @brief 结束CEC消息
 */
CEC_Status_t CEC_EndOfMessage(void)
{
    if (!g_cec_initialized)
    {
        return CEC_ERROR_NOT_INITIALIZED;
    }
    
    CEC_EndOfMessageCmd(ENABLE);
    return CEC_OK;
}

/**
 * @brief 获取CEC标志状态（直接调用SPL库函数）
 */
uint8_t CEC_GetFlagStatus(uint32_t flag)
{
    if (CEC_GetFlagStatus(flag) != RESET)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief 清除CEC标志（直接调用SPL库函数）
 */
CEC_Status_t CEC_ClearFlag(uint32_t flag)
{
    if (!g_cec_initialized)
    {
        return CEC_ERROR_NOT_INITIALIZED;
    }
    
    CEC_ClearFlag(flag);
    return CEC_OK;
}

/**
 * @brief 获取CEC中断状态（直接调用SPL库函数）
 */
uint8_t CEC_GetITStatus(uint8_t it)
{
    if (CEC_GetITStatus(it) != RESET)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief 清除CEC中断挂起位（直接调用SPL库函数）
 */
CEC_Status_t CEC_ClearITPendingBit(uint16_t it)
{
    if (!g_cec_initialized)
    {
        return CEC_ERROR_NOT_INITIALIZED;
    }
    
    CEC_ClearITPendingBit(it);
    return CEC_OK;
}

/**
 * @brief 检查CEC是否已初始化
 */
uint8_t CEC_IsInitialized(void)
{
    return g_cec_initialized ? 1 : 0;
}

#endif /* CONFIG_MODULE_CEC_ENABLED */

