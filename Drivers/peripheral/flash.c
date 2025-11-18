/**
 * @file flash.c
 * @brief FLASH驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的FLASH驱动，支持Flash编程/擦除、选项字节管理、Flash保护等功能
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_FLASH_ENABLED

/* Include our header */
#include "flash.h"

#include "delay.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_flash.h"
#include <stdbool.h>
#include <stddef.h>

/* 默认超时时间（毫秒） */
#define FLASH_DEFAULT_TIMEOUT_MS  5000

/**
 * @brief 设置FLASH延迟周期（别名函数，避免与SPL库函数名冲突）
 */
FLASH_Status_t FLASH_ConfigLatency(FLASH_Latency_t latency)
{
    FLASH_Latency_TypeDef flash_latency;
    
    if (latency > 2)
    {
        return FLASH_ERROR_INVALID_PARAM;
    }
    
    /* 转换延迟周期枚举值 */
    switch (latency)
    {
        case FLASH_LATENCY_0: flash_latency = FLASH_Latency_0; break;
        case FLASH_LATENCY_1: flash_latency = FLASH_Latency_1; break;
        case FLASH_LATENCY_2: flash_latency = FLASH_Latency_2; break;
        default: return FLASH_ERROR_INVALID_PARAM;
    }
    
    /* 设置FLASH延迟周期（直接调用SPL库函数） */
    FLASH_SetLatency(flash_latency);
    
    return FLASH_OK;
}

/**
 * @brief 使能FLASH预取缓冲器
 */
FLASH_Status_t FLASH_EnablePrefetchBuffer(void)
{
    FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
    return FLASH_OK;
}

/**
 * @brief 禁用FLASH预取缓冲器
 */
FLASH_Status_t FLASH_DisablePrefetchBuffer(void)
{
    FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Disable);
    return FLASH_OK;
}

/**
 * @brief 使能FLASH半周期访问
 */
FLASH_Status_t FLASH_EnableHalfCycleAccess(void)
{
    FLASH_HalfCycleAccessCmd(FLASH_HalfCycleAccess_Enable);
    return FLASH_OK;
}

/**
 * @brief 禁用FLASH半周期访问
 */
FLASH_Status_t FLASH_DisableHalfCycleAccess(void)
{
    FLASH_HalfCycleAccessCmd(FLASH_HalfCycleAccess_Disable);
    return FLASH_OK;
}

/**
 * @brief 解锁FLASH（直接调用SPL库函数）
 */
FLASH_Status_t FLASH_UnlockFlash(void)
{
    FLASH_Unlock();
    return FLASH_OK;
}

/**
 * @brief 锁定FLASH（直接调用SPL库函数）
 */
FLASH_Status_t FLASH_LockFlash(void)
{
    FLASH_Lock();
    return FLASH_OK;
}

/**
 * @brief 擦除FLASH页
 */
FLASH_Status_t FLASH_ErasePage(uint32_t page_address)
{
    FLASH_Status status;
    
    /* 擦除页 */
    status = FLASH_ErasePage(page_address);
    
    /* 转换SPL库状态码 */
    if (status == FLASH_COMPLETE)
    {
        return FLASH_OK;
    }
    else if (status == FLASH_ERROR_PG)
    {
        return ERROR_FLASH_ERROR_PG;
    }
    else if (status == FLASH_ERROR_WRP)
    {
        return ERROR_FLASH_ERROR_WRP;
    }
    else if (status == FLASH_TIMEOUT)
    {
        return FLASH_ERROR_TIMEOUT;
    }
    else
    {
        return FLASH_ERROR_BUSY;
    }
}

/**
 * @brief 擦除所有FLASH页
 */
FLASH_Status_t FLASH_EraseAllPages(void)
{
    FLASH_Status status;
    
    /* 擦除所有页 */
    status = FLASH_EraseAllPages();
    
    /* 转换SPL库状态码 */
    if (status == FLASH_COMPLETE)
    {
        return FLASH_OK;
    }
    else if (status == FLASH_ERROR_PG)
    {
        return ERROR_FLASH_ERROR_PG;
    }
    else if (status == FLASH_ERROR_WRP)
    {
        return ERROR_FLASH_ERROR_WRP;
    }
    else if (status == FLASH_TIMEOUT)
    {
        return FLASH_ERROR_TIMEOUT;
    }
    else
    {
        return FLASH_ERROR_BUSY;
    }
}

