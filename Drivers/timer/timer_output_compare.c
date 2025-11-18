/**
 * @file timer_output_compare.c
 * @brief 定时器输出比较模块实现
 */

#include "timer_output_compare.h"
#include "board.h"
#include "config.h"
#include "error_handler.h"
#include "gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"
#include "system_stm32f10x.h"
#include <stdbool.h>
#include <stddef.h>

#if CONFIG_MODULE_TIMER_ENABLED

/* 定时器外设映射 */
static TIM_TypeDef *oc_tim_periphs[] = {
    TIM1,  /* OC_INSTANCE_TIM1 */
    TIM2,  /* OC_INSTANCE_TIM2 */
    TIM3,  /* OC_INSTANCE_TIM3 */
    TIM4,  /* OC_INSTANCE_TIM4 */
};

/* 定时器通道映射 */
static const uint16_t oc_tim_channels[] = {
    TIM_Channel_1,  /* OC_CHANNEL_1 */
    TIM_Channel_2,  /* OC_CHANNEL_2 */
    TIM_Channel_3,  /* OC_CHANNEL_3 */
    TIM_Channel_4,  /* OC_CHANNEL_4 */
};

/* 输出比较模式映射 */
static const uint16_t oc_mode_map[] = {
    TIM_OCMode_Timing,    /* OC_MODE_TIMING */
    TIM_OCMode_Active,    /* OC_MODE_ACTIVE */
    TIM_OCMode_Inactive,  /* OC_MODE_INACTIVE */
    TIM_OCMode_Toggle,    /* OC_MODE_TOGGLE */
};

/* 初始化标志 */
static bool g_oc_initialized[OC_INSTANCE_MAX][OC_CHANNEL_MAX] = {false};

/**
 * @brief 获取定时器外设时钟
 */
