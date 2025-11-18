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
#include "nvic.h"
#include "dma.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_spi.h"
#include "misc.h"
#include <stdbool.h>
#include <stddef.h>

/* 从board.h加载配置 */
static SPI_Config_t g_spi_configs[SPI_INSTANCE_MAX] = SPI_CONFIGS;

/* 初始化标志 */
static bool g_spi_initialized[SPI_INSTANCE_MAX] = {false, false, false};

/* 默认超时时间（毫秒） */
#define SPI_DEFAULT_TIMEOUT_MS  1000

/* 中断回调函数数组 */
static SPI_IT_Callback_t g_spi_it_callbacks[SPI_INSTANCE_MAX][3] = {NULL};
static void *g_spi_it_user_data[SPI_INSTANCE_MAX][3] = {NULL};

/* 中断模式发送/接收缓冲区 */
static const uint8_t *g_spi_tx_buffer[SPI_INSTANCE_MAX] = {NULL};
static uint8_t *g_spi_rx_buffer[SPI_INSTANCE_MAX] = {NULL};
static uint16_t g_spi_tx_length[SPI_INSTANCE_MAX] = {0};
static uint16_t g_spi_tx_index[SPI_INSTANCE_MAX] = {0};
static uint16_t g_spi_rx_length[SPI_INSTANCE_MAX] = {0};
static uint16_t g_spi_rx_index[SPI_INSTANCE_MAX] = {0};
static uint16_t g_spi_rx_max_length[SPI_INSTANCE_MAX] = {0};

/**
 * @brief 获取SPI外设时钟
 * @param[in] spi_periph SPI外设指针
 * @return uint32_t 时钟使能值，失败返回0
 */
static uint32_t SPI_GetPeriphClock(SPI_TypeDef *spi_periph)
{
    if (spi_periph == SPI1)
    {
        return RCC_APB2Periph_SPI1;
    }
    else if (spi_periph == SPI2)
    {
        return RCC_APB1Periph_SPI2;
    }
    else if (spi_periph == SPI3)
    {
        return RCC_APB1Periph_SPI3;
    }
    return 0;
}

/**
 * @brief 检查并配置SPI引脚重映射
 * @param[in] spi_periph SPI外设指针
 * @param[in] sck_port SCK引脚端口
 * @param[in] sck_pin SCK引脚号
 * @return SPI_Status_t 错误码
 */
static SPI_Status_t SPI_ConfigRemap(SPI_TypeDef *spi_periph, GPIO_TypeDef *sck_port, uint16_t sck_pin)
{
    /* SPI1重映射检查：SPI1可以重映射到PB3/PB4/PB5 */
    if (spi_periph == SPI1)
    {
        if (sck_port == GPIOB && sck_pin == GPIO_Pin_3)
        {
            /* 启用SPI1重映射到PB3/PB4/PB5 */
            GPIO_PinRemapConfig(GPIO_Remap_SPI1, ENABLE);
        }
        else if (sck_port == GPIOA && sck_pin == GPIO_Pin_5)
        {
            /* 默认引脚PA5/PA6/PA7，不需要重映射 */
            GPIO_PinRemapConfig(GPIO_Remap_SPI1, DISABLE);
        }
        else
        {
            /* 不支持的引脚配置 */
            return SPI_ERROR_GPIO_FAILED;
        }
    }
    /* SPI2和SPI3不需要重映射 */
    else if (spi_periph == SPI2)
    {
        if (sck_port != GPIOB || sck_pin != GPIO_Pin_13)
        {
            /* SPI2只支持PB13/PB14/PB15 */
            return SPI_ERROR_GPIO_FAILED;
        }
    }
    else if (spi_periph == SPI3)
    {
        if (sck_port != GPIOB || (sck_pin != GPIO_Pin_3 && sck_pin != GPIO_Pin_10))
        {
            /* SPI3支持PB3/PB4/PB5或PB10/PB11/PB12 */
            return SPI_ERROR_GPIO_FAILED;
        }
    }
    else
    {
        return SPI_ERROR_INVALID_PERIPH;
    }
    
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
    uint32_t start_tick = Delay_GetTick();
    
    while (SPI_GetFlagStatus(spi_periph, flag) == RESET)
    {
        /* 检查超时（使用Delay_GetElapsed处理溢出） */
        uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
        if (elapsed > timeout_ms)
        {
            return SPI_ERROR_TIMEOUT;
        }
        
        /* 检查错误标志 */
        if (SPI_GetFlagStatus(spi_periph, SPI_FLAG_OVR) != RESET)
        {
            SPI_ClearFlag(spi_periph, SPI_FLAG_OVR);
            return SPI_ERROR_OVERRUN;
        }
        if (SPI_GetFlagStatus(spi_periph, SPI_FLAG_MODF) != RESET)
        {
            SPI_ClearFlag(spi_periph, SPI_FLAG_MODF);
            return SPI_ERROR_MODE_FAULT;
        }
        if (SPI_GetFlagStatus(spi_periph, SPI_FLAG_CRCERR) != RESET)
        {
            SPI_ClearFlag(spi_periph, SPI_FLAG_CRCERR);
            return SPI_ERROR_CRC;
        }
    }
    
    return SPI_OK;
}

/**
 * @brief 硬件SPI初始化
 */
