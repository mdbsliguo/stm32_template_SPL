/**
 * @file i2c_hw.c
 * @brief 硬件I2C驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的硬件I2C驱动，支持主模式（轮询/中断/DMA）、7位/10位地址、总线扫描等功能
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_I2C_ENABLED

/* Include our header */
#include "i2c_hw.h"

/* Note: We can directly use I2C_Init from the standard library since our function is named I2C_HW_Init */
#include "gpio.h"
#include "delay.h"
#include "nvic.h"
#if CONFIG_MODULE_DMA_ENABLED
#include "dma.h"
#endif
#include "stm32f10x_rcc.h"
#include "stm32f10x_i2c.h"
#include "misc.h"
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>

/* 从board.h加载配置 */
static I2C_Config_t g_i2c_configs[I2C_INSTANCE_MAX] = I2C_CONFIGS;

/* 初始化标志 */
static bool g_i2c_initialized[I2C_INSTANCE_MAX] = {false, false};

/* 默认超时时间（毫秒） */
#define I2C_DEFAULT_TIMEOUT_MS  1000

/* I2C_MasterWriteRegs最大长度限制（避免栈溢出） */
/* STM32F103C8T6的栈大小通常为2KB，这里限制为128字节是安全的 */
#define I2C_MAX_WRITE_REGS_LENGTH 128

/* 中断模式相关变量 */
static I2C_TransferMode_t g_i2c_transfer_mode[I2C_INSTANCE_MAX] = {I2C_MODE_POLLING, I2C_MODE_POLLING};
static I2C_Callback_t g_i2c_callback[I2C_INSTANCE_MAX] = {NULL, NULL};

/* 从模式相关变量 */
static I2C_SlaveCallback_t g_i2c_slave_callback[I2C_INSTANCE_MAX] = {NULL};
static void *g_i2c_slave_user_data[I2C_INSTANCE_MAX] = {NULL};
static bool g_i2c_slave_mode[I2C_INSTANCE_MAX] = {false};

/* 中断模式发送/接收缓冲区 */
static const uint8_t *g_i2c_tx_buffer[I2C_INSTANCE_MAX] = {NULL};
static uint8_t *g_i2c_rx_buffer[I2C_INSTANCE_MAX] = {NULL};
static uint16_t g_i2c_tx_length[I2C_INSTANCE_MAX] = {0};
static uint16_t g_i2c_tx_index[I2C_INSTANCE_MAX] = {0};
static uint16_t g_i2c_rx_length[I2C_INSTANCE_MAX] = {0};
static uint16_t g_i2c_rx_index[I2C_INSTANCE_MAX] = {0};
static uint8_t g_i2c_slave_addr[I2C_INSTANCE_MAX] = {0};
static I2C_Status_t g_i2c_status[I2C_INSTANCE_MAX] = {I2C_OK};

#if CONFIG_MODULE_DMA_ENABLED
/* DMA通道映射（I2C TX/RX对应的DMA通道） */
/* 注意：I2C1使用DMA1_CH6(TX)/CH7(RX)，I2C2使用DMA1_CH4(TX)/CH5(RX) */
static const DMA_Channel_t i2c_tx_dma_channels[I2C_INSTANCE_MAX] = {
    DMA_CHANNEL_1_6,  /* I2C1 TX -> DMA1_CH6 */
    DMA_CHANNEL_1_4,  /* I2C2 TX -> DMA1_CH4 */
};

static const DMA_Channel_t i2c_rx_dma_channels[I2C_INSTANCE_MAX] = {
    DMA_CHANNEL_1_7,  /* I2C1 RX -> DMA1_CH7 */
    DMA_CHANNEL_1_5,  /* I2C2 RX -> DMA1_CH5 */
};
#endif /* CONFIG_MODULE_DMA_ENABLED */

/**
 * @brief 获取I2C外设时钟
 * @param[in] i2c_periph I2C外设指针
 * @return uint32_t 时钟使能值，失败返回0
 */
static uint32_t I2C_GetPeriphClock(I2C_TypeDef *i2c_periph)
{
    if (i2c_periph == I2C1)
    {
        return RCC_APB1Periph_I2C1;
    }
    else if (i2c_periph == I2C2)
    {
        return RCC_APB1Periph_I2C2;
    }
    return 0;
}

/**
 * @brief 检查并配置I2C引脚重映射
 * @param[in] i2c_periph I2C外设指针
 * @param[in] scl_port SCL引脚端口
 * @param[in] scl_pin SCL引脚号
 * @return I2C_Status_t 错误码
 */
static I2C_Status_t I2C_ConfigRemap(I2C_TypeDef *i2c_periph, GPIO_TypeDef *scl_port, uint16_t scl_pin)
{
    /* I2C1重映射检查：PB8/PB9需要重映射 */
    if (i2c_periph == I2C1)
    {
        if (scl_port == GPIOB && scl_pin == GPIO_Pin_8)
        {
            /* 启用I2C1重映射到PB8/PB9 */
            GPIO_PinRemapConfig(GPIO_Remap_I2C1, ENABLE);
        }
        else if (scl_port == GPIOB && scl_pin == GPIO_Pin_6)
        {
            /* 默认引脚PB6/PB7，不需要重映射 */
            GPIO_PinRemapConfig(GPIO_Remap_I2C1, DISABLE);
        }
        else
        {
            /* 不支持的引脚配置 */
            return I2C_ERROR_GPIO_FAILED;
        }
    }
    /* I2C2不需要重映射 */
    else if (i2c_periph == I2C2)
    {
        if (scl_port != GPIOB || scl_pin != GPIO_Pin_10)
        {
            /* I2C2只支持PB10/PB11 */
            return I2C_ERROR_GPIO_FAILED;
        }
    }
    else
    {
        return I2C_ERROR_INVALID_PERIPH;
    }
    
    return I2C_OK;
}

/**
 * @brief 等待I2C标志位
 * @param[in] i2c_periph I2C外设指针
 * @param[in] flag 标志位
 * @param[in] timeout_ms 超时时间（毫秒）
 * @return I2C_Status_t 错误码
 */
static I2C_Status_t I2C_WaitFlag(I2C_TypeDef *i2c_periph, uint32_t flag, uint32_t timeout_ms)
{
    uint32_t start_tick = Delay_GetTick();
    
    while (!I2C_CheckEvent(i2c_periph, flag))
    {
        /* 检查超时（使用Delay_GetElapsed处理溢出） */
        uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
        if (elapsed > timeout_ms)
        {
            return I2C_ERROR_TIMEOUT;
        }
        
        /* 检查错误标志 */
        if (I2C_GetFlagStatus(i2c_periph, I2C_FLAG_AF) != RESET)
        {
            I2C_ClearFlag(i2c_periph, I2C_FLAG_AF);
            return I2C_ERROR_NACK;
        }
        if (I2C_GetFlagStatus(i2c_periph, I2C_FLAG_ARLO) != RESET)
        {
            I2C_ClearFlag(i2c_periph, I2C_FLAG_ARLO);
            return I2C_ERROR_ARBITRATION_LOST;
        }
        if (I2C_GetFlagStatus(i2c_periph, I2C_FLAG_BERR) != RESET)
        {
            I2C_ClearFlag(i2c_periph, I2C_FLAG_BERR);
            return I2C_ERROR_BUS_ERROR;
        }
    }
    
    return I2C_OK;
}

/**
 * @brief 等待I2C总线空闲
 * @param[in] i2c_periph I2C外设指针
 * @param[in] timeout_ms 超时时间（毫秒）
 * @return I2C_Status_t 错误码
 */
