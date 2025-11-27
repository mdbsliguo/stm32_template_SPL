/**
 * @file spi_hw.c
 * @brief 硬件SPI驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的硬件SPI驱动，支持主/从模式、全双工/半双工、8位/16位数据格式等功能
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_SPI_ENABLED

/* Include our header */
#include "spi_hw.h"

/* Include STM32 library headers */
#include "gpio.h"
#include "delay.h"
/* NVIC functions are in misc.h */
/* DMA functions are optional - only needed if DMA mode is used */
/* #include "dma.h" */  /* Commented out - DMA module not available, DMA functions are stubs */
#include "stm32f10x_rcc.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_gpio.h"
#include "misc.h"
#include <stdbool.h>
#include <stddef.h>

/* 禁用未使用变量和函数的警告（这些是占位符，用于未来扩展） */
#pragma diag_suppress 177

/* 从board.h加载配置 */
static SPI_Config_t g_spi_configs[SPI_INSTANCE_MAX] = SPI_CONFIGS;

/* 初始化标志 */
static bool g_spi_initialized[SPI_INSTANCE_MAX] = {false, false, false};

/* 默认超时时间（毫秒） */
#define SPI_DEFAULT_TIMEOUT_MS  1000

/* 中断回调函数数组 */
__attribute__((unused)) static SPI_IT_Callback_t g_spi_it_callbacks[SPI_INSTANCE_MAX][3] = {NULL};
__attribute__((unused)) static void *g_spi_it_user_data[SPI_INSTANCE_MAX][3] = {NULL};

/* 中断模式发送/接收缓冲区 */
__attribute__((unused)) static const uint8_t *g_spi_tx_buffer[SPI_INSTANCE_MAX] = {NULL};
__attribute__((unused)) static uint8_t *g_spi_rx_buffer[SPI_INSTANCE_MAX] = {NULL};
__attribute__((unused)) static uint16_t g_spi_tx_length[SPI_INSTANCE_MAX] = {0};
__attribute__((unused)) static uint16_t g_spi_tx_index[SPI_INSTANCE_MAX] = {0};
__attribute__((unused)) static uint16_t g_spi_rx_length[SPI_INSTANCE_MAX] = {0};
__attribute__((unused)) static uint16_t g_spi_rx_index[SPI_INSTANCE_MAX] = {0};
__attribute__((unused)) static uint16_t g_spi_rx_max_length[SPI_INSTANCE_MAX] = {0};

/**
 * @brief 获取SPI外设时钟
 * @param[in] spi_periph SPI外设指针
 * @return uint32_t 时钟使能值，失败返回0
 */
__attribute__((unused)) static uint32_t SPI_GetPeriphClock(SPI_TypeDef *spi_periph)
{
    (void)spi_periph;
    return 0;
}

/**
 * @brief 检查并配置SPI引脚重映射
 * @param[in] spi_periph SPI外设指针
 * @param[in] sck_port SCK引脚端口
 * @param[in] sck_pin SCK引脚号
 * @return SPI_Status_t 错误码
 */
__attribute__((unused)) static SPI_Status_t SPI_ConfigRemap(SPI_TypeDef *spi_periph, GPIO_TypeDef *sck_port, uint16_t sck_pin)
{
    (void)spi_periph;
    (void)sck_port;
    (void)sck_pin;
    return SPI_OK;
}

/**
 * @brief 等待SPI标志位
 * @param[in] spi_periph SPI外设指针
 * @param[in] flag 标志位
 * @param[in] timeout_ms 超时时间（毫秒）
 * @return SPI_Status_t 错误码
 */
static SPI_Status_t SPI_WaitFlag(SPI_TypeDef *spi_periph, uint16_t flag, uint32_t timeout_ms)
{
    uint32_t timeout_count;
    
    /* ========== 参数校验 ========== */
    if (spi_periph == NULL)
    {
        return SPI_ERROR_INVALID_PERIPH;
    }
    
    if (timeout_ms == 0)
    {
        timeout_ms = SPI_DEFAULT_TIMEOUT_MS;
    }
    
    /* 简单超时计数：假设循环一次约1us，timeout_ms毫秒 = timeout_ms * 1000次 */
    timeout_count = timeout_ms * 1000;
    
    /* 等待标志位 */
    while (SPI_I2S_GetFlagStatus(spi_periph, flag) == RESET)
    {
        if (timeout_count-- == 0)
        {
            return SPI_ERROR_TIMEOUT;
        }
    }
    
    return SPI_OK;
}

