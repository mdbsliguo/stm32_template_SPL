/**
 * @file adc.c
 * @brief ADC驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的ADC驱动，支持ADC1，单次/连续转换，多通道扫描
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_ADC_ENABLED

/* Include our header */
#include "adc.h"

#include "gpio.h"
#include "delay.h"
#include "nvic.h"
#include "dma.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_adc.h"
#include "misc.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 从board.h加载配置 */
static ADC_Config_t g_adc_configs[ADC_INSTANCE_MAX] = ADC_CONFIGS;

/* 初始化标志 */
static bool g_adc_initialized[ADC_INSTANCE_MAX] = {false};

/* 连续转换标志 */
static bool g_adc_continuous[ADC_INSTANCE_MAX] = {false};

/* 默认超时时间（毫秒） */
#define ADC_DEFAULT_TIMEOUT_MS  1000

/* 中断回调函数数组 */
static ADC_IT_Callback_t g_adc_it_callbacks[ADC_INSTANCE_MAX][3] = {NULL};
static void *g_adc_it_user_data[ADC_INSTANCE_MAX][3] = {NULL};

/* DMA模式相关变量 */
static uint16_t *g_adc_dma_buffer[ADC_INSTANCE_MAX] = {NULL};
static uint16_t g_adc_dma_buffer_size[ADC_INSTANCE_MAX] = {0};
static uint8_t g_adc_dma_channel_count[ADC_INSTANCE_MAX] = {0};

/* DMA通道映射（ADC对应的DMA通道） */
/* 注意：ADC1使用DMA2_CH1（仅HD/CL/HD_VL），ADC2/3不支持DMA */
static const DMA_Channel_t adc_dma_channels[ADC_INSTANCE_MAX] = {
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    DMA_CHANNEL_2_1,  /* ADC1 -> DMA2_CH1 (仅HD/CL/HD_VL) */
#else
    DMA_CHANNEL_MAX,  /* ADC1在MD/LD型号上不支持DMA */
#endif
};

/**
 * @brief 获取ADC外设时钟
 * @param[in] adc_periph ADC外设指针
 * @return uint32_t 时钟使能值，失败返回0
 */
static uint32_t ADC_GetPeriphClock(ADC_TypeDef *adc_periph)
{
    if (adc_periph == ADC1)
    {
        return RCC_APB2Periph_ADC1;
    }
    else if (adc_periph == ADC2)
    {
        return RCC_APB2Periph_ADC2;
    }
    else if (adc_periph == ADC3)
    {
        return RCC_APB2Periph_ADC3;
    }
    return 0;
}

/**
 * @brief 获取GPIO时钟
 * @param[in] port GPIO端口指针
 * @return uint32_t GPIO时钟使能值
 */
static uint32_t ADC_GetGPIOClock(GPIO_TypeDef *port)
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
 * @brief 获取ADC通道对应的GPIO配置
 * @param[in] channel ADC通道
 * @param[out] port GPIO端口指针
 * @param[out] pin GPIO引脚号
 * @return ADC_Status_t 错误码
 */
static ADC_Status_t ADC_GetChannelGPIO(uint8_t channel, GPIO_TypeDef **port, uint16_t *pin)
{
    /* ADC通道到GPIO的映射（STM32F103C8T6） */
    switch (channel)
    {
        case ADC_Channel_0:  /* PA0 */
            *port = GPIOA;
            *pin = GPIO_Pin_0;
            break;
        case ADC_Channel_1:  /* PA1 */
            *port = GPIOA;
            *pin = GPIO_Pin_1;
            break;
        case ADC_Channel_2:  /* PA2 */
            *port = GPIOA;
            *pin = GPIO_Pin_2;
            break;
        case ADC_Channel_3:  /* PA3 */
            *port = GPIOA;
            *pin = GPIO_Pin_3;
            break;
        case ADC_Channel_4:  /* PA4 */
            *port = GPIOA;
            *pin = GPIO_Pin_4;
            break;
        case ADC_Channel_5:  /* PA5 */
            *port = GPIOA;
            *pin = GPIO_Pin_5;
            break;
        case ADC_Channel_6:  /* PA6 */
            *port = GPIOA;
            *pin = GPIO_Pin_6;
            break;
        case ADC_Channel_7:  /* PA7 */
            *port = GPIOA;
            *pin = GPIO_Pin_7;
            break;
        case ADC_Channel_8:  /* PB0 */
            *port = GPIOB;
            *pin = GPIO_Pin_0;
            break;
        case ADC_Channel_9:  /* PB1 */
            *port = GPIOB;
            *pin = GPIO_Pin_1;
            break;
        default:
            return ADC_ERROR_INVALID_CHANNEL;
    }
    
    return ADC_OK;
}

