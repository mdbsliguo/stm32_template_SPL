/**
 * @file dac.c
 * @brief DAC驱动模块实现
 */

#include "dac.h"
#include "board.h"
#include "config.h"
#include "error_handler.h"
#include "gpio.h"
#include "dma.h"
#include "stm32f10x_dac.h"
#include "stm32f10x_rcc.h"
#include <stdbool.h>
#include <math.h>

#if CONFIG_MODULE_DAC_ENABLED

#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL) || defined(STM32F10X_MD_VL)

/* DAC通道到GPIO的映射 */
static const struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} dac_channel_gpio[] = {
    {GPIOA, GPIO_Pin_4},  /* DAC_CHANNEL_1 -> PA4 */
    {GPIOA, GPIO_Pin_5},  /* DAC_CHANNEL_2 -> PA5 */
};

/* DAC通道到SPL库通道值的映射 */
static const uint32_t dac_channel_value[] = {
    DAC_Channel_1,
    DAC_Channel_2,
};

/* 初始化标志 */
static bool g_dac_initialized[DAC_CHANNEL_MAX] = {false};

/* DMA通道映射（DAC对应的DMA通道） */
/* DAC1使用DMA1_CH3，DAC2使用DMA1_CH4 */
static const DMA_Channel_t dac_dma_channels[DAC_CHANNEL_MAX] = {
    DMA_CHANNEL_1_3,  /* DAC1 -> DMA1_CH3 */
    DMA_CHANNEL_1_4,  /* DAC2 -> DMA1_CH4 */
};

/* 默认参考电压（V） */
#define DAC_VREF_DEFAULT  3.3f
/* DAC分辨率（位） */
#define DAC_RESOLUTION    12
/* DAC最大值 */
#define DAC_MAX_VALUE     ((1 << DAC_RESOLUTION) - 1)

/**
 * @brief 获取DAC触发模式对应的SPL库值
 */
static uint32_t DAC_GetTriggerValue(DAC_Trigger_t trigger)
{
    switch (trigger)
    {
        case DAC_TRIGGER_NONE: return DAC_Trigger_None;
        case DAC_TRIGGER_SOFTWARE: return DAC_Trigger_Software;
        case DAC_TRIGGER_TIM6: return DAC_Trigger_T6_TRGO;
        case DAC_TRIGGER_TIM7: return DAC_Trigger_T7_TRGO;
        case DAC_TRIGGER_TIM2: return DAC_Trigger_T2_TRGO;
        case DAC_TRIGGER_TIM4: return DAC_Trigger_T4_TRGO;
        case DAC_TRIGGER_EXTI9: return DAC_Trigger_Ext_IT9;
        default: return DAC_Trigger_None;
    }
}

/**
 * @brief DAC初始化
 */
DAC_Status_t DAC_Init(DAC_Channel_t channel, DAC_Trigger_t trigger, DAC_OutputBuffer_t output_buffer)
{
    DAC_InitTypeDef DAC_InitStructure;
    
    /* 参数校验 */
    if (channel >= DAC_CHANNEL_MAX)
    {
        ERROR_HANDLER_Report(ERROR_BASE_DAC, __FILE__, __LINE__, "Invalid DAC channel");
        return DAC_ERROR_INVALID_CHANNEL;
    }
    
    if (g_dac_initialized[channel])
    {
        ERROR_HANDLER_Report(ERROR_BASE_DAC, __FILE__, __LINE__, "DAC already initialized");
        return DAC_ERROR_ALREADY_INITIALIZED;
    }
    
    /* 使能DAC和GPIO时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);
    GPIO_EnableClock(dac_channel_gpio[channel].port);
    
    /* 配置GPIO为模拟输入模式（DAC输出需要） */
    GPIO_Config(dac_channel_gpio[channel].port, dac_channel_gpio[channel].pin, 
                GPIO_MODE_INPUT_ANALOG, GPIO_SPEED_50MHz);
    
    /* 配置DAC */
    DAC_InitStructure.DAC_Trigger = DAC_GetTriggerValue(trigger);
    DAC_InitStructure.DAC_WaveGeneration = DAC_WaveGeneration_None;
    DAC_InitStructure.DAC_LFSRUnmask_TriangleAmplitude = DAC_LFSRUnmask_Bit0;
    DAC_InitStructure.DAC_OutputBuffer = (output_buffer == DAC_OUTPUT_BUFFER_ENABLE) ? 
                                         DAC_OutputBuffer_Enable : DAC_OutputBuffer_Disable;
    DAC_Init(dac_channel_value[channel], &DAC_InitStructure);
    
    /* 标记为已初始化 */
    g_dac_initialized[channel] = true;
    
    return DAC_OK;
}

