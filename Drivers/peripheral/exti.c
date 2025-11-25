/**
 * @file exti.c
 * @brief EXTI外部中断模块实现
 */

#include "exti.h"
#include "board.h"
#include "config.h"
#include "error_handler.h"
#include "gpio.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_rcc.h"
#include "misc.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#if CONFIG_MODULE_EXTI_ENABLED

/* 保存标准库函数指针，避免函数名冲突 */
#define STM32_EXTI_Init EXTI_Init
#define STM32_EXTI_GenerateSWInterrupt EXTI_GenerateSWInterrupt
#undef EXTI_Init
#undef EXTI_GenerateSWInterrupt

/* EXTI线到GPIO引脚的映射（用于GPIO配置） */
static const uint16_t exti_line_to_pin[] = {
    GPIO_Pin_0, GPIO_Pin_1, GPIO_Pin_2, GPIO_Pin_3,
    GPIO_Pin_4, GPIO_Pin_5, GPIO_Pin_6, GPIO_Pin_7,
    GPIO_Pin_8, GPIO_Pin_9, GPIO_Pin_10, GPIO_Pin_11,
    GPIO_Pin_12, GPIO_Pin_13, GPIO_Pin_14, GPIO_Pin_15,
    0, 0, 0, 0  /* Line 16-19 是特殊功能，不需要GPIO配置 */
};

/* 初始化标志 */
static bool g_exti_initialized[EXTI_LINE_MAX] = {false};

/* 中断回调函数 */
static EXTI_Callback_t g_exti_callbacks[EXTI_LINE_MAX] = {NULL};
static void *g_exti_user_data[EXTI_LINE_MAX] = {NULL};

/**
 * @brief 获取EXTI线对应的SPL库线值
 */
static uint32_t EXTI_GetLineValue(EXTI_Line_t line)
{
    switch (line)
    {
        case EXTI_LINE_0: return EXTI_Line0;
        case EXTI_LINE_1: return EXTI_Line1;
        case EXTI_LINE_2: return EXTI_Line2;
        case EXTI_LINE_3: return EXTI_Line3;
        case EXTI_LINE_4: return EXTI_Line4;
        case EXTI_LINE_5: return EXTI_Line5;
        case EXTI_LINE_6: return EXTI_Line6;
        case EXTI_LINE_7: return EXTI_Line7;
        case EXTI_LINE_8: return EXTI_Line8;
        case EXTI_LINE_9: return EXTI_Line9;
        case EXTI_LINE_10: return EXTI_Line10;
        case EXTI_LINE_11: return EXTI_Line11;
        case EXTI_LINE_12: return EXTI_Line12;
        case EXTI_LINE_13: return EXTI_Line13;
        case EXTI_LINE_14: return EXTI_Line14;
        case EXTI_LINE_15: return EXTI_Line15;
        case EXTI_LINE_16: return EXTI_Line16;
        case EXTI_LINE_17: return EXTI_Line17;
        case EXTI_LINE_18: return EXTI_Line18;
        case EXTI_LINE_19: return EXTI_Line19;
        default: return 0;
    }
}

/**
 * @brief 配置GPIO为EXTI模式
 */
