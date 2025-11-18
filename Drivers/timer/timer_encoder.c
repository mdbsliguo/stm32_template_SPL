/**
 * @file timer_encoder.c
 * @brief 定时器编码器模式模块实现
 */

#include "timer_encoder.h"
#include "board.h"
#include "config.h"
#include "error_handler.h"
#include "gpio.h"
#include "nvic.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"
#include "misc.h"
#include "system_stm32f10x.h"
#include <stdbool.h>
#include <stddef.h>

#if CONFIG_MODULE_TIMER_ENABLED

/* 定时器外设映射 */
static TIM_TypeDef *encoder_tim_periphs[] = {
    TIM1,  /* ENCODER_INSTANCE_TIM1 */
    TIM2,  /* ENCODER_INSTANCE_TIM2 */
    TIM3,  /* ENCODER_INSTANCE_TIM3 */
    TIM4,  /* ENCODER_INSTANCE_TIM4 */
};

/* 编码器模式映射 */
static const uint16_t encoder_mode_map[] = {
    TIM_EncoderMode_TI1,   /* ENCODER_MODE_TI1 */
    TIM_EncoderMode_TI2,   /* ENCODER_MODE_TI2 */
    TIM_EncoderMode_TI12, /* ENCODER_MODE_TI12 */
};

/* 初始化标志 */
static bool g_encoder_initialized[ENCODER_INSTANCE_MAX] = {false};

/* 中断回调函数数组 */
static ENCODER_IT_Callback_t g_encoder_it_callbacks[ENCODER_INSTANCE_MAX][2] = {NULL};
static void *g_encoder_it_user_data[ENCODER_INSTANCE_MAX][2] = {NULL};

/* 方向变化检测 */
static ENCODER_Direction_t g_encoder_last_direction[ENCODER_INSTANCE_MAX] = {ENCODER_DIR_FORWARD};

/**
 * @brief 获取定时器外设时钟
 */
static uint32_t ENCODER_GetPeriphClock(TIM_TypeDef *tim_periph)
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
 * @brief 获取GPIO引脚配置（TI1和TI2）
 */
static void ENCODER_GetGPIOConfig(ENCODER_Instance_t instance, 
                                   GPIO_TypeDef **port1, uint16_t *pin1,
                                   GPIO_TypeDef **port2, uint16_t *pin2)
{
    /* 根据定时器确定GPIO引脚 */
    switch (instance)
    {
        case ENCODER_INSTANCE_TIM1:
            *port1 = GPIOA; *pin1 = GPIO_Pin_8;   /* TI1: PA8 */
            *port2 = GPIOA; *pin2 = GPIO_Pin_9;   /* TI2: PA9 */
            break;
        case ENCODER_INSTANCE_TIM2:
            *port1 = GPIOA; *pin1 = GPIO_Pin_0;   /* TI1: PA0 */
            *port2 = GPIOA; *pin2 = GPIO_Pin_1;   /* TI2: PA1 */
            break;
        case ENCODER_INSTANCE_TIM3:
            *port1 = GPIOA; *pin1 = GPIO_Pin_6;   /* TI1: PA6 */
            *port2 = GPIOA; *pin2 = GPIO_Pin_7;   /* TI2: PA7 */
            break;
        case ENCODER_INSTANCE_TIM4:
            *port1 = GPIOB; *pin1 = GPIO_Pin_6;   /* TI1: PB6 */
            *port2 = GPIOB; *pin2 = GPIO_Pin_7;   /* TI2: PB7 */
            break;
        default:
            *port1 = GPIOA; *pin1 = GPIO_Pin_0;
            *port2 = GPIOA; *pin2 = GPIO_Pin_1;
            break;
    }
}

/**
 * @brief 编码器初始化
 */