/**
 * @brief 等待ADC标志位
 * @param[in] adc_periph ADC外设指针
 * @param[in] flag 标志位
 * @param[in] timeout_ms 超时时间（毫秒）
 * @return ADC_Status_t 错误码
 */
static ADC_Status_t ADC_WaitFlag(ADC_TypeDef *adc_periph, uint32_t flag, uint32_t timeout_ms)
{
    uint32_t start_tick = Delay_GetTick();
    
    while (ADC_GetFlagStatus(adc_periph, flag) == RESET)
    {
        /* 检查超时 */
        uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
        if (elapsed > timeout_ms)
        {
            return ADC_ERROR_TIMEOUT;
        }
    }
    
    return ADC_OK;
}

/**
 * @brief ADC初始化
 */
ADC_Status_t ADC_Init(ADC_Instance_t instance)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;
    ADC_Status_t status;
    uint32_t adc_clock;
    uint32_t gpio_clock;
    uint8_t i;
    
    /* 参数校验 */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    if (!g_adc_configs[instance].enabled)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    if (g_adc_initialized[instance])
    {
        return ADC_OK;
    }
    
    /* 获取ADC外设时钟 */
    adc_clock = ADC_GetPeriphClock(g_adc_configs[instance].adc_periph);
    if (adc_clock == 0)
    {
        return ADC_ERROR_INVALID_PERIPH;
    }
    
    /* 使能ADC外设时钟 */
    RCC_APB2PeriphClockCmd(adc_clock, ENABLE);
    
    /* 使能ADC时钟 */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);  /* ADC时钟 = APB2时钟 / 6 */
    
    /* 配置ADC通道GPIO为模拟输入 */
    for (i = 0; i < g_adc_configs[instance].channel_count; i++)
    {
        GPIO_TypeDef *port;
        uint16_t pin;
        
        status = ADC_GetChannelGPIO(g_adc_configs[instance].channels[i], &port, &pin);
        if (status != ADC_OK)
        {
            return status;
        }
        
        gpio_clock = ADC_GetGPIOClock(port);
        if (gpio_clock != 0)
        {
            RCC_APB2PeriphClockCmd(gpio_clock, ENABLE);
        }
        
        GPIO_InitStructure.GPIO_Pin = pin;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;  /* 模拟输入 */
        GPIO_Init(port, &GPIO_InitStructure);
    }
    
    /* 配置ADC外设 */
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = (g_adc_configs[instance].channel_count > 1) ? ENABLE : DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;  /* 默认单次转换 */
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = g_adc_configs[instance].channel_count;
    
    ADC_Init(g_adc_configs[instance].adc_periph, &ADC_InitStructure);
    
    /* 配置ADC通道序列 */
    for (i = 0; i < g_adc_configs[instance].channel_count; i++)
    {
        ADC_RegularChannelConfig(g_adc_configs[instance].adc_periph,
                                 g_adc_configs[instance].channels[i],
                                 i + 1,  /* 序列位置（1-16） */
                                 g_adc_configs[instance].sample_time);
    }
    
    /* 使能ADC */
    ADC_Cmd(g_adc_configs[instance].adc_periph, ENABLE);
    
    /* ADC校准 */
    ADC_ResetCalibration(g_adc_configs[instance].adc_periph);
    while (ADC_GetResetCalibrationStatus(g_adc_configs[instance].adc_periph));
    ADC_StartCalibration(g_adc_configs[instance].adc_periph);
    while (ADC_GetCalibrationStatus(g_adc_configs[instance].adc_periph));
    
    /* 标记为已初始化 */
    g_adc_initialized[instance] = true;
    g_adc_continuous[instance] = false;
    
    return ADC_OK;
}

/**
 * @brief ADC反初始化
 */