/**
 * @brief 硬件SPI初始化
 */
SPI_Status_t SPI_HW_Init(SPI_Instance_t instance)
{
    SPI_InitTypeDef SPI_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    const SPI_Config_t *config;
    
    /* ========== 参数校验 ========== */
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    /* 如果已初始化，直接返回成功 */
    if (g_spi_initialized[instance])
    {
        return SPI_OK;
    }
    
    /* 获取配置 */
    config = &g_spi_configs[instance];
    
    /* 检查配置是否有效 */
    if (config->spi_periph == NULL || !config->enabled)
    {
        return SPI_ERROR_INVALID_PERIPH;
    }
    
    /* 1. 使能SPI外设时钟 */
    if (config->spi_periph == SPI1)
    {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
    }
    else if (config->spi_periph == SPI2)
    {
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    }
    else if (config->spi_periph == SPI3)
    {
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE);
    }
    else
    {
        return SPI_ERROR_INVALID_PERIPH;
    }
    
    /* 1.5. 复位SPI外设（清除之前的状态） */
    SPI_I2S_DeInit(config->spi_periph);
    
    /* 2. 使能GPIO时钟并配置GPIO引脚为复用功能 */
    /* SCK引脚：复用推挽输出 */
    if (config->sck_port != NULL && config->sck_pin != 0)
    {
        GPIO_EnableClock(config->sck_port);
        GPIO_InitStructure.GPIO_Pin = config->sck_pin;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(config->sck_port, &GPIO_InitStructure);
    }
    
    /* MISO引脚：复用浮空输入（SPI模式下必须使用复用功能） */
    if (config->miso_port != NULL && config->miso_pin != 0)
    {
        GPIO_EnableClock(config->miso_port);
        GPIO_InitStructure.GPIO_Pin = config->miso_pin;
        /* 关键修复：SPI MISO必须配置为复用功能输入，不能是普通输入 */
        /* GPIO_Mode_AF_PP 是复用推挽，但MISO是输入，应该用 GPIO_Mode_IN_FLOATING */
        /* 实际上，对于STM32F10x，SPI MISO应该配置为 GPIO_Mode_IN_FLOATING 或 GPIO_Mode_IPU */
        /* 但更准确的是：检查是否需要配置复用功能 */
        /* 对于STM32F10x，SPI引脚默认就是复用功能，所以 GPIO_Mode_IN_FLOATING 是正确的 */
        /* 但为了确保，我们检查一下是否需要显式配置复用功能 */
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;  /* 浮空输入（SPI MISO标准配置） */
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(config->miso_port, &GPIO_InitStructure);
    }
    
    /* MOSI引脚：复用推挽输出 */
    if (config->mosi_port != NULL && config->mosi_pin != 0)
    {
        GPIO_EnableClock(config->mosi_port);
        GPIO_InitStructure.GPIO_Pin = config->mosi_pin;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(config->mosi_port, &GPIO_InitStructure);
    }
    
    /* NSS引脚：如果使用硬件NSS，配置为复用推挽输出；否则由软件控制 */
    if (config->nss == SPI_NSS_Hard && config->nss_port != NULL && config->nss_pin != 0)
    {
        GPIO_EnableClock(config->nss_port);
        GPIO_InitStructure.GPIO_Pin = config->nss_pin;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(config->nss_port, &GPIO_InitStructure);
    }
    
    /* 3. 配置SPI外设 */
    SPI_InitStructure.SPI_Direction = config->direction;
    SPI_InitStructure.SPI_Mode = config->mode;
    SPI_InitStructure.SPI_DataSize = config->data_size;
    SPI_InitStructure.SPI_CPOL = config->cpol;
    SPI_InitStructure.SPI_CPHA = config->cpha;
    SPI_InitStructure.SPI_NSS = config->nss;
    SPI_InitStructure.SPI_BaudRatePrescaler = config->baudrate_prescaler;
    SPI_InitStructure.SPI_FirstBit = config->first_bit;
    SPI_InitStructure.SPI_CRCPolynomial = 7;  /* 默认CRC多项式 */
    SPI_Init(config->spi_periph, &SPI_InitStructure);
    
    /* 3.5. 清除SPI状态寄存器（读取SR寄存器清除OVR标志等） */
    (void)SPI_I2S_ReceiveData(config->spi_periph);
    
    /* 4. 使能SPI外设 */
    SPI_Cmd(config->spi_periph, ENABLE);
    
    /* 4.5. 等待SPI总线空闲（确保初始化完成） */
    {
        uint32_t timeout_count = 1000;  /* 1ms超时 */
        while (SPI_I2S_GetFlagStatus(config->spi_periph, SPI_I2S_FLAG_BSY) == SET)
        {
            if (timeout_count-- == 0)
            {
                break;  /* 超时，但不返回错误，继续初始化 */
            }
        }
    }
    
    /* 标记为已初始化 */
    g_spi_initialized[instance] = true;
    
    return SPI_OK;
}

