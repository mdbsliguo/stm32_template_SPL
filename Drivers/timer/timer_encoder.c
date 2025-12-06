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
#include "stm32f10x_gpio.h"  /* 用于GPIO_PinRemapConfig */
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

/* TIM3重映射标志（用于支持PB4/PB5或PC6/PC7） */
static bool g_tim3_remap_enabled = false;  /* false=默认(PA6/PA7), true=重映射(PB4/PB5或PC6/PC7) */
static bool g_tim3_full_remap = false;     /* false=部分重映射(PB4/PB5), true=完全重映射(PC6/PC7) */

/* 中断回调函数数组 */
static ENCODER_IT_Callback_t g_encoder_it_callbacks[ENCODER_INSTANCE_MAX][2] = {NULL};
static void *g_encoder_it_user_data[ENCODER_INSTANCE_MAX][2] = {NULL};

/* 方向变化检测（用于DIRECTION中断） */
static ENCODER_Direction_t g_encoder_last_direction[ENCODER_INSTANCE_MAX] = {ENCODER_DIR_FORWARD, ENCODER_DIR_FORWARD, ENCODER_DIR_FORWARD, ENCODER_DIR_FORWARD};

/**
 * @brief 获取定时器外设时钟使能值
 */
static uint32_t ENCODER_GetPeriphClock(TIM_TypeDef *tim_periph)
{
    if (tim_periph == TIM1) return RCC_APB2Periph_TIM1;
    if (tim_periph == TIM2) return RCC_APB1Periph_TIM2;
    if (tim_periph == TIM3) return RCC_APB1Periph_TIM3;
    if (tim_periph == TIM4) return RCC_APB1Periph_TIM4;
    return 0;
}

/**
 * @brief 获取GPIO端口对应的时钟使能值
 */
static uint32_t ENCODER_GetGPIOClock(GPIO_TypeDef *gpio_port)
{
    if (gpio_port == GPIOA) return RCC_APB2Periph_GPIOA;
    if (gpio_port == GPIOB) return RCC_APB2Periph_GPIOB;
    if (gpio_port == GPIOC) return RCC_APB2Periph_GPIOC;
    if (gpio_port == GPIOD) return RCC_APB2Periph_GPIOD;
    return 0;
}

/**
 * @brief 公共校验和获取外设函数
 * @param[in] instance 编码器实例索引
 * @param[out] tim_periph 定时器外设指针
 * @return ENCODER_Status_t 状态码
 */
static ENCODER_Status_t ENCODER_ValidateAndGetPeriph(ENCODER_Instance_t instance, TIM_TypeDef **tim_periph)
{
    if (instance >= ENCODER_INSTANCE_MAX) {
        return ENCODER_ERROR_INVALID_INSTANCE;
    }
    
    if (!g_encoder_initialized[instance]) {
        return ENCODER_ERROR_NOT_INITIALIZED;
    }
    
    *tim_periph = encoder_tim_periphs[instance];
    if (*tim_periph == NULL) {
        return ENCODER_ERROR_INVALID_PERIPH;
    }
    
    return ENCODER_OK;
}

/**
 * @brief 设置TIM3重映射配置
 * @param[in] enable_remap 是否启用重映射（true=启用，false=默认PA6/PA7）
 * @param[in] full_remap 是否完全重映射（true=PC6/PC7，false=PB4/PB5）
 * @note 必须在ENCODER_Init之前调用
 * @note 仅对TIM3有效
 */
void ENCODER_SetTIM3Remap(bool enable_remap, bool full_remap)
{
    g_tim3_remap_enabled = enable_remap;
    g_tim3_full_remap = full_remap;
}

/**
 * @brief 获取GPIO引脚配置（TI1和TI2）
 * @note 编码器接口模式只能使用定时器的CH1和CH2
 */