ADC_Status_t ADC_Deinit(ADC_Instance_t instance)
{
    uint32_t adc_clock;
    
    /* 参数校验 */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    if (!g_adc_initialized[instance])
    {
        return ADC_OK;
    }
    
    /* 禁用ADC */
    ADC_Cmd(g_adc_configs[instance].adc_periph, DISABLE);
    
    /* 获取ADC外设时钟 */
    adc_clock = ADC_GetPeriphClock(g_adc_configs[instance].adc_periph);
    if (adc_clock != 0)
    {
        /* 禁用ADC外设时钟 */
        RCC_APB2PeriphClockCmd(adc_clock, DISABLE);
    }
    
    /* 标记为未初始化 */
    g_adc_initialized[instance] = false;
    g_adc_continuous[instance] = false;
    
    return ADC_OK;
}

/**
 * @brief ADC单次转换（阻塞式）
 */
ADC_Status_t ADC_ReadChannel(ADC_Instance_t instance, uint8_t channel, uint16_t *value, uint32_t timeout)
{
    ADC_Status_t status;
    
    /* 参数校验 */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    if (value == NULL)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    if (!g_adc_initialized[instance])
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }
    
    /* 使用默认超时时间 */
    if (timeout == 0)
    {
        timeout = ADC_DEFAULT_TIMEOUT_MS;
    }
    
    /* 如果正在连续转换，先停止 */
    if (g_adc_continuous[instance])
    {
        ADC_StopContinuous(instance);
    }
    
    /* 配置单通道转换 */
    ADC_RegularChannelConfig(g_adc_configs[instance].adc_periph, channel, 1, g_adc_configs[instance].sample_time);
    
    /* 启动转换 */
    ADC_SoftwareStartConvCmd(g_adc_configs[instance].adc_periph, ENABLE);
    
    /* 等待转换完成 */
    status = ADC_WaitFlag(g_adc_configs[instance].adc_periph, ADC_FLAG_EOC, timeout);
    if (status != ADC_OK)
    {
        return status;
    }
    
    /* 读取转换结果 */
    *value = ADC_GetConversionValue(g_adc_configs[instance].adc_periph);
    
    return ADC_OK;
}

/**
 * @brief ADC设置通道采样时间
 */
ADC_Status_t ADC_SetChannelSampleTime(ADC_Instance_t instance, uint8_t channel, uint8_t sample_time)
{
    ADC_TypeDef *adc_periph;
    
    if (instance >= ADC_INSTANCE_MAX || !g_adc_initialized[instance])
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }
    
    if (channel > ADC_Channel_17)
    {
        return ADC_ERROR_INVALID_CHANNEL;
    }
    
    adc_periph = g_adc_configs[instance].adc_periph;
    
    /* 配置通道采样时间（使用规则通道配置函数，rank=1） */
    ADC_RegularChannelConfig(adc_periph, channel, 1, sample_time);
    
    return ADC_OK;
}

/**
 * @brief ADC多通道扫描转换（阻塞式）
 */
ADC_Status_t ADC_ReadChannels(ADC_Instance_t instance, const uint8_t *channels, uint8_t channel_count,
                              uint16_t *values, uint32_t timeout)
{
    ADC_Status_t status;
    uint8_t i;
    
    /* 参数校验 */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    if (channels == NULL || values == NULL || channel_count == 0 || channel_count > 16)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    if (!g_adc_initialized[instance])
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }
    
    /* 使用默认超时时间 */
    if (timeout == 0)
    {
        timeout = ADC_DEFAULT_TIMEOUT_MS;
    }
    
    /* 如果正在连续转换，先停止 */
    if (g_adc_continuous[instance])
    {
        ADC_StopContinuous(instance);
    }
    
    /* 配置多通道序列 */
    for (i = 0; i < channel_count; i++)
    {
        ADC_RegularChannelConfig(g_adc_configs[instance].adc_periph,
                                 channels[i],
                                 i + 1,
                                 g_adc_configs[instance].sample_time);
    }
    
    /* 启动转换 */
    ADC_SoftwareStartConvCmd(g_adc_configs[instance].adc_periph, ENABLE);
    
    /* 等待所有通道转换完成 */
    status = ADC_WaitFlag(g_adc_configs[instance].adc_periph, ADC_FLAG_EOC, timeout);
    if (status != ADC_OK)
    {
        return status;
    }
    
    /* 读取所有通道的转换结果 */
    for (i = 0; i < channel_count; i++)
    {
        values[i] = ADC_GetConversionValue(g_adc_configs[instance].adc_periph);
        
        /* 等待下一个通道转换完成（最后一个通道除外） */
        if (i < channel_count - 1)
        {
            status = ADC_WaitFlag(g_adc_configs[instance].adc_periph, ADC_FLAG_EOC, timeout);
            if (status != ADC_OK)
            {
                return status;
            }
        }
    }
    
    return ADC_OK;
}

