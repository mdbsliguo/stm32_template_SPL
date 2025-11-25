/**
 * @file TIM_sw.c
 * @brief 软件定时器模块实现（基于TIM2_TimeBase）
 * @version 1.0.0
 * @date 2024-01-01
 * @note 基于TIM2_TimeBase的1ms tick，提供多个软件定时器实例
 * @note 支持单次和周期模式，支持回调函数
 * 
 * @details 实现说明：
 * - 支持最多16个软件定时器实例（TIM_SW_MAX_COUNT）
 * - 基于TIM2_TimeBase的1ms tick，频率切换时自动适配
 * - 支持单次和周期模式，支持回调函数和用户数据
 * - 支持暂停/恢复功能，暂停时保持剩余时间
 * - 自动处理32位整数溢出（约49.7天后溢出）
 * - TIM_SW_Update()在TIM2中断中调用，检查所有定时器并触发回调
 */

#include "TIM_sw.h"
#include "TIM2_TimeBase.h"
#include <stdbool.h>
#include <string.h>
#include <limits.h>  /* 用于UINT32_MAX */

/* 软件定时器最大数量 */
#define TIM_SW_MAX_COUNT         16

/* 定时器状态 */
typedef struct
{
    bool used;                      /**< 是否已使用 */
    bool running;                   /**< 是否正在运行 */
    bool paused;                    /**< 是否已暂停 */
    uint32_t period_ms;             /**< 定时器周期（毫秒） */
    uint32_t start_tick;            /**< 启动时的tick值 */
    uint32_t pause_remaining_ms;    /**< 暂停时的剩余时间（用于暂停/恢复） */
    TIM_SW_Mode_t mode;             /**< 定时器模式 */
    TIM_SW_Callback_t callback;     /**< 回调函数 */
    void *user_data;                /**< 用户数据指针 */
} TIM_SW_Timer_t;

/* 定时器数组 */
static TIM_SW_Timer_t g_timers[TIM_SW_MAX_COUNT];

/* 模块状态 */
static bool g_tim_sw_initialized = false;

/**
 * @brief 计算时间差（处理溢出）
 * @param[in] current_tick 当前tick值
 * @param[in] previous_tick 之前的tick值
 * @return 时间差（毫秒）
 * @note 处理uint32_t溢出情况（约49.7天后溢出）
 */
