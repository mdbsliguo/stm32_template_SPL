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

/* 从board.h加载配置（board.h在timer_pwm.h中包含） */
/* 注意：PWM_CONFIGS必须在board.h中定义，且必须包含3个元素（对应TIM1/TIM3/TIM4） */
static PWM_Config_t g_pwm_configs[PWM_INSTANCE_MAX] __attribute__((unused)) = PWM_CONFIGS;

/* 初始化标志 */
static bool g_pwm_initialized[PWM_INSTANCE_MAX] __attribute__((unused)) = {false, false, false};

/* 当前频率（Hz） */
static uint32_t g_pwm_frequency[PWM_INSTANCE_MAX] __attribute__((unused)) = {1000, 1000, 1000};

/**
 * @brief 获取GPIO时钟使能值
 * @param[in] port GPIO端口指针
 * @return uint32_t GPIO时钟使能值，失败返回0
 */
static uint32_t PWM_GetGPIOClock(GPIO_TypeDef *port)
{
    if (port == GPIOA) return RCC_APB2Periph_GPIOA;
    if (port == GPIOB) return RCC_APB2Periph_GPIOB;
    if (port == GPIOC) return RCC_APB2Periph_GPIOC;
    if (port == GPIOD) return RCC_APB2Periph_GPIOD;
    if (port == GPIOE) return RCC_APB2Periph_GPIOE;
    if (port == GPIOF) return RCC_APB2Periph_GPIOF;
    if (port == GPIOG) return RCC_APB2Periph_GPIOG;
    return 0;
}

/**
 * @brief 获取定时器外设时钟使能值
 * @param[in] tim_periph 定时器外设指针
 * @return uint32_t 时钟使能值，失败返回0
 * @note TIM1在APB2，TIM3/TIM4在APB1
 */
static uint32_t PWM_GetPeriphClock(TIM_TypeDef *tim_periph)
{
    if (tim_periph == TIM1) return RCC_APB2Periph_TIM1;
    if (tim_periph == TIM3) return RCC_APB1Periph_TIM3;
    if (tim_periph == TIM4) return RCC_APB1Periph_TIM4;
    return 0;
}

/**
 * @brief 获取定时器实际时钟频率
 * @param[in] tim_periph 定时器外设指针
 * @return uint32_t 定时器时钟频率（Hz），失败返回0
 * @note TIM1在APB2，TIM3/TIM4在APB1
 * @note 如果APB1/APB2分频>1，定时器时钟 = APB时钟 * 2
 */
static uint32_t PWM_GetTimerClock(TIM_TypeDef *tim_periph)
{
    uint32_t apb_clk;
    uint32_t tim_clk;
    
    /* 更新SystemCoreClock */
    SystemCoreClockUpdate();
    
    if (tim_periph == TIM1)
    {
        /* TIM1在APB2总线上 */
        uint32_t apb2_prescaler = RCC->CFGR & RCC_CFGR_PPRE2;
        apb2_prescaler = (apb2_prescaler >> 11) & 0x07;
        
        if (apb2_prescaler < 4)
        {
            /* APB2没有分频或分频为1 */
            apb_clk = SystemCoreClock;
        }
        else
        {
            /* APB2有分频，计算实际频率 */
            uint8_t presc_table[] = {0, 0, 0, 0, 1, 2, 3, 4};
            apb_clk = SystemCoreClock >> presc_table[apb2_prescaler];
        }
        
        /* 如果APB2分频>1，TIM1时钟 = APB2时钟 * 2 */
        if (apb2_prescaler >= 4)
        {
            tim_clk = apb_clk * 2;
        }
        else
        {
            tim_clk = apb_clk;
        }
    }
    else if (tim_periph == TIM3 || tim_periph == TIM4)
    {
        /* TIM3/TIM4在APB1总线上 */
        uint32_t apb1_prescaler = RCC->CFGR & RCC_CFGR_PPRE1;
        apb1_prescaler = (apb1_prescaler >> 8) & 0x07;
        
        if (apb1_prescaler < 4)
        {
            /* APB1没有分频或分频为1 */
            apb_clk = SystemCoreClock;
        }
        else
        {
            /* APB1有分频，计算实际频率 */
            uint8_t presc_table[] = {0, 0, 0, 0, 1, 2, 3, 4};
            apb_clk = SystemCoreClock >> presc_table[apb1_prescaler];
        }
        
        /* 如果APB1分频>1，TIM3/TIM4时钟 = APB1时钟 * 2 */
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
        return 0;  /* 不支持的定时器 */
    }
    
    return tim_clk;
}