/**
 * @brief 启动ADC连续转换
 */
ADC_Status_t ADC_StartContinuous(ADC_Instance_t instance, uint8_t channel)
{
    /* 参数校验 */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    if (!g_adc_initialized[instance])
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }
    
    /* 配置单通道 */
    ADC_RegularChannelConfig(g_adc_configs[instance].adc_periph, channel, 1, g_adc_configs[instance].sample_time);
    
    /* 配置为连续转换模式 */
    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(g_adc_configs[instance].adc_periph, &ADC_InitStructure);
    
    /* 启动连续转换 */
    ADC_SoftwareStartConvCmd(g_adc_configs[instance].adc_periph, ENABLE);
    
    g_adc_continuous[instance] = true;
    
    return ADC_OK;
}

/**
 * @brief 停止ADC连续转换
 */
ADC_Status_t ADC_StopContinuous(ADC_Instance_t instance)
{
    /* 参数校验 */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    if (!g_adc_initialized[instance])
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }
    
    /* 停止连续转换 */
    ADC_SoftwareStartConvCmd(g_adc_configs[instance].adc_periph, DISABLE);
    
    g_adc_continuous[instance] = false;
    
    return ADC_OK;
}

/**
 * @brief 读取ADC连续转换结果
 */
ADC_Status_t ADC_ReadContinuous(ADC_Instance_t instance, uint16_t *value)
{
    /* 参数校验 */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    if (value == NULL)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    if (!g_adc_initialized[instance])
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }
    
    if (!g_adc_continuous[instance])
    {
        return ADC_ERROR_BUSY;
    }
    
    /* 读取转换结果 */
    *value = ADC_GetConversionValue(g_adc_configs[instance].adc_periph);
    
    return ADC_OK;
}

/**
 * @brief 检查ADC是否已初始化
 */
uint8_t ADC_IsInitialized(ADC_Instance_t instance)
{
    if (instance >= ADC_INSTANCE_MAX)
    {
        return 0;
    }
    
    return g_adc_initialized[instance] ? 1 : 0;
}

/**
 * @brief 获取ADC外设指针
 */
ADC_TypeDef* ADC_GetPeriph(ADC_Instance_t instance)
{
    if (instance >= ADC_INSTANCE_MAX)
    {
        return NULL;
    }
    
    if (!g_adc_initialized[instance])
    {
        return NULL;
    }
    
    return g_adc_configs[instance].adc_periph;
}

/* ========== 中断模式功能实现 ========== */

/**
 * @brief 获取ADC中断类型对应的SPL库中断值
 */
static uint16_t ADC_GetITValue(ADC_IT_t it_type)
{
    switch (it_type)
    {
        case ADC_IT_EOC:  return ADC_IT_EOC;
        case ADC_IT_JEOC: return ADC_IT_JEOC;
        case ADC_IT_AWD:  return ADC_IT_AWD;
        default: return 0;
    }
}

/**
 * @brief 使能ADC中断
 */
ADC_Status_t ADC_EnableIT(ADC_Instance_t instance, ADC_IT_t it_type)
{
    ADC_TypeDef *adc_periph;
    uint16_t it_value;
    
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    if (!g_adc_initialized[instance])
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 3)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    adc_periph = g_adc_configs[instance].adc_periph;
    it_value = ADC_GetITValue(it_type);
    if (it_value == 0)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    /* 使能ADC中断 */
    ADC_ITConfig(adc_periph, it_value, ENABLE);
    
    /* 使能NVIC中断 */
    NVIC_HW_EnableIRQ(ADC1_2_IRQn);
    
    return ADC_OK;
}

/**
 * @brief 禁用ADC中断
 */
