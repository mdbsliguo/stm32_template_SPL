/**
 * @file adc.c
 * @brief ADC驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的ADC驱动，支持ADC1/2/3，单次/连续转换，多通道扫描，中断模式，DMA模式，注入通道，双ADC模式
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_ADC_ENABLED

/* Include SPL library headers first to avoid function name conflicts */
#include "stm32f10x_rcc.h"
#include "stm32f10x_adc.h"
#include "misc.h"

/* Include our header after SPL headers */
#include "adc.h"

/* 包含board.h以获取ADC_Config_t的完整定义和ADC_CONFIGS宏 */
#include "board.h"

#include "gpio.h"
#include "delay.h"
#include "nvic.h"
#include "dma.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* ========== 配置和状态变量 ========== */

/* 从board.h加载配置 */
static ADC_Config_t g_adc_configs[ADC_INSTANCE_MAX] = ADC_CONFIGS;

/* 初始化标志 */
static bool g_adc_initialized[ADC_INSTANCE_MAX] = {false};

/* 连续转换标志 */
static bool g_adc_continuous[ADC_INSTANCE_MAX] = {false};

/* 中断回调函数数组 */
static ADC_IT_Callback_t g_adc_it_callbacks[ADC_INSTANCE_MAX][3] = {NULL};
static void *g_adc_it_user_data[ADC_INSTANCE_MAX][3] = {NULL};

/* DMA模式相关变量 */
static uint16_t *g_adc_dma_buffer[ADC_INSTANCE_MAX] = {NULL};
static uint16_t g_adc_dma_buffer_size[ADC_INSTANCE_MAX] = {0};
static uint8_t g_adc_dma_channel_count[ADC_INSTANCE_MAX] = {0};

/* ========== 常量定义 ========== */

/* 默认超时时间（毫秒） */
#define ADC_DEFAULT_TIMEOUT_MS  1000

/* DMA通道映射（ADC对应的DMA通道） */
/* 注意：ADC1使用DMA2_CH1（仅HD/CL/HD_VL），ADC2/3不支持DMA */
static const DMA_Channel_t adc_dma_channels[ADC_INSTANCE_MAX] = {
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    DMA_CHANNEL_2_1,  /* ADC1 -> DMA2_CH1 (仅HD/CL/HD_VL) */
#else
    DMA_CHANNEL_MAX,  /* ADC1在MD/LD型号上不支持DMA */
#endif
};

/* ========== 静态辅助函数 ========== */

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
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    if (adc_periph == ADC2)
    {
        return RCC_APB2Periph_ADC2;
    }
    if (adc_periph == ADC3)
    {
        return RCC_APB2Periph_ADC3;
    }
#endif
    return 0;
}

/**
 * @brief 获取GPIO时钟
 * @param[in] port GPIO端口指针
 * @return uint32_t GPIO时钟使能值，失败返回0
 */
static uint32_t ADC_GetGPIOClock(GPIO_TypeDef *port)
{
    if (port == GPIOA)
    {
        return RCC_APB2Periph_GPIOA;
    }
    if (port == GPIOB)
    {
        return RCC_APB2Periph_GPIOB;
    }
    if (port == GPIOC)
    {
        return RCC_APB2Periph_GPIOC;
    }
    if (port == GPIOD)
    {
        return RCC_APB2Periph_GPIOD;
    }
    if (port == GPIOE)
    {
        return RCC_APB2Periph_GPIOE;
    }
    if (port == GPIOF)
    {
        return RCC_APB2Periph_GPIOF;
    }
    if (port == GPIOG)
    {
        return RCC_APB2Periph_GPIOG;
    }
    return 0;
}

/**
 * @brief ADC通道到GPIO映射结构体
 */
typedef struct
{
    uint8_t channel;        /**< ADC通道 */
    GPIO_TypeDef *port;     /**< GPIO端口 */
    uint16_t pin;           /**< GPIO引脚 */
} ADC_ChannelGPIO_Map_t;

