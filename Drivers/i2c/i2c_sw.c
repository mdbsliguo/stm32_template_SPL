/**
 * @file i2c_sw.c
 * @brief 软件I2C驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于GPIO的软件I2C驱动，支持主模式、7位/10位地址、总线扫描等功能
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_SOFT_I2C_ENABLED

#include "i2c_sw.h"
#include "gpio.h"
#include "delay.h"
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>

/* 软件I2C配置表（从board.h中获取） */
#define SOFT_I2C_CONFIGS_DECLARE \
    static const SoftI2C_Config_t g_soft_i2c_configs[SOFT_I2C_INSTANCE_MAX] = SOFT_I2C_CONFIGS;

SOFT_I2C_CONFIGS_DECLARE

/* 初始化状态标志 */
static bool g_soft_i2c_initialized[SOFT_I2C_INSTANCE_MAX] = {false, false, false, false};

/* 默认超时时间（毫秒） */
#define SOFT_I2C_DEFAULT_TIMEOUT_MS  1000
#define SOFT_I2C_SCAN_DEFAULT_TIMEOUT_MS  10  /* 总线扫描默认超时 */

/* 参数校验宏 */
#define SOFT_I2C_CHECK_INIT(inst) \
    do { \
        if ((inst) >= SOFT_I2C_INSTANCE_MAX) \
            return SOFT_I2C_ERROR_INVALID_PARAM; \
        if (!g_soft_i2c_initialized[inst]) \
            return SOFT_I2C_ERROR_NOT_INITIALIZED; \
    } while(0)

#define SOFT_I2C_CHECK_PARAM(ptr) \
    do { \
        if ((ptr) == NULL) \
            return SOFT_I2C_ERROR_INVALID_PARAM; \
    } while(0)

/**
 * @brief 计算实际超时时间
 * @param[in] timeout_ms 用户指定的超时时间
 * @return uint32_t 实际超时时间（毫秒），0表示不超时
 */
static inline uint32_t SoftI2C_GetActualTimeout(uint32_t timeout_ms)
{
    if (timeout_ms == 0)
    {
        return 0;  /* 不超时 */
    }
    else if (timeout_ms == UINT32_MAX)
    {
        return SOFT_I2C_DEFAULT_TIMEOUT_MS;  /* 使用默认超时 */
    }
    else
    {
        return timeout_ms;  /* 使用指定超时 */
    }
}

/**
 * @brief 检查超时
 * @param[in] start_tick 开始时间
 * @param[in] timeout_ms 超时时间（毫秒），0表示不超时
 * @return uint8_t 1=超时，0=未超时
 */
static inline uint8_t SoftI2C_CheckTimeout(uint32_t start_tick, uint32_t timeout_ms)
{
    if (timeout_ms == 0)
    {
        return 0;  /* 不超时 */
    }
    
    uint32_t current_tick = Delay_GetTick();
    uint32_t elapsed = Delay_GetElapsed(current_tick, start_tick);
    
    return (elapsed >= timeout_ms) ? 1 : 0;
}

/* 超时检查宏：检查超时并返回错误 */
#define SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick) \
    do { \
        if (actual_timeout > 0 && SoftI2C_CheckTimeout(start_tick, actual_timeout)) { \
            SoftI2C_Stop(instance); \
            return SOFT_I2C_ERROR_TIMEOUT; \
        } \
    } while(0)

/* 错误处理宏：NACK错误处理 */
#define SOFT_I2C_HANDLE_NACK(instance) \
    do { \
        SoftI2C_Stop(instance); \
        return SOFT_I2C_ERROR_NACK; \
    } while(0)

/* ==================== 内部函数 ==================== */

/**
 * @brief 软件I2C延时
 * @param[in] instance 软件I2C实例索引
 * @param[in] delay_us 延时时间（微秒）
 */
static void SoftI2C_Delay(SoftI2C_Instance_t instance, uint32_t delay_us)
{
    if (delay_us > 0)
    {
        Delay_us(delay_us);
    }
}

/**
 * @brief 软件I2C起始信号
 * @param[in] instance 软件I2C实例索引
 */
static void SoftI2C_Start(SoftI2C_Instance_t instance)
{
    GPIO_TypeDef *scl_port = g_soft_i2c_configs[instance].scl_port;
    uint16_t scl_pin = g_soft_i2c_configs[instance].scl_pin;
    GPIO_TypeDef *sda_port = g_soft_i2c_configs[instance].sda_port;
    uint16_t sda_pin = g_soft_i2c_configs[instance].sda_pin;
    uint32_t delay = g_soft_i2c_configs[instance].delay_us;
    
    /* 确保SDA为高 */
    GPIO_WritePin(sda_port, sda_pin, Bit_SET);
    SoftI2C_Delay(instance, delay);
    
    /* 确保SCL为高 */
    GPIO_WritePin(scl_port, scl_pin, Bit_SET);
    SoftI2C_Delay(instance, delay);
    
    /* SCL高电平时拉低SDA，产生起始信号 */
    GPIO_WritePin(sda_port, sda_pin, Bit_RESET);
    SoftI2C_Delay(instance, delay);
    
    /* 拉低SCL，准备发送数据 */
    GPIO_WritePin(scl_port, scl_pin, Bit_RESET);
    SoftI2C_Delay(instance, delay);
}

