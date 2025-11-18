/**
 * @file timer_input_capture.c
 * @brief 定时器输入捕获模块实现
 */

#include "timer_input_capture.h"
#include "board.h"
#include "config.h"
#include "error_handler.h"
#include "gpio.h"
#include "delay.h"
#include "nvic.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"
#include "misc.h"
#include "system_stm32f10x.h"
#include <stdbool.h>
#include <stddef.h>

#if CONFIG_MODULE_TIMER_ENABLED

/* 定时器外设映射 */
static TIM_TypeDef *ic_tim_periphs[] = {
    TIM1,  /* IC_INSTANCE_TIM1 */
    TIM2,  /* IC_INSTANCE_TIM2 */
    TIM3,  /* IC_INSTANCE_TIM3 */
    TIM4,  /* IC_INSTANCE_TIM4 */
};

/* 定时器通道映射 */
static const uint16_t ic_tim_channels[] = {
    TIM_Channel_1,  /* IC_CHANNEL_1 */
    TIM_Channel_2,  /* IC_CHANNEL_2 */
    TIM_Channel_3,  /* IC_CHANNEL_3 */
    TIM_Channel_4,  /* IC_CHANNEL_4 */
};

/* 初始化标志 */
static bool g_ic_initialized[IC_INSTANCE_MAX][IC_CHANNEL_MAX] = {false};

/* 中断回调函数数组 */
static IC_IT_Callback_t g_ic_it_callbacks[IC_INSTANCE_MAX][IC_CHANNEL_MAX][2] = {NULL};
static void *g_ic_it_user_data[IC_INSTANCE_MAX][IC_CHANNEL_MAX][2] = {NULL};

/**
 * @brief 获取定时器外设时钟
 */
static uint32_t IC_GetPeriphClock(TIM_TypeDef *tim_periph)
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
 * @brief 获取定时器实际时钟频率
 */
static uint32_t IC_GetTimerClock(TIM_TypeDef *tim_periph)
{
    RCC_ClocksTypeDef RCC_Clocks;
    uint32_t tim_clk;
    
    RCC_GetClocksFreq(&RCC_Clocks);
    
    if (tim_periph == TIM1)
    {
        tim_clk = RCC_Clocks.PCLK2_Frequency;
        if (RCC_Clocks.HCLK_Frequency != RCC_Clocks.PCLK2_Frequency)
        {
            tim_clk = RCC_Clocks.PCLK2_Frequency * 2;
        }
    }
    else
    {
        tim_clk = RCC_Clocks.PCLK1_Frequency;
        if (RCC_Clocks.HCLK_Frequency != RCC_Clocks.PCLK1_Frequency)
        {
            tim_clk = RCC_Clocks.PCLK1_Frequency * 2;
        }
    }
    
    return tim_clk;
}

/**
 * @brief 获取GPIO引脚配置（需要根据定时器和通道确定）
 */