/**
 * @brief ADC通道到GPIO映射表（STM32F103）
 * @note 通道16和17是内部通道，不需要GPIO配置
 */
static const ADC_ChannelGPIO_Map_t adc_channel_gpio_map[] =
{
    {ADC_Channel_0, GPIOA, GPIO_Pin_0},   /* PA0 */
    {ADC_Channel_1, GPIOA, GPIO_Pin_1},   /* PA1 */
    {ADC_Channel_2, GPIOA, GPIO_Pin_2},   /* PA2 */
    {ADC_Channel_3, GPIOA, GPIO_Pin_3},   /* PA3 */
    {ADC_Channel_4, GPIOA, GPIO_Pin_4},   /* PA4 */
    {ADC_Channel_5, GPIOA, GPIO_Pin_5},   /* PA5 */
    {ADC_Channel_6, GPIOA, GPIO_Pin_6},   /* PA6 */
    {ADC_Channel_7, GPIOA, GPIO_Pin_7},   /* PA7 */
    {ADC_Channel_8, GPIOB, GPIO_Pin_0},   /* PB0 */
    {ADC_Channel_9, GPIOB, GPIO_Pin_1},   /* PB1 */
    {ADC_Channel_10, GPIOC, GPIO_Pin_0},  /* PC0 */
    {ADC_Channel_11, GPIOC, GPIO_Pin_1},  /* PC1 */
    {ADC_Channel_12, GPIOC, GPIO_Pin_2},  /* PC2 */
    {ADC_Channel_13, GPIOC, GPIO_Pin_3},  /* PC3 */
    {ADC_Channel_14, GPIOC, GPIO_Pin_4},  /* PC4 */
    {ADC_Channel_15, GPIOC, GPIO_Pin_5},  /* PC5 */
    /* ADC_Channel_16: 内部温度传感器，不需要GPIO */
    /* ADC_Channel_17: 内部参考电压，不需要GPIO */
};

/**
 * @brief 获取ADC通道对应的GPIO配置
 * @param[in] channel ADC通道（ADC_Channel_0 ~ ADC_Channel_17）
 * @param[out] port GPIO端口指针（输出参数）
 * @param[out] pin GPIO引脚号（输出参数）
 * @return ADC_Status_t 错误码
 * @note 通道16和17是内部通道，不需要GPIO配置，返回port=NULL, pin=0
 */
static ADC_Status_t ADC_GetChannelGPIO(uint8_t channel, GPIO_TypeDef **port, uint16_t *pin)
{
    /* ========== 参数校验 ========== */
    if (port == NULL || pin == NULL)
    {
        return ADC_ERROR_NULL_PTR;
    }
    
    /* 通道16和17是内部通道，不需要GPIO */
    if (channel == ADC_Channel_16 || channel == ADC_Channel_17)
    {
        *port = NULL;
        *pin = 0;
        return ADC_OK;
    }
    
    /* 查找映射表 */
    for (uint8_t i = 0; i < sizeof(adc_channel_gpio_map) / sizeof(adc_channel_gpio_map[0]); i++)
    {
        if (adc_channel_gpio_map[i].channel == channel)
        {
            *port = adc_channel_gpio_map[i].port;
            *pin = adc_channel_gpio_map[i].pin;
            return ADC_OK;
        }
    }
    
    return ADC_ERROR_INVALID_CHANNEL;
}

/**
 * @brief 等待ADC标志位
 * @param[in] adc_periph ADC外设指针
 * @param[in] flag 标志位
 * @param[in] timeout_ms 超时时间（毫秒），0表示使用默认超时
 * @return ADC_Status_t 错误码
 */