static EXTI_Status_t EXTI_ConfigGPIO(EXTI_Line_t line, GPIO_TypeDef *port)
{
    if (line >= EXTI_LINE_16)
    {
        /* Line 16-19 是特殊功能，不需要GPIO配置 */
        return EXTI_OK;
    }
    
    if (port == NULL)
    {
        return EXTI_ERROR_INVALID_PARAM;
    }
    
    /* 使能AFIO时钟（用于GPIO复用功能） */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    
    /* 配置GPIO为浮空输入模式 */
    GPIO_Config(port, exti_line_to_pin[line], GPIO_MODE_INPUT_FLOATING, GPIO_SPEED_50MHz);
    
    /* 配置GPIO复用功能（将GPIO连接到EXTI线） */
    uint8_t port_source = 0;
    uint8_t pin_source = 0;
    
    if (port == GPIOA) port_source = GPIO_PortSourceGPIOA;
    else if (port == GPIOB) port_source = GPIO_PortSourceGPIOB;
    else if (port == GPIOC) port_source = GPIO_PortSourceGPIOC;
    else if (port == GPIOD) port_source = GPIO_PortSourceGPIOD;
    else if (port == GPIOE) port_source = GPIO_PortSourceGPIOE;
    else if (port == GPIOF) port_source = GPIO_PortSourceGPIOF;
    else if (port == GPIOG) port_source = GPIO_PortSourceGPIOG;
    
    if (exti_line_to_pin[line] == GPIO_Pin_0) pin_source = GPIO_PinSource0;
    else if (exti_line_to_pin[line] == GPIO_Pin_1) pin_source = GPIO_PinSource1;
    else if (exti_line_to_pin[line] == GPIO_Pin_2) pin_source = GPIO_PinSource2;
    else if (exti_line_to_pin[line] == GPIO_Pin_3) pin_source = GPIO_PinSource3;
    else if (exti_line_to_pin[line] == GPIO_Pin_4) pin_source = GPIO_PinSource4;
    else if (exti_line_to_pin[line] == GPIO_Pin_5) pin_source = GPIO_PinSource5;
    else if (exti_line_to_pin[line] == GPIO_Pin_6) pin_source = GPIO_PinSource6;
    else if (exti_line_to_pin[line] == GPIO_Pin_7) pin_source = GPIO_PinSource7;
    else if (exti_line_to_pin[line] == GPIO_Pin_8) pin_source = GPIO_PinSource8;
    else if (exti_line_to_pin[line] == GPIO_Pin_9) pin_source = GPIO_PinSource9;
    else if (exti_line_to_pin[line] == GPIO_Pin_10) pin_source = GPIO_PinSource10;
    else if (exti_line_to_pin[line] == GPIO_Pin_11) pin_source = GPIO_PinSource11;
    else if (exti_line_to_pin[line] == GPIO_Pin_12) pin_source = GPIO_PinSource12;
    else if (exti_line_to_pin[line] == GPIO_Pin_13) pin_source = GPIO_PinSource13;
    else if (exti_line_to_pin[line] == GPIO_Pin_14) pin_source = GPIO_PinSource14;
    else if (exti_line_to_pin[line] == GPIO_Pin_15) pin_source = GPIO_PinSource15;
    
    GPIO_EXTILineConfig(port_source, pin_source);
    
    return EXTI_OK;
}

/**
 * @brief EXTI初始化
 */
EXTI_Status_t EXTI_HW_Init(EXTI_Line_t line, EXTI_Trigger_t trigger, EXTI_Mode_t mode)
{
    EXTI_InitTypeDef EXTI_InitStructure;
    uint32_t line_value;
    
    /* 参数校验 */
    if (line >= EXTI_LINE_MAX)
    {
        ErrorHandler_Handle(EXTI_ERROR_INVALID_LINE, "EXTI");
        return EXTI_ERROR_INVALID_LINE;
    }
    
    if (g_exti_initialized[line])
    {
        ErrorHandler_Handle(EXTI_ERROR_ALREADY_INITIALIZED, "EXTI");
        return EXTI_ERROR_ALREADY_INITIALIZED;
    }
    
    /* 获取EXTI线值 */
    line_value = EXTI_GetLineValue(line);
    if (line_value == 0)
    {
        return EXTI_ERROR_INVALID_LINE;
    }
    
    /* 配置GPIO为EXTI模式（从board.h读取配置） */
    if (line < EXTI_LINE_16)
    {
        /* 从board.h读取EXTI配置 */
        static EXTI_Config_t g_exti_configs[EXTI_LINE_MAX] = EXTI_CONFIGS;
        if (g_exti_configs[line].enabled && g_exti_configs[line].port != NULL)
        {
            EXTI_Status_t gpio_status = EXTI_ConfigGPIO(line, g_exti_configs[line].port);
            if (gpio_status != EXTI_OK)
            {
                return gpio_status;
            }
        }
    }
    
    /* 配置EXTI */
    EXTI_InitStructure.EXTI_Line = line_value;
    EXTI_InitStructure.EXTI_Mode = (mode == EXTI_MODE_INTERRUPT) ? EXTI_Mode_Interrupt : EXTI_Mode_Event;
    
    switch (trigger)
    {
        case EXTI_TRIGGER_RISING:
            EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
            break;
        case EXTI_TRIGGER_FALLING:
            EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
            break;
        case EXTI_TRIGGER_RISING_FALLING:
            EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
            break;
        default:
            return EXTI_ERROR_INVALID_PARAM;
    }
    
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    STM32_EXTI_Init(&EXTI_InitStructure);
    
    /* 标记为已初始化 */
    g_exti_initialized[line] = true;
    
    return EXTI_OK;
}

/**
 * @brief EXTI反初始化
 */