static void IC_GetGPIOConfig(IC_Instance_t instance, IC_Channel_t channel, 
                              GPIO_TypeDef **port, uint16_t *pin)
{
    /* 这里需要根据具体的硬件配置来确定GPIO引脚 */
    /* 暂时使用默认配置，实际使用时需要根据board.h配置 */
    *port = GPIOA;
    *pin = GPIO_Pin_0;
    
    /* 根据定时器和通道确定GPIO引脚 */
    switch (instance)
    {
        case IC_INSTANCE_TIM1:
            if (channel == IC_CHANNEL_1) { *port = GPIOA; *pin = GPIO_Pin_8; }
            else if (channel == IC_CHANNEL_2) { *port = GPIOA; *pin = GPIO_Pin_9; }
            else if (channel == IC_CHANNEL_3) { *port = GPIOA; *pin = GPIO_Pin_10; }
            else if (channel == IC_CHANNEL_4) { *port = GPIOA; *pin = GPIO_Pin_11; }
            break;
        case IC_INSTANCE_TIM2:
            if (channel == IC_CHANNEL_1) { *port = GPIOA; *pin = GPIO_Pin_0; }
            else if (channel == IC_CHANNEL_2) { *port = GPIOA; *pin = GPIO_Pin_1; }
            else if (channel == IC_CHANNEL_3) { *port = GPIOA; *pin = GPIO_Pin_2; }
            else if (channel == IC_CHANNEL_4) { *port = GPIOA; *pin = GPIO_Pin_3; }
            break;
        case IC_INSTANCE_TIM3:
            if (channel == IC_CHANNEL_1) { *port = GPIOA; *pin = GPIO_Pin_6; }
            else if (channel == IC_CHANNEL_2) { *port = GPIOA; *pin = GPIO_Pin_7; }
            else if (channel == IC_CHANNEL_3) { *port = GPIOB; *pin = GPIO_Pin_0; }
            else if (channel == IC_CHANNEL_4) { *port = GPIOB; *pin = GPIO_Pin_1; }
            break;
        case IC_INSTANCE_TIM4:
            if (channel == IC_CHANNEL_1) { *port = GPIOB; *pin = GPIO_Pin_6; }
            else if (channel == IC_CHANNEL_2) { *port = GPIOB; *pin = GPIO_Pin_7; }
            else if (channel == IC_CHANNEL_3) { *port = GPIOB; *pin = GPIO_Pin_8; }
            else if (channel == IC_CHANNEL_4) { *port = GPIOB; *pin = GPIO_Pin_9; }
            break;
        default:
            break;
    }
}

/**
 * @brief 输入捕获初始化
 */
IC_Status_t IC_Init(IC_Instance_t instance, IC_Channel_t channel, IC_Polarity_t polarity)
{
    TIM_TypeDef *tim_periph;
    TIM_ICInitTypeDef TIM_ICInitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    GPIO_TypeDef *gpio_port;
    uint16_t gpio_pin;
    uint32_t tim_clock;
    
    /* 参数校验 */
    if (instance >= IC_INSTANCE_MAX || channel >= IC_CHANNEL_MAX)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    if (g_ic_initialized[instance][channel])
    {
        ERROR_HANDLER_Report(ERROR_BASE_TIMER, __FILE__, __LINE__, "IC already initialized");
        return IC_ERROR_ALREADY_INITIALIZED;
    }
    
    tim_periph = ic_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return IC_ERROR_INVALID_PERIPH;
    }
    
    /* 使能定时器和GPIO时钟 */
    RCC_APB1PeriphClockCmd(IC_GetPeriphClock(tim_periph), ENABLE);
    if (tim_periph == TIM1)
    {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    }
    
    /* 获取GPIO配置 */
    IC_GetGPIOConfig(instance, channel, &gpio_port, &gpio_pin);
    GPIO_EnableClock(gpio_port);
    
    /* 配置GPIO为浮空输入模式（输入捕获） */
    GPIO_Config(gpio_port, gpio_pin, GPIO_MODE_INPUT_FLOATING, GPIO_SPEED_50MHz);
    
    /* 配置定时器时基 */
    tim_clock = IC_GetTimerClock(tim_periph);
    TIM_TimeBaseStructure.TIM_Period = 0xFFFF;  /* 最大计数值 */
    TIM_TimeBaseStructure.TIM_Prescaler = (tim_clock / 1000000) - 1; /* 1MHz计数频率（1微秒分辨率） */
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(tim_periph, &TIM_TimeBaseStructure);
    
    /* 配置输入捕获 */
    TIM_ICInitStructure.TIM_Channel = ic_tim_channels[channel];
    TIM_ICInitStructure.TIM_ICPolarity = (polarity == IC_POLARITY_RISING) ? 
                                          TIM_ICPolarity_Rising : TIM_ICPolarity_Falling;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPrescaler_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 0x0;
    
    if (polarity == IC_POLARITY_BOTH)
    {
        /* PWM输入模式（双边沿捕获） */
        TIM_PWMIConfig(tim_periph, &TIM_ICInitStructure);
    }
    else
    {
        /* 单边沿捕获 */
        TIM_ICInit(tim_periph, &TIM_ICInitStructure);
    }
    
    /* 使能定时器 */
    TIM_Cmd(tim_periph, ENABLE);
    
    /* 初始化中断相关变量 */
    for (int i = 0; i < 2; i++)
    {
        g_ic_it_callbacks[instance][channel][i] = NULL;
        g_ic_it_user_data[instance][channel][i] = NULL;
    }
    
    g_ic_initialized[instance][channel] = true;
    
    return IC_OK;
}

