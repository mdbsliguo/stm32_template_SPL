/**
 * @file timer_pwm.c
 * @brief 定时器PWM驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的定时器PWM驱动，支持TIM1/TIM3/TIM4，PWM输出，占空比和频率设置
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_TIMER_ENABLED

/* Include our header */
#include "timer_pwm.h"

#include "gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"
#include "system_stm32f10x.h"
#include <stdbool.h>
#include <stddef.h>

/* 从board.h加载配置 */
static PWM_Config_t g_pwm_configs[PWM_INSTANCE_MAX] = PWM_CONFIGS;

/* 初始化标志 */
static bool g_pwm_initialized[PWM_INSTANCE_MAX] = {false, false, false};

/* 当前频率（Hz） */
static uint32_t g_pwm_frequency[PWM_INSTANCE_MAX] = {1000, 1000, 1000};

/**
 * @brief 获取GPIO时钟
 * @param[in] port GPIO端口指针
 * @return uint32_t GPIO时钟使能值
 */
static uint32_t PWM_GetGPIOClock(GPIO_TypeDef *port)
{
    if (port == GPIOA)
    {
        return RCC_APB2Periph_GPIOA;
    }
    else if (port == GPIOB)
    {
        return RCC_APB2Periph_GPIOB;
    }
    else if (port == GPIOC)
    {
        return RCC_APB2Periph_GPIOC;
    }
    else if (port == GPIOD)
    {
        return RCC_APB2Periph_GPIOD;
    }
    return 0;
}

/**
 * @brief 获取定时器外设时钟
 * @param[in] tim_periph 定时器外设指针
 * @return uint32_t 时钟使能值，失败返回0
 */
static uint32_t PWM_GetPeriphClock(TIM_TypeDef *tim_periph)
{
    if (tim_periph == TIM1)
    {
        return RCC_APB2Periph_TIM1;
    }
    else if (tim_periph == TIM3)
    {
        return RCC_APB1Periph_TIM3;
    }
    else if (tim_periph == TIM4)
    {
        return RCC_APB1Periph_TIM4;
    }
    return 0;
}

/**
 * @brief 获取定时器实际时钟频率
 * @param[in] tim_periph 定时器外设指针
 * @return uint32_t 定时器时钟频率（Hz），失败返回0
 */
static uint32_t PWM_GetTimerClock(TIM_TypeDef *tim_periph)
{
    RCC_ClocksTypeDef RCC_Clocks;
    uint32_t tim_clk;
    
    RCC_GetClocksFreq(&RCC_Clocks);
    
    if (tim_periph == TIM1)
    {
        /* TIM1挂载在APB2总线上 */
        tim_clk = RCC_Clocks.PCLK2_Frequency;
        if (RCC_Clocks.HCLK_Frequency != RCC_Clocks.PCLK2_Frequency)
        {
            tim_clk = RCC_Clocks.PCLK2_Frequency * 2;
        }
    }
    else if (tim_periph == TIM3 || tim_periph == TIM4)
    {
        /* TIM3/TIM4挂载在APB1总线上 */
        tim_clk = RCC_Clocks.PCLK1_Frequency;
        if (RCC_Clocks.HCLK_Frequency != RCC_Clocks.PCLK1_Frequency)
        {
            tim_clk = RCC_Clocks.PCLK1_Frequency * 2;
        }
    }
    else
    {
        return 0;
    }
    
    return tim_clk;
}

/**
 * @brief 获取PWM通道对应的TIM通道
 * @param[in] channel PWM通道
 * @return uint16_t TIM通道，失败返回0
 */
static uint16_t PWM_GetTIMChannel(PWM_Channel_t channel)
{
    switch (channel)
    {
        case PWM_CHANNEL_1:
            return TIM_Channel_1;
        case PWM_CHANNEL_2:
            return TIM_Channel_2;
        case PWM_CHANNEL_3:
            return TIM_Channel_3;
        case PWM_CHANNEL_4:
            return TIM_Channel_4;
        default:
            return 0;
    }
}

/**
 * @brief 获取PWM通道对应的OC模式
 * @param[in] channel PWM通道
 * @return uint16_t OC模式，失败返回0
 */
