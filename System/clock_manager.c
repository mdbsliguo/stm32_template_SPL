/**
 * @file clock_manager.c
 * @brief 时钟管理模块实现（动态频率调节DVFS）
 * @version 1.0.0
 * @date 2024-01-01
 * 
 * @details 实现说明：
 * - 支持9个频率档位（8MHz~72MHz），通过PLL或HSI实现
 * - 手动模式：固定频率，通过API手动切换
 * - 自动模式：根据CPU负载自动调节频率，实现DVFS（动态电压频率调节）
 * - 频率切换时自动重新配置延时模块，确保时间基准不变
 * 
 * @note 频率切换流程：
 * 1. 进入临界区（禁用中断）
 * 2. 如果使用PLL，先切换到HSI
 * 3. 配置PLL参数（倍频系数、Flash等待周期）
 * 4. 启动PLL并等待锁定
 * 5. 切换到目标时钟源
 * 6. 更新SystemCoreClock
 * 7. 退出临界区（使能中断）
 * 8. 重新配置延时模块
 */

#include "clock_manager.h"
#include "board.h"
#include "delay.h"
#include "stm32f10x.h"
#include <stdbool.h>

/* 任务tick计数器（在TIM2_TimeBase.c中定义，TIM2中断中递增） */
extern volatile uint32_t g_task_tick;

/* 频率配置表（系数1-9，对应72MHz-8MHz） */
static const CLKM_Config_t g_freq_table[CLKM_LVL_MAX] = {
    {72000000U, CLKM_SRC_PLL,  9, 2},  /* 系数1：72MHz，PLL x9 */
    {64000000U, CLKM_SRC_PLL,  8, 2},  /* 系数2：64MHz，PLL x8 */
    {56000000U, CLKM_SRC_PLL,  7, 2},  /* 系数3：56MHz，PLL x7 */
    {48000000U, CLKM_SRC_PLL,  6, 1},  /* 系数4：48MHz，PLL x6 */
    {40000000U, CLKM_SRC_PLL,  5, 1},  /* 系数5：40MHz，PLL x5 */
    {32000000U, CLKM_SRC_PLL,  4, 1},  /* 系数6：32MHz，PLL x4 */
    {24000000U, CLKM_SRC_PLL,  3, 0},  /* 系数7：24MHz，PLL x3 */
    {16000000U, CLKM_SRC_PLL,  2, 0},  /* 系数8：16MHz，PLL x2（HSE 8MHz × 2） */
    { 8000000U, CLKM_SRC_HSI,  0, 0},  /* 系数9：8MHz，HSI */
};

/* 模块状态 */
static struct {
    bool is_initialized;              /**< 是否已初始化 */
    CLKM_Mode_t current_mode;         /**< 当前运行模式 */
    CLKM_FreqLevel_t current_level;   /**< 当前频率档位 */
    CLKM_FreqLevel_t min_auto_level;  /**< 自动模式最低档位 */
    uint32_t last_switch_tick;        /**< 上次切换时间（ms） */
    uint32_t idle_ticks;              /**< 空闲tick计数 */
    uint32_t total_ticks;             /**< 总tick计数 */
    uint32_t busy_ticks;              /**< 忙碌tick计数（新增） */
    uint8_t cpu_load;                 /**< CPU负载（%） */
    uint32_t last_check_tick;         /**< 上次检测时间（ms） */
    /* 1秒定时器CPU使用率统计 */
    volatile uint32_t cpu_idle_cnt;   /**< CPU空闲计数（1秒内） */
    uint32_t cpu_calc_tick;           /**< 上次计算CPU使用率的时间（ms） */
} g_clkm_state = {0};

/* 内部函数声明 */
static CLKM_ErrorCode_t CLKM_SwitchToLevel(CLKM_FreqLevel_t level);
static CLKM_ErrorCode_t CLKM_AdjustLevelInternal(int8_t step);

/**
 * @brief 初始化时钟管理模块
 */
CLKM_ErrorCode_t CLKM_Init(void)
{
    if (g_clkm_state.is_initialized)
    {
        return CLKM_OK;
    }
    
    /* 默认频率为72MHz（系数1），但实际时钟可能还是8MHz HSI */
    /* 需要调用CLKM_SetMode来实际切换时钟 */
    g_clkm_state.current_level = CLKM_LVL_72MHZ;
    g_clkm_state.current_mode = CLKM_MODE_MANUAL;
    g_clkm_state.is_initialized = true;
    
    /* 初始化1秒定时器CPU使用率统计 */
    g_clkm_state.cpu_idle_cnt = 0;
    g_clkm_state.cpu_calc_tick = 0;
    
    return CLKM_OK;
}

