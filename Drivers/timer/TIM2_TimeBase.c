/**
 * @file TIM2_TimeBase.c
 * @brief TIM2时间基准模块实现（时间基准，动态降频自适应）
 * @version 2.0.0
 * @date 2024-01-01
 * @note 使用TIM2外设定时器，确保调频时1ms中断始终准确
 * @note delay模块基于此定时器，确保频率变化时1秒永远是1秒
 */

#include "TIM2_TimeBase.h"
#include "stm32f10x.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_rcc.h"
#include "system_stm32f10x.h"  /* 需要SystemCoreClockUpdate */
#include <stdbool.h>

/* 任务tick计数器（TIM2中断中递增，代表真实时间，毫秒） */
volatile uint32_t g_task_tick = 0;

/* 模块状态 */
static bool g_tim2_timebase_initialized = false;  /**< TIM2时间基准初始化标志 */

/**
 * @brief 计算TIM2的PSC和ARR值，使中断频率为1ms（1000Hz）
 * @param[in] tim2_clk TIM2的时钟频率（Hz）
 * @param[out] psc 预分频器值
 * @param[out] arr 自动重装载值
 * @return 0=成功，1=失败
 * @note TIM2时钟 = APB1时钟（如果APB1分频>1，则TIM2时钟 = APB1时钟 * 2）
 * @note 中断频率 = TIM2时钟 / (PSC + 1) / (ARR + 1) = 1000Hz
 */
static uint8_t TIM2_TimeBase_CalculateParams(uint32_t tim2_clk, uint16_t *psc, uint16_t *arr)
{
    /* 目标：1ms中断，即1000Hz */
    const uint32_t target_freq = 1000;
    
    /* 计算所需的计数值 */
    uint32_t total_count = tim2_clk / target_freq;
    
    /* 检查是否超出范围 */
    if (total_count == 0 || total_count > 0xFFFFFFFF)
    {
        return 1;  /* 失败 */
    }
    
    /* 策略：PSC尽量大，但不超过65535，ARR = total_count / (PSC + 1) - 1 */
    /* 为了简化，固定PSC = 71（这样72MHz/72=1MHz，ARR=1000-1=999） */
    /* 但为了适应不同频率，动态计算PSC */
    
    /* 尝试找到一个合适的PSC值，使得ARR在合理范围内（100-65535） */
    uint16_t best_psc = 0;
    uint16_t best_arr = 0;
    uint16_t psc_candidate;
    
    /* 从PSC=71开始尝试（适合72MHz） */
    for (psc_candidate = 71; psc_candidate <= 1000; psc_candidate++)
    {
        uint32_t arr_candidate = total_count / (psc_candidate + 1);
        if (arr_candidate > 0 && arr_candidate <= 65535)
        {
            best_psc = psc_candidate;
            best_arr = (uint16_t)(arr_candidate - 1);
            break;
        }
    }
    
    /* 如果没找到，尝试更小的PSC */
    if (best_psc == 0)
    {
        for (psc_candidate = 1; psc_candidate < 71; psc_candidate++)
        {
            uint32_t arr_candidate = total_count / (psc_candidate + 1);
            if (arr_candidate > 0 && arr_candidate <= 65535)
            {
                best_psc = psc_candidate;
                best_arr = (uint16_t)(arr_candidate - 1);
                break;
            }
        }
    }
    
    if (best_psc == 0)
    {
        return 1;  /* 失败 */
    }
    
    *psc = best_psc;
    *arr = best_arr;
    
    return 0;  /* 成功 */
}

/**
 * @brief 获取TIM2的实际时钟频率
 * @return TIM2的时钟频率（Hz）
 * @note TIM2挂载在APB1总线上
 * @note 如果APB1分频>1，TIM2时钟 = APB1时钟 * 2
 */
static uint32_t TIM2_TimeBase_GetTIM2Clock(void)
{
    uint32_t apb1_clk;
    uint32_t tim2_clk;
    
    /* 更新SystemCoreClock */
    SystemCoreClockUpdate();
    
    /* 获取APB1时钟 */
    uint32_t apb1_prescaler = RCC->CFGR & RCC_CFGR_PPRE1;
    apb1_prescaler = (apb1_prescaler >> 8) & 0x07;
    
    if (apb1_prescaler < 4)
    {
        /* APB1没有分频或分频为1 */
        apb1_clk = SystemCoreClock;
    }
    else
    {
        /* APB1有分频，计算实际频率 */
        uint8_t presc_table[] = {0, 0, 0, 0, 1, 2, 3, 4};
        apb1_clk = SystemCoreClock >> presc_table[apb1_prescaler];
    }
    
    /* 如果APB1分频>1，TIM2时钟 = APB1时钟 * 2 */
    if (apb1_prescaler >= 4)
    {
        tim2_clk = apb1_clk * 2;
    }
    else
    {
        tim2_clk = apb1_clk;
    }
    
    return tim2_clk;
}