static uint32_t OC_GetPeriphClock(TIM_TypeDef *tim_periph)
{
    if (tim_periph == TIM1)
    {
        return RCC_APB2Periph_TIM1;
    }
    else if (tim_periph == TIM2)
    {
        return RCC_APB1Periph_TIM2;
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
 * @brief 获取GPIO引脚配置（需要根据定时器和通道确定）
 */
static void OC_GetGPIOConfig(OC_Instance_t instance, OC_Channel_t channel, 
                              GPIO_TypeDef **port, uint16_t *pin)
{
    /* 根据定时器和通道确定GPIO引脚 */
    switch (instance)
    {
        case OC_INSTANCE_TIM1:
            if (channel == OC_CHANNEL_1) { *port = GPIOA; *pin = GPIO_Pin_8; }
            else if (channel == OC_CHANNEL_2) { *port = GPIOA; *pin = GPIO_Pin_9; }
            else if (channel == OC_CHANNEL_3) { *port = GPIOA; *pin = GPIO_Pin_10; }
            else if (channel == OC_CHANNEL_4) { *port = GPIOA; *pin = GPIO_Pin_11; }
            break;
        case OC_INSTANCE_TIM2:
            if (channel == OC_CHANNEL_1) { *port = GPIOA; *pin = GPIO_Pin_0; }
            else if (channel == OC_CHANNEL_2) { *port = GPIOA; *pin = GPIO_Pin_1; }
            else if (channel == OC_CHANNEL_3) { *port = GPIOA; *pin = GPIO_Pin_2; }
            else if (channel == OC_CHANNEL_4) { *port = GPIOA; *pin = GPIO_Pin_3; }
            break;
        case OC_INSTANCE_TIM3:
            if (channel == OC_CHANNEL_1) { *port = GPIOA; *pin = GPIO_Pin_6; }
            else if (channel == OC_CHANNEL_2) { *port = GPIOA; *pin = GPIO_Pin_7; }
            else if (channel == OC_CHANNEL_3) { *port = GPIOB; *pin = GPIO_Pin_0; }
            else if (channel == OC_CHANNEL_4) { *port = GPIOB; *pin = GPIO_Pin_1; }
            break;
        case OC_INSTANCE_TIM4:
            if (channel == OC_CHANNEL_1) { *port = GPIOB; *pin = GPIO_Pin_6; }
            else if (channel == OC_CHANNEL_2) { *port = GPIOB; *pin = GPIO_Pin_7; }
            else if (channel == OC_CHANNEL_3) { *port = GPIOB; *pin = GPIO_Pin_8; }
            else if (channel == OC_CHANNEL_4) { *port = GPIOB; *pin = GPIO_Pin_9; }
            break;
        default:
            *port = GPIOA; *pin = GPIO_Pin_0;
            break;
    }
}

/**
 * @brief 输出比较初始化
 */
OC_Status_t OC_Init(OC_Instance_t instance, OC_Channel_t channel, OC_Mode_t mode, uint16_t period, uint16_t compare_value)
{
    TIM_TypeDef *tim_periph;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    GPIO_TypeDef *gpio_port;
    uint16_t gpio_pin;
    uint32_t tim_clock;
    
    /* 参数校验 */
    if (instance >= OC_INSTANCE_MAX || channel >= OC_CHANNEL_MAX)
    {
        return OC_ERROR_INVALID_PARAM;
    }
    
    if (mode >= 4) /* OC_MODE_TOGGLE = 3 */
    {
        return OC_ERROR_INVALID_PARAM;
    }
    
    if (g_oc_initialized[instance][channel])
    {
        ERROR_HANDLER_Report(ERROR_BASE_TIMER, __FILE__, __LINE__, "OC already initialized");
        return OC_ERROR_ALREADY_INITIALIZED;
    }
    
    tim_periph = oc_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return OC_ERROR_INVALID_PERIPH;
    }
    
    /* 使能定时器和GPIO时钟 */
    uint32_t periph_clock = OC_GetPeriphClock(tim_periph);
    if (tim_periph == TIM1)
    {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    }
    else
    {
        RCC_APB1PeriphClockCmd(periph_clock, ENABLE);
    }
    
    /* 获取GPIO配置 */
    OC_GetGPIOConfig(instance, channel, &gpio_port, &gpio_pin);
    GPIO_EnableClock(gpio_port);
    
    /* 配置GPIO为复用推挽输出模式（输出比较） */
    GPIO_Config(gpio_port, gpio_pin, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHz);
    
    /* 配置定时器时基 */
    tim_clock = 72000000; /* 假设72MHz系统时钟，实际应该从系统获取 */
    RCC_ClocksTypeDef RCC_Clocks;
    RCC_GetClocksFreq(&RCC_Clocks);
    if (tim_periph == TIM1)
    {
        tim_clock = RCC_Clocks.PCLK2_Frequency;
        if (RCC_Clocks.HCLK_Frequency != RCC_Clocks.PCLK2_Frequency)
        {
            tim_clock = RCC_Clocks.PCLK2_Frequency * 2;
        }
    }
    else
    {
        tim_clock = RCC_Clocks.PCLK1_Frequency;
        if (RCC_Clocks.HCLK_Frequency != RCC_Clocks.PCLK1_Frequency)
        {
            tim_clock = RCC_Clocks.PCLK1_Frequency * 2;
        }
    }
    
    TIM_TimeBaseStructure.TIM_Period = period;
    TIM_TimeBaseStructure.TIM_Prescaler = 0;  /* 默认不分频 */
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(tim_periph, &TIM_TimeBaseStructure);
    
    /* 配置输出比较 */
    TIM_OCInitStructure.TIM_OCMode = oc_mode_map[mode];
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = compare_value;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    
    /* 根据通道配置输出比较 */
    switch (channel)
    {
        case OC_CHANNEL_1:
            TIM_OC1Init(tim_periph, &TIM_OCInitStructure);
            TIM_OC1PreloadConfig(tim_periph, TIM_OCPreload_Disable);
            break;
        case OC_CHANNEL_2:
            TIM_OC2Init(tim_periph, &TIM_OCInitStructure);
            TIM_OC2PreloadConfig(tim_periph, TIM_OCPreload_Disable);
            break;
        case OC_CHANNEL_3:
            TIM_OC3Init(tim_periph, &TIM_OCInitStructure);
            TIM_OC3PreloadConfig(tim_periph, TIM_OCPreload_Disable);
            break;
        case OC_CHANNEL_4:
            TIM_OC4Init(tim_periph, &TIM_OCInitStructure);
            TIM_OC4PreloadConfig(tim_periph, TIM_OCPreload_Disable);
            break;
        default:
            return OC_ERROR_INVALID_CHANNEL;
    }
    
    /* 使能定时器预装载寄存器 */
    TIM_ARRPreloadConfig(tim_periph, ENABLE);
    
    /* 对于TIM1，需要使能主输出 */
    if (tim_periph == TIM1)
    {
        TIM_CtrlPWMOutputs(TIM1, ENABLE);
    }
    
    g_oc_initialized[instance][channel] = true;
    
    return OC_OK;
}

/**
 * @brief 输出比较反初始化
 */
OC_Status_t OC_Deinit(OC_Instance_t instance, OC_Channel_t channel)
{
    TIM_TypeDef *tim_periph;
    
    if (instance >= OC_INSTANCE_MAX || channel >= OC_CHANNEL_MAX)
    {
        return OC_ERROR_INVALID_PARAM;
    }
    
    if (!g_oc_initialized[instance][channel])
    {
        return OC_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = oc_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return OC_ERROR_INVALID_PERIPH;
    }
    
    /* 禁用输出比较通道 */
    TIM_CCxCmd(tim_periph, oc_tim_channels[channel], TIM_CCx_Disable);
    
    /* 禁用定时器 */
    TIM_Cmd(tim_periph, DISABLE);
    
    g_oc_initialized[instance][channel] = false;
    
    return OC_OK;
}

/**
 * @brief 设置比较值
 */
OC_Status_t OC_SetCompareValue(OC_Instance_t instance, OC_Channel_t channel, uint16_t compare_value)
{
    TIM_TypeDef *tim_periph;
    
    if (instance >= OC_INSTANCE_MAX || channel >= OC_CHANNEL_MAX)
    {
        return OC_ERROR_INVALID_PARAM;
    }
    
    if (!g_oc_initialized[instance][channel])
    {
        return OC_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = oc_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return OC_ERROR_INVALID_PERIPH;
    }
    
    /* 设置比较值 */
    switch (channel)
    {
        case OC_CHANNEL_1: TIM_SetCompare1(tim_periph, compare_value); break;
        case OC_CHANNEL_2: TIM_SetCompare2(tim_periph, compare_value); break;
        case OC_CHANNEL_3: TIM_SetCompare3(tim_periph, compare_value); break;
        case OC_CHANNEL_4: TIM_SetCompare4(tim_periph, compare_value); break;
        default: return OC_ERROR_INVALID_CHANNEL;
    }
    
    return OC_OK;
}

/**
 * @brief 获取比较值
 */
OC_Status_t OC_GetCompareValue(OC_Instance_t instance, OC_Channel_t channel, uint16_t *compare_value)
{
    TIM_TypeDef *tim_periph;
    
    if (instance >= OC_INSTANCE_MAX || channel >= OC_CHANNEL_MAX)
    {
        return OC_ERROR_INVALID_PARAM;
    }
    
    if (!g_oc_initialized[instance][channel])
    {
        return OC_ERROR_NOT_INITIALIZED;
    }
    
    if (compare_value == NULL)
    {
        return OC_ERROR_INVALID_PARAM;
    }
    
    tim_periph = oc_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return OC_ERROR_INVALID_PERIPH;
    }
    
    /* 读取比较值 */
    switch (channel)
    {
        case OC_CHANNEL_1: *compare_value = tim_periph->CCR1; break;
        case OC_CHANNEL_2: *compare_value = tim_periph->CCR2; break;
        case OC_CHANNEL_3: *compare_value = tim_periph->CCR3; break;
        case OC_CHANNEL_4: *compare_value = tim_periph->CCR4; break;
        default: return OC_ERROR_INVALID_CHANNEL;
    }
    
    return OC_OK;
}

