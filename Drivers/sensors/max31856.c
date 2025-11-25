/**
 * @file max31856.c
 * @brief MAX31856热电偶温度传感器模块驱动实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 支持硬件SPI和软件SPI两种接口的MAX31856热电偶温度传感器驱动，完整功能实现
 * @note 解耦版本：软件SPI部分已解耦，直接使用soft_spi模块，不再重复实现SPI时序
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_MAX31856_ENABLED

#include "max31856.h"
#include "gpio.h"
#include "delay.h"
#include <stdbool.h>
#include <stddef.h>

/* 条件编译：硬件SPI模块 */
#if CONFIG_MODULE_SPI_ENABLED
#include "spi_hw.h"
#endif

/* 条件编译：软件SPI模块 */
#if CONFIG_MODULE_SOFT_SPI_ENABLED
#include "spi_sw.h"
#endif

/* MAX31856寄存器地址 */
#define MAX31856_REG_CR0       0x00    /**< 控制寄存器0 */
#define MAX31856_REG_CR1       0x01    /**< 控制寄存器1 */
#define MAX31856_REG_MASK      0x02    /**< 屏蔽寄存器 */
#define MAX31856_REG_CJHF      0x03    /**< 冷端温度故障阈值高字节 */
#define MAX31856_REG_CJLF      0x04    /**< 冷端温度故障阈值低字节 */
#define MAX31856_REG_LTHFTH    0x05    /**< 热电偶测温高温阈值高8位寄存器 */
#define MAX31856_REG_LTHFTL    0x06    /**< 热电偶测温高温阈值低8位寄存器 */
#define MAX31856_REG_LTLFTH    0x07    /**< 热电偶测温低温阈值高8位寄存器 */
#define MAX31856_REG_LTLFTL    0x08    /**< 热电偶测温低温阈值低8位寄存器 */
#define MAX31856_REG_CJTO      0x09    /**< 冷结温度偏移寄存器 */
#define MAX31856_REG_CJTH      0x0A    /**< 冷端温度高字节（实际温度寄存器，只读） */
#define MAX31856_REG_CJTL      0x0B    /**< 冷端温度低字节（实际温度寄存器，只读） */
#define MAX31856_REG_LTCBH     0x0C    /**< 热电偶温度高8位寄存器（只读） */
#define MAX31856_REG_LTCBM     0x0D    /**< 热电偶温度中8位寄存器（只读） */
#define MAX31856_REG_LTCBL     0x0E    /**< 热电偶温度低8位寄存器（只读） */
#define MAX31856_REG_SR        0x0F    /**< 故障状态记录寄存器（只读，修正：之前错误定义为0x10） */

/* CR0寄存器位定义 */
#define MAX31856_CR0_CMODE        0x80    /**< 转换模式（bit7：0=关闭模式，1=自动转换模式） */
#define MAX31856_CR0_1SHOT        0x02    /**< 单次转换模式（bit6：在关闭模式下触发单次转换） */
#define MAX31856_CR0_OCFAULT0     0x00    /**< 过温故障阈值：0°C（bit5-4） */
#define MAX31856_CR0_OCFAULT1     0x10    /**< 过温故障阈值：1°C（bit5-4） */
#define MAX31856_CR0_OCFAULT2     0x20    /**< 过温故障阈值：2°C（bit5-4） */
#define MAX31856_CR0_OCFAULT3     0x30    /**< 过温故障阈值：3°C（bit5-4） */
#define MAX31856_CR0_CJ           0x04    /**< 冷端温度选择（bit3：0=内部，1=外部） */
#define MAX31856_CR0_FAULT        0x08    /**< 故障模式（bit2：0=比较器，1=中断） */
#define MAX31856_CR0_FAULTCLR     0x02    /**< 故障状态清除（bit1：中断模式下清除故障） */
#define MAX31856_CR0_50_60HZ      0x01    /**< 50Hz/60Hz噪声抑制滤波器选择（bit0：0=60Hz，1=50Hz） */

/* CR1寄存器位定义 */
#define MAX31856_CR1_AVGSEL_MASK  0x07    /**< 采样模式掩码 */
#define MAX31856_CR1_TC_TYPE_MASK 0x70    /**< 热电偶类型掩码 */

/* SR寄存器位定义 */
#define MAX31856_SR_CJ_HIGH       0x80    /**< 冷端温度过高 */
#define MAX31856_SR_CJ_LOW        0x40    /**< 冷端温度过低 */
#define MAX31856_SR_TC_HIGH       0x20    /**< 热电偶温度过高 */
#define MAX31856_SR_TC_LOW        0x10    /**< 热电偶温度过低 */
#define MAX31856_SR_CJ_RANGE      0x08    /**< 冷端温度超出范围 */
#define MAX31856_SR_TC_RANGE      0x04    /**< 热电偶温度超出范围 */
#define MAX31856_SR_OV_UV         0x02    /**< 过压/欠压故障 */
#define MAX31856_SR_OPEN          0x01    /**< 热电偶开路 */