static ADC_Status_t ADC_WaitFlag(ADC_TypeDef *adc_periph, uint32_t flag, uint32_t timeout_ms)
{
    /* ========== 参数校验 ========== */
    if (adc_periph == NULL)
    {
        return ADC_ERROR_NULL_PTR;
    }
    
    /* 使用默认超时时间 */
    if (timeout_ms == 0)
    {
        timeout_ms = ADC_DEFAULT_TIMEOUT_MS;
    }
    
#if CONFIG_MODULE_DELAY_ENABLED
    /* 使用delay模块实现超时（更准确） */
    uint32_t start_tick = Delay_GetTick();
    
    /* 等待标志位 */
    while (ADC_GetFlagStatus(adc_periph, flag) == RESET)
    {
        /* 检查超时（使用Delay_GetElapsed处理溢出） */
        uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
        if (elapsed > timeout_ms)
        {
            return ADC_ERROR_TIMEOUT;
        }
    }
#else
    /* 如果没有delay模块，使用简单的循环计数 */
    uint32_t counter = 0;
    uint32_t timeout_count = timeout_ms * 72000;  /* 72MHz系统时钟，粗略估算 */
    
    while (ADC_GetFlagStatus(adc_periph, flag) == RESET)
    {
        if (++counter > timeout_count)
        {
            return ADC_ERROR_TIMEOUT;
        }
    }
#endif
    
    return ADC_OK;
}

/* ========== 公共函数实现 ========== */

/**
 * @brief ADC模块初始化
 * @param[in] instance ADC实例索引（ADC_INSTANCE_1/2/3）
 * @return ADC_Status_t 错误码
 * @note 根据board.h中的配置初始化ADC外设和GPIO引脚
 * @note 函数名已重命名为ADC_ModuleInit，避免与SPL库的ADC_Init函数名冲突
 */
ADC_Status_t ADC_ModuleInit(ADC_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (g_adc_initialized[instance])
    {
        return ADC_OK;  /* 已初始化，直接返回成功 */
    }
    
    /* 获取配置 */
    ADC_Config_t *cfg = &g_adc_configs[instance];
    if (cfg->adc_periph == NULL)
    {
        return ADC_ERROR_INVALID_PERIPH;
    }
    if (!cfg->enabled)
    {
        return ADC_OK;  /* 未启用，直接返回成功 */
    }
    
    ADC_TypeDef *adc_periph = cfg->adc_periph;
    
    /* ========== 1. 使能ADC和GPIO时钟 ========== */
    uint32_t adc_clock = ADC_GetPeriphClock(adc_periph);
    if (adc_clock == 0)
    {
        return ADC_ERROR_INVALID_PERIPH;
    }
    RCC_APB2PeriphClockCmd(adc_clock, ENABLE);
    
    /* 配置所有通道的GPIO为模拟输入 */
    for (uint8_t i = 0; i < cfg->channel_count; i++)
    {
        uint8_t channel = cfg->channels[i];
        GPIO_TypeDef *port;
        uint16_t pin;
        
        ADC_Status_t status = ADC_GetChannelGPIO(channel, &port, &pin);
        if (status != ADC_OK)
        {
            /* 内部通道（16、17）不需要GPIO配置 */
            if (channel == ADC_Channel_16 || channel == ADC_Channel_17)
            {
                continue;
            }
            return status;
        }
        
        if (port != NULL && pin != 0)
        {
            /* 使能GPIO时钟 */
            uint32_t gpio_clock = ADC_GetGPIOClock(port);
            if (gpio_clock != 0)
            {
                RCC_APB2PeriphClockCmd(gpio_clock, ENABLE);
            }
            
            /* 配置GPIO为模拟输入模式 */
            GPIO_Status_t gpio_status = GPIO_Config(port, pin, GPIO_MODE_INPUT_ANALOG, GPIO_SPEED_2MHz);
            if (gpio_status != GPIO_OK)
            {
                return ADC_ERROR_GPIO_FAILED;
            }
        }
    }
    
    /* ========== 2. 复位ADC ========== */
    ADC_DeInit(adc_periph);
    
    /* ========== 3. 配置ADC参数 ========== */
    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;  /* 独立模式 */
    ADC_InitStructure.ADC_ScanConvMode = (cfg->channel_count > 1) ? ENABLE : DISABLE;  /* 多通道扫描模式 */
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;  /* 单次转换模式 */
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;  /* 软件触发 */
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;  /* 右对齐 */
    ADC_InitStructure.ADC_NbrOfChannel = cfg->channel_count;  /* 通道数量 */
    /* 调用SPL库的ADC_Init函数 */
    ADC_Init(adc_periph, &ADC_InitStructure);
    
    /* ========== 4. 配置ADC通道和采样时间 ========== */
    for (uint8_t i = 0; i < cfg->channel_count; i++)
    {
        uint8_t channel = cfg->channels[i];
        uint8_t rank = i + 1;  /* 规则通道序号从1开始 */
        
        /* 配置规则通道 */
        ADC_RegularChannelConfig(adc_periph, channel, rank, cfg->sample_time);
    }
    
    /* ========== 5. 使能ADC ========== */
    ADC_Cmd(adc_periph, ENABLE);
    
    /* ========== 6. ADC校准 ========== */
    /* 复位校准 */
    ADC_ResetCalibration(adc_periph);
    while (ADC_GetResetCalibrationStatus(adc_periph));
    
    /* 开始校准 */
    ADC_StartCalibration(adc_periph);
    while (ADC_GetCalibrationStatus(adc_periph));
    
    /* ========== 7. 标记为已初始化 ========== */
    g_adc_initialized[instance] = true;
    
    return ADC_OK;
}