/**
 * @brief 输入捕获反初始化
 */
IC_Status_t IC_Deinit(IC_Instance_t instance, IC_Channel_t channel)
{
    TIM_TypeDef *tim_periph;
    
    if (instance >= IC_INSTANCE_MAX || channel >= IC_CHANNEL_MAX)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    if (!g_ic_initialized[instance][channel])
    {
        return IC_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = ic_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return IC_ERROR_INVALID_PERIPH;
    }
    
    /* 禁用定时器 */
    TIM_Cmd(tim_periph, DISABLE);
    
    /* 禁用输入捕获通道 */
    switch (channel)
    {
        case IC_CHANNEL_1: TIM_CCxCmd(tim_periph, TIM_Channel_1, TIM_CCx_Disable); break;
        case IC_CHANNEL_2: TIM_CCxCmd(tim_periph, TIM_Channel_2, TIM_CCx_Disable); break;
        case IC_CHANNEL_3: TIM_CCxCmd(tim_periph, TIM_Channel_3, TIM_CCx_Disable); break;
        case IC_CHANNEL_4: TIM_CCxCmd(tim_periph, TIM_Channel_4, TIM_CCx_Disable); break;
        default: break;
    }
    
    g_ic_initialized[instance][channel] = false;
    
    return IC_OK;
}

/**
 * @brief 启动输入捕获
 */
IC_Status_t IC_Start(IC_Instance_t instance, IC_Channel_t channel)
{
    TIM_TypeDef *tim_periph;
    
    if (instance >= IC_INSTANCE_MAX || channel >= IC_CHANNEL_MAX)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    if (!g_ic_initialized[instance][channel])
    {
        return IC_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = ic_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return IC_ERROR_INVALID_PERIPH;
    }
    
    /* 使能输入捕获通道 */
    TIM_CCxCmd(tim_periph, ic_tim_channels[channel], TIM_CCx_Enable);
    
    /* 使能定时器 */
    TIM_Cmd(tim_periph, ENABLE);
    
    return IC_OK;
}

/**
 * @brief 停止输入捕获
 */
IC_Status_t IC_Stop(IC_Instance_t instance, IC_Channel_t channel)
{
    TIM_TypeDef *tim_periph;
    
    if (instance >= IC_INSTANCE_MAX || channel >= IC_CHANNEL_MAX)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    if (!g_ic_initialized[instance][channel])
    {
        return IC_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = ic_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return IC_ERROR_INVALID_PERIPH;
    }
    
    /* 禁用输入捕获通道 */
    TIM_CCxCmd(tim_periph, ic_tim_channels[channel], TIM_CCx_Disable);
    
    return IC_OK;
}

/**
 * @brief 读取捕获值
 */
IC_Status_t IC_ReadValue(IC_Instance_t instance, IC_Channel_t channel, uint32_t *value)
{
    TIM_TypeDef *tim_periph;
    
    if (instance >= IC_INSTANCE_MAX || channel >= IC_CHANNEL_MAX)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    if (!g_ic_initialized[instance][channel])
    {
        return IC_ERROR_NOT_INITIALIZED;
    }
    
    if (value == NULL)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    tim_periph = ic_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return IC_ERROR_INVALID_PERIPH;
    }
    
    /* 读取捕获值 */
    switch (channel)
    {
        case IC_CHANNEL_1: *value = TIM_GetCapture1(tim_periph); break;
        case IC_CHANNEL_2: *value = TIM_GetCapture2(tim_periph); break;
        case IC_CHANNEL_3: *value = TIM_GetCapture3(tim_periph); break;
        case IC_CHANNEL_4: *value = TIM_GetCapture4(tim_periph); break;
        default: return IC_ERROR_INVALID_CHANNEL;
    }
    
    return IC_OK;
}