static uint16_t PWM_GetOCMode(PWM_Channel_t channel)
{
    switch (channel)
    {
        case PWM_CHANNEL_1:
            return TIM_OCMode_PWM1;
        case PWM_CHANNEL_2:
            return TIM_OCMode_PWM1;
        case PWM_CHANNEL_3:
            return TIM_OCMode_PWM1;
        case PWM_CHANNEL_4:
            return TIM_OCMode_PWM1;
        default:
            return 0;
    }
}

/**
 * @brief PWM初始化
 */
PWM_Status_t PWM_Init(PWM_Instance_t instance)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    PWM_Status_t status;
    uint32_t tim_clock;
    uint32_t gpio_clock;
    uint16_t i;
    TIM_TypeDef *tim_periph;
    
    /* 参数校验 */
    if (instance >= PWM_INSTANCE_MAX)
    {
        return PWM_ERROR_INVALID_PARAM;
    }
    
    if (!g_pwm_configs[instance].enabled)
    {
        return PWM_ERROR_INVALID_PARAM;
    }
    
    if (g_pwm_initialized[instance])
    {
        return PWM_OK;
    }
    
    tim_periph = g_pwm_configs[instance].tim_periph;
    
    /* 获取定时器外设时钟 */
    tim_clock = PWM_GetPeriphClock(tim_periph);
    if (tim_clock == 0)
    {
        return PWM_ERROR_INVALID_PERIPH;
    }
    
    /* 使能定时器外设时钟 */
    if (tim_clock & RCC_APB2Periph_TIM1)
    {
        RCC_APB2PeriphClockCmd(tim_clock, ENABLE);
    }
    else
    {
        RCC_APB1PeriphClockCmd(tim_clock, ENABLE);
    }
    
    /* 使能GPIO时钟 */
    for (i = 0; i < PWM_CHANNEL_MAX; i++)
    {
        if (g_pwm_configs[instance].channels[i].enabled)
        {
            gpio_clock = PWM_GetGPIOClock(g_pwm_configs[instance].channels[i].port);
            if (gpio_clock != 0)
            {
                RCC_APB2PeriphClockCmd(gpio_clock, ENABLE);
            }
        }
    }
    
    /* 配置GPIO引脚为复用推挽输出 */
    for (i = 0; i < PWM_CHANNEL_MAX; i++)
    {
        if (g_pwm_configs[instance].channels[i].enabled)
        {
            GPIO_InitStructure.GPIO_Pin = g_pwm_configs[instance].channels[i].pin;
            GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
            GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
            GPIO_Init(g_pwm_configs[instance].channels[i].port, &GPIO_InitStructure);
        }
    }
    
    /* 配置定时器时基 */
    TIM_TimeBaseStructure.TIM_Period = 1000 - 1;  /* 默认ARR = 999，频率 = 定时器时钟 / 1000 */
    TIM_TimeBaseStructure.TIM_Prescaler = 0;      /* 默认PSC = 0 */
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(tim_periph, &TIM_TimeBaseStructure);
    
    /* 配置PWM输出通道 */
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;  /* 默认占空比0% */
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    
    /* 配置所有启用的通道 */
    for (i = 0; i < PWM_CHANNEL_MAX; i++)
    {
        if (g_pwm_configs[instance].channels[i].enabled)
        {
            uint16_t tim_channel = PWM_GetTIMChannel((PWM_Channel_t)i);
            if (tim_channel == 0)
            {
                continue;
            }
            
            if (tim_channel == TIM_Channel_1)
            {
                TIM_OC1Init(tim_periph, &TIM_OCInitStructure);
                TIM_OC1PreloadConfig(tim_periph, TIM_OCPreload_Enable);
            }
            else if (tim_channel == TIM_Channel_2)
            {
                TIM_OC2Init(tim_periph, &TIM_OCInitStructure);
                TIM_OC2PreloadConfig(tim_periph, TIM_OCPreload_Enable);
            }
            else if (tim_channel == TIM_Channel_3)
            {
                TIM_OC3Init(tim_periph, &TIM_OCInitStructure);
                TIM_OC3PreloadConfig(tim_periph, TIM_OCPreload_Enable);
            }
            else if (tim_channel == TIM_Channel_4)
            {
                TIM_OC4Init(tim_periph, &TIM_OCInitStructure);
                TIM_OC4PreloadConfig(tim_periph, TIM_OCPreload_Enable);
            }
        }
    }
    
    /* 使能定时器预装载寄存器 */
    TIM_ARRPreloadConfig(tim_periph, ENABLE);
    
    /* 使能定时器（对于TIM1，还需要使能主输出） */
    if (tim_periph == TIM1)
    {
        TIM_CtrlPWMOutputs(TIM1, ENABLE);
    }
    
    TIM_Cmd(tim_periph, ENABLE);
    
    /* 标记为已初始化 */
    g_pwm_initialized[instance] = true;
    
    /* 设置默认频率 */
    g_pwm_frequency[instance] = 1000;
    
    return PWM_OK;
}