/**
 * @brief 编程FLASH（半字）
 */
FLASH_Status_t FLASH_ProgramHalfWord(uint32_t address, uint16_t data)
{
    FLASH_Status status;
    
    /* 检查地址对齐 */
    if (address & 0x01)
    {
        return FLASH_ERROR_INVALID_PARAM;
    }
    
    /* 编程半字 */
    status = FLASH_ProgramHalfWord(address, data);
    
    /* 转换SPL库状态码 */
    if (status == FLASH_COMPLETE)
    {
        return FLASH_OK;
    }
    else if (status == FLASH_ERROR_PG)
    {
        return ERROR_FLASH_ERROR_PG;
    }
    else if (status == FLASH_ERROR_WRP)
    {
        return ERROR_FLASH_ERROR_WRP;
    }
    else if (status == FLASH_TIMEOUT)
    {
        return FLASH_ERROR_TIMEOUT;
    }
    else
    {
        return FLASH_ERROR_BUSY;
    }
}

/**
 * @brief 编程FLASH（字）
 */
FLASH_Status_t FLASH_ProgramWord(uint32_t address, uint32_t data)
{
    FLASH_Status status;
    
    /* 检查地址对齐 */
    if (address & 0x03)
    {
        return FLASH_ERROR_INVALID_PARAM;
    }
    
    /* 编程字 */
    status = FLASH_ProgramWord(address, data);
    
    /* 转换SPL库状态码 */
    if (status == FLASH_COMPLETE)
    {
        return FLASH_OK;
    }
    else if (status == FLASH_ERROR_PG)
    {
        return ERROR_FLASH_ERROR_PG;
    }
    else if (status == FLASH_ERROR_WRP)
    {
        return ERROR_FLASH_ERROR_WRP;
    }
    else if (status == FLASH_TIMEOUT)
    {
        return FLASH_ERROR_TIMEOUT;
    }
    else
    {
        return FLASH_ERROR_BUSY;
    }
}

/**
 * @brief 读取FLASH数据（半字）
 */
uint16_t FLASH_ReadHalfWord(uint32_t address)
{
    return *((volatile uint16_t *)address);
}

/**
 * @brief 读取FLASH数据（字）
 */
uint32_t FLASH_ReadWord(uint32_t address)
{
    return *((volatile uint32_t *)address);
}

/**
 * @brief 获取FLASH状态
 */
FLASH_Status_t FLASH_GetStatus(void)
{
    FLASH_Status status;
    
    status = FLASH_GetStatus();
    
    /* 转换SPL库状态码 */
    if (status == FLASH_COMPLETE)
    {
        return FLASH_OK;
    }
    else if (status == FLASH_ERROR_PG)
    {
        return ERROR_FLASH_ERROR_PG;
    }
    else if (status == FLASH_ERROR_WRP)
    {
        return ERROR_FLASH_ERROR_WRP;
    }
    else if (status == FLASH_TIMEOUT)
    {
        return FLASH_ERROR_TIMEOUT;
    }
    else
    {
        return FLASH_ERROR_BUSY;
    }
}

/**
 * @brief 等待FLASH操作完成
 */
FLASH_Status_t FLASH_WaitForLastOperation(void)
{
    return FLASH_GetStatus();
}

/**
 * @brief 等待FLASH操作完成（带超时）
 */
FLASH_Status_t FLASH_WaitForLastOperationWithTimeout(uint32_t timeout_ms)
{
    uint32_t start_tick = Delay_GetTick();
    
    while (FLASH_GetStatus() == FLASH_BUSY)
    {
        uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
        if (elapsed > timeout_ms)
        {
            return FLASH_ERROR_TIMEOUT;
        }
    }
    
    return FLASH_GetStatus();
}

