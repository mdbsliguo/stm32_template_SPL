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

/* 从模式标志（记录哪些实例使用了从模式） */
static bool g_ic_slave_mode[IC_INSTANCE_MAX] = {false};

/* 中断回调函数数组（暂时未使用，保留用于将来实现中断功能） */
/* 注释掉以避免编译警告，需要时再启用 */
/*
static IC_IT_Callback_t g_ic_it_callbacks[IC_INSTANCE_MAX][IC_CHANNEL_MAX][2] = {NULL};
static void *g_ic_it_user_data[IC_INSTANCE_MAX][IC_CHANNEL_MAX][2] = {NULL};
*/

/**
 * @brief 获取定时器外设时钟
 */
static uint32_t IC_GetPeriphClock(TIM_TypeDef *tim_periph)
{
    if (tim_periph == TIM1) return RCC_APB2Periph_TIM1;
    if (tim_periph == TIM2) return RCC_APB1Periph_TIM2;
    if (tim_periph == TIM3) return RCC_APB1Periph_TIM3;
    if (tim_periph == TIM4) return RCC_APB1Periph_TIM4;
    return 0;
}

/**
 * @brief 获取定时器实际时钟频率
 */
static uint32_t IC_GetTimerClock(TIM_TypeDef *tim_periph)
{
    uint32_t apb_clk;
    uint32_t tim_clk;
    
    SystemCoreClockUpdate();
    
    if (tim_periph == TIM1)
    {
        uint32_t apb2_prescaler = RCC->CFGR & RCC_CFGR_PPRE2;
        apb2_prescaler = (apb2_prescaler >> 11) & 0x07;
        
        if (apb2_prescaler < 4)
        {
            apb_clk = SystemCoreClock;
        }
        else
        {
            uint8_t presc_table[] = {0, 0, 0, 0, 1, 2, 3, 4};
            apb_clk = SystemCoreClock >> presc_table[apb2_prescaler];
        }
        
        if (apb2_prescaler >= 4)
        {
            tim_clk = apb_clk * 2;
        }
        else
        {
            tim_clk = apb_clk;
        }
    }
    else if (tim_periph == TIM2 || tim_periph == TIM3 || tim_periph == TIM4)
    {
        uint32_t apb1_prescaler = RCC->CFGR & RCC_CFGR_PPRE1;
        apb1_prescaler = (apb1_prescaler >> 8) & 0x07;
        
        if (apb1_prescaler < 4)
        {
            apb_clk = SystemCoreClock;
        }
        else
        {
            uint8_t presc_table[] = {0, 0, 0, 0, 1, 2, 3, 4};
            apb_clk = SystemCoreClock >> presc_table[apb1_prescaler];
        }
        
        if (apb1_prescaler >= 4)
        {
            tim_clk = apb_clk * 2;
        }
        else
        {
            tim_clk = apb_clk;
        }
    }
    else
    {
        return 0;
    }
    
    return tim_clk;
}

/**
 * @brief 获取GPIO引脚配置（需要根据定时器和通道确定）
 */
static void IC_GetGPIOConfig(IC_Instance_t instance, IC_Channel_t channel, 
                              GPIO_TypeDef **port, uint16_t *pin)
{
    *port = NULL;
    *pin = 0;
    
    if (instance == IC_INSTANCE_TIM1)
    {
        if (channel == IC_CHANNEL_1) { *port = GPIOA; *pin = GPIO_Pin_8; }
        else if (channel == IC_CHANNEL_2) { *port = GPIOA; *pin = GPIO_Pin_9; }
        else if (channel == IC_CHANNEL_3) { *port = GPIOA; *pin = GPIO_Pin_10; }
        else if (channel == IC_CHANNEL_4) { *port = GPIOA; *pin = GPIO_Pin_11; }
    }
    else if (instance == IC_INSTANCE_TIM2)
    {
        if (channel == IC_CHANNEL_1) { *port = GPIOA; *pin = GPIO_Pin_0; }
        else if (channel == IC_CHANNEL_2) { *port = GPIOA; *pin = GPIO_Pin_1; }
        else if (channel == IC_CHANNEL_3) { *port = GPIOA; *pin = GPIO_Pin_2; }
        else if (channel == IC_CHANNEL_4) { *port = GPIOA; *pin = GPIO_Pin_3; }
    }
    else if (instance == IC_INSTANCE_TIM3)
    {
        if (channel == IC_CHANNEL_1) { *port = GPIOA; *pin = GPIO_Pin_6; }
        else if (channel == IC_CHANNEL_2) { *port = GPIOA; *pin = GPIO_Pin_7; }
        else if (channel == IC_CHANNEL_3) { *port = GPIOB; *pin = GPIO_Pin_0; }
        else if (channel == IC_CHANNEL_4) { *port = GPIOB; *pin = GPIO_Pin_1; }
    }
    else if (instance == IC_INSTANCE_TIM4)
    {
        if (channel == IC_CHANNEL_1) { *port = GPIOB; *pin = GPIO_Pin_6; }
        else if (channel == IC_CHANNEL_2) { *port = GPIOB; *pin = GPIO_Pin_7; }
        else if (channel == IC_CHANNEL_3) { *port = GPIOB; *pin = GPIO_Pin_8; }
        else if (channel == IC_CHANNEL_4) { *port = GPIOB; *pin = GPIO_Pin_9; }
    }
}