/**
 * @brief ADC反初始化
 * @param[in] instance ADC实例索引
 * @return ADC_Status_t 错误码
 */
ADC_Status_t ADC_Deinit(ADC_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_INSTANCE;
    }
    
    if (!g_adc_initialized[instance])
    {
        return ADC_OK;  /* 未初始化，直接返回成功 */
    }
    
    /* 获取配置 */
    ADC_Config_t *cfg = &g_adc_configs[instance];
    if (cfg->adc_periph == NULL)
    {
        return ADC_ERROR_INVALID_PERIPH;
    }
    
    ADC_TypeDef *adc_periph = cfg->adc_periph;
    
    /* 禁用ADC */
    ADC_Cmd(adc_periph, DISABLE);
    
    /* 停止连续转换 */
    if (g_adc_continuous[instance])
    {
        ADC_StopContinuous(instance);
    }
    
    /* 标记为未初始化 */
    g_adc_initialized[instance] = false;
    
    return ADC_OK;
}

/**
 * @brief ADC单次转换（阻塞式）
 * @param[in] instance ADC实例索引
 * @param[in] channel ADC通道（ADC_Channel_0 ~ ADC_Channel_17）
 * @param[out] value 转换结果（12位，0-4095）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return ADC_Status_t 错误码
 */
ADC_Status_t ADC_ReadChannel(ADC_Instance_t instance, uint8_t channel, uint16_t *value, uint32_t timeout)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_INSTANCE;
    }
    if (channel > 17)  /* ADC通道范围：0-17 */
    {
        return ADC_ERROR_INVALID_CHANNEL;
    }
    if (value == NULL)
    {
        return ADC_ERROR_NULL_PTR;
    }
    
    /* 检查是否已初始化 */
    if (!g_adc_initialized[instance])
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取ADC外设 */
    ADC_TypeDef *adc_periph = ADC_GetPeriph(instance);
    if (adc_periph == NULL)
    {
        return ADC_ERROR_INVALID_PERIPH;
    }
    
    /* 如果正在连续转换，先停止 */
    if (g_adc_continuous[instance])
    {
        ADC_StopContinuous(instance);
    }
    
    /* ========== 配置单通道转换 ========== */
    /* 配置规则通道1（单通道转换） */
    ADC_RegularChannelConfig(adc_periph, channel, 1, ADC_SampleTime_55Cycles5);
    
    /* ========== 启动转换 ========== */
    ADC_SoftwareStartConvCmd(adc_periph, ENABLE);
    
    /* ========== 等待转换完成 ========== */
    ADC_Status_t status = ADC_WaitFlag(adc_periph, ADC_FLAG_EOC, timeout);
    if (status != ADC_OK)
    {
        ADC_SoftwareStartConvCmd(adc_periph, DISABLE);
        return status;
    }
    
    /* ========== 读取转换结果 ========== */
    *value = ADC_GetConversionValue(adc_periph);
    
    /* 清除EOC标志 */
    ADC_ClearFlag(adc_periph, ADC_FLAG_EOC);
    
    /* 停止转换 */
    ADC_SoftwareStartConvCmd(adc_periph, DISABLE);
    
    return ADC_OK;
}