SPI_Status_t SPI_HW_Init(SPI_Instance_t instance)
{
    SPI_Status_t status;
    GPIO_Status_t gpio_status;
    SPI_InitTypeDef SPI_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    uint32_t spi_clock;
    
    /* 参数校验 */
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_configs[instance].enabled)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (g_spi_configs[instance].spi_periph == NULL)
    {
        return SPI_ERROR_INVALID_PERIPH;
    }
    
    /* 检查端口指针是否为NULL */
    if (g_spi_configs[instance].sck_port == NULL ||
        g_spi_configs[instance].miso_port == NULL ||
        g_spi_configs[instance].mosi_port == NULL)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已初始化 */
    if (g_spi_initialized[instance])
    {
        return SPI_OK;
    }
    
    /* 获取SPI外设时钟 */
    spi_clock = SPI_GetPeriphClock(g_spi_configs[instance].spi_periph);
    if (spi_clock == 0)
    {
        return SPI_ERROR_INVALID_PERIPH;
    }
    
    /* 使能SPI外设时钟 */
    if (g_spi_configs[instance].spi_periph == SPI1)
    {
        RCC_APB2PeriphClockCmd(spi_clock, ENABLE);
    }
    else
    {
        RCC_APB1PeriphClockCmd(spi_clock, ENABLE);
    }
    
    /* 使能GPIO时钟 */
    gpio_status = GPIO_EnableClock(g_spi_configs[instance].sck_port);
    if (gpio_status != GPIO_OK)
    {
        return SPI_ERROR_GPIO_FAILED;
    }
    
    if (g_spi_configs[instance].miso_port != g_spi_configs[instance].sck_port)
    {
        gpio_status = GPIO_EnableClock(g_spi_configs[instance].miso_port);
        if (gpio_status != GPIO_OK)
        {
            return SPI_ERROR_GPIO_FAILED;
        }
    }
    
    if (g_spi_configs[instance].mosi_port != g_spi_configs[instance].sck_port &&
        g_spi_configs[instance].mosi_port != g_spi_configs[instance].miso_port)
    {
        gpio_status = GPIO_EnableClock(g_spi_configs[instance].mosi_port);
        if (gpio_status != GPIO_OK)
        {
            return SPI_ERROR_GPIO_FAILED;
        }
    }
    
    /* 配置SPI引脚重映射（如果需要） */
    status = SPI_ConfigRemap(g_spi_configs[instance].spi_periph,
                              g_spi_configs[instance].sck_port,
                              g_spi_configs[instance].sck_pin);
    if (status != SPI_OK)
    {
        return status;
    }
    
    /* 配置SCK引脚为复用推挽输出 */
    GPIO_InitStructure.GPIO_Pin = g_spi_configs[instance].sck_pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(g_spi_configs[instance].sck_port, &GPIO_InitStructure);
    
    /* 配置MISO引脚为复用浮空输入（主模式）或复用推挽输出（从模式） */
    if (g_spi_configs[instance].mode == SPI_Mode_Master)
    {
        GPIO_InitStructure.GPIO_Pin = g_spi_configs[instance].miso_pin;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        GPIO_Init(g_spi_configs[instance].miso_port, &GPIO_InitStructure);
    }
    else
    {
        GPIO_InitStructure.GPIO_Pin = g_spi_configs[instance].miso_pin;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
        GPIO_Init(g_spi_configs[instance].miso_port, &GPIO_InitStructure);
    }
    
    /* 配置MOSI引脚为复用推挽输出（主模式）或复用浮空输入（从模式） */
    if (g_spi_configs[instance].mode == SPI_Mode_Master)
    {
        GPIO_InitStructure.GPIO_Pin = g_spi_configs[instance].mosi_pin;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
        GPIO_Init(g_spi_configs[instance].mosi_port, &GPIO_InitStructure);
    }
    else
    {
        GPIO_InitStructure.GPIO_Pin = g_spi_configs[instance].mosi_pin;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        GPIO_Init(g_spi_configs[instance].mosi_port, &GPIO_InitStructure);
    }
    
    /* 配置NSS引脚（如果使用硬件NSS） */
    if (g_spi_configs[instance].nss == SPI_NSS_Hard && 
        g_spi_configs[instance].nss_port != NULL && 
        g_spi_configs[instance].nss_pin != 0)
    {
        if (g_spi_configs[instance].mode == SPI_Mode_Master)
        {
            /* 主模式：NSS配置为复用推挽输出 */
            GPIO_InitStructure.GPIO_Pin = g_spi_configs[instance].nss_pin;
            GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
            GPIO_Init(g_spi_configs[instance].nss_port, &GPIO_InitStructure);
        }
        else
        {
            /* 从模式：NSS配置为浮空输入 */
            GPIO_InitStructure.GPIO_Pin = g_spi_configs[instance].nss_pin;
            GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
            GPIO_Init(g_spi_configs[instance].nss_port, &GPIO_InitStructure);
        }
    }
    else if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
             g_spi_configs[instance].nss_port != NULL && 
             g_spi_configs[instance].nss_pin != 0)
    {
        /* 软件NSS：配置为普通推挽输出 */
        GPIO_InitStructure.GPIO_Pin = g_spi_configs[instance].nss_pin;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(g_spi_configs[instance].nss_port, &GPIO_InitStructure);
        /* 默认拉高NSS */
        GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
    }
    
    /* 配置SPI外设 */
    SPI_InitStructure.SPI_Direction = g_spi_configs[instance].direction;
    SPI_InitStructure.SPI_Mode = g_spi_configs[instance].mode;
    SPI_InitStructure.SPI_DataSize = g_spi_configs[instance].data_size;
    SPI_InitStructure.SPI_CPOL = g_spi_configs[instance].cpol;
    SPI_InitStructure.SPI_CPHA = g_spi_configs[instance].cpha;
    SPI_InitStructure.SPI_NSS = g_spi_configs[instance].nss;
    SPI_InitStructure.SPI_BaudRatePrescaler = g_spi_configs[instance].baudrate_prescaler;
    SPI_InitStructure.SPI_FirstBit = g_spi_configs[instance].first_bit;
    SPI_InitStructure.SPI_CRCPolynomial = 7;  /* 默认CRC多项式 */
    
    /* Call the standard library's SPI_Init function */
    SPI_Init(g_spi_configs[instance].spi_periph, &SPI_InitStructure);
    
    /* 使能SPI外设 */
    SPI_Cmd(g_spi_configs[instance].spi_periph, ENABLE);
    
    /* 初始化中断相关变量 */
    for (int i = 0; i < 3; i++)
    {
        g_spi_it_callbacks[instance][i] = NULL;
        g_spi_it_user_data[instance][i] = NULL;
    }
    g_spi_tx_buffer[instance] = NULL;
    g_spi_rx_buffer[instance] = NULL;
    g_spi_tx_length[instance] = 0;
    g_spi_tx_index[instance] = 0;
    g_spi_rx_length[instance] = 0;
    g_spi_rx_index[instance] = 0;
    g_spi_rx_max_length[instance] = 0;
    
    /* 标记为已初始化 */
    g_spi_initialized[instance] = true;
    
    return SPI_OK;
}

/**
 * @brief SPI反初始化
 */
SPI_Status_t SPI_Deinit(SPI_Instance_t instance)
{
    uint32_t spi_clock;
    
    /* 参数校验 */
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_OK;
    }
    
    /* 禁用SPI外设 */
    SPI_Cmd(g_spi_configs[instance].spi_periph, DISABLE);
    
    /* 获取SPI外设时钟 */
    spi_clock = SPI_GetPeriphClock(g_spi_configs[instance].spi_periph);
    if (spi_clock != 0)
    {
        /* 禁用SPI外设时钟 */
        if (g_spi_configs[instance].spi_periph == SPI1)
        {
            RCC_APB2PeriphClockCmd(spi_clock, DISABLE);
        }
        else
        {
            RCC_APB1PeriphClockCmd(spi_clock, DISABLE);
        }
    }
    
    /* 标记为未初始化 */
    g_spi_initialized[instance] = false;
    
    return SPI_OK;
}

/**
 * @brief SPI主模式发送数据（8位）
 */
SPI_Status_t SPI_MasterTransmit(SPI_Instance_t instance, const uint8_t *data, uint16_t length, uint32_t timeout)
{
    SPI_TypeDef *spi_periph;
    SPI_Status_t status;
    uint16_t i;
    uint32_t actual_timeout;
    
    /* 参数校验 */
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (g_spi_configs[instance].data_size != SPI_DataSize_8b)
    {
        return SPI_ERROR_INVALID_PARAM;  /* 数据大小不匹配 */
    }
    
    /* 检查模式：必须是主模式 */
    if (g_spi_configs[instance].mode != SPI_Mode_Master)
    {
        return SPI_ERROR_MODE_FAULT;  /* 不是主模式 */
    }
    
    /* 检查数据方向：不能是只接收模式 */
    if (g_spi_configs[instance].direction == SPI_Direction_2Lines_RxOnly ||
        g_spi_configs[instance].direction == SPI_Direction_1Line_Rx)
    {
        return SPI_ERROR_INVALID_PARAM;  /* 配置为只接收模式，不能发送 */
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    actual_timeout = (timeout == 0) ? SPI_DEFAULT_TIMEOUT_MS : timeout;
    
    /* 软件NSS：拉低NSS */
    if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
        g_spi_configs[instance].nss_port != NULL)
    {
        GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_RESET);
    }
    
    /* 发送数据 */
    for (i = 0; i < length; i++)
    {
        /* 等待发送缓冲区为空 */
        status = SPI_WaitFlag(spi_periph, SPI_FLAG_TXE, actual_timeout);
        if (status != SPI_OK)
        {
            if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
                g_spi_configs[instance].nss_port != NULL)
            {
                GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
            }
            return status;
        }
        
        /* 发送数据 */
        SPI_SendData8(spi_periph, data[i]);
        
        /* 等待接收完成（全双工模式下，发送的同时也会接收） */
        status = SPI_WaitFlag(spi_periph, SPI_FLAG_RXNE, actual_timeout);
        if (status != SPI_OK)
        {
            if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
                g_spi_configs[instance].nss_port != NULL)
            {
                GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
            }
            return status;
        }
        
        /* 读取接收数据（丢弃，因为这是发送模式） */
        SPI_ReceiveData8(spi_periph);
    }
    
    /* 等待总线空闲（循环已经处理了所有数据，不需要再等待和读取） */
    {
        uint32_t busy_start_tick = Delay_GetTick();
        while (SPI_GetFlagStatus(spi_periph, SPI_FLAG_BSY) != RESET)
        {
            uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), busy_start_tick);
            if (elapsed > actual_timeout)
            {
                if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
                    g_spi_configs[instance].nss_port != NULL)
                {
                    GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
                }
                return SPI_ERROR_TIMEOUT;
            }
        }
    }
    
    /* 软件NSS：拉高NSS */
    if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
        g_spi_configs[instance].nss_port != NULL)
    {
        GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
    }
    
    return SPI_OK;
}