/**
 * @brief DAC反初始化
 */
DAC_Status_t DAC_Deinit(DAC_Channel_t channel)
{
    if (channel >= DAC_CHANNEL_MAX)
    {
        return DAC_ERROR_INVALID_CHANNEL;
    }
    
    if (!g_dac_initialized[channel])
    {
        return DAC_ERROR_NOT_INITIALIZED;
    }
    
    /* 禁用DAC通道 */
    DAC_Cmd(dac_channel_value[channel], DISABLE);
    
    /* 标记为未初始化 */
    g_dac_initialized[channel] = false;
    
    return DAC_OK;
}

/**
 * @brief 设置DAC输出电压值
 */
DAC_Status_t DAC_SetValue(DAC_Channel_t channel, uint16_t value)
{
    if (channel >= DAC_CHANNEL_MAX)
    {
        return DAC_ERROR_INVALID_CHANNEL;
    }
    
    if (!g_dac_initialized[channel])
    {
        return DAC_ERROR_NOT_INITIALIZED;
    }
    
    if (value > DAC_MAX_VALUE)
    {
        return DAC_ERROR_INVALID_PARAM;
    }
    
    /* 设置DAC数据（12位右对齐） */
    if (channel == DAC_CHANNEL_1)
    {
        DAC_SetChannel1Data(DAC_Align_12b_R, value);
    }
    else
    {
        DAC_SetChannel2Data(DAC_Align_12b_R, value);
    }
    
    return DAC_OK;
}

/**
 * @brief 设置DAC输出电压
 */
DAC_Status_t DAC_SetVoltage(DAC_Channel_t channel, float voltage)
{
    uint16_t value;
    
    if (channel >= DAC_CHANNEL_MAX)
    {
        return DAC_ERROR_INVALID_CHANNEL;
    }
    
    if (!g_dac_initialized[channel])
    {
        return DAC_ERROR_NOT_INITIALIZED;
    }
    
    if (voltage < 0.0f || voltage > DAC_VREF_DEFAULT)
    {
        return DAC_ERROR_INVALID_PARAM;
    }
    
    /* 将电压转换为DAC值 */
    value = (uint16_t)((voltage / DAC_VREF_DEFAULT) * DAC_MAX_VALUE);
    
    return DAC_SetValue(channel, value);
}

/**
 * @brief 使能DAC通道
 */
DAC_Status_t DAC_Enable(DAC_Channel_t channel)
{
    if (channel >= DAC_CHANNEL_MAX)
    {
        return DAC_ERROR_INVALID_CHANNEL;
    }
    
    if (!g_dac_initialized[channel])
    {
        return DAC_ERROR_NOT_INITIALIZED;
    }
    
    DAC_Cmd(dac_channel_value[channel], ENABLE);
    
    return DAC_OK;
}

/**
 * @brief 禁用DAC通道
 */
DAC_Status_t DAC_Disable(DAC_Channel_t channel)
{
    if (channel >= DAC_CHANNEL_MAX)
    {
        return DAC_ERROR_INVALID_CHANNEL;
    }
    
    if (!g_dac_initialized[channel])
    {
        return DAC_ERROR_NOT_INITIALIZED;
    }
    
    DAC_Cmd(dac_channel_value[channel], DISABLE);
    
    return DAC_OK;
}

/**
 * @brief 配置波形生成
 */