/**
 * @brief ADC设置通道采样时间
 * @param[in] instance ADC实例索引
 * @param[in] channel ADC通道（ADC_Channel_0 ~ ADC_Channel_17）
 * @param[in] sample_time 采样时间（ADC_SampleTime_1Cycles5 ~ ADC_SampleTime_239Cycles5）
 * @return ADC_Status_t 错误码
 * @note 采样时间越长，转换精度越高，但转换速度越慢
 * @note 使用规则通道1，因为单通道转换时使用通道1
 */
ADC_Status_t ADC_SetChannelSampleTime(ADC_Instance_t instance, uint8_t channel, uint8_t sample_time)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_INSTANCE;
    }
    if (channel > 17)  /* ADC通道范围：0-17 */
    {
        return ADC_ERROR_INVALID_CHANNEL;
    }
    
    /* 检查是否已初始化 */
    if (!g_adc_initialized[instance])
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取ADC外设 */
    ADC_TypeDef *adc_periph = ADC_GetPeriph(instance);
    if (adc_periph == NULL)
    {
        return ADC_ERROR_INVALID_PERIPH;
    }
    
    /* 设置采样时间（使用规则通道1，因为单通道转换时使用通道1） */
    ADC_RegularChannelConfig(adc_periph, channel, 1, sample_time);
    
    return ADC_OK;
}

/**
 * @brief ADC多通道扫描转换（阻塞式）
 * @param[in] instance ADC实例索引
 * @param[in] channels 通道数组（ADC_Channel_0 ~ ADC_Channel_17）
 * @param[in] channel_count 通道数量（1-16）
 * @param[out] values 转换结果数组（12位，0-4095）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_ReadChannels(ADC_Instance_t instance, const uint8_t *channels, uint8_t channel_count,
                              uint16_t *values, uint32_t timeout)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_INSTANCE;
    }
    if (channels == NULL)
    {
        return ADC_ERROR_NULL_PTR;
    }
    if (values == NULL)
    {
        return ADC_ERROR_NULL_PTR;
    }
    if (channel_count == 0 || channel_count > 16)  /* 最多16个通道 */
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    /* ========== 占位空函数 ========== */
    (void)timeout;
    (void)channels;
    (void)values;
    (void)channel_count;
    #warning "ADC函数: 占位空函数，功能未实现，待完善"
    
    return ADC_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 启动ADC连续转换
 * @param[in] instance ADC实例索引
 * @param[in] channel ADC通道（ADC_Channel_0 ~ ADC_Channel_17）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_StartContinuous(ADC_Instance_t instance, uint8_t channel)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_INSTANCE;
    }
    if (channel > 17)  /* ADC通道范围：0-17 */
    {
        return ADC_ERROR_INVALID_CHANNEL;
    }
    
    /* ========== 占位空函数 ========== */
    (void)channel;
    #warning "ADC函数: 占位空函数，功能未实现，待完善"
    
    return ADC_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 停止ADC连续转换
 * @param[in] instance ADC实例索引
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_StopContinuous(ADC_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_INSTANCE;
    }
    
    /* ========== 占位空函数 ========== */
    #warning "ADC函数: 占位空函数，功能未实现，待完善"
    
    return ADC_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 读取ADC连续转换结果
 * @param[in] instance ADC实例索引
 * @param[out] value 转换结果（12位，0-4095）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_ReadContinuous(ADC_Instance_t instance, uint16_t *value)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_INSTANCE;
    }
    if (value == NULL)
    {
        return ADC_ERROR_NULL_PTR;
    }
    
    /* ========== 占位空函数 ========== */
    (void)value;
    #warning "ADC函数: 占位空函数，功能未实现，待完善"
    
    return ADC_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 检查ADC是否已初始化
 * @param[in] instance ADC实例索引
 * @return uint8_t 1-已初始化，0-未初始化
 */