/**
 * @brief 获取PWM通道对应的TIM通道值
 * @param[in] channel PWM通道（PWM_CHANNEL_1/2/3/4）
 * @return uint16_t TIM通道值（TIM_Channel_1/2/3/4），失败返回0
 */
static uint16_t PWM_GetTIMChannel(PWM_Channel_t channel)
{
    switch (channel)
    {
        case PWM_CHANNEL_1: return TIM_Channel_1;
        case PWM_CHANNEL_2: return TIM_Channel_2;
        case PWM_CHANNEL_3: return TIM_Channel_3;
        case PWM_CHANNEL_4: return TIM_Channel_4;
        default: return 0;
    }
}


/**
 * @brief PWM初始化
 * @param[in] instance PWM实例索引
 * @return PWM_Status_t 错误码
 * @note 根据board.h中的配置初始化定时器外设和GPIO引脚
 * @note 初始化后，PWM频率和占空比需要单独配置
 */
PWM_Status_t PWM_Init(PWM_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= PWM_INSTANCE_MAX) {
        return PWM_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (g_pwm_initialized[instance]) {
        return PWM_OK;  /* 已初始化，直接返回成功 */
    }
    
    /* 获取配置 */
    PWM_Config_t *cfg = &g_pwm_configs[instance];
    if (cfg->tim_periph == NULL) {
        return PWM_ERROR_INVALID_PERIPH;
    }
    if (!cfg->enabled) {
        return PWM_ERROR_INVALID_PARAM;  /* 配置未启用 */
    }
    
    TIM_TypeDef *tim_periph = cfg->tim_periph;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    
    /* ========== 1. 使能定时器时钟 ========== */
    uint32_t tim_clock = PWM_GetPeriphClock(tim_periph);
    if (tim_clock == 0) {
        return PWM_ERROR_INVALID_PERIPH;
    }
    
    if (tim_periph == TIM1) {
        RCC_APB2PeriphClockCmd(tim_clock, ENABLE);
    } else {
        RCC_APB1PeriphClockCmd(tim_clock, ENABLE);
    }
    
    /* ========== 2. 使能AFIO时钟（用于GPIO复用功能） ========== */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    
    /* ========== 3. 配置GPIO为复用推挽输出 ========== */
    for (int i = 0; i < 4; i++) {
        if (cfg->channels[i].enabled) {
            GPIO_TypeDef *port = cfg->channels[i].port;
            uint16_t pin = cfg->channels[i].pin;
            
            if (port == NULL || pin == 0) {
                continue;  /* 跳过无效配置 */
            }
            
            /* 使能GPIO时钟 */
            uint32_t gpio_clock = PWM_GetGPIOClock(port);
            if (gpio_clock == 0) {
                return PWM_ERROR_GPIO_FAILED;
            }
            RCC_APB2PeriphClockCmd(gpio_clock, ENABLE);
            
            /* 配置GPIO为复用推挽输出，50MHz */
            GPIO_InitStructure.GPIO_Pin = pin;
            GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
            GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
            GPIO_Init(port, &GPIO_InitStructure);
        }
    }
    
    /* ========== 4. 配置定时器时基（默认1kHz频率） ========== */
    uint32_t tim_clk = PWM_GetTimerClock(tim_periph);
    if (tim_clk == 0) {
        return PWM_ERROR_INVALID_PERIPH;
    }
    
    /* 计算PSC和ARR（目标频率1kHz） */
    uint32_t target_freq = 1000;  /* 默认1kHz */
    uint32_t arr = 1000;  /* 默认ARR=1000 */
    uint32_t psc = (tim_clk / (arr * target_freq)) - 1;
    
    /* 如果PSC太大，调整ARR */
    if (psc > 65535) {
        arr = tim_clk / (target_freq * 65536);
        if (arr < 1) arr = 1;
        psc = (tim_clk / (arr * target_freq)) - 1;
        if (psc > 65535) psc = 65535;
    }
    
    TIM_TimeBaseStructure.TIM_Period = arr - 1;  /* ARR寄存器值 */
    TIM_TimeBaseStructure.TIM_Prescaler = psc;   /* PSC寄存器值 */
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(tim_periph, &TIM_TimeBaseStructure);
    
    /* ========== 5. 配置PWM输出模式（每个启用的通道） ========== */
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Disable;  /* 初始状态为禁用，需要时再使能 */
    TIM_OCInitStructure.TIM_Pulse = 0;  /* 初始占空比为0 */
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    
    for (int i = 0; i < 4; i++) {
        if (cfg->channels[i].enabled) {
            uint16_t tim_channel = PWM_GetTIMChannel((PWM_Channel_t)i);
            /* 注意：TIM_Channel_1的值是0，所以不能简单地用tim_channel == 0来判断是否有效 */
            /* 应该检查tim_channel是否是有效的TIM通道值之一（TIM_Channel_1/2/3/4） */
            if (tim_channel != TIM_Channel_1 && tim_channel != TIM_Channel_2 && 
                tim_channel != TIM_Channel_3 && tim_channel != TIM_Channel_4) {
                continue;
            }
            
            /* 根据通道选择对应的初始化函数 */
            switch (tim_channel) {
                case TIM_Channel_1:
                    TIM_OC1Init(tim_periph, &TIM_OCInitStructure);
                    TIM_OC1PreloadConfig(tim_periph, TIM_OCPreload_Enable);
                    break;
                case TIM_Channel_2:
                    TIM_OC2Init(tim_periph, &TIM_OCInitStructure);
                    TIM_OC2PreloadConfig(tim_periph, TIM_OCPreload_Enable);
                    break;
                case TIM_Channel_3:
                    TIM_OC3Init(tim_periph, &TIM_OCInitStructure);
                    TIM_OC3PreloadConfig(tim_periph, TIM_OCPreload_Enable);
                    break;
                case TIM_Channel_4:
                    TIM_OC4Init(tim_periph, &TIM_OCInitStructure);
                    TIM_OC4PreloadConfig(tim_periph, TIM_OCPreload_Enable);
                    break;
                default:
                    break;
            }
        }
    }
    
    /* ========== 6. TIM1特殊处理：使能主输出 ========== */
    if (tim_periph == TIM1) {
        TIM_CtrlPWMOutputs(TIM1, ENABLE);
    }
    
    /* ========== 7. 使能定时器自动重装载 ========== */
    TIM_ARRPreloadConfig(tim_periph, ENABLE);
    
    /* ========== 8. 启动定时器 ========== */
    TIM_Cmd(tim_periph, ENABLE);
    
    /* ========== 9. 保存状态 ========== */
    g_pwm_initialized[instance] = true;
    g_pwm_frequency[instance] = target_freq;
    
    return PWM_OK;
}