/**
 * @brief 软件I2C停止信号
 * @param[in] instance 软件I2C实例索引
 */
static void SoftI2C_Stop(SoftI2C_Instance_t instance)
{
    GPIO_TypeDef *scl_port = g_soft_i2c_configs[instance].scl_port;
    uint16_t scl_pin = g_soft_i2c_configs[instance].scl_pin;
    GPIO_TypeDef *sda_port = g_soft_i2c_configs[instance].sda_port;
    uint16_t sda_pin = g_soft_i2c_configs[instance].sda_pin;
    uint32_t delay = g_soft_i2c_configs[instance].delay_us;
    
    /* 确保SDA为低 */
    GPIO_WritePin(sda_port, sda_pin, Bit_RESET);
    SoftI2C_Delay(instance, delay);
    
    /* 拉高SCL */
    GPIO_WritePin(scl_port, scl_pin, Bit_SET);
    SoftI2C_Delay(instance, delay);
    
    /* SCL高电平时拉高SDA，产生停止信号 */
    GPIO_WritePin(sda_port, sda_pin, Bit_SET);
    SoftI2C_Delay(instance, delay);
}

/**
 * @brief 软件I2C发送一个字节
 * @param[in] instance 软件I2C实例索引
 * @param[in] byte 要发送的字节
 * @return uint8_t 0=收到ACK，1=收到NACK
 */
static uint8_t SoftI2C_SendByte(SoftI2C_Instance_t instance, uint8_t byte)
{
    GPIO_TypeDef *scl_port = g_soft_i2c_configs[instance].scl_port;
    uint16_t scl_pin = g_soft_i2c_configs[instance].scl_pin;
    GPIO_TypeDef *sda_port = g_soft_i2c_configs[instance].sda_port;
    uint16_t sda_pin = g_soft_i2c_configs[instance].sda_pin;
    uint32_t delay = g_soft_i2c_configs[instance].delay_us;
    uint8_t i;
    uint8_t ack;
    
    /* 循环发送8位数据（高位在前） */
    for (i = 0; i < 8; i++)
    {
        /* 设置SDA电平（根据byte的对应位） */
        GPIO_WritePin(sda_port, sda_pin, (BitAction)(!!(byte & (0x80 >> i))));
        SoftI2C_Delay(instance, delay);
        
        /* 拉高SCL，从机读取SDA */
        GPIO_WritePin(scl_port, scl_pin, Bit_SET);
        SoftI2C_Delay(instance, delay);
        
        /* 拉低SCL，准备下一位 */
        GPIO_WritePin(scl_port, scl_pin, Bit_RESET);
        SoftI2C_Delay(instance, delay);
    }
    
    /* 释放SDA，准备接收ACK */
    GPIO_WritePin(sda_port, sda_pin, Bit_SET);
    SoftI2C_Delay(instance, delay);
    
    /* 拉高SCL，读取ACK */
    GPIO_WritePin(scl_port, scl_pin, Bit_SET);
    SoftI2C_Delay(instance, delay);
    
    /* 读取ACK信号（0=ACK，1=NACK） */
    ack = GPIO_ReadPin(sda_port, sda_pin);
    
    /* 拉低SCL */
    GPIO_WritePin(scl_port, scl_pin, Bit_RESET);
    SoftI2C_Delay(instance, delay);
    
    return ack;
}

/**
 * @brief 软件I2C接收一个字节
 * @param[in] instance 软件I2C实例索引
 * @param[in] ack 是否发送ACK（0=ACK，1=NACK）
 * @return uint8_t 接收到的字节
 */