/**
 * @brief SPI主模式接收数据（8位）
 */
SPI_Status_t SPI_MasterReceive(SPI_Instance_t instance, uint8_t *data, uint16_t length, uint32_t timeout)
{
    SPI_TypeDef *spi_periph;
    SPI_Status_t status;
    uint16_t i;
    uint32_t actual_timeout;
    
    /* 参数校验 */
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (g_spi_configs[instance].data_size != SPI_DataSize_8b)
    {
        return SPI_ERROR_INVALID_PARAM;  /* 数据大小不匹配 */
    }
    
    /* 检查模式：必须是主模式 */
    if (g_spi_configs[instance].mode != SPI_Mode_Master)
    {
        return SPI_ERROR_MODE_FAULT;  /* 不是主模式 */
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    actual_timeout = (timeout == 0) ? SPI_DEFAULT_TIMEOUT_MS : timeout;
    
    /* 软件NSS：拉低NSS */
    if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
        g_spi_configs[instance].nss_port != NULL)
    {
        GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_RESET);
    }
    
    /* 接收数据（需要发送dummy数据来产生时钟） */
    for (i = 0; i < length; i++)
    {
        /* 等待发送缓冲区为空 */
        status = SPI_WaitFlag(spi_periph, SPI_FLAG_TXE, actual_timeout);
        if (status != SPI_OK)
        {
            if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
                g_spi_configs[instance].nss_port != NULL)
            {
                GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
            }
            return status;
        }
        
        /* 发送dummy数据（0xFF）来产生时钟 */
        SPI_SendData8(spi_periph, 0xFF);
        
        /* 等待接收完成 */
        status = SPI_WaitFlag(spi_periph, SPI_FLAG_RXNE, actual_timeout);
        if (status != SPI_OK)
        {
            if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
                g_spi_configs[instance].nss_port != NULL)
            {
                GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
            }
            return status;
        }
        
        /* 读取接收数据 */
        data[i] = SPI_ReceiveData8(spi_periph);
    }
    
    /* 等待总线空闲（循环已经处理了所有数据，不需要再等待和读取） */
    {
        uint32_t busy_start_tick = Delay_GetTick();
        while (SPI_GetFlagStatus(spi_periph, SPI_FLAG_BSY) != RESET)
        {
            uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), busy_start_tick);
            if (elapsed > actual_timeout)
            {
                if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
                    g_spi_configs[instance].nss_port != NULL)
                {
                    GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
                }
                return SPI_ERROR_TIMEOUT;
            }
        }
    }
    
    /* 软件NSS：拉高NSS */
    if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
        g_spi_configs[instance].nss_port != NULL)
    {
        GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
    }
    
    return SPI_OK;
}

/**
 * @brief SPI主模式全双工传输（8位）
 */
SPI_Status_t SPI_MasterTransmitReceive(SPI_Instance_t instance, const uint8_t *tx_data, uint8_t *rx_data, uint16_t length, uint32_t timeout)
{
    SPI_TypeDef *spi_periph;
    SPI_Status_t status;
    uint16_t i;
    uint32_t actual_timeout;
    
    /* 参数校验 */
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (tx_data == NULL || rx_data == NULL || length == 0)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (g_spi_configs[instance].data_size != SPI_DataSize_8b)
    {
        return SPI_ERROR_INVALID_PARAM;  /* 数据大小不匹配 */
    }
    
    /* 检查模式：必须是主模式 */
    if (g_spi_configs[instance].mode != SPI_Mode_Master)
    {
        return SPI_ERROR_MODE_FAULT;  /* 不是主模式 */
    }
    
    /* 检查数据方向：必须是全双工模式 */
    if (g_spi_configs[instance].direction != SPI_Direction_2Lines_FullDuplex)
    {
        return SPI_ERROR_INVALID_PARAM;  /* 不是全双工模式 */
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    actual_timeout = (timeout == 0) ? SPI_DEFAULT_TIMEOUT_MS : timeout;
    
    /* 软件NSS：拉低NSS */
    if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
        g_spi_configs[instance].nss_port != NULL)
    {
        GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_RESET);
    }
    
    /* 全双工传输 */
    for (i = 0; i < length; i++)
    {
        /* 等待发送缓冲区为空 */
        status = SPI_WaitFlag(spi_periph, SPI_FLAG_TXE, actual_timeout);
        if (status != SPI_OK)
        {
            if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
                g_spi_configs[instance].nss_port != NULL)
            {
                GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
            }
            return status;
        }
        
        /* 发送数据 */
        SPI_SendData8(spi_periph, tx_data[i]);
        
        /* 等待接收完成 */
        status = SPI_WaitFlag(spi_periph, SPI_FLAG_RXNE, actual_timeout);
        if (status != SPI_OK)
        {
            if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
                g_spi_configs[instance].nss_port != NULL)
            {
                GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
            }
            return status;
        }
        
        /* 读取接收数据 */
        rx_data[i] = SPI_ReceiveData8(spi_periph);
    }
    
    /* 等待总线空闲（循环已经处理了所有数据，不需要再等待和读取） */
    {
        uint32_t busy_start_tick = Delay_GetTick();
        while (SPI_GetFlagStatus(spi_periph, SPI_FLAG_BSY) != RESET)
        {
            uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), busy_start_tick);
            if (elapsed > actual_timeout)
            {
                if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
                    g_spi_configs[instance].nss_port != NULL)
                {
                    GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
                }
                return SPI_ERROR_TIMEOUT;
            }
        }
    }
    
    /* 软件NSS：拉高NSS */
    if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
        g_spi_configs[instance].nss_port != NULL)
    {
        GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
    }
    
    return SPI_OK;
}

/**
 * @brief SPI主模式发送单个字节
 */
SPI_Status_t SPI_MasterTransmitByte(SPI_Instance_t instance, uint8_t data, uint32_t timeout)
{
    return SPI_MasterTransmit(instance, &data, 1, timeout);
}

/**
 * @brief SPI主模式接收单个字节
 */
SPI_Status_t SPI_MasterReceiveByte(SPI_Instance_t instance, uint8_t *data, uint32_t timeout)
{
    return SPI_MasterReceive(instance, data, 1, timeout);
}

/**
 * @brief SPI主模式发送数据（16位）
 */