uint8_t ADC_IsInitialized(ADC_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return 0;  /* 无效实例返回0（未初始化） */
    }
    
    return g_adc_initialized[instance] ? 1 : 0;
}

/**
 * @brief 获取ADC外设指针
 * @param[in] instance ADC实例索引
 * @return ADC_TypeDef* ADC外设指针，失败返回NULL
 */
ADC_TypeDef* ADC_GetPeriph(ADC_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return NULL;  /* 无效实例返回NULL */
    }
    
    /* 获取配置 */
    ADC_Config_t *cfg = &g_adc_configs[instance];
    return cfg->adc_periph;
}

/* ========== 中断模式功能实现 ========== */

/**
 * @brief 获取ADC中断类型对应的SPL库中断值
 * @param[in] it_type ADC中断类型
 * @return uint16_t SPL库中断值，失败返回0
 */
static uint16_t ADC_GetITValue(ADC_IT_t it_type)
{
    switch (it_type)
    {
        case ADC_IT_TYPE_EOC:  return ADC_IT_EOC;
        case ADC_IT_TYPE_JEOC: return ADC_IT_JEOC;
        case ADC_IT_TYPE_AWD:  return ADC_IT_AWD;
        default: return 0;
    }
}

/**
 * @brief 使能ADC中断
 * @param[in] instance ADC实例索引
 * @param[in] it_type 中断类型（ADC_IT_TYPE_EOC/JEOC/AWD）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_EnableIT(ADC_Instance_t instance, ADC_IT_t it_type)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_INSTANCE;
    }
    if (it_type > ADC_IT_TYPE_AWD)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    /* ========== 占位空函数 ========== */
    (void)it_type;
    #warning "ADC函数: 占位空函数，功能未实现，待完善"
    
    return ADC_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 禁用ADC中断
 * @param[in] instance ADC实例索引
 * @param[in] it_type 中断类型（ADC_IT_TYPE_EOC/JEOC/AWD）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_DisableIT(ADC_Instance_t instance, ADC_IT_t it_type)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_INSTANCE;
    }
    if (it_type > ADC_IT_TYPE_AWD)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    /* ========== 占位空函数 ========== */
    (void)it_type;
    #warning "ADC函数: 占位空函数，功能未实现，待完善"
    
    return ADC_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 设置ADC中断回调函数
 * @param[in] instance ADC实例索引
 * @param[in] it_type 中断类型（ADC_IT_TYPE_EOC/JEOC/AWD）
 * @param[in] callback 回调函数指针，NULL表示禁用回调
 * @param[in] user_data 用户数据指针
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_SetITCallback(ADC_Instance_t instance, ADC_IT_t it_type,
                                ADC_IT_Callback_t callback, void *user_data)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_INSTANCE;
    }
    if (it_type > ADC_IT_TYPE_AWD)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    /* 注意：callback可以为NULL（表示禁用回调），user_data可以为NULL */
    
    /* ========== 占位空函数 ========== */
    (void)callback;
    (void)user_data;
    #warning "ADC函数: 占位空函数，功能未实现，待完善"
    
    return ADC_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief ADC中断服务函数（应在中断服务程序中调用）
 * @param[in] instance ADC实例索引
 * @note 功能未实现，当前为占位函数
 */