static uint32_t TIM_SW_GetElapsed(uint32_t current_tick, uint32_t previous_tick)
{
    if (previous_tick == 0)
    {
        /* 如果 previous_tick == 0，表示从未设置过，返回一个很大的值（表示已经过了很长时间） */
        return UINT32_MAX;
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
 * @brief 初始化软件定时器模块
 * @note 必须在TIM2_TimeBase初始化之后调用
 */
void TIM_SW_Init(void)
{
    if (g_tim_sw_initialized)
    {
        return;
    }
    
    /* 确保TIM2_TimeBase已初始化 */
    if (!TIM2_TimeBase_IsInitialized())
    {
        TIM2_TimeBase_Init();
    }
    
    /* 清零定时器数组 */
    memset(g_timers, 0, sizeof(g_timers));
    
    g_tim_sw_initialized = true;
}

/**
 * @brief 创建软件定时器
 * @param[in] period_ms 定时器周期（毫秒）
 * @param[in] mode 定时器模式
 * @param[in] callback 回调函数
 * @param[in] user_data 用户数据指针
 * @return 定时器句柄
 */
TIM_SW_Handle_t TIM_SW_Create(uint32_t period_ms, TIM_SW_Mode_t mode, TIM_SW_Callback_t callback, void *user_data)
{
    uint8_t i;
    
    if (!g_tim_sw_initialized)
    {
        return TIM_SW_HANDLE_INVALID;
    }
    
    if (period_ms == 0)
    {
        return TIM_SW_HANDLE_INVALID;
    }
    
    /* 查找空闲的定时器 */
    for (i = 0; i < TIM_SW_MAX_COUNT; i++)
    {
        if (!g_timers[i].used)
        {
            g_timers[i].used = true;
            g_timers[i].running = false;
            g_timers[i].paused = false;
            g_timers[i].period_ms = period_ms;
            g_timers[i].start_tick = 0;
            g_timers[i].pause_remaining_ms = 0;
            g_timers[i].mode = mode;
            g_timers[i].callback = callback;
            g_timers[i].user_data = user_data;
            return i;
        }
    }
    
    /* 没有空闲的定时器 */
    return TIM_SW_HANDLE_INVALID;
}

/**
 * @brief 启动软件定时器
 * @param[in] handle 定时器句柄
 * @return 0=成功，1=失败
 */
uint8_t TIM_SW_Start(TIM_SW_Handle_t handle)
{
    if (handle >= TIM_SW_MAX_COUNT)
    {
        return 1;
    }
    
    if (!g_timers[handle].used)
    {
        return 1;
    }
    
    g_timers[handle].running = true;
    g_timers[handle].paused = false;
    g_timers[handle].start_tick = TIM2_TimeBase_GetTick();
    g_timers[handle].pause_remaining_ms = 0;
    
    return 0;
}

/**
 * @brief 停止软件定时器
 * @param[in] handle 定时器句柄
 * @return 0=成功，1=失败
 */
uint8_t TIM_SW_Stop(TIM_SW_Handle_t handle)
{
    if (handle >= TIM_SW_MAX_COUNT)
    {
        return 1;
    }
    
    if (!g_timers[handle].used)
    {
        return 1;
    }
    
    g_timers[handle].running = false;
    g_timers[handle].paused = false;
    g_timers[handle].pause_remaining_ms = 0;
    
    return 0;
}

/**
 * @brief 重启软件定时器
 * @param[in] handle 定时器句柄
 * @return 0=成功，1=失败
 */
uint8_t TIM_SW_Restart(TIM_SW_Handle_t handle)
{
    if (handle >= TIM_SW_MAX_COUNT)
    {
        return 1;
    }
    
    if (!g_timers[handle].used)
    {
        return 1;
    }
    
    g_timers[handle].running = true;
    g_timers[handle].paused = false;
    g_timers[handle].start_tick = TIM2_TimeBase_GetTick();
    g_timers[handle].pause_remaining_ms = 0;
    
    return 0;
}

/**
 * @brief 删除软件定时器
 * @param[in] handle 定时器句柄
 * @return 0=成功，1=失败
 */
uint8_t TIM_SW_Delete(TIM_SW_Handle_t handle)
{
    if (handle >= TIM_SW_MAX_COUNT)
    {
        return 1;
    }
    
    if (!g_timers[handle].used)
    {
        return 1;
    }
    
    g_timers[handle].used = false;
    g_timers[handle].running = false;
    g_timers[handle].paused = false;
    g_timers[handle].callback = NULL;
    g_timers[handle].user_data = NULL;
    
    return 0;
}

/**
 * @brief 检查定时器是否正在运行
 * @param[in] handle 定时器句柄
 * @return true=正在运行，false=未运行
 */
bool TIM_SW_IsRunning(TIM_SW_Handle_t handle)
{
    if (handle >= TIM_SW_MAX_COUNT)
    {
        return false;
    }
    
    if (!g_timers[handle].used)
    {
        return false;
    }
    
    return g_timers[handle].running;
}

/**
 * @brief 获取定时器剩余时间
 * @param[in] handle 定时器句柄
 * @return 剩余时间（毫秒）
 */
uint32_t TIM_SW_GetRemainingTime(TIM_SW_Handle_t handle)
{
    uint32_t current_tick;
    uint32_t elapsed;
    
    if (handle >= TIM_SW_MAX_COUNT)
    {
        return 0;
    }
    
    if (!g_timers[handle].used || !g_timers[handle].running)
    {
        return 0;
    }
    
    if (g_timers[handle].paused)
    {
        /* 暂停中：返回保存的剩余时间 */
        return g_timers[handle].pause_remaining_ms;
    }
    
    current_tick = TIM2_TimeBase_GetTick();
    elapsed = TIM_SW_GetElapsed(current_tick, g_timers[handle].start_tick);
    
    if (elapsed >= g_timers[handle].period_ms)
    {
        return 0;  /* 已到期 */
    }
    
    return g_timers[handle].period_ms - elapsed;
}

/**
 * @brief 更新软件定时器（在TIM2中断中调用）
 * @note 检查所有定时器，到期时触发回调
 */
void TIM_SW_Update(void)
{
    uint8_t i;
    uint32_t current_tick;
    uint32_t elapsed;
    
    if (!g_tim_sw_initialized)
    {
        return;
    }
    
    current_tick = TIM2_TimeBase_GetTick();
    
    for (i = 0; i < TIM_SW_MAX_COUNT; i++)
    {
        if (g_timers[i].used && g_timers[i].running && !g_timers[i].paused)
        {
            elapsed = TIM_SW_GetElapsed(current_tick, g_timers[i].start_tick);
            
            if (elapsed >= g_timers[i].period_ms)
            {
                /* 定时器到期 */
                if (g_timers[i].callback != NULL)
                {
                    /* 调用回调函数，传递user_data */
                    g_timers[i].callback(i, g_timers[i].user_data);
                }
                
                if (g_timers[i].mode == TIM_SW_MODE_PERIODIC)
                {
                    /* 周期模式：重新启动 */
                    g_timers[i].start_tick = current_tick;
                }
                else
                {
                    /* 单次模式：停止 */
                    g_timers[i].running = false;
                }
            }
        }
    }
}

/**
 * @brief 修改定时器周期
 * @param[in] handle 定时器句柄
 * @param[in] period_ms 新的周期（毫秒）
 * @return 0=成功，1=失败
 */
uint8_t TIM_SW_SetPeriod(TIM_SW_Handle_t handle, uint32_t period_ms)
{
    uint32_t current_tick;
    uint32_t elapsed;
    
    if (handle >= TIM_SW_MAX_COUNT)
    {
        return 1;
    }
    
    if (!g_timers[handle].used)
    {
        return 1;
    }
    
    if (period_ms == 0)
    {
        return 1;
    }
    
    if (g_timers[handle].running && !g_timers[handle].paused)
    {
        /* 运行中：保持已运行时间不变，重新计算start_tick */
        current_tick = TIM2_TimeBase_GetTick();
        elapsed = TIM_SW_GetElapsed(current_tick, g_timers[handle].start_tick);
        
        if (elapsed < period_ms)
        {
            /* 已运行时间小于新周期，重新计算start_tick */
            /* 使用安全的减法，避免溢出 */
            if (current_tick >= elapsed)
            {
                g_timers[handle].start_tick = current_tick - elapsed;
            }
            else
            {
                /* 溢出情况：从最大值绕回 */
                g_timers[handle].start_tick = (UINT32_MAX - elapsed + current_tick + 1);
            }
        }
        else
        {
            /* 已运行时间大于等于新周期，立即到期 */
            /* 使用安全的减法，避免溢出 */
            if (current_tick >= period_ms)
            {
                g_timers[handle].start_tick = current_tick - period_ms;
            }
            else
            {
                /* 溢出情况：从最大值绕回 */
                g_timers[handle].start_tick = (UINT32_MAX - period_ms + current_tick + 1);
            }
        }
    }
    else if (g_timers[handle].paused)
    {
        /* 暂停中：保持剩余时间不变，调整周期 */
        /* pause_remaining_ms 保持不变 */
    }
    
    g_timers[handle].period_ms = period_ms;
    
    return 0;
}

/**
 * @brief 获取定时器已运行时间
 * @param[in] handle 定时器句柄
 * @return 已运行时间（毫秒）
 */
uint32_t TIM_SW_GetElapsedTime(TIM_SW_Handle_t handle)
{
    uint32_t current_tick;
    uint32_t elapsed;
    
    if (handle >= TIM_SW_MAX_COUNT)
    {
        return 0;
    }
    
    if (!g_timers[handle].used || !g_timers[handle].running)
    {
        return 0;
    }
    
    if (g_timers[handle].paused)
    {
        /* 暂停中：已运行时间 = 周期 - 剩余时间 */
        return g_timers[handle].period_ms - g_timers[handle].pause_remaining_ms;
    }
    
    current_tick = TIM2_TimeBase_GetTick();
    elapsed = TIM_SW_GetElapsed(current_tick, g_timers[handle].start_tick);
    
    if (elapsed >= g_timers[handle].period_ms)
    {
        return g_timers[handle].period_ms;  /* 已到期，返回周期 */
    }
    
    return elapsed;
}

/**
 * @brief 暂停定时器（保持剩余时间）
 * @param[in] handle 定时器句柄
 * @return 0=成功，1=失败
 */
uint8_t TIM_SW_Pause(TIM_SW_Handle_t handle)
{
    uint32_t current_tick;
    uint32_t elapsed;
    uint32_t remaining;
    
    if (handle >= TIM_SW_MAX_COUNT)
    {
        return 1;
    }
    
    if (!g_timers[handle].used || !g_timers[handle].running)
    {
        return 1;
    }
    
    if (g_timers[handle].paused)
    {
        return 1;  /* 已经暂停 */
    }
    
    /* 计算剩余时间 */
    current_tick = TIM2_TimeBase_GetTick();
    elapsed = TIM_SW_GetElapsed(current_tick, g_timers[handle].start_tick);
    
    if (elapsed >= g_timers[handle].period_ms)
    {
        remaining = 0;  /* 已到期 */
    }
    else
    {
        remaining = g_timers[handle].period_ms - elapsed;
    }
    
    /* 保存剩余时间并标记为暂停 */
    g_timers[handle].pause_remaining_ms = remaining;
    g_timers[handle].paused = true;
    
    return 0;
}

/**
 * @brief 恢复定时器（从暂停点继续）
 * @param[in] handle 定时器句柄
 * @return 0=成功，1=失败
 */
uint8_t TIM_SW_Resume(TIM_SW_Handle_t handle)
{
    uint32_t current_tick;
    
    if (handle >= TIM_SW_MAX_COUNT)
    {
        return 1;
    }
    
    if (!g_timers[handle].used || !g_timers[handle].running)
    {
        return 1;
    }
    
    if (!g_timers[handle].paused)
    {
        return 1;  /* 未暂停 */
    }
    
    /* 从保存的剩余时间继续计时 */
    current_tick = TIM2_TimeBase_GetTick();
    {
        uint32_t elapsed_before_pause = g_timers[handle].period_ms - g_timers[handle].pause_remaining_ms;
        /* 使用安全的减法，避免溢出 */
        if (current_tick >= elapsed_before_pause)
        {
            g_timers[handle].start_tick = current_tick - elapsed_before_pause;
        }
        else
        {
            /* 溢出情况：从最大值绕回 */
            g_timers[handle].start_tick = (UINT32_MAX - elapsed_before_pause + current_tick + 1);
        }
    }
    g_timers[handle].paused = false;
    g_timers[handle].pause_remaining_ms = 0;
    
    return 0;
}

/**
 * @brief 检查软件定时器模块是否已初始化
 * @return true=已初始化，false=未初始化
 */
bool TIM_SW_IsInitialized(void)
{
    return g_tim_sw_initialized;
}