/**
 * @brief PWM反初始化
 * @param[in] instance PWM实例索引
 * @return PWM_Status_t 错误码
 * @note 简单实现：停止定时器，清除初始化标志
 */
PWM_Status_t PWM_Deinit(PWM_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= PWM_INSTANCE_MAX) {
        return PWM_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (!g_pwm_initialized[instance]) {
        return PWM_OK;  /* 未初始化，直接返回成功 */
    }
    
    /* 获取配置 */
    PWM_Config_t *cfg = &g_pwm_configs[instance];
    TIM_TypeDef *tim_periph = cfg->tim_periph;
    if (tim_periph == NULL) {
        return PWM_ERROR_INVALID_PERIPH;
    }
    
    /* 停止定时器 */
    TIM_Cmd(tim_periph, DISABLE);
    
    /* TIM1特殊处理：禁用主输出 */
    if (tim_periph == TIM1) {
        TIM_CtrlPWMOutputs(TIM1, DISABLE);
    }
    
    /* 清除初始化标志 */
    g_pwm_initialized[instance] = false;
    g_pwm_frequency[instance] = 1000;  /* 重置为默认频率 */
    
    return PWM_OK;
}

/**
 * @brief 设置PWM频率
 * @param[in] instance PWM实例索引
 * @param[in] frequency 频率（Hz），范围：1Hz ~ 72MHz（取决于系统时钟）
 * @return PWM_Status_t 错误码
 * @note 设置频率后，所有通道的频率都会改变
 */