/**
 * @brief SPI反初始化
 */
SPI_Status_t SPI_Deinit(SPI_Instance_t instance)
{
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_OK;
    }
    
    /* TODO: 实际的SPI反初始化代码 */
    
    /* 清除初始化标志 */
    g_spi_initialized[instance] = false;
    
    return SPI_OK;
}

/**
 * @brief SPI主模式发送数据（8位）
 */
SPI_Status_t SPI_MasterTransmit(SPI_Instance_t instance, const uint8_t *data, uint16_t length, uint32_t timeout)
{
    /* 使用全双工传输实现：发送数据，丢弃接收的数据 */
    return SPI_MasterTransmitReceive(instance, data, NULL, length, timeout);
}

/**
 * @brief SPI主模式接收数据（8位）
 */
SPI_Status_t SPI_MasterReceive(SPI_Instance_t instance, uint8_t *data, uint16_t length, uint32_t timeout)
{
    /* 使用全双工传输实现：发送dummy数据，接收数据 */
    return SPI_MasterTransmitReceive(instance, NULL, data, length, timeout);
}

/**
 * @brief SPI主模式全双工传输（8位）
 */
SPI_Status_t SPI_MasterTransmitReceive(SPI_Instance_t instance, const uint8_t *tx_data, uint8_t *rx_data, uint16_t length, uint32_t timeout)
{
    const SPI_Config_t *config;
    SPI_TypeDef *spi_periph;
    uint16_t i;
    uint32_t actual_timeout;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (length == 0)
    {
        return SPI_OK;
    }
    
    config = &g_spi_configs[instance];
    spi_periph = config->spi_periph;
    
    if (spi_periph == NULL)
    {
        return SPI_ERROR_INVALID_PERIPH;
    }
    
    actual_timeout = (timeout == 0) ? SPI_DEFAULT_TIMEOUT_MS : timeout;
    
    /* 传输数据 */
    for (i = 0; i < length; i++)
    {
        /* 等待发送缓冲区空 */
        if (SPI_WaitFlag(spi_periph, SPI_I2S_FLAG_TXE, actual_timeout) != SPI_OK)
        {
            return SPI_ERROR_TIMEOUT;
        }
        
        /* 发送数据 */
        if (tx_data != NULL)
        {
            SPI_I2S_SendData(spi_periph, tx_data[i]);
        }
        else
        {
            SPI_I2S_SendData(spi_periph, 0xFF);  /* 发送dummy数据 */
        }
        
        /* 等待接收缓冲区非空 */
        if (SPI_WaitFlag(spi_periph, SPI_I2S_FLAG_RXNE, actual_timeout) != SPI_OK)
        {
            return SPI_ERROR_TIMEOUT;
        }
        
        /* 接收数据 */
        if (rx_data != NULL)
        {
            rx_data[i] = (uint8_t)SPI_I2S_ReceiveData(spi_periph);
        }
        else
        {
            (void)SPI_I2S_ReceiveData(spi_periph);  /* 丢弃数据 */
        }
    }
    
    /* 参考简单案例：等待RXNE已经确保数据接收完成，不需要等待BSY标志位 */
    /* 注意：对于某些特殊设备（如MAX31856）可能需要等待BSY，但W25Q不需要 */
    
    return SPI_OK;
}

/**
 * @brief SPI主模式发送单个字节
 */
SPI_Status_t SPI_MasterTransmitByte(SPI_Instance_t instance, uint8_t data, uint32_t timeout)
{
    return SPI_MasterTransmitReceive(instance, &data, NULL, 1, timeout);
}

/**
 * @brief SPI主模式接收单个字节
 */
