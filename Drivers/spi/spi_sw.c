/**
 * @file spi_sw.c
 * @brief 软件SPI驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于GPIO的软件SPI驱动，支持主模式、8位/16位数据格式、多种SPI模式等功能
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_SOFT_SPI_ENABLED

#include "spi_sw.h"
#include "gpio.h"
#include "delay.h"
#include <stdbool.h>
#include <stddef.h>

/* 软件SPI配置表（从board.h中获取） */
#define SOFT_SPI_CONFIGS_DECLARE \
    static const SoftSPI_Config_t g_soft_spi_configs[SPI_SW_INSTANCE_MAX] = SOFT_SPI_CONFIGS;

SOFT_SPI_CONFIGS_DECLARE

/* 初始化状态标志 */
static bool g_soft_spi_initialized[SPI_SW_INSTANCE_MAX] = {false, false, false, false};

/* 参数校验宏 */
#define SPI_SW_CHECK_INIT(inst) \
    do { \
        if ((inst) >= SPI_SW_INSTANCE_MAX) \
            return SPI_SW_ERROR_INVALID_PARAM; \
        if (!g_soft_spi_initialized[inst]) \
            return SPI_SW_ERROR_NOT_INITIALIZED; \
    } while(0)

#define SPI_SW_CHECK_PARAM(ptr) \
    do { \
        if ((ptr) == NULL) \
            return SPI_SW_ERROR_INVALID_PARAM; \
    } while(0)

/* ==================== 内部函数 ==================== */

/**
 * @brief 软件SPI NSS控制（拉低NSS，内部函数）
 * @param[in] instance 软件SPI实例索引
 */
static inline void SPI_SW_NSS_Low_Internal(SPI_SW_Instance_t instance)
{
    if (g_soft_spi_configs[instance].nss_port != NULL && 
        g_soft_spi_configs[instance].nss_pin != 0)
    {
        GPIO_WritePin(g_soft_spi_configs[instance].nss_port, 
                     g_soft_spi_configs[instance].nss_pin, Bit_RESET);
    }
}

/**
 * @brief 软件SPI NSS控制（拉高NSS，内部函数）
 * @param[in] instance 软件SPI实例索引
 */
static inline void SPI_SW_NSS_High_Internal(SPI_SW_Instance_t instance)
{
    if (g_soft_spi_configs[instance].nss_port != NULL && 
        g_soft_spi_configs[instance].nss_pin != 0)
    {
        GPIO_WritePin(g_soft_spi_configs[instance].nss_port, 
                     g_soft_spi_configs[instance].nss_pin, Bit_SET);
    }
}

/**
 * @brief 软件SPI延时
 * @param[in] instance 软件SPI实例索引
 * @param[in] delay_us 延时时间（微秒）
 */
static inline void SPI_SW_Delay(SPI_SW_Instance_t instance, uint32_t delay_us)
{
    if (delay_us > 0)
    {
        Delay_us(delay_us);
    }
}

/**
 * @brief 设置SCK引脚电平
 * @param[in] instance 软件SPI实例索引
 * @param[in] level 电平（Bit_SET或Bit_RESET）
 */
static inline void SPI_SW_SetSCK(SPI_SW_Instance_t instance, BitAction level)
{
    GPIO_WritePin(g_soft_spi_configs[instance].sck_port, 
                  g_soft_spi_configs[instance].sck_pin, level);
}

/**
 * @brief 设置MOSI引脚电平
 * @param[in] instance 软件SPI实例索引
 * @param[in] level 电平（Bit_SET或Bit_RESET）
 */
static inline void SPI_SW_SetMOSI(SPI_SW_Instance_t instance, BitAction level)
{
    GPIO_WritePin(g_soft_spi_configs[instance].mosi_port, 
                  g_soft_spi_configs[instance].mosi_pin, level);
}

/**
 * @brief 读取MISO引脚电平
 * @param[in] instance 软件SPI实例索引
 * @return uint8_t 1=高电平，0=低电平
 */
static inline uint8_t SPI_SW_ReadMISO(SPI_SW_Instance_t instance)
{
    return GPIO_ReadPin(g_soft_spi_configs[instance].miso_port, 
                       g_soft_spi_configs[instance].miso_pin);
}


/**
 * @brief 发送一个字节（8位）
 * @param[in] instance 软件SPI实例索引
 * @param[in] data 要发送的数据
 * @return uint8_t 接收到的数据
 */