static I2C_Status_t I2C_WaitBusIdle(I2C_TypeDef *i2c_periph, uint32_t timeout_ms)
{
    uint32_t start_tick = Delay_GetTick();
    
    while (I2C_GetFlagStatus(i2c_periph, I2C_FLAG_BUSY) != RESET)
    {
        /* 检查超时（使用Delay_GetElapsed处理溢出） */
        uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
        if (elapsed > timeout_ms)
        {
            return I2C_ERROR_BUSY;
        }
    }
    
    return I2C_OK;
}

/**
 * @brief 硬件I2C初始化
 */
I2C_Status_t I2C_HW_Init(I2C_Instance_t instance)
{
    I2C_Status_t status;
    GPIO_Status_t gpio_status;
    I2C_InitTypeDef I2C_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    uint32_t i2c_clock;
    
    /* ========== 参数校验 ========== */
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_configs[instance].enabled)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (g_i2c_configs[instance].i2c_periph == NULL)
    {
        return I2C_ERROR_INVALID_PERIPH;
    }
    
    /* 检查是否已初始化 */
    if (g_i2c_initialized[instance])
    {
        return I2C_OK;
    }
    
    /* 获取I2C外设时钟 */
    i2c_clock = I2C_GetPeriphClock(g_i2c_configs[instance].i2c_periph);
    if (i2c_clock == 0)
    {
        return I2C_ERROR_INVALID_PERIPH;
    }
    
    /* 使能I2C外设时钟 */
    RCC_APB1PeriphClockCmd(i2c_clock, ENABLE);
    
    /* 使能GPIO时钟 */
    gpio_status = GPIO_EnableClock(g_i2c_configs[instance].scl_port);
    if (gpio_status != GPIO_OK)
    {
        return I2C_ERROR_GPIO_FAILED;
    }
    
    if (g_i2c_configs[instance].scl_port != g_i2c_configs[instance].sda_port)
    {
        gpio_status = GPIO_EnableClock(g_i2c_configs[instance].sda_port);
        if (gpio_status != GPIO_OK)
        {
            return I2C_ERROR_GPIO_FAILED;
        }
    }
    
    /* 配置I2C引脚重映射（如果需要） */
    status = I2C_ConfigRemap(g_i2c_configs[instance].i2c_periph,
                              g_i2c_configs[instance].scl_port,
                              g_i2c_configs[instance].scl_pin);
    if (status != I2C_OK)
    {
        return status;
    }
    
    /* 配置SCL引脚为复用开漏输出 */
    GPIO_InitStructure.GPIO_Pin = g_i2c_configs[instance].scl_pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(g_i2c_configs[instance].scl_port, &GPIO_InitStructure);
    
    /* 配置SDA引脚为复用开漏输出 */
    GPIO_InitStructure.GPIO_Pin = g_i2c_configs[instance].sda_pin;
    GPIO_Init(g_i2c_configs[instance].sda_port, &GPIO_InitStructure);
    
    /* 配置I2C外设 */
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = g_i2c_configs[instance].own_address;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = g_i2c_configs[instance].clock_speed;
    
    /* Call the standard library's I2C_Init function */
    I2C_Init(g_i2c_configs[instance].i2c_periph, &I2C_InitStructure);
    
    /* 使能I2C外设 */
    I2C_Cmd(g_i2c_configs[instance].i2c_periph, ENABLE);
    
    /* 标记为已初始化 */
    g_i2c_initialized[instance] = true;
    
    return I2C_OK;
}

/**
 * @brief I2C反初始化
 */
I2C_Status_t I2C_Deinit(I2C_Instance_t instance)
{
    uint32_t i2c_clock;
    
    /* ========== 参数校验 ========== */
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_OK;
    }
    
    /* 禁用I2C外设 */
    I2C_Cmd(g_i2c_configs[instance].i2c_periph, DISABLE);
    
    /* 获取I2C外设时钟 */
    i2c_clock = I2C_GetPeriphClock(g_i2c_configs[instance].i2c_periph);
    if (i2c_clock != 0)
    {
        /* 禁用I2C外设时钟 */
        RCC_APB1PeriphClockCmd(i2c_clock, DISABLE);
    }
    
    /* 标记为未初始化 */
    g_i2c_initialized[instance] = false;
    
    return I2C_OK;
}

/**
 * @brief I2C主模式发送数据
 */
I2C_Status_t I2C_MasterTransmit(I2C_Instance_t instance, uint8_t slave_addr, 
                                 const uint8_t *data, uint16_t length, uint32_t timeout)
{
    I2C_TypeDef *i2c_periph;
    I2C_Status_t status;
    uint16_t i;
    uint32_t actual_timeout;
    
    /* ========== 参数校验 ========== */
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    actual_timeout = (timeout == 0) ? I2C_DEFAULT_TIMEOUT_MS : timeout;
    
    /* 等待总线空闲 */
    status = I2C_WaitBusIdle(i2c_periph, actual_timeout);
    if (status != I2C_OK)
    {
        return status;
    }
    
    /* 生成START信号 */
    I2C_GenerateSTART(i2c_periph, ENABLE);
    
    /* 等待START信号发送完成 */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_MODE_SELECT, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 发送从机地址（写模式） */
    I2C_Send7bitAddress(i2c_periph, slave_addr << 1, I2C_Direction_Transmitter);
    
    /* 等待地址发送完成并收到ACK */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 发送数据 */
    /* 注意：地址发送完成后（EV6），TXE应该已经置位，第一个字节可以直接发送 */
    for (i = 0; i < length; i++)
    {
        /* 第一个字节：地址发送完成后TXE已置位，可以直接发送 */
        /* 后续字节：需要等待TXE标志 */
        if (i > 0)
        {
            /* 等待数据寄存器为空（TXE标志） */
            status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_BYTE_TRANSMITTING, actual_timeout);
            if (status != I2C_OK)
            {
                I2C_GenerateSTOP(i2c_periph, ENABLE);
                return status;
            }
        }
        
        /* 发送数据 */
        I2C_SendData(i2c_periph, data[i]);
    }
    
    /* 等待最后一个字节发送完成（BTF标志） */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_BYTE_TRANSMITTED, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 生成STOP信号 */
    I2C_GenerateSTOP(i2c_periph, ENABLE);
    
    return I2C_OK;
}

/**
 * @brief I2C主模式接收数据
 */
I2C_Status_t I2C_MasterReceive(I2C_Instance_t instance, uint8_t slave_addr, 
                                uint8_t *data, uint16_t length, uint32_t timeout)
{
    I2C_TypeDef *i2c_periph;
    I2C_Status_t status;
    uint16_t i;
    uint32_t actual_timeout;
    
    /* ========== 参数校验 ========== */
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    actual_timeout = (timeout == 0) ? I2C_DEFAULT_TIMEOUT_MS : timeout;
    
    /* 等待总线空闲 */
    status = I2C_WaitBusIdle(i2c_periph, actual_timeout);
    if (status != I2C_OK)
    {
        return status;
    }
    
    /* 使能ACK */
    I2C_AcknowledgeConfig(i2c_periph, ENABLE);
    
    /* 生成START信号 */
    I2C_GenerateSTART(i2c_periph, ENABLE);
    
    /* 等待START信号发送完成 */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_MODE_SELECT, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 发送从机地址（读模式） */
    I2C_Send7bitAddress(i2c_periph, slave_addr << 1, I2C_Direction_Receiver);
    
    /* 等待地址发送完成并收到ACK */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 接收数据 */
    if (length == 1)
    {
        /* 单字节接收：禁用ACK，生成STOP */
        I2C_AcknowledgeConfig(i2c_periph, DISABLE);
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        
        /* 等待数据接收完成 */
        status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_BYTE_RECEIVED, actual_timeout);
        if (status != I2C_OK)
        {
            return status;
        }
        
        /* 读取数据 */
        data[0] = I2C_ReceiveData(i2c_periph);
    }
    else
    {
        /* 多字节接收 */
        for (i = 0; i < length; i++)
        {
            /* 等待数据接收完成 */
            status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_BYTE_RECEIVED, actual_timeout);
            if (status != I2C_OK)
            {
                return status;
            }
            
            /* 读取数据 */
            data[i] = I2C_ReceiveData(i2c_periph);
            
            /* 如果是倒数第二个字节，禁用ACK（为最后一个字节做准备） */
            if (i == length - 2)
            {
                I2C_AcknowledgeConfig(i2c_periph, DISABLE);
            }
            
            /* 如果是最后一个字节，生成STOP */
            if (i == length - 1)
            {
                I2C_GenerateSTOP(i2c_periph, ENABLE);
            }
        }
    }
    
    return I2C_OK;
}