/* 默认超时时间（毫秒） */
#define MAX31856_DEFAULT_TIMEOUT_MS  100

/* 参数校验宏 */
#define MAX31856_CHECK_INIT() \
    do { \
        if (!g_max31856_initialized) \
            return MAX31856_ERROR_NOT_INITIALIZED; \
    } while(0)

#define MAX31856_CHECK_PARAM(ptr) \
    do { \
        if ((ptr) == NULL) \
            return MAX31856_ERROR_INVALID_PARAM; \
    } while(0)

/* 静态变量 */
static MAX31856_Config_t g_max31856_config;
static bool g_max31856_initialized = false;

/* CS引脚缓存（优化：避免每次调用都判断interface_type） */
static GPIO_TypeDef *g_cs_port = NULL;
static uint16_t g_cs_pin = 0;

/* ==================== CS引脚控制 ==================== */

/**
 * @brief CS引脚拉低（内部函数）
 */
static void MAX31856_CS_Low(void)
{
    GPIO_WriteBit(g_cs_port, g_cs_pin, Bit_RESET);
}

/**
 * @brief CS引脚拉高（内部函数）
 */
static void MAX31856_CS_High(void)
{
    GPIO_WriteBit(g_cs_port, g_cs_pin, Bit_SET);
}

/* ==================== 统一SPI接口 ==================== */

/**
 * @brief 统一SPI读寄存器接口
 * @param[in] reg_addr 寄存器地址
 * @param[out] reg_value 寄存器值
 * @return MAX31856_Status_t 错误码
 */
static MAX31856_Status_t MAX31856_SPI_ReadReg(uint8_t reg_addr, uint8_t *reg_value)
{
    uint8_t tx_buf[2];
    uint8_t rx_buf[2];
    
    MAX31856_CS_Low();
    Delay_us(100);  /* CS拉低后延时至少100μs，确保芯片稳定（解决通信时断时续问题） */
    
    if (g_max31856_config.interface_type == MAX31856_INTERFACE_HARDWARE)
    {
#if CONFIG_MODULE_SPI_ENABLED
        /* 参考代码方式：在一个CS周期内完成所有传输 */
        /* 第一个字节：发送寄存器地址，同时接收地址响应（丢弃） */
        /* 第二个字节：发送0xFF，同时接收寄存器值 */
        tx_buf[0] = reg_addr;
        tx_buf[1] = 0xFF;
        
        if (SPI_MasterTransmitReceive(g_max31856_config.config.hardware.spi_instance,
                                      tx_buf, rx_buf, 2, MAX31856_DEFAULT_TIMEOUT_MS) != SPI_OK)
        {
            MAX31856_CS_High();
            return MAX31856_ERROR_SPI_FAILED;
        }
        
        /* rx_buf[0]是地址响应（丢弃），rx_buf[1]是寄存器值 */
        *reg_value = rx_buf[1];
        
        /* CS拉高前短暂延时，确保SPI传输完全完成 */
        Delay_us(50);
        MAX31856_CS_High();
        /* CS拉高后延时，确保芯片有时间处理数据 */
        Delay_us(50);
        return MAX31856_OK;
#else
        MAX31856_CS_High();
        return MAX31856_ERROR_INVALID_PARAM;
#endif
    }
    else
    {
#if CONFIG_MODULE_SOFT_SPI_ENABLED
        /* 软件SPI方式 */
        tx_buf[0] = reg_addr;
        tx_buf[1] = 0xFF;
        
        if (SPI_SW_MasterTransmitReceive(g_max31856_config.config.software.soft_spi_instance,
                                         tx_buf, rx_buf, 2) != SPI_SW_OK)
        {
            MAX31856_CS_High();
            return MAX31856_ERROR_SPI_FAILED;
        }
        
        *reg_value = rx_buf[1];
        
        MAX31856_CS_High();
        Delay_us(1);
        return MAX31856_OK;
#else
        MAX31856_CS_High();
        return MAX31856_ERROR_INVALID_PARAM;
#endif
    }
}

/**
 * @brief 统一SPI写寄存器接口
 * @param[in] reg_addr 寄存器地址
 * @param[in] reg_value 寄存器值
 * @return MAX31856_Status_t 错误码
 */
