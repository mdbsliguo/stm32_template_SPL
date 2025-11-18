/**
 * @file wwdg.c
 * @brief WWDG窗口看门狗模块实现
 */

#include "wwdg.h"
#include "board.h"
#include "config.h"
#include "error_handler.h"
#include "stm32f10x.h"
#include "stm32f10x_wwdg.h"
#include "stm32f10x_rcc.h"
#include <stdbool.h>

#if CONFIG_MODULE_WWDG_ENABLED

/* 预分频器映射 */
static const uint32_t wwdg_prescaler_map[] = {
    WWDG_Prescaler_1,  /* WWDG_PRESCALER_1 */
    WWDG_Prescaler_2,  /* WWDG_PRESCALER_2 */
    WWDG_Prescaler_4,  /* WWDG_PRESCALER_4 */
    WWDG_Prescaler_8,  /* WWDG_PRESCALER_8 */
};

/* 初始化标志 */
static bool g_wwdg_initialized = false;

/**
 * @brief WWDG初始化
 */
WWDG_Status_t WWDG_Init(WWDG_Prescaler_t prescaler, uint8_t window_value, uint8_t counter)
{
    if (g_wwdg_initialized)
    {
        ERROR_HANDLER_Report(ERROR_BASE_IWDG, __FILE__, __LINE__, "WWDG already initialized");
        return WWDG_ERROR_ALREADY_INITIALIZED;
    }
    
    if (prescaler > WWDG_PRESCALER_8)
    {
        return WWDG_ERROR_INVALID_PARAM;
    }
    
    if (window_value > 0x7F)
    {
        return WWDG_ERROR_INVALID_PARAM;
    }
    
    if (counter < 0x40 || counter > 0x7F)
    {
        return WWDG_ERROR_INVALID_PARAM;
    }
    
    if (window_value >= counter)
    {
        ERROR_HANDLER_Report(ERROR_BASE_IWDG, __FILE__, __LINE__, "Window value must be less than counter value");
        return WWDG_ERROR_INVALID_PARAM;
    }
    
    /* 使能WWDG时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_WWDG, ENABLE);
    
    /* 配置预分频器 */
    WWDG_SetPrescaler(wwdg_prescaler_map[prescaler]);
    
    /* 配置窗口值 */
    WWDG_SetWindowValue(window_value);
    
    /* 使能WWDG并设置计数器 */
    WWDG_Enable(counter);
    
    g_wwdg_initialized = true;
    
    return WWDG_OK;
}

/**
 * @brief WWDG反初始化
 */
WWDG_Status_t WWDG_Deinit(void)
{
    if (!g_wwdg_initialized)
    {
        return WWDG_ERROR_NOT_INITIALIZED;
    }
    
    /* 禁用WWDG（通过复位） */
    WWDG_DeInit();
    
    g_wwdg_initialized = false;
    
    return WWDG_OK;
}

/**
 * @brief 喂狗（刷新计数器）
 */
WWDG_Status_t WWDG_Feed(uint8_t counter)
{
    uint8_t current_counter;
    
    if (!g_wwdg_initialized)
    {
        return WWDG_ERROR_NOT_INITIALIZED;
    }
    
    if (counter < 0x40 || counter > 0x7F)
    {
        return WWDG_ERROR_INVALID_PARAM;
    }
    
    /* 读取当前计数器值 */
    current_counter = WWDG_GetCounter();
    
    /* 检查是否在窗口内（计数器值必须大于窗口值且小于0x7F） */
    /* 注意：窗口值的含义是：当计数器值小于窗口值时，不允许喂狗 */
    /* 当计数器值在窗口值和0x7F之间时，允许喂狗 */
    /* 当计数器值达到0x40时，会产生复位 */
    
    /* 设置新的计数器值 */
    WWDG_SetCounter(counter);
    
    return WWDG_OK;
}

/**
 * @brief 使能WWDG中断
 */
WWDG_Status_t WWDG_EnableInterrupt(void)
{
    if (!g_wwdg_initialized)
    {
        return WWDG_ERROR_NOT_INITIALIZED;
    }
    
    WWDG_EnableIT();
    
    /* 配置NVIC */
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = WWDG_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    return WWDG_OK;
}

/**
 * @brief 禁用WWDG中断
 */
WWDG_Status_t WWDG_DisableInterrupt(void)
{
    if (!g_wwdg_initialized)
    {
        return WWDG_ERROR_NOT_INITIALIZED;
    }
    
    /* 禁用NVIC中断 */
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = WWDG_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = DISABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    return WWDG_OK;
}

/**
 * @brief 检查WWDG标志
 */
uint8_t WWDG_CheckFlag(void)
{
    if (!g_wwdg_initialized)
    {
        return 0;
    }
    
    return (WWDG_GetFlagStatus() == SET) ? 1 : 0;
}

/**
 * @brief 清除WWDG标志
 */
WWDG_Status_t WWDG_ClearFlag(void)
{
    if (!g_wwdg_initialized)
    {
        return WWDG_ERROR_NOT_INITIALIZED;
    }
    
    WWDG_ClearFlag();
    
    return WWDG_OK;
}

/**
 * @brief 检查WWDG是否已初始化
 */
uint8_t WWDG_IsInitialized(void)
{
    return g_wwdg_initialized ? 1 : 0;
}

/**
 * @brief 获取当前计数器值
 */
uint8_t WWDG_GetCounter(void)
{
    if (!g_wwdg_initialized)
    {
        return 0;
    }
    
    /* 读取WWDG计数器值（从WWDG->CR寄存器） */
    return (WWDG->CR & 0x7F);
}

#endif /* CONFIG_MODULE_WWDG_ENABLED */