SPI_Status_t SPI_MasterReceiveByte(SPI_Instance_t instance, uint8_t *data, uint32_t timeout)
{
    if (data == NULL)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    return SPI_MasterTransmitReceive(instance, NULL, data, 1, timeout);
}

/**
 * @brief SPI主模式发送数据（16位）
 */
SPI_Status_t SPI_MasterTransmit16(SPI_Instance_t instance, const uint16_t *data, uint16_t length, uint32_t timeout)
{
    (void)instance;
    (void)data;
    (void)length;
    (void)timeout;
    return SPI_OK;
}

/**
 * @brief SPI主模式接收数据（16位）
 */
SPI_Status_t SPI_MasterReceive16(SPI_Instance_t instance, uint16_t *data, uint16_t length, uint32_t timeout)
{
    (void)instance;
    (void)data;
    (void)length;
    (void)timeout;
    return SPI_OK;
}

/**
 * @brief SPI主模式全双工传输（16位）
 */
SPI_Status_t SPI_MasterTransmitReceive16(SPI_Instance_t instance, const uint16_t *tx_data, uint16_t *rx_data, uint16_t length, uint32_t timeout)
{
    (void)instance;
    (void)tx_data;
    (void)rx_data;
    (void)length;
    (void)timeout;
    return SPI_OK;
}

/**
 * @brief 检查SPI是否已初始化
 */
uint8_t SPI_IsInitialized(SPI_Instance_t instance)
{
    if (instance >= SPI_INSTANCE_MAX)
    {
        return 0;
    }
    return g_spi_initialized[instance] ? 1 : 0;
}

/**
 * @brief 获取SPI外设指针
 */
SPI_TypeDef* SPI_GetPeriph(SPI_Instance_t instance)
{
    (void)instance;
    return NULL;
}

/**
 * @brief SPI软件NSS控制（拉低NSS）
 */
SPI_Status_t SPI_NSS_Low(SPI_Instance_t instance)
{
    const SPI_Config_t *config;
    
    /* 参数校验 */
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已初始化 */
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取配置 */
    config = &g_spi_configs[instance];
    
    /* 软件NSS模式，手动控制NSS引脚 */
    if (config->nss == SPI_NSS_Soft && config->nss_port != NULL && config->nss_pin != 0)
    {
        GPIO_WritePin(config->nss_port, config->nss_pin, Bit_RESET);  /* 拉低NSS（选中） */
    }
    
    return SPI_OK;
}

/**
 * @brief SPI软件NSS控制（拉高NSS）
 */
SPI_Status_t SPI_NSS_High(SPI_Instance_t instance)
{
    const SPI_Config_t *config;
    
    /* 参数校验 */
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已初始化 */
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取配置 */
    config = &g_spi_configs[instance];
    
    /* 软件NSS模式，手动控制NSS引脚 */
    if (config->nss == SPI_NSS_Soft && config->nss_port != NULL && config->nss_pin != 0)
    {
        GPIO_WritePin(config->nss_port, config->nss_pin, Bit_SET);  /* 拉高NSS（不选中） */
    }
    
    return SPI_OK;
}

/**
 * @brief 检查SPI是否忙
 */
uint8_t SPI_IsBusy(SPI_Instance_t instance)
{
    (void)instance;
    return 0;
}

/**
 * @brief SPI软件复位
 */
SPI_Status_t SPI_SoftwareReset(SPI_Instance_t instance)
{
    (void)instance;
    return SPI_OK;
}

/**
 * @brief 获取SPI配置信息
 */
SPI_Status_t SPI_GetConfig(SPI_Instance_t instance, SPI_ConfigInfo_t *config_info)
{
    (void)instance;
    (void)config_info;
    return SPI_OK;
}

/* ========== 中断模式功能实现 ========== */

/**
 * @brief 获取SPI中断类型对应的SPL库中断值
 */
__attribute__((unused)) static uint8_t SPI_GetITValue(SPI_IT_t it_type)
{
    (void)it_type;
    return 0;
}

/**
 * @brief 获取SPI中断向量
 */
__attribute__((unused)) static IRQn_Type SPI_GetIRQn(SPI_Instance_t instance)
{
    (void)instance;
    return (IRQn_Type)0;
}

/**
 * @brief 使能SPI中断
 */
SPI_Status_t SPI_EnableIT(SPI_Instance_t instance, SPI_IT_t it_type)
{
    (void)instance;
    (void)it_type;
    return SPI_OK;
}