/**
 * @brief 设置运行模式
 */
CLKM_ErrorCode_t CLKM_SetMode(CLKM_Mode_t mode, uint8_t param)
{
    if (!g_clkm_state.is_initialized)
    {
        return CLKM_ERROR_NOT_INIT;
    }
    
    /* 检查切换间隔（防止切换过快） */
    /* 注意：如果g_task_tick还未初始化（为0），跳过检查 */
    /* 注意：自动模式不切换频率，所以不需要检查切换间隔 */
    extern volatile uint32_t g_task_tick;
    if (mode == CLKM_MODE_MANUAL && g_task_tick > 0)
    {
        uint32_t elapsed = (g_task_tick >= g_clkm_state.last_switch_tick) ?
                          (g_task_tick - g_clkm_state.last_switch_tick) :
                          (0xFFFFFFFF - g_clkm_state.last_switch_tick + g_task_tick + 1);
        if (elapsed < CLKM_SWITCH_INTERVAL_UP)  /* 使用升频间隔作为最小间隔 */
        {
            return CLKM_ERROR_SWITCH_TOO_FAST;
        }
    }
    
    g_clkm_state.current_mode = mode;
    /* 只有手动模式切换频率时才更新last_switch_tick */
    if (mode == CLKM_MODE_MANUAL)
    {
        g_clkm_state.last_switch_tick = g_task_tick;
    }
    
    if (mode == CLKM_MODE_MANUAL)
    {
        /* 手动模式：设置固定频率 */
        if (param >= CLKM_LVL_MAX)
        {
            return CLKM_ERROR_INVALID_FREQ;
        }
        return CLKM_SwitchToLevel((CLKM_FreqLevel_t)param);
    }
    else
    {
        /* 自动模式：设置最低频率档位 */
        if (param >= CLKM_LVL_MAX)
        {
            g_clkm_state.min_auto_level = CLKM_LVL_8MHZ;  /* 默认最低8MHz */
        }
        else
        {
            g_clkm_state.min_auto_level = (CLKM_FreqLevel_t)param;
        }
        
        /* 重置统计 */
        g_clkm_state.idle_ticks = 0;
        g_clkm_state.total_ticks = 0;
        g_clkm_state.busy_ticks = 0;
        g_clkm_state.cpu_load = 0;
        /* 注意：last_check_tick应该设置为当前tick，而不是0 */
        /* 如果设置为0，第一次调用CLKM_AdaptiveTask时，g_task_tick - 0可能小于50，导致直接返回 */
        extern volatile uint32_t g_task_tick;
        g_clkm_state.last_check_tick = g_task_tick;
        
        return CLKM_OK;
    }
}

/**
 * @brief 手动设置固定频率档位
 */
CLKM_ErrorCode_t CLKM_SetFixedLevel(CLKM_FreqLevel_t level)
{
    if (!g_clkm_state.is_initialized)
    {
        return CLKM_ERROR_NOT_INIT;
    }
    
    if (g_clkm_state.current_mode != CLKM_MODE_MANUAL)
    {
        return CLKM_ERROR_MODE_CONFLICT;
    }
    
    if (level >= CLKM_LVL_MAX)
    {
        return CLKM_ERROR_INVALID_FREQ;
    }
    
    return CLKM_SwitchToLevel(level);
}

/**
 * @brief 手动调整频率档位（递进式）
 */
CLKM_ErrorCode_t CLKM_AdjustLevel(int8_t step)
{
    if (!g_clkm_state.is_initialized)
    {
        return CLKM_ERROR_NOT_INIT;
    }
    
    if (g_clkm_state.current_mode != CLKM_MODE_MANUAL)
    {
        return CLKM_ERROR_MODE_CONFLICT;
    }
    
    return CLKM_AdjustLevelInternal(step);
}

/**
 * @brief 内部调整频率档位（不检查模式）
 */
static CLKM_ErrorCode_t CLKM_AdjustLevelInternal(int8_t step)
{
    int16_t new_level = (int16_t)g_clkm_state.current_level + step;
    
    /* 限制范围 */
    if (new_level < 0)
    {
        new_level = 0;
    }
    if (new_level >= CLKM_LVL_MAX)
    {
        new_level = CLKM_LVL_MAX - 1;
    }
    
    return CLKM_SwitchToLevel((CLKM_FreqLevel_t)new_level);
}