/**
 * @brief I2C主模式写寄存器
 */
I2C_Status_t I2C_MasterWriteReg(I2C_Instance_t instance, uint8_t slave_addr, 
                                 uint8_t reg_addr, uint8_t reg_value, uint32_t timeout)
{
    uint8_t tx_data[2];
    
    tx_data[0] = reg_addr;
    tx_data[1] = reg_value;
    
    return I2C_MasterTransmit(instance, slave_addr, tx_data, 2, timeout);
}

/**
 * @brief I2C主模式读寄存器（使用Repeated START）
 */
I2C_Status_t I2C_MasterReadReg(I2C_Instance_t instance, uint8_t slave_addr, 
                                 uint8_t reg_addr, uint8_t *reg_value, uint32_t timeout)
{
    I2C_TypeDef *i2c_periph;
    I2C_Status_t status;
    uint32_t actual_timeout;
    
    if (reg_value == NULL)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (instance >= I2C_INSTANCE_MAX || !g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    actual_timeout = (timeout == 0) ? I2C_DEFAULT_TIMEOUT_MS : timeout;
    
    /* 等待总线空闲 */
    status = I2C_WaitBusIdle(i2c_periph, actual_timeout);
    if (status != I2C_OK)
    {
        return status;
    }
    
    /* 生成START信号 */
    I2C_GenerateSTART(i2c_periph, ENABLE);
    
    /* 等待START信号发送完成 */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_MODE_SELECT, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 发送从机地址（写模式） */
    I2C_Send7bitAddress(i2c_periph, slave_addr << 1, I2C_Direction_Transmitter);
    
    /* 等待地址发送完成并收到ACK */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 发送寄存器地址 */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_BYTE_TRANSMITTING, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    I2C_SendData(i2c_periph, reg_addr);
    
    /* 等待寄存器地址发送完成 */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_BYTE_TRANSMITTED, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 生成Repeated START信号 */
    I2C_GenerateSTART(i2c_periph, ENABLE);
    
    /* 等待Repeated START信号发送完成 */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_MODE_SELECT, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 发送从机地址（读模式） */
    I2C_Send7bitAddress(i2c_periph, slave_addr << 1, I2C_Direction_Receiver);
    
    /* 等待地址发送完成并收到ACK */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 单字节接收：禁用ACK，生成STOP */
    I2C_AcknowledgeConfig(i2c_periph, DISABLE);
    I2C_GenerateSTOP(i2c_periph, ENABLE);
    
    /* 等待数据接收完成 */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_BYTE_RECEIVED, actual_timeout);
    if (status != I2C_OK)
    {
        return status;
    }
    
    /* 读取数据 */
    *reg_value = I2C_ReceiveData(i2c_periph);
    
    return I2C_OK;
}

/**
 * @brief I2C主模式写多个寄存器
 */
I2C_Status_t I2C_MasterWriteRegs(I2C_Instance_t instance, uint8_t slave_addr, 
                                  uint8_t reg_addr, const uint8_t *data, uint16_t length, uint32_t timeout)
{
    uint16_t i;
    
    /* ========== 参数校验 ========== */
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    /* 分配临时缓冲区（寄存器地址 + 数据） */
    /* 注意：这里使用栈分配，限制最大长度避免栈溢出 */
    if (length > I2C_MAX_WRITE_REGS_LENGTH)
    {
        return I2C_ERROR_INVALID_PARAM;  /* 超过最大长度限制 */
    }
    
    uint8_t tx_buf[I2C_MAX_WRITE_REGS_LENGTH + 1];
    tx_buf[0] = reg_addr;
    for (i = 0; i < length; i++)
    {
        tx_buf[i + 1] = data[i];
    }
    
    return I2C_MasterTransmit(instance, slave_addr, tx_buf, length + 1, timeout);
}

/**
 * @brief I2C主模式读多个寄存器（使用Repeated START）
 */
I2C_Status_t I2C_MasterReadRegs(I2C_Instance_t instance, uint8_t slave_addr, 
                                  uint8_t reg_addr, uint8_t *data, uint16_t length, uint32_t timeout)
{
    I2C_TypeDef *i2c_periph;
    I2C_Status_t status;
    uint16_t i;
    uint32_t actual_timeout;
    
    if (data == NULL || length == 0)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (instance >= I2C_INSTANCE_MAX || !g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    actual_timeout = (timeout == 0) ? I2C_DEFAULT_TIMEOUT_MS : timeout;
    
    /* 等待总线空闲 */
    status = I2C_WaitBusIdle(i2c_periph, actual_timeout);
    if (status != I2C_OK)
    {
        return status;
    }
    
    /* 生成START信号 */
    I2C_GenerateSTART(i2c_periph, ENABLE);
    
    /* 等待START信号发送完成 */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_MODE_SELECT, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 发送从机地址（写模式） */
    I2C_Send7bitAddress(i2c_periph, slave_addr << 1, I2C_Direction_Transmitter);
    
    /* 等待地址发送完成并收到ACK */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 发送寄存器地址 */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_BYTE_TRANSMITTING, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    I2C_SendData(i2c_periph, reg_addr);
    
    /* 等待寄存器地址发送完成 */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_BYTE_TRANSMITTED, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 生成Repeated START信号 */
    I2C_GenerateSTART(i2c_periph, ENABLE);
    
    /* 等待Repeated START信号发送完成 */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_MODE_SELECT, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 发送从机地址（读模式） */
    I2C_Send7bitAddress(i2c_periph, slave_addr << 1, I2C_Direction_Receiver);
    
    /* 等待地址发送完成并收到ACK */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 使能ACK */
    I2C_AcknowledgeConfig(i2c_periph, ENABLE);
    
    /* 接收数据 */
    if (length == 1)
    {
        /* 单字节接收：禁用ACK，生成STOP */
        I2C_AcknowledgeConfig(i2c_periph, DISABLE);
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        
        /* 等待数据接收完成 */
        status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_BYTE_RECEIVED, actual_timeout);
        if (status != I2C_OK)
        {
            return status;
        }
        
        /* 读取数据 */
        data[0] = I2C_ReceiveData(i2c_periph);
    }
    else
    {
        /* 多字节接收 */
        for (i = 0; i < length; i++)
        {
            /* 等待数据接收完成 */
            status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_BYTE_RECEIVED, actual_timeout);
            if (status != I2C_OK)
            {
                return status;
            }
            
            /* 读取数据 */
            data[i] = I2C_ReceiveData(i2c_periph);
            
            /* 如果是倒数第二个字节，禁用ACK（为最后一个字节做准备） */
            if (i == length - 2)
            {
                I2C_AcknowledgeConfig(i2c_periph, DISABLE);
            }
            
            /* 如果是最后一个字节，生成STOP */
            if (i == length - 1)
            {
                I2C_GenerateSTOP(i2c_periph, ENABLE);
            }
        }
    }
    
    return I2C_OK;
}

/**
 * @brief 检查I2C是否已初始化
 */
uint8_t I2C_IsInitialized(I2C_Instance_t instance)
{
    if (instance >= I2C_INSTANCE_MAX)
    {
        return 0;
    }
    
    return g_i2c_initialized[instance] ? 1 : 0;
}