/**
 * @brief 启动输出比较
 */
OC_Status_t OC_Start(OC_Instance_t instance, OC_Channel_t channel)
{
    TIM_TypeDef *tim_periph;
    
    if (instance >= OC_INSTANCE_MAX || channel >= OC_CHANNEL_MAX)
    {
        return OC_ERROR_INVALID_PARAM;
    }
    
    if (!g_oc_initialized[instance][channel])
    {
        return OC_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = oc_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return OC_ERROR_INVALID_PERIPH;
    }
    
    /* 使能输出比较通道 */
    TIM_CCxCmd(tim_periph, oc_tim_channels[channel], TIM_CCx_Enable);
    
    /* 使能定时器 */
    TIM_Cmd(tim_periph, ENABLE);
    
    return OC_OK;
}

/**
 * @brief 停止输出比较
 */
OC_Status_t OC_Stop(OC_Instance_t instance, OC_Channel_t channel)
{
    TIM_TypeDef *tim_periph;
    
    if (instance >= OC_INSTANCE_MAX || channel >= OC_CHANNEL_MAX)
    {
        return OC_ERROR_INVALID_PARAM;
    }
    
    if (!g_oc_initialized[instance][channel])
    {
        return OC_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = oc_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return OC_ERROR_INVALID_PERIPH;
    }
    
    /* 禁用输出比较通道 */
    TIM_CCxCmd(tim_periph, oc_tim_channels[channel], TIM_CCx_Disable);
    
    return OC_OK;
}