ADC_Status_t ADC_DisableIT(ADC_Instance_t instance, ADC_IT_t it_type)
{
    ADC_TypeDef *adc_periph;
    uint16_t it_value;
    
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    if (!g_adc_initialized[instance])
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 3)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    adc_periph = g_adc_configs[instance].adc_periph;
    it_value = ADC_GetITValue(it_type);
    if (it_value == 0)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    /* 禁用ADC中断 */
    ADC_ITConfig(adc_periph, it_value, DISABLE);
    
    return ADC_OK;
}

/**
 * @brief 设置ADC中断回调函数
 */
ADC_Status_t ADC_SetITCallback(ADC_Instance_t instance, ADC_IT_t it_type,
                                ADC_IT_Callback_t callback, void *user_data)
{
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    if (!g_adc_initialized[instance])
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 3)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    g_adc_it_callbacks[instance][it_type] = callback;
    g_adc_it_user_data[instance][it_type] = user_data;
    
    return ADC_OK;
}

/**
 * @brief ADC中断服务函数
 */
void ADC_IRQHandler(ADC_Instance_t instance)
{
    ADC_TypeDef *adc_periph;
    uint16_t value;
    
    if (instance >= ADC_INSTANCE_MAX || !g_adc_initialized[instance])
    {
        return;
    }
    
    adc_periph = g_adc_configs[instance].adc_periph;
    
    /* 处理EOC中断（转换完成） */
    if (ADC_GetITStatus(adc_periph, ADC_IT_EOC) != RESET)
    {
        ADC_ClearITPendingBit(adc_periph, ADC_IT_EOC);
        value = ADC_GetConversionValue(adc_periph);
        
        /* 调用回调函数 */
        if (g_adc_it_callbacks[instance][ADC_IT_EOC] != NULL)
        {
            g_adc_it_callbacks[instance][ADC_IT_EOC](instance, ADC_IT_EOC, value, g_adc_it_user_data[instance][ADC_IT_EOC]);
        }
    }
    
    /* 处理JEOC中断（注入转换完成） */
    if (ADC_GetITStatus(adc_periph, ADC_IT_JEOC) != RESET)
    {
        ADC_ClearITPendingBit(adc_periph, ADC_IT_JEOC);
        value = ADC_GetInjectedConversionValue(adc_periph, ADC_InjectedChannel_1);
        
        /* 调用回调函数 */
        if (g_adc_it_callbacks[instance][ADC_IT_JEOC] != NULL)
        {
            g_adc_it_callbacks[instance][ADC_IT_JEOC](instance, ADC_IT_JEOC, value, g_adc_it_user_data[instance][ADC_IT_JEOC]);
        }
    }
    
    /* 处理AWD中断（模拟看门狗） */
    if (ADC_GetITStatus(adc_periph, ADC_IT_AWD) != RESET)
    {
        ADC_ClearITPendingBit(adc_periph, ADC_IT_AWD);
        
        /* 调用回调函数 */
        if (g_adc_it_callbacks[instance][ADC_IT_AWD] != NULL)
        {
            g_adc_it_callbacks[instance][ADC_IT_AWD](instance, ADC_IT_AWD, 0, g_adc_it_user_data[instance][ADC_IT_AWD]);
        }
    }
}

/* ADC中断服务程序入口 */
void ADC1_2_IRQHandler(void)
{
    ADC_IRQHandler(ADC_INSTANCE_1);
    /* ADC2中断处理（如果实现） */
}

/* ========== DMA模式功能实现 ========== */

/**
 * @brief ADC DMA启动（多通道扫描）
 */