static uint8_t SPI_SW_TransferByte(SPI_SW_Instance_t instance, uint8_t data)
{
    uint8_t first_bit = g_soft_spi_configs[instance].first_bit;
    uint8_t cpha = g_soft_spi_configs[instance].cpha;
    uint8_t cpol = g_soft_spi_configs[instance].cpol;
    uint32_t delay = g_soft_spi_configs[instance].delay_us;
    uint8_t i;
    uint8_t rx_data = 0;
    BitAction idle_level = cpol ? Bit_SET : Bit_RESET;
    BitAction active_level = cpol ? Bit_RESET : Bit_SET;
    
    if (first_bit == 0)
    {
        /* MSB优先 */
        for (i = 0; i < 8; i++)
        {
            if (cpha == 0)
            {
                /* CPHA=0: 在第一个边沿采样，第二个边沿准备 */
                /* 设置MOSI数据位（在时钟变化前） */
                SPI_SW_SetMOSI(instance, (data & 0x80) ? Bit_SET : Bit_RESET);
                data <<= 1;
                SPI_SW_Delay(instance, delay);
                
                /* 第一个边沿：从空闲到活动，采样数据 */
                SPI_SW_SetSCK(instance, active_level);
                SPI_SW_Delay(instance, delay);
                rx_data <<= 1;
                rx_data |= SPI_SW_ReadMISO(instance);
                
                /* 第二个边沿：从活动到空闲，准备下一个数据 */
                SPI_SW_SetSCK(instance, idle_level);
                SPI_SW_Delay(instance, delay);
            }
            else
            {
                /* CPHA=1: 在第一个边沿准备，第二个边沿采样 */
                /* 设置MOSI数据位（在时钟变化前） */
                SPI_SW_SetMOSI(instance, (data & 0x80) ? Bit_SET : Bit_RESET);
                data <<= 1;
                SPI_SW_Delay(instance, delay);
                
                /* 第一个边沿：从空闲到活动，准备数据 */
                SPI_SW_SetSCK(instance, active_level);
                SPI_SW_Delay(instance, delay);
                
                /* 第二个边沿：从活动到空闲，采样数据 */
                SPI_SW_SetSCK(instance, idle_level);
                SPI_SW_Delay(instance, delay);
                rx_data <<= 1;
                rx_data |= SPI_SW_ReadMISO(instance);
            }
        }
    }
    else
    {
        /* LSB优先 */
        for (i = 0; i < 8; i++)
        {
            if (cpha == 0)
            {
                /* CPHA=0: 在第一个边沿采样，第二个边沿准备 */
                /* 设置MOSI数据位（在时钟变化前） */
                SPI_SW_SetMOSI(instance, (data & 0x01) ? Bit_SET : Bit_RESET);
                data >>= 1;
                SPI_SW_Delay(instance, delay);
                
                /* 第一个边沿：从空闲到活动，采样数据 */
                SPI_SW_SetSCK(instance, active_level);
                SPI_SW_Delay(instance, delay);
                rx_data |= ((uint8_t)SPI_SW_ReadMISO(instance) << i);
                
                /* 第二个边沿：从活动到空闲，准备下一个数据 */
                SPI_SW_SetSCK(instance, idle_level);
                SPI_SW_Delay(instance, delay);
            }
            else
            {
                /* CPHA=1: 在第一个边沿准备，第二个边沿采样 */
                /* 设置MOSI数据位（在时钟变化前） */
                SPI_SW_SetMOSI(instance, (data & 0x01) ? Bit_SET : Bit_RESET);
                data >>= 1;
                SPI_SW_Delay(instance, delay);
                
                /* 第一个边沿：从空闲到活动，准备数据 */
                SPI_SW_SetSCK(instance, active_level);
                SPI_SW_Delay(instance, delay);
                
                /* 第二个边沿：从活动到空闲，采样数据 */
                SPI_SW_SetSCK(instance, idle_level);
                SPI_SW_Delay(instance, delay);
                rx_data |= ((uint8_t)SPI_SW_ReadMISO(instance) << i);
            }
        }
    }
    
    return rx_data;
}

/**
 * @brief 发送一个16位数据
 * @param[in] instance 软件SPI实例索引
 * @param[in] data 要发送的数据
 * @return uint16_t 接收到的数据
 */