/**
 * @brief 自适应任务（自动调频）
 * @note 新策略：
 *   - CPU使用率 < 30%：降频1个系数，最低8MHz，降频间隔5秒
 *   - CPU使用率 > 50%：升频3个系数，升频间隔1秒，最高72MHz
 */
void CLKM_AdaptiveTask(void)
{
#if CLKM_ADAPTIVE_ENABLE
    if (!g_clkm_state.is_initialized)
    {
        return;
    }
    
    if (g_clkm_state.current_mode != CLKM_MODE_AUTO)
    {
        return;
    }
    
    /* 每50ms检测一次CPU负载 */
    extern volatile uint32_t g_task_tick;
    if ((g_task_tick - g_clkm_state.last_check_tick) < CLKM_LOAD_CHECK_INTERVAL)
    {
        return;
    }
    g_clkm_state.last_check_tick = g_task_tick;
    
    /* 计算本周期CPU负载 */
    /* 总tick = 空闲tick + 忙碌tick */
    uint32_t total = g_clkm_state.idle_ticks + g_clkm_state.busy_ticks;
    if (total > 0)
    {
        /* CPU负载 = 忙碌时间 / 总时间 */
        g_clkm_state.cpu_load = (g_clkm_state.busy_ticks * 100) / total;
    }
    else
    {
        /* 如果没有统计，默认认为负载为0（空闲） */
        g_clkm_state.cpu_load = 0;
    }
    
    /* 重置统计 */
    g_clkm_state.total_ticks = 0;
    g_clkm_state.idle_ticks = 0;
    g_clkm_state.busy_ticks = 0;  /* 修复：重置busy_ticks */
    
    /* 计算距离上次切换的时间 */
    uint32_t elapsed = (g_task_tick >= g_clkm_state.last_switch_tick) ?
                      (g_task_tick - g_clkm_state.last_switch_tick) :
                      (0xFFFFFFFF - g_clkm_state.last_switch_tick + g_task_tick + 1);
    
    /* 高负载>50%：跳3档升频，间隔1秒 */
    if (g_clkm_state.cpu_load > CLKM_LOAD_THRESHOLD_HIGH)
    {
        /* 检查升频间隔（1秒） */
        if (elapsed >= CLKM_SWITCH_INTERVAL_UP)
        {
            /* 如果当前不是最高频率（系数1=72MHz，level=0），则升频 */
            if (g_clkm_state.current_level > CLKM_LVL_72MHZ)
            {
                CLKM_ErrorCode_t err = CLKM_AdjustLevelInternal(-CLKM_AUTO_POLICY_JUMP);
                if (err == CLKM_OK)
                {
                    g_clkm_state.last_switch_tick = g_task_tick;
                }
            }
            else if (g_clkm_state.current_level == CLKM_LVL_72MHZ)
            {
                /* 已经是最高频率，不需要升频 */
            }
        }
    }
    /* 低负载<30%：降1档，间隔5秒 */
    else if (g_clkm_state.cpu_load < CLKM_LOAD_THRESHOLD_LOW)
    {
        /* 检查降频间隔（5秒） */
        if (elapsed >= CLKM_SWITCH_INTERVAL_DOWN)
        {
            /* 如果当前不是最低频率（系数9=8MHz，level=8），则降频 */
            if (g_clkm_state.current_level < CLKM_LVL_8MHZ)
            {
                CLKM_ErrorCode_t err = CLKM_AdjustLevelInternal(CLKM_AUTO_POLICY_STEP);
                if (err == CLKM_OK)
                {
                    g_clkm_state.last_switch_tick = g_task_tick;
                }
            }
            else if (g_clkm_state.current_level == CLKM_LVL_8MHZ)
            {
                /* 已经是最低频率，不需要降频 */
            }
        }
    }
#endif
}

/**
 * @brief 空闲钩子函数
 * @note 在空闲时调用，用于统计CPU负载
 */
void CLKM_IdleHook(void)
{
#if CLKM_IDLE_HOOK_ENABLE
    if (g_clkm_state.is_initialized && g_clkm_state.current_mode == CLKM_MODE_AUTO)
    {
        g_clkm_state.idle_ticks++;
        /* 1秒定时器统计：空闲计数递增 */
        g_clkm_state.cpu_idle_cnt++;
    }
#endif
}

/**
 * @brief 忙碌钩子函数
 * @note 在忙碌时调用，用于统计CPU负载
 */