/**
 * @brief 测量频率
 */
IC_Status_t IC_MeasureFrequency(IC_Instance_t instance, IC_Channel_t channel, uint32_t *frequency, uint32_t timeout_ms)
{
    TIM_TypeDef *tim_periph;
    uint32_t capture1, capture2;
    uint32_t period;
    uint32_t tim_clock;
    uint32_t start_time;
    
    if (instance >= IC_INSTANCE_MAX || channel >= IC_CHANNEL_MAX)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    if (!g_ic_initialized[instance][channel])
    {
        return IC_ERROR_NOT_INITIALIZED;
    }
    
    if (frequency == NULL)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    tim_periph = ic_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return IC_ERROR_INVALID_PERIPH;
    }
    
    tim_clock = IC_GetTimerClock(tim_periph);
    uint32_t prescaler = (tim_clock / 1000000); /* 1MHz计数频率 */
    
    /* 清除捕获标志 */
    TIM_ClearFlag(tim_periph, TIM_FLAG_CC1 << channel);
    
    /* 等待第一次捕获 */
    start_time = Delay_GetTick();
    while (!(TIM_GetFlagStatus(tim_periph, TIM_FLAG_CC1 << channel)) && 
           (Delay_GetTick() - start_time < timeout_ms))
    {
        /* 等待 */
    }
    
    if (!(TIM_GetFlagStatus(tim_periph, TIM_FLAG_CC1 << channel)))
    {
        return IC_ERROR_TIMEOUT;
    }
    
    /* 读取第一次捕获值 */
    switch (channel)
    {
        case IC_CHANNEL_1: capture1 = TIM_GetCapture1(tim_periph); break;
        case IC_CHANNEL_2: capture1 = TIM_GetCapture2(tim_periph); break;
        case IC_CHANNEL_3: capture1 = TIM_GetCapture3(tim_periph); break;
        case IC_CHANNEL_4: capture1 = TIM_GetCapture4(tim_periph); break;
        default: return IC_ERROR_INVALID_CHANNEL;
    }
    
    TIM_ClearFlag(tim_periph, TIM_FLAG_CC1 << channel);
    
    /* 等待第二次捕获 */
    start_time = Delay_GetTick();
    while (!(TIM_GetFlagStatus(tim_periph, TIM_FLAG_CC1 << channel)) && 
           (Delay_GetTick() - start_time < timeout_ms))
    {
        /* 等待 */
    }
    
    if (!(TIM_GetFlagStatus(tim_periph, TIM_FLAG_CC1 << channel)))
    {
        return IC_ERROR_TIMEOUT;
    }
    
    /* 读取第二次捕获值 */
    switch (channel)
    {
        case IC_CHANNEL_1: capture2 = TIM_GetCapture1(tim_periph); break;
        case IC_CHANNEL_2: capture2 = TIM_GetCapture2(tim_periph); break;
        case IC_CHANNEL_3: capture2 = TIM_GetCapture3(tim_periph); break;
        case IC_CHANNEL_4: capture2 = TIM_GetCapture4(tim_periph); break;
        default: return IC_ERROR_INVALID_CHANNEL;
    }
    
    /* 计算周期（微秒） */
    if (capture2 > capture1)
    {
        period = capture2 - capture1;
    }
    else
    {
        period = (0xFFFF - capture1) + capture2 + 1;
    }
    
    /* 计算频率 */
    if (period > 0)
    {
        *frequency = 1000000 / period; /* 频率 = 1MHz / 周期（微秒） */
    }
    else
    {
        *frequency = 0;
    }
    
    return IC_OK;
}