EXTI_Status_t EXTI_Deinit(EXTI_Line_t line)
{
    EXTI_InitTypeDef EXTI_InitStructure;
    uint32_t line_value;
    
    /* 参数校验 */
    if (line >= EXTI_LINE_MAX)
    {
        return EXTI_ERROR_INVALID_LINE;
    }
    
    if (!g_exti_initialized[line])
    {
        return EXTI_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取EXTI线值 */
    line_value = EXTI_GetLineValue(line);
    if (line_value == 0)
    {
        return EXTI_ERROR_INVALID_LINE;
    }
    
    /* 禁用EXTI */
    EXTI_InitStructure.EXTI_Line = line_value;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = DISABLE;
    STM32_EXTI_Init(&EXTI_InitStructure);
    
    /* 清除回调函数 */
    g_exti_callbacks[line] = NULL;
    g_exti_user_data[line] = NULL;
    
    /* 标记为未初始化 */
    g_exti_initialized[line] = false;
    
    return EXTI_OK;
}

/**
 * @brief 设置EXTI中断回调函数
 */
EXTI_Status_t EXTI_SetCallback(EXTI_Line_t line, EXTI_Callback_t callback, void *user_data)
{
    if (line >= EXTI_LINE_MAX)
    {
        return EXTI_ERROR_INVALID_LINE;
    }
    
    if (!g_exti_initialized[line])
    {
        return EXTI_ERROR_NOT_INITIALIZED;
    }
    
    g_exti_callbacks[line] = callback;
    g_exti_user_data[line] = user_data;
    
    return EXTI_OK;
}

/**
 * @brief 使能EXTI中断
 */
EXTI_Status_t EXTI_Enable(EXTI_Line_t line)
{
    EXTI_InitTypeDef EXTI_InitStructure;
    uint32_t line_value;
    NVIC_InitTypeDef NVIC_InitStructure;
    
    if (line >= EXTI_LINE_MAX)
    {
        return EXTI_ERROR_INVALID_LINE;
    }
    
    if (!g_exti_initialized[line])
    {
        return EXTI_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取EXTI线值 */
    line_value = EXTI_GetLineValue(line);
    if (line_value == 0)
    {
        return EXTI_ERROR_INVALID_LINE;
    }
    
    /* 从board.h读取EXTI配置，确保EXTI_Mode和EXTI_Trigger正确 */
    static EXTI_Config_t g_exti_configs[EXTI_LINE_MAX] = EXTI_CONFIGS;
    
    /* 使能EXTI线（需要设置完整的配置，不能只设置Line和LineCmd） */
    EXTI_InitStructure.EXTI_Line = line_value;
    EXTI_InitStructure.EXTI_Mode = (g_exti_configs[line].mode == EXTI_MODE_INTERRUPT) ? EXTI_Mode_Interrupt : EXTI_Mode_Event;
    
    switch (g_exti_configs[line].trigger)
    {
        case EXTI_TRIGGER_RISING:
            EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
            break;
        case EXTI_TRIGGER_FALLING:
            EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
            break;
        case EXTI_TRIGGER_RISING_FALLING:
            EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
            break;
        default:
            /* 如果配置无效，使用双边沿触发作为默认值 */
            EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
            break;
    }
    
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    STM32_EXTI_Init(&EXTI_InitStructure);
    
    /* 配置NVIC中断优先级（使用默认优先级） */
    if (line <= EXTI_LINE_4)
    {
        /* EXTI0-4 有独立的中断向量 */
        IRQn_Type irq;
        switch (line)
        {
            case EXTI_LINE_0: irq = EXTI0_IRQn; break;
            case EXTI_LINE_1: irq = EXTI1_IRQn; break;
            case EXTI_LINE_2: irq = EXTI2_IRQn; break;
            case EXTI_LINE_3: irq = EXTI3_IRQn; break;
            case EXTI_LINE_4: irq = EXTI4_IRQn; break;
            default: return EXTI_ERROR_INVALID_LINE;
        }
        NVIC_InitStructure.NVIC_IRQChannel = irq;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;  /* 提高EXTI中断优先级，确保能及时响应 */
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);
    }
    else if (line <= EXTI_LINE_9)
    {
        /* EXTI5-9 共享中断向量 */
        NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);
    }
    else if (line <= EXTI_LINE_15)
    {
        /* EXTI10-15 共享中断向量 */
        NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);
    }
    else if (line == EXTI_LINE_16)
    {
        /* EXTI16 (PVD) 有独立的中断向量 */
        NVIC_InitStructure.NVIC_IRQChannel = PVD_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);
    }
    else if (line == EXTI_LINE_17)
    {
        /* EXTI17 (RTC Alarm) 有独立的中断向量 */
        NVIC_InitStructure.NVIC_IRQChannel = RTCAlarm_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);
    }
    else if (line == EXTI_LINE_18)
    {
        /* EXTI18 (USB Wakeup) 有独立的中断向量 */
        NVIC_InitStructure.NVIC_IRQChannel = USBWakeUp_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);
    }
    else if (line == EXTI_LINE_19)
    {
        /* EXTI19 (Ethernet Wakeup) 有独立的中断向量，仅在HD/XL型号上可用 */
        #if defined(STM32F10X_HD) || defined(STM32F10X_XL) || defined(STM32F10X_HD_VL)
        NVIC_InitStructure.NVIC_IRQChannel = ETH_WKUP_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);
        #endif
    }
    
    return EXTI_OK;
}