static uint16_t SPI_SW_Transfer16(SPI_SW_Instance_t instance, uint16_t data)
{
    uint8_t first_bit = g_soft_spi_configs[instance].first_bit;
    uint16_t rx_data = 0;
    
    if (first_bit == 0)
    {
        /* MSB优先：先发送高字节 */
        uint8_t high_byte = (uint8_t)(data >> 8);
        uint8_t low_byte = (uint8_t)(data & 0xFF);
        uint8_t rx_high = SPI_SW_TransferByte(instance, high_byte);
        uint8_t rx_low = SPI_SW_TransferByte(instance, low_byte);
        rx_data = ((uint16_t)rx_high << 8) | rx_low;
    }
    else
    {
        /* LSB优先：先发送低字节 */
        uint8_t high_byte = (uint8_t)(data >> 8);
        uint8_t low_byte = (uint8_t)(data & 0xFF);
        uint8_t rx_low = SPI_SW_TransferByte(instance, low_byte);
        uint8_t rx_high = SPI_SW_TransferByte(instance, high_byte);
        rx_data = ((uint16_t)rx_high << 8) | rx_low;
    }
    
    return rx_data;
}

/* ==================== 公共函数 ==================== */

/**
 * @brief 软件SPI初始化
 */
SPI_SW_Status_t SPI_SW_Init(SPI_SW_Instance_t instance)
{
    GPIO_Status_t gpio_status;
    GPIO_InitTypeDef GPIO_InitStructure;
    
    /* 参数校验 */
    if (instance >= SPI_SW_INSTANCE_MAX)
    {
        return SPI_SW_ERROR_INVALID_PARAM;
    }
    
    if (!g_soft_spi_configs[instance].enabled)
    {
        return SPI_SW_ERROR_INVALID_PARAM;
    }
    
    /* 检查端口指针是否为NULL */
    if (g_soft_spi_configs[instance].sck_port == NULL ||
        g_soft_spi_configs[instance].miso_port == NULL ||
        g_soft_spi_configs[instance].mosi_port == NULL)
    {
        return SPI_SW_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已初始化 */
    if (g_soft_spi_initialized[instance])
    {
        return SPI_SW_OK;
    }
    
    /* 使能GPIO时钟 */
    gpio_status = GPIO_EnableClock(g_soft_spi_configs[instance].sck_port);
    if (gpio_status != GPIO_OK)
    {
        return SPI_SW_ERROR_GPIO_FAILED;
    }
    
    if (g_soft_spi_configs[instance].miso_port != g_soft_spi_configs[instance].sck_port)
    {
        gpio_status = GPIO_EnableClock(g_soft_spi_configs[instance].miso_port);
        if (gpio_status != GPIO_OK)
        {
            return SPI_SW_ERROR_GPIO_FAILED;
        }
    }
    
    if (g_soft_spi_configs[instance].mosi_port != g_soft_spi_configs[instance].sck_port &&
        g_soft_spi_configs[instance].mosi_port != g_soft_spi_configs[instance].miso_port)
    {
        gpio_status = GPIO_EnableClock(g_soft_spi_configs[instance].mosi_port);
        if (gpio_status != GPIO_OK)
        {
            return SPI_SW_ERROR_GPIO_FAILED;
        }
    }
    
    if (g_soft_spi_configs[instance].nss_port != NULL)
    {
        if (g_soft_spi_configs[instance].nss_port != g_soft_spi_configs[instance].sck_port &&
            g_soft_spi_configs[instance].nss_port != g_soft_spi_configs[instance].miso_port &&
            g_soft_spi_configs[instance].nss_port != g_soft_spi_configs[instance].mosi_port)
        {
            gpio_status = GPIO_EnableClock(g_soft_spi_configs[instance].nss_port);
            if (gpio_status != GPIO_OK)
            {
                return SPI_SW_ERROR_GPIO_FAILED;
            }
        }
    }
    
    /* 配置SCK引脚为推挽输出 */
    GPIO_InitStructure.GPIO_Pin = g_soft_spi_configs[instance].sck_pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(g_soft_spi_configs[instance].sck_port, &GPIO_InitStructure);
    
    /* 配置MISO引脚为浮空输入 */
    GPIO_InitStructure.GPIO_Pin = g_soft_spi_configs[instance].miso_pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(g_soft_spi_configs[instance].miso_port, &GPIO_InitStructure);
    
    /* 配置MOSI引脚为推挽输出 */
    GPIO_InitStructure.GPIO_Pin = g_soft_spi_configs[instance].mosi_pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(g_soft_spi_configs[instance].mosi_port, &GPIO_InitStructure);
    
    /* 配置NSS引脚（如果使用） */
    if (g_soft_spi_configs[instance].nss_port != NULL && 
        g_soft_spi_configs[instance].nss_pin != 0)
    {
        GPIO_InitStructure.GPIO_Pin = g_soft_spi_configs[instance].nss_pin;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(g_soft_spi_configs[instance].nss_port, &GPIO_InitStructure);
        /* 默认拉高NSS */
        GPIO_WritePin(g_soft_spi_configs[instance].nss_port, 
                     g_soft_spi_configs[instance].nss_pin, Bit_SET);
    }
    
    /* 初始化SCK为空闲电平 */
    SPI_SW_SetSCK(instance, g_soft_spi_configs[instance].cpol ? Bit_SET : Bit_RESET);
    
    /* 标记为已初始化 */
    g_soft_spi_initialized[instance] = true;
    
    return SPI_SW_OK;
}

/**
 * @brief 软件SPI反初始化
 */
SPI_SW_Status_t SPI_SW_Deinit(SPI_SW_Instance_t instance)
{
    /* 参数校验 */
    if (instance >= SPI_SW_INSTANCE_MAX)
    {
        return SPI_SW_ERROR_INVALID_PARAM;
    }
    
    if (!g_soft_spi_initialized[instance])
    {
        return SPI_SW_OK;
    }
    
    /* 标记为未初始化 */
    g_soft_spi_initialized[instance] = false;
    
    return SPI_SW_OK;
}

/**
 * @brief 检查软件SPI是否已初始化
 */
uint8_t SPI_SW_IsInitialized(SPI_SW_Instance_t instance)
{
    if (instance >= SPI_SW_INSTANCE_MAX)
    {
        return 0;
    }
    
    return g_soft_spi_initialized[instance] ? 1 : 0;
}

/**
 * @brief 软件SPI主模式发送数据（8位）
 */
SPI_SW_Status_t SPI_SW_MasterTransmit(SPI_SW_Instance_t instance, const uint8_t *data, uint16_t length)
{
    uint16_t i;
    
    SPI_SW_CHECK_INIT(instance);
    SPI_SW_CHECK_PARAM(data);
    
    if (length == 0)
    {
        return SPI_SW_ERROR_INVALID_PARAM;
    }
    
    /* 拉低NSS */
    SPI_SW_NSS_Low_Internal(instance);
    
    /* 发送数据 */
    for (i = 0; i < length; i++)
    {
        SPI_SW_TransferByte(instance, data[i]);
    }
    
    /* 拉高NSS */
    SPI_SW_NSS_High_Internal(instance);
    
    return SPI_SW_OK;
}

/**
 * @brief 软件SPI主模式接收数据（8位）
 */
SPI_SW_Status_t SPI_SW_MasterReceive(SPI_SW_Instance_t instance, uint8_t *data, uint16_t length)
{
    uint16_t i;
    
    SPI_SW_CHECK_INIT(instance);
    SPI_SW_CHECK_PARAM(data);
    
    if (length == 0)
    {
        return SPI_SW_ERROR_INVALID_PARAM;
    }
    
    /* 拉低NSS */
    SPI_SW_NSS_Low_Internal(instance);
    
    /* 接收数据（发送dummy数据0xFF来产生时钟） */
    for (i = 0; i < length; i++)
    {
        data[i] = SPI_SW_TransferByte(instance, 0xFF);
    }
    
    /* 拉高NSS */
    SPI_SW_NSS_High_Internal(instance);
    
    return SPI_SW_OK;
}

/**
 * @brief 软件SPI主模式全双工传输（8位）
 */
SPI_SW_Status_t SPI_SW_MasterTransmitReceive(SPI_SW_Instance_t instance, const uint8_t *tx_data, uint8_t *rx_data, uint16_t length)
{
    uint16_t i;
    
    SPI_SW_CHECK_INIT(instance);
    SPI_SW_CHECK_PARAM(tx_data);
    SPI_SW_CHECK_PARAM(rx_data);
    
    if (length == 0)
    {
        return SPI_SW_ERROR_INVALID_PARAM;
    }
    
    /* 拉低NSS */
    SPI_SW_NSS_Low_Internal(instance);
    
    /* 全双工传输 */
    for (i = 0; i < length; i++)
    {
        rx_data[i] = SPI_SW_TransferByte(instance, tx_data[i]);
    }
    
    /* 拉高NSS */
    SPI_SW_NSS_High_Internal(instance);
    
    return SPI_SW_OK;
}

/**
 * @brief 软件SPI主模式发送单个字节
 */
SPI_SW_Status_t SPI_SW_MasterTransmitByte(SPI_SW_Instance_t instance, uint8_t data)
{
    return SPI_SW_MasterTransmit(instance, &data, 1);
}

/**
 * @brief 软件SPI主模式接收单个字节
 */
SPI_SW_Status_t SPI_SW_MasterReceiveByte(SPI_SW_Instance_t instance, uint8_t *data)
{
    return SPI_SW_MasterReceive(instance, data, 1);
}

/**
 * @brief 软件SPI主模式发送数据（16位）
 */
SPI_SW_Status_t SPI_SW_MasterTransmit16(SPI_SW_Instance_t instance, const uint16_t *data, uint16_t length)
{
    uint16_t i;
    
    SPI_SW_CHECK_INIT(instance);
    SPI_SW_CHECK_PARAM(data);
    
    if (length == 0)
    {
        return SPI_SW_ERROR_INVALID_PARAM;
    }
    
    /* 拉低NSS */
    SPI_SW_NSS_Low_Internal(instance);
    
    /* 发送数据 */
    for (i = 0; i < length; i++)
    {
        SPI_SW_Transfer16(instance, data[i]);
    }
    
    /* 拉高NSS */
    SPI_SW_NSS_High_Internal(instance);
    
    return SPI_SW_OK;
}

/**
 * @brief 软件SPI主模式接收数据（16位）
 */
SPI_SW_Status_t SPI_SW_MasterReceive16(SPI_SW_Instance_t instance, uint16_t *data, uint16_t length)
{
    uint16_t i;
    
    SPI_SW_CHECK_INIT(instance);
    SPI_SW_CHECK_PARAM(data);
    
    if (length == 0)
    {
        return SPI_SW_ERROR_INVALID_PARAM;
    }
    
    /* 拉低NSS */
    SPI_SW_NSS_Low_Internal(instance);
    
    /* 接收数据（发送dummy数据0xFFFF来产生时钟） */
    for (i = 0; i < length; i++)
    {
        data[i] = SPI_SW_Transfer16(instance, 0xFFFF);
    }
    
    /* 拉高NSS */
    SPI_SW_NSS_High_Internal(instance);
    
    return SPI_SW_OK;
}

/**
 * @brief 软件SPI主模式全双工传输（16位）
 */
SPI_SW_Status_t SPI_SW_MasterTransmitReceive16(SPI_SW_Instance_t instance, const uint16_t *tx_data, uint16_t *rx_data, uint16_t length)
{
    uint16_t i;
    
    SPI_SW_CHECK_INIT(instance);
    SPI_SW_CHECK_PARAM(tx_data);
    SPI_SW_CHECK_PARAM(rx_data);
    
    if (length == 0)
    {
        return SPI_SW_ERROR_INVALID_PARAM;
    }
    
    /* 拉低NSS */
    SPI_SW_NSS_Low_Internal(instance);
    
    /* 全双工传输 */
    for (i = 0; i < length; i++)
    {
        rx_data[i] = SPI_SW_Transfer16(instance, tx_data[i]);
    }
    
    /* 拉高NSS */
    SPI_SW_NSS_High_Internal(instance);
    
    return SPI_SW_OK;
}

/**
 * @brief 软件SPI NSS控制（拉低NSS）
 */
SPI_SW_Status_t SPI_SW_NSS_Low(SPI_SW_Instance_t instance)
{
    SPI_SW_CHECK_INIT(instance);
    
    if (g_soft_spi_configs[instance].nss_port == NULL || 
        g_soft_spi_configs[instance].nss_pin == 0)
    {
        return SPI_SW_ERROR_INVALID_PARAM;
    }
    
    GPIO_WritePin(g_soft_spi_configs[instance].nss_port, 
                 g_soft_spi_configs[instance].nss_pin, Bit_RESET);
    
    return SPI_SW_OK;
}

/**
 * @brief 软件SPI NSS控制（拉高NSS）
 */
SPI_SW_Status_t SPI_SW_NSS_High(SPI_SW_Instance_t instance)
{
    SPI_SW_CHECK_INIT(instance);
    
    if (g_soft_spi_configs[instance].nss_port == NULL || 
        g_soft_spi_configs[instance].nss_pin == 0)
    {
        return SPI_SW_ERROR_INVALID_PARAM;
    }
    
    GPIO_WritePin(g_soft_spi_configs[instance].nss_port, 
                 g_soft_spi_configs[instance].nss_pin, Bit_SET);
    
    return SPI_SW_OK;
}

#endif /* CONFIG_MODULE_SOFT_SPI_ENABLED */