SPI_Status_t SPI_MasterTransmit16(SPI_Instance_t instance, const uint16_t *data, uint16_t length, uint32_t timeout)
{
    SPI_TypeDef *spi_periph;
    SPI_Status_t status;
    uint16_t i;
    uint32_t actual_timeout;
    
    /* 参数校验 */
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (g_spi_configs[instance].data_size != SPI_DataSize_16b)
    {
        return SPI_ERROR_INVALID_PARAM;  /* 数据大小不匹配 */
    }
    
    /* 检查模式：必须是主模式 */
    if (g_spi_configs[instance].mode != SPI_Mode_Master)
    {
        return SPI_ERROR_MODE_FAULT;  /* 不是主模式 */
    }
    
    /* 检查数据方向：不能是只接收模式 */
    if (g_spi_configs[instance].direction == SPI_Direction_2Lines_RxOnly ||
        g_spi_configs[instance].direction == SPI_Direction_1Line_Rx)
    {
        return SPI_ERROR_INVALID_PARAM;  /* 配置为只接收模式，不能发送 */
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    actual_timeout = (timeout == 0) ? SPI_DEFAULT_TIMEOUT_MS : timeout;
    
    /* 软件NSS：拉低NSS */
    if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
        g_spi_configs[instance].nss_port != NULL)
    {
        GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_RESET);
    }
    
    /* 发送数据 */
    for (i = 0; i < length; i++)
    {
        /* 等待发送缓冲区为空 */
        status = SPI_WaitFlag(spi_periph, SPI_FLAG_TXE, actual_timeout);
        if (status != SPI_OK)
        {
            if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
                g_spi_configs[instance].nss_port != NULL)
            {
                GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
            }
            return status;
        }
        
        /* 发送数据 */
        SPI_SendData16(spi_periph, data[i]);
        
        /* 等待接收完成 */
        status = SPI_WaitFlag(spi_periph, SPI_FLAG_RXNE, actual_timeout);
        if (status != SPI_OK)
        {
            if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
                g_spi_configs[instance].nss_port != NULL)
            {
                GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
            }
            return status;
        }
        
        /* 读取接收数据（丢弃） */
        SPI_ReceiveData16(spi_periph);
    }
    
    /* 等待总线空闲（循环已经处理了所有数据，不需要再等待和读取） */
    {
        uint32_t busy_start_tick = Delay_GetTick();
        while (SPI_GetFlagStatus(spi_periph, SPI_FLAG_BSY) != RESET)
        {
            uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), busy_start_tick);
            if (elapsed > actual_timeout)
            {
                if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
                    g_spi_configs[instance].nss_port != NULL)
                {
                    GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
                }
                return SPI_ERROR_TIMEOUT;
            }
        }
    }
    
    /* 软件NSS：拉高NSS */
    if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
        g_spi_configs[instance].nss_port != NULL)
    {
        GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
    }
    
    return SPI_OK;
}

/**
 * @brief SPI主模式接收数据（16位）
 */
SPI_Status_t SPI_MasterReceive16(SPI_Instance_t instance, uint16_t *data, uint16_t length, uint32_t timeout)
{
    SPI_TypeDef *spi_periph;
    SPI_Status_t status;
    uint16_t i;
    uint32_t actual_timeout;
    
    /* 参数校验 */
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (g_spi_configs[instance].data_size != SPI_DataSize_16b)
    {
        return SPI_ERROR_INVALID_PARAM;  /* 数据大小不匹配 */
    }
    
    /* 检查模式：必须是主模式 */
    if (g_spi_configs[instance].mode != SPI_Mode_Master)
    {
        return SPI_ERROR_MODE_FAULT;  /* 不是主模式 */
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    actual_timeout = (timeout == 0) ? SPI_DEFAULT_TIMEOUT_MS : timeout;
    
    /* 软件NSS：拉低NSS */
    if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
        g_spi_configs[instance].nss_port != NULL)
    {
        GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_RESET);
    }
    
    /* 接收数据（需要发送dummy数据来产生时钟） */
    for (i = 0; i < length; i++)
    {
        /* 等待发送缓冲区为空 */
        status = SPI_WaitFlag(spi_periph, SPI_FLAG_TXE, actual_timeout);
        if (status != SPI_OK)
        {
            if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
                g_spi_configs[instance].nss_port != NULL)
            {
                GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
            }
            return status;
        }
        
        /* 发送dummy数据（0xFFFF）来产生时钟 */
        SPI_SendData16(spi_periph, 0xFFFF);
        
        /* 等待接收完成 */
        status = SPI_WaitFlag(spi_periph, SPI_FLAG_RXNE, actual_timeout);
        if (status != SPI_OK)
        {
            if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
                g_spi_configs[instance].nss_port != NULL)
            {
                GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
            }
            return status;
        }
        
        /* 读取接收数据 */
        data[i] = SPI_ReceiveData16(spi_periph);
    }
    
    /* 等待总线空闲（循环已经处理了所有数据，不需要再等待和读取） */
    {
        uint32_t busy_start_tick = Delay_GetTick();
        while (SPI_GetFlagStatus(spi_periph, SPI_FLAG_BSY) != RESET)
        {
            uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), busy_start_tick);
            if (elapsed > actual_timeout)
            {
                if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
                    g_spi_configs[instance].nss_port != NULL)
                {
                    GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
                }
                return SPI_ERROR_TIMEOUT;
            }
        }
    }
    
    /* 软件NSS：拉高NSS */
    if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
        g_spi_configs[instance].nss_port != NULL)
    {
        GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
    }
    
    return SPI_OK;
}

/**
 * @brief SPI主模式全双工传输（16位）
 */
SPI_Status_t SPI_MasterTransmitReceive16(SPI_Instance_t instance, const uint16_t *tx_data, uint16_t *rx_data, uint16_t length, uint32_t timeout)
{
    SPI_TypeDef *spi_periph;
    SPI_Status_t status;
    uint16_t i;
    uint32_t actual_timeout;
    
    /* 参数校验 */
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (tx_data == NULL || rx_data == NULL || length == 0)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (g_spi_configs[instance].data_size != SPI_DataSize_16b)
    {
        return SPI_ERROR_INVALID_PARAM;  /* 数据大小不匹配 */
    }
    
    /* 检查模式：必须是主模式 */
    if (g_spi_configs[instance].mode != SPI_Mode_Master)
    {
        return SPI_ERROR_MODE_FAULT;  /* 不是主模式 */
    }
    
    /* 检查数据方向：必须是全双工模式 */
    if (g_spi_configs[instance].direction != SPI_Direction_2Lines_FullDuplex)
    {
        return SPI_ERROR_INVALID_PARAM;  /* 不是全双工模式 */
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    actual_timeout = (timeout == 0) ? SPI_DEFAULT_TIMEOUT_MS : timeout;
    
    /* 软件NSS：拉低NSS */
    if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
        g_spi_configs[instance].nss_port != NULL)
    {
        GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_RESET);
    }
    
    /* 全双工传输 */
    for (i = 0; i < length; i++)
    {
        /* 等待发送缓冲区为空 */
        status = SPI_WaitFlag(spi_periph, SPI_FLAG_TXE, actual_timeout);
        if (status != SPI_OK)
        {
            if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
                g_spi_configs[instance].nss_port != NULL)
            {
                GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
            }
            return status;
        }
        
        /* 发送数据 */
        SPI_SendData16(spi_periph, tx_data[i]);
        
        /* 等待接收完成 */
        status = SPI_WaitFlag(spi_periph, SPI_FLAG_RXNE, actual_timeout);
        if (status != SPI_OK)
        {
            if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
                g_spi_configs[instance].nss_port != NULL)
            {
                GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
            }
            return status;
        }
        
        /* 读取接收数据 */
        rx_data[i] = SPI_ReceiveData16(spi_periph);
    }
    
    /* 等待总线空闲（循环已经处理了所有数据，不需要再等待和读取） */
    {
        uint32_t busy_start_tick = Delay_GetTick();
        while (SPI_GetFlagStatus(spi_periph, SPI_FLAG_BSY) != RESET)
        {
            uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), busy_start_tick);
            if (elapsed > actual_timeout)
            {
                if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
                    g_spi_configs[instance].nss_port != NULL)
                {
                    GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
                }
                return SPI_ERROR_TIMEOUT;
            }
        }
    }
    
    /* 软件NSS：拉高NSS */
    if (g_spi_configs[instance].nss == SPI_NSS_Soft && 
        g_spi_configs[instance].nss_port != NULL)
    {
        GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
    }
    
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
    if (instance >= SPI_INSTANCE_MAX)
    {
        return NULL;
    }
    
    if (!g_spi_initialized[instance])
    {
        return NULL;
    }
    
    return g_spi_configs[instance].spi_periph;
}