PWM_Status_t PWM_SetFrequency(PWM_Instance_t instance, uint32_t frequency)
{
    /* ========== 参数校验 ========== */
    if (instance >= PWM_INSTANCE_MAX) {
        return PWM_ERROR_INVALID_INSTANCE;
    }
    if (frequency == 0) {
        return PWM_ERROR_INVALID_PARAM;  /* 频率不能为0 */
    }
    
    /* 检查是否已初始化 */
    if (!g_pwm_initialized[instance]) {
        return PWM_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取配置 */
    PWM_Config_t *cfg = &g_pwm_configs[instance];
    TIM_TypeDef *tim_periph = cfg->tim_periph;
    if (tim_periph == NULL) {
        return PWM_ERROR_INVALID_PERIPH;
    }
    
    /* 获取定时器时钟频率 */
    uint32_t tim_clk = PWM_GetTimerClock(tim_periph);
    if (tim_clk == 0) {
        return PWM_ERROR_INVALID_PERIPH;
    }
    
    /* 检查频率是否超出范围 */
    if (frequency > tim_clk) {
        return PWM_ERROR_FREQ_OUT_OF_RANGE;
    }
    
    /* 计算PSC和ARR值 */
    /* 目标：tim_clk / ((PSC + 1) * (ARR + 1)) = frequency */
    /* 策略：优先使用较大的ARR值，以获得更好的分辨率 */
    uint32_t arr = 1000;  /* 默认ARR=1000 */
    uint32_t psc = (tim_clk / (arr * frequency)) - 1;
    
    /* 如果PSC太大，调整ARR */
    if (psc > 65535) {
        arr = tim_clk / (frequency * 65536);
        if (arr < 1) arr = 1;
        psc = (tim_clk / (arr * frequency)) - 1;
        if (psc > 65535) psc = 65535;
    }
    
    /* 如果PSC为0，调整ARR */
    if (psc == 0 && arr > 1) {
        arr = tim_clk / frequency;
        if (arr < 1) arr = 1;
        if (arr > 65535) arr = 65535;
    }
    
    /* 更新ARR寄存器 */
    TIM_SetAutoreload(tim_periph, arr - 1);
    
    /* 更新PSC寄存器 */
    TIM_PrescalerConfig(tim_periph, psc, TIM_PSCReloadMode_Immediate);
    
    /* 保存当前频率 */
    g_pwm_frequency[instance] = frequency;
    
    return PWM_OK;
}

/**
 * @brief 获取PWM频率
 * @param[in] instance PWM实例索引
 * @param[out] frequency 频率指针（Hz）
 * @return PWM_Status_t 错误码
 */
PWM_Status_t PWM_GetFrequency(PWM_Instance_t instance, uint32_t *frequency)
{
    /* ========== 参数校验 ========== */
    if (instance >= PWM_INSTANCE_MAX) {
        return PWM_ERROR_INVALID_INSTANCE;
    }
    if (frequency == NULL) {
        return PWM_ERROR_NULL_PTR;
    }
    
    /* 检查是否已初始化 */
    if (!g_pwm_initialized[instance]) {
        return PWM_ERROR_NOT_INITIALIZED;
    }
    
    /* 返回保存的频率值 */
    *frequency = g_pwm_frequency[instance];
    
    return PWM_OK;
}

/**
 * @brief 设置PWM占空比
 * @param[in] instance PWM实例索引
 * @param[in] channel PWM通道（PWM_CHANNEL_1/2/3/4）
 * @param[in] duty_cycle 占空比（0.0~100.0，单位：百分比）
 * @return PWM_Status_t 错误码
 */
PWM_Status_t PWM_SetDutyCycle(PWM_Instance_t instance, PWM_Channel_t channel, float duty_cycle)
{
    /* ========== 参数校验 ========== */
    if (instance >= PWM_INSTANCE_MAX) {
        return PWM_ERROR_INVALID_INSTANCE;
    }
    if (channel >= PWM_CHANNEL_MAX) {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    if (duty_cycle < 0.0f || duty_cycle > 100.0f) {
        return PWM_ERROR_INVALID_PARAM;  /* 占空比范围：0.0 ~ 100.0 */
    }
    
    /* 检查是否已初始化 */
    if (!g_pwm_initialized[instance]) {
        return PWM_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取配置 */
    PWM_Config_t *cfg = &g_pwm_configs[instance];
    TIM_TypeDef *tim_periph = cfg->tim_periph;
    if (tim_periph == NULL) {
        return PWM_ERROR_INVALID_PERIPH;
    }
    
    /* 检查通道是否启用 */
    if (!cfg->channels[channel].enabled) {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    /* 获取ARR值（直接读取寄存器） */
    uint32_t arr = tim_periph->ARR + 1;
    
    /* 计算CCR值：CCR = ARR * duty_cycle / 100.0 */
    uint32_t ccr = (uint32_t)((float)arr * duty_cycle / 100.0f);
    if (ccr > arr) {
        ccr = arr;  /* 限制最大值 */
    }
    
    /* 获取TIM通道值 */
    uint16_t tim_channel = PWM_GetTIMChannel(channel);
    /* 注意：TIM_Channel_1的值是0，所以不能简单地用tim_channel == 0来判断是否有效 */
    /* 应该检查tim_channel是否是有效的TIM通道值之一（TIM_Channel_1/2/3/4） */
    if (tim_channel != TIM_Channel_1 && tim_channel != TIM_Channel_2 && 
        tim_channel != TIM_Channel_3 && tim_channel != TIM_Channel_4) {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    /* 根据通道设置CCR寄存器 */
    switch (tim_channel) {
        case TIM_Channel_1:
            TIM_SetCompare1(tim_periph, ccr);
            break;
        case TIM_Channel_2:
            TIM_SetCompare2(tim_periph, ccr);
            break;
        case TIM_Channel_3:
            TIM_SetCompare3(tim_periph, ccr);
            break;
        case TIM_Channel_4:
            TIM_SetCompare4(tim_periph, ccr);
            break;
        default:
            return PWM_ERROR_INVALID_CHANNEL;
    }
    
    return PWM_OK;
}

/**
 * @brief 使能PWM通道
 * @param[in] instance PWM实例索引
 * @param[in] channel PWM通道（PWM_CHANNEL_1/2/3/4）
 * @return PWM_Status_t 错误码
 */
PWM_Status_t PWM_EnableChannel(PWM_Instance_t instance, PWM_Channel_t channel)
{
    /* ========== 参数校验 ========== */
    if (instance >= PWM_INSTANCE_MAX) {
        return PWM_ERROR_INVALID_INSTANCE;
    }
    if (channel >= PWM_CHANNEL_MAX) {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    /* 检查是否已初始化 */
    if (!g_pwm_initialized[instance]) {
        return PWM_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取配置 */
    PWM_Config_t *cfg = &g_pwm_configs[instance];
    TIM_TypeDef *tim_periph = cfg->tim_periph;
    if (tim_periph == NULL) {
        return PWM_ERROR_INVALID_PERIPH;
    }
    
    /* 检查通道是否启用 */
    if (!cfg->channels[channel].enabled) {
        /* 通道未启用，返回错误 */
        /* 调试信息：输出配置状态以便排查问题 */
        /* 注意：如果这里失败，可能是配置表未正确加载（使用了错误的board.h） */
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    /* 获取TIM通道值 */
    uint16_t tim_channel = PWM_GetTIMChannel(channel);
    /* 注意：TIM_Channel_1的值是0，所以不能简单地用tim_channel == 0来判断是否有效 */
    /* 应该检查tim_channel是否是有效的TIM通道值之一（TIM_Channel_1/2/3/4） */
    if (tim_channel != TIM_Channel_1 && tim_channel != TIM_Channel_2 && 
        tim_channel != TIM_Channel_3 && tim_channel != TIM_Channel_4) {
        /* 无效的通道值，返回错误 */
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    /* 根据通道使能CCER寄存器对应位 */
    switch (tim_channel) {
        case TIM_Channel_1:
            TIM_CCxCmd(tim_periph, TIM_Channel_1, TIM_CCx_Enable);
            break;
        case TIM_Channel_2:
            TIM_CCxCmd(tim_periph, TIM_Channel_2, TIM_CCx_Enable);
            break;
        case TIM_Channel_3:
            TIM_CCxCmd(tim_periph, TIM_Channel_3, TIM_CCx_Enable);
            break;
        case TIM_Channel_4:
            TIM_CCxCmd(tim_periph, TIM_Channel_4, TIM_CCx_Enable);
            break;
        default:
            return PWM_ERROR_INVALID_CHANNEL;
    }
    
    return PWM_OK;
}

/**
 * @brief 禁用PWM通道
 * @param[in] instance PWM实例索引
 * @param[in] channel PWM通道（PWM_CHANNEL_1/2/3/4）
 * @return PWM_Status_t 错误码
 */
PWM_Status_t PWM_DisableChannel(PWM_Instance_t instance, PWM_Channel_t channel)
{
    /* ========== 参数校验 ========== */
    if (instance >= PWM_INSTANCE_MAX) {
        return PWM_ERROR_INVALID_INSTANCE;
    }
    if (channel >= PWM_CHANNEL_MAX) {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    /* 检查是否已初始化 */
    if (!g_pwm_initialized[instance]) {
        return PWM_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取配置 */
    PWM_Config_t *cfg = &g_pwm_configs[instance];
    TIM_TypeDef *tim_periph = cfg->tim_periph;
    if (tim_periph == NULL) {
        return PWM_ERROR_INVALID_PERIPH;
    }
    
    /* 检查通道是否启用（与PWM_EnableChannel保持一致） */
    if (!cfg->channels[channel].enabled) {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    /* 获取TIM通道值 */
    uint16_t tim_channel = PWM_GetTIMChannel(channel);
    /* 注意：TIM_Channel_1的值是0，所以不能简单地用tim_channel == 0来判断是否有效 */
    /* 应该检查tim_channel是否是有效的TIM通道值之一（TIM_Channel_1/2/3/4） */
    if (tim_channel != TIM_Channel_1 && tim_channel != TIM_Channel_2 && 
        tim_channel != TIM_Channel_3 && tim_channel != TIM_Channel_4) {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    /* 根据通道禁用CCER寄存器对应位 */
    switch (tim_channel) {
        case TIM_Channel_1:
            TIM_CCxCmd(tim_periph, TIM_Channel_1, TIM_CCx_Disable);
            break;
        case TIM_Channel_2:
            TIM_CCxCmd(tim_periph, TIM_Channel_2, TIM_CCx_Disable);
            break;
        case TIM_Channel_3:
            TIM_CCxCmd(tim_periph, TIM_Channel_3, TIM_CCx_Disable);
            break;
        case TIM_Channel_4:
            TIM_CCxCmd(tim_periph, TIM_Channel_4, TIM_CCx_Disable);
            break;
        default:
            return PWM_ERROR_INVALID_CHANNEL;
    }
    
    return PWM_OK;
}

/**
 * @brief 检查PWM是否已初始化
 * @param[in] instance PWM实例索引
 * @return uint8_t 1-已初始化，0-未初始化
 */
uint8_t PWM_IsInitialized(PWM_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= PWM_INSTANCE_MAX) {
        return 0;  /* 无效实例返回0（未初始化） */
    }
    
    return g_pwm_initialized[instance] ? 1 : 0;
}

/**
 * @brief 获取定时器外设指针
 * @param[in] instance PWM实例索引
 * @return TIM_TypeDef* 定时器外设指针，失败返回NULL
 */
TIM_TypeDef* PWM_GetPeriph(PWM_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= PWM_INSTANCE_MAX) {
        return NULL;  /* 无效实例返回NULL */
    }
    
    /* 检查是否已初始化 */
    if (!g_pwm_initialized[instance]) {
        return NULL;
    }
    
    /* 获取配置 */
    PWM_Config_t *cfg = &g_pwm_configs[instance];
    return cfg->tim_periph;
}

/* ========== 互补输出和死区时间功能实现（仅TIM1/TIM8） ========== */

/**
 * @brief 使能PWM互补输出
 * @param[in] instance PWM实例索引
 * @param[in] channel PWM通道
 * @return PWM_Status_t 错误码
 * @note 占位空函数，功能未实现，待完善
 */
PWM_Status_t PWM_EnableComplementary(PWM_Instance_t instance, PWM_Channel_t channel)
{
    /* ========== 参数校验 ========== */
    if (instance >= PWM_INSTANCE_MAX) {
        return PWM_ERROR_INVALID_INSTANCE;
    }
    if (channel >= PWM_CHANNEL_MAX) {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    /* 注意：互补输出仅支持TIM1/TIM8，但参数校验阶段只检查范围 */
    
    /* ========== 占位空函数 ========== */
    #warning "PWM_EnableComplementary: 占位空函数，功能未实现，待完善"
    
    return PWM_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 禁用PWM互补输出
 * @param[in] instance PWM实例索引
 * @param[in] channel PWM通道
 * @return PWM_Status_t 错误码
 * @note 占位空函数，功能未实现，待完善
 */
PWM_Status_t PWM_DisableComplementary(PWM_Instance_t instance, PWM_Channel_t channel)
{
    /* ========== 参数校验 ========== */
    if (instance >= PWM_INSTANCE_MAX) {
        return PWM_ERROR_INVALID_INSTANCE;
    }
    if (channel >= PWM_CHANNEL_MAX) {
        return PWM_ERROR_INVALID_CHANNEL;
    }
    
    /* ========== 占位空函数 ========== */
    #warning "PWM_DisableComplementary: 占位空函数，功能未实现，待完善"
    
    return PWM_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 设置PWM死区时间
 * @param[in] instance PWM实例索引
 * @param[in] dead_time_ns 死区时间（纳秒）
 * @return PWM_Status_t 错误码
 * @note 占位空函数，功能未实现，待完善
 */
PWM_Status_t PWM_SetDeadTime(PWM_Instance_t instance, uint16_t dead_time_ns)
{
    /* ========== 参数校验 ========== */
    if (instance >= PWM_INSTANCE_MAX) {
        return PWM_ERROR_INVALID_INSTANCE;
    }
    /* 注意：dead_time_ns范围0~65535ns，但uint16_t已经限制了范围，无需额外检查 */
    
    /* ========== 占位空函数 ========== */
    (void)dead_time_ns;
    #warning "PWM_SetDeadTime: 占位空函数，功能未实现，待完善"
    
    return PWM_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 使能PWM主输出（MOE）
 * @param[in] instance PWM实例索引
 * @return PWM_Status_t 错误码
 * @note 占位空函数，功能未实现，待完善
 */
PWM_Status_t PWM_EnableMainOutput(PWM_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= PWM_INSTANCE_MAX) {
        return PWM_ERROR_INVALID_INSTANCE;
    }
    
    /* ========== 占位空函数 ========== */
    #warning "PWM_EnableMainOutput: 占位空函数，功能未实现，待完善"
    
    return PWM_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 禁用PWM主输出（MOE）
 * @param[in] instance PWM实例索引
 * @return PWM_Status_t 错误码
 * @note 占位空函数，功能未实现，待完善
 */
PWM_Status_t PWM_DisableMainOutput(PWM_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= PWM_INSTANCE_MAX) {
        return PWM_ERROR_INVALID_INSTANCE;
    }
    
    /* ========== 占位空函数 ========== */
    #warning "PWM_DisableMainOutput: 占位空函数，功能未实现，待完善"
    
    return PWM_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 使能PWM刹车功能
 * @param[in] instance PWM实例索引
 * @param[in] source 刹车源
 * @param[in] polarity 刹车极性
 * @return PWM_Status_t 错误码
 * @note 占位空函数，功能未实现，待完善
 */
PWM_Status_t PWM_EnableBrake(PWM_Instance_t instance, PWM_BrakeSource_t source, PWM_BrakePolarity_t polarity)
{
    /* ========== 参数校验 ========== */
    if (instance >= PWM_INSTANCE_MAX) {
        return PWM_ERROR_INVALID_INSTANCE;
    }
    if (source > PWM_BRAKE_SOURCE_LOCK) {
        return PWM_ERROR_INVALID_PARAM;
    }
    if (polarity > PWM_BRAKE_POLARITY_HIGH) {
        return PWM_ERROR_INVALID_PARAM;
    }
    
    /* ========== 占位空函数 ========== */
    #warning "PWM_EnableBrake: 占位空函数，功能未实现，待完善"
    
    return PWM_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 禁用PWM刹车功能
 * @param[in] instance PWM实例索引
 * @return PWM_Status_t 错误码
 * @note 占位空函数，功能未实现，待完善
 */
PWM_Status_t PWM_DisableBrake(PWM_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= PWM_INSTANCE_MAX) {
        return PWM_ERROR_INVALID_INSTANCE;
    }
    
    /* ========== 占位空函数 ========== */
    #warning "PWM_DisableBrake: 占位空函数，功能未实现，待完善"
    
    return PWM_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 设置PWM对齐模式
 * @param[in] instance PWM实例索引
 * @param[in] align_mode 对齐模式
 * @return PWM_Status_t 错误码
 * @note 占位空函数，功能未实现，待完善
 */
PWM_Status_t PWM_SetAlignMode(PWM_Instance_t instance, PWM_AlignMode_t align_mode)
{
    /* ========== 参数校验 ========== */
    if (instance >= PWM_INSTANCE_MAX) {
        return PWM_ERROR_INVALID_INSTANCE;
    }
    if (align_mode > PWM_ALIGN_CENTER_3) {
        return PWM_ERROR_INVALID_PARAM;
    }
    
    /* ========== 占位空函数 ========== */
    #warning "PWM_SetAlignMode: 占位空函数，功能未实现，待完善"
    
    return PWM_ERROR_NOT_IMPLEMENTED;
}

#endif /* CONFIG_MODULE_TIMER_ENABLED */