/**
 * @brief PWM反初始化
 */
PWM_Status_t PWM_Deinit(PWM_Instance_t instance)
{
    uint32_t tim_clock;
    TIM_TypeDef *tim_periph;
    
    /* 参数校验 */
    if (instance >= PWM_INSTANCE_MAX)
    {
        return PWM_ERROR_INVALID_PARAM;
    }
    
    if (!g_pwm_initialized[instance])
    {
        return PWM_OK;
    }
    
    tim_periph = g_pwm_configs[instance].tim_periph;
    
    /* 禁用定时器 */
    TIM_Cmd(tim_periph, DISABLE);
    
    if (tim_periph == TIM1)
    {
        TIM_CtrlPWMOutputs(TIM1, DISABLE);
    }
    
    /* 获取定时器外设时钟 */
    tim_clock = PWM_GetPeriphClock(tim_periph);
    if (tim_clock != 0)
    {
        /* 禁用定时器外设时钟 */
        if (tim_clock & RCC_APB2Periph_TIM1)
        {
            RCC_APB2PeriphClockCmd(tim_clock, DISABLE);
        }
        else
        {
            RCC_APB1PeriphClockCmd(tim_clock, DISABLE);
        }
    }
    
    /* 标记为未初始化 */
    g_pwm_initialized[instance] = false;
    
    return PWM_OK;
}

/**
 * @brief 设置PWM频率
 */