/**
 * @brief SPI软件NSS控制（拉低NSS）
 */
SPI_Status_t SPI_NSS_Low(SPI_Instance_t instance)
{
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (g_spi_configs[instance].nss != SPI_NSS_Soft)
    {
        return SPI_ERROR_INVALID_PARAM;  /* 不是软件NSS模式 */
    }
    
    if (g_spi_configs[instance].nss_port == NULL || g_spi_configs[instance].nss_pin == 0)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_RESET);
    
    return SPI_OK;
}

/**
 * @brief SPI软件NSS控制（拉高NSS）
 */
SPI_Status_t SPI_NSS_High(SPI_Instance_t instance)
{
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (g_spi_configs[instance].nss != SPI_NSS_Soft)
    {
        return SPI_ERROR_INVALID_PARAM;  /* 不是软件NSS模式 */
    }
    
    if (g_spi_configs[instance].nss_port == NULL || g_spi_configs[instance].nss_pin == 0)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    GPIO_WritePin(g_spi_configs[instance].nss_port, g_spi_configs[instance].nss_pin, Bit_SET);
    
    return SPI_OK;
}

/**
 * @brief 检查SPI是否忙
 */
uint8_t SPI_IsBusy(SPI_Instance_t instance)
{
    SPI_TypeDef *spi_periph;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return 0;
    }
    
    if (!g_spi_initialized[instance])
    {
        return 0;
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    
    return (SPI_GetFlagStatus(spi_periph, SPI_FLAG_BSY) != RESET) ? 1 : 0;
}

/**
 * @brief SPI软件复位
 */
SPI_Status_t SPI_SoftwareReset(SPI_Instance_t instance)
{
    SPI_TypeDef *spi_periph;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    
    /* 禁用SPI */
    SPI_Cmd(spi_periph, DISABLE);
    
    /* 清除所有错误标志 */
    SPI_ClearFlag(spi_periph, SPI_FLAG_OVR);
    SPI_ClearFlag(spi_periph, SPI_FLAG_MODF);
    SPI_ClearFlag(spi_periph, SPI_FLAG_CRCERR);
    
    /* 重新使能SPI */
    SPI_Cmd(spi_periph, ENABLE);
    
    return SPI_OK;
}

/**
 * @brief 获取SPI配置信息
 */
SPI_Status_t SPI_GetConfig(SPI_Instance_t instance, SPI_ConfigInfo_t *config_info)
{
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (config_info == NULL)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    config_info->spi_periph = g_spi_configs[instance].spi_periph;
    config_info->mode = g_spi_configs[instance].mode;
    config_info->direction = g_spi_configs[instance].direction;
    config_info->data_size = g_spi_configs[instance].data_size;
    config_info->cpol = g_spi_configs[instance].cpol;
    config_info->cpha = g_spi_configs[instance].cpha;
    config_info->nss = g_spi_configs[instance].nss;
    config_info->baudrate_prescaler = g_spi_configs[instance].baudrate_prescaler;
    config_info->first_bit = g_spi_configs[instance].first_bit;
    config_info->enabled = g_spi_configs[instance].enabled;
    
    return SPI_OK;
}

/* ========== 中断模式功能实现 ========== */

/**
 * @brief 获取SPI中断类型对应的SPL库中断值
 */
static uint8_t SPI_GetITValue(SPI_IT_t it_type)
{
    switch (it_type)
    {
        case SPI_IT_TXE:  return SPI_I2S_IT_TXE;
        case SPI_IT_RXNE: return SPI_I2S_IT_RXNE;
        case SPI_IT_ERR:  return SPI_I2S_IT_ERR;
        default: return 0;
    }
}

/**
 * @brief 获取SPI中断向量
 */
static IRQn_Type SPI_GetIRQn(SPI_Instance_t instance)
{
    switch (instance)
    {
        case SPI_INSTANCE_1: return SPI1_IRQn;
        case SPI_INSTANCE_2: return SPI2_IRQn;
        case SPI_INSTANCE_3: return SPI3_IRQn;
        default: return (IRQn_Type)0;
    }
}

/**
 * @brief 使能SPI中断
 */
SPI_Status_t SPI_EnableIT(SPI_Instance_t instance, SPI_IT_t it_type)
{
    SPI_TypeDef *spi_periph;
    uint8_t it_value;
    IRQn_Type irqn;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 3)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    it_value = SPI_GetITValue(it_type);
    if (it_value == 0)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    /* 使能SPI中断 */
    SPI_I2S_ITConfig(spi_periph, it_value, ENABLE);
    
    /* 使能NVIC中断 */
    irqn = SPI_GetIRQn(instance);
    if (irqn != 0)
    {
        NVIC_EnableIRQ(irqn);
    }
    
    return SPI_OK;
}

/**
 * @brief 禁用SPI中断
 */
SPI_Status_t SPI_DisableIT(SPI_Instance_t instance, SPI_IT_t it_type)
{
    SPI_TypeDef *spi_periph;
    uint8_t it_value;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 3)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    it_value = SPI_GetITValue(it_type);
    if (it_value == 0)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    /* 禁用SPI中断 */
    SPI_I2S_ITConfig(spi_periph, it_value, DISABLE);
    
    return SPI_OK;
}

/**
 * @brief 设置SPI中断回调函数
 */
SPI_Status_t SPI_SetITCallback(SPI_Instance_t instance, SPI_IT_t it_type,
                                SPI_IT_Callback_t callback, void *user_data)
{
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (it_type >= 3)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    g_spi_it_callbacks[instance][it_type] = callback;
    g_spi_it_user_data[instance][it_type] = user_data;
    
    return SPI_OK;
}

/**
 * @brief SPI非阻塞发送（中断模式）
 */
