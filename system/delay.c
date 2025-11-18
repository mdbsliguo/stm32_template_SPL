/**
 * @file delay.c
 * @brief 延时模块实现（阻塞式+非阻塞式，基于base_TIM2）
 * @version 2.0.0
 * @date 2024-01-01
 * @note 基于base_TIM2模块，确保频率变化时1秒永远是1秒
 */

#include "delay.h"
#include "base_TIM2.h"  /* 基于base_TIM2模块 */
#include "stm32f10x.h"
#include "misc.h"  /* 需要SysTick_CLKSourceConfig */
#include <stdbool.h>  /* 需要bool类型 */
#include <limits.h>  /* 用于UINT32_MAX */

/* 静态变量 */
static uint8_t fac_us = 0;  /**< 微秒延时系数 = SystemCoreClock/8000000（用于阻塞式延时） */
static uint16_t fac_ms = 0; /**< 毫秒延时系数 = SystemCoreClock/8000（用于阻塞式延时） */
static bool g_delay_initialized = false;  /**< 延时模块初始化标志 */

/**
 * @brief 初始化延时模块（基于base_TIM2）
 * @note 初始化base_TIM2（如果未初始化），然后计算阻塞式延时的系数
 * @note 阻塞式延时基于SysTick寄存器，非阻塞式延时基于base_TIM2的g_task_tick
 */
void Delay_Init(void)
{
    /* 防止重复初始化 */
    if (g_delay_initialized)
    {
        return;
    }
    
        /* 确保base_TIM2已初始化 */
        if (!BaseTimer_IsInitialized())
        {
            BaseTimer_Init();
        }
    
    /* 自动从系统核心时钟寄存器获取频率 */
    uint32_t core_clk = SystemCoreClock;
    
    /* 计算延时系数（因为时钟源是HCLK/8，所以除以8） */
    fac_us = core_clk / 8000000;  /* 8MHz下 = 1, 72MHz下 = 9 */
    fac_ms = core_clk / 8000;     /* 8MHz下 = 1000, 72MHz下 = 9000 */
    
    /* 增加时钟有效性检查（防止时钟配置错误） */
    if (fac_us == 0 || fac_ms == 0)
    {
        /* 严重错误：时钟频率过低 */
        while (1)
            ;
    }
    
    g_delay_initialized = true;
}

/**
 * @brief 重新配置延时模块（频率切换时调用）
 * @param[in] new_freq 新的系统频率（Hz）
 * @note 频率切换时，调用base_TIM2的重新配置，然后更新阻塞式延时的系数
 * @note base_TIM2会自动保持1ms中断间隔不变，确保1秒永远是1秒
 */
void Delay_Reconfig(uint32_t new_freq)
{
    /* 检查是否已初始化 */
    if (!g_delay_initialized)
    {
        /* 未初始化，直接调用Delay_Init() */
        Delay_Init();
        return;
    }

    /* 重新配置base_TIM2（会自动保持1ms中断间隔） */
    BaseTimer_Reconfig(new_freq);
    
    /* 重新计算阻塞式延时的系数 */
    fac_us = new_freq / 8000000;
    fac_ms = new_freq / 8000;
    
    /* 检查有效性 */
    if (fac_us == 0 || fac_ms == 0)
    {
        /* 严重错误：时钟频率过低 */
        while (1)
            ;
    }
}

/**
 * @brief 微秒级精确延时（阻塞式，基于SysTick寄存器）
 * @param[in] us 延时微秒数（自动限制最大值，防止溢出）
 * @warning 此函数为阻塞式，执行期间CPU不处理其他任务
 * @note 用于精确延时（<1ms），频率切换时自动适配
 */
void Delay_us(uint32_t us)
{
    /* 超时保护：计算最大允许值（2^24-1）/ fac_us */
    const uint32_t max_us = (0xFFFFFFUL / fac_us) - 1;
    if (us > max_us)
    {
        us = max_us;  /* 超限则自动限制到最大值 */
    }
    
    uint32_t temp;
    SysTick->LOAD = us * fac_us;
    SysTick->VAL = 0x00;
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
    
    do
    {
        temp = SysTick->CTRL;
    } while ((temp & 0x01) && !(temp & (1 << 16)));
    
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
    SysTick->VAL = 0x00;
}

/**
 * @brief 毫秒级精确延时（阻塞式，基于SysTick寄存器）
 * @param[in] ms 延时毫秒数（超限自动分段处理，防止溢出）
 * @warning 此函数为阻塞式，执行期间CPU不处理其他任务
 * @note 用于精确延时（<100ms），频率切换时自动适配
 */
void Delay_ms(uint32_t ms)
{
    /* 超时保护：计算最大允许值（约1864ms @72MHz） */
    const uint32_t max_ms = (0xFFFFFFUL / fac_ms) - 1;
    if (ms > max_ms)
    {
        /* 自动分段延时 */
        while (ms > max_ms)
        {
            Delay_ms(max_ms);
            ms -= max_ms;
        }
    }
    
    uint32_t temp;
    SysTick->LOAD = ms * fac_ms;
    SysTick->VAL = 0x00;
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
    
    do
    {
        temp = SysTick->CTRL;
    } while ((temp & 0x01) && !(temp & (1 << 16)));
    
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
    SysTick->VAL = 0x00;
}

/**
 * @brief 毫秒级非阻塞延时（基于base_TIM2，动态降频自适应）
 * @param[in] start_tick 开始时间（BaseTimer_GetTick()的返回值）
 * @param[in] delay_ms 延时毫秒数
 * @return 1=延时完成，0=延时未完成
 * @note 基于base_TIM2，频率切换时自动适配（1秒永远是1秒）
 * @note 用于长时间延时（>100ms），不阻塞CPU
 * 
 * @example
 * uint32_t start = Delay_GetTick();
 * while (!Delay_ms_nonblock(start, 1000))  // 延时1秒
 * {
 *     // 执行其他任务
 * }
 */
uint8_t Delay_ms_nonblock(uint32_t start_tick, uint32_t delay_ms)
{
    uint32_t current_tick = BaseTimer_GetTick();
    uint32_t elapsed = Delay_GetElapsed(current_tick, start_tick);
    return (elapsed >= delay_ms) ? 1 : 0;
}

/**
 * @brief 计算时间差（处理溢出）
 */
uint32_t Delay_GetElapsed(uint32_t current_tick, uint32_t previous_tick)
{
    /* 如果 previous_tick == 0，表示从未设置过，返回一个很大的值（表示已经过了很长时间） */
    if (previous_tick == 0)
    {
        return UINT32_MAX;  /* 表示已经过了很长时间，应该立即执行 */
    }
    
    /* 处理溢出情况 */
    if (current_tick >= previous_tick)
    {
        return current_tick - previous_tick;
    }
    else
    {
        /* 溢出情况：从最大值绕回0 */
        return (UINT32_MAX - previous_tick + current_tick + 1);
    }
}

/**
 * @brief 获取当前tick（用于非阻塞延时）
 * @return 当前tick值（代表真实时间，毫秒）
 * @note 基于base_TIM2，频率切换时自动适配
 */
uint32_t Delay_GetTick(void)
{
    return BaseTimer_GetTick();
}

/**
 * @brief 秒级延时（阻塞式，循环调用Delay_ms）
 * @param[in] s 延时秒数
 * @warning 长时间阻塞，实时性要求高的场景禁用
 * @note 建议使用Delay_ms_nonblock()实现非阻塞延时
 */
void Delay_s(uint32_t s)
{
    while (s--)
    {
        Delay_ms(1000);
    }
}