ADC_Status_t ADC_StartDMA(ADC_Instance_t instance, const uint8_t *channels, uint8_t channel_count,
                          uint16_t *buffer, uint16_t buffer_size)
{
    ADC_TypeDef *adc_periph;
    DMA_Channel_t dma_channel;
    DMA_Status_t dma_status;
    ADC_InitTypeDef ADC_InitStructure;
    uint8_t i;
    
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    if (!g_adc_initialized[instance])
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }
    
    if (channels == NULL || channel_count == 0 || buffer == NULL || buffer_size == 0)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    if (buffer_size % channel_count != 0)
    {
        return ADC_ERROR_INVALID_PARAM;  /* 缓冲区大小必须是通道数量的倍数 */
    }
    
    dma_channel = adc_dma_channels[instance];
    if (dma_channel >= DMA_CHANNEL_MAX)
    {
        return ADC_ERROR_INVALID_PARAM;  /* 该ADC实例不支持DMA */
    }
    
    adc_periph = g_adc_configs[instance].adc_periph;
    
    /* 检查DMA通道是否已初始化 */
    if (!DMA_IsInitialized(dma_channel))
    {
        dma_status = DMA_HW_Init(dma_channel);
        if (dma_status != DMA_OK)
        {
            return ADC_ERROR_INVALID_PARAM;
        }
    }
    
    /* 停止之前的传输 */
    DMA_Stop(dma_channel);
    ADC_DMACmd(adc_periph, DISABLE);
    
    /* 配置ADC为多通道扫描模式 */
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = channel_count;
    ADC_Init(adc_periph, &ADC_InitStructure);
    
    /* 配置ADC通道序列 */
    for (i = 0; i < channel_count; i++)
    {
        ADC_RegularChannelConfig(adc_periph, channels[i], i + 1, ADC_SampleTime_55Cycles5);
    }
    
    /* 配置DMA传输（外设到内存，循环模式） */
    dma_status = DMA_ConfigTransfer(dma_channel, (uint32_t)&adc_periph->DR,
                                    (uint32_t)buffer, buffer_size,
                                    DMA_DIR_PERIPHERAL_TO_MEMORY, 2);  /* 半字 */
    if (dma_status != DMA_OK)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    /* 设置DMA为循环模式 */
    DMA_SetMode(dma_channel, 1);  /* 循环模式 */
    
    /* 保存DMA相关信息 */
    g_adc_dma_buffer[instance] = buffer;
    g_adc_dma_buffer_size[instance] = buffer_size;
    g_adc_dma_channel_count[instance] = channel_count;
    
    /* 使能ADC DMA */
    ADC_DMACmd(adc_periph, ENABLE);
    
    /* 启动DMA传输 */
    dma_status = DMA_Start(dma_channel);
    if (dma_status != DMA_OK)
    {
        ADC_DMACmd(adc_periph, DISABLE);
        return ADC_ERROR_INVALID_PARAM;
    }
    
    /* 启动ADC转换 */
    ADC_SoftwareStartConvCmd(adc_periph, ENABLE);
    
    return ADC_OK;
}

/**
 * @brief ADC DMA停止
 */
ADC_Status_t ADC_StopDMA(ADC_Instance_t instance)
{
    ADC_TypeDef *adc_periph;
    DMA_Channel_t dma_channel;
    
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    if (!g_adc_initialized[instance])
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }
    
    dma_channel = adc_dma_channels[instance];
    if (dma_channel >= DMA_CHANNEL_MAX)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    adc_periph = g_adc_configs[instance].adc_periph;
    
    /* 停止ADC转换 */
    ADC_SoftwareStartConvCmd(adc_periph, DISABLE);
    
    /* 禁用ADC DMA */
    ADC_DMACmd(adc_periph, DISABLE);
    
    /* 停止DMA传输 */
    DMA_Stop(dma_channel);
    
    /* 清除DMA相关信息 */
    g_adc_dma_buffer[instance] = NULL;
    g_adc_dma_buffer_size[instance] = 0;
    g_adc_dma_channel_count[instance] = 0;
    
    return ADC_OK;
}

/* ========== 注入通道功能实现 ========== */

/**
 * @brief ADC注入通道配置
 */
ADC_Status_t ADC_ConfigInjectedChannel(ADC_Instance_t instance, uint8_t channel, uint8_t rank, uint8_t sample_time)
{
    ADC_TypeDef *adc_periph;
    
    if (instance >= ADC_INSTANCE_MAX || !g_adc_initialized[instance])
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }
    
    if (channel > ADC_Channel_17)
    {
        return ADC_ERROR_INVALID_CHANNEL;
    }
    
    if (rank < 1 || rank > 4)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    adc_periph = g_adc_configs[instance].adc_periph;
    ADC_InjectedChannelConfig(adc_periph, channel, rank, sample_time);
    
    return ADC_OK;
}

/**
 * @brief ADC启动注入转换
 */