/**
 * @brief 清除FLASH状态标志（直接调用SPL库函数）
 */
FLASH_Status_t FLASH_ClearStatusFlag(uint32_t flag)
{
    FLASH_ClearFlag(flag);
    return FLASH_OK;
}

/* ========== 选项字节功能实现 ========== */

/**
 * @brief 解锁选项字节（直接调用SPL库函数）
 */
FLASH_Status_t FLASH_UnlockOptionByte(void)
{
    FLASH_UnlockOptionBytes();
    return FLASH_OK;
}

/**
 * @brief 锁定选项字节（直接调用SPL库函数）
 */
FLASH_Status_t FLASH_LockOptionByte(void)
{
    FLASH_LockOptionBytes();
    return FLASH_OK;
}

/**
 * @brief 擦除选项字节
 */
FLASH_Status_t FLASH_EraseOptionBytes(void)
{
    FLASH_Status status;
    
    status = FLASH_EraseOptionBytes();
    
    /* 转换SPL库状态码 */
    if (status == FLASH_COMPLETE)
    {
        return FLASH_OK;
    }
    else if (status == FLASH_ERROR_PG)
    {
        return ERROR_FLASH_ERROR_PG;
    }
    else if (status == FLASH_ERROR_WRP)
    {
        return ERROR_FLASH_ERROR_WRP;
    }
    else if (status == FLASH_TIMEOUT)
    {
        return FLASH_ERROR_TIMEOUT;
    }
    else
    {
        return FLASH_ERROR_BUSY;
    }
}

/**
 * @brief 编程选项字节
 */
FLASH_Status_t FLASH_ProgramOptionByteData(uint32_t address, uint8_t data)
{
    FLASH_Status status;
    
    status = FLASH_ProgramOptionByteData(address, data);
    
    /* 转换SPL库状态码 */
    if (status == FLASH_COMPLETE)
    {
        return FLASH_OK;
    }
    else if (status == FLASH_ERROR_PG)
    {
        return ERROR_FLASH_ERROR_PG;
    }
    else if (status == FLASH_ERROR_WRP)
    {
        return ERROR_FLASH_ERROR_WRP;
    }
    else if (status == FLASH_TIMEOUT)
    {
        return FLASH_ERROR_TIMEOUT;
    }
    else
    {
        return FLASH_ERROR_BUSY;
    }
}

/**
 * @brief 使能写保护
 */
FLASH_Status_t FLASH_EnableWriteProtection(uint32_t pages)
{
    FLASH_Status status;
    
    status = FLASH_EnableWriteProtection(pages);
    
    /* 转换SPL库状态码 */
    if (status == FLASH_COMPLETE)
    {
        return FLASH_OK;
    }
    else if (status == FLASH_ERROR_PG)
    {
        return ERROR_FLASH_ERROR_PG;
    }
    else if (status == FLASH_ERROR_WRP)
    {
        return ERROR_FLASH_ERROR_WRP;
    }
    else if (status == FLASH_TIMEOUT)
    {
        return FLASH_ERROR_TIMEOUT;
    }
    else
    {
        return FLASH_ERROR_BUSY;
    }
}

/**
 * @brief 禁用写保护
 */
FLASH_Status_t FLASH_DisableWriteProtection(void)
{
    /* 禁用写保护需要擦除选项字节 */
    return FLASH_EraseOptionBytes();
}

/**
 * @brief 读取用户选项字节（直接调用SPL库函数）
 */
uint16_t FLASH_ReadUserOptionByte(void)
{
    return (uint16_t)FLASH_GetUserOptionByte();
}

/**
 * @brief 读取写保护选项字节（直接调用SPL库函数）
 */
uint16_t FLASH_ReadWriteProtectionOptionByte(void)
{
    return (uint16_t)FLASH_GetWriteProtectionOptionByte();
}

/**
 * @brief 读取读保护状态
 */
uint8_t FLASH_GetReadProtectionStatus(void)
{
    if (FLASH_GetReadOutProtectionStatus() != RESET)
    {
        return 1;
    }
    return 0;
}

#endif /* CONFIG_MODULE_FLASH_ENABLED */

