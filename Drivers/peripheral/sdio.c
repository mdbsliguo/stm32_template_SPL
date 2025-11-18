/**
 * @file sdio.c
 * @brief SDIO驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的SDIO驱动，支持SD卡接口、MMC卡支持、4位/1位模式
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_SDIO_ENABLED

/* Include our header */
#include "sdio.h"

#include "stm32f10x_rcc.h"
#include "stm32f10x_sdio.h"
#include <stdbool.h>
#include <stddef.h>

/* 初始化标志 */
static bool g_sdio_initialized = false;

/**
 * @brief SDIO初始化
 */
SDIO_Status_t SDIO_Init(uint8_t clock_div, SDIO_BusWidth_t bus_width)
{
    SDIO_InitTypeDef SDIO_InitStructure;
    uint32_t bus_width_value;
    
    if (clock_div > 255)
    {
        return SDIO_ERROR_INVALID_PARAM;
    }
    
    if (bus_width >= 2)
    {
        return SDIO_ERROR_INVALID_PARAM;
    }
    
    /* 使能SDIO时钟 */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_SDIO, ENABLE);
    
    /* 反初始化SDIO */
    SDIO_DeInit();
    
    /* 配置SDIO */
    SDIO_InitStructure.SDIO_ClockEdge = SDIO_ClockEdge_Rising;
    SDIO_InitStructure.SDIO_ClockBypass = SDIO_ClockBypass_Disable;
    SDIO_InitStructure.SDIO_ClockPowerSave = SDIO_ClockPowerSave_Disable;
    SDIO_InitStructure.SDIO_ClockDiv = clock_div;
    SDIO_InitStructure.SDIO_HardwareFlowControl = SDIO_HardwareFlowControl_Disable;
    
    /* 转换总线宽度 */
    bus_width_value = (bus_width == SDIO_BUS_WIDTH_1BIT) ? SDIO_BusWide_1b : SDIO_BusWide_4b;
    SDIO_InitStructure.SDIO_BusWide = bus_width_value;
    
    /* 初始化SDIO */
    SDIO_Init(&SDIO_InitStructure);
    
    /* 使能SDIO时钟 */
    SDIO_ClockCmd(ENABLE);
    
    /* 设置电源状态为ON */
    SDIO_SetPowerState(SDIO_PowerState_ON);
    
    g_sdio_initialized = true;
    
    return SDIO_OK;
}

/**
 * @brief SDIO反初始化
 */
SDIO_Status_t SDIO_Deinit(void)
{
    SDIO_DeInit();
    g_sdio_initialized = false;
    return SDIO_OK;
}

/**
 * @brief 使能SDIO时钟
 */
SDIO_Status_t SDIO_EnableClock(void)
{
    SDIO_ClockCmd(ENABLE);
    return SDIO_OK;
}

/**
 * @brief 禁用SDIO时钟
 */
SDIO_Status_t SDIO_DisableClock(void)
{
    SDIO_ClockCmd(DISABLE);
    return SDIO_OK;
}

/**
 * @brief 设置SDIO电源状态（直接调用SPL库函数）
 */
SDIO_Status_t SDIO_SetPowerState(uint32_t power_state)
{
    SDIO_SetPowerState(power_state);
    return SDIO_OK;
}

/**
 * @brief 获取SDIO电源状态（直接调用SPL库函数）
 */
uint32_t SDIO_GetPowerState(void)
{
    return SDIO_GetPowerState();
}

/**
 * @brief 使能SDIO中断
 */
SDIO_Status_t SDIO_EnableIT(uint32_t sdio_it)
{
    if (!g_sdio_initialized)
    {
        return SDIO_ERROR_NOT_INITIALIZED;
    }
    
    SDIO_ITConfig(sdio_it, ENABLE);
    return SDIO_OK;
}

/**
 * @brief 禁用SDIO中断
 */
SDIO_Status_t SDIO_DisableIT(uint32_t sdio_it)
{
    if (!g_sdio_initialized)
    {
        return SDIO_ERROR_NOT_INITIALIZED;
    }
    
    SDIO_ITConfig(sdio_it, DISABLE);
    return SDIO_OK;
}

/**
 * @brief 使能SDIO DMA
 */
SDIO_Status_t SDIO_EnableDMA(void)
{
    if (!g_sdio_initialized)
    {
        return SDIO_ERROR_NOT_INITIALIZED;
    }
    
    SDIO_DMACmd(ENABLE);
    return SDIO_OK;
}

/**
 * @brief 禁用SDIO DMA
 */