void CLKM_BusyHook(void)
{
#if CLKM_IDLE_HOOK_ENABLE
    if (g_clkm_state.is_initialized && g_clkm_state.current_mode == CLKM_MODE_AUTO)
    {
        g_clkm_state.busy_ticks++;
    }
#endif
}

/**
 * @brief 获取当前频率档位
 */
CLKM_FreqLevel_t CLKM_GetCurrentLevel(void)
{
    return g_clkm_state.current_level;
}

/**
 * @brief 获取当前运行模式
 */
CLKM_Mode_t CLKM_GetCurrentMode(void)
{
    return g_clkm_state.current_mode;
}

/**
 * @brief 获取当前频率
 */
uint32_t CLKM_GetCurrentFrequency(void)
{
    return SystemCoreClock;
}

/**
 * @brief 1秒定时器计算CPU使用率（在TIM2中断中调用）
 * @note 每1秒调用一次，计算CPU使用率
 * @note CPU使用率 = 100 - (cpu_idle_cnt / 10)
 * @note 如果cpu_idle_cnt >= 1000，说明CPU完全空闲（使用率0%）
 * @note 如果cpu_idle_cnt = 0，说明CPU完全忙碌（使用率100%）
 * @note 原理：1秒内如果空闲计数越多，CPU使用率越低
 */
void CLKM_CalculateCPULoad1Sec(void)
{
#if CLKM_IDLE_HOOK_ENABLE
    if (!g_clkm_state.is_initialized || g_clkm_state.current_mode != CLKM_MODE_AUTO)
    {
        return;
    }
    
    /* 计算CPU使用率 */
    /* 原理：主循环每100ms一次，1秒约10次循环 */
    /* 如果每次循环都调用IdleHook，说明CPU完全空闲（0%） */
    /* 如果很少调用IdleHook，说明CPU有负载 */
    /* CPU使用率 = 100 - (空闲计数 * 10) */
    /* 例如：1秒内空闲计数=4，则CPU使用率 = 100 - 40 = 60% */
    
    uint32_t idle_cnt = g_clkm_state.cpu_idle_cnt;
    
    /* 假设1秒内主循环执行约10次（每100ms一次） */
    /* 如果idle_cnt >= 10，说明CPU完全空闲 */
    if (idle_cnt >= 10)
    {
        g_clkm_state.cpu_load = 0;
    }
    else
    {
        /* CPU使用率 = 100 - (空闲计数 * 10) */
        /* 例如：idle_cnt = 4，则CPU使用率 = 100 - 40 = 60% */
        uint32_t idle_percent = idle_cnt * 10;
        if (idle_percent > 100)
        {
            idle_percent = 100;
        }
        g_clkm_state.cpu_load = 100 - (uint8_t)idle_percent;
    }
    
    /* 清零重新计数 */
    g_clkm_state.cpu_idle_cnt = 0;
#endif
}

/**
 * @brief 获取CPU负载
 */
uint8_t CLKM_GetCPULoad(void)
{
    return g_clkm_state.cpu_load;
}

/**
 * @brief 切换到指定频率档位（内部函数）
 */