/**
 * @brief 初始化TIM2时间基准（配置1ms中断，作为时间基准）
 * @return error_code_t 错误码（TIM2_TIMEBASE_OK表示成功）
 * @note 使用TIM2外设定时器，配置为1ms中断一次
 * @note g_task_tick在TIM2_IRQHandler中递增，代表真实时间（毫秒）
 * @note 频率切换时调用TIM2_TimeBase_Reconfig()重新配置，保持1ms中断间隔不变
 * @note 历史遗留模块，待重构
 */
error_code_t TIM2_TimeBase_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    uint16_t psc, arr;
    uint32_t tim2_clk;
    
    /* 防止重复初始化 */
    if (g_tim2_timebase_initialized)
    {
        return TIM2_TIMEBASE_ERROR_ALREADY_INITIALIZED;
    }
    
    /* 等待时钟稳定 */
    volatile uint32_t wait = 1000;
    while (wait--);
    
    /* 使能TIM2时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    
    /* 获取TIM2时钟频率 */
    tim2_clk = TIM2_TimeBase_GetTIM2Clock();
    
    /* 计算PSC和ARR */
    if (TIM2_TimeBase_CalculateParams(tim2_clk, &psc, &arr) != 0)
    {
        /* 配置失败，返回错误码 */
        return TIM2_TIMEBASE_ERROR_CALC_FAILED;
    }
    
    /* 配置TIM2 */
    TIM_TimeBaseStructure.TIM_Period = arr;           /* 自动重装载值 */
    TIM_TimeBaseStructure.TIM_Prescaler = psc;         /* 预分频器 */
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;  /* 时钟分频 */
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  /* 向上计数 */
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    
    /* 使能TIM2更新中断 */
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    
    /* 配置NVIC */
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;  /* 最高优先级 */
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    /* 使能TIM2 */
    TIM_Cmd(TIM2, ENABLE);
    
    /* 初始化g_task_tick */
    g_task_tick = 0;
    
    g_tim2_timebase_initialized = true;
    
    return TIM2_TIMEBASE_OK;
}

/**
 * @brief 重新配置TIM2时间基准（频率切换时调用，保持1ms中断间隔不变）
 * @param[in] new_freq 新的系统频率（Hz）
 * @return error_code_t 错误码（TIM2_TIMEBASE_OK表示成功）
 * @note 频率切换时，重新计算TIM2的PSC和ARR，保持1ms中断间隔不变
 * @note 这样g_task_tick始终代表真实时间（毫秒），无论频率如何变化
 * @note 1秒永远是1秒，无论频率如何变化
 * @note 历史遗留模块，待重构
 */
error_code_t TIM2_TimeBase_Reconfig(uint32_t new_freq)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    uint16_t psc, arr;
    uint32_t tim2_clk;
    
    /* 检查是否已初始化 */
    if (!g_tim2_timebase_initialized)
    {
        /* 未初始化，直接调用TIM2_TimeBase_Init() */
        return TIM2_TimeBase_Init();
    }
    
    /* 更新SystemCoreClock */
    SystemCoreClock = new_freq;
    
    /* 禁用TIM2中断 */
    TIM_ITConfig(TIM2, TIM_IT_Update, DISABLE);
    TIM_Cmd(TIM2, DISABLE);
    
    /* 等待TIM2完全停止 */
    volatile uint32_t wait = 100;
    while (wait--);
    
    /* 获取新的TIM2时钟频率 */
    tim2_clk = TIM2_TimeBase_GetTIM2Clock();
    
    /* 重新计算PSC和ARR */
    if (TIM2_TimeBase_CalculateParams(tim2_clk, &psc, &arr) != 0)
    {
        /* 配置失败，返回错误码 */
        return TIM2_TIMEBASE_ERROR_CALC_FAILED;
    }
    
    /* 重新配置TIM2 */
    TIM_TimeBaseStructure.TIM_Period = arr;
    TIM_TimeBaseStructure.TIM_Prescaler = psc;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    
    /* 清除更新标志 */
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    
    /* 重新使能TIM2中断和定时器 */
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
    
    return TIM2_TIMEBASE_OK;
}

/**
 * @brief 获取当前tick（代表真实时间，毫秒）
 * @return 当前g_task_tick值（代表真实时间，毫秒）
 * @note g_task_tick在TIM2_IRQHandler中递增，频率切换时自动适配
 */
uint32_t TIM2_TimeBase_GetTick(void)
{
    return g_task_tick;
}

/**
 * @brief 检查TIM2时间基准是否已初始化
 * @return true=已初始化，false=未初始化
 */
bool TIM2_TimeBase_IsInitialized(void)
{
    return g_tim2_timebase_initialized;
}