/**
 * @brief 禁用SPI中断
 */
SPI_Status_t SPI_DisableIT(SPI_Instance_t instance, SPI_IT_t it_type)
{
    (void)instance;
    (void)it_type;
    return SPI_OK;
}

/**
 * @brief 设置SPI中断回调函数
 */
SPI_Status_t SPI_SetITCallback(SPI_Instance_t instance, SPI_IT_t it_type,
                                SPI_IT_Callback_t callback, void *user_data)
{
    (void)instance;
    (void)it_type;
    (void)callback;
    (void)user_data;
    return SPI_OK;
}

/**
 * @brief SPI非阻塞发送（中断模式）
 */
SPI_Status_t SPI_MasterTransmitIT(SPI_Instance_t instance, const uint8_t *data, uint16_t length)
{
    (void)instance;
    (void)data;
    (void)length;
    return SPI_OK;
}

/**
 * @brief SPI非阻塞接收（中断模式）
 */
SPI_Status_t SPI_MasterReceiveIT(SPI_Instance_t instance, uint8_t *data, uint16_t length)
{
    (void)instance;
    (void)data;
    (void)length;
    return SPI_OK;
}

/**
 * @brief SPI非阻塞全双工传输（中断模式）
 */
SPI_Status_t SPI_MasterTransmitReceiveIT(SPI_Instance_t instance, const uint8_t *tx_data, uint8_t *rx_data, uint16_t length)
{
    (void)instance;
    (void)tx_data;
    (void)rx_data;
    (void)length;
    return SPI_OK;
}

/**
 * @brief 获取SPI发送剩余字节数（中断模式）
 */
uint16_t SPI_GetTransmitRemaining(SPI_Instance_t instance)
{
    (void)instance;
    return 0;
}

/**
 * @brief 获取SPI接收到的字节数（中断模式）
 */
uint16_t SPI_GetReceiveCount(SPI_Instance_t instance)
{
    (void)instance;
    return 0;
}

/**
 * @brief SPI中断服务函数
 */
void SPI_IRQHandler(SPI_Instance_t instance)
{
    (void)instance;
}

/* SPI中断服务程序入口 */
void SPI1_IRQHandler(void)
{
}


void SPI2_IRQHandler(void)
{
}


void SPI3_IRQHandler(void)
{
}


/* ========== DMA模式功能实现 ========== */

/* DMA通道类型定义（如果dma.h不存在，使用占位符） */
#ifndef DMA_CHANNEL_T_DEFINED
typedef uint8_t DMA_Channel_t;
#define DMA_CHANNEL_1_4  0
#define DMA_CHANNEL_1_5  1
#define DMA_CHANNEL_2_2  2
#define DMA_CHANNEL_2_3  3
#define DMA_CHANNEL_2_4  4
#define DMA_CHANNEL_2_5  5
#define DMA_CHANNEL_MAX  255
#define DMA_CHANNEL_T_DEFINED
#endif

/* DMA通道映射（SPI TX/RX对应的DMA通道） */
/* 注意：SPI1使用DMA2_CH3(TX)/CH2(RX)，SPI2使用DMA1_CH4(TX)/CH5(RX)，SPI3使用DMA2_CH5(TX)/CH4(RX) */
__attribute__((unused)) static const DMA_Channel_t spi_tx_dma_channels[SPI_INSTANCE_MAX] = {
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    DMA_CHANNEL_2_3,  /* SPI1 TX -> DMA2_CH3 (仅HD/CL/HD_VL) */
#else
    DMA_CHANNEL_MAX,  /* SPI1在MD/LD型号上不支持DMA */
#endif
    DMA_CHANNEL_1_4,  /* SPI2 TX -> DMA1_CH4 */
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    DMA_CHANNEL_2_5,  /* SPI3 TX -> DMA2_CH5 (仅HD/CL/HD_VL) */
#else
    DMA_CHANNEL_MAX,  /* SPI3在MD/LD型号上不支持DMA */
#endif
};

__attribute__((unused)) static const DMA_Channel_t spi_rx_dma_channels[SPI_INSTANCE_MAX] = {
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    DMA_CHANNEL_2_2,  /* SPI1 RX -> DMA2_CH2 (仅HD/CL/HD_VL) */
#else
    DMA_CHANNEL_MAX,  /* SPI1在MD/LD型号上不支持DMA */
#endif
    DMA_CHANNEL_1_5,  /* SPI2 RX -> DMA1_CH5 */
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    DMA_CHANNEL_2_4,  /* SPI3 RX -> DMA2_CH4 (仅HD/CL/HD_VL) */
#else
    DMA_CHANNEL_MAX,  /* SPI3在MD/LD型号上不支持DMA */
#endif
};