static CLKM_ErrorCode_t CLKM_SwitchToLevel(CLKM_FreqLevel_t level)
{
    uint32_t timeout;  /* 超时计数器 */
    
    if (level >= CLKM_LVL_MAX)
    {
        return CLKM_ERROR_INVALID_FREQ;
    }
    
    /* 临界区保护 */
    __disable_irq();
    
    const CLKM_Config_t *config = &g_freq_table[level];
    
    /* 如果使用PLL或HSE，需要先启动HSE */
    if (config->source == CLKM_SRC_PLL || config->source == CLKM_SRC_HSE)
    {
        RCC->CR |= RCC_CR_HSEON;
        timeout = 500000;
        while (!(RCC->CR & RCC_CR_HSERDY) && --timeout);
        if (timeout == 0)
        {
            __enable_irq();
            return CLKM_ERROR_HSE_NOT_FOUND;
        }
    }
    
    /* 根据时钟源切换 */
    if (config->source == CLKM_SRC_HSI)
    {
        /* 使用HSI */
        RCC->CR |= RCC_CR_HSION;
        timeout = 500000;
        while (!(RCC->CR & RCC_CR_HSIRDY) && --timeout);
        if (timeout == 0)
        {
            __enable_irq();
            return CLKM_ERROR_HSE_NOT_FOUND;  /* 复用错误码，表示HSI启动失败 */
        }
        
        RCC->CFGR &= ~RCC_CFGR_SW;
        RCC->CFGR |= RCC_CFGR_SW_HSI;
        timeout = 500000;
        while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI && --timeout);
        if (timeout == 0)
        {
            __enable_irq();
            return CLKM_ERROR_HSE_NOT_FOUND;  /* 复用错误码，表示切换失败 */
        }
    }
    else if (config->source == CLKM_SRC_PLL)
    {
        /* 使用PLL */
        /* 如果当前系统时钟是PLL，需要先切换到HSI，否则关闭PLL会导致系统停止 */
        uint32_t current_sw = RCC->CFGR & RCC_CFGR_SWS;
        if (current_sw == RCC_CFGR_SWS_PLL)
        {
            /* 切换到HSI */
            RCC->CR |= RCC_CR_HSION;
            timeout = 500000;
            while (!(RCC->CR & RCC_CR_HSIRDY) && --timeout);
            if (timeout == 0)
            {
                __enable_irq();
                return CLKM_ERROR_HSE_NOT_FOUND;  /* HSI启动失败 */
            }
            
            RCC->CFGR &= ~RCC_CFGR_SW;
            RCC->CFGR |= RCC_CFGR_SW_HSI;
            timeout = 500000;
            while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI && --timeout);
            if (timeout == 0)
            {
                __enable_irq();
                return CLKM_ERROR_HSE_NOT_FOUND;  /* 切换到HSI失败 */
            }
        }
        
        /* 如果PLL已经启动，先关闭它（PLL配置只能在关闭时修改） */
        if (RCC->CR & RCC_CR_PLLON)
        {
            RCC->CR &= ~RCC_CR_PLLON;
            /* 等待PLL关闭 */
            timeout = 500000;
            while ((RCC->CR & RCC_CR_PLLRDY) && --timeout);
            if (timeout == 0)
            {
                __enable_irq();
                return CLKM_ERROR_PLL_LOCK_TIMEOUT;  /* PLL关闭超时 */
            }
        }
        
        /* 设置Flash等待周期 */
        FLASH->ACR &= ~FLASH_ACR_LATENCY;
        FLASH->ACR |= config->flash_latency;
        
        /* 配置PLL */
        /* 清除PLL配置位 */
        RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL);
        
        /* 设置PLL源为HSE，不使用分频（直接使用HSE） */
        RCC->CFGR |= RCC_CFGR_PLLSRC_HSE;
        
        /* 设置PLL倍频系数（根据pll_mul值选择对应的宏） */
        switch (config->pll_mul)
        {
            case 2: RCC->CFGR |= RCC_CFGR_PLLMULL2; break;
            case 3: RCC->CFGR |= RCC_CFGR_PLLMULL3; break;
            case 4: RCC->CFGR |= RCC_CFGR_PLLMULL4; break;
            case 5: RCC->CFGR |= RCC_CFGR_PLLMULL5; break;
            case 6: RCC->CFGR |= RCC_CFGR_PLLMULL6; break;
            case 7: RCC->CFGR |= RCC_CFGR_PLLMULL7; break;
            case 8: RCC->CFGR |= RCC_CFGR_PLLMULL8; break;
            case 9: RCC->CFGR |= RCC_CFGR_PLLMULL9; break;
            default: 
                __enable_irq();
                return CLKM_ERROR_INVALID_FREQ;
        }
        
        /* 启动PLL */
        RCC->CR |= RCC_CR_PLLON;
        timeout = 500000;
        while (!(RCC->CR & RCC_CR_PLLRDY) && --timeout);
        if (timeout == 0)
        {
            __enable_irq();
            return CLKM_ERROR_PLL_LOCK_TIMEOUT;
        }
        
        /* 切换到PLL */
        RCC->CFGR &= ~RCC_CFGR_SW;
        RCC->CFGR |= RCC_CFGR_SW_PLL;
        timeout = 500000;
        while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL && --timeout);
        if (timeout == 0)
        {
            __enable_irq();
            return CLKM_ERROR_PLL_LOCK_TIMEOUT;  /* 复用错误码，表示切换失败 */
        }
    }
    
    /* 更新系统时钟 */
    SystemCoreClock = config->target_freq;
    SystemCoreClockUpdate();
    
    __enable_irq();
    
    /* 更新当前档位 */
    g_clkm_state.current_level = level;
    
    /* 重新配置延时模块和基时定时器（因为时钟频率改变了） */
    /* 注意：必须调用Delay_Reconfig()，而不是Delay_Init()，因为Delay_Init()会检查初始化标志 */
    Delay_Reconfig(config->target_freq);
    
    return CLKM_OK;
}