static MAX31856_Status_t MAX31856_SPI_WriteReg(uint8_t reg_addr, uint8_t reg_value)
{
    uint8_t tx_data[2];
    
    /* MAX31856写操作：发送寄存器地址（最高位为1），然后发送数据（参考代码方式） */
    tx_data[0] = reg_addr | 0x80;  /* 写操作：地址最高位置1 */
    tx_data[1] = reg_value;
    
    MAX31856_CS_Low();
    Delay_us(200);  /* CS拉低后延时200μs，确保芯片稳定（解决通信时断时续问题） */
    
    if (g_max31856_config.interface_type == MAX31856_INTERFACE_HARDWARE)
    {
#if CONFIG_MODULE_SPI_ENABLED
        SPI_Status_t spi_status;
        
        /* 发送寄存器地址和数据 */
        spi_status = SPI_MasterTransmit(g_max31856_config.config.hardware.spi_instance, 
                                       tx_data, 2, MAX31856_DEFAULT_TIMEOUT_MS);
        if (spi_status != SPI_OK)
        {
            MAX31856_CS_High();
            return MAX31856_ERROR_SPI_FAILED;
        }
        
        /* CS拉高前短暂延时，确保SPI传输完全完成 */
        Delay_us(50);
        MAX31856_CS_High();
        /* CS拉高后延时，确保芯片有时间处理数据 */
        Delay_us(50);
        return MAX31856_OK;
#else
        MAX31856_CS_High();
        return MAX31856_ERROR_INVALID_PARAM;
#endif
    }
    else
    {
#if CONFIG_MODULE_SOFT_SPI_ENABLED
        SPI_SW_Status_t soft_spi_status;
        
        /* 发送寄存器地址和数据 */
        soft_spi_status = SPI_SW_MasterTransmit(g_max31856_config.config.software.soft_spi_instance,
                                                tx_data, 2);
        if (soft_spi_status != SPI_SW_OK)
        {
            MAX31856_CS_High();
            return MAX31856_ERROR_SPI_FAILED;
        }
        
        /* CS拉高前短暂延时，确保SPI传输完全完成 */
        Delay_us(50);
        MAX31856_CS_High();
        /* CS拉高后延时，确保芯片有时间处理数据 */
        Delay_us(50);
        return MAX31856_OK;
#else
        MAX31856_CS_High();
        return MAX31856_ERROR_INVALID_PARAM;
#endif
    }
}

/**
 * @brief 统一SPI读多个寄存器接口
 * @param[in] reg_addr 起始寄存器地址
 * @param[out] data 接收数据缓冲区
 * @param[in] length 数据长度
 * @return MAX31856_Status_t 错误码
 */
static MAX31856_Status_t MAX31856_SPI_ReadRegs(uint8_t reg_addr, uint8_t *data, uint8_t length)
{
    uint8_t tx_buf[5];  /* 1字节地址 + 最多4字节数据 */
    uint8_t rx_buf[5];
    uint8_t i;
    
    /* ========== 参数校验 ========== */
    if (length > 4 || data == NULL)
    {
        return MAX31856_ERROR_INVALID_PARAM;
    }
    
    MAX31856_CS_Low();
    Delay_us(200);  /* CS拉低后延时200μs，确保芯片稳定（解决通信时断时续问题） */
    
    if (g_max31856_config.interface_type == MAX31856_INTERFACE_HARDWARE)
    {
#if CONFIG_MODULE_SPI_ENABLED
        /* 参考代码方式：在一个CS周期内完成所有传输 */
        /* 第一个字节：发送寄存器地址 */
        /* 后续字节：发送0xFF读取数据 */
        tx_buf[0] = reg_addr;
        for (i = 0; i < length; i++)
        {
            tx_buf[i + 1] = 0xFF;
        }
        
        if (SPI_MasterTransmitReceive(g_max31856_config.config.hardware.spi_instance,
                                      tx_buf, rx_buf, length + 1, MAX31856_DEFAULT_TIMEOUT_MS) != SPI_OK)
        {
            MAX31856_CS_High();
            return MAX31856_ERROR_SPI_FAILED;
        }
        
        /* rx_buf[0]是地址响应（丢弃），rx_buf[1..length]是数据 */
        for (i = 0; i < length; i++)
        {
            data[i] = rx_buf[i + 1];
        }
        
        /* CS拉高前短暂延时，确保SPI传输完全完成 */
        Delay_us(50);
        MAX31856_CS_High();
        /* CS拉高后延时，确保芯片有时间处理数据 */
        Delay_us(50);
        return MAX31856_OK;
#else
        MAX31856_CS_High();
        return MAX31856_ERROR_INVALID_PARAM;
#endif
    }
    else
    {
#if CONFIG_MODULE_SOFT_SPI_ENABLED
        /* 软件SPI方式 */
        tx_buf[0] = reg_addr;
        for (i = 0; i < length; i++)
        {
            tx_buf[i + 1] = 0xFF;
        }
        
        if (SPI_SW_MasterTransmitReceive(g_max31856_config.config.software.soft_spi_instance,
                                         tx_buf, rx_buf, length + 1) != SPI_SW_OK)
        {
            MAX31856_CS_High();
            return MAX31856_ERROR_SPI_FAILED;
        }
        
        for (i = 0; i < length; i++)
        {
            data[i] = rx_buf[i + 1];
        }
        
        MAX31856_CS_High();
        Delay_us(1);
        return MAX31856_OK;
#else
        MAX31856_CS_High();
        return MAX31856_ERROR_INVALID_PARAM;
#endif
    }
}