PWM_Status_t PWM_SetFrequency(PWM_Instance_t instance, uint32_t frequency)
{
    TIM_TypeDef *tim_periph;
    uint32_t tim_clk;
    uint32_t arr;
    uint16_t psc;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    
    /* 参数校验 */
    if (instance >= PWM_INSTANCE_MAX)
    {
        return PWM_ERROR_INVALID_PARAM;
    }
    
    if (frequency == 0 || frequency > 72000000)
    {
        return PWM_ERROR_FREQ_OUT_OF_RANGE;
    }
    
    if (!g_pwm_initialized[instance])
    {
        return PWM_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = g_pwm_configs[instance].tim_periph;
    
    /* 获取定时器时钟频率 */
    tim_clk = PWM_GetTimerClock(tim_periph);
    if (tim_clk == 0)
    {
        return PWM_ERROR_INVALID_PERIPH;
    }
    
    /* 计算PSC和ARR */
    /* 策略：尽量使用ARR=1000，PSC根据频率计算 */
    arr = 1000 - 1;  /* ARR = 999 */
    psc = (uint16_t)((tim_clk / frequency) / (arr + 1));
    
    /* 检查PSC是否超出范围 */
    if (psc > 65535)
    {
        /* PSC超出范围，调整ARR */
        psc = 65535;
        arr = (tim_clk / frequency) / (psc + 1) - 1;
        if (arr > 65535)
        {
            return PWM_ERROR_FREQ_OUT_OF_RANGE;
        }
    }
    
    /* 重新配置定时器时基 */
    TIM_TimeBaseStructure.TIM_Period = (uint16_t)arr;
    TIM_TimeBaseStructure.TIM_Prescaler = psc;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(tim_periph, &TIM_TimeBaseStructure);
    
    /* 保存频率 */
    g_pwm_frequency[instance] = frequency;
    
    return PWM_OK;
}

/**
 * @brief 设置PWM占空比
 */
PWM_Status_t PWM_SetDutyCycle(PWM_Instance_t instance, PWM_Channel_t channel, float duty_cycle)
{
    TIM_TypeDef *tim_periph;
    uint16_t tim_channel;
    uint32_t pulse;
    uint16_t arr;
    
    /* 参数校验 */
    if (instance >= PWM_INSTANCE_MAX)
    {
        return PWM_ERROR_INVALID_PARAM;
    }
    
    if (channel >= PWM_CHANNEL_MAX)
    {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    if (duty_cycle < 0.0f || duty_cycle > 100.0f)
    {
        return PWM_ERROR_INVALID_PARAM;
    }
    
    if (!g_pwm_initialized[instance])
    {
        return PWM_ERROR_NOT_INITIALIZED;
    }
    
    if (!g_pwm_configs[instance].channels[channel].enabled)
    {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    tim_periph = g_pwm_configs[instance].tim_periph;
    tim_channel = PWM_GetTIMChannel(channel);
    if (tim_channel == 0)
    {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    /* 获取ARR值 */
    arr = TIM_GetAutoreload(tim_periph);
    
    /* 计算Pulse值 */
    pulse = (uint32_t)((duty_cycle / 100.0f) * (arr + 1));
    
    /* 设置PWM占空比 */
    if (tim_channel == TIM_Channel_1)
    {
        TIM_SetCompare1(tim_periph, (uint16_t)pulse);
    }
    else if (tim_channel == TIM_Channel_2)
    {
        TIM_SetCompare2(tim_periph, (uint16_t)pulse);
    }
    else if (tim_channel == TIM_Channel_3)
    {
        TIM_SetCompare3(tim_periph, (uint16_t)pulse);
    }
    else if (tim_channel == TIM_Channel_4)
    {
        TIM_SetCompare4(tim_periph, (uint16_t)pulse);
    }
    
    return PWM_OK;
}

/**
 * @brief 使能PWM通道
 */
PWM_Status_t PWM_EnableChannel(PWM_Instance_t instance, PWM_Channel_t channel)
{
    TIM_TypeDef *tim_periph;
    uint16_t tim_channel;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    
    /* 参数校验 */
    if (instance >= PWM_INSTANCE_MAX)
    {
        return PWM_ERROR_INVALID_PARAM;
    }
    
    if (channel >= PWM_CHANNEL_MAX)
    {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    if (!g_pwm_initialized[instance])
    {
        return PWM_ERROR_NOT_INITIALIZED;
    }
    
    if (!g_pwm_configs[instance].channels[channel].enabled)
    {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    tim_periph = g_pwm_configs[instance].tim_periph;
    tim_channel = PWM_GetTIMChannel(channel);
    if (tim_channel == 0)
    {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    /* 配置PWM输出通道 */
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    
    /* 获取当前占空比值 */
    if (tim_channel == TIM_Channel_1)
    {
        TIM_OCInitStructure.TIM_Pulse = TIM_GetCapture1(tim_periph);
    }
    else if (tim_channel == TIM_Channel_2)
    {
        TIM_OCInitStructure.TIM_Pulse = TIM_GetCapture2(tim_periph);
    }
    else if (tim_channel == TIM_Channel_3)
    {
        TIM_OCInitStructure.TIM_Pulse = TIM_GetCapture3(tim_periph);
    }
    else if (tim_channel == TIM_Channel_4)
    {
        TIM_OCInitStructure.TIM_Pulse = TIM_GetCapture4(tim_periph);
    }
    else
    {
        TIM_OCInitStructure.TIM_Pulse = 0;
    }
    
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    
    if (tim_channel == TIM_Channel_1)
    {
        TIM_OC1Init(tim_periph, &TIM_OCInitStructure);
        TIM_OC1PreloadConfig(tim_periph, TIM_OCPreload_Enable);
    }
    else if (tim_channel == TIM_Channel_2)
    {
        TIM_OC2Init(tim_periph, &TIM_OCInitStructure);
        TIM_OC2PreloadConfig(tim_periph, TIM_OCPreload_Enable);
    }
    else if (tim_channel == TIM_Channel_3)
    {
        TIM_OC3Init(tim_periph, &TIM_OCInitStructure);
        TIM_OC3PreloadConfig(tim_periph, TIM_OCPreload_Enable);
    }
    else if (tim_channel == TIM_Channel_4)
    {
        TIM_OC4Init(tim_periph, &TIM_OCInitStructure);
        TIM_OC4PreloadConfig(tim_periph, TIM_OCPreload_Enable);
    }
    
    return PWM_OK;
}

/**
 * @brief 禁用PWM通道
 */
PWM_Status_t PWM_DisableChannel(PWM_Instance_t instance, PWM_Channel_t channel)
{
    TIM_TypeDef *tim_periph;
    uint16_t tim_channel;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    
    /* 参数校验 */
    if (instance >= PWM_INSTANCE_MAX)
    {
        return PWM_ERROR_INVALID_PARAM;
    }
    
    if (channel >= PWM_CHANNEL_MAX)
    {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    if (!g_pwm_initialized[instance])
    {
        return PWM_ERROR_NOT_INITIALIZED;
    }
    
    if (!g_pwm_configs[instance].channels[channel].enabled)
    {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    tim_periph = g_pwm_configs[instance].tim_periph;
    tim_channel = PWM_GetTIMChannel(channel);
    if (tim_channel == 0)
    {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    /* 配置PWM输出通道为禁用 */
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Disable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    
    if (tim_channel == TIM_Channel_1)
    {
        TIM_OC1Init(tim_periph, &TIM_OCInitStructure);
    }
    else if (tim_channel == TIM_Channel_2)
    {
        TIM_OC2Init(tim_periph, &TIM_OCInitStructure);
    }
    else if (tim_channel == TIM_Channel_3)
    {
        TIM_OC3Init(tim_periph, &TIM_OCInitStructure);
    }
    else if (tim_channel == TIM_Channel_4)
    {
        TIM_OC4Init(tim_periph, &TIM_OCInitStructure);
    }
    
    return PWM_OK;
}

/**
 * @brief 检查PWM是否已初始化
 */
uint8_t PWM_IsInitialized(PWM_Instance_t instance)
{
    if (instance >= PWM_INSTANCE_MAX)
    {
        return 0;
    }
    
    return g_pwm_initialized[instance] ? 1 : 0;
}

/**
 * @brief 获取定时器外设指针
 */
TIM_TypeDef* PWM_GetPeriph(PWM_Instance_t instance)
{
    if (instance >= PWM_INSTANCE_MAX)
    {
        return NULL;
    }
    
    if (!g_pwm_initialized[instance])
    {
        return NULL;
    }
    
    return g_pwm_configs[instance].tim_periph;
}

/* ========== 互补输出和死区时间功能实现（仅TIM1/TIM8） ========== */

/**
 * @brief 使能PWM互补输出
 */
PWM_Status_t PWM_EnableComplementary(PWM_Instance_t instance, PWM_Channel_t channel)
{
    TIM_TypeDef *tim_periph;
    uint16_t tim_channel;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    
    if (instance >= PWM_INSTANCE_MAX)
    {
        return PWM_ERROR_INVALID_PARAM;
    }
    
    if (!g_pwm_initialized[instance])
    {
        return PWM_ERROR_NOT_INITIALIZED;
    }
    
    if (channel >= PWM_CHANNEL_MAX)
    {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    tim_periph = g_pwm_configs[instance].tim_periph;
    
    /* 仅TIM1/TIM8支持互补输出 */
    if (tim_periph != TIM1)
    {
        return PWM_ERROR_INVALID_PERIPH;
    }
    
    /* 获取定时器通道 */
    switch (channel)
    {
        case PWM_CHANNEL_1: tim_channel = TIM_Channel_1; break;
        case PWM_CHANNEL_2: tim_channel = TIM_Channel_2; break;
        case PWM_CHANNEL_3: tim_channel = TIM_Channel_3; break;
        case PWM_CHANNEL_4: tim_channel = TIM_Channel_4; break;
        default: return PWM_ERROR_INVALID_CHANNEL;
    }
    
    /* 读取当前OC配置 */
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;  /* 使能互补输出 */
    TIM_OCInitStructure.TIM_Pulse = TIM_GetCapturex(tim_periph, tim_channel);
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
    TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
    TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;
    
    /* 重新配置OC通道 */
    switch (channel)
    {
        case PWM_CHANNEL_1: TIM_OC1Init(tim_periph, &TIM_OCInitStructure); TIM_OC1PreloadConfig(tim_periph, TIM_OCPreload_Enable); break;
        case PWM_CHANNEL_2: TIM_OC2Init(tim_periph, &TIM_OCInitStructure); TIM_OC2PreloadConfig(tim_periph, TIM_OCPreload_Enable); break;
        case PWM_CHANNEL_3: TIM_OC3Init(tim_periph, &TIM_OCInitStructure); TIM_OC3PreloadConfig(tim_periph, TIM_OCPreload_Enable); break;
        case PWM_CHANNEL_4: TIM_OC4Init(tim_periph, &TIM_OCInitStructure); TIM_OC4PreloadConfig(tim_periph, TIM_OCPreload_Enable); break;
        default: return PWM_ERROR_INVALID_CHANNEL;
    }
    
    /* 使能主输出 */
    TIM_CtrlPWMOutputs(tim_periph, ENABLE);
    
    return PWM_OK;
}

/**
 * @brief 禁用PWM互补输出
 */
PWM_Status_t PWM_DisableComplementary(PWM_Instance_t instance, PWM_Channel_t channel)
{
    TIM_TypeDef *tim_periph;
    uint16_t tim_channel;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    
    if (instance >= PWM_INSTANCE_MAX)
    {
        return PWM_ERROR_INVALID_PARAM;
    }
    
    if (!g_pwm_initialized[instance])
    {
        return PWM_ERROR_NOT_INITIALIZED;
    }
    
    if (channel >= PWM_CHANNEL_MAX)
    {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    tim_periph = g_pwm_configs[instance].tim_periph;
    
    /* 仅TIM1/TIM8支持互补输出 */
    if (tim_periph != TIM1)
    {
        return PWM_ERROR_INVALID_PERIPH;
    }
    
    /* 获取定时器通道 */
    switch (channel)
    {
        case PWM_CHANNEL_1: tim_channel = TIM_Channel_1; break;
        case PWM_CHANNEL_2: tim_channel = TIM_Channel_2; break;
        case PWM_CHANNEL_3: tim_channel = TIM_Channel_3; break;
        case PWM_CHANNEL_4: tim_channel = TIM_Channel_4; break;
        default: return PWM_ERROR_INVALID_CHANNEL;
    }
    
    /* 读取当前OC配置 */
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Disable;  /* 禁用互补输出 */
    TIM_OCInitStructure.TIM_Pulse = TIM_GetCapturex(tim_periph, tim_channel);
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
    TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
    TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;
    
    /* 重新配置OC通道 */
    switch (channel)
    {
        case PWM_CHANNEL_1: TIM_OC1Init(tim_periph, &TIM_OCInitStructure); TIM_OC1PreloadConfig(tim_periph, TIM_OCPreload_Enable); break;
        case PWM_CHANNEL_2: TIM_OC2Init(tim_periph, &TIM_OCInitStructure); TIM_OC2PreloadConfig(tim_periph, TIM_OCPreload_Enable); break;
        case PWM_CHANNEL_3: TIM_OC3Init(tim_periph, &TIM_OCInitStructure); TIM_OC3PreloadConfig(tim_periph, TIM_OCPreload_Enable); break;
        case PWM_CHANNEL_4: TIM_OC4Init(tim_periph, &TIM_OCInitStructure); TIM_OC4PreloadConfig(tim_periph, TIM_OCPreload_Enable); break;
        default: return PWM_ERROR_INVALID_CHANNEL;
    }
    
    return PWM_OK;
}

/**
 * @brief 设置PWM死区时间
 */
PWM_Status_t PWM_SetDeadTime(PWM_Instance_t instance, uint16_t dead_time_ns)
{
    TIM_TypeDef *tim_periph;
    TIM_BDTRInitTypeDef TIM_BDTRInitStructure;
    uint32_t tim_clk;
    uint16_t dtg;
    
    if (instance >= PWM_INSTANCE_MAX)
    {
        return PWM_ERROR_INVALID_PARAM;
    }
    
    if (!g_pwm_initialized[instance])
    {
        return PWM_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = g_pwm_configs[instance].tim_periph;
    
    /* 仅TIM1/TIM8支持死区时间 */
    if (tim_periph != TIM1)
    {
        return PWM_ERROR_INVALID_PERIPH;
    }
    
    /* 获取定时器时钟频率 */
    tim_clk = PWM_GetTimerClock(tim_periph);
    if (tim_clk == 0)
    {
        return PWM_ERROR_INVALID_PERIPH;
    }
    
    /* 计算DTG值：死区时间 = (DTG[7:0] + 1) * tDTS，其中tDTS = 1 / 定时器时钟频率 */
    /* 简化计算：DTG = (dead_time_ns * tim_clk / 1000000000) - 1 */
    dtg = (uint16_t)((uint32_t)dead_time_ns * tim_clk / 1000000000UL);
    if (dtg > 0)
    {
        dtg--;
    }
    if (dtg > 127)
    {
        dtg = 127;  /* DTG最大值为127 */
    }
    
    /* 配置BDTR寄存器 */
    TIM_BDTRInitStructure.TIM_OSSRState = TIM_OSSRState_Enable;
    TIM_BDTRInitStructure.TIM_OSSIState = TIM_OSSIState_Enable;
    TIM_BDTRInitStructure.TIM_LOCKLevel = TIM_LOCKLevel_OFF;
    TIM_BDTRInitStructure.TIM_DeadTime = dtg;
    TIM_BDTRInitStructure.TIM_Break = TIM_Break_Disable;
    TIM_BDTRInitStructure.TIM_BreakPolarity = TIM_BreakPolarity_Low;
    TIM_BDTRInitStructure.TIM_AutomaticOutput = TIM_AutomaticOutput_Enable;
    
    TIM_BDTRConfig(tim_periph, &TIM_BDTRInitStructure);
    
    return PWM_OK;
}

/**
 * @brief 使能PWM主输出（MOE）
 */
PWM_Status_t PWM_EnableMainOutput(PWM_Instance_t instance)
{
    TIM_TypeDef *tim_periph;
    
    if (instance >= PWM_INSTANCE_MAX)
    {
        return PWM_ERROR_INVALID_PARAM;
    }
    
    if (!g_pwm_initialized[instance])
    {
        return PWM_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = g_pwm_configs[instance].tim_periph;
    
    /* 仅TIM1/TIM8支持主输出使能 */
    if (tim_periph != TIM1)
    {
        return PWM_ERROR_INVALID_PERIPH;
    }
    
    TIM_CtrlPWMOutputs(tim_periph, ENABLE);
    
    return PWM_OK;
}

/**
 * @brief 禁用PWM主输出（MOE）
 */
PWM_Status_t PWM_DisableMainOutput(PWM_Instance_t instance)
{
    TIM_TypeDef *tim_periph;
    
    if (instance >= PWM_INSTANCE_MAX)
    {
        return PWM_ERROR_INVALID_PARAM;
    }
    
    if (!g_pwm_initialized[instance])
    {
        return PWM_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = g_pwm_configs[instance].tim_periph;
    
    /* 仅TIM1/TIM8支持主输出使能 */
    if (tim_periph != TIM1)
    {
        return PWM_ERROR_INVALID_PERIPH;
    }
    
    TIM_CtrlPWMOutputs(tim_periph, DISABLE);
    
    return PWM_OK;
}

/**
 * @brief 使能PWM刹车功能
 */
PWM_Status_t PWM_EnableBrake(PWM_Instance_t instance, PWM_BrakeSource_t source, PWM_BrakePolarity_t polarity)
{
    TIM_TypeDef *tim_periph;
    TIM_BDTRInitTypeDef TIM_BDTRInitStructure;
    uint16_t dtg;
    
    if (instance >= PWM_INSTANCE_MAX)
    {
        return PWM_ERROR_INVALID_PARAM;
    }
    
    if (!g_pwm_initialized[instance])
    {
        return PWM_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = g_pwm_configs[instance].tim_periph;
    
    /* 仅TIM1/TIM8支持刹车功能 */
    if (tim_periph != TIM1)
    {
        return PWM_ERROR_INVALID_PERIPH;
    }
    
    /* 读取当前BDTR配置 */
    dtg = (uint16_t)(tim_periph->BDTR & TIM_BDTR_DTG);
    
    /* 配置BDTR寄存器 */
    TIM_BDTRInitStructure.TIM_OSSRState = TIM_OSSRState_Enable;
    TIM_BDTRInitStructure.TIM_OSSIState = TIM_OSSIState_Enable;
    TIM_BDTRInitStructure.TIM_LOCKLevel = TIM_LOCKLevel_OFF;
    TIM_BDTRInitStructure.TIM_DeadTime = dtg;
    TIM_BDTRInitStructure.TIM_Break = TIM_Break_Enable;
    TIM_BDTRInitStructure.TIM_BreakPolarity = (polarity == PWM_BRAKE_POLARITY_HIGH) ? TIM_BreakPolarity_High : TIM_BreakPolarity_Low;
    TIM_BDTRInitStructure.TIM_AutomaticOutput = TIM_AutomaticOutput_Enable;
    
    TIM_BDTRConfig(tim_periph, &TIM_BDTRInitStructure);
    
    return PWM_OK;
}

/**
 * @brief 禁用PWM刹车功能
 */
PWM_Status_t PWM_DisableBrake(PWM_Instance_t instance)
{
    TIM_TypeDef *tim_periph;
    TIM_BDTRInitTypeDef TIM_BDTRInitStructure;
    uint16_t dtg;
    
    if (instance >= PWM_INSTANCE_MAX)
    {
        return PWM_ERROR_INVALID_PARAM;
    }
    
    if (!g_pwm_initialized[instance])
    {
        return PWM_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = g_pwm_configs[instance].tim_periph;
    
    /* 仅TIM1/TIM8支持刹车功能 */
    if (tim_periph != TIM1)
    {
        return PWM_ERROR_INVALID_PERIPH;
    }
    
    /* 读取当前BDTR配置 */
    dtg = (uint16_t)(tim_periph->BDTR & TIM_BDTR_DTG);
    
    /* 配置BDTR寄存器 */
    TIM_BDTRInitStructure.TIM_OSSRState = TIM_OSSRState_Enable;
    TIM_BDTRInitStructure.TIM_OSSIState = TIM_OSSIState_Enable;
    TIM_BDTRInitStructure.TIM_LOCKLevel = TIM_LOCKLevel_OFF;
    TIM_BDTRInitStructure.TIM_DeadTime = dtg;
    TIM_BDTRInitStructure.TIM_Break = TIM_Break_Disable;
    TIM_BDTRInitStructure.TIM_BreakPolarity = TIM_BreakPolarity_Low;
    TIM_BDTRInitStructure.TIM_AutomaticOutput = TIM_AutomaticOutput_Enable;
    
    TIM_BDTRConfig(tim_periph, &TIM_BDTRInitStructure);
    
    return PWM_OK;
}

/**
 * @brief 设置PWM对齐模式
 */
PWM_Status_t PWM_SetAlignMode(PWM_Instance_t instance, PWM_AlignMode_t align_mode)
{
    TIM_TypeDef *tim_periph;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    uint16_t counter_mode;
    
    if (instance >= PWM_INSTANCE_MAX)
    {
        return PWM_ERROR_INVALID_PARAM;
    }
    
    if (!g_pwm_initialized[instance])
    {
        return PWM_ERROR_NOT_INITIALIZED;
    }
    
    if (align_mode > PWM_ALIGN_CENTER_3)
    {
        return PWM_ERROR_INVALID_PARAM;
    }
    
    tim_periph = g_pwm_configs[instance].tim_periph;
    
    /* 转换对齐模式 */
    switch (align_mode)
    {
        case PWM_ALIGN_EDGE: counter_mode = TIM_CounterMode_Up; break;
        case PWM_ALIGN_CENTER_1: counter_mode = TIM_CounterMode_CenterAligned1; break;
        case PWM_ALIGN_CENTER_2: counter_mode = TIM_CounterMode_CenterAligned2; break;
        case PWM_ALIGN_CENTER_3: counter_mode = TIM_CounterMode_CenterAligned3; break;
        default: return PWM_ERROR_INVALID_PARAM;
    }
    
    /* 读取当前时基配置 */
    TIM_TimeBaseStructure.TIM_Period = tim_periph->ARR;
    TIM_TimeBaseStructure.TIM_Prescaler = tim_periph->PSC;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = counter_mode;
    
    /* 重新配置时基 */
    TIM_TimeBaseInit(tim_periph, &TIM_TimeBaseStructure);
    
    return PWM_OK;
}

#endif /* CONFIG_MODULE_TIMER_ENABLED */