ADC_Status_t ADC_StartInjectedConversion(ADC_Instance_t instance, uint8_t external_trigger)
{
    ADC_TypeDef *adc_periph;
    
    if (instance >= ADC_INSTANCE_MAX || !g_adc_initialized[instance])
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }
    
    adc_periph = g_adc_configs[instance].adc_periph;
    
    if (external_trigger)
    {
        ADC_ExternalTrigInjectedConvCmd(adc_periph, ENABLE);
    }
    else
    {
        ADC_ExternalTrigInjectedConvCmd(adc_periph, DISABLE);
        ADC_SoftwareStartInjectedConvCmd(adc_periph, ENABLE);
    }
    
    return ADC_OK;
}

/**
 * @brief ADC读取注入转换结果
 */
ADC_Status_t ADC_ReadInjectedChannel(ADC_Instance_t instance, uint8_t rank, uint16_t *value)
{
    ADC_TypeDef *adc_periph;
    uint8_t injected_channel;
    
    if (instance >= ADC_INSTANCE_MAX || !g_adc_initialized[instance] || value == NULL)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    if (rank < 1 || rank > 4)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    adc_periph = g_adc_configs[instance].adc_periph;
    
    switch (rank)
    {
        case 1: injected_channel = ADC_InjectedChannel_1; break;
        case 2: injected_channel = ADC_InjectedChannel_2; break;
        case 3: injected_channel = ADC_InjectedChannel_3; break;
        case 4: injected_channel = ADC_InjectedChannel_4; break;
        default: return ADC_ERROR_INVALID_PARAM;
    }
    
    *value = ADC_GetInjectedConversionValue(adc_periph, injected_channel);
    return ADC_OK;
}

/**
 * @brief ADC设置注入通道偏移量
 */
ADC_Status_t ADC_SetInjectedOffset(ADC_Instance_t instance, uint8_t rank, uint16_t offset)
{
    ADC_TypeDef *adc_periph;
    uint8_t injected_channel;
    
    if (instance >= ADC_INSTANCE_MAX || !g_adc_initialized[instance])
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }
    
    if (rank < 1 || rank > 4 || offset > 4095)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    adc_periph = g_adc_configs[instance].adc_periph;
    
    switch (rank)
    {
        case 1: injected_channel = ADC_InjectedChannel_1; break;
        case 2: injected_channel = ADC_InjectedChannel_2; break;
        case 3: injected_channel = ADC_InjectedChannel_3; break;
        case 4: injected_channel = ADC_InjectedChannel_4; break;
        default: return ADC_ERROR_INVALID_PARAM;
    }
    
    ADC_SetInjectedOffset(adc_periph, injected_channel, offset);
    return ADC_OK;
}

/* ========== 双ADC模式功能实现 ========== */

/**
 * @brief 获取双ADC模式对应的SPL库模式值
 */
static uint32_t ADC_GetDualModeValue(ADC_DualMode_t mode)
{
    switch (mode)
    {
        case ADC_DUAL_MODE_INDEPENDENT: return ADC_Mode_Independent;
        case ADC_DUAL_MODE_REG_INJEC_SIMULT: return ADC_Mode_RegInjecSimult;
        case ADC_DUAL_MODE_REG_SIMULT_ALTER_TRIG: return ADC_Mode_RegSimult_AlterTrig;
        case ADC_DUAL_MODE_INJEC_SIMULT_FAST_INTERL: return ADC_Mode_InjecSimult_FastInterl;
        case ADC_DUAL_MODE_INJEC_SIMULT_SLOW_INTERL: return ADC_Mode_InjecSimult_SlowInterl;
        case ADC_DUAL_MODE_INJEC_SIMULT: return ADC_Mode_InjecSimult;
        case ADC_DUAL_MODE_REG_SIMULT: return ADC_Mode_RegSimult;
        case ADC_DUAL_MODE_FAST_INTERL: return ADC_Mode_FastInterl;
        case ADC_DUAL_MODE_SLOW_INTERL: return ADC_Mode_SlowInterl;
        case ADC_DUAL_MODE_ALTER_TRIG: return ADC_Mode_AlterTrig;
        default: return ADC_Mode_Independent;
    }
}

/**
 * @brief 配置ADC双ADC模式
 */