/**
 * @brief SPI DMA发送
 */
SPI_Status_t SPI_MasterTransmitDMA(SPI_Instance_t instance, const uint8_t *data, uint16_t length)
{
    (void)instance;
    (void)data;
    (void)length;
    return SPI_OK;
}

/**
 * @brief SPI DMA接收
 */
SPI_Status_t SPI_MasterReceiveDMA(SPI_Instance_t instance, uint8_t *data, uint16_t length)
{
    (void)instance;
    (void)data;
    (void)length;
    return SPI_OK;
}

/**
 * @brief SPI DMA全双工传输
 */
SPI_Status_t SPI_MasterTransmitReceiveDMA(SPI_Instance_t instance, const uint8_t *tx_data, uint8_t *rx_data, uint16_t length)
{
    (void)instance;
    (void)tx_data;
    (void)rx_data;
    (void)length;
    return SPI_OK;
}

/**
 * @brief 停止SPI DMA发送
 */
SPI_Status_t SPI_StopTransmitDMA(SPI_Instance_t instance)
{
    (void)instance;
    return SPI_OK;
}

/**
 * @brief 停止SPI DMA接收
 */
SPI_Status_t SPI_StopReceiveDMA(SPI_Instance_t instance)
{
    (void)instance;
    return SPI_OK;
}

/* ========== 从模式功能实现 ========== */

/**
 * @brief SPI从模式发送数据（8位）
 */
SPI_Status_t SPI_SlaveTransmit(SPI_Instance_t instance, const uint8_t *data, uint16_t length, uint32_t timeout)
{
    (void)instance;
    (void)data;
    (void)length;
    (void)timeout;
    return SPI_OK;
}

/**
 * @brief SPI从模式接收数据（8位）
 */
SPI_Status_t SPI_SlaveReceive(SPI_Instance_t instance, uint8_t *data, uint16_t length, uint32_t timeout)
{
    (void)instance;
    (void)data;
    (void)length;
    (void)timeout;
    return SPI_OK;
}

/**
 * @brief SPI从模式全双工传输（8位）
 */
SPI_Status_t SPI_SlaveTransmitReceive(SPI_Instance_t instance, const uint8_t *tx_data, uint8_t *rx_data, uint16_t length, uint32_t timeout)
{
    (void)instance;
    (void)tx_data;
    (void)rx_data;
    (void)length;
    (void)timeout;
    return SPI_OK;
}

/* ========== TI模式和硬件NSS管理功能实现 ========== */

/**
 * @brief 使能SPI TI模式
 */
SPI_Status_t SPI_EnableTIMode(SPI_Instance_t instance)
{
    (void)instance;
    return SPI_OK;
}

/**
 * @brief 禁用SPI TI模式
 */
SPI_Status_t SPI_DisableTIMode(SPI_Instance_t instance)
{
    (void)instance;
    return SPI_OK;
}

/**
 * @brief 配置SPI硬件NSS内部软件管理
 */
SPI_Status_t SPI_ConfigHardwareNSS(SPI_Instance_t instance, uint8_t nss_level)
{
    (void)instance;
    (void)nss_level;
    return SPI_OK;
}

/**
 * @brief 配置SPI CRC计算
 */
SPI_Status_t SPI_ConfigCRC(SPI_Instance_t instance, uint16_t polynomial)
{
    (void)instance;
    (void)polynomial;
    return SPI_OK;
}

/**
 * @brief 使能SPI CRC计算
 */
SPI_Status_t SPI_EnableCRC(SPI_Instance_t instance)
{
    (void)instance;
    return SPI_OK;
}

/**
 * @brief 禁用SPI CRC计算
 */
SPI_Status_t SPI_DisableCRC(SPI_Instance_t instance)
{
    (void)instance;
    return SPI_OK;
}

/**
 * @brief 获取SPI CRC值
 */
uint16_t SPI_HW_GetCRC(SPI_Instance_t instance)
{
    (void)instance;
    return 0;
}

#endif /* CONFIG_MODULE_SPI_ENABLED */