static void ENCODER_GetGPIOConfig(ENCODER_Instance_t instance, 
                                   GPIO_TypeDef **port1, uint16_t *pin1,
                                   GPIO_TypeDef **port2, uint16_t *pin2)
{
    *port1 = NULL;
    *pin1 = 0;
    *port2 = NULL;
    *pin2 = 0;
    
    if (instance == ENCODER_INSTANCE_TIM1)
    {
        *port1 = GPIOA; *pin1 = GPIO_Pin_8;  /* TIM1_CH1 */
        *port2 = GPIOA; *pin2 = GPIO_Pin_9;  /* TIM1_CH2 */
    }
    else if (instance == ENCODER_INSTANCE_TIM2)
    {
        *port1 = GPIOA; *pin1 = GPIO_Pin_0;  /* TIM2_CH1 */
        *port2 = GPIOA; *pin2 = GPIO_Pin_1;  /* TIM2_CH2 */
    }
    else if (instance == ENCODER_INSTANCE_TIM3)
    {
        if (g_tim3_remap_enabled)
        {
            if (g_tim3_full_remap)
            {
                /* TIM3完全重映射：CH1=PC6, CH2=PC7 */
                *port1 = GPIOC; *pin1 = GPIO_Pin_6;  /* TIM3_CH1 */
                *port2 = GPIOC; *pin2 = GPIO_Pin_7;  /* TIM3_CH2 */
            }
            else
            {
                /* TIM3部分重映射：CH1=PB4, CH2=PB5 */
                *port1 = GPIOB; *pin1 = GPIO_Pin_4;  /* TIM3_CH1 */
                *port2 = GPIOB; *pin2 = GPIO_Pin_5;  /* TIM3_CH2 */
            }
        }
        else
        {
            /* TIM3默认：CH1=PA6, CH2=PA7 */
            *port1 = GPIOA; *pin1 = GPIO_Pin_6;  /* TIM3_CH1 */
            *port2 = GPIOA; *pin2 = GPIO_Pin_7;  /* TIM3_CH2 */
        }
    }
    else if (instance == ENCODER_INSTANCE_TIM4)
    {
        *port1 = GPIOB; *pin1 = GPIO_Pin_6;  /* TIM4_CH1 */
        *port2 = GPIOB; *pin2 = GPIO_Pin_7;  /* TIM4_CH2 */
    }
}

/**
 * @brief 编码器初始化
 */