/**
 * @brief 禁用EXTI中断
 */
EXTI_Status_t EXTI_Disable(EXTI_Line_t line)
{
    EXTI_InitTypeDef EXTI_InitStructure;
    uint32_t line_value;
    
    if (line >= EXTI_LINE_MAX)
    {
        return EXTI_ERROR_INVALID_LINE;
    }
    
    if (!g_exti_initialized[line])
    {
        return EXTI_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取EXTI线值 */
    line_value = EXTI_GetLineValue(line);
    if (line_value == 0)
    {
        return EXTI_ERROR_INVALID_LINE;
    }
    
    /* 禁用EXTI线 */
    EXTI_InitStructure.EXTI_Line = line_value;
    EXTI_InitStructure.EXTI_LineCmd = DISABLE;
    STM32_EXTI_Init(&EXTI_InitStructure);
    
    return EXTI_OK;
}

/**
 * @brief 清除EXTI挂起标志
 */
EXTI_Status_t EXTI_ClearPending(EXTI_Line_t line)
{
    uint32_t line_value;
    
    if (line >= EXTI_LINE_MAX)
    {
        return EXTI_ERROR_INVALID_LINE;
    }
    
    line_value = EXTI_GetLineValue(line);
    if (line_value == 0)
    {
        return EXTI_ERROR_INVALID_LINE;
    }
    
    EXTI_ClearFlag(line_value);
    
    return EXTI_OK;
}

/**
 * @brief 获取EXTI挂起标志状态
 */
uint8_t EXTI_GetPendingStatus(EXTI_Line_t line)
{
    uint32_t line_value;
    
    if (line >= EXTI_LINE_MAX)
    {
        return 0;
    }
    
    line_value = EXTI_GetLineValue(line);
    if (line_value == 0)
    {
        return 0;
    }
    
    return (EXTI_GetFlagStatus(line_value) == SET) ? 1 : 0;
}

/**
 * @brief 生成软件中断
 */
EXTI_Status_t EXTI_HW_GenerateSWInterrupt(EXTI_Line_t line)
{
    uint32_t line_value;
    
    if (line >= EXTI_LINE_MAX)
    {
        return EXTI_ERROR_INVALID_LINE;
    }
    
    if (!g_exti_initialized[line])
    {
        return EXTI_ERROR_NOT_INITIALIZED;
    }
    
    line_value = EXTI_GetLineValue(line);
    if (line_value == 0)
    {
        return EXTI_ERROR_INVALID_LINE;
    }
    
    STM32_EXTI_GenerateSWInterrupt(line_value);
    
    return EXTI_OK;
}

/**
 * @brief 检查EXTI是否已初始化
 */
uint8_t EXTI_IsInitialized(EXTI_Line_t line)
{
    if (line >= EXTI_LINE_MAX)
    {
        return 0;
    }
    
    return g_exti_initialized[line] ? 1 : 0;
}

/**
 * @brief EXTI中断处理函数（应在中断服务程序中调用）
 */
void EXTI_IRQHandler(EXTI_Line_t line)
{
    uint32_t line_value;
    
    if (line >= EXTI_LINE_MAX)
    {
        return;
    }
    
    line_value = EXTI_GetLineValue(line);
    if (line_value == 0)
    {
        return;
    }
    
    /* 检查中断标志 */
    if (EXTI_GetITStatus(line_value) != RESET)
    {
        /* 清除中断标志 */
        EXTI_ClearITPendingBit(line_value);
        
        /* 调用回调函数 */
        if (g_exti_callbacks[line] != NULL)
        {
            g_exti_callbacks[line](line, g_exti_user_data[line]);
        }
    }
}

#endif /* CONFIG_MODULE_EXTI_ENABLED */