ENCODER_Status_t ENCODER_Init(ENCODER_Instance_t instance, ENCODER_Mode_t mode)
{
    TIM_TypeDef *tim_periph;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    GPIO_TypeDef *gpio_port1, *gpio_port2;
    uint16_t gpio_pin1, gpio_pin2;
    
    /* 参数校验 */
    if (instance >= ENCODER_INSTANCE_MAX)
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    if (mode >= 3) /* ENCODER_MODE_TI12 = 2 */
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    if (g_encoder_initialized[instance])
    {
        ERROR_HANDLER_Report(ERROR_BASE_TIMER, __FILE__, __LINE__, "Encoder already initialized");
        return ENCODER_ERROR_ALREADY_INITIALIZED;
    }
    
    tim_periph = encoder_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return ENCODER_ERROR_INVALID_PERIPH;
    }
    
    /* 使能定时器和GPIO时钟 */
    uint32_t periph_clock = ENCODER_GetPeriphClock(tim_periph);
    if (tim_periph == TIM1)
    {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    }
    else
    {
        RCC_APB1PeriphClockCmd(periph_clock, ENABLE);
    }
    
    /* 获取GPIO配置 */
    ENCODER_GetGPIOConfig(instance, &gpio_port1, &gpio_pin1, &gpio_port2, &gpio_pin2);
    GPIO_EnableClock(gpio_port1);
    if (gpio_port1 != gpio_port2)
    {
        GPIO_EnableClock(gpio_port2);
    }
    
    /* 配置GPIO为浮空输入模式（编码器输入） */
    GPIO_Config(gpio_port1, gpio_pin1, GPIO_MODE_INPUT_FLOATING, GPIO_SPEED_50MHz);
    GPIO_Config(gpio_port2, gpio_pin2, GPIO_MODE_INPUT_FLOATING, GPIO_SPEED_50MHz);
    
    /* 配置定时器时基（编码器模式下，Period用于设置计数范围） */
    TIM_TimeBaseStructure.TIM_Period = 0xFFFF;  /* 16位计数器最大值 */
    TIM_TimeBaseStructure.TIM_Prescaler = 0;     /* 编码器模式下预分频器无效 */
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; /* 编码器模式下此参数无效 */
    TIM_TimeBaseInit(tim_periph, &TIM_TimeBaseStructure);
    
    /* 配置编码器接口 */
    TIM_EncoderInterfaceConfig(tim_periph, encoder_mode_map[mode],
                               TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);
    
    /* 清零计数器 */
    TIM_SetCounter(tim_periph, 0);
    
    /* 使能定时器 */
    TIM_Cmd(tim_periph, ENABLE);
    
    g_encoder_initialized[instance] = true;
    
    return ENCODER_OK;
}

/**
 * @brief 编码器反初始化
 */
ENCODER_Status_t ENCODER_Deinit(ENCODER_Instance_t instance)
{
    TIM_TypeDef *tim_periph;
    
    if (instance >= ENCODER_INSTANCE_MAX)
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    if (!g_encoder_initialized[instance])
    {
        return ENCODER_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = encoder_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return ENCODER_ERROR_INVALID_PERIPH;
    }
    
    /* 禁用定时器 */
    TIM_Cmd(tim_periph, DISABLE);
    
    g_encoder_initialized[instance] = false;
    
    return ENCODER_OK;
}

/**
 * @brief 读取编码器计数值（有符号）
 */
ENCODER_Status_t ENCODER_ReadCount(ENCODER_Instance_t instance, int32_t *count)
{
    TIM_TypeDef *tim_periph;
    uint16_t counter_value;
    
    if (instance >= ENCODER_INSTANCE_MAX)
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    if (!g_encoder_initialized[instance])
    {
        return ENCODER_ERROR_NOT_INITIALIZED;
    }
    
    if (count == NULL)
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    tim_periph = encoder_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return ENCODER_ERROR_INVALID_PERIPH;
    }
    
    /* 读取计数器值 */
    counter_value = TIM_GetCounter(tim_periph);
    
    /* 转换为有符号数（将0-65535映射到-32768到32767） */
    if (counter_value > 32767)
    {
        *count = (int32_t)counter_value - 65536;
    }
    else
    {
        *count = (int32_t)counter_value;
    }
    
    return ENCODER_OK;
}

/**
 * @brief 读取编码器计数值（无符号）
 */
ENCODER_Status_t ENCODER_ReadCountUnsigned(ENCODER_Instance_t instance, uint32_t *count)
{
    TIM_TypeDef *tim_periph;
    
    if (instance >= ENCODER_INSTANCE_MAX)
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    if (!g_encoder_initialized[instance])
    {
        return ENCODER_ERROR_NOT_INITIALIZED;
    }
    
    if (count == NULL)
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    tim_periph = encoder_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return ENCODER_ERROR_INVALID_PERIPH;
    }
    
    /* 读取计数器值 */
    *count = TIM_GetCounter(tim_periph);
    
    return ENCODER_OK;
}

/**
 * @brief 设置编码器计数值
 */