SPI_Status_t SPI_MasterTransmitIT(SPI_Instance_t instance, const uint8_t *data, uint16_t length)
{
    SPI_TypeDef *spi_periph;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (g_spi_tx_buffer[instance] != NULL)
    {
        return SPI_ERROR_BUSY;
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    
    /* 保存发送缓冲区信息 */
    g_spi_tx_buffer[instance] = data;
    g_spi_tx_length[instance] = length;
    g_spi_tx_index[instance] = 0;
    g_spi_rx_buffer[instance] = NULL;
    
    /* 使能TXE中断 */
    SPI_EnableIT(instance, SPI_IT_TXE);
    
    return SPI_OK;
}

/**
 * @brief SPI非阻塞接收（中断模式）
 */
SPI_Status_t SPI_MasterReceiveIT(SPI_Instance_t instance, uint8_t *data, uint16_t length)
{
    SPI_TypeDef *spi_periph;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (g_spi_rx_buffer[instance] != NULL)
    {
        return SPI_ERROR_BUSY;
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    
    /* 保存接收缓冲区信息 */
    g_spi_rx_buffer[instance] = data;
    g_spi_rx_max_length[instance] = length;
    g_spi_rx_index[instance] = 0;
    g_spi_rx_length[instance] = 0;
    g_spi_tx_buffer[instance] = NULL;
    
    /* 使能TXE和RXNE中断（发送dummy数据以接收） */
    SPI_EnableIT(instance, SPI_IT_TXE);
    SPI_EnableIT(instance, SPI_IT_RXNE);
    
    /* 发送第一个dummy字节启动传输 */
    SPI_I2S_SendData(spi_periph, 0xFF);
    
    return SPI_OK;
}

/**
 * @brief SPI非阻塞全双工传输（中断模式）
 */
SPI_Status_t SPI_MasterTransmitReceiveIT(SPI_Instance_t instance, const uint8_t *tx_data, uint8_t *rx_data, uint16_t length)
{
    SPI_TypeDef *spi_periph;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (tx_data == NULL || rx_data == NULL || length == 0)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (g_spi_tx_buffer[instance] != NULL || g_spi_rx_buffer[instance] != NULL)
    {
        return SPI_ERROR_BUSY;
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    
    /* 保存发送/接收缓冲区信息 */
    g_spi_tx_buffer[instance] = tx_data;
    g_spi_tx_length[instance] = length;
    g_spi_tx_index[instance] = 0;
    g_spi_rx_buffer[instance] = rx_data;
    g_spi_rx_max_length[instance] = length;
    g_spi_rx_index[instance] = 0;
    g_spi_rx_length[instance] = 0;
    
    /* 使能TXE和RXNE中断 */
    SPI_EnableIT(instance, SPI_IT_TXE);
    SPI_EnableIT(instance, SPI_IT_RXNE);
    
    /* 发送第一个字节启动传输 */
    SPI_I2S_SendData(spi_periph, tx_data[0]);
    g_spi_tx_index[instance]++;
    
    return SPI_OK;
}

/**
 * @brief 获取SPI发送剩余字节数（中断模式）
 */
uint16_t SPI_GetTransmitRemaining(SPI_Instance_t instance)
{
    if (instance >= SPI_INSTANCE_MAX || !g_spi_initialized[instance])
    {
        return 0;
    }
    
    if (g_spi_tx_buffer[instance] == NULL)
    {
        return 0;
    }
    
    return g_spi_tx_length[instance] - g_spi_tx_index[instance];
}

/**
 * @brief 获取SPI接收到的字节数（中断模式）
 */
uint16_t SPI_GetReceiveCount(SPI_Instance_t instance)
{
    if (instance >= SPI_INSTANCE_MAX || !g_spi_initialized[instance])
    {
        return 0;
    }
    
    return g_spi_rx_length[instance];
}

/**
 * @brief SPI中断服务函数
 */
void SPI_IRQHandler(SPI_Instance_t instance)
{
    SPI_TypeDef *spi_periph;
    uint8_t data;
    
    if (instance >= SPI_INSTANCE_MAX || !g_spi_initialized[instance])
    {
        return;
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    
    /* 处理TXE中断（发送缓冲区空） */
    if (SPI_I2S_GetITStatus(spi_periph, SPI_I2S_IT_TXE) != RESET)
    {
        if (g_spi_tx_buffer[instance] != NULL && g_spi_tx_index[instance] < g_spi_tx_length[instance])
        {
            /* 发送下一个字节 */
            SPI_I2S_SendData(spi_periph, g_spi_tx_buffer[instance][g_spi_tx_index[instance]++]);
        }
        else if (g_spi_tx_buffer[instance] != NULL)
        {
            /* 发送完成，禁用TXE中断 */
            SPI_DisableIT(instance, SPI_IT_TXE);
            g_spi_tx_buffer[instance] = NULL;
        }
        else if (g_spi_rx_buffer[instance] != NULL && g_spi_rx_index[instance] < g_spi_rx_max_length[instance])
        {
            /* 接收模式，发送dummy数据 */
            SPI_I2S_SendData(spi_periph, 0xFF);
        }
        else if (g_spi_rx_buffer[instance] != NULL)
        {
            /* 接收完成，禁用TXE中断 */
            SPI_DisableIT(instance, SPI_IT_TXE);
        }
        
        /* 调用回调函数 */
        if (g_spi_it_callbacks[instance][SPI_IT_TXE] != NULL)
        {
            g_spi_it_callbacks[instance][SPI_IT_TXE](instance, SPI_IT_TXE, g_spi_it_user_data[instance][SPI_IT_TXE]);
        }
    }
    
    /* 处理RXNE中断（接收缓冲区非空） */
    if (SPI_I2S_GetITStatus(spi_periph, SPI_I2S_IT_RXNE) != RESET)
    {
        data = SPI_I2S_ReceiveData(spi_periph);
        
        if (g_spi_rx_buffer[instance] != NULL && g_spi_rx_index[instance] < g_spi_rx_max_length[instance])
        {
            g_spi_rx_buffer[instance][g_spi_rx_index[instance]++] = data;
            g_spi_rx_length[instance] = g_spi_rx_index[instance];
            
            if (g_spi_rx_index[instance] >= g_spi_rx_max_length[instance])
            {
                /* 接收完成，禁用RXNE中断 */
                SPI_DisableIT(instance, SPI_IT_RXNE);
                g_spi_rx_buffer[instance] = NULL;
            }
        }
        
        /* 调用回调函数 */
        if (g_spi_it_callbacks[instance][SPI_IT_RXNE] != NULL)
        {
            g_spi_it_callbacks[instance][SPI_IT_RXNE](instance, SPI_IT_RXNE, g_spi_it_user_data[instance][SPI_IT_RXNE]);
        }
    }
    
    /* 处理错误中断 */
    if (SPI_I2S_GetITStatus(spi_periph, SPI_I2S_IT_OVR) != RESET)
    {
        SPI_I2S_ClearITPendingBit(spi_periph, SPI_I2S_IT_OVR);
        
        /* 调用回调函数 */
        if (g_spi_it_callbacks[instance][SPI_IT_ERR] != NULL)
        {
            g_spi_it_callbacks[instance][SPI_IT_ERR](instance, SPI_IT_ERR, g_spi_it_user_data[instance][SPI_IT_ERR]);
        }
    }
    
    if (SPI_I2S_GetITStatus(spi_periph, SPI_IT_MODF) != RESET)
    {
        SPI_I2S_ClearITPendingBit(spi_periph, SPI_IT_MODF);
        
        /* 调用回调函数 */
        if (g_spi_it_callbacks[instance][SPI_IT_ERR] != NULL)
        {
            g_spi_it_callbacks[instance][SPI_IT_ERR](instance, SPI_IT_ERR, g_spi_it_user_data[instance][SPI_IT_ERR]);
        }
    }
    
    if (SPI_I2S_GetITStatus(spi_periph, SPI_IT_CRCERR) != RESET)
    {
        SPI_I2S_ClearITPendingBit(spi_periph, SPI_IT_CRCERR);
        
        /* 调用回调函数 */
        if (g_spi_it_callbacks[instance][SPI_IT_ERR] != NULL)
        {
            g_spi_it_callbacks[instance][SPI_IT_ERR](instance, SPI_IT_ERR, g_spi_it_user_data[instance][SPI_IT_ERR]);
        }
    }
}

/* SPI中断服务程序入口 */
void SPI1_IRQHandler(void)
{
    SPI_IRQHandler(SPI_INSTANCE_1);
}

void SPI2_IRQHandler(void)
{
    SPI_IRQHandler(SPI_INSTANCE_2);
}

void SPI3_IRQHandler(void)
{
    SPI_IRQHandler(SPI_INSTANCE_3);
}

/* ========== DMA模式功能实现 ========== */

/* DMA通道映射（SPI TX/RX对应的DMA通道） */
/* 注意：SPI1使用DMA2_CH3(TX)/CH2(RX)，SPI2使用DMA1_CH4(TX)/CH5(RX)，SPI3使用DMA2_CH5(TX)/CH4(RX) */
static const DMA_Channel_t spi_tx_dma_channels[SPI_INSTANCE_MAX] = {
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

static const DMA_Channel_t spi_rx_dma_channels[SPI_INSTANCE_MAX] = {
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
    SPI_TypeDef *spi_periph;
    DMA_Channel_t dma_channel;
    DMA_Status_t dma_status;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    dma_channel = spi_tx_dma_channels[instance];
    if (dma_channel >= DMA_CHANNEL_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;  /* 该SPI实例不支持DMA */
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    
    /* 检查DMA通道是否已初始化 */
    if (!DMA_IsInitialized(dma_channel))
    {
        dma_status = DMA_Init(dma_channel);
        if (dma_status != DMA_OK)
        {
            return SPI_ERROR_INVALID_PARAM;
        }
    }
    
    /* 停止之前的传输 */
    DMA_Stop(dma_channel);
    
    /* 配置DMA传输（内存到外设） */
    dma_status = DMA_ConfigTransfer(dma_channel, (uint32_t)&spi_periph->DR,
                                    (uint32_t)data, length,
                                    DMA_DIR_MEMORY_TO_PERIPHERAL, 1);
    if (dma_status != DMA_OK)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    /* 使能SPI DMA发送请求 */
    SPI_I2S_DMACmd(spi_periph, SPI_I2S_DMAReq_Tx, ENABLE);
    
    /* 启动DMA传输 */
    dma_status = DMA_Start(dma_channel);
    if (dma_status != DMA_OK)
    {
        SPI_I2S_DMACmd(spi_periph, SPI_I2S_DMAReq_Tx, DISABLE);
        return SPI_ERROR_INVALID_PARAM;
    }
    
    return SPI_OK;
}

/**
 * @brief SPI DMA接收
 */
SPI_Status_t SPI_MasterReceiveDMA(SPI_Instance_t instance, uint8_t *data, uint16_t length)
{
    SPI_TypeDef *spi_periph;
    DMA_Channel_t dma_channel;
    DMA_Status_t dma_status;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    dma_channel = spi_rx_dma_channels[instance];
    if (dma_channel >= DMA_CHANNEL_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;  /* 该SPI实例不支持DMA */
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    
    /* 检查DMA通道是否已初始化 */
    if (!DMA_IsInitialized(dma_channel))
    {
        dma_status = DMA_Init(dma_channel);
        if (dma_status != DMA_OK)
        {
            return SPI_ERROR_INVALID_PARAM;
        }
    }
    
    /* 停止之前的传输 */
    DMA_Stop(dma_channel);
    
    /* 配置DMA传输（外设到内存） */
    dma_status = DMA_ConfigTransfer(dma_channel, (uint32_t)&spi_periph->DR,
                                    (uint32_t)data, length,
                                    DMA_DIR_PERIPHERAL_TO_MEMORY, 1);
    if (dma_status != DMA_OK)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    /* 使能SPI DMA接收请求 */
    SPI_I2S_DMACmd(spi_periph, SPI_I2S_DMAReq_Rx, ENABLE);
    
    /* 启动DMA传输（需要先发送dummy数据启动SPI接收） */
    SPI_I2S_SendData(spi_periph, 0xFF);
    
    dma_status = DMA_Start(dma_channel);
    if (dma_status != DMA_OK)
    {
        SPI_I2S_DMACmd(spi_periph, SPI_I2S_DMAReq_Rx, DISABLE);
        return SPI_ERROR_INVALID_PARAM;
    }
    
    return SPI_OK;
}

/**
 * @brief SPI DMA全双工传输
 */
SPI_Status_t SPI_MasterTransmitReceiveDMA(SPI_Instance_t instance, const uint8_t *tx_data, uint8_t *rx_data, uint16_t length)
{
    SPI_TypeDef *spi_periph;
    DMA_Channel_t tx_dma_channel, rx_dma_channel;
    DMA_Status_t dma_status;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (tx_data == NULL || rx_data == NULL || length == 0)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    tx_dma_channel = spi_tx_dma_channels[instance];
    rx_dma_channel = spi_rx_dma_channels[instance];
    if (tx_dma_channel >= DMA_CHANNEL_MAX || rx_dma_channel >= DMA_CHANNEL_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;  /* 该SPI实例不支持DMA */
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    
    /* 检查DMA通道是否已初始化 */
    if (!DMA_IsInitialized(tx_dma_channel))
    {
        dma_status = DMA_Init(tx_dma_channel);
        if (dma_status != DMA_OK)
        {
            return SPI_ERROR_INVALID_PARAM;
        }
    }
    
    if (!DMA_IsInitialized(rx_dma_channel))
    {
        dma_status = DMA_Init(rx_dma_channel);
        if (dma_status != DMA_OK)
        {
            return SPI_ERROR_INVALID_PARAM;
        }
    }
    
    /* 停止之前的传输 */
    DMA_Stop(tx_dma_channel);
    DMA_Stop(rx_dma_channel);
    
    /* 配置发送DMA传输（内存到外设） */
    dma_status = DMA_ConfigTransfer(tx_dma_channel, (uint32_t)&spi_periph->DR,
                                    (uint32_t)tx_data, length,
                                    DMA_DIR_MEMORY_TO_PERIPHERAL, 1);
    if (dma_status != DMA_OK)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    /* 配置接收DMA传输（外设到内存） */
    dma_status = DMA_ConfigTransfer(rx_dma_channel, (uint32_t)&spi_periph->DR,
                                    (uint32_t)rx_data, length,
                                    DMA_DIR_PERIPHERAL_TO_MEMORY, 1);
    if (dma_status != DMA_OK)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    /* 使能SPI DMA发送和接收请求 */
    SPI_I2S_DMACmd(spi_periph, SPI_I2S_DMAReq_Tx, ENABLE);
    SPI_I2S_DMACmd(spi_periph, SPI_I2S_DMAReq_Rx, ENABLE);
    
    /* 启动DMA传输 */
    dma_status = DMA_Start(rx_dma_channel);
    if (dma_status != DMA_OK)
    {
        SPI_I2S_DMACmd(spi_periph, SPI_I2S_DMAReq_Tx, DISABLE);
        SPI_I2S_DMACmd(spi_periph, SPI_I2S_DMAReq_Rx, DISABLE);
        return SPI_ERROR_INVALID_PARAM;
    }
    
    dma_status = DMA_Start(tx_dma_channel);
    if (dma_status != DMA_OK)
    {
        DMA_Stop(rx_dma_channel);
        SPI_I2S_DMACmd(spi_periph, SPI_I2S_DMAReq_Tx, DISABLE);
        SPI_I2S_DMACmd(spi_periph, SPI_I2S_DMAReq_Rx, DISABLE);
        return SPI_ERROR_INVALID_PARAM;
    }
    
    return SPI_OK;
}

/**
 * @brief 停止SPI DMA发送
 */
SPI_Status_t SPI_StopTransmitDMA(SPI_Instance_t instance)
{
    SPI_TypeDef *spi_periph;
    DMA_Channel_t dma_channel;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    dma_channel = spi_tx_dma_channels[instance];
    if (dma_channel >= DMA_CHANNEL_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    
    /* 禁用SPI DMA发送请求 */
    SPI_I2S_DMACmd(spi_periph, SPI_I2S_DMAReq_Tx, DISABLE);
    
    /* 停止DMA传输 */
    DMA_Stop(dma_channel);
    
    return SPI_OK;
}

/**
 * @brief 停止SPI DMA接收
 */
SPI_Status_t SPI_StopReceiveDMA(SPI_Instance_t instance)
{
    SPI_TypeDef *spi_periph;
    DMA_Channel_t dma_channel;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    dma_channel = spi_rx_dma_channels[instance];
    if (dma_channel >= DMA_CHANNEL_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    
    /* 禁用SPI DMA接收请求 */
    SPI_I2S_DMACmd(spi_periph, SPI_I2S_DMAReq_Rx, DISABLE);
    
    /* 停止DMA传输 */
    DMA_Stop(dma_channel);
    
    return SPI_OK;
}

/* ========== 从模式功能实现 ========== */

/**
 * @brief SPI从模式发送数据（8位）
 */
SPI_Status_t SPI_SlaveTransmit(SPI_Instance_t instance, const uint8_t *data, uint16_t length, uint32_t timeout)
{
    SPI_TypeDef *spi_periph;
    SPI_Status_t status;
    uint16_t i;
    uint32_t actual_timeout;
    
    if (instance >= SPI_INSTANCE_MAX || !g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (g_spi_configs[instance].data_size != SPI_DataSize_8b)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (g_spi_configs[instance].mode != SPI_Mode_Slave)
    {
        return SPI_ERROR_MODE_FAULT;
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    actual_timeout = (timeout == 0) ? SPI_DEFAULT_TIMEOUT_MS : timeout;
    
    for (i = 0; i < length; i++)
    {
        status = SPI_WaitFlag(spi_periph, SPI_FLAG_TXE, actual_timeout);
        if (status != SPI_OK)
        {
            return status;
        }
        
        SPI_SendData8(spi_periph, data[i]);
        
        status = SPI_WaitFlag(spi_periph, SPI_FLAG_RXNE, actual_timeout);
        if (status != SPI_OK)
        {
            return status;
        }
        
        SPI_ReceiveData8(spi_periph);
    }
    
    return SPI_OK;
}

/**
 * @brief SPI从模式接收数据（8位）
 */
SPI_Status_t SPI_SlaveReceive(SPI_Instance_t instance, uint8_t *data, uint16_t length, uint32_t timeout)
{
    SPI_TypeDef *spi_periph;
    SPI_Status_t status;
    uint16_t i;
    uint32_t actual_timeout;
    
    if (instance >= SPI_INSTANCE_MAX || !g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (g_spi_configs[instance].data_size != SPI_DataSize_8b)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (g_spi_configs[instance].mode != SPI_Mode_Slave)
    {
        return SPI_ERROR_MODE_FAULT;
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    actual_timeout = (timeout == 0) ? SPI_DEFAULT_TIMEOUT_MS : timeout;
    
    for (i = 0; i < length; i++)
    {
        status = SPI_WaitFlag(spi_periph, SPI_FLAG_TXE, actual_timeout);
        if (status != SPI_OK)
        {
            return status;
        }
        
        SPI_SendData8(spi_periph, 0xFF);
        
        status = SPI_WaitFlag(spi_periph, SPI_FLAG_RXNE, actual_timeout);
        if (status != SPI_OK)
        {
            return status;
        }
        
        data[i] = SPI_ReceiveData8(spi_periph);
    }
    
    return SPI_OK;
}

/**
 * @brief SPI从模式全双工传输（8位）
 */
SPI_Status_t SPI_SlaveTransmitReceive(SPI_Instance_t instance, const uint8_t *tx_data, uint8_t *rx_data, uint16_t length, uint32_t timeout)
{
    SPI_TypeDef *spi_periph;
    SPI_Status_t status;
    uint16_t i;
    uint32_t actual_timeout;
    
    if (instance >= SPI_INSTANCE_MAX || !g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (tx_data == NULL || rx_data == NULL || length == 0)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (g_spi_configs[instance].data_size != SPI_DataSize_8b)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (g_spi_configs[instance].mode != SPI_Mode_Slave)
    {
        return SPI_ERROR_MODE_FAULT;
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    actual_timeout = (timeout == 0) ? SPI_DEFAULT_TIMEOUT_MS : timeout;
    
    for (i = 0; i < length; i++)
    {
        status = SPI_WaitFlag(spi_periph, SPI_FLAG_TXE, actual_timeout);
        if (status != SPI_OK)
        {
            return status;
        }
        
        SPI_SendData8(spi_periph, tx_data[i]);
        
        status = SPI_WaitFlag(spi_periph, SPI_FLAG_RXNE, actual_timeout);
        if (status != SPI_OK)
        {
            return status;
        }
        
        rx_data[i] = SPI_ReceiveData8(spi_periph);
    }
    
    return SPI_OK;
}

/* ========== TI模式和硬件NSS管理功能实现 ========== */

/**
 * @brief 使能SPI TI模式
 */
SPI_Status_t SPI_EnableTIMode(SPI_Instance_t instance)
{
    SPI_TypeDef *spi_periph;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    
    /* 使能TI模式（SSI模式） */
    SPI_TI_ModeCmd(spi_periph, ENABLE);
    
    return SPI_OK;
}

/**
 * @brief 禁用SPI TI模式
 */
SPI_Status_t SPI_DisableTIMode(SPI_Instance_t instance)
{
    SPI_TypeDef *spi_periph;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    
    /* 禁用TI模式 */
    SPI_TI_ModeCmd(spi_periph, DISABLE);
    
    return SPI_OK;
}

/**
 * @brief 配置SPI硬件NSS内部软件管理
 */
SPI_Status_t SPI_ConfigHardwareNSS(SPI_Instance_t instance, uint8_t nss_level)
{
    SPI_TypeDef *spi_periph;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (g_spi_configs[instance].nss != SPI_NSS_Hard)
    {
        return SPI_ERROR_INVALID_PARAM;  /* 不是硬件NSS模式 */
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    
    /* 配置硬件NSS内部软件管理 */
    if (nss_level == 0)
    {
        SPI_NSSInternalSoftwareConfig(spi_periph, SPI_NSSInternalSoft_Reset);
    }
    else
    {
        SPI_NSSInternalSoftwareConfig(spi_periph, SPI_NSSInternalSoft_Set);
    }
    
    return SPI_OK;
}

/**
 * @brief 配置SPI CRC计算
 */
SPI_Status_t SPI_ConfigCRC(SPI_Instance_t instance, uint16_t polynomial)
{
    SPI_TypeDef *spi_periph;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    if (polynomial < 7 || polynomial > 65535)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    
    /* 禁用SPI */
    SPI_Cmd(spi_periph, DISABLE);
    
    /* 设置CRC多项式 */
    SPI_SetCRCPolynomial(spi_periph, polynomial);
    
    /* 重新使能SPI */
    SPI_Cmd(spi_periph, ENABLE);
    
    return SPI_OK;
}

/**
 * @brief 使能SPI CRC计算
 */
SPI_Status_t SPI_EnableCRC(SPI_Instance_t instance)
{
    SPI_TypeDef *spi_periph;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    
    /* 使能CRC计算 */
    SPI_CalculateCRC(spi_periph, ENABLE);
    
    return SPI_OK;
}

/**
 * @brief 禁用SPI CRC计算
 */
SPI_Status_t SPI_DisableCRC(SPI_Instance_t instance)
{
    SPI_TypeDef *spi_periph;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return SPI_ERROR_INVALID_PARAM;
    }
    
    if (!g_spi_initialized[instance])
    {
        return SPI_ERROR_NOT_INITIALIZED;
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    
    /* 禁用CRC计算 */
    SPI_CalculateCRC(spi_periph, DISABLE);
    
    return SPI_OK;
}

/**
 * @brief 获取SPI CRC值
 */
uint16_t SPI_GetCRC(SPI_Instance_t instance)
{
    SPI_TypeDef *spi_periph;
    
    if (instance >= SPI_INSTANCE_MAX)
    {
        return 0;
    }
    
    if (!g_spi_initialized[instance])
    {
        return 0;
    }
    
    spi_periph = g_spi_configs[instance].spi_periph;
    
    /* 读取CRC寄存器值 */
    return SPI_GetCRC(spi_periph, SPI_CRC_Rx);
}

#endif /* CONFIG_MODULE_SPI_ENABLED */