/**
 * @brief 获取I2C外设指针
 */
I2C_TypeDef* I2C_GetPeriph(I2C_Instance_t instance)
{
    if (instance >= I2C_INSTANCE_MAX)
    {
        return NULL;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return NULL;
    }
    
    return g_i2c_configs[instance].i2c_periph;
}

/**
 * @brief I2C软件复位
 */
I2C_Status_t I2C_SoftwareReset(I2C_Instance_t instance)
{
    I2C_TypeDef *i2c_periph;
    
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    
    /* 执行软件复位 */
    I2C_SoftwareResetCmd(i2c_periph, ENABLE);
    I2C_SoftwareResetCmd(i2c_periph, DISABLE);
    
    /* 清除所有错误标志 */
    I2C_ClearFlag(i2c_periph, I2C_FLAG_AF);
    I2C_ClearFlag(i2c_periph, I2C_FLAG_ARLO);
    I2C_ClearFlag(i2c_periph, I2C_FLAG_BERR);
    
    return I2C_OK;
}

/**
 * @brief 检查I2C总线是否忙
 */
uint8_t I2C_IsBusBusy(I2C_Instance_t instance)
{
    I2C_TypeDef *i2c_periph;
    
    if (instance >= I2C_INSTANCE_MAX)
    {
        return 0;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return 0;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    
    return (I2C_GetFlagStatus(i2c_periph, I2C_FLAG_BUSY) != RESET) ? 1 : 0;
}

/**
 * @brief I2C总线扫描
 */
I2C_Status_t I2C_ScanBus(I2C_Instance_t instance, uint8_t *found_addr, uint8_t max_count, uint8_t *count, uint32_t timeout)
{
    I2C_TypeDef *i2c_periph;
    I2C_Status_t status;
    uint8_t addr;
    uint8_t found = 0;
    uint32_t actual_timeout;
    
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    if (found_addr == NULL || count == NULL || max_count == 0)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    actual_timeout = (timeout == 0) ? 10 : timeout;  /* 扫描使用较短的超时时间 */
    
    *count = 0;
    
    /* 扫描地址范围：0x08-0x77（7位地址，排除保留地址0x00-0x07和0x78-0x7F） */
    for (addr = 0x08; addr <= 0x77 && found < max_count; addr++)
    {
        /* 等待总线空闲（使用I2C_WaitBusIdle统一处理） */
        status = I2C_WaitBusIdle(i2c_periph, 10);  /* 最多等待10ms */
        if (status != I2C_OK)
        {
            continue;  /* 总线忙，跳过这个地址 */
        }
        
        /* 生成START信号 */
        I2C_GenerateSTART(i2c_periph, ENABLE);
        
        /* 等待START信号发送完成 */
        status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_MODE_SELECT, actual_timeout);
        if (status != I2C_OK)
        {
            I2C_GenerateSTOP(i2c_periph, ENABLE);
            continue;
        }
        
        /* 发送从机地址（写模式） */
        I2C_Send7bitAddress(i2c_periph, addr << 1, I2C_Direction_Transmitter);
        
        /* 等待地址发送完成（使用I2C_WaitFlag统一处理） */
        status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, actual_timeout);
        
        /* 如果收到ACK，说明设备存在 */
        if (status == I2C_OK)
        {
            found_addr[found] = addr;
            found++;
        }
        
        /* 无论成功与否，都需要生成STOP信号 */
        /* 如果之前已经因为错误生成了STOP，这里再次生成也是安全的（幂等操作） */
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        
        /* 等待STOP信号完成（使用短延时，避免阻塞） */
        /* 注意：这里使用Delay_us而不是Delay_ms，减少延时时间 */
        Delay_us(100);  /* 100微秒足够STOP信号完成 */
    }
    
    *count = found;
    return I2C_OK;
}

/**
 * @brief I2C主模式发送数据（10位地址）
 */
I2C_Status_t I2C_MasterTransmit10bit(I2C_Instance_t instance, uint16_t slave_addr, 
                                      const uint8_t *data, uint16_t length, uint32_t timeout)
{
    I2C_TypeDef *i2c_periph;
    I2C_Status_t status;
    uint16_t i;
    uint32_t actual_timeout;
    
    /* ========== 参数校验 ========== */
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (slave_addr > 0x3FF)
    {
        return I2C_ERROR_INVALID_PARAM;  /* 10位地址范围：0x000-0x3FF */
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    actual_timeout = (timeout == 0) ? I2C_DEFAULT_TIMEOUT_MS : timeout;
    
    /* 等待总线空闲 */
    status = I2C_WaitBusIdle(i2c_periph, actual_timeout);
    if (status != I2C_OK)
    {
        return status;
    }
    
    /* 生成START信号 */
    I2C_GenerateSTART(i2c_periph, ENABLE);
    
    /* 等待START信号发送完成 */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_MODE_SELECT, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 发送10位地址的第一字节（11110XX + 地址高2位 + R/W=0） */
    /* 格式：11110 + 地址[9:8] + 0（写） */
    uint8_t addr_byte1 = 0xF0 | ((slave_addr >> 7) & 0x06);
    I2C_SendData(i2c_periph, addr_byte1);
    
    /* 等待地址发送完成（EV9事件） */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_MODE_ADDRESS10, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 发送10位地址的第二字节（地址低8位） */
    /* 根据STM32标准库文档，第二字节使用I2C_Send7bitAddress发送 */
    /* 注意：对于10位地址，这里发送的是地址低8位，I2C_Send7bitAddress会正确处理 */
    I2C_Send7bitAddress(i2c_periph, (uint8_t)(slave_addr & 0xFF), I2C_Direction_Transmitter);
    
    /* 等待地址发送完成并收到ACK（EV6事件） */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 发送数据 */
    /* 注意：地址发送完成后（EV6），TXE应该已经置位，第一个字节可以直接发送 */
    for (i = 0; i < length; i++)
    {
        /* 第一个字节：地址发送完成后TXE已置位，可以直接发送 */
        /* 后续字节：需要等待TXE标志 */
        if (i > 0)
        {
            /* 等待数据寄存器为空 */
            status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_BYTE_TRANSMITTING, actual_timeout);
            if (status != I2C_OK)
            {
                I2C_GenerateSTOP(i2c_periph, ENABLE);
                return status;
            }
        }
        
        /* 发送数据 */
        I2C_SendData(i2c_periph, data[i]);
    }
    
    /* 等待最后一个字节发送完成 */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_BYTE_TRANSMITTED, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 生成STOP信号 */
    I2C_GenerateSTOP(i2c_periph, ENABLE);
    
    return I2C_OK;
}

/**
 * @brief I2C主模式接收数据（10位地址）
 */