/**
 * @brief 测量PWM信号
 */
IC_Status_t IC_MeasurePWM(IC_Instance_t instance, IC_Channel_t channel, IC_MeasureResult_t *result, uint32_t timeout_ms)
{
    TIM_TypeDef *tim_periph;
    uint32_t ic1_value, ic2_value;
    uint32_t tim_clock;
    uint32_t start_time;
    
    if (instance >= IC_INSTANCE_MAX || channel >= IC_CHANNEL_MAX)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    if (!g_ic_initialized[instance][channel])
    {
        return IC_ERROR_NOT_INITIALIZED;
    }
    
    if (result == NULL)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    tim_periph = ic_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return IC_ERROR_INVALID_PERIPH;
    }
    
    /* PWM输入模式需要两个通道（IC1和IC2） */
    if (channel != IC_CHANNEL_1)
    {
        return IC_ERROR_INVALID_CHANNEL; /* PWM输入模式只支持通道1 */
    }
    
    tim_clock = IC_GetTimerClock(tim_periph);
    uint32_t prescaler = (tim_clock / 1000000); /* 1MHz计数频率 */
    
    /* 等待捕获完成 */
    start_time = Delay_GetTick();
    while (!(TIM_GetFlagStatus(tim_periph, TIM_FLAG_CC1) && TIM_GetFlagStatus(tim_periph, TIM_FLAG_CC2)) && 
           (Delay_GetTick() - start_time < timeout_ms))
    {
        /* 等待 */
    }
    
    if (!(TIM_GetFlagStatus(tim_periph, TIM_FLAG_CC1) && TIM_GetFlagStatus(tim_periph, TIM_FLAG_CC2)))
    {
        return IC_ERROR_TIMEOUT;
    }
    
    /* 读取捕获值 */
    ic1_value = TIM_GetCapture1(tim_periph);
    ic2_value = TIM_GetCapture2(tim_periph);
    
    /* 计算周期和脉冲宽度（微秒） */
    uint32_t period_us = (ic1_value > ic2_value) ? (ic1_value - ic2_value) : ((0xFFFF - ic2_value) + ic1_value + 1);
    uint32_t pulse_width_us = ic2_value;
    
    /* 计算频率 */
    if (period_us > 0)
    {
        result->frequency = 1000000 / period_us;
        result->period = period_us;
    }
    else
    {
        result->frequency = 0;
        result->period = 0;
    }
    
    /* 计算占空比 */
    if (period_us > 0)
    {
        result->pulse_width = pulse_width_us;
        result->duty_cycle = (pulse_width_us * 100) / period_us;
    }
    else
    {
        result->pulse_width = 0;
        result->duty_cycle = 0;
    }
    
    /* 清除标志 */
    TIM_ClearFlag(tim_periph, TIM_FLAG_CC1 | TIM_FLAG_CC2);
    
    return IC_OK;
}

/**
 * @brief 测量脉冲宽度
 */