/**
 * @brief 输入捕获初始化
 */
IC_Status_t IC_Init(IC_Instance_t instance, IC_Channel_t channel, IC_Polarity_t polarity)
{
    TIM_TypeDef *tim_periph;
    GPIO_TypeDef *gpio_port;
    uint16_t gpio_pin;
    uint16_t tim_channel;
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_ICInitTypeDef TIM_ICInitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    uint32_t gpio_clock;
    uint32_t tim_clock;
    
    /* ========== 参数校验 ========== */
    if (instance >= IC_INSTANCE_MAX) {
        return IC_ERROR_INVALID_INSTANCE;
    }
    if (channel >= IC_CHANNEL_MAX) {
        return IC_ERROR_INVALID_CHANNEL;
    }
    if (polarity > IC_POLARITY_BOTH) {
        return IC_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已初始化 */
    if (g_ic_initialized[instance][channel]) {
        return IC_ERROR_ALREADY_INITIALIZED;
    }
    
    tim_periph = ic_tim_periphs[instance];
    tim_channel = ic_tim_channels[channel];
    
    /* 获取GPIO配置 */
    IC_GetGPIOConfig(instance, channel, &gpio_port, &gpio_pin);
    if (gpio_port == NULL || gpio_pin == 0) {
        return IC_ERROR_INVALID_PERIPH;
    }
    
    /* ========== 1. 初始化 ========== */
    /* 1.1 RCC时钟使能 */
    if (gpio_port == GPIOA) gpio_clock = RCC_APB2Periph_GPIOA;
    else if (gpio_port == GPIOB) gpio_clock = RCC_APB2Periph_GPIOB;
    else if (gpio_port == GPIOC) gpio_clock = RCC_APB2Periph_GPIOC;
    else if (gpio_port == GPIOD) gpio_clock = RCC_APB2Periph_GPIOD;
    else return IC_ERROR_INVALID_PERIPH;
    
    RCC_APB2PeriphClockCmd(gpio_clock | RCC_APB2Periph_AFIO, ENABLE);
    
    tim_clock = IC_GetPeriphClock(tim_periph);
    if (tim_clock == 0) {
        return IC_ERROR_INVALID_PERIPH;
    }
    
    if (tim_periph == TIM1) {
        RCC_APB2PeriphClockCmd(tim_clock, ENABLE);
    } else {
        RCC_APB1PeriphClockCmd(tim_clock, ENABLE);
    }
    
    /* 1.2 GPIO配置成浮空输入模式 */
    GPIO_InitStructure.GPIO_Pin = gpio_pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(gpio_port, &GPIO_InitStructure);
    
    /* ========== 2. 配置时基单元：让CNT计数器在内部时钟的驱动下自增运行 ========== */
    TIM_TimeBaseStructure.TIM_Period = 0xFFFF;
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(tim_periph, &TIM_TimeBaseStructure);
    
    /* ========== 3. 配置输入捕获单元 ========== */
    TIM_ICStructInit(&TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_Channel = tim_channel;
    
    /* 3.1 极性 */
    TIM_ICInitStructure.TIM_ICPolarity = (polarity == IC_POLARITY_RISING) ? TIM_ICPolarity_Rising : 
                                         (polarity == IC_POLARITY_FALLING) ? TIM_ICPolarity_Falling : 
                                         TIM_ICPolarity_BothEdge;
    
    /* 3.2 直连通道/交叉通道 */
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    
    /* 3.3 分频器参数 */
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    
    /* 3.4 滤波器 */
    TIM_ICInitStructure.TIM_ICFilter = 0x0;
    
    /* 如果是PWM模式（双边沿），使用PWMI配置 */
    if (polarity == IC_POLARITY_BOTH) {
        TIM_PWMIConfig(tim_periph, &TIM_ICInitStructure);
    } else {
        TIM_ICInit(tim_periph, &TIM_ICInitStructure);
    }
    
    /* 使能输入捕获通道（必须在初始化时使能） */
    TIM_CCxCmd(tim_periph, tim_channel, TIM_CCx_Enable);
    
    /* 标记为已初始化（不使用从模式） */
    g_ic_initialized[instance][channel] = true;
    g_ic_slave_mode[instance] = false;
    
    return IC_OK;
}

/**
 * @brief 输入捕获初始化（使用从模式+Reset）
 */
IC_Status_t IC_InitWithSlaveMode(IC_Instance_t instance, IC_Channel_t channel, IC_Polarity_t polarity)
{
    TIM_TypeDef *tim_periph;
    GPIO_TypeDef *gpio_port;
    uint16_t gpio_pin;
    uint16_t tim_channel;
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_ICInitTypeDef TIM_ICInitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    uint32_t gpio_clock;
    uint32_t tim_clock;
    uint16_t trigger_source;
    
    /* ========== 参数校验 ========== */
    if (instance >= IC_INSTANCE_MAX) {
        return IC_ERROR_INVALID_INSTANCE;
    }
    if (channel >= IC_CHANNEL_MAX) {
        return IC_ERROR_INVALID_CHANNEL;
    }
    if (polarity > IC_POLARITY_BOTH) {
        return IC_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已初始化 */
    if (g_ic_initialized[instance][channel]) {
        return IC_ERROR_ALREADY_INITIALIZED;
    }
    
    tim_periph = ic_tim_periphs[instance];
    tim_channel = ic_tim_channels[channel];
    
    /* 获取GPIO配置 */
    IC_GetGPIOConfig(instance, channel, &gpio_port, &gpio_pin);
    if (gpio_port == NULL || gpio_pin == 0) {
        return IC_ERROR_INVALID_PERIPH;
    }
    
    /* ========== 1. 初始化 ========== */
    /* 1.1 RCC时钟使能 */
    if (gpio_port == GPIOA) gpio_clock = RCC_APB2Periph_GPIOA;
    else if (gpio_port == GPIOB) gpio_clock = RCC_APB2Periph_GPIOB;
    else if (gpio_port == GPIOC) gpio_clock = RCC_APB2Periph_GPIOC;
    else if (gpio_port == GPIOD) gpio_clock = RCC_APB2Periph_GPIOD;
    else return IC_ERROR_INVALID_PERIPH;
    
    RCC_APB2PeriphClockCmd(gpio_clock | RCC_APB2Periph_AFIO, ENABLE);
    
    tim_clock = IC_GetPeriphClock(tim_periph);
    if (tim_clock == 0) {
        return IC_ERROR_INVALID_PERIPH;
    }
    
    if (tim_periph == TIM1) {
        RCC_APB2PeriphClockCmd(tim_clock, ENABLE);
    } else {
        RCC_APB1PeriphClockCmd(tim_clock, ENABLE);
    }
    
    /* 1.2 GPIO配置成浮空输入模式 */
    GPIO_InitStructure.GPIO_Pin = gpio_pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(gpio_port, &GPIO_InitStructure);
    
    /* ========== 2. 配置时基单元：让CNT计数器在内部时钟的驱动下自增运行 ========== */
    TIM_TimeBaseStructure.TIM_Period = 0xFFFF;
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(tim_periph, &TIM_TimeBaseStructure);
    
    /* ========== 3. 配置输入捕获单元 ========== */
    TIM_ICStructInit(&TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_Channel = tim_channel;
    
    /* 3.1 极性 */
    TIM_ICInitStructure.TIM_ICPolarity = (polarity == IC_POLARITY_RISING) ? TIM_ICPolarity_Rising : 
                                         (polarity == IC_POLARITY_FALLING) ? TIM_ICPolarity_Falling : 
                                         TIM_ICPolarity_BothEdge;
    
    /* 3.2 直连通道/交叉通道 */
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    
    /* 3.3 分频器参数 */
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    
    /* 3.4 滤波器 */
    TIM_ICInitStructure.TIM_ICFilter = 0x0;
    
    /* 如果是PWM模式（双边沿），使用PWMI配置 */
    if (polarity == IC_POLARITY_BOTH) {
        TIM_PWMIConfig(tim_periph, &TIM_ICInitStructure);
    } else {
        TIM_ICInit(tim_periph, &TIM_ICInitStructure);
    }
    
    /* 使能输入捕获通道（必须在初始化时使能） */
    TIM_CCxCmd(tim_periph, tim_channel, TIM_CCx_Enable);
    
    /* ========== 4. 从模式触发源 ========== */
    /* 根据通道选择触发源 */
    if (channel == IC_CHANNEL_1) {
        trigger_source = TIM_TS_TI1FP1;
    } else if (channel == IC_CHANNEL_2) {
        trigger_source = TIM_TS_TI2FP2;
    } else if (channel == IC_CHANNEL_3) {
        trigger_source = TIM_TS_TI3FP3;
    } else {
        trigger_source = TIM_TS_TI4FP4;
    }
    TIM_SelectInputTrigger(tim_periph, trigger_source);
    
    /* ========== 5. 选择触发之后执行操作（Reset） ========== */
    /* 使用Reset模式：每次捕获时自动将CNT重置为0 */
    TIM_SelectSlaveMode(tim_periph, TIM_SlaveMode_Reset);
    
    /* 标记为已初始化（使用从模式） */
    g_ic_initialized[instance][channel] = true;
    g_ic_slave_mode[instance] = true;
    
    return IC_OK;
}

/**
 * @brief 输入捕获反初始化
 */
IC_Status_t IC_Deinit(IC_Instance_t instance, IC_Channel_t channel)
{
    /* ========== 参数校验 ========== */
    if (instance >= IC_INSTANCE_MAX) {
        return IC_ERROR_INVALID_INSTANCE;
    }
    if (channel >= IC_CHANNEL_MAX) {
        return IC_ERROR_INVALID_CHANNEL;
    }
    
    /* ========== 占位空函数 ========== */
    /* 功能未实现，待完善 */
    
    return IC_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 启动输入捕获
 */
IC_Status_t IC_Start(IC_Instance_t instance, IC_Channel_t channel)
{
    TIM_TypeDef *tim_periph;
    uint16_t tim_channel;
    
    /* ========== 参数校验 ========== */
    if (instance >= IC_INSTANCE_MAX) {
        return IC_ERROR_INVALID_INSTANCE;
    }
    if (channel >= IC_CHANNEL_MAX) {
        return IC_ERROR_INVALID_CHANNEL;
    }
    
    /* 检查是否已初始化 */
    if (!g_ic_initialized[instance][channel]) {
        return IC_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = ic_tim_periphs[instance];
    tim_channel = ic_tim_channels[channel];
    
    /* 清除捕获标志 */
    if (channel == IC_CHANNEL_1) {
        TIM_ClearFlag(tim_periph, TIM_FLAG_CC1);
    } else if (channel == IC_CHANNEL_2) {
        TIM_ClearFlag(tim_periph, TIM_FLAG_CC2);
    } else if (channel == IC_CHANNEL_3) {
        TIM_ClearFlag(tim_periph, TIM_FLAG_CC3);
    } else if (channel == IC_CHANNEL_4) {
        TIM_ClearFlag(tim_periph, TIM_FLAG_CC4);
    }
    
    /* 确保输入捕获通道已使能（在Init中已使能，这里再次确认） */
    TIM_CCxCmd(tim_periph, tim_channel, TIM_CCx_Enable);
    
    /* ========== 6. 调用TIM_Cmd：开启定时器 ========== */
    TIM_Cmd(tim_periph, ENABLE);
    
    return IC_OK;
}

/**
 * @brief 停止输入捕获
 */
IC_Status_t IC_Stop(IC_Instance_t instance, IC_Channel_t channel)
{
    /* ========== 参数校验 ========== */
    if (instance >= IC_INSTANCE_MAX) {
        return IC_ERROR_INVALID_INSTANCE;
    }
    if (channel >= IC_CHANNEL_MAX) {
        return IC_ERROR_INVALID_CHANNEL;
    }
    
    /* ========== 占位空函数 ========== */
    /* 功能未实现，待完善 */
    
    return IC_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 读取捕获值
 */
IC_Status_t IC_ReadValue(IC_Instance_t instance, IC_Channel_t channel, uint32_t *value)
{
    /* ========== 参数校验 ========== */
    if (instance >= IC_INSTANCE_MAX) {
        return IC_ERROR_INVALID_INSTANCE;
    }
    if (channel >= IC_CHANNEL_MAX) {
        return IC_ERROR_INVALID_CHANNEL;
    }
    if (value == NULL) {
        return IC_ERROR_NULL_PTR;
    }
    
    /* ========== 占位空函数 ========== */
    /* 功能未实现，待完善 */
    
    return IC_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 测量频率
 */
IC_Status_t IC_MeasureFrequency(IC_Instance_t instance, IC_Channel_t channel, uint32_t *frequency, uint32_t timeout_ms)
{
    TIM_TypeDef *tim_periph;
    uint32_t timer_clock;
    uint32_t start_time;
    uint16_t capture1, capture2;
    uint64_t period_count_64;
    uint16_t flag;
    uint16_t tim_channel;
    
    /* ========== 参数校验 ========== */
    if (instance >= IC_INSTANCE_MAX) {
        return IC_ERROR_INVALID_INSTANCE;
    }
    if (channel >= IC_CHANNEL_MAX) {
        return IC_ERROR_INVALID_CHANNEL;
    }
    if (frequency == NULL) {
        return IC_ERROR_NULL_PTR;
    }
    
    /* 检查是否已初始化 */
    if (!g_ic_initialized[instance][channel]) {
        return IC_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = ic_tim_periphs[instance];
    tim_channel = ic_tim_channels[channel];
    timer_clock = IC_GetTimerClock(tim_periph);
    
    if (timer_clock == 0) {
        return IC_ERROR_INVALID_PERIPH;
    }
    
    /* 获取捕获标志位 */
    if (channel == IC_CHANNEL_1) flag = TIM_FLAG_CC1;
    else if (channel == IC_CHANNEL_2) flag = TIM_FLAG_CC2;
    else if (channel == IC_CHANNEL_3) flag = TIM_FLAG_CC3;
    else flag = TIM_FLAG_CC4;
    
    /* 确保定时器已启动 */
    if (!(tim_periph->CR1 & TIM_CR1_CEN)) {
        TIM_Cmd(tim_periph, ENABLE);
    }
    
    /* 确保输入捕获通道已使能 */
    TIM_CCxCmd(tim_periph, tim_channel, TIM_CCx_Enable);
    
    /* ========== 7. 需要读取频率时，直接读取CCR寄存器，按照FC/N，计算一下即可 ========== */
    
    /* 检查是否使用从模式 */
    if (g_ic_slave_mode[instance]) {
        /* 使用从模式+Reset：每次捕获自动将CNT重置为0，直接读取CCR即可得到周期计数 */
        /* 清除捕获标志 */
        TIM_ClearFlag(tim_periph, flag);
        
        /* 等待第一个上升沿（触发Reset，CNT被重置为0） */
        start_time = Delay_GetTick();
        while (!TIM_GetFlagStatus(tim_periph, flag)) {
            if (Delay_GetElapsed(Delay_GetTick(), start_time) > timeout_ms) {
                return IC_ERROR_TIMEOUT;
            }
        }
        
        /* 第一个上升沿触发Reset，CNT被重置为0，CCR也应该是0或很小的值 */
        /* 清除捕获标志，等待第二个上升沿 */
        TIM_ClearFlag(tim_periph, flag);
        start_time = Delay_GetTick();
        while (!TIM_GetFlagStatus(tim_periph, flag)) {
            if (Delay_GetElapsed(Delay_GetTick(), start_time) > timeout_ms) {
                return IC_ERROR_TIMEOUT;
            }
        }
        
        /* 读取第二个上升沿的捕获值（CCR寄存器） */
        /* 由于使用了Reset模式，CNT在第一个上升沿时被重置为0，第二个上升沿时CCR就是周期计数 */
        if (channel == IC_CHANNEL_1) capture2 = TIM_GetCapture1(tim_periph);
        else if (channel == IC_CHANNEL_2) capture2 = TIM_GetCapture2(tim_periph);
        else if (channel == IC_CHANNEL_3) capture2 = TIM_GetCapture3(tim_periph);
        else capture2 = TIM_GetCapture4(tim_periph);
        
        /* 计算频率：frequency = timer_clock / CCR (FC/N) */
        if (capture2 == 0) {
            return IC_ERROR_INVALID_PARAM;
        }
        
        *frequency = timer_clock / capture2;
    } else {
        /* 不使用从模式：使用测周法，需要等待两个上升沿并统计溢出次数 */
        /* 清除捕获标志 */
        TIM_ClearFlag(tim_periph, flag);
        
        /* 等待第一个上升沿 */
        start_time = Delay_GetTick();
        while (!TIM_GetFlagStatus(tim_periph, flag)) {
            if (Delay_GetElapsed(Delay_GetTick(), start_time) > timeout_ms) {
                return IC_ERROR_TIMEOUT;
            }
        }
        
        /* 清除溢出标志，准备统计溢出 */
        TIM_ClearFlag(tim_periph, TIM_FLAG_Update);
        
        /* 读取第一个捕获值 */
        if (channel == IC_CHANNEL_1) capture1 = TIM_GetCapture1(tim_periph);
        else if (channel == IC_CHANNEL_2) capture1 = TIM_GetCapture2(tim_periph);
        else if (channel == IC_CHANNEL_3) capture1 = TIM_GetCapture3(tim_periph);
        else capture1 = TIM_GetCapture4(tim_periph);
        
        /* 读取第一个CNT值（用于溢出检测） */
        uint16_t cnt1_first = tim_periph->CNT;
        
        /* 清除捕获标志，等待第二个上升沿 */
        TIM_ClearFlag(tim_periph, flag);
        start_time = Delay_GetTick();
        
        /* 统计溢出次数，使用CNT寄存器检测溢出 */
        uint32_t overflow_count = 0;
        uint16_t last_cnt = cnt1_first;
        uint32_t actual_wait_time_ms = 0;
        
        while (!TIM_GetFlagStatus(tim_periph, flag)) {
            uint16_t current_cnt = tim_periph->CNT;
            
            /* 检查定时器溢出：CNT从大值跳回小值（或接近0） */
            /* 注意：需要考虑CNT可能从65535跳回0，或者从任意值跳回更小的值 */
            if (current_cnt < last_cnt) {
                /* CNT减小，说明发生了溢出 */
                overflow_count++;
            }
            last_cnt = current_cnt;
            
            /* 检查定时器溢出标志（备用方法，但主要依赖CNT检测） */
            if (TIM_GetFlagStatus(tim_periph, TIM_FLAG_Update)) {
                TIM_ClearFlag(tim_periph, TIM_FLAG_Update);
            }
            
            /* 检查超时并记录实际等待时间 */
            actual_wait_time_ms = Delay_GetElapsed(Delay_GetTick(), start_time);
            if (actual_wait_time_ms > timeout_ms) {
                return IC_ERROR_TIMEOUT;
            }
            
            /* 对于低频信号（如1Hz），如果等待时间已经超过500ms，使用时间估算方法 */
            /* 这样可以避免溢出统计不准确的问题 */
            if (actual_wait_time_ms > 500 && overflow_count == 0) {
                /* 等待时间已经超过500ms，但溢出次数为0，说明可能漏掉了溢出 */
                /* 使用时间估算：每65536个计数对应一次溢出 */
                /* 估算溢出次数 = (timer_clock * elapsed_ms / 1000) / 65536 */
                uint64_t estimated_overflow = ((uint64_t)timer_clock * actual_wait_time_ms) / (1000ULL * 0x10000ULL);
                if (estimated_overflow > 0) {
                    overflow_count = (uint32_t)estimated_overflow;
                }
            }
        }
        
        /* 记录最终实际等待时间 */
        actual_wait_time_ms = Delay_GetElapsed(Delay_GetTick(), start_time);
        
        /* 读取第二个捕获值 */
        if (channel == IC_CHANNEL_1) capture2 = TIM_GetCapture1(tim_periph);
        else if (channel == IC_CHANNEL_2) capture2 = TIM_GetCapture2(tim_periph);
        else if (channel == IC_CHANNEL_3) capture2 = TIM_GetCapture3(tim_periph);
        else capture2 = TIM_GetCapture4(tim_periph);
        
        /* 读取第二个CNT值（用于验证） */
        uint16_t cnt2_last = tim_periph->CNT;
        
        /* 计算周期（考虑多次溢出） */
        /* 使用64位计算避免溢出 */
        if (capture2 >= capture1) {
            /* capture2 >= capture1：可能没有溢出，或溢出后继续计数 */
            period_count_64 = (uint64_t)overflow_count * 0x10000ULL + (capture2 - capture1);
        } else {
            /* capture2 < capture1：至少发生了一次溢出（最后一次） */
            /* 如果overflow_count == 0，说明在读取capture2之前发生了溢出但未检测到 */
            if (overflow_count == 0) {
                overflow_count = 1;  /* 至少发生了一次溢出 */
            }
            period_count_64 = (uint64_t)overflow_count * 0x10000ULL - capture1 + capture2;
        }
        
        /* 验证：如果CNT也发生了溢出，应该与capture的溢出一致 */
        /* 如果cnt2_last < cnt1_first，说明CNT也发生了溢出，应该与overflow_count一致 */
        if (cnt2_last < cnt1_first && overflow_count == 0) {
            /* CNT发生了溢出，但overflow_count为0，说明溢出检测有问题 */
            /* 至少应该有一次溢出 */
            overflow_count = 1;
            /* 重新计算周期 */
            if (capture2 >= capture1) {
                period_count_64 = 0x10000ULL + (capture2 - capture1);
            } else {
                period_count_64 = 0x10000ULL - capture1 + capture2;
            }
        }
        
        /* 计算频率 */
        if (period_count_64 == 0) {
            return IC_ERROR_INVALID_PARAM;
        }
        
        /* 使用64位除法计算频率，避免精度损失 */
        /* 对于1Hz信号，period_count_64应该约等于timer_clock */
        /* 如果period_count_64 > timer_clock * 2，说明溢出次数统计过多，需要修正 */
        /* 或者如果实际等待时间 > 500ms，使用时间估算方法更准确（适用于低频信号） */
        if (period_count_64 > ((uint64_t)timer_clock * 2) || actual_wait_time_ms > 500) {
            /* 溢出次数统计不准确，或等待时间较长，使用实际等待时间来估算周期 */
            if (actual_wait_time_ms > 0) {
                /* 使用实际等待时间来计算周期 */
                /* period_count_64 = timer_clock * actual_wait_time_ms / 1000 */
                period_count_64 = ((uint64_t)timer_clock * actual_wait_time_ms) / 1000ULL;
            } else {
                /* 如果actual_wait_time_ms为0，使用capture值的差值 */
                uint64_t min_period;
                if (capture2 >= capture1) {
                    min_period = capture2 - capture1;
                } else {
                    min_period = 0x10000ULL - capture1 + capture2;
                }
                /* 如果min_period太小，说明发生了溢出，至少加一次溢出 */
                if (min_period < 1000) {
                    period_count_64 = 0x10000ULL + min_period;
                } else {
                    period_count_64 = min_period;
                }
            }
        }
        
        /* 再次检查period_count_64是否为0 */
        if (period_count_64 == 0) {
            return IC_ERROR_INVALID_PARAM;
        }
        
        /* 使用64位除法计算频率，避免精度损失 */
        *frequency = (uint32_t)((uint64_t)timer_clock / period_count_64);
    }
    
    return IC_OK;
}

/**
 * @brief 测量频率（测频法，适用于高频信号）
 * @note 测频法：在固定时间内计数脉冲数量（上升沿），然后计算频率 f = count / time
 */
IC_Status_t IC_MeasureFrequencyByCount(IC_Instance_t instance, IC_Channel_t channel, uint32_t *frequency, uint32_t gate_time_ms)
{
    TIM_TypeDef *tim_periph;
    uint32_t start_time;
    uint32_t gate_start_time;
    uint32_t pulse_count;
    uint16_t flag;
    uint16_t tim_channel;
    
    /* ========== 参数校验 ========== */
    if (instance >= IC_INSTANCE_MAX) {
        return IC_ERROR_INVALID_INSTANCE;
    }
    if (channel >= IC_CHANNEL_MAX) {
        return IC_ERROR_INVALID_CHANNEL;
    }
    if (frequency == NULL) {
        return IC_ERROR_NULL_PTR;
    }
    if (gate_time_ms == 0) {
        return IC_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已初始化 */
    if (!g_ic_initialized[instance][channel]) {
        return IC_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = ic_tim_periphs[instance];
    tim_channel = ic_tim_channels[channel];
    
    /* 获取捕获标志位 */
    if (channel == IC_CHANNEL_1) flag = TIM_FLAG_CC1;
    else if (channel == IC_CHANNEL_2) flag = TIM_FLAG_CC2;
    else if (channel == IC_CHANNEL_3) flag = TIM_FLAG_CC3;
    else flag = TIM_FLAG_CC4;
    
    /* 确保定时器已启动 */
    if (!(tim_periph->CR1 & TIM_CR1_CEN)) {
        TIM_Cmd(tim_periph, ENABLE);
    }
    
    /* 确保输入捕获通道已使能 */
    TIM_CCxCmd(tim_periph, tim_channel, TIM_CCx_Enable);
    
    /* 清除捕获标志 */
    TIM_ClearFlag(tim_periph, flag);
    
    /* 等待第一个上升沿作为起始点 */
    start_time = Delay_GetTick();
    while (!TIM_GetFlagStatus(tim_periph, flag)) {
        if (Delay_GetElapsed(Delay_GetTick(), start_time) > gate_time_ms) {
            return IC_ERROR_TIMEOUT;
        }
    }
    
    /* 清除捕获标志，开始计数 */
    TIM_ClearFlag(tim_periph, flag);
    pulse_count = 0;  /* 从0开始计数，第一个上升沿作为同步点，不计入 */
    gate_start_time = Delay_GetTick();  /* 记录闸门开始时间 */
    
    /* 在闸门时间内计数脉冲（上升沿） */
    /* 注意：使用精确的时间控制，确保在gate_time_ms时间内计数 */
    /* 对于高频信号，需要快速循环检查捕获标志，避免错过上升沿 */
    uint32_t gate_end_time = gate_start_time + gate_time_ms;
    while (Delay_GetTick() < gate_end_time) {
        /* 快速检查捕获标志（上升沿） */
        if (TIM_GetFlagStatus(tim_periph, flag)) {
            pulse_count++;
            /* 立即清除捕获标志，准备捕获下一个上升沿 */
            TIM_ClearFlag(tim_periph, flag);
        }
        /* 注意：不要在这里添加延时，保持循环尽可能快 */
    }
    
    /* 计算实际经过的时间（毫秒） */
    uint32_t actual_time_ms = Delay_GetElapsed(Delay_GetTick(), gate_start_time);
    if (actual_time_ms == 0) {
        actual_time_ms = 1;  /* 避免除零 */
    }
    
    /* 计算频率：f = count / time (Hz) */
    if (pulse_count == 0) {
        return IC_ERROR_INVALID_PARAM;
    }
    
    *frequency = (pulse_count * 1000) / actual_time_ms;
    
    return IC_OK;
}

/**
 * @brief 测量PWM信号
 */
IC_Status_t IC_MeasurePWM(IC_Instance_t instance, IC_Channel_t channel, IC_MeasureResult_t *result, uint32_t timeout_ms)
{
    TIM_TypeDef *tim_periph;
    uint32_t timer_clock;
    uint32_t start_time;
    uint32_t period_count, pulse_width_count;
    uint32_t period_us, pulse_width_us;
    
    /* ========== 参数校验 ========== */
    if (instance >= IC_INSTANCE_MAX) {
        return IC_ERROR_INVALID_INSTANCE;
    }
    if (channel >= IC_CHANNEL_MAX) {
        return IC_ERROR_INVALID_CHANNEL;
    }
    if (result == NULL) {
        return IC_ERROR_NULL_PTR;
    }
    
    /* 检查是否已初始化（PWM模式需要IC_POLARITY_BOTH） */
    if (!g_ic_initialized[instance][channel]) {
        return IC_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = ic_tim_periphs[instance];
    timer_clock = IC_GetTimerClock(tim_periph);
    
    if (timer_clock == 0) {
        return IC_ERROR_INVALID_PERIPH;
    }
    
    /* PWMI模式：通道1捕获周期，通道2捕获脉宽 */
    if (channel == IC_CHANNEL_1) {
        /* 等待捕获完成 */
        start_time = Delay_GetTick();
        while (!TIM_GetFlagStatus(tim_periph, TIM_FLAG_CC1) || !TIM_GetFlagStatus(tim_periph, TIM_FLAG_CC2)) {
            if (Delay_GetElapsed(Delay_GetTick(), start_time) > timeout_ms) {
                return IC_ERROR_TIMEOUT;
            }
        }
        
        /* 读取周期和脉宽 */
        period_count = TIM_GetCapture1(tim_periph);
        pulse_width_count = TIM_GetCapture2(tim_periph);
    } else if (channel == IC_CHANNEL_2) {
        /* 等待捕获完成 */
        start_time = Delay_GetTick();
        while (!TIM_GetFlagStatus(tim_periph, TIM_FLAG_CC1) || !TIM_GetFlagStatus(tim_periph, TIM_FLAG_CC2)) {
            if (Delay_GetElapsed(Delay_GetTick(), start_time) > timeout_ms) {
                return IC_ERROR_TIMEOUT;
            }
        }
        
        /* 读取周期和脉宽 */
        period_count = TIM_GetCapture2(tim_periph);
        pulse_width_count = TIM_GetCapture1(tim_periph);
    } else {
        return IC_ERROR_INVALID_CHANNEL;  /* PWMI模式只支持通道1和2 */
    }
    
    if (period_count == 0) {
        return IC_ERROR_INVALID_PARAM;
    }
    
    /* 计算周期和脉宽（微秒） */
    period_us = (uint32_t)((uint64_t)period_count * 1000000ULL / timer_clock);
    pulse_width_us = (uint32_t)((uint64_t)pulse_width_count * 1000000ULL / timer_clock);
    
    /* 计算频率 */
    result->frequency = timer_clock / period_count;
    result->period = period_us;
    result->pulse_width = pulse_width_us;
    result->duty_cycle = (uint32_t)((uint64_t)pulse_width_count * 100ULL / period_count);
    
    return IC_OK;
}

/**
 * @brief 测量脉冲宽度
 */
IC_Status_t IC_MeasurePulseWidth(IC_Instance_t instance, IC_Channel_t channel, uint32_t *pulse_width_us, uint32_t timeout_ms)
{
    /* ========== 参数校验 ========== */
    if (instance >= IC_INSTANCE_MAX) {
        return IC_ERROR_INVALID_INSTANCE;
    }
    if (channel >= IC_CHANNEL_MAX) {
        return IC_ERROR_INVALID_CHANNEL;
    }
    if (pulse_width_us == NULL) {
        return IC_ERROR_NULL_PTR;
    }
    
    /* ========== 占位空函数 ========== */
    (void)timeout_ms;
    /* 功能未实现，待完善 */
    
    return IC_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 检查输入捕获是否已初始化
 */
uint8_t IC_IsInitialized(IC_Instance_t instance, IC_Channel_t channel)
{
    /* ========== 参数校验 ========== */
    if (instance >= IC_INSTANCE_MAX) {
        return 0;  /* 无效实例返回0（未初始化） */
    }
    if (channel >= IC_CHANNEL_MAX) {
        return 0;  /* 无效通道返回0（未初始化） */
    }
    
    /* ========== 占位空函数 ========== */
    return 0;
}

/* ========== 中断模式功能实现 ========== */

/* ========== 以下函数暂时未使用，保留用于将来实现中断功能 ========== */
/* 注释掉以避免编译警告，需要时再启用 */

/* 获取输入捕获中断类型对应的SPL库中断值 */
/*
static uint16_t IC_GetITValue(IC_Channel_t channel, IC_IT_t it_type)
{
    (void)channel;
    (void)it_type;
    return 0;
}
*/

/* 获取定时器中断向量 */
/*
static IRQn_Type IC_GetIRQn(IC_Instance_t instance)
{
    (void)instance;
    return (IRQn_Type)0;
}
*/

/**
 * @brief 使能输入捕获中断
 */
IC_Status_t IC_EnableIT(IC_Instance_t instance, IC_Channel_t channel, IC_IT_t it_type)
{
    /* ========== 参数校验 ========== */
    if (instance >= IC_INSTANCE_MAX) {
        return IC_ERROR_INVALID_INSTANCE;
    }
    if (channel >= IC_CHANNEL_MAX) {
        return IC_ERROR_INVALID_CHANNEL;
    }
    if (it_type > IC_IT_OVERFLOW) {
        return IC_ERROR_INVALID_PARAM;
    }
    
    /* ========== 占位空函数 ========== */
    /* 功能未实现，待完善 */
    
    return IC_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 禁用输入捕获中断
 */
IC_Status_t IC_DisableIT(IC_Instance_t instance, IC_Channel_t channel, IC_IT_t it_type)
{
    /* ========== 参数校验 ========== */
    if (instance >= IC_INSTANCE_MAX) {
        return IC_ERROR_INVALID_INSTANCE;
    }
    if (channel >= IC_CHANNEL_MAX) {
        return IC_ERROR_INVALID_CHANNEL;
    }
    if (it_type > IC_IT_OVERFLOW) {
        return IC_ERROR_INVALID_PARAM;
    }
    
    /* ========== 占位空函数 ========== */
    /* 功能未实现，待完善 */
    
    return IC_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 设置输入捕获中断回调函数
 */
IC_Status_t IC_SetITCallback(IC_Instance_t instance, IC_Channel_t channel, IC_IT_t it_type,
                             IC_IT_Callback_t callback, void *user_data)
{
    /* ========== 参数校验 ========== */
    if (instance >= IC_INSTANCE_MAX) {
        return IC_ERROR_INVALID_INSTANCE;
    }
    if (channel >= IC_CHANNEL_MAX) {
        return IC_ERROR_INVALID_CHANNEL;
    }
    if (it_type > IC_IT_OVERFLOW) {
        return IC_ERROR_INVALID_PARAM;
    }
    /* 注意：callback可以为NULL（表示禁用回调），user_data可以为NULL */
    
    /* ========== 占位空函数 ========== */
    (void)callback;
    (void)user_data;
    /* 功能未实现，待完善 */
    
    return IC_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 输入捕获中断服务函数
 */
void IC_IRQHandler(IC_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= IC_INSTANCE_MAX) {
        return;  /* 无效实例直接返回 */
    }
    
    /* ========== 占位空函数 ========== */
    /* 功能未实现 */
}


/* 定时器中断服务程序入口 */
void TIM1_CC_IRQHandler(void) {
}


/**
 * @brief 配置输入捕获预分频器
 */
IC_Status_t IC_SetPrescaler(IC_Instance_t instance, IC_Channel_t channel, uint8_t prescaler)
{
    /* ========== 参数校验 ========== */
    if (instance >= IC_INSTANCE_MAX) {
        return IC_ERROR_INVALID_INSTANCE;
    }
    if (channel >= IC_CHANNEL_MAX) {
        return IC_ERROR_INVALID_CHANNEL;
    }
    if (prescaler > 15) {
        return IC_ERROR_INVALID_PARAM;  /* 预分频器范围：0-15 */
    }
    
    /* ========== 占位空函数 ========== */
    /* 功能未实现，待完善 */
    
    return IC_ERROR_NOT_IMPLEMENTED;
}

#endif /* CONFIG_MODULE_TIMER_ENABLED */