DAC_Status_t DAC_ConfigWave(DAC_Channel_t channel, DAC_Wave_t wave, uint16_t amplitude)
{
    uint32_t wave_value, amplitude_value;
    
    if (channel >= DAC_CHANNEL_MAX)
    {
        return DAC_ERROR_INVALID_CHANNEL;
    }
    
    if (!g_dac_initialized[channel])
    {
        return DAC_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取波形类型 */
    switch (wave)
    {
        case DAC_WAVE_NONE:
            wave_value = DAC_WaveGeneration_None;
            amplitude_value = DAC_LFSRUnmask_Bit0;
            break;
        case DAC_WAVE_NOISE:
            wave_value = DAC_WaveGeneration_Noise;
            /* 噪声波形的幅度由LFSR掩码决定 */
            if (amplitude <= 1) amplitude_value = DAC_LFSRUnmask_Bit0;
            else if (amplitude <= 3) amplitude_value = DAC_LFSRUnmask_Bits1_0;
            else if (amplitude <= 7) amplitude_value = DAC_LFSRUnmask_Bits2_0;
            else if (amplitude <= 15) amplitude_value = DAC_LFSRUnmask_Bits3_0;
            else if (amplitude <= 31) amplitude_value = DAC_LFSRUnmask_Bits4_0;
            else if (amplitude <= 63) amplitude_value = DAC_LFSRUnmask_Bits5_0;
            else if (amplitude <= 127) amplitude_value = DAC_LFSRUnmask_Bits6_0;
            else if (amplitude <= 255) amplitude_value = DAC_LFSRUnmask_Bits7_0;
            else if (amplitude <= 511) amplitude_value = DAC_LFSRUnmask_Bits8_0;
            else if (amplitude <= 1023) amplitude_value = DAC_LFSRUnmask_Bits9_0;
            else if (amplitude <= 2047) amplitude_value = DAC_LFSRUnmask_Bits10_0;
            else amplitude_value = DAC_LFSRUnmask_Bits11_0;
            break;
        case DAC_WAVE_TRIANGLE:
            wave_value = DAC_WaveGeneration_Triangle;
            /* 三角波的幅度 */
            if (amplitude <= 1) amplitude_value = DAC_TriangleAmplitude_1;
            else if (amplitude <= 3) amplitude_value = DAC_TriangleAmplitude_3;
            else if (amplitude <= 7) amplitude_value = DAC_TriangleAmplitude_7;
            else if (amplitude <= 15) amplitude_value = DAC_TriangleAmplitude_15;
            else if (amplitude <= 31) amplitude_value = DAC_TriangleAmplitude_31;
            else if (amplitude <= 63) amplitude_value = DAC_TriangleAmplitude_63;
            else if (amplitude <= 127) amplitude_value = DAC_TriangleAmplitude_127;
            else if (amplitude <= 255) amplitude_value = DAC_TriangleAmplitude_255;
            else if (amplitude <= 511) amplitude_value = DAC_TriangleAmplitude_511;
            else if (amplitude <= 1023) amplitude_value = DAC_TriangleAmplitude_1023;
            else if (amplitude <= 2047) amplitude_value = DAC_TriangleAmplitude_2047;
            else amplitude_value = DAC_TriangleAmplitude_4095;
            break;
        default:
            return DAC_ERROR_INVALID_PARAM;
    }
    
    /* 配置波形生成 */
    DAC_WaveGenerationCmd(dac_channel_value[channel], wave_value, ENABLE);
    
    /* 更新幅度（需要重新初始化） */
    DAC_InitTypeDef DAC_InitStructure;
    DAC_StructInit(&DAC_InitStructure);
    DAC_InitStructure.DAC_Trigger = DAC_Trigger_None;
    DAC_InitStructure.DAC_WaveGeneration = wave_value;
    DAC_InitStructure.DAC_LFSRUnmask_TriangleAmplitude = amplitude_value;
    DAC_InitStructure.DAC_OutputBuffer = DAC_OutputBuffer_Enable;
    DAC_Init(dac_channel_value[channel], &DAC_InitStructure);
    
    return DAC_OK;
}

/**
 * @brief 禁用波形生成
 */
DAC_Status_t DAC_DisableWave(DAC_Channel_t channel)
{
    if (channel >= DAC_CHANNEL_MAX)
    {
        return DAC_ERROR_INVALID_CHANNEL;
    }
    
    if (!g_dac_initialized[channel])
    {
        return DAC_ERROR_NOT_INITIALIZED;
    }
    
    DAC_WaveGenerationCmd(dac_channel_value[channel], DAC_WaveGeneration_None, DISABLE);
    
    return DAC_OK;
}

/**
 * @brief 软件触发DAC转换
 */
DAC_Status_t DAC_SoftwareTrigger(DAC_Channel_t channel)
{
    if (channel >= DAC_CHANNEL_MAX)
    {
        return DAC_ERROR_INVALID_CHANNEL;
    }
    
    if (!g_dac_initialized[channel])
    {
        return DAC_ERROR_NOT_INITIALIZED;
    }
    
    DAC_SoftwareTriggerCmd(dac_channel_value[channel], ENABLE);
    
    return DAC_OK;
}

/**
 * @brief 检查DAC是否已初始化
 */
uint8_t DAC_IsInitialized(DAC_Channel_t channel)
{
    if (channel >= DAC_CHANNEL_MAX)
    {
        return 0;
    }
    
    return g_dac_initialized[channel] ? 1 : 0;
}

/**
 * @brief 获取DAC当前输出值
 */
uint16_t DAC_GetValue(DAC_Channel_t channel)
{
    if (channel >= DAC_CHANNEL_MAX)
    {
        return 0;
    }
    
    if (!g_dac_initialized[channel])
    {
        return 0;
    }
    
    return DAC_GetDataOutputValue(dac_channel_value[channel]);
}

/* ========== DMA模式功能实现 ========== */

/**
 * @brief DAC DMA启动
 */
DAC_Status_t DAC_StartDMA(DAC_Channel_t channel, const uint16_t *buffer, uint16_t length)
{
    uint32_t dac_channel;
    DMA_Channel_t dma_channel;
    DMA_Status_t dma_status;
    
    if (channel >= DAC_CHANNEL_MAX)
    {
        return DAC_ERROR_INVALID_CHANNEL;
    }
    
    if (!g_dac_initialized[channel])
    {
        return DAC_ERROR_NOT_INITIALIZED;
    }
    
    if (buffer == NULL || length == 0)
    {
        return DAC_ERROR_INVALID_PARAM;
    }
    
    dac_channel = dac_channel_value[channel];
    dma_channel = dac_dma_channels[channel];
    
    /* 检查DMA通道是否已初始化 */
    if (!DMA_IsInitialized(dma_channel))
    {
        dma_status = DMA_Init(dma_channel);
        if (dma_status != DMA_OK)
        {
            return DAC_ERROR_INVALID_PARAM;
        }
    }
    
    /* 停止之前的传输 */
    DMA_Stop(dma_channel);
    DAC_DMACmd(dac_channel, DISABLE);
    
    /* 配置DMA传输（内存到外设，半字） */
    /* DAC1使用DHR12R1，DAC2使用DHR12R2 */
    uint32_t dac_dhr_addr = (channel == DAC_CHANNEL_1) ? 
                            (uint32_t)&DAC->DHR12R1 : 
                            (uint32_t)&DAC->DHR12R2;
    
    dma_status = DMA_ConfigTransfer(dma_channel, dac_dhr_addr,
                                    (uint32_t)buffer, length,
                                    DMA_DIR_MEMORY_TO_PERIPHERAL, 2);  /* 半字 */
    if (dma_status != DMA_OK)
    {
        return DAC_ERROR_INVALID_PARAM;
    }
    
    /* 使能DAC DMA */
    DAC_DMACmd(dac_channel, ENABLE);
    
    /* 启动DMA传输 */
    dma_status = DMA_Start(dma_channel);
    if (dma_status != DMA_OK)
    {
        DAC_DMACmd(dac_channel, DISABLE);
        return DAC_ERROR_INVALID_PARAM;
    }
    
    return DAC_OK;
}

/**
 * @brief DAC DMA停止
 */
DAC_Status_t DAC_StopDMA(DAC_Channel_t channel)
{
    uint32_t dac_channel;
    DMA_Channel_t dma_channel;
    
    if (channel >= DAC_CHANNEL_MAX)
    {
        return DAC_ERROR_INVALID_CHANNEL;
    }
    
    if (!g_dac_initialized[channel])
    {
        return DAC_ERROR_NOT_INITIALIZED;
    }
    
    dac_channel = dac_channel_value[channel];
    dma_channel = dac_dma_channels[channel];
    
    /* 禁用DAC DMA */
    DAC_DMACmd(dac_channel, DISABLE);
    
    /* 停止DMA传输 */
    DMA_Stop(dma_channel);
    
    return DAC_OK;
}

/* ========== 双通道同步输出功能实现 ========== */

/**
 * @brief DAC双通道同步设置值
 */
DAC_Status_t DAC_SetDualValue(uint16_t channel1_value, uint16_t channel2_value)
{
    if (channel1_value > 4095 || channel2_value > 4095)
    {
        return DAC_ERROR_INVALID_PARAM;
    }
    
    if (!g_dac_initialized[DAC_CHANNEL_1] || !g_dac_initialized[DAC_CHANNEL_2])
    {
        return DAC_ERROR_NOT_INITIALIZED;
    }
    
    /* 设置DAC1和DAC2的值（使用DHR12RD寄存器，双通道右对齐） */
    DAC->DHR12RD = ((uint32_t)channel2_value << 16) | channel1_value;
    
    return DAC_OK;
}

/**
 * @brief DAC双通道同步软件触发
 */
DAC_Status_t DAC_DualSoftwareTrigger(void)
{
    if (!g_dac_initialized[DAC_CHANNEL_1] || !g_dac_initialized[DAC_CHANNEL_2])
    {
        return DAC_ERROR_NOT_INITIALIZED;
    }
    
    /* 双通道软件触发 */
    DAC_DualSoftwareTriggerCmd(ENABLE);
    
    return DAC_OK;
}

#endif /* STM32F10X_HD || STM32F10X_CL || STM32F10X_HD_VL || STM32F10X_MD_VL */

#endif /* CONFIG_MODULE_DAC_ENABLED */