IC_Status_t IC_MeasurePulseWidth(IC_Instance_t instance, IC_Channel_t channel, uint32_t *pulse_width_us, uint32_t timeout_ms)
{
    TIM_TypeDef *tim_periph;
    uint32_t capture1, capture2;
    uint32_t tim_clock;
    uint32_t start_time;
    
    if (instance >= IC_INSTANCE_MAX || channel >= IC_CHANNEL_MAX)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    if (!g_ic_initialized[instance][channel])
    {
        return IC_ERROR_NOT_INITIALIZED;
    }
    
    if (pulse_width_us == NULL)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    tim_periph = ic_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return IC_ERROR_INVALID_PERIPH;
    }
    
    tim_clock = IC_GetTimerClock(tim_periph);
    uint32_t prescaler = (tim_clock / 1000000); /* 1MHz计数频率 */
    
    /* 清除捕获标志 */
    TIM_ClearFlag(tim_periph, TIM_FLAG_CC1 << channel);
    
    /* 等待第一次捕获（上升沿） */
    start_time = Delay_GetTick();
    while (!(TIM_GetFlagStatus(tim_periph, TIM_FLAG_CC1 << channel)) && 
           (Delay_GetTick() - start_time < timeout_ms))
    {
        /* 等待 */
    }
    
    if (!(TIM_GetFlagStatus(tim_periph, TIM_FLAG_CC1 << channel)))
    {
        return IC_ERROR_TIMEOUT;
    }
    
    /* 读取第一次捕获值 */
    switch (channel)
    {
        case IC_CHANNEL_1: capture1 = TIM_GetCapture1(tim_periph); break;
        case IC_CHANNEL_2: capture1 = TIM_GetCapture2(tim_periph); break;
        case IC_CHANNEL_3: capture1 = TIM_GetCapture3(tim_periph); break;
        case IC_CHANNEL_4: capture1 = TIM_GetCapture4(tim_periph); break;
        default: return IC_ERROR_INVALID_CHANNEL;
    }
    
    TIM_ClearFlag(tim_periph, TIM_FLAG_CC1 << channel);
    
    /* 等待第二次捕获（下降沿） */
    start_time = Delay_GetTick();
    while (!(TIM_GetFlagStatus(tim_periph, TIM_FLAG_CC1 << channel)) && 
           (Delay_GetTick() - start_time < timeout_ms))
    {
        /* 等待 */
    }
    
    if (!(TIM_GetFlagStatus(tim_periph, TIM_FLAG_CC1 << channel)))
    {
        return IC_ERROR_TIMEOUT;
    }
    
    /* 读取第二次捕获值 */
    switch (channel)
    {
        case IC_CHANNEL_1: capture2 = TIM_GetCapture1(tim_periph); break;
        case IC_CHANNEL_2: capture2 = TIM_GetCapture2(tim_periph); break;
        case IC_CHANNEL_3: capture2 = TIM_GetCapture3(tim_periph); break;
        case IC_CHANNEL_4: capture2 = TIM_GetCapture4(tim_periph); break;
        default: return IC_ERROR_INVALID_CHANNEL;
    }
    
    /* 计算脉冲宽度（微秒） */
    if (capture2 > capture1)
    {
        *pulse_width_us = capture2 - capture1;
    }
    else
    {
        *pulse_width_us = (0xFFFF - capture1) + capture2 + 1;
    }
    
    return IC_OK;
}

/**
 * @brief 检查输入捕获是否已初始化
 */
uint8_t IC_IsInitialized(IC_Instance_t instance, IC_Channel_t channel)
{
    if (instance >= IC_INSTANCE_MAX || channel >= IC_CHANNEL_MAX)
    {
        return 0;
    }
    
    return g_ic_initialized[instance][channel] ? 1 : 0;
}

/* ========== 中断模式功能实现 ========== */

/**
 * @brief 获取输入捕获中断类型对应的SPL库中断值
 */
static uint16_t IC_GetITValue(IC_Channel_t channel, IC_IT_t it_type)
{
    uint16_t channel_it;
    
    switch (channel)
    {
        case IC_CHANNEL_1: channel_it = TIM_IT_CC1; break;
        case IC_CHANNEL_2: channel_it = TIM_IT_CC2; break;
        case IC_CHANNEL_3: channel_it = TIM_IT_CC3; break;
        case IC_CHANNEL_4: channel_it = TIM_IT_CC4; break;
        default: return 0;
    }
    
    if (it_type == IC_IT_OVERFLOW)
    {
        return TIM_IT_Update;
    }
    
    return channel_it;
}

/**
 * @brief 获取定时器中断向量
 */
static IRQn_Type IC_GetIRQn(IC_Instance_t instance)
{
    switch (instance)
    {
        case IC_INSTANCE_TIM1: return TIM1_CC_IRQn;
        case IC_INSTANCE_TIM2: return TIM2_IRQn;
        case IC_INSTANCE_TIM3: return TIM3_IRQn;
        case IC_INSTANCE_TIM4: return TIM4_IRQn;
        default: return (IRQn_Type)0;
    }
}