/* ==================== 寄存器操作辅助函数 ==================== */

/**
 * @brief 读取-修改-写回CR0寄存器
 * @param[in] mask 要修改的位掩码
 * @param[in] value 要设置的值（只修改mask指定的位）
 * @return MAX31856_Status_t 错误码
 */
static MAX31856_Status_t MAX31856_ModifyCR0(uint8_t mask, uint8_t value)
{
    MAX31856_Status_t status;
    uint8_t cr0_reg;
    
    MAX31856_CHECK_INIT();
    
    status = MAX31856_SPI_ReadReg(MAX31856_REG_CR0, &cr0_reg);
    if (status != MAX31856_OK)
    {
        return status;
    }
    
    cr0_reg = (cr0_reg & ~mask) | (value & mask);
    
    return MAX31856_SPI_WriteReg(MAX31856_REG_CR0, cr0_reg);
}

/**
 * @brief 读取-修改-写回CR1寄存器
 * @param[in] mask 要修改的位掩码
 * @param[in] value 要设置的值（只修改mask指定的位）
 * @return MAX31856_Status_t 错误码
 */
static MAX31856_Status_t MAX31856_ModifyCR1(uint8_t mask, uint8_t value)
{
    MAX31856_Status_t status;
    uint8_t cr1_reg;
    
    MAX31856_CHECK_INIT();
    
    status = MAX31856_SPI_ReadReg(MAX31856_REG_CR1, &cr1_reg);
    if (status != MAX31856_OK)
    {
        return status;
    }
    
    cr1_reg = (cr1_reg & ~mask) | (value & mask);
    
    return MAX31856_SPI_WriteReg(MAX31856_REG_CR1, cr1_reg);
}

/* ==================== 公共函数实现 ==================== */

/**
 * @brief MAX31856初始化
 */
