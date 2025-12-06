/**
 * @file system_init.c
 * @brief 系统初始化框架实现
 * @version 1.0.0
 * @date 2024-01-01
 * 
 * @details 实现统一的系统初始化流程，按顺序初始化各个模块
 * 
 * @note 初始化顺序说明：
 * 1. TIM2_TimeBase：时间基准模块，提供1ms中断，是其他模块的基础
 * 2. Delay：延时模块，基于TIM2_TimeBase，提供微秒/毫秒级延时
 * 3. TIM_SW：软件定时器模块，基于TIM2_TimeBase，提供多个软件定时器实例
 * 4. GPIO：GPIO驱动，使用时自动使能时钟，无需显式初始化
 * 5. LED：LED驱动，如果启用则初始化
 * 
 * @note 模块开关控制：
 * - 通过System/config.h中的CONFIG_MODULE_XXX_ENABLED控制模块编译
 * - 未启用的模块不会编译，减少代码体积
 */

#include "system_init.h"
#include "TIM2_TimeBase.h"  /* TIM2时间基准 */
#include "delay.h"
#include "gpio.h"
/* 使用 <config.h> 而不是 "config.h"，这样会优先在包含路径中查找 */
/* 案例工程的包含路径中，案例目录在最前面，所以会优先找到案例的config.h */
/* 主工程没有案例目录的config.h，会找到System/config.h */
#include <config.h>
#if defined(CONFIG_MODULE_LED_ENABLED) && CONFIG_MODULE_LED_ENABLED
#include "led.h"
#endif
#include "stm32f10x.h"
#include <stdbool.h>
#include <stdint.h>

/* 初始化状态标志 */
static bool g_system_initialized = false;

sys_init_error_t System_Init(void)
{
    /* 防止重复初始化 */
    if (g_system_initialized)
    {
        return SYS_INIT_OK;
    }
    
    /* 步骤1：初始化TIM2时间基准（时间基准，1ms中断） */
    TIM2_TimeBase_Init();
    
    /* 步骤2：初始化延时模块（基于TIM2_TimeBase） */
    Delay_Init();
    
    /* 步骤2.5：初始化软件定时器模块（基于TIM2_TimeBase） */
#if defined(CONFIG_MODULE_TIM_SW_ENABLED) && CONFIG_MODULE_TIM_SW_ENABLED
    #include "TIM_sw.h"
    TIM_SW_Init();
#endif
    
    /* 步骤2：BSP层初始化（板级配置）
     * 注意：board.h中的配置在编译时已确定，无需运行时初始化
     * 如需板级初始化函数，可在BSP/board_config.c中实现
     */
    
    /* 步骤3：驱动层初始化 */
    /* GPIO驱动无需显式初始化（使用时自动使能时钟） */
    
#if defined(CONFIG_MODULE_LED_ENABLED) && CONFIG_MODULE_LED_ENABLED
    /* LED驱动初始化 */
    LED_Status_t led_err = LED_Init();
    if (led_err != LED_OK)
    {
        return SYS_INIT_ERROR_DRIVER;
    }
#endif
    
    /* 标记为已初始化 */
    g_system_initialized = true;
    
    return SYS_INIT_OK;
}

sys_init_error_t System_Deinit(void)
{
    if (!g_system_initialized)
    {
        return SYS_INIT_OK;
    }
    
    /* 反初始化驱动层 */
#if defined(CONFIG_MODULE_LED_ENABLED) && CONFIG_MODULE_LED_ENABLED
    LED_Deinit();
#endif
    
    /* 反初始化SysTick（可选，通常不需要） */
    /* SYSTICK_Deinit(); */
    
    g_system_initialized = false;
    
    return SYS_INIT_OK;
}

uint8_t System_IsInitialized(void)
{
    return g_system_initialized ? 1 : 0;
}