SDIO_Status_t SDIO_DisableDMA(void)
{
    if (!g_sdio_initialized)
    {
        return SDIO_ERROR_NOT_INITIALIZED;
    }
    
    SDIO_DMACmd(DISABLE);
    return SDIO_OK;
}

/**
 * @brief 发送SDIO命令
 */
SDIO_Status_t SDIO_SendCommand(uint8_t cmd_index, uint32_t argument, uint32_t response_type)
{
    SDIO_CmdInitTypeDef SDIO_CmdInitStructure;
    
    if (!g_sdio_initialized)
    {
        return SDIO_ERROR_NOT_INITIALIZED;
    }
    
    if (cmd_index >= 64)
    {
        return SDIO_ERROR_INVALID_PARAM;
    }
    
    /* 配置命令 */
    SDIO_CmdInitStructure.SDIO_Argument = argument;
    SDIO_CmdInitStructure.SDIO_CmdIndex = cmd_index;
    SDIO_CmdInitStructure.SDIO_Response = response_type;
    SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
    SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
    
    /* 发送命令 */
    SDIO_SendCommand(&SDIO_CmdInitStructure);
    
    return SDIO_OK;
}

/**
 * @brief 获取SDIO响应
 */
uint32_t SDIO_GetResponse(uint32_t response_register)
{
    return SDIO_GetResponse(response_register);
}

/**
 * @brief 配置SDIO数据传输
 */
SDIO_Status_t SDIO_ConfigData(uint32_t data_length, uint32_t block_size, 
                              uint32_t transfer_dir, uint32_t transfer_mode)
{
    SDIO_DataInitTypeDef SDIO_DataInitStructure;
    
    if (!g_sdio_initialized)
    {
        return SDIO_ERROR_NOT_INITIALIZED;
    }
    
    /* 配置数据传输 */
    SDIO_DataInitStructure.SDIO_DataTimeOut = 0xFFFFFFFF;
    SDIO_DataInitStructure.SDIO_DataLength = data_length;
    SDIO_DataInitStructure.SDIO_DataBlockSize = block_size;
    SDIO_DataInitStructure.SDIO_TransferDir = transfer_dir;
    SDIO_DataInitStructure.SDIO_TransferMode = transfer_mode;
    SDIO_DataInitStructure.SDIO_DPSM = SDIO_DPSM_Enable;
    
    /* 配置数据 */
    SDIO_DataConfig(&SDIO_DataInitStructure);
    
    return SDIO_OK;
}

/**
 * @brief 读取SDIO数据（直接调用SPL库函数）
 */
uint32_t SDIO_ReadData(void)
{
    return SDIO_ReadData();
}

/**
 * @brief 写入SDIO数据
 */
SDIO_Status_t SDIO_WriteData(uint32_t data)
{
    if (!g_sdio_initialized)
    {
        return SDIO_ERROR_NOT_INITIALIZED;
    }
    
    SDIO_WriteData(data);
    return SDIO_OK;
}

/**
 * @brief 获取SDIO FIFO计数（直接调用SPL库函数）
 */
uint32_t SDIO_GetFIFOCount(void)
{
    return SDIO_GetFIFOCount();
}

/**
 * @brief 获取SDIO标志状态（直接调用SPL库函数）
 */
uint8_t SDIO_GetFlagStatus(uint32_t flag)
{
    if (SDIO_GetFlagStatus(flag) != RESET)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief 清除SDIO标志（直接调用SPL库函数）
 */
SDIO_Status_t SDIO_ClearFlag(uint32_t flag)
{
    if (!g_sdio_initialized)
    {
        return SDIO_ERROR_NOT_INITIALIZED;
    }
    
    SDIO_ClearFlag(flag);
    return SDIO_OK;
}

/**
 * @brief 获取SDIO中断状态（直接调用SPL库函数）
 */
uint8_t SDIO_GetITStatus(uint32_t it)
{
    if (SDIO_GetITStatus(it) != RESET)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief 清除SDIO中断挂起位（直接调用SPL库函数）
 */
SDIO_Status_t SDIO_ClearITPendingBit(uint32_t it)
{
    if (!g_sdio_initialized)
    {
        return SDIO_ERROR_NOT_INITIALIZED;
    }
    
    SDIO_ClearITPendingBit(it);
    return SDIO_OK;
}

/**
 * @brief 检查SDIO是否已初始化
 */
uint8_t SDIO_IsInitialized(void)
{
    return g_sdio_initialized ? 1 : 0;
}

#endif /* CONFIG_MODULE_SDIO_ENABLED */