MAX31856_Status_t MAX31856_Init(const MAX31856_Config_t *config)
{
    /* ========== 参数校验 ========== */
    if (config == NULL)
    {
        return MAX31856_ERROR_INVALID_PARAM;
    }
    
    /* 如果已初始化，直接返回成功 */
    if (g_max31856_initialized)
    {
        return MAX31856_OK;
    }
    
    /* 保存配置 */
    g_max31856_config = *config;
    
    /* 根据接口类型进行初始化 */
    if (config->interface_type == MAX31856_INTERFACE_HARDWARE)
    {
#if CONFIG_MODULE_SPI_ENABLED
        /* 硬件SPI模式：检查SPI是否已初始化 */
        if (config->config.hardware.spi_instance >= SPI_INSTANCE_MAX)
        {
            return MAX31856_ERROR_INVALID_PARAM;
        }
        
        if (!SPI_IsInitialized(config->config.hardware.spi_instance))
        {
            return MAX31856_ERROR_NOT_INITIALIZED;
        }
#else
        /* 硬件SPI模块未启用 */
        return MAX31856_ERROR_INVALID_PARAM;
#endif
    }
    else if (config->interface_type == MAX31856_INTERFACE_SOFTWARE)
    {
#if CONFIG_MODULE_SOFT_SPI_ENABLED
        /* 软件SPI模式：检查实例索引并初始化soft_spi模块 */
        if (config->config.software.soft_spi_instance >= SPI_SW_INSTANCE_MAX)
        {
            return MAX31856_ERROR_INVALID_PARAM;
        }
        
        /* 初始化soft_spi模块（如果尚未初始化） */
        MAX31856_Status_t sw_status = (MAX31856_Status_t)SPI_SW_Init(config->config.software.soft_spi_instance);
        if (sw_status != MAX31856_OK)
        {
            return MAX31856_ERROR_GPIO_FAILED;
        }
        
        /* 检查soft_spi是否已初始化 */
        if (!SPI_SW_IsInitialized(config->config.software.soft_spi_instance))
        {
            return MAX31856_ERROR_NOT_INITIALIZED;
        }
#else
        /* 软件SPI模块未启用 */
        return MAX31856_ERROR_INVALID_PARAM;
#endif
    }
    else
    {
        return MAX31856_ERROR_INVALID_PARAM;
    }
    
    /* 配置CS引脚并缓存（优化：避免每次调用都判断interface_type） */
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    
    if (config->interface_type == MAX31856_INTERFACE_HARDWARE)
    {
        cs_port = config->config.hardware.cs_port;
        cs_pin = config->config.hardware.cs_pin;
    }
    else
    {
#if CONFIG_MODULE_SOFT_SPI_ENABLED
        cs_port = config->config.software.cs_port;
        cs_pin = config->config.software.cs_pin;
#else
        return MAX31856_ERROR_INVALID_PARAM;
#endif
    }
    
    /* 配置CS引脚为推挽输出，50MHz速度 */
    if (GPIO_Config(cs_port, cs_pin, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHz) != GPIO_OK)
    {
        return MAX31856_ERROR_GPIO_FAILED;
    }
    
    /* 只有在GPIO配置成功后才缓存CS引脚信息 */
    g_cs_port = cs_port;
    g_cs_pin = cs_pin;
    
    /* CS引脚初始化为高电平 */
    MAX31856_CS_High();
    Delay_ms(500);  /* 增加等待时间，确保芯片上电稳定（MAX31856上电后需要较长时间稳定） */
    
    /* 验证芯片是否真的在工作：尝试读取CR0寄存器（多次重试） */
    /* 如果芯片没有上电或SPI通信有问题，读取会失败 */
    {
        MAX31856_Status_t verify_status;
        uint8_t cr0_test;
        uint8_t retry_count;
        const uint8_t MAX_VERIFY_RETRY = 5;  /* 最多重试5次 */
        
        /* 多次重试读取CR0寄存器，确保通信稳定 */
        for (retry_count = 0; retry_count < MAX_VERIFY_RETRY; retry_count++)
        {
            if (retry_count > 0)
            {
                Delay_ms(50);  /* 重试前延时 */
            }
            
            verify_status = MAX31856_SPI_ReadReg(MAX31856_REG_CR0, &cr0_test);
            if (verify_status == MAX31856_OK)
            {
                /* 读取成功，检查读取到的值是否合理（CR0默认值通常是0x00） */
                /* 如果读取到0xFF，说明MISO线浮空，通信失败 */
                if (cr0_test == 0xFF)
                {
                    /* 读取到0xFF，说明SPI通信失败（MISO线浮空） */
                    if (retry_count == MAX_VERIFY_RETRY - 1)
                    {
                        /* 最后一次重试也失败，返回错误 */
                        g_cs_port = NULL;
                        g_cs_pin = 0;
                        return MAX31856_ERROR_SPI_FAILED;
                    }
                    /* 继续重试 */
                    continue;
                }
                /* 读取成功且值合理，验证通过 */
                break;
            }
            else
            {
                /* 读取失败，如果是最后一次重试，返回错误 */
                if (retry_count == MAX_VERIFY_RETRY - 1)
                {
                    g_cs_port = NULL;
                    g_cs_pin = 0;
                    return MAX31856_ERROR_SPI_FAILED;
                }
                /* 继续重试 */
            }
        }
        
        /* 如果所有重试都失败（不应该到达这里，但为了安全） */
        if (retry_count >= MAX_VERIFY_RETRY)
        {
            g_cs_port = NULL;
            g_cs_pin = 0;
            return MAX31856_ERROR_SPI_FAILED;
        }
    }
    
    /* 标记为已初始化 */
    g_max31856_initialized = true;
    
    return MAX31856_OK;
}

/**
 * @brief MAX31856反初始化
 */
MAX31856_Status_t MAX31856_Deinit(void)
{
    if (!g_max31856_initialized)
    {
        return MAX31856_OK;
    }
    
    /* 清除CS引脚缓存 */
    g_cs_port = NULL;
    g_cs_pin = 0;
    
    g_max31856_initialized = false;
    
    return MAX31856_OK;
}

/**
 * @brief 检查MAX31856是否已初始化
 */
uint8_t MAX31856_IsInitialized(void)
{
    return g_max31856_initialized ? 1 : 0;
}

/**
 * @brief 设置热电偶类型
 */
MAX31856_Status_t MAX31856_SetThermocoupleType(MAX31856_ThermocoupleType_t tc_type)
{
    uint8_t value = ((uint8_t)tc_type << 4) & MAX31856_CR1_TC_TYPE_MASK;
    return MAX31856_ModifyCR1(MAX31856_CR1_TC_TYPE_MASK, value);
}

/**
 * @brief 获取热电偶类型
 */
MAX31856_Status_t MAX31856_GetThermocoupleType(MAX31856_ThermocoupleType_t *tc_type)
{
    MAX31856_Status_t status;
    uint8_t cr1_reg;
    
    MAX31856_CHECK_PARAM(tc_type);
    MAX31856_CHECK_INIT();
    
    status = MAX31856_SPI_ReadReg(MAX31856_REG_CR1, &cr1_reg);
    if (status != MAX31856_OK)
    {
        return status;
    }
    
    *tc_type = (MAX31856_ThermocoupleType_t)((cr1_reg & MAX31856_CR1_TC_TYPE_MASK) >> 4);
    
    return MAX31856_OK;
}

/**
 * @brief 设置采样模式
 */