/**
 * @brief 生成单脉冲
 */
OC_Status_t OC_GenerateSinglePulse(OC_Instance_t instance, OC_Channel_t channel, uint16_t pulse_width)
{
    TIM_TypeDef *tim_periph;
    
    if (instance >= OC_INSTANCE_MAX || channel >= OC_CHANNEL_MAX)
    {
        return OC_ERROR_INVALID_PARAM;
    }
    
    if (!g_oc_initialized[instance][channel])
    {
        return OC_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = oc_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return OC_ERROR_INVALID_PERIPH;
    }
    
    /* 设置比较值为脉冲宽度 */
    OC_SetCompareValue(instance, channel, pulse_width);
    
    /* 配置为单脉冲模式 */
    TIM_SelectOnePulseMode(tim_periph, TIM_OPMode_Single);
    
    /* 启动输出比较 */
    OC_Start(instance, channel);
    
    return OC_OK;
}

/**
 * @brief 检查输出比较是否已初始化
 */
uint8_t OC_IsInitialized(OC_Instance_t instance, OC_Channel_t channel)
{
    if (instance >= OC_INSTANCE_MAX || channel >= OC_CHANNEL_MAX)
    {
        return 0;
    }
    
    return g_oc_initialized[instance][channel] ? 1 : 0;
}

/* ========== 高级功能实现 ========== */

/**
 * @brief 使能输出比较预装载
 */