static uint8_t SoftI2C_ReceiveByte(SoftI2C_Instance_t instance, uint8_t ack)
{
    GPIO_TypeDef *scl_port = g_soft_i2c_configs[instance].scl_port;
    uint16_t scl_pin = g_soft_i2c_configs[instance].scl_pin;
    GPIO_TypeDef *sda_port = g_soft_i2c_configs[instance].sda_port;
    uint16_t sda_pin = g_soft_i2c_configs[instance].sda_pin;
    uint32_t delay = g_soft_i2c_configs[instance].delay_us;
    uint8_t i;
    uint8_t byte = 0;
    
    /* 释放SDA，准备接收数据 */
    GPIO_WritePin(sda_port, sda_pin, Bit_SET);
    SoftI2C_Delay(instance, delay);
    
    /* 循环接收8位数据（高位在前） */
    for (i = 0; i < 8; i++)
    {
        /* 拉高SCL，从机发送数据 */
        GPIO_WritePin(scl_port, scl_pin, Bit_SET);
        SoftI2C_Delay(instance, delay);
        
        /* 读取SDA电平（高位在前） */
        byte <<= 1;
        if (GPIO_ReadPin(sda_port, sda_pin))
        {
            byte |= 0x01;
        }
        
        /* 拉低SCL，准备下一位 */
        GPIO_WritePin(scl_port, scl_pin, Bit_RESET);
        SoftI2C_Delay(instance, delay);
    }
    
    /* 发送ACK/NACK */
    GPIO_WritePin(sda_port, sda_pin, (BitAction)ack);
    SoftI2C_Delay(instance, delay);
    
    /* 拉高SCL，从机读取ACK */
    GPIO_WritePin(scl_port, scl_pin, Bit_SET);
    SoftI2C_Delay(instance, delay);
    
    /* 拉低SCL */
    GPIO_WritePin(scl_port, scl_pin, Bit_RESET);
    SoftI2C_Delay(instance, delay);
    
    return byte;
}

/**
 * @brief 软件I2C重复起始信号
 * @param[in] instance 软件I2C实例索引
 */
static void SoftI2C_Restart(SoftI2C_Instance_t instance)
{
    GPIO_TypeDef *scl_port = g_soft_i2c_configs[instance].scl_port;
    uint16_t scl_pin = g_soft_i2c_configs[instance].scl_pin;
    GPIO_TypeDef *sda_port = g_soft_i2c_configs[instance].sda_port;
    uint16_t sda_pin = g_soft_i2c_configs[instance].sda_pin;
    uint32_t delay = g_soft_i2c_configs[instance].delay_us;
    
    /* 拉高SCL */
    GPIO_WritePin(scl_port, scl_pin, Bit_SET);
    SoftI2C_Delay(instance, delay);
    
    /* 拉高SDA */
    GPIO_WritePin(sda_port, sda_pin, Bit_SET);
    SoftI2C_Delay(instance, delay);
    
    /* SCL高电平时拉低SDA，产生重复起始信号 */
    GPIO_WritePin(sda_port, sda_pin, Bit_RESET);
    SoftI2C_Delay(instance, delay);
    
    /* 拉低SCL，准备发送数据 */
    GPIO_WritePin(scl_port, scl_pin, Bit_RESET);
    SoftI2C_Delay(instance, delay);
}

/**
 * @brief 计算10位I2C地址的两个字节
 * @param[in] slave_addr 10位从机地址（0x000-0x3FF）
 * @param[out] addr_byte1 第一个地址字节（11110XX + A9A8 + R/W）
 * @param[out] addr_byte2 第二个地址字节（A7A6A5A4A3A2A1A0）
 * @note 第一个字节的bit0用于R/W位，需要在调用后根据读写模式设置
 * @note I2C 10位地址格式：
 *       - 第一个字节：11110 + A9 + A8 + R/W（bit7-5=111固定，bit4-3=10固定，bit2-1=A9A8，bit0=R/W）
 *       - 第二个字节：A7A6A5A4A3A2A1A0（地址的低8位）
 */
static inline void SoftI2C_Calc10bitAddress(uint16_t slave_addr, uint8_t *addr_byte1, uint8_t *addr_byte2)
{
    /* 10位地址格式：11110XX + A9A8 + R/W */
    /* 第一个字节：11110 + A9 + A8 + R/W（bit0用于R/W，由调用者设置） */
    /* bit7-5=111（固定），bit4-3=10（固定），bit2-1=A9A8，bit0=R/W */
    *addr_byte1 = 0xF0 | (((slave_addr >> 8) & 0x03) << 1);
    /* 第二个字节：A7A6A5A4A3A2A1A0 */
    *addr_byte2 = slave_addr & 0xFF;
}

/* ==================== 公共API函数 ==================== */

/**
 * @brief 软件I2C初始化
 */