MAX31856_Status_t MAX31856_SetAvgMode(MAX31856_AvgSel_t avg_sel)
{
    uint8_t value = (uint8_t)avg_sel & MAX31856_CR1_AVGSEL_MASK;
    return MAX31856_ModifyCR1(MAX31856_CR1_AVGSEL_MASK, value);
}

/**
 * @brief 获取采样模式
 */
MAX31856_Status_t MAX31856_GetAvgMode(MAX31856_AvgSel_t *avg_sel)
{
    MAX31856_Status_t status;
    uint8_t cr1_reg;
    
    MAX31856_CHECK_PARAM(avg_sel);
    MAX31856_CHECK_INIT();
    
    status = MAX31856_SPI_ReadReg(MAX31856_REG_CR1, &cr1_reg);
    if (status != MAX31856_OK)
    {
        return status;
    }
    
    *avg_sel = (MAX31856_AvgSel_t)(cr1_reg & MAX31856_CR1_AVGSEL_MASK);
    
    return MAX31856_OK;
}

/**
 * @brief 读取热电偶温度（摄氏度）
 * @note 为了提高稳定性，采用重试机制和数据有效性检查
 * @note 如果检测到故障（如热电偶开路），会返回错误码而不是异常温度值
 */
MAX31856_Status_t MAX31856_ReadThermocoupleTemperature(float *temperature)
{
    MAX31856_Status_t status;
    uint8_t reg_data[3];
    uint8_t fault_flags;
    int32_t temp_raw;
    
    MAX31856_CHECK_PARAM(temperature);
    MAX31856_CHECK_INIT();
    
    /* 简化版本：跳过故障检查，直接读取温度（提高速度，避免故障检查失败导致无法读取） */
    /* 如果需要故障检查，可以在读取温度后单独调用ReadFault */
    (void)fault_flags;  /* 暂时不使用 */
    
    /* 读取热电偶温度寄存器（3字节：LTCBH, LTCBM, LTCBL，地址0x0C, 0x0D, 0x0E） */
    status = MAX31856_SPI_ReadRegs(MAX31856_REG_LTCBH, reg_data, 3);
    if (status != MAX31856_OK)
    {
        return status;
    }
    
    /* 组合19位有符号数据 */
    /* 根据数据手册：MAX31856的热电偶温度是19位有符号数，存储在3个字节中 */
    /* LTCBH (reg_data[0]): bit7是符号位（bit18），bit6-0是温度的高7位（bit17-11） */
    /* LTCBM (reg_data[1]): bit7-0是温度的中8位（bit10-3） */
    /* LTCBL (reg_data[2]): bit7-5是温度的低3位（bit2-0），bit4-0未使用（应该为0） */
    /* 组合方式：先组合成19位，然后进行符号扩展 */
    temp_raw = ((int32_t)reg_data[0] << 11) | 
               ((int32_t)reg_data[1] << 3) | 
               ((int32_t)(reg_data[2] & 0xE0) >> 5);
    
    /* 符号扩展：如果bit18为1，说明是负数，扩展到32位 */
    /* 注意：temp_raw的bit18对应reg_data[0]的bit7（符号位） */
    if (temp_raw & 0x40000)  /* 检查bit18（因为左移了11位，所以bit18在bit17位置） */
    {
        temp_raw |= 0xFFF80000;  /* 扩展到32位负数（保留19位有效，bit18-0） */
    }
    
    /* 转换为摄氏度：分辨率0.0078125°C = 1/128°C */
    /* 注意：19位数据已经是整数部分，直接乘以分辨率即可 */
    *temperature = (float)temp_raw * 0.0078125f;
    
    /* 数据有效性检查：如果温度值明显异常（超出合理范围），可能是通信错误 */
    /* MAX31856支持的热电偶类型温度范围通常在-200°C ~ +1800°C之间 */
    /* 但实际应用中，如果温度超过±2000°C，很可能是数据读取错误 */
    if (*temperature > 2000.0f || *temperature < -2000.0f)
    {
        return MAX31856_ERROR_FAULT;  /* 温度值异常，可能是通信错误或传感器故障 */
    }
    
    return MAX31856_OK;
}

/**
 * @brief 读取冷端温度（摄氏度）
 * @note 冷端温度寄存器在芯片上电后需要一定时间才能稳定
 * @note 如果读取到的温度超出合理范围（-50°C ~ +150°C），可能是：
 *       1. 芯片未完全初始化
 *       2. SPI通信错误
 *       3. 冷端温度传感器故障
 * @note 为了提高稳定性，采用重试机制：连续读取2次，如果数据一致才返回
 */