ENCODER_Status_t ENCODER_SetCount(ENCODER_Instance_t instance, int32_t count)
{
    TIM_TypeDef *tim_periph;
    uint16_t counter_value;
    
    if (instance >= ENCODER_INSTANCE_MAX)
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    if (!g_encoder_initialized[instance])
    {
        return ENCODER_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = encoder_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return ENCODER_ERROR_INVALID_PERIPH;
    }
    
    /* 将有符号数转换为无符号数 */
    if (count < 0)
    {
        counter_value = (uint16_t)(count + 65536);
    }
    else
    {
        counter_value = (uint16_t)count;
    }
    
    /* 设置计数器值 */
    TIM_SetCounter(tim_periph, counter_value);
    
    return ENCODER_OK;
}

/**
 * @brief 清零编码器计数值
 */
ENCODER_Status_t ENCODER_ClearCount(ENCODER_Instance_t instance)
{
    return ENCODER_SetCount(instance, 0);
}

/**
 * @brief 获取编码器方向
 */
ENCODER_Status_t ENCODER_GetDirection(ENCODER_Instance_t instance, ENCODER_Direction_t *direction)
{
    TIM_TypeDef *tim_periph;
    uint16_t dir_bit;
    
    if (instance >= ENCODER_INSTANCE_MAX)
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    if (!g_encoder_initialized[instance])
    {
        return ENCODER_ERROR_NOT_INITIALIZED;
    }
    
    if (direction == NULL)
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    tim_periph = encoder_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return ENCODER_ERROR_INVALID_PERIPH;
    }
    
    /* 读取方向位（DIR位在CR1寄存器中） */
    dir_bit = (tim_periph->CR1 & TIM_CR1_DIR) >> 4;
    
    /* 编码器模式下，DIR=0表示向上计数（正向），DIR=1表示向下计数（反向） */
    *direction = (dir_bit == 0) ? ENCODER_DIR_FORWARD : ENCODER_DIR_BACKWARD;
    
    return ENCODER_OK;
}

/**
 * @brief 启动编码器
 */
ENCODER_Status_t ENCODER_Start(ENCODER_Instance_t instance)
{
    TIM_TypeDef *tim_periph;
    
    if (instance >= ENCODER_INSTANCE_MAX)
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    if (!g_encoder_initialized[instance])
    {
        return ENCODER_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = encoder_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return ENCODER_ERROR_INVALID_PERIPH;
    }
    
    /* 使能定时器 */
    TIM_Cmd(tim_periph, ENABLE);
    
    return ENCODER_OK;
}

/**
 * @brief 停止编码器
 */
ENCODER_Status_t ENCODER_Stop(ENCODER_Instance_t instance)
{
    TIM_TypeDef *tim_periph;
    
    if (instance >= ENCODER_INSTANCE_MAX)
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    if (!g_encoder_initialized[instance])
    {
        return ENCODER_ERROR_NOT_INITIALIZED;
    }
    
    tim_periph = encoder_tim_periphs[instance];
    if (tim_periph == NULL)
    {
        return ENCODER_ERROR_INVALID_PERIPH;
    }
    
    /* 禁用定时器 */
    TIM_Cmd(tim_periph, DISABLE);
    
    return ENCODER_OK;
}

/**
 * @brief 检查编码器是否已初始化
 */
uint8_t ENCODER_IsInitialized(ENCODER_Instance_t instance)
{
    if (instance >= ENCODER_INSTANCE_MAX)
    {
        return 0;
    }
    
    return g_encoder_initialized[instance] ? 1 : 0;
}

/* ========== 中断模式功能实现 ========== */

/**
 * @brief 获取编码器中断类型对应的SPL库中断值
 */
static uint16_t ENCODER_GetITValue(ENCODER_IT_t it_type)
{
    switch (it_type)
    {
        case ENCODER_IT_OVERFLOW: return TIM_IT_Update;
        case ENCODER_IT_DIRECTION: return TIM_IT_Update;  /* 方向变化通过读取DIR位检测 */
        default: return 0;
    }
}

/**
 * @brief 获取定时器中断向量
 */
static IRQn_Type ENCODER_GetIRQn(ENCODER_Instance_t instance)
{
    switch (instance)
    {
        case ENCODER_INSTANCE_TIM1: return TIM1_UP_IRQn;
        case ENCODER_INSTANCE_TIM2: return TIM2_IRQn;
        case ENCODER_INSTANCE_TIM3: return TIM3_IRQn;
        case ENCODER_INSTANCE_TIM4: return TIM4_IRQn;
        default: return (IRQn_Type)0;
    }
}