I2C_Status_t I2C_MasterReceive10bit(I2C_Instance_t instance, uint16_t slave_addr, 
                                     uint8_t *data, uint16_t length, uint32_t timeout)
{
    I2C_TypeDef *i2c_periph;
    I2C_Status_t status;
    uint16_t i;
    uint32_t actual_timeout;
    
    /* ========== 参数校验 ========== */
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (slave_addr > 0x3FF)
    {
        return I2C_ERROR_INVALID_PARAM;  /* 10位地址范围：0x000-0x3FF */
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    actual_timeout = (timeout == 0) ? I2C_DEFAULT_TIMEOUT_MS : timeout;
    
    /* 等待总线空闲 */
    status = I2C_WaitBusIdle(i2c_periph, actual_timeout);
    if (status != I2C_OK)
    {
        return status;
    }
    
    /* 使能ACK */
    I2C_AcknowledgeConfig(i2c_periph, ENABLE);
    
    /* 生成START信号 */
    I2C_GenerateSTART(i2c_periph, ENABLE);
    
    /* 等待START信号发送完成 */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_MODE_SELECT, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 发送10位地址的第一字节（11110XX + 地址高2位 + R/W=1） */
    /* 格式：11110 + 地址[9:8] + 1（读） */
    uint8_t addr_byte1 = 0xF0 | ((slave_addr >> 7) & 0x06) | 0x01;
    I2C_SendData(i2c_periph, addr_byte1);
    
    /* 等待地址发送完成（EV9事件） */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_MODE_ADDRESS10, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 发送10位地址的第二字节（地址低8位） */
    /* 根据STM32标准库文档，第二字节使用I2C_Send7bitAddress发送 */
    /* 注意：对于10位地址，这里发送的是地址低8位，I2C_Send7bitAddress会正确处理 */
    I2C_Send7bitAddress(i2c_periph, (uint8_t)(slave_addr & 0xFF), I2C_Direction_Receiver);
    
    /* 等待地址发送完成并收到ACK（EV6事件） */
    status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, actual_timeout);
    if (status != I2C_OK)
    {
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        return status;
    }
    
    /* 接收数据 */
    if (length == 1)
    {
        /* 单字节接收：禁用ACK，生成STOP */
        I2C_AcknowledgeConfig(i2c_periph, DISABLE);
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        
        /* 等待数据接收完成 */
        status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_BYTE_RECEIVED, actual_timeout);
        if (status != I2C_OK)
        {
            return status;
        }
        
        /* 读取数据 */
        data[0] = I2C_ReceiveData(i2c_periph);
    }
    else
    {
        /* 多字节接收 */
        for (i = 0; i < length; i++)
        {
            /* 等待数据接收完成 */
            status = I2C_WaitFlag(i2c_periph, I2C_EVENT_MASTER_BYTE_RECEIVED, actual_timeout);
            if (status != I2C_OK)
            {
                return status;
            }
            
            /* 读取数据 */
            data[i] = I2C_ReceiveData(i2c_periph);
            
            /* 如果是倒数第二个字节，禁用ACK（为最后一个字节做准备） */
            if (i == length - 2)
            {
                I2C_AcknowledgeConfig(i2c_periph, DISABLE);
            }
            
            /* 如果是最后一个字节，生成STOP */
            if (i == length - 1)
            {
                I2C_GenerateSTOP(i2c_periph, ENABLE);
            }
        }
    }
    
    return I2C_OK;
}

/**
 * @brief 获取I2C配置信息
 */
I2C_Status_t I2C_GetConfig(I2C_Instance_t instance, I2C_ConfigInfo_t *config_info)
{
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    if (config_info == NULL)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    config_info->i2c_periph = g_i2c_configs[instance].i2c_periph;
    config_info->clock_speed = g_i2c_configs[instance].clock_speed;
    config_info->own_address = g_i2c_configs[instance].own_address;
    config_info->address_mode = 0;  /* 当前只支持7位地址 */
    config_info->enabled = g_i2c_configs[instance].enabled;
    
    return I2C_OK;
}

/* 传输模式和回调函数（预留接口，用于中断/DMA模式） */
/* 注意：这些变量已在文件开头定义（第43-44行），这里不再重复定义 */

/* 抑制未使用变量警告（预留接口） */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((unused))
#elif defined(__ICCARM__)
#pragma diag_suppress=177
#elif defined(__CC_ARM)
#pragma diag_suppress 177
#endif

/**
 * @brief 设置I2C传输模式
 */
I2C_Status_t I2C_SetTransferMode(I2C_Instance_t instance, I2C_TransferMode_t mode)
{
    I2C_TypeDef *i2c_periph;
    IRQn_Type ev_irqn, er_irqn;
    NVIC_InitTypeDef NVIC_InitStructure;
    
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    if (mode >= 3)  /* I2C_MODE_DMA = 2 */
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    
    /* 获取中断向量（函数定义在后面，需要前向声明或移动定义） */
    /* 临时内联实现，避免前向声明问题 */
    if (instance == I2C_INSTANCE_1)
    {
        ev_irqn = I2C1_EV_IRQn;
        er_irqn = I2C1_ER_IRQn;
    }
    else if (instance == I2C_INSTANCE_2)
    {
        ev_irqn = I2C2_EV_IRQn;
        er_irqn = I2C2_ER_IRQn;
    }
    else
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    /* 如果之前是中断模式，先禁用中断 */
    if (g_i2c_transfer_mode[instance] == I2C_MODE_INTERRUPT)
    {
        I2C_ITConfig(i2c_periph, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR, DISABLE);
        NVIC_InitStructure.NVIC_IRQChannel = ev_irqn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
        NVIC_InitStructure.NVIC_IRQChannelCmd = DISABLE;
        NVIC_Init(&NVIC_InitStructure);
        
        NVIC_InitStructure.NVIC_IRQChannel = er_irqn;
        NVIC_Init(&NVIC_InitStructure);
    }
    
    /* 设置传输模式 */
    g_i2c_transfer_mode[instance] = mode;
    
    /* 如果是中断模式，使能中断 */
    if (mode == I2C_MODE_INTERRUPT)
    {
        /* 使能I2C中断 */
        I2C_ITConfig(i2c_periph, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR, ENABLE);
        
        /* 配置NVIC中断优先级 */
        NVIC_InitStructure.NVIC_IRQChannel = ev_irqn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);
        
        NVIC_InitStructure.NVIC_IRQChannel = er_irqn;
        NVIC_Init(&NVIC_InitStructure);
    }
    
    return I2C_OK;
}

/**
 * @brief 设置I2C中断回调函数
 */
I2C_Status_t I2C_SetCallback(I2C_Instance_t instance, I2C_Callback_t callback)
{
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    g_i2c_callback[instance] = callback;
    (void)g_i2c_callback;  /* 抑制未使用变量警告（预留接口） */
    
    return I2C_OK;
}

#if CONFIG_MODULE_DMA_ENABLED
/**
 * @brief I2C主模式发送数据（DMA模式）
 */