void ADC_IRQHandler(ADC_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return;  /* 无效实例直接返回 */
    }
    
    /* ========== 占位空函数 ========== */
    /* 功能未实现 */
}

/**
 * @brief ADC中断服务程序入口（ADC1和ADC2共享）
 * @note 功能未实现，当前为占位函数
 */
void ADC1_2_IRQHandler(void)
{
    /* 功能未实现 */
}

/* ========== DMA模式功能实现 ========== */

/**
 * @brief ADC DMA启动（多通道扫描）
 * @param[in] instance ADC实例索引
 * @param[in] channels 通道数组（ADC_Channel_0 ~ ADC_Channel_17）
 * @param[in] channel_count 通道数量（1-16）
 * @param[out] buffer 转换结果缓冲区（12位，0-4095）
 * @param[in] buffer_size 缓冲区大小（必须是通道数量的倍数）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 * @note 仅ADC1支持DMA（HD/CL/HD_VL型号使用DMA2_CH1，MD/LD型号不支持DMA）
 */
ADC_Status_t ADC_StartDMA(ADC_Instance_t instance, const uint8_t *channels, uint8_t channel_count,
                          uint16_t *buffer, uint16_t buffer_size)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_INSTANCE;
    }
    if (channels == NULL)
    {
        return ADC_ERROR_NULL_PTR;
    }
    if (buffer == NULL)
    {
        return ADC_ERROR_NULL_PTR;
    }
    if (channel_count == 0 || channel_count > 16)  /* 最多16个通道 */
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    if (buffer_size < channel_count)
    {
        return ADC_ERROR_INVALID_PARAM;  /* 缓冲区大小必须至少等于通道数量 */
    }
    
    /* ========== 占位空函数 ========== */
    (void)channels;
    (void)buffer;
    (void)channel_count;
    (void)buffer_size;
    #warning "ADC函数: 占位空函数，功能未实现，待完善"
    
    return ADC_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief ADC DMA停止
 * @param[in] instance ADC实例索引
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_StopDMA(ADC_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_INSTANCE;
    }
    
    /* ========== 占位空函数 ========== */
    #warning "ADC函数: 占位空函数，功能未实现，待完善"
    
    return ADC_ERROR_NOT_IMPLEMENTED;
}

/* ========== 注入通道功能实现 ========== */

/**
 * @brief ADC注入通道配置
 * @param[in] instance ADC实例索引
 * @param[in] channel ADC通道（ADC_Channel_0 ~ ADC_Channel_17）
 * @param[in] rank 注入通道序号（1-4，对应ADC_InjectedChannel_1~4）
 * @param[in] sample_time 采样时间（ADC_SampleTime_1Cycles5 ~ ADC_SampleTime_239Cycles5）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_ConfigInjectedChannel(ADC_Instance_t instance, uint8_t channel, uint8_t rank, uint8_t sample_time)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_INSTANCE;
    }
    if (channel > 17)  /* ADC通道范围：0-17 */
    {
        return ADC_ERROR_INVALID_CHANNEL;
    }
    if (rank < 1 || rank > 4)  /* 注入通道序号：1-4 */
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    /* 注意：sample_time范围检查需要在实现时根据SPL库定义进行 */
    
    /* ========== 占位空函数 ========== */
    (void)channel;
    (void)rank;
    (void)sample_time;
    #warning "ADC函数: 占位空函数，功能未实现，待完善"
    
    return ADC_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief ADC启动注入转换
 * @param[in] instance ADC实例索引
 * @param[in] external_trigger 外部触发使能（1=使能，0=禁用）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_StartInjectedConversion(ADC_Instance_t instance, uint8_t external_trigger)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_INSTANCE;
    }
    /* 注意：external_trigger为0或1，uint8_t已限制范围，无需额外检查 */
    
    /* ========== 占位空函数 ========== */
    (void)external_trigger;
    #warning "ADC函数: 占位空函数，功能未实现，待完善"
    
    return ADC_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief ADC读取注入转换结果
 * @param[in] instance ADC实例索引
 * @param[in] rank 注入通道序号（1-4）
 * @param[out] value 转换结果（12位，0-4095）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_ReadInjectedChannel(ADC_Instance_t instance, uint8_t rank, uint16_t *value)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_INSTANCE;
    }
    if (rank < 1 || rank > 4)  /* 注入通道序号：1-4 */
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    if (value == NULL)
    {
        return ADC_ERROR_NULL_PTR;
    }
    
    /* ========== 占位空函数 ========== */
    (void)rank;
    (void)value;
    #warning "ADC函数: 占位空函数，功能未实现，待完善"
    
    return ADC_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief ADC设置注入通道偏移量（占位空函数，与SPL库函数名冲突，暂时不实现）
 * @note 此函数与SPL库的ADC_SetInjectedOffset函数名冲突，暂时注释实现
 * @note 如需实现，需要重命名函数以避免与SPL库函数名冲突
 */