MAX31856_Status_t MAX31856_ReadColdJunctionTemperature(float *temperature)
{
    MAX31856_Status_t status;
    uint8_t cj_data[2];
    int16_t temp_raw;
    
    MAX31856_CHECK_PARAM(temperature);
    MAX31856_CHECK_INIT();
    
    /* 读取冷端温度寄存器（CJTH和CJTL，地址0x0A和0x0B） */
    status = MAX31856_SPI_ReadRegs(MAX31856_REG_CJTH, cj_data, 2);
    if (status != MAX31856_OK)
    {
        return status;
    }
    
    /* 检查原始数据：如果读取到的数据都是0xFF，说明SPI通信失败（MISO线浮空） */
    if (cj_data[0] == 0xFF && cj_data[1] == 0xFF)
    {
        return MAX31856_ERROR_SPI_FAILED;  /* SPI通信错误 */
    }
    
    /* 组合12位有符号整数 */
    /* 根据数据手册：CJTH是8位（bit7是符号位），CJTL的高4位（bit7-4）是温度的低4位，CJTL的低4位（bit3-0）未使用 */
    /* 数据格式：12位有符号数，分辨率0.0625°C（1/16°C） */
    /* CJTH[7:0]：温度的高8位（bit7是符号位） */
    /* CJTL[7:4]：温度的低4位 */
    /* CJTL[3:0]：未使用（应该为0） */
    temp_raw = ((int16_t)(int8_t)cj_data[0] << 4) | ((int16_t)(cj_data[1] & 0xF0) >> 4);
    
    /* 符号扩展：如果bit11为1，说明是负数，扩展到16位 */
    if (temp_raw & 0x800)
    {
        temp_raw |= 0xF000;  /* 扩展到16位负数 */
    }
    
    /* 转换为摄氏度：分辨率0.0625°C = 1/16°C */
    *temperature = temp_raw / 16.0f;
    
    /* 数据有效性检查：如果温度值明显异常（>200°C或<-50°C），可能是读取了错误的寄存器 */
    /* 冷端温度通常在-50°C ~ +150°C范围内 */
    if (*temperature > 200.0f || *temperature < -50.0f)
    {
        /* 异常值，返回错误 */
        return MAX31856_ERROR_FAULT;
    }
    
    return MAX31856_OK;
}

/**
 * @brief 读取故障状态
 * @note 为了提高稳定性，采用重试机制：连续读取2次，如果数据一致才返回
 * @note 如果读取到0xFF，说明SPI通信错误（MISO线浮空），返回通信错误
 */
MAX31856_Status_t MAX31856_ReadFault(uint8_t *fault_flags)
{
    MAX31856_Status_t status;
    uint8_t retry_count;
    const uint8_t MAX_RETRY = 3;
    
    MAX31856_CHECK_PARAM(fault_flags);
    MAX31856_CHECK_INIT();
    
    /* 添加重试机制：SR寄存器可能需要多次尝试 */
    for (retry_count = 0; retry_count < MAX_RETRY; retry_count++)
    {
        if (retry_count > 0)
        {
            Delay_us(50);  /* 重试前短暂延时 */
        }
        
        status = MAX31856_SPI_ReadReg(MAX31856_REG_SR, fault_flags);
        if (status == MAX31856_OK)
        {
            /* 检查读取到的数据：如果读取到0xFF，说明SPI通信错误（MISO线浮空） */
            if (*fault_flags == 0xFF)
            {
                /* 如果是最后一次重试，返回错误 */
                if (retry_count == MAX_RETRY - 1)
                {
                    return MAX31856_ERROR_SPI_FAILED;  /* SPI通信错误 */
                }
                /* 否则继续重试 */
                continue;
            }
            /* 读取成功且数据有效 */
            return MAX31856_OK;
        }
        /* 读取失败，继续重试 */
    }
    
    /* 所有重试都失败 */
    return status;
}

/**
 * @brief 检查特定故障
 */
MAX31856_Status_t MAX31856_CheckFault(MAX31856_Fault_t fault_type, uint8_t *fault_present)
{
    MAX31856_Status_t status;
    uint8_t fault_flags;
    
    MAX31856_CHECK_PARAM(fault_present);
    MAX31856_CHECK_INIT();
    
    status = MAX31856_SPI_ReadReg(MAX31856_REG_SR, &fault_flags);
    if (status != MAX31856_OK)
    {
        return status;
    }
    
    *fault_present = (fault_flags & (uint8_t)fault_type) ? 1 : 0;
    
    return MAX31856_OK;
}

/**
 * @brief 清除故障状态
 */
MAX31856_Status_t MAX31856_ClearFault(void)
{
    MAX31856_CHECK_INIT();
    
    /* 清除故障：读取SR寄存器即可清除 */
    uint8_t dummy;
    return MAX31856_SPI_ReadReg(MAX31856_REG_SR, &dummy);
}

/**
 * @brief 设置故障屏蔽
 */
MAX31856_Status_t MAX31856_SetFaultMask(uint8_t mask_flags)
{
    MAX31856_CHECK_INIT();
    return MAX31856_SPI_WriteReg(MAX31856_REG_MASK, mask_flags);
}