/**
 * @brief 使能编码器中断
 */
ENCODER_Status_t ENCODER_EnableIT(ENCODER_Instance_t instance, ENCODER_IT_t it_type)
{
    TIM_TypeDef *tim_periph;
    uint16_t it_value;
    IRQn_Type irqn;
    
    if (instance >= ENCODER_INSTANCE_MAX)
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    if (!g_encoder_initialized[instance])
    {
        return ENCODER_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 2)
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    tim_periph = encoder_tim_periphs[instance];
    it_value = ENCODER_GetITValue(it_type);
    if (it_value == 0)
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    /* 使能定时器中断 */
    TIM_ITConfig(tim_periph, it_value, ENABLE);
    
    /* 使能NVIC中断 */
    irqn = ENCODER_GetIRQn(instance);
    if (irqn != 0)
    {
        NVIC_EnableIRQ(irqn);
    }
    
    return ENCODER_OK;
}

/**
 * @brief 禁用编码器中断
 */
ENCODER_Status_t ENCODER_DisableIT(ENCODER_Instance_t instance, ENCODER_IT_t it_type)
{
    TIM_TypeDef *tim_periph;
    uint16_t it_value;
    
    if (instance >= ENCODER_INSTANCE_MAX)
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    if (!g_encoder_initialized[instance])
    {
        return ENCODER_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 2)
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    tim_periph = encoder_tim_periphs[instance];
    it_value = ENCODER_GetITValue(it_type);
    if (it_value == 0)
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    /* 禁用定时器中断 */
    TIM_ITConfig(tim_periph, it_value, DISABLE);
    
    return ENCODER_OK;
}

/**
 * @brief 设置编码器中断回调函数
 */
ENCODER_Status_t ENCODER_SetITCallback(ENCODER_Instance_t instance, ENCODER_IT_t it_type,
                                       ENCODER_IT_Callback_t callback, void *user_data)
{
    if (instance >= ENCODER_INSTANCE_MAX)
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    if (!g_encoder_initialized[instance])
    {
        return ENCODER_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 2)
    {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    g_encoder_it_callbacks[instance][it_type] = callback;
    g_encoder_it_user_data[instance][it_type] = user_data;
    
    return ENCODER_OK;
}

/**
 * @brief 编码器中断服务函数
 */
void ENCODER_IRQHandler(ENCODER_Instance_t instance)
{
    TIM_TypeDef *tim_periph;
    ENCODER_Direction_t current_direction;
    int32_t count;
    
    if (instance >= ENCODER_INSTANCE_MAX || !g_encoder_initialized[instance])
    {
        return;
    }
    
    tim_periph = encoder_tim_periphs[instance];
    
    /* 处理溢出中断 */
    if (TIM_GetITStatus(tim_periph, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(tim_periph, TIM_IT_Update);
        
        /* 读取当前计数值 */
        count = (int32_t)TIM_GetCounter(tim_periph);
        
        /* 调用溢出回调 */
        if (g_encoder_it_callbacks[instance][ENCODER_IT_OVERFLOW] != NULL)
        {
            g_encoder_it_callbacks[instance][ENCODER_IT_OVERFLOW](
                instance, ENCODER_IT_OVERFLOW, count,
                g_encoder_it_user_data[instance][ENCODER_IT_OVERFLOW]);
        }
        
        /* 检测方向变化 */
        current_direction = (TIM_GetDirectionStatus(tim_periph) == TIM_CounterMode_Up) ?
                           ENCODER_DIR_FORWARD : ENCODER_DIR_BACKWARD;
        
        if (current_direction != g_encoder_last_direction[instance])
        {
            g_encoder_last_direction[instance] = current_direction;
            
            /* 调用方向变化回调 */
            if (g_encoder_it_callbacks[instance][ENCODER_IT_DIRECTION] != NULL)
            {
                g_encoder_it_callbacks[instance][ENCODER_IT_DIRECTION](
                    instance, ENCODER_IT_DIRECTION, count,
                    g_encoder_it_user_data[instance][ENCODER_IT_DIRECTION]);
            }
        }
    }
}

/* 定时器中断服务程序入口 */
void TIM1_UP_IRQHandler(void) { ENCODER_IRQHandler(ENCODER_INSTANCE_TIM1); }
/* TIM2/3/4的溢出中断与输入捕获共用，已在timer_input_capture.c中定义 */

#endif /* CONFIG_MODULE_TIMER_ENABLED */