ADC_Status_t ADC_ConfigDualMode(ADC_DualMode_t mode)
{
    ADC_InitTypeDef ADC_InitStructure;
    uint32_t dual_mode_value;
    
#if !defined(STM32F10X_HD) && !defined(STM32F10X_CL) && !defined(STM32F10X_HD_VL)
    /* 双ADC模式仅支持HD/CL/HD_VL型号 */
    return ADC_ERROR_INVALID_PERIPH;
#else
    /* 检查ADC1和ADC2是否都已初始化 */
    if (!g_adc_initialized[ADC_INSTANCE_1])
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }
    
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    if (ADC_INSTANCE_MAX > 1 && !g_adc_initialized[ADC_INSTANCE_2])
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }
#endif
    
    if (mode >= 10)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    /* 获取双ADC模式值 */
    dual_mode_value = ADC_GetDualModeValue(mode);
    
    /* 读取ADC1当前配置 */
    ADC_InitStructure.ADC_Mode = dual_mode_value;
    ADC_InitStructure.ADC_ScanConvMode = (g_adc_configs[ADC_INSTANCE_1].channel_count > 1) ? ENABLE : DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = g_adc_continuous[ADC_INSTANCE_1] ? ENABLE : DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = g_adc_configs[ADC_INSTANCE_1].channel_count;
    
    /* 重新配置ADC1（双ADC模式在ADC1的CR1寄存器中配置） */
    ADC_Init(ADC1, &ADC_InitStructure);
    
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    /* 读取ADC2当前配置 */
    if (ADC_INSTANCE_MAX > 1)
    {
        ADC_InitStructure.ADC_Mode = dual_mode_value;
        ADC_InitStructure.ADC_ScanConvMode = (g_adc_configs[ADC_INSTANCE_2].channel_count > 1) ? ENABLE : DISABLE;
        ADC_InitStructure.ADC_ContinuousConvMode = g_adc_continuous[ADC_INSTANCE_2] ? ENABLE : DISABLE;
        ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
        ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
        ADC_InitStructure.ADC_NbrOfChannel = g_adc_configs[ADC_INSTANCE_2].channel_count;
        
        /* 重新配置ADC2 */
        ADC_Init(ADC2, &ADC_InitStructure);
    }
#endif
    
    return ADC_OK;
#endif
}

/**
 * @brief 获取当前ADC双ADC模式
 */
ADC_Status_t ADC_GetDualMode(ADC_DualMode_t *mode)
{
    uint32_t dual_mode_reg;
    
    if (mode == NULL)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
#if !defined(STM32F10X_HD) && !defined(STM32F10X_CL) && !defined(STM32F10X_HD_VL)
    /* 双ADC模式仅支持HD/CL/HD_VL型号 */
    *mode = ADC_DUAL_MODE_INDEPENDENT;
    return ADC_ERROR_INVALID_PERIPH;
#else
    if (!g_adc_initialized[ADC_INSTANCE_1])
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }
    
    /* 读取ADC1的CR1寄存器中的DUALMOD位 */
    dual_mode_reg = (ADC1->CR1 & ADC_CR1_DUALMOD) >> 16;
    
    /* 将寄存器值转换为枚举值 */
    switch (dual_mode_reg)
    {
        case 0: *mode = ADC_DUAL_MODE_INDEPENDENT; break;
        case 1: *mode = ADC_DUAL_MODE_REG_INJEC_SIMULT; break;
        case 2: *mode = ADC_DUAL_MODE_REG_SIMULT_ALTER_TRIG; break;
        case 3: *mode = ADC_DUAL_MODE_INJEC_SIMULT_FAST_INTERL; break;
        case 4: *mode = ADC_DUAL_MODE_INJEC_SIMULT_SLOW_INTERL; break;
        case 5: *mode = ADC_DUAL_MODE_INJEC_SIMULT; break;
        case 6: *mode = ADC_DUAL_MODE_REG_SIMULT; break;
        case 7: *mode = ADC_DUAL_MODE_FAST_INTERL; break;
        case 8: *mode = ADC_DUAL_MODE_SLOW_INTERL; break;
        case 9: *mode = ADC_DUAL_MODE_ALTER_TRIG; break;
        default: *mode = ADC_DUAL_MODE_INDEPENDENT; break;
    }
    
    return ADC_OK;
#endif
}

#endif /* CONFIG_MODULE_ADC_ENABLED */