ENCODER_Status_t ENCODER_Init(ENCODER_Instance_t instance, ENCODER_Mode_t mode)
{
    TIM_TypeDef *tim_periph;
    GPIO_TypeDef *gpio_port1, *gpio_port2;
    uint16_t gpio_pin1, gpio_pin2;
    uint32_t gpio_clock1, gpio_clock2;
    uint32_t tim_clock;
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    
    /* ========== 参数校验 ========== */
    if (instance >= ENCODER_INSTANCE_MAX) {
        return ENCODER_ERROR_INVALID_INSTANCE;
    }
    if (mode > ENCODER_MODE_TI12) {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已初始化 */
    if (g_encoder_initialized[instance]) {
        return ENCODER_ERROR_ALREADY_INITIALIZED;
    }
    
    /* 获取定时器外设 */
    tim_periph = encoder_tim_periphs[instance];
    if (tim_periph == NULL) {
        return ENCODER_ERROR_INVALID_PERIPH;
    }
    
    /* 获取GPIO配置 */
    ENCODER_GetGPIOConfig(instance, &gpio_port1, &gpio_pin1, &gpio_port2, &gpio_pin2);
    if (gpio_port1 == NULL || gpio_pin1 == 0 || gpio_port2 == NULL || gpio_pin2 == 0) {
        return ENCODER_ERROR_INVALID_PERIPH;
    }
    
    /* ========== 1. RCC时钟使能 ========== */
    /* 1.1 GPIO时钟使能 */
    gpio_clock1 = ENCODER_GetGPIOClock(gpio_port1);
    if (gpio_clock1 == 0) {
        return ENCODER_ERROR_GPIO_FAILED;
    }
    
    gpio_clock2 = ENCODER_GetGPIOClock(gpio_port2);
    if (gpio_clock2 == 0) {
        return ENCODER_ERROR_GPIO_FAILED;
    }
    
    /* 如果两个GPIO端口相同，只使能一次 */
    if (gpio_port1 == gpio_port2) {
        RCC_APB2PeriphClockCmd(gpio_clock1 | RCC_APB2Periph_AFIO, ENABLE);
    } else {
        RCC_APB2PeriphClockCmd(gpio_clock1 | gpio_clock2 | RCC_APB2Periph_AFIO, ENABLE);
    }
    
    /* 1.2 定时器时钟使能 */
    tim_clock = ENCODER_GetPeriphClock(tim_periph);
    if (tim_clock == 0) {
        return ENCODER_ERROR_INVALID_PERIPH;
    }
    
    if (tim_periph == TIM1) {
        RCC_APB2PeriphClockCmd(tim_clock, ENABLE);
    } else {
        RCC_APB1PeriphClockCmd(tim_clock, ENABLE);
    }
    
    /* ========== 1.3 配置TIM3重映射（如果需要） ========== */
    if (instance == ENCODER_INSTANCE_TIM3 && g_tim3_remap_enabled)
    {
        if (g_tim3_full_remap)
        {
            /* TIM3完全重映射：CH1=PC6, CH2=PC7 */
            GPIO_PinRemapConfig(GPIO_FullRemap_TIM3, ENABLE);
        }
        else
        {
            /* TIM3部分重映射：CH1=PB4, CH2=PB5 */
            GPIO_PinRemapConfig(GPIO_PartialRemap_TIM3, ENABLE);
        }
    }
    
    /* ========== 2. GPIO配置为复用功能输入模式 ========== */
    /* 编码器接口模式需要GPIO配置为复用功能（Alternate Function） */
    /* 对于TIM输入通道，使用上拉输入模式（适用于开漏输出的编码器） */
    /* 如果编码器内部已有上拉，可以改为浮空输入（GPIO_Mode_IN_FLOATING） */
    /* 注意：TIM3的CH1/CH2默认复用功能在PA6/PA7，无需重映射 */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  /* 上拉输入，适用于开漏输出的编码器 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    
    GPIO_InitStructure.GPIO_Pin = gpio_pin1;
    GPIO_Init(gpio_port1, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = gpio_pin2;
    GPIO_Init(gpio_port2, &GPIO_InitStructure);
    
    /* 配置GPIO复用功能（对于TIM输入通道，默认复用功能无需额外配置） */
    /* TIM3 CH1/CH2默认在PA6/PA7，无需GPIO_PinRemapConfig */
    
    /* ========== 3. 配置定时器时基单元 ========== */
    /* 先停止定时器，确保干净的状态（如果之前被其他模块使用过） */
    TIM_Cmd(tim_periph, DISABLE);
    
    /* 编码器接口模式下，CNT计数器在编码器信号的驱动下运行，不需要内部时钟 */
    /* 设置ARR为最大值，允许CNT在-32768到32767之间变化（16位计数器） */
    TIM_TimeBaseStructure.TIM_Period = 0xFFFF;
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  /* 编码器接口模式会自动处理方向 */
    TIM_TimeBaseInit(tim_periph, &TIM_TimeBaseStructure);
    
    /* ========== 4. 配置输入捕获通道（在编码器接口配置之前） ========== */
    /* 参考标准实现：先配置输入捕获通道，设置滤波器，然后再配置编码器接口 */
    /* 注意：TIM_EncoderInterfaceConfig会覆盖部分输入捕获配置，所以要先配置 */
    TIM_ICInitTypeDef TIM_ICInitStructure;
    TIM_ICStructInit(&TIM_ICInitStructure);  /* 初始化结构体，避免未赋值项产生随机值 */
    
    /* 配置通道1：设置滤波器，抗抖滤波 */
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
    TIM_ICInitStructure.TIM_ICFilter = 0xF;  /* 滤波器值：0x0-0xF，0xF为最大滤波 */
    TIM_ICInit(tim_periph, &TIM_ICInitStructure);
    
    /* 配置通道2：设置滤波器，抗抖滤波 */
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
    TIM_ICInitStructure.TIM_ICFilter = 0xF;  /* 滤波器值：0x0-0xF，0xF为最大滤波 */
    TIM_ICInit(tim_periph, &TIM_ICInitStructure);
    
    /* ========== 5. 配置编码器接口模式 ========== */
    /* 使用TIM_EncoderInterfaceConfig配置编码器接口 */
    /* 注意：此函数必须在输入捕获初始化之后调用，否则会被覆盖部分配置 */
    /* 注意：在编码器模式下，TIM_ICPolarity_Rising/Falling表示"是否反相"，不是上升沿/下降沿 */
    TIM_EncoderInterfaceConfig(tim_periph, 
                               encoder_mode_map[mode],  /* 编码器模式：TI1/TI2/TI12 */
                               TIM_ICPolarity_Rising,   /* TI1极性：不反相 */
                               TIM_ICPolarity_Rising);  /* TI2极性：不反相 */
    
    /* ========== 6. 使能编码器通道 ========== */
    /* 编码器接口模式需要同时使能CH1和CH2 */
    TIM_CCxCmd(tim_periph, TIM_Channel_1, TIM_CCx_Enable);
    TIM_CCxCmd(tim_periph, TIM_Channel_2, TIM_CCx_Enable);
    
    /* ========== 6. 清零计数器 ========== */
    TIM_SetCounter(tim_periph, 0);
    
    /* ========== 7. 标记为已初始化 ========== */
    g_encoder_initialized[instance] = true;
    
    return ENCODER_OK;
}

/**
 * @brief 编码器反初始化
 */
ENCODER_Status_t ENCODER_Deinit(ENCODER_Instance_t instance)
{
    TIM_TypeDef *tim_periph;
    
    /* ========== 参数校验 ========== */
    if (instance >= ENCODER_INSTANCE_MAX) {
        return ENCODER_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (!g_encoder_initialized[instance]) {
        return ENCODER_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取定时器外设 */
    tim_periph = encoder_tim_periphs[instance];
    if (tim_periph == NULL) {
        return ENCODER_ERROR_INVALID_PERIPH;
    }
    
    /* 停止定时器 */
    TIM_Cmd(tim_periph, DISABLE);
    
    /* 禁用编码器通道 */
    TIM_CCxCmd(tim_periph, TIM_Channel_1, TIM_CCx_Disable);
    TIM_CCxCmd(tim_periph, TIM_Channel_2, TIM_CCx_Disable);
    
    /* 清除中断回调 */
    g_encoder_it_callbacks[instance][0] = NULL;
    g_encoder_it_callbacks[instance][1] = NULL;
    g_encoder_it_user_data[instance][0] = NULL;
    g_encoder_it_user_data[instance][1] = NULL;
    
    /* 标记为未初始化 */
    g_encoder_initialized[instance] = false;
    
    return ENCODER_OK;
}

/**
 * @brief 读取编码器计数值（有符号）
 */
ENCODER_Status_t ENCODER_ReadCount(ENCODER_Instance_t instance, int32_t *count)
{
    TIM_TypeDef *tim_periph;
    ENCODER_Status_t status;
    uint16_t cnt_value;
    
    /* ========== 参数校验 ========== */
    if (count == NULL) {
        return ENCODER_ERROR_NULL_PTR;
    }
    
    status = ENCODER_ValidateAndGetPeriph(instance, &tim_periph);
    if (status != ENCODER_OK) {
        return status;
    }
    
    /* 读取CNT寄存器值（16位无符号）并转换为有符号32位 */
    cnt_value = TIM_GetCounter(tim_periph);
    *count = (int32_t)(int16_t)cnt_value;
    
    return ENCODER_OK;
}

/**
 * @brief 读取编码器计数值（无符号）
 */
ENCODER_Status_t ENCODER_ReadCountUnsigned(ENCODER_Instance_t instance, uint32_t *count)
{
    TIM_TypeDef *tim_periph;
    ENCODER_Status_t status;
    
    /* ========== 参数校验 ========== */
    if (count == NULL) {
        return ENCODER_ERROR_NULL_PTR;
    }
    
    status = ENCODER_ValidateAndGetPeriph(instance, &tim_periph);
    if (status != ENCODER_OK) {
        return status;
    }
    
    /* 读取CNT寄存器值（16位无符号） */
    *count = (uint32_t)TIM_GetCounter(tim_periph);
    
    return ENCODER_OK;
}

/**
 * @brief 设置编码器计数值
 */
ENCODER_Status_t ENCODER_SetCount(ENCODER_Instance_t instance, int32_t count)
{
    TIM_TypeDef *tim_periph;
    ENCODER_Status_t status;
    uint16_t cnt_value;
    
    /* ========== 参数校验 ========== */
    status = ENCODER_ValidateAndGetPeriph(instance, &tim_periph);
    if (status != ENCODER_OK) {
        return status;
    }
    
    /* 将32位有符号数转换为16位无符号数 */
    /* 限制在-32768到32767范围内 */
    if (count > 32767) {
        cnt_value = 32767;
    } else if (count < -32768) {
        cnt_value = 0x8000;  /* -32768的16位无符号表示 */
    } else {
        /* 先转换为有符号16位，再转换为无符号16位 */
        int16_t signed_val = (int16_t)count;
        cnt_value = (uint16_t)signed_val;
    }
    
    /* 设置CNT寄存器值 */
    TIM_SetCounter(tim_periph, cnt_value);
    
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
    ENCODER_Status_t status;
    uint16_t dir_bit;
    
    /* ========== 参数校验 ========== */
    if (direction == NULL) {
        return ENCODER_ERROR_NULL_PTR;
    }
    
    status = ENCODER_ValidateAndGetPeriph(instance, &tim_periph);
    if (status != ENCODER_OK) {
        return status;
    }
    
    /* 读取DIR位（在CR1寄存器的第4位，bit 4） */
    /* 标准库没有定义TIM_CR1_DIR宏，直接使用位掩码0x0010 */
    dir_bit = (tim_periph->CR1 & 0x0010) >> 4;
    
    /* DIR=0表示向上计数（正转），DIR=1表示向下计数（反转） */
    if (dir_bit == 0) {
        *direction = ENCODER_DIR_FORWARD;  /* 正转 */
    } else {
        *direction = ENCODER_DIR_BACKWARD;  /* 反转 */
    }
    
    return ENCODER_OK;
}

/**
 * @brief 启动编码器
 */
ENCODER_Status_t ENCODER_Start(ENCODER_Instance_t instance)
{
    TIM_TypeDef *tim_periph;
    ENCODER_Status_t status;
    
    /* ========== 参数校验 ========== */
    status = ENCODER_ValidateAndGetPeriph(instance, &tim_periph);
    if (status != ENCODER_OK) {
        return status;
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
    ENCODER_Status_t status;
    
    /* ========== 参数校验 ========== */
    status = ENCODER_ValidateAndGetPeriph(instance, &tim_periph);
    if (status != ENCODER_OK) {
        return status;
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
    /* ========== 参数校验 ========== */
    if (instance >= ENCODER_INSTANCE_MAX) {
        return 0;  /* 无效实例返回0（未初始化） */
    }
    
    /* 返回初始化状态 */
    return g_encoder_initialized[instance] ? 1 : 0;
}

/* ========== 中断模式功能实现 ========== */

/**
 * @brief 获取编码器中断类型对应的SPL库中断值
 * @param[in] it_type 编码器中断类型
 * @return uint16_t TIM中断类型值
 */
static uint16_t ENCODER_GetITValue(ENCODER_IT_t it_type)
{
    switch (it_type) {
        case ENCODER_IT_OVERFLOW:
            return TIM_IT_Update;  /* 溢出中断对应Update中断 */
        case ENCODER_IT_DIRECTION:
            return TIM_IT_CC1;     /* 方向变化中断使用CC1中断（编码器接口模式下，CC1/CC2都可以） */
        default:
            return 0;
    }
}

/**
 * @brief 获取定时器中断向量
 * @param[in] instance 编码器实例索引
 * @return IRQn_Type 中断向量号
 */
static IRQn_Type ENCODER_GetIRQn(ENCODER_Instance_t instance)
{
    switch (instance) {
        case ENCODER_INSTANCE_TIM1:
            return TIM1_UP_IRQn;  /* TIM1使用UP中断 */
        case ENCODER_INSTANCE_TIM2:
            return TIM2_IRQn;     /* TIM2全局中断 */
        case ENCODER_INSTANCE_TIM3:
            return TIM3_IRQn;     /* TIM3全局中断 */
        case ENCODER_INSTANCE_TIM4:
            return TIM4_IRQn;     /* TIM4全局中断 */
        default:
            return (IRQn_Type)0;
    }
}

/**
 * @brief 使能编码器中断
 */
ENCODER_Status_t ENCODER_EnableIT(ENCODER_Instance_t instance, ENCODER_IT_t it_type)
{
    TIM_TypeDef *tim_periph;
    uint16_t tim_it;
    IRQn_Type irqn;
    
    /* ========== 参数校验 ========== */
    if (instance >= ENCODER_INSTANCE_MAX) {
        return ENCODER_ERROR_INVALID_INSTANCE;
    }
    if (it_type > ENCODER_IT_DIRECTION) {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    if (!g_encoder_initialized[instance]) {
        return ENCODER_ERROR_NOT_INITIALIZED;
    }
    
    /* ========== 获取定时器外设 ========== */
    tim_periph = encoder_tim_periphs[instance];
    if (tim_periph == NULL) {
        return ENCODER_ERROR_INVALID_PERIPH;
    }
    
    /* ========== 获取中断类型和中断向量 ========== */
    tim_it = ENCODER_GetITValue(it_type);
    irqn = ENCODER_GetIRQn(instance);
    
    if (tim_it == 0 || irqn == 0) {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    /* ========== 使能定时器中断 ========== */
    TIM_ITConfig(tim_periph, tim_it, ENABLE);
    
    /* ========== 使能NVIC中断 ========== */
    NVIC_ConfigIRQ(irqn, 2, 0, true);  /* 抢占优先级=2，子优先级=0，使能 */
    
    /* ========== 初始化方向记录（用于DIRECTION中断） ========== */
    if (it_type == ENCODER_IT_DIRECTION) {
        ENCODER_Direction_t current_dir;
        if (ENCODER_GetDirection(instance, &current_dir) == ENCODER_OK) {
            g_encoder_last_direction[instance] = current_dir;
        }
    }
    
    return ENCODER_OK;
}

/**
 * @brief 禁用编码器中断
 */
ENCODER_Status_t ENCODER_DisableIT(ENCODER_Instance_t instance, ENCODER_IT_t it_type)
{
    TIM_TypeDef *tim_periph;
    uint16_t tim_it;
    
    /* ========== 参数校验 ========== */
    if (instance >= ENCODER_INSTANCE_MAX) {
        return ENCODER_ERROR_INVALID_INSTANCE;
    }
    if (it_type > ENCODER_IT_DIRECTION) {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    if (!g_encoder_initialized[instance]) {
        return ENCODER_ERROR_NOT_INITIALIZED;
    }
    
    /* ========== 获取定时器外设 ========== */
    tim_periph = encoder_tim_periphs[instance];
    if (tim_periph == NULL) {
        return ENCODER_ERROR_INVALID_PERIPH;
    }
    
    /* ========== 获取中断类型 ========== */
    tim_it = ENCODER_GetITValue(it_type);
    if (tim_it == 0) {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    
    /* ========== 禁用定时器中断 ========== */
    TIM_ITConfig(tim_periph, tim_it, DISABLE);
    
    /* 注意：不在这里禁用NVIC，因为可能有其他中断类型仍在使用同一个中断向量 */
    
    return ENCODER_OK;
}

/**
 * @brief 设置编码器中断回调函数
 */
ENCODER_Status_t ENCODER_SetITCallback(ENCODER_Instance_t instance, ENCODER_IT_t it_type,
                                       ENCODER_IT_Callback_t callback, void *user_data)
{
    /* ========== 参数校验 ========== */
    if (instance >= ENCODER_INSTANCE_MAX) {
        return ENCODER_ERROR_INVALID_INSTANCE;
    }
    if (it_type > ENCODER_IT_DIRECTION) {
        return ENCODER_ERROR_INVALID_PARAM;
    }
    /* 注意：callback可以为NULL（表示禁用回调），user_data可以为NULL */
    
    /* ========== 保存回调函数 ========== */
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
    int32_t count;
    ENCODER_Direction_t current_dir;
    
    /* ========== 参数校验 ========== */
    if (instance >= ENCODER_INSTANCE_MAX) {
        return;  /* 无效实例直接返回 */
    }
    
    if (!g_encoder_initialized[instance]) {
        return;  /* 未初始化直接返回 */
    }
    
    tim_periph = encoder_tim_periphs[instance];
    if (tim_periph == NULL) {
        return;
    }
    
    /* ========== 处理溢出中断 ========== */
    if (TIM_GetITStatus(tim_periph, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(tim_periph, TIM_IT_Update);
        
        /* 读取当前计数值 */
        if (ENCODER_ReadCount(instance, &count) == ENCODER_OK) {
            /* 调用溢出中断回调 */
            if (g_encoder_it_callbacks[instance][ENCODER_IT_OVERFLOW] != NULL) {
                g_encoder_it_callbacks[instance][ENCODER_IT_OVERFLOW](
                    instance, ENCODER_IT_OVERFLOW, count, 
                    g_encoder_it_user_data[instance][ENCODER_IT_OVERFLOW]);
            }
        }
    }
    
    /* ========== 处理方向变化中断（通过CC1中断检测，每次边沿变化都触发） ========== */
    if (TIM_GetITStatus(tim_periph, TIM_IT_CC1) != RESET) {
        TIM_ClearITPendingBit(tim_periph, TIM_IT_CC1);
        
        /* 读取当前计数值（每次边沿变化CNT都会更新） */
        if (ENCODER_ReadCount(instance, &count) == ENCODER_OK) {
            /* 读取当前方向 */
            if (ENCODER_GetDirection(instance, &current_dir) == ENCODER_OK) {
                /* 检查方向是否变化 */
                if (current_dir != g_encoder_last_direction[instance]) {
                    g_encoder_last_direction[instance] = current_dir;
                }
            }
            
            /* 每次边沿变化都调用方向变化中断回调（用于实时响应编码器旋转） */
            /* 注意：这里复用ENCODER_IT_DIRECTION回调，但实际是每次计数变化都触发 */
            if (g_encoder_it_callbacks[instance][ENCODER_IT_DIRECTION] != NULL) {
                g_encoder_it_callbacks[instance][ENCODER_IT_DIRECTION](
                    instance, ENCODER_IT_DIRECTION, count, 
                    g_encoder_it_user_data[instance][ENCODER_IT_DIRECTION]);
            }
        }
    }
}


/* 定时器中断服务程序入口 */
void TIM1_UP_IRQHandler(void) { ENCODER_IRQHandler(ENCODER_INSTANCE_TIM1); }

/* TIM2/3/4的全局中断服务程序（与输入捕获和TIM2_TimeBase共用） */
/* 注意：TIM2_IRQHandler已在stm32f10x_it.c中定义（用于TIM2_TimeBase），需要在那里同时处理编码器中断 */
/* 注意：如果同时使用编码器和输入捕获，需要在中断服务程序中同时处理 */

/* TIM3中断服务程序（用于编码器，与输入捕获共用） */
void TIM3_IRQHandler(void) 
{
    /* 检查是否是编码器中断 */
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET || 
        TIM_GetITStatus(TIM3, TIM_IT_CC1) != RESET) {
        ENCODER_IRQHandler(ENCODER_INSTANCE_TIM3);
    }
    /* 注意：如果同时使用输入捕获，需要在这里也调用IC_IRQHandler */
}

/* TIM4中断服务程序（用于编码器，与输入捕获共用） */
void TIM4_IRQHandler(void) 
{
    /* 检查是否是编码器中断 */
    if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET || 
        TIM_GetITStatus(TIM4, TIM_IT_CC1) != RESET) {
        ENCODER_IRQHandler(ENCODER_INSTANCE_TIM4);
    }
    /* 注意：如果同时使用输入捕获，需要在这里也调用IC_IRQHandler */
}

#endif /* CONFIG_MODULE_TIMER_ENABLED */