SoftI2C_Status_t I2C_SW_Init(SoftI2C_Instance_t instance)
{
    GPIO_Status_t gpio_status;
    
    /* 参数校验 */
    if (instance >= SOFT_I2C_INSTANCE_MAX)
    {
        return SOFT_I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_soft_i2c_configs[instance].enabled)
    {
        return SOFT_I2C_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已初始化 */
    if (g_soft_i2c_initialized[instance])
    {
        return SOFT_I2C_OK;
    }
    
    /* 使能GPIO时钟 */
    gpio_status = GPIO_EnableClock(g_soft_i2c_configs[instance].scl_port);
    if (gpio_status != GPIO_OK)
    {
        return SOFT_I2C_ERROR_GPIO_FAILED;
    }
    
    if (g_soft_i2c_configs[instance].scl_port != g_soft_i2c_configs[instance].sda_port)
    {
        gpio_status = GPIO_EnableClock(g_soft_i2c_configs[instance].sda_port);
        if (gpio_status != GPIO_OK)
        {
            return SOFT_I2C_ERROR_GPIO_FAILED;
        }
    }
    
    /* 配置SCL引脚为开漏输出 */
    gpio_status = GPIO_Config(g_soft_i2c_configs[instance].scl_port, 
                              g_soft_i2c_configs[instance].scl_pin, 
                              GPIO_MODE_OUTPUT_OD, GPIO_SPEED_50MHz);
    if (gpio_status != GPIO_OK)
    {
        return SOFT_I2C_ERROR_GPIO_FAILED;
    }
    
    /* 配置SDA引脚为开漏输出 */
    gpio_status = GPIO_Config(g_soft_i2c_configs[instance].sda_port, 
                              g_soft_i2c_configs[instance].sda_pin, 
                              GPIO_MODE_OUTPUT_OD, GPIO_SPEED_50MHz);
    if (gpio_status != GPIO_OK)
    {
        return SOFT_I2C_ERROR_GPIO_FAILED;
    }
    
    /* 设置I2C总线空闲状态（SCL=1，SDA=1） */
    GPIO_WritePin(g_soft_i2c_configs[instance].scl_port, g_soft_i2c_configs[instance].scl_pin, Bit_SET);
    GPIO_WritePin(g_soft_i2c_configs[instance].sda_port, g_soft_i2c_configs[instance].sda_pin, Bit_SET);
    
    /* 延时确保总线稳定 */
    Delay_us(10);
    
    /* 标记为已初始化 */
    g_soft_i2c_initialized[instance] = true;
    
    return SOFT_I2C_OK;
}

/**
 * @brief 软件I2C反初始化
 */
SoftI2C_Status_t SoftI2C_Deinit(SoftI2C_Instance_t instance)
{
    /* 参数校验 */
    if (instance >= SOFT_I2C_INSTANCE_MAX)
    {
        return SOFT_I2C_ERROR_INVALID_PARAM;
    }
    
    if (!g_soft_i2c_initialized[instance])
    {
        return SOFT_I2C_OK;
    }
    
    /* 确保总线处于空闲状态 */
    GPIO_WritePin(g_soft_i2c_configs[instance].scl_port, g_soft_i2c_configs[instance].scl_pin, Bit_SET);
    GPIO_WritePin(g_soft_i2c_configs[instance].sda_port, g_soft_i2c_configs[instance].sda_pin, Bit_SET);
    
    /* 标记为未初始化 */
    g_soft_i2c_initialized[instance] = false;
    
    return SOFT_I2C_OK;
}

/**
 * @brief 检查软件I2C是否已初始化
 */
uint8_t SoftI2C_IsInitialized(SoftI2C_Instance_t instance)
{
    if (instance >= SOFT_I2C_INSTANCE_MAX)
    {
        return 0;
    }
    
    return g_soft_i2c_initialized[instance] ? 1 : 0;
}

/**
 * @brief 软件I2C主模式发送数据
 */
SoftI2C_Status_t SoftI2C_MasterTransmit(SoftI2C_Instance_t instance, uint8_t slave_addr, 
                                        const uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
    uint16_t i;
    uint32_t actual_timeout = SoftI2C_GetActualTimeout(timeout_ms);
    uint32_t start_tick = 0;
    
    /* 参数校验 */
    SOFT_I2C_CHECK_INIT(instance);
    SOFT_I2C_CHECK_PARAM(data);
    
    if (length == 0)
    {
        return SOFT_I2C_ERROR_INVALID_PARAM;
    }
    
    if (actual_timeout > 0)
    {
        start_tick = Delay_GetTick();
    }
    
    /* 起始信号 */
    SoftI2C_Start(instance);
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送从机地址（写模式，7位地址左移1位） */
    if (SoftI2C_SendByte(instance, slave_addr << 1))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送数据 */
    for (i = 0; i < length; i++)
    {
        if (SoftI2C_SendByte(instance, data[i]))
        {
            SOFT_I2C_HANDLE_NACK(instance);
        }
        
        SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    }
    
    /* 停止信号 */
    SoftI2C_Stop(instance);
    
    return SOFT_I2C_OK;
}

/**
 * @brief 软件I2C主模式接收数据
 */
SoftI2C_Status_t SoftI2C_MasterReceive(SoftI2C_Instance_t instance, uint8_t slave_addr, 
                                        uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
    uint16_t i;
    uint32_t actual_timeout = SoftI2C_GetActualTimeout(timeout_ms);
    uint32_t start_tick = 0;
    
    /* 参数校验 */
    SOFT_I2C_CHECK_INIT(instance);
    SOFT_I2C_CHECK_PARAM(data);
    
    if (length == 0)
    {
        return SOFT_I2C_ERROR_INVALID_PARAM;
    }
    
    if (actual_timeout > 0)
    {
        start_tick = Delay_GetTick();
    }
    
    /* 起始信号 */
    SoftI2C_Start(instance);
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送从机地址（读模式，7位地址左移1位后或0x01） */
    if (SoftI2C_SendByte(instance, (slave_addr << 1) | 0x01))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 接收数据 */
    for (i = 0; i < length; i++)
    {
        if (i == length - 1)
        {
            /* 最后一个字节：发送NACK */
            data[i] = SoftI2C_ReceiveByte(instance, 1);
        }
        else
        {
            /* 其他字节：发送ACK */
            data[i] = SoftI2C_ReceiveByte(instance, 0);
        }
        
        SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    }
    
    /* 停止信号 */
    SoftI2C_Stop(instance);
    
    return SOFT_I2C_OK;
}

/**
 * @brief 软件I2C主模式写寄存器
 */
SoftI2C_Status_t SoftI2C_MasterWriteReg(SoftI2C_Instance_t instance, uint8_t slave_addr, 
                                         uint8_t reg_addr, uint8_t reg_value, uint32_t timeout_ms)
{
    uint32_t actual_timeout = SoftI2C_GetActualTimeout(timeout_ms);
    uint32_t start_tick = 0;
    
    /* 参数校验 */
    SOFT_I2C_CHECK_INIT(instance);
    
    if (actual_timeout > 0)
    {
        start_tick = Delay_GetTick();
    }
    
    /* 起始信号 */
    SoftI2C_Start(instance);
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送从机地址（写模式） */
    if (SoftI2C_SendByte(instance, slave_addr << 1))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送寄存器地址 */
    if (SoftI2C_SendByte(instance, reg_addr))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送寄存器值 */
    if (SoftI2C_SendByte(instance, reg_value))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    /* 停止信号 */
    SoftI2C_Stop(instance);
    
    return SOFT_I2C_OK;
}

/**
 * @brief 软件I2C主模式读寄存器
 */
SoftI2C_Status_t SoftI2C_MasterReadReg(SoftI2C_Instance_t instance, uint8_t slave_addr, 
                                        uint8_t reg_addr, uint8_t *reg_value, uint32_t timeout_ms)
{
    uint32_t actual_timeout = SoftI2C_GetActualTimeout(timeout_ms);
    uint32_t start_tick = 0;
    
    /* 参数校验 */
    SOFT_I2C_CHECK_INIT(instance);
    SOFT_I2C_CHECK_PARAM(reg_value);
    
    if (actual_timeout > 0)
    {
        start_tick = Delay_GetTick();
    }
    
    /* 起始信号 */
    SoftI2C_Start(instance);
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送从机地址（写模式） */
    if (SoftI2C_SendByte(instance, slave_addr << 1))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送寄存器地址 */
    if (SoftI2C_SendByte(instance, reg_addr))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 重复起始信号 */
    SoftI2C_Restart(instance);
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送从机地址（读模式） */
    if (SoftI2C_SendByte(instance, (slave_addr << 1) | 0x01))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 接收数据（发送NACK） */
    *reg_value = SoftI2C_ReceiveByte(instance, 1);
    
    /* 停止信号 */
    SoftI2C_Stop(instance);
    
    return SOFT_I2C_OK;
}

/**
 * @brief 软件I2C主模式写多个寄存器
 */
SoftI2C_Status_t SoftI2C_MasterWriteRegs(SoftI2C_Instance_t instance, uint8_t slave_addr, 
                                          uint8_t reg_addr, const uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
    uint16_t i;
    uint32_t actual_timeout = SoftI2C_GetActualTimeout(timeout_ms);
    uint32_t start_tick = 0;
    
    /* 参数校验 */
    SOFT_I2C_CHECK_INIT(instance);
    SOFT_I2C_CHECK_PARAM(data);
    
    if (length == 0)
    {
        return SOFT_I2C_ERROR_INVALID_PARAM;
    }
    
    if (actual_timeout > 0)
    {
        start_tick = Delay_GetTick();
    }
    
    /* 起始信号 */
    SoftI2C_Start(instance);
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送从机地址（写模式） */
    if (SoftI2C_SendByte(instance, slave_addr << 1))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送寄存器地址 */
    if (SoftI2C_SendByte(instance, reg_addr))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送数据 */
    for (i = 0; i < length; i++)
    {
        if (SoftI2C_SendByte(instance, data[i]))
        {
            SOFT_I2C_HANDLE_NACK(instance);
        }
        
        SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    }
    
    /* 停止信号 */
    SoftI2C_Stop(instance);
    
    return SOFT_I2C_OK;
}

/**
 * @brief 软件I2C主模式读多个寄存器
 */
SoftI2C_Status_t SoftI2C_MasterReadRegs(SoftI2C_Instance_t instance, uint8_t slave_addr, 
                                         uint8_t reg_addr, uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
    uint16_t i;
    uint32_t actual_timeout = SoftI2C_GetActualTimeout(timeout_ms);
    uint32_t start_tick = 0;
    
    /* 参数校验 */
    SOFT_I2C_CHECK_INIT(instance);
    SOFT_I2C_CHECK_PARAM(data);
    
    if (length == 0)
    {
        return SOFT_I2C_ERROR_INVALID_PARAM;
    }
    
    if (actual_timeout > 0)
    {
        start_tick = Delay_GetTick();
    }
    
    /* 起始信号 */
    SoftI2C_Start(instance);
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送从机地址（写模式） */
    if (SoftI2C_SendByte(instance, slave_addr << 1))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送寄存器地址 */
    if (SoftI2C_SendByte(instance, reg_addr))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 重复起始信号 */
    SoftI2C_Restart(instance);
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送从机地址（读模式） */
    if (SoftI2C_SendByte(instance, (slave_addr << 1) | 0x01))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 接收数据 */
    for (i = 0; i < length; i++)
    {
        if (i == length - 1)
        {
            /* 最后一个字节：发送NACK */
            data[i] = SoftI2C_ReceiveByte(instance, 1);
        }
        else
        {
            /* 其他字节：发送ACK */
            data[i] = SoftI2C_ReceiveByte(instance, 0);
        }
        
        SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    }
    
    /* 停止信号 */
    SoftI2C_Stop(instance);
    
    return SOFT_I2C_OK;
}

/**
 * @brief 软件I2C总线扫描
 */
SoftI2C_Status_t SoftI2C_ScanBus(SoftI2C_Instance_t instance, uint8_t *found_addr, 
                                  uint8_t max_count, uint8_t *count, uint32_t timeout_ms)
{
    uint8_t addr;
    uint8_t found = 0;
    uint32_t addr_timeout = (timeout_ms == 0) ? SOFT_I2C_SCAN_DEFAULT_TIMEOUT_MS : timeout_ms;
    uint32_t start_tick;
    
    /* 参数校验 */
    SOFT_I2C_CHECK_INIT(instance);
    SOFT_I2C_CHECK_PARAM(found_addr);
    SOFT_I2C_CHECK_PARAM(count);
    
    if (max_count == 0)
    {
        return SOFT_I2C_ERROR_INVALID_PARAM;
    }
    
    *count = 0;
    
    /* 扫描地址范围：0x08-0x77（7位地址，排除保留地址） */
    for (addr = 0x08; addr <= 0x77 && found < max_count; addr++)
    {
        start_tick = Delay_GetTick();
        
        /* 起始信号 */
        SoftI2C_Start(instance);
        
        /* 发送从机地址（写模式） */
        if (SoftI2C_SendByte(instance, addr << 1) == 0)
        {
            /* 收到ACK，设备存在 */
            found_addr[found] = addr;
            found++;
        }
        
        /* 停止信号 */
        SoftI2C_Stop(instance);
        
        /* 短暂延时，确保总线稳定 */
        Delay_us(100);
        
        /* 检查总超时（可选，避免扫描时间过长） */
        if (timeout_ms > 0 && Delay_GetElapsed(Delay_GetTick(), start_tick) > addr_timeout)
        {
            /* 单个地址超时，继续扫描下一个 */
        }
    }
    
    *count = found;
    return SOFT_I2C_OK;
}

/**
 * @brief 软件I2C主模式发送数据（10位地址）
 */
SoftI2C_Status_t SoftI2C_MasterTransmit10bit(SoftI2C_Instance_t instance, uint16_t slave_addr, 
                                               const uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
    uint16_t i;
    uint8_t addr_byte1, addr_byte2;
    uint32_t actual_timeout = SoftI2C_GetActualTimeout(timeout_ms);
    uint32_t start_tick = 0;
    
    /* 参数校验 */
    SOFT_I2C_CHECK_INIT(instance);
    SOFT_I2C_CHECK_PARAM(data);
    
    if (length == 0 || slave_addr > 0x3FF)
    {
        return SOFT_I2C_ERROR_INVALID_PARAM;
    }
    
    if (actual_timeout > 0)
    {
        start_tick = Delay_GetTick();
    }
    
    /* 计算10位地址的两个字节 */
    SoftI2C_Calc10bitAddress(slave_addr, &addr_byte1, &addr_byte2);
    
    /* 起始信号 */
    SoftI2C_Start(instance);
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送10位地址的第一个字节 */
    if (SoftI2C_SendByte(instance, addr_byte1))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送10位地址的第二个字节 */
    if (SoftI2C_SendByte(instance, addr_byte2))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送数据 */
    for (i = 0; i < length; i++)
    {
        if (SoftI2C_SendByte(instance, data[i]))
        {
            SOFT_I2C_HANDLE_NACK(instance);
        }
        
        SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    }
    
    /* 停止信号 */
    SoftI2C_Stop(instance);
    
    return SOFT_I2C_OK;
}

/**
 * @brief 软件I2C主模式接收数据（10位地址）
 */
SoftI2C_Status_t SoftI2C_MasterReceive10bit(SoftI2C_Instance_t instance, uint16_t slave_addr, 
                                             uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
    uint16_t i;
    uint8_t addr_byte1, addr_byte2;
    uint32_t actual_timeout = SoftI2C_GetActualTimeout(timeout_ms);
    uint32_t start_tick = 0;
    
    /* 参数校验 */
    SOFT_I2C_CHECK_INIT(instance);
    SOFT_I2C_CHECK_PARAM(data);
    
    if (length == 0 || slave_addr > 0x3FF)
    {
        return SOFT_I2C_ERROR_INVALID_PARAM;
    }
    
    if (actual_timeout > 0)
    {
        start_tick = Delay_GetTick();
    }
    
    /* 计算10位地址的两个字节 */
    SoftI2C_Calc10bitAddress(slave_addr, &addr_byte1, &addr_byte2);
    
    /* 起始信号 */
    SoftI2C_Start(instance);
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送10位地址的第一个字节（写模式，用于地址传输） */
    if (SoftI2C_SendByte(instance, addr_byte1))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送10位地址的第二个字节 */
    if (SoftI2C_SendByte(instance, addr_byte2))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 重复起始信号 */
    SoftI2C_Restart(instance);
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送10位地址的第一个字节（读模式） */
    addr_byte1 |= 0x01;  /* bit0=1（读） */
    if (SoftI2C_SendByte(instance, addr_byte1))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 接收数据 */
    for (i = 0; i < length; i++)
    {
        if (i == length - 1)
        {
            /* 最后一个字节：发送NACK */
            data[i] = SoftI2C_ReceiveByte(instance, 1);
        }
        else
        {
            /* 其他字节：发送ACK */
            data[i] = SoftI2C_ReceiveByte(instance, 0);
        }
        
        SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    }
    
    /* 停止信号 */
    SoftI2C_Stop(instance);
    
    return SOFT_I2C_OK;
}

/**
 * @brief 软件I2C软件复位
 */
SoftI2C_Status_t SoftI2C_SoftwareReset(SoftI2C_Instance_t instance)
{
    GPIO_TypeDef *scl_port = g_soft_i2c_configs[instance].scl_port;
    uint16_t scl_pin = g_soft_i2c_configs[instance].scl_pin;
    GPIO_TypeDef *sda_port = g_soft_i2c_configs[instance].sda_port;
    uint16_t sda_pin = g_soft_i2c_configs[instance].sda_pin;
    uint32_t delay = g_soft_i2c_configs[instance].delay_us;
    uint8_t i;
    
    /* 参数校验 */
    SOFT_I2C_CHECK_INIT(instance);
    
    /* 确保SDA为高 */
    GPIO_WritePin(sda_port, sda_pin, Bit_SET);
    
    /* 发送9个时钟脉冲（用于恢复总线状态） */
    for (i = 0; i < 9; i++)
    {
        /* 拉低SCL */
        GPIO_WritePin(scl_port, scl_pin, Bit_RESET);
        SoftI2C_Delay(instance, delay);
        
        /* 拉高SCL */
        GPIO_WritePin(scl_port, scl_pin, Bit_SET);
        SoftI2C_Delay(instance, delay);
    }
    
    /* 起始信号 */
    SoftI2C_Start(instance);
    
    /* 停止信号 */
    SoftI2C_Stop(instance);
    
    return SOFT_I2C_OK;
}

/**
 * @brief 检查软件I2C总线是否忙
 */
uint8_t SoftI2C_IsBusBusy(SoftI2C_Instance_t instance)
{
    GPIO_TypeDef *sda_port = g_soft_i2c_configs[instance].sda_port;
    uint16_t sda_pin = g_soft_i2c_configs[instance].sda_pin;
    
    /* 参数校验 */
    if (instance >= SOFT_I2C_INSTANCE_MAX)
    {
        return 1;  /* 参数错误，认为总线忙 */
    }
    
    if (!g_soft_i2c_initialized[instance])
    {
        return 1;  /* 未初始化，认为总线忙 */
    }
    
    /* 读取SDA线状态，如果为低则总线忙 */
    return (GPIO_ReadPin(sda_port, sda_pin) == 0) ? 1 : 0;
}

/**
 * @brief 软件I2C主模式写寄存器（10位地址）
 */
SoftI2C_Status_t SoftI2C_MasterWriteReg10bit(SoftI2C_Instance_t instance, uint16_t slave_addr, 
                                              uint8_t reg_addr, uint8_t reg_value, uint32_t timeout_ms)
{
    uint8_t addr_byte1, addr_byte2;
    uint32_t actual_timeout = SoftI2C_GetActualTimeout(timeout_ms);
    uint32_t start_tick = 0;
    
    /* 参数校验 */
    SOFT_I2C_CHECK_INIT(instance);
    
    if (slave_addr > 0x3FF)
    {
        return SOFT_I2C_ERROR_INVALID_PARAM;
    }
    
    if (actual_timeout > 0)
    {
        start_tick = Delay_GetTick();
    }
    
    /* 计算10位地址的两个字节 */
    SoftI2C_Calc10bitAddress(slave_addr, &addr_byte1, &addr_byte2);
    
    /* 起始信号 */
    SoftI2C_Start(instance);
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送10位地址的第一个字节 */
    if (SoftI2C_SendByte(instance, addr_byte1))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送10位地址的第二个字节 */
    if (SoftI2C_SendByte(instance, addr_byte2))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送寄存器地址 */
    if (SoftI2C_SendByte(instance, reg_addr))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送寄存器值 */
    if (SoftI2C_SendByte(instance, reg_value))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    /* 停止信号 */
    SoftI2C_Stop(instance);
    
    return SOFT_I2C_OK;
}

/**
 * @brief 软件I2C主模式读寄存器（10位地址）
 */
SoftI2C_Status_t SoftI2C_MasterReadReg10bit(SoftI2C_Instance_t instance, uint16_t slave_addr, 
                                             uint8_t reg_addr, uint8_t *reg_value, uint32_t timeout_ms)
{
    uint8_t addr_byte1, addr_byte2;
    uint32_t actual_timeout = SoftI2C_GetActualTimeout(timeout_ms);
    uint32_t start_tick = 0;
    
    /* 参数校验 */
    SOFT_I2C_CHECK_INIT(instance);
    SOFT_I2C_CHECK_PARAM(reg_value);
    
    if (slave_addr > 0x3FF)
    {
        return SOFT_I2C_ERROR_INVALID_PARAM;
    }
    
    if (actual_timeout > 0)
    {
        start_tick = Delay_GetTick();
    }
    
    /* 计算10位地址的两个字节 */
    SoftI2C_Calc10bitAddress(slave_addr, &addr_byte1, &addr_byte2);
    
    /* 起始信号 */
    SoftI2C_Start(instance);
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送10位地址的第一个字节（写模式，用于地址传输） */
    if (SoftI2C_SendByte(instance, addr_byte1))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送10位地址的第二个字节 */
    if (SoftI2C_SendByte(instance, addr_byte2))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送寄存器地址 */
    if (SoftI2C_SendByte(instance, reg_addr))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 重复起始信号 */
    SoftI2C_Restart(instance);
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 发送10位地址的第一个字节（读模式） */
    addr_byte1 |= 0x01;  /* bit0=1（读） */
    if (SoftI2C_SendByte(instance, addr_byte1))
    {
        SOFT_I2C_HANDLE_NACK(instance);
    }
    
    SOFT_I2C_CHECK_TIMEOUT(instance, actual_timeout, start_tick);
    
    /* 接收数据（发送NACK） */
    *reg_value = SoftI2C_ReceiveByte(instance, 1);
    
    /* 停止信号 */
    SoftI2C_Stop(instance);
    
    return SOFT_I2C_OK;
}

/**
 * @brief 获取软件I2C配置信息
 */
SoftI2C_Status_t SoftI2C_GetConfig(SoftI2C_Instance_t instance, SoftI2C_ConfigInfo_t *config_info)
{
    /* 参数校验 */
    if (instance >= SOFT_I2C_INSTANCE_MAX)
    {
        return SOFT_I2C_ERROR_INVALID_PARAM;
    }
    
    SOFT_I2C_CHECK_PARAM(config_info);
    
    /* 复制配置信息 */
    config_info->scl_port = g_soft_i2c_configs[instance].scl_port;
    config_info->scl_pin = g_soft_i2c_configs[instance].scl_pin;
    config_info->sda_port = g_soft_i2c_configs[instance].sda_port;
    config_info->sda_pin = g_soft_i2c_configs[instance].sda_pin;
    config_info->delay_us = g_soft_i2c_configs[instance].delay_us;
    config_info->enabled = g_soft_i2c_configs[instance].enabled;
    
    return SOFT_I2C_OK;
}

#endif /* CONFIG_MODULE_SOFT_I2C_ENABLED */