OC_Status_t OC_EnablePreload(OC_Instance_t instance, OC_Channel_t channel)
{
    TIM_TypeDef *tim_periph;
    uint16_t tim_channel;
    
    if (instance >= OC_INSTANCE_MAX || channel >= OC_CHANNEL_MAX)
    {
        return OC_ERROR_INVALID_PARAM;
    }
    
    if (!g_oc_initialized[instance][channel])
    {
        return OC_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = oc_tim_periphs[instance];
    tim_channel = oc_tim_channels[channel];
    
    /* 使能预装载 */
    switch (channel)
    {
        case OC_CHANNEL_1: TIM_OC1PreloadConfig(tim_periph, TIM_OCPreload_Enable); break;
        case OC_CHANNEL_2: TIM_OC2PreloadConfig(tim_periph, TIM_OCPreload_Enable); break;
        case OC_CHANNEL_3: TIM_OC3PreloadConfig(tim_periph, TIM_OCPreload_Enable); break;
        case OC_CHANNEL_4: TIM_OC4PreloadConfig(tim_periph, TIM_OCPreload_Enable); break;
        default: return OC_ERROR_INVALID_CHANNEL;
    }
    
    return OC_OK;
}

/**
 * @brief 禁用输出比较预装载
 */
OC_Status_t OC_DisablePreload(OC_Instance_t instance, OC_Channel_t channel)
{
    TIM_TypeDef *tim_periph;
    uint16_t tim_channel;
    
    if (instance >= OC_INSTANCE_MAX || channel >= OC_CHANNEL_MAX)
    {
        return OC_ERROR_INVALID_PARAM;
    }
    
    if (!g_oc_initialized[instance][channel])
    {
        return OC_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = oc_tim_periphs[instance];
    tim_channel = oc_tim_channels[channel];
    
    /* 禁用预装载 */
    switch (channel)
    {
        case OC_CHANNEL_1: TIM_OC1PreloadConfig(tim_periph, TIM_OCPreload_Disable); break;
        case OC_CHANNEL_2: TIM_OC2PreloadConfig(tim_periph, TIM_OCPreload_Disable); break;
        case OC_CHANNEL_3: TIM_OC3PreloadConfig(tim_periph, TIM_OCPreload_Disable); break;
        case OC_CHANNEL_4: TIM_OC4PreloadConfig(tim_periph, TIM_OCPreload_Disable); break;
        default: return OC_ERROR_INVALID_CHANNEL;
    }
    
    return OC_OK;
}

/**
 * @brief 清除输出比较输出（OCxClear）
 */
OC_Status_t OC_ClearOutput(OC_Instance_t instance, OC_Channel_t channel)
{
    TIM_TypeDef *tim_periph;
    uint16_t tim_channel;
    
    if (instance >= OC_INSTANCE_MAX || channel >= OC_CHANNEL_MAX)
    {
        return OC_ERROR_INVALID_PARAM;
    }
    
    if (!g_oc_initialized[instance][channel])
    {
        return OC_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = oc_tim_periphs[instance];
    tim_channel = oc_tim_channels[channel];
    
    /* 清除输出（OCxClear） */
    switch (channel)
    {
        case OC_CHANNEL_1: TIM_OC1ClearConfig(tim_periph, TIM_OCClear_Enable); break;
        case OC_CHANNEL_2: TIM_OC2ClearConfig(tim_periph, TIM_OCClear_Enable); break;
        case OC_CHANNEL_3: TIM_OC3ClearConfig(tim_periph, TIM_OCClear_Enable); break;
        case OC_CHANNEL_4: TIM_OC4ClearConfig(tim_periph, TIM_OCClear_Enable); break;
        default: return OC_ERROR_INVALID_CHANNEL;
    }
    
    return OC_OK;
}

/**
 * @brief 强制输出比较输出为高电平
 */
OC_Status_t OC_ForceOutputHigh(OC_Instance_t instance, OC_Channel_t channel)
{
    TIM_TypeDef *tim_periph;
    uint16_t tim_channel;
    
    if (instance >= OC_INSTANCE_MAX || channel >= OC_CHANNEL_MAX)
    {
        return OC_ERROR_INVALID_PARAM;
    }
    
    if (!g_oc_initialized[instance][channel])
    {
        return OC_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = oc_tim_periphs[instance];
    tim_channel = oc_tim_channels[channel];
    
    /* 强制输出高电平 */
    switch (channel)
    {
        case OC_CHANNEL_1: TIM_ForcedOC1Config(tim_periph, TIM_ForcedAction_Active); break;
        case OC_CHANNEL_2: TIM_ForcedOC2Config(tim_periph, TIM_ForcedAction_Active); break;
        case OC_CHANNEL_3: TIM_ForcedOC3Config(tim_periph, TIM_ForcedAction_Active); break;
        case OC_CHANNEL_4: TIM_ForcedOC4Config(tim_periph, TIM_ForcedAction_Active); break;
        default: return OC_ERROR_INVALID_CHANNEL;
    }
    
    return OC_OK;
}

/**
 * @brief 强制输出比较输出为低电平
 */
OC_Status_t OC_ForceOutputLow(OC_Instance_t instance, OC_Channel_t channel)
{
    TIM_TypeDef *tim_periph;
    uint16_t tim_channel;
    
    if (instance >= OC_INSTANCE_MAX || channel >= OC_CHANNEL_MAX)
    {
        return OC_ERROR_INVALID_PARAM;
    }
    
    if (!g_oc_initialized[instance][channel])
    {
        return OC_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = oc_tim_periphs[instance];
    tim_channel = oc_tim_channels[channel];
    
    /* 强制输出低电平 */
    switch (channel)
    {
        case OC_CHANNEL_1: TIM_ForcedOC1Config(tim_periph, TIM_ForcedAction_InActive); break;
        case OC_CHANNEL_2: TIM_ForcedOC2Config(tim_periph, TIM_ForcedAction_InActive); break;
        case OC_CHANNEL_3: TIM_ForcedOC3Config(tim_periph, TIM_ForcedAction_InActive); break;
        case OC_CHANNEL_4: TIM_ForcedOC4Config(tim_periph, TIM_ForcedAction_InActive); break;
        default: return OC_ERROR_INVALID_CHANNEL;
    }
    
    return OC_OK;
}

#endif /* CONFIG_MODULE_TIMER_ENABLED */