I2C_Status_t I2C_MasterTransmitDMA(I2C_Instance_t instance, uint8_t slave_addr, 
                                    const uint8_t *data, uint16_t length)
{
    I2C_TypeDef *i2c_periph;
    DMA_Channel_t dma_channel;
    DMA_Status_t dma_status;
    
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    dma_channel = i2c_tx_dma_channels[instance];
    
    /* 检查DMA通道是否已初始化 */
    if (!DMA_IsInitialized(dma_channel))
    {
        dma_status = DMA_HW_Init(dma_channel);
        if (dma_status != DMA_OK)
        {
            return I2C_ERROR_INVALID_PARAM;
        }
    }
    
    /* 停止之前的传输 */
    DMA_Stop(dma_channel);
    
    /* 配置DMA传输（内存到外设） */
    dma_status = DMA_ConfigTransfer(dma_channel, (uint32_t)&i2c_periph->DR,
                                    (uint32_t)data, length,
                                    DMA_DIR_MEMORY_TO_PERIPHERAL, 1);
    if (dma_status != DMA_OK)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    /* 使能I2C DMA请求 */
    I2C_DMACmd(i2c_periph, ENABLE);
    
    /* 生成START条件并发送地址 */
    I2C_GenerateSTART(i2c_periph, ENABLE);
    
    /* 等待START位 */
    uint32_t timeout = Delay_GetTick() + I2C_DEFAULT_TIMEOUT_MS;
    while (!I2C_CheckEvent(i2c_periph, I2C_EVENT_MASTER_MODE_SELECT))
    {
        if (Delay_GetElapsed(Delay_GetTick(), timeout) > I2C_DEFAULT_TIMEOUT_MS)
        {
            I2C_DMACmd(i2c_periph, DISABLE);
            return I2C_ERROR_TIMEOUT;
        }
    }
    
    /* 发送从机地址（写） */
    I2C_Send7bitAddress(i2c_periph, slave_addr << 1, I2C_Direction_Transmitter);
    
    /* 等待地址发送完成 */
    timeout = Delay_GetTick() + I2C_DEFAULT_TIMEOUT_MS;
    while (!I2C_CheckEvent(i2c_periph, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
    {
        if (Delay_GetElapsed(Delay_GetTick(), timeout) > I2C_DEFAULT_TIMEOUT_MS)
        {
            I2C_DMACmd(i2c_periph, DISABLE);
            return I2C_ERROR_TIMEOUT;
        }
    }
    
    /* 启动DMA传输 */
    dma_status = DMA_Start(dma_channel);
    if (dma_status != DMA_OK)
    {
        I2C_DMACmd(i2c_periph, DISABLE);
        return I2C_ERROR_INVALID_PARAM;
    }
    
    return I2C_OK;
}

/**
 * @brief I2C主模式接收数据（DMA模式）
 */
I2C_Status_t I2C_MasterReceiveDMA(I2C_Instance_t instance, uint8_t slave_addr, 
                                   uint8_t *data, uint16_t length)
{
    I2C_TypeDef *i2c_periph;
    DMA_Channel_t dma_channel;
    DMA_Status_t dma_status;
    
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    if (data == NULL || length == 0)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    dma_channel = i2c_rx_dma_channels[instance];
    
    /* 检查DMA通道是否已初始化 */
    if (!DMA_IsInitialized(dma_channel))
    {
        dma_status = DMA_HW_Init(dma_channel);
        if (dma_status != DMA_OK)
        {
            return I2C_ERROR_INVALID_PARAM;
        }
    }
    
    /* 停止之前的传输 */
    DMA_Stop(dma_channel);
    
    /* 配置DMA传输（外设到内存） */
    dma_status = DMA_ConfigTransfer(dma_channel, (uint32_t)&i2c_periph->DR,
                                    (uint32_t)data, length,
                                    DMA_DIR_PERIPHERAL_TO_MEMORY, 1);
    if (dma_status != DMA_OK)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    /* 使能I2C DMA接收请求 */
    I2C_DMACmd(i2c_periph, ENABLE);
    
    /* 生成START条件并发送地址 */
    I2C_GenerateSTART(i2c_periph, ENABLE);
    
    /* 等待START位 */
    uint32_t timeout = Delay_GetTick() + I2C_DEFAULT_TIMEOUT_MS;
    while (!I2C_CheckEvent(i2c_periph, I2C_EVENT_MASTER_MODE_SELECT))
    {
        if (Delay_GetElapsed(Delay_GetTick(), timeout) > I2C_DEFAULT_TIMEOUT_MS)
        {
            I2C_DMACmd(i2c_periph, DISABLE);
            return I2C_ERROR_TIMEOUT;
        }
    }
    
    /* 发送从机地址（读） */
    if (length == 1)
    {
        I2C_AcknowledgeConfig(i2c_periph, DISABLE);
    }
    I2C_Send7bitAddress(i2c_periph, slave_addr << 1, I2C_Direction_Receiver);
    
    /* 等待地址发送完成 */
    timeout = Delay_GetTick() + I2C_DEFAULT_TIMEOUT_MS;
    while (!I2C_CheckEvent(i2c_periph, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
    {
        if (Delay_GetElapsed(Delay_GetTick(), timeout) > I2C_DEFAULT_TIMEOUT_MS)
        {
            I2C_DMACmd(i2c_periph, DISABLE);
            return I2C_ERROR_TIMEOUT;
        }
    }
    
    if (length == 1)
    {
        I2C_AcknowledgeConfig(i2c_periph, DISABLE);
        I2C_GenerateSTOP(i2c_periph, ENABLE);
    }
    else if (length == 2)
    {
        I2C_AcknowledgeConfig(i2c_periph, DISABLE);
    }
    
    /* 启动DMA传输 */
    dma_status = DMA_Start(dma_channel);
    if (dma_status != DMA_OK)
    {
        I2C_DMACmd(i2c_periph, DISABLE);
        return I2C_ERROR_INVALID_PARAM;
    }
    
    return I2C_OK;
}
#endif /* CONFIG_MODULE_DMA_ENABLED */

/* ========== 中断模式功能实现 ========== */

/**
 * @brief 获取I2C中断向量
 */
static void I2C_GetIRQn(I2C_Instance_t instance, IRQn_Type *ev_irqn, IRQn_Type *er_irqn)
{
    if (instance == I2C_INSTANCE_1)
    {
        *ev_irqn = I2C1_EV_IRQn;
        *er_irqn = I2C1_ER_IRQn;
    }
    else
    {
        *ev_irqn = I2C2_EV_IRQn;
        *er_irqn = I2C2_ER_IRQn;
    }
}

/**
 * @brief I2C非阻塞发送（中断模式）
 */
I2C_Status_t I2C_MasterTransmitIT(I2C_Instance_t instance, uint8_t slave_addr, 
                                   const uint8_t *data, uint16_t length)
{
    I2C_TypeDef *i2c_periph;
    
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    if (g_i2c_transfer_mode[instance] != I2C_MODE_INTERRUPT)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (data == NULL || length == 0)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (g_i2c_tx_buffer[instance] != NULL)
    {
        return I2C_ERROR_BUSY;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    
    /* 保存发送缓冲区信息 */
    g_i2c_tx_buffer[instance] = data;
    g_i2c_tx_length[instance] = length;
    g_i2c_tx_index[instance] = 0;
    g_i2c_slave_addr[instance] = slave_addr;
    g_i2c_status[instance] = I2C_OK;
    
    /* 生成START条件 */
    I2C_GenerateSTART(i2c_periph, ENABLE);
    
    return I2C_OK;
}

/**
 * @brief I2C非阻塞接收（中断模式）
 */
I2C_Status_t I2C_MasterReceiveIT(I2C_Instance_t instance, uint8_t slave_addr, 
                                  uint8_t *data, uint16_t length)
{
    I2C_TypeDef *i2c_periph;
    
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    if (g_i2c_transfer_mode[instance] != I2C_MODE_INTERRUPT)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (data == NULL || length == 0)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (g_i2c_rx_buffer[instance] != NULL)
    {
        return I2C_ERROR_BUSY;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    
    /* 保存接收缓冲区信息 */
    g_i2c_rx_buffer[instance] = data;
    g_i2c_rx_length[instance] = length;
    g_i2c_rx_index[instance] = 0;
    g_i2c_slave_addr[instance] = slave_addr;
    g_i2c_status[instance] = I2C_OK;
    
    /* 生成START条件 */
    I2C_GenerateSTART(i2c_periph, ENABLE);
    
    return I2C_OK;
}

/**
 * @brief I2C事件中断服务函数
 */
void I2C_EV_IRQHandler(I2C_Instance_t instance)
{
    I2C_TypeDef *i2c_periph;
    uint32_t sr1, sr2;
    uint32_t event;
    uint8_t data;
    
    if (instance >= I2C_INSTANCE_MAX || !g_i2c_initialized[instance])
    {
        return;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    sr1 = i2c_periph->SR1;
    sr2 = i2c_periph->SR2;
    
    /* 从模式处理 */
    if (g_i2c_slave_mode[instance] && g_i2c_slave_callback[instance] != NULL)
    {
        /* 地址匹配事件 */
        if (sr1 & I2C_FLAG_ADDR)
        {
            event = I2C_GetLastEvent(i2c_periph);
            
            /* 从机接收模式（主机写） */
            if (event == I2C_EVENT_SLAVE_RECEIVER_ADDRESS_MATCHED)
            {
                g_i2c_slave_callback[instance](instance, I2C_EVENT_SLAVE_RECEIVER_ADDRESS_MATCHED, 0, g_i2c_slave_user_data[instance]);
            }
            /* 从机发送模式（主机读） */
            else if (event == I2C_EVENT_SLAVE_TRANSMITTER_ADDRESS_MATCHED)
            {
                g_i2c_slave_callback[instance](instance, I2C_EVENT_SLAVE_TRANSMITTER_ADDRESS_MATCHED, 0, g_i2c_slave_user_data[instance]);
            }
            
            /* 清除ADDR标志（读取SR1和SR2） */
            (void)i2c_periph->SR1;
            (void)i2c_periph->SR2;
        }
        
        /* 数据接收事件 */
        if (sr1 & I2C_FLAG_RXNE)
        {
            if (!(sr2 & I2C_FLAG_TRA))  /* 接收模式 */
            {
                data = I2C_ReceiveData(i2c_periph);
                g_i2c_slave_callback[instance](instance, I2C_EVENT_SLAVE_BYTE_RECEIVED, data, g_i2c_slave_user_data[instance]);
            }
        }
        
        /* 数据发送事件 */
        if (sr1 & I2C_FLAG_TXE)
        {
            if (sr2 & I2C_FLAG_TRA)  /* 发送模式 */
            {
                g_i2c_slave_callback[instance](instance, I2C_EVENT_SLAVE_BYTE_TRANSMITTING, 0, g_i2c_slave_user_data[instance]);
            }
        }
        
        /* STOP事件 */
        if (sr1 & I2C_FLAG_STOPF)
        {
            I2C_ClearFlag(i2c_periph, I2C_FLAG_STOPF);
            g_i2c_slave_callback[instance](instance, I2C_EVENT_SLAVE_STOP_DETECTED, 0, g_i2c_slave_user_data[instance]);
        }
        
        return;  /* 从模式处理完成，不执行主模式处理 */
    }
    
    /* 处理发送 */
    if (g_i2c_tx_buffer[instance] != NULL)
    {
        /* START位已发送 */
        if (sr1 & I2C_FLAG_SB)
        {
            /* 发送从机地址（写） */
            I2C_Send7bitAddress(i2c_periph, g_i2c_slave_addr[instance] << 1, I2C_Direction_Transmitter);
        }
        /* 地址已发送 */
        else if (sr1 & I2C_FLAG_ADDR)
        {
            /* 清除ADDR标志 */
            (void)sr2;  /* 读取SR2清除ADDR标志 */
            
            /* 发送第一个字节 */
            if (g_i2c_tx_index[instance] < g_i2c_tx_length[instance])
            {
                I2C_SendData(i2c_periph, g_i2c_tx_buffer[instance][g_i2c_tx_index[instance]++]);
            }
        }
        /* 发送缓冲区空 */
        else if (sr1 & I2C_FLAG_TXE)
        {
            if (g_i2c_tx_index[instance] < g_i2c_tx_length[instance])
            {
                /* 发送下一个字节 */
                I2C_SendData(i2c_periph, g_i2c_tx_buffer[instance][g_i2c_tx_index[instance]++]);
            }
            else
            {
                /* 发送完成，生成STOP */
                I2C_GenerateSTOP(i2c_periph, ENABLE);
                g_i2c_tx_buffer[instance] = NULL;
                g_i2c_status[instance] = I2C_OK;
                
                /* 调用回调函数 */
                if (g_i2c_callback[instance] != NULL)
                {
                    g_i2c_callback[instance](instance, I2C_OK);
                }
            }
        }
    }
    /* 处理接收 */
    else if (g_i2c_rx_buffer[instance] != NULL)
    {
        /* START位已发送 */
        if (sr1 & I2C_FLAG_SB)
        {
            /* 发送从机地址（读） */
            if (g_i2c_rx_length[instance] == 1)
            {
                I2C_AcknowledgeConfig(i2c_periph, DISABLE);
            }
            I2C_Send7bitAddress(i2c_periph, g_i2c_slave_addr[instance] << 1, I2C_Direction_Receiver);
        }
        /* 地址已发送 */
        else if (sr1 & I2C_FLAG_ADDR)
        {
            /* 清除ADDR标志 */
            (void)sr2;  /* 读取SR2清除ADDR标志 */
            
            if (g_i2c_rx_length[instance] == 1)
            {
                I2C_AcknowledgeConfig(i2c_periph, DISABLE);
                I2C_GenerateSTOP(i2c_periph, ENABLE);
            }
            else if (g_i2c_rx_length[instance] == 2)
            {
                I2C_AcknowledgeConfig(i2c_periph, DISABLE);
            }
        }
        /* 接收数据就绪 */
        else if (sr1 & I2C_FLAG_RXNE)
        {
            if (g_i2c_rx_index[instance] < g_i2c_rx_length[instance])
            {
                g_i2c_rx_buffer[instance][g_i2c_rx_index[instance]++] = I2C_ReceiveData(i2c_periph);
                
                if (g_i2c_rx_index[instance] == g_i2c_rx_length[instance] - 1)
                {
                    I2C_AcknowledgeConfig(i2c_periph, DISABLE);
                    I2C_GenerateSTOP(i2c_periph, ENABLE);
                }
                else if (g_i2c_rx_index[instance] == g_i2c_rx_length[instance] - 2)
                {
                    I2C_AcknowledgeConfig(i2c_periph, DISABLE);
                }
                
                if (g_i2c_rx_index[instance] >= g_i2c_rx_length[instance])
                {
                    /* 接收完成 */
                    g_i2c_rx_buffer[instance] = NULL;
                    I2C_AcknowledgeConfig(i2c_periph, ENABLE);
                    g_i2c_status[instance] = I2C_OK;
                    
                    /* 调用回调函数 */
                    if (g_i2c_callback[instance] != NULL)
                    {
                        g_i2c_callback[instance](instance, I2C_OK);
                    }
                }
            }
        }
    }
}

/**
 * @brief I2C错误中断服务函数
 */
void I2C_ER_IRQHandler(I2C_Instance_t instance)
{
    I2C_TypeDef *i2c_periph;
    
    if (instance >= I2C_INSTANCE_MAX || !g_i2c_initialized[instance])
    {
        return;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    
    /* 总线错误 */
    if (I2C_GetITStatus(i2c_periph, I2C_IT_BERR) != RESET)
    {
        I2C_ClearITPendingBit(i2c_periph, I2C_IT_BERR);
        g_i2c_status[instance] = I2C_ERROR_BUS_ERROR;
    }
    
    /* 仲裁丢失 */
    if (I2C_GetITStatus(i2c_periph, I2C_IT_ARLO) != RESET)
    {
        I2C_ClearITPendingBit(i2c_periph, I2C_IT_ARLO);
        g_i2c_status[instance] = I2C_ERROR_ARBITRATION_LOST;
    }
    
    /* 应答失败 */
    if (I2C_GetITStatus(i2c_periph, I2C_IT_AF) != RESET)
    {
        I2C_ClearITPendingBit(i2c_periph, I2C_IT_AF);
        I2C_GenerateSTOP(i2c_periph, ENABLE);
        g_i2c_status[instance] = I2C_ERROR_NACK;
    }
    
    /* 清除发送/接收缓冲区 */
    g_i2c_tx_buffer[instance] = NULL;
    g_i2c_rx_buffer[instance] = NULL;
    
    /* 调用回调函数 */
    if (g_i2c_callback[instance] != NULL)
    {
        g_i2c_callback[instance](instance, g_i2c_status[instance]);
    }
}

/* I2C中断服务程序入口 */
void I2C1_EV_IRQHandler(void)
{
    I2C_EV_IRQHandler(I2C_INSTANCE_1);
}

void I2C1_ER_IRQHandler(void)
{
    I2C_ER_IRQHandler(I2C_INSTANCE_1);
}

void I2C2_EV_IRQHandler(void)
{
    I2C_EV_IRQHandler(I2C_INSTANCE_2);
}

void I2C2_ER_IRQHandler(void)
{
    I2C_ER_IRQHandler(I2C_INSTANCE_2);
}

/* ========== 10位地址功能实现 ========== */
/* 注意：10位地址功能已在前面实现（第1010行和第1124行），这里不再重复定义 */

/* ========== 从模式功能实现 ========== */

/* 从模式回调函数（已在文件开头定义，第47-49行） */

/**
 * @brief I2C从模式初始化
 */
I2C_Status_t I2C_SlaveInit(I2C_Instance_t instance, uint8_t slave_address,
                           I2C_SlaveCallback_t callback, void *user_data)
{
    I2C_InitTypeDef I2C_InitStructure;
    
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (slave_address > 0x7F)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    /* 如果已初始化为主模式，需要先反初始化 */
    if (g_i2c_initialized[instance])
    {
        I2C_Deinit(instance);
    }
    
    /* 使能I2C外设时钟 */
    RCC_APB1PeriphClockCmd(I2C_GetPeriphClock(g_i2c_configs[instance].i2c_periph), ENABLE);
    
    /* 配置I2C为从模式 */
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = slave_address << 1;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = 100000;
    
    I2C_Init(g_i2c_configs[instance].i2c_periph, &I2C_InitStructure);
    I2C_Cmd(g_i2c_configs[instance].i2c_periph, ENABLE);
    
    /* 使能地址匹配中断 */
    I2C_ITConfig(g_i2c_configs[instance].i2c_periph, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR, ENABLE);
    
    g_i2c_slave_callback[instance] = callback;
    g_i2c_slave_user_data[instance] = user_data;
    g_i2c_slave_mode[instance] = true;
    g_i2c_initialized[instance] = true;
    
    return I2C_OK;
}

/**
 * @brief I2C从模式反初始化
 */
I2C_Status_t I2C_SlaveDeinit(I2C_Instance_t instance)
{
    if (instance >= I2C_INSTANCE_MAX || !g_i2c_slave_mode[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    I2C_ITConfig(g_i2c_configs[instance].i2c_periph, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR, DISABLE);
    I2C_Cmd(g_i2c_configs[instance].i2c_periph, DISABLE);
    
    g_i2c_slave_callback[instance] = NULL;
    g_i2c_slave_user_data[instance] = NULL;
    g_i2c_slave_mode[instance] = false;
    g_i2c_initialized[instance] = false;
    
    return I2C_OK;
}

/**
 * @brief I2C从模式发送数据
 */
I2C_Status_t I2C_SlaveTransmit(I2C_Instance_t instance, uint8_t data)
{
    if (instance >= I2C_INSTANCE_MAX || !g_i2c_slave_mode[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    uint32_t timeout = Delay_GetTick() + I2C_DEFAULT_TIMEOUT_MS;
    while (I2C_GetFlagStatus(g_i2c_configs[instance].i2c_periph, I2C_FLAG_TXE) == RESET)
    {
        if (Delay_GetElapsed(Delay_GetTick(), timeout) > I2C_DEFAULT_TIMEOUT_MS)
        {
            return I2C_ERROR_TIMEOUT;
        }
    }
    
    I2C_SendData(g_i2c_configs[instance].i2c_periph, data);
    return I2C_OK;
}

/**
 * @brief I2C从模式接收数据
 */
I2C_Status_t I2C_SlaveReceive(I2C_Instance_t instance, uint8_t *data)
{
    if (instance >= I2C_INSTANCE_MAX || !g_i2c_slave_mode[instance] || data == NULL)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    uint32_t timeout = Delay_GetTick() + I2C_DEFAULT_TIMEOUT_MS;
    while (I2C_GetFlagStatus(g_i2c_configs[instance].i2c_periph, I2C_FLAG_RXNE) == RESET)
    {
        if (Delay_GetElapsed(Delay_GetTick(), timeout) > I2C_DEFAULT_TIMEOUT_MS)
        {
            return I2C_ERROR_TIMEOUT;
        }
    }
    
    *data = I2C_ReceiveData(g_i2c_configs[instance].i2c_periph);
    return I2C_OK;
}

/* 修改I2C_EV_IRQHandler以支持从模式 - 需要在现有函数中添加从模式处理 */
/* 由于I2C_EV_IRQHandler已经存在，我们需要在函数开头检查是否为从模式 */

/* ========== SMBus/PEC功能实现 ========== */

/**
 * @brief 使能I2C PEC计算
 */
I2C_Status_t I2C_EnablePEC(I2C_Instance_t instance)
{
    I2C_TypeDef *i2c_periph;
    
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    
    /* 使能PEC计算 */
    I2C_CalculatePEC(i2c_periph, ENABLE);
    
    return I2C_OK;
}

/**
 * @brief 禁用I2C PEC计算
 */
I2C_Status_t I2C_DisablePEC(I2C_Instance_t instance)
{
    I2C_TypeDef *i2c_periph;
    
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    
    /* 禁用PEC计算 */
    I2C_CalculatePEC(i2c_periph, DISABLE);
    
    return I2C_OK;
}

/**
 * @brief 使能I2C PEC传输
 */
I2C_Status_t I2C_EnablePECTransmission(I2C_Instance_t instance)
{
    I2C_TypeDef *i2c_periph;
    
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    
    /* 使能PEC传输 */
    I2C_TransmitPEC(i2c_periph, ENABLE);
    
    return I2C_OK;
}

/**
 * @brief 禁用I2C PEC传输
 */
I2C_Status_t I2C_DisablePECTransmission(I2C_Instance_t instance)
{
    I2C_TypeDef *i2c_periph;
    
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    
    /* 禁用PEC传输 */
    I2C_TransmitPEC(i2c_periph, DISABLE);
    
    return I2C_OK;
}

/**
 * @brief 配置I2C PEC位置
 */
I2C_Status_t I2C_ConfigPECPosition(I2C_Instance_t instance, I2C_PECPosition_t position)
{
    I2C_TypeDef *i2c_periph;
    uint16_t pec_position;
    
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    if (position >= 2)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    
    /* 转换PEC位置枚举值 */
    pec_position = (position == I2C_PEC_POSITION_NEXT) ? I2C_PECPosition_Next : I2C_PECPosition_Current;
    
    /* 配置PEC位置 */
    I2C_PECPositionConfig(i2c_periph, pec_position);
    
    return I2C_OK;
}

/**
 * @brief 获取I2C PEC值
 */
uint8_t I2C_HW_GetPEC(I2C_Instance_t instance)
{
    I2C_TypeDef *i2c_periph;
    
    if (instance >= I2C_INSTANCE_MAX)
    {
        return 0;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return 0;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    
    /* 读取PEC值（调用标准库函数） */
    return I2C_GetPEC(i2c_periph);
}

/**
 * @brief 配置I2C SMBus Alert引脚
 */
I2C_Status_t I2C_ConfigSMBusAlert(I2C_Instance_t instance, I2C_SMBusAlert_t alert_level)
{
    I2C_TypeDef *i2c_periph;
    uint16_t smbus_alert;
    
    if (instance >= I2C_INSTANCE_MAX)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_i2c_initialized[instance])
    {
        return I2C_ERROR_NOT_INITIALIZED;
    }
    
    if (alert_level >= 2)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    
    i2c_periph = g_i2c_configs[instance].i2c_periph;
    
    /* 转换SMBus Alert枚举值 */
    smbus_alert = (alert_level == I2C_SMBUS_ALERT_LOW) ? I2C_SMBusAlert_Low : I2C_SMBusAlert_High;
    
    /* 配置SMBus Alert引脚 */
    I2C_SMBusAlertConfig(i2c_periph, smbus_alert);
    
    return I2C_OK;
}

#endif /* CONFIG_MODULE_I2C_ENABLED */