#if 0
ADC_Status_t ADC_SetInjectedOffset(ADC_Instance_t instance, uint8_t rank, uint16_t offset)
{
    /* ========== 参数校验 ========== */
    if (instance >= ADC_INSTANCE_MAX)
    {
        return ADC_ERROR_INVALID_INSTANCE;
    }
    if (rank < 1 || rank > 4)  /* 注入通道序号：1-4 */
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    /* 注意：offset范围0-4095，但uint16_t已经限制了范围，无需额外检查 */
    
    /* ========== 占位空函数 ========== */
    (void)offset;
    #warning "ADC函数: 占位空函数，功能未实现，待完善"
    
    return ADC_ERROR_NOT_IMPLEMENTED;
}
#endif

/* ========== 双ADC模式功能实现 ========== */

/**
 * @brief 获取双ADC模式对应的SPL库模式值
 * @param[in] mode ADC双ADC模式
 * @return uint32_t SPL库模式值，失败返回0
 * @note 功能未实现，当前为占位函数
 */
static uint32_t ADC_GetDualModeValue(ADC_DualMode_t mode)
{
    (void)mode;
    return 0;
}

/**
 * @brief 配置ADC双ADC模式
 * @param[in] mode 双ADC模式（ADC_DUAL_MODE_INDEPENDENT等）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 * @note 需要ADC1和ADC2都已初始化（仅HD/CL/HD_VL型号支持）
 * @note 双ADC模式下，ADC1为主ADC，ADC2为从ADC
 */
ADC_Status_t ADC_ConfigDualMode(ADC_DualMode_t mode)
{
    /* ========== 参数校验 ========== */
    if (mode > ADC_DUAL_MODE_ALTER_TRIG)
    {
        return ADC_ERROR_INVALID_PARAM;
    }
    
    /* ========== 占位空函数 ========== */
    (void)mode;
    #warning "ADC函数: 占位空函数，功能未实现，待完善"
    
    return ADC_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 获取当前ADC双ADC模式
 * @param[out] mode 双ADC模式（输出参数）
 * @return ADC_Status_t 错误码
 * @note 功能未实现，当前为占位函数，返回ADC_ERROR_NOT_IMPLEMENTED
 */
ADC_Status_t ADC_GetDualMode(ADC_DualMode_t *mode)
{
    /* ========== 参数校验 ========== */
    if (mode == NULL)
    {
        return ADC_ERROR_NULL_PTR;
    }
    
    /* ========== 占位空函数 ========== */
    (void)mode;
    #warning "ADC函数: 占位空函数，功能未实现，待完善"
    
    return ADC_ERROR_NOT_IMPLEMENTED;
}

#endif /* CONFIG_MODULE_ADC_ENABLED */