/**
 * @brief 使能输入捕获中断
 */
IC_Status_t IC_EnableIT(IC_Instance_t instance, IC_Channel_t channel, IC_IT_t it_type)
{
    TIM_TypeDef *tim_periph;
    uint16_t it_value;
    IRQn_Type irqn;
    
    if (instance >= IC_INSTANCE_MAX || channel >= IC_CHANNEL_MAX)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    if (!g_ic_initialized[instance][channel])
    {
        return IC_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 2)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    tim_periph = ic_tim_periphs[instance];
    it_value = IC_GetITValue(channel, it_type);
    if (it_value == 0)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    /* 使能定时器中断 */
    TIM_ITConfig(tim_periph, it_value, ENABLE);
    
    /* 使能NVIC中断 */
    irqn = IC_GetIRQn(instance);
    if (irqn != 0)
    {
        NVIC_EnableIRQ(irqn);
    }
    
    return IC_OK;
}

/**
 * @brief 禁用输入捕获中断
 */
IC_Status_t IC_DisableIT(IC_Instance_t instance, IC_Channel_t channel, IC_IT_t it_type)
{
    TIM_TypeDef *tim_periph;
    uint16_t it_value;
    
    if (instance >= IC_INSTANCE_MAX || channel >= IC_CHANNEL_MAX)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    if (!g_ic_initialized[instance][channel])
    {
        return IC_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 2)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    tim_periph = ic_tim_periphs[instance];
    it_value = IC_GetITValue(channel, it_type);
    if (it_value == 0)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    /* 禁用定时器中断 */
    TIM_ITConfig(tim_periph, it_value, DISABLE);
    
    return IC_OK;
}

/**
 * @brief 设置输入捕获中断回调函数
 */
IC_Status_t IC_SetITCallback(IC_Instance_t instance, IC_Channel_t channel, IC_IT_t it_type,
                             IC_IT_Callback_t callback, void *user_data)
{
    if (instance >= IC_INSTANCE_MAX || channel >= IC_CHANNEL_MAX)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    if (!g_ic_initialized[instance][channel])
    {
        return IC_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 2)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    g_ic_it_callbacks[instance][channel][it_type] = callback;
    g_ic_it_user_data[instance][channel][it_type] = user_data;
    
    return IC_OK;
}

/**
 * @brief 输入捕获中断服务函数
 */