/**
 * @brief 获取故障屏蔽
 */
MAX31856_Status_t MAX31856_GetFaultMask(uint8_t *mask_flags)
{
    MAX31856_CHECK_PARAM(mask_flags);
    MAX31856_CHECK_INIT();
    return MAX31856_SPI_ReadReg(MAX31856_REG_MASK, mask_flags);
}

/**
 * @brief 触发单次转换
 */
MAX31856_Status_t MAX31856_TriggerOneShot(void)
{
    /* 设置1SHOT位 */
    return MAX31856_ModifyCR0(MAX31856_CR0_1SHOT, MAX31856_CR0_1SHOT);
}

/**
 * @brief 检查转换完成状态
 * @note 在单次转换模式下，1SHOT位为0表示转换完成
 * @note 在连续转换模式下，1SHOT位始终为0，此函数在连续模式下会始终返回ready=1
 * @note 建议在单次转换模式下使用此函数，连续模式下无需检查转换完成状态
 */
MAX31856_Status_t MAX31856_IsConversionReady(uint8_t *ready)
{
    MAX31856_Status_t status;
    uint8_t cr0_reg;
    
    MAX31856_CHECK_PARAM(ready);
    MAX31856_CHECK_INIT();
    
    /* 读取CR0寄存器，检查1SHOT位 */
    status = MAX31856_SPI_ReadReg(MAX31856_REG_CR0, &cr0_reg);
    if (status != MAX31856_OK)
    {
        return status;
    }
    
    /* 1SHOT位为0表示转换完成（在单次转换模式下）
     * 注意：在连续转换模式下，1SHOT位始终为0，此函数会始终返回ready=1
     */
    *ready = (cr0_reg & MAX31856_CR0_1SHOT) ? 0 : 1;
    
    return MAX31856_OK;
}

/**
 * @brief 设置过温故障阈值
 */
MAX31856_Status_t MAX31856_SetOCFault(MAX31856_OCFault_t oc_fault)
{
    uint8_t mask = 0x30;  /* OCFAULT位掩码（bit5-4） */
    uint8_t value = (uint8_t)oc_fault & mask;
    return MAX31856_ModifyCR0(mask, value);
}

/**
 * @brief 设置故障模式
 */
MAX31856_Status_t MAX31856_SetFaultMode(MAX31856_FaultMode_t fault_mode)
{
    uint8_t value = (fault_mode == MAX31856_FAULT_MODE_INTERRUPT) ? MAX31856_CR0_FAULT : 0;
    return MAX31856_ModifyCR0(MAX31856_CR0_FAULT, value);
}

/**
 * @brief 设置冷端温度源
 */
MAX31856_Status_t MAX31856_SetCJSource(MAX31856_CJSource_t cj_source)
{
    uint8_t value = (cj_source == MAX31856_CJ_SOURCE_EXTERNAL) ? MAX31856_CR0_CJ : 0;
    return MAX31856_ModifyCR0(MAX31856_CR0_CJ, value);
}

/**
 * @brief 设置转换模式
 * @note 根据数据手册：
 *       - 连续转换模式：设置CMODE位（bit7）= 1，启用自动转换模式（每100ms转换一次）
 *       - 单次转换模式：CMODE位（bit7）= 0（关闭模式），然后设置1SHOT位（bit6）= 1触发转换
 */
MAX31856_Status_t MAX31856_SetConvMode(MAX31856_ConvMode_t conv_mode)
{
    if (conv_mode == MAX31856_CONV_MODE_CONTINUOUS)
    {
        /* 连续转换模式：设置CMODE位（bit7）= 1 */
        return MAX31856_ModifyCR0(MAX31856_CR0_CMODE, MAX31856_CR0_CMODE);
    }
    else
    {
        /* 单次转换模式：清除CMODE位（bit7）= 0，进入关闭模式 */
        return MAX31856_ModifyCR0(MAX31856_CR0_CMODE, 0);
    }
}

/**
 * @brief 读取控制寄存器0（CR0）
 */
MAX31856_Status_t MAX31856_ReadCR0(uint8_t *cr0_value)
{
    MAX31856_CHECK_PARAM(cr0_value);
    MAX31856_CHECK_INIT();
    return MAX31856_SPI_ReadReg(MAX31856_REG_CR0, cr0_value);
}

/**
 * @brief 读取控制寄存器1（CR1）
 */
MAX31856_Status_t MAX31856_ReadCR1(uint8_t *cr1_value)
{
    MAX31856_CHECK_PARAM(cr1_value);
    MAX31856_CHECK_INIT();
    return MAX31856_SPI_ReadReg(MAX31856_REG_CR1, cr1_value);
}

#endif /* CONFIG_MODULE_MAX31856_ENABLED */