void IC_IRQHandler(IC_Instance_t instance)
{
    TIM_TypeDef *tim_periph;
    uint32_t capture_value;
    uint16_t it_flags;
    
    if (instance >= IC_INSTANCE_MAX)
    {
        return;
    }
    
    tim_periph = ic_tim_periphs[instance];
    
    /* 处理溢出中断 */
    if (TIM_GetITStatus(tim_periph, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(tim_periph, TIM_IT_Update);
        
        /* 检查所有通道的溢出回调 */
        for (int ch = 0; ch < IC_CHANNEL_MAX; ch++)
        {
            if (g_ic_initialized[instance][ch] && 
                g_ic_it_callbacks[instance][ch][IC_IT_OVERFLOW] != NULL)
            {
                g_ic_it_callbacks[instance][ch][IC_IT_OVERFLOW](
                    instance, (IC_Channel_t)ch, IC_IT_OVERFLOW, 0, 
                    g_ic_it_user_data[instance][ch][IC_IT_OVERFLOW]);
            }
        }
    }
    
    /* 处理捕获中断 */
    for (int ch = 0; ch < IC_CHANNEL_MAX; ch++)
    {
        if (!g_ic_initialized[instance][ch])
        {
            continue;
        }
        
        it_flags = TIM_IT_CC1 << ch;
        if (TIM_GetITStatus(tim_periph, it_flags) != RESET)
        {
            TIM_ClearITPendingBit(tim_periph, it_flags);
            
            /* 读取捕获值 */
            switch (ch)
            {
                case IC_CHANNEL_1: capture_value = TIM_GetCapture1(tim_periph); break;
                case IC_CHANNEL_2: capture_value = TIM_GetCapture2(tim_periph); break;
                case IC_CHANNEL_3: capture_value = TIM_GetCapture3(tim_periph); break;
                case IC_CHANNEL_4: capture_value = TIM_GetCapture4(tim_periph); break;
                default: capture_value = 0; break;
            }
            
            /* 调用回调函数 */
            if (g_ic_it_callbacks[instance][ch][IC_IT_CAPTURE] != NULL)
            {
                g_ic_it_callbacks[instance][ch][IC_IT_CAPTURE](
                    instance, (IC_Channel_t)ch, IC_IT_CAPTURE, capture_value,
                    g_ic_it_user_data[instance][ch][IC_IT_CAPTURE]);
            }
        }
    }
}

/* 定时器中断服务程序入口 */
void TIM1_CC_IRQHandler(void) { IC_IRQHandler(IC_INSTANCE_TIM1); }
void TIM2_IRQHandler(void) { IC_IRQHandler(IC_INSTANCE_TIM2); }
void TIM3_IRQHandler(void) { IC_IRQHandler(IC_INSTANCE_TIM3); }
void TIM4_IRQHandler(void) { IC_IRQHandler(IC_INSTANCE_TIM4); }

/**
 * @brief 配置输入捕获滤波器
 */
IC_Status_t IC_SetFilter(IC_Instance_t instance, IC_Channel_t channel, uint8_t filter_value)
{
    TIM_TypeDef *tim_periph;
    TIM_ICInitTypeDef TIM_ICInitStructure;
    uint16_t tim_channel;
    
    if (instance >= IC_INSTANCE_MAX || channel >= IC_CHANNEL_MAX)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    if (!g_ic_initialized[instance][channel])
    {
        return IC_ERROR_NOT_INITIALIZED;
    }
    
    if (filter_value > 15)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    tim_periph = ic_tim_periphs[instance];
    tim_channel = ic_tim_channels[channel];
    
    /* 读取当前IC配置 */
    TIM_ICStructInit(&TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_ICFilter = filter_value;
    
    /* 重新配置IC通道 */
    TIM_ICInit(tim_periph, &TIM_ICInitStructure);
    
    return IC_OK;
}

/**
 * @brief 配置输入捕获预分频器
 */
IC_Status_t IC_SetPrescaler(IC_Instance_t instance, IC_Channel_t channel, uint8_t prescaler)
{
    TIM_TypeDef *tim_periph;
    TIM_ICInitTypeDef TIM_ICInitStructure;
    uint16_t tim_channel;
    uint16_t icpsc_value;
    
    if (instance >= IC_INSTANCE_MAX || channel >= IC_CHANNEL_MAX)
    {
        return IC_ERROR_INVALID_PARAM;
    }
    
    if (!g_ic_initialized[instance][channel])
    {
        return IC_ERROR_NOT_INITIALIZED;
    }
    
    switch (prescaler)
    {
        case 1: icpsc_value = TIM_ICPSC_DIV1; break;
        case 2: icpsc_value = TIM_ICPSC_DIV2; break;
        case 4: icpsc_value = TIM_ICPSC_DIV4; break;
        case 8: icpsc_value = TIM_ICPSC_DIV8; break;
        default: return IC_ERROR_INVALID_PARAM;
    }
    
    tim_periph = ic_tim_periphs[instance];
    tim_channel = ic_tim_channels[channel];
    
    /* 读取当前IC配置 */
    TIM_ICStructInit(&TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_ICPSC = icpsc_value;
    
    /* 重新配置IC通道 */
    TIM_ICInit(tim_periph, &TIM_ICInitStructure);
    
    return IC_OK;
}

#endif /* CONFIG_MODULE_TIMER_ENABLED */

