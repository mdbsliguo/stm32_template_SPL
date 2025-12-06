/**
 * @file ds3231.c
 * @brief DS3231实时时钟模块驱动实现
 * @version 3.0.0
 * @date 2024-01-01
 * @details 支持硬件I2C和软件I2C两种接口的DS3231 RTC驱动，完整功能实现
 * @note 解耦版本：软件I2C部分已解耦，直接使用soft_i2c模块，不再重复实现I2C时序
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_DS3231_ENABLED

#include "ds3231.h"
#include "gpio.h"
#include "delay.h"
#include <stdbool.h>
#include <stddef.h>

/* 条件编译：硬件I2C模块 */
#if CONFIG_MODULE_I2C_ENABLED
#include "i2c_hw.h"
#endif

/* 条件编译：软件I2C模块 */
#if CONFIG_MODULE_SOFT_I2C_ENABLED
#include "i2c_sw.h"
#endif

/* DS3231 I2C地址（7位地址） */
#define DS3231_I2C_ADDR        0x68

/* DS3231寄存器地址 */
#define DS3231_REG_SECOND      0x00    /**< 秒寄存器 */
#define DS3231_REG_MINUTE      0x01    /**< 分寄存器 */
#define DS3231_REG_HOUR        0x02    /**< 时寄存器 */
#define DS3231_REG_WEEKDAY     0x03    /**< 星期寄存器 */
#define DS3231_REG_DAY         0x04    /**< 日寄存器 */
#define DS3231_REG_MONTH       0x05    /**< 月寄存器 */
#define DS3231_REG_YEAR        0x06    /**< 年寄存器 */
#define DS3231_REG_ALARM1_SEC   0x07    /**< 闹钟1秒寄存器 */
#define DS3231_REG_ALARM1_MIN   0x08    /**< 闹钟1分寄存器 */
#define DS3231_REG_ALARM1_HOUR  0x09    /**< 闹钟1时寄存器 */
#define DS3231_REG_ALARM1_DAY   0x0A    /**< 闹钟1日寄存器 */
#define DS3231_REG_ALARM2_MIN   0x0B    /**< 闹钟2分寄存器 */
#define DS3231_REG_ALARM2_HOUR  0x0C    /**< 闹钟2时寄存器 */
#define DS3231_REG_ALARM2_DAY   0x0D    /**< 闹钟2日寄存器 */
#define DS3231_REG_CONTROL      0x0E    /**< 控制寄存器 */
#define DS3231_REG_STATUS       0x0F    /**< 状态寄存器 */
#define DS3231_REG_AGING_OFFSET 0x10    /**< 老化偏移寄存器 */
#define DS3231_REG_TEMP_MSB     0x11    /**< 温度高字节寄存器 */
#define DS3231_REG_TEMP_LSB     0x12    /**< 温度低字节寄存器 */

/* 状态寄存器位定义 */
#define DS3231_STATUS_OSF       0x80    /**< 振荡器停止标志 */
#define DS3231_STATUS_EN32KHZ   0x08    /**< 32kHz输出使能 */
#define DS3231_STATUS_BSY       0x04    /**< 忙标志 */
#define DS3231_STATUS_A2F       0x02    /**< 闹钟2标志 */
#define DS3231_STATUS_A1F       0x01    /**< 闹钟1标志 */

/* 控制寄存器位定义 */
#define DS3231_CTRL_EOSC        0x80    /**< 振荡器使能（0=使能，1=禁用） */
#define DS3231_CTRL_BBSQW       0x40    /**< 电池备份方波输出 */
#define DS3231_CTRL_CONV        0x20    /**< 温度转换 */
#define DS3231_CTRL_RS2         0x10    /**< 方波频率选择位2 */
#define DS3231_CTRL_RS1         0x08    /**< 方波频率选择位1 */
#define DS3231_CTRL_INTCN       0x04    /**< 中断控制 */
#define DS3231_CTRL_A2IE        0x02    /**< 闹钟2中断使能 */
#define DS3231_CTRL_A1IE        0x01    /**< 闹钟1中断使能 */

/* 默认超时时间（毫秒） */
#define DS3231_DEFAULT_TIMEOUT_MS  100

/* 参数校验宏 */
#define DS3231_CHECK_INIT() \
    do { \
        if (!g_ds3231_initialized) \
            return DS3231_ERROR_NOT_INITIALIZED; \
    } while(0)

#define DS3231_CHECK_PARAM(ptr) \
    do { \
        if ((ptr) == NULL) \
            return DS3231_ERROR_INVALID_PARAM; \
    } while(0)

/* 静态变量 */
static DS3231_Config_t g_ds3231_config;
static bool g_ds3231_initialized = false;

/* ==================== 软件I2C实现（已解耦，使用soft_i2c模块） ==================== */
/* 
 * 注意：软件I2C实现已移除，改为使用soft_i2c模块的API
 * 所有DS3231_SoftI2C_*函数已删除，统一接口直接调用SoftI2C_MasterReadReg等API
 * 解耦版本：DS3231模块专注于RTC功能，I2C通信由soft_i2c模块负责
 */

/**
 * @brief BCD转二进制
 * @param[in] bcd BCD码
 * @return uint8_t 二进制值
 */
static uint8_t DS3231_BCD2Bin(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

/**
 * @brief 二进制转BCD
 * @param[in] bin 二进制值
 * @return uint8_t BCD码
 */
static uint8_t DS3231_Bin2BCD(uint8_t bin)
{
    return ((bin / 10) << 4) | (bin % 10);
}

/* ==================== 统一I2C接口 ==================== */

/**
 * @brief 统一I2C读寄存器接口
 * @param[in] reg_addr 寄存器地址
 * @param[out] reg_value 寄存器值
 * @return DS3231_Status_t 错误码
 */
static DS3231_Status_t DS3231_I2C_ReadReg(uint8_t reg_addr, uint8_t *reg_value)
{
    if (g_ds3231_config.interface_type == DS3231_INTERFACE_HARDWARE)
    {
#if CONFIG_MODULE_I2C_ENABLED
        I2C_Status_t i2c_status;
        i2c_status = I2C_MasterReadReg(g_ds3231_config.config.hardware.i2c_instance, 
                                       DS3231_I2C_ADDR, reg_addr, reg_value, 
                                       DS3231_DEFAULT_TIMEOUT_MS);
        return (i2c_status == I2C_OK) ? DS3231_OK : DS3231_ERROR_I2C_FAILED;
#else
        return DS3231_ERROR_INVALID_PARAM;
#endif
    }
    else
    {
#if CONFIG_MODULE_SOFT_I2C_ENABLED
        SoftI2C_Status_t soft_i2c_status;
        soft_i2c_status = SoftI2C_MasterReadReg(g_ds3231_config.config.software.soft_i2c_instance,
                                                 DS3231_I2C_ADDR, reg_addr, reg_value,
                                                 DS3231_DEFAULT_TIMEOUT_MS);
        return (soft_i2c_status == SOFT_I2C_OK) ? DS3231_OK : DS3231_ERROR_I2C_FAILED;
#else
        return DS3231_ERROR_INVALID_PARAM;
#endif
    }
}

/**
 * @brief 统一I2C写寄存器接口
 * @param[in] reg_addr 寄存器地址
 * @param[in] reg_value 寄存器值
 * @return DS3231_Status_t 错误码
 */
static DS3231_Status_t DS3231_I2C_WriteReg(uint8_t reg_addr, uint8_t reg_value)
{
    if (g_ds3231_config.interface_type == DS3231_INTERFACE_HARDWARE)
    {
#if CONFIG_MODULE_I2C_ENABLED
        I2C_Status_t i2c_status;
        i2c_status = I2C_MasterWriteReg(g_ds3231_config.config.hardware.i2c_instance, 
                                         DS3231_I2C_ADDR, reg_addr, reg_value, 
                                         DS3231_DEFAULT_TIMEOUT_MS);
        return (i2c_status == I2C_OK) ? DS3231_OK : DS3231_ERROR_I2C_FAILED;
#else
        return DS3231_ERROR_INVALID_PARAM;
#endif
    }
    else
    {
#if CONFIG_MODULE_SOFT_I2C_ENABLED
        SoftI2C_Status_t soft_i2c_status;
        soft_i2c_status = SoftI2C_MasterWriteReg(g_ds3231_config.config.software.soft_i2c_instance,
                                                  DS3231_I2C_ADDR, reg_addr, reg_value,
                                                  DS3231_DEFAULT_TIMEOUT_MS);
        return (soft_i2c_status == SOFT_I2C_OK) ? DS3231_OK : DS3231_ERROR_I2C_FAILED;
#else
        return DS3231_ERROR_INVALID_PARAM;
#endif
    }
}

/**
 * @brief 统一I2C读多个寄存器接口
 * @param[in] reg_addr 起始寄存器地址
 * @param[out] data 接收数据缓冲区
 * @param[in] length 数据长度
 * @return DS3231_Status_t 错误码
 */
static DS3231_Status_t DS3231_I2C_ReadRegs(uint8_t reg_addr, uint8_t *data, uint8_t length)
{
    if (g_ds3231_config.interface_type == DS3231_INTERFACE_HARDWARE)
    {
#if CONFIG_MODULE_I2C_ENABLED
        I2C_Status_t i2c_status;
        i2c_status = I2C_MasterReadRegs(g_ds3231_config.config.hardware.i2c_instance, 
                                        DS3231_I2C_ADDR, reg_addr, data, length, 
                                        DS3231_DEFAULT_TIMEOUT_MS);
        return (i2c_status == I2C_OK) ? DS3231_OK : DS3231_ERROR_I2C_FAILED;
#else
        return DS3231_ERROR_INVALID_PARAM;
#endif
    }
    else
    {
#if CONFIG_MODULE_SOFT_I2C_ENABLED
        SoftI2C_Status_t soft_i2c_status;
        soft_i2c_status = SoftI2C_MasterReadRegs(g_ds3231_config.config.software.soft_i2c_instance,
                                                  DS3231_I2C_ADDR, reg_addr, data, length,
                                                  DS3231_DEFAULT_TIMEOUT_MS);
        return (soft_i2c_status == SOFT_I2C_OK) ? DS3231_OK : DS3231_ERROR_I2C_FAILED;
#else
        return DS3231_ERROR_INVALID_PARAM;
#endif
    }
}

/**
 * @brief 统一I2C写多个寄存器接口
 * @param[in] reg_addr 起始寄存器地址
 * @param[in] data 发送数据缓冲区
 * @param[in] length 数据长度
 * @return DS3231_Status_t 错误码
 */
static DS3231_Status_t DS3231_I2C_WriteRegs(uint8_t reg_addr, const uint8_t *data, uint8_t length)
{
    if (g_ds3231_config.interface_type == DS3231_INTERFACE_HARDWARE)
    {
#if CONFIG_MODULE_I2C_ENABLED
        I2C_Status_t i2c_status;
        i2c_status = I2C_MasterWriteRegs(g_ds3231_config.config.hardware.i2c_instance, 
                                         DS3231_I2C_ADDR, reg_addr, data, length, 
                                         DS3231_DEFAULT_TIMEOUT_MS);
        return (i2c_status == I2C_OK) ? DS3231_OK : DS3231_ERROR_I2C_FAILED;
#else
        return DS3231_ERROR_INVALID_PARAM;
#endif
    }
    else
    {
#if CONFIG_MODULE_SOFT_I2C_ENABLED
        SoftI2C_Status_t soft_i2c_status;
        soft_i2c_status = SoftI2C_MasterWriteRegs(g_ds3231_config.config.software.soft_i2c_instance,
                                                   DS3231_I2C_ADDR, reg_addr, data, length,
                                                   DS3231_DEFAULT_TIMEOUT_MS);
        return (soft_i2c_status == SOFT_I2C_OK) ? DS3231_OK : DS3231_ERROR_I2C_FAILED;
#else
        return DS3231_ERROR_INVALID_PARAM;
#endif
    }
}

/* ==================== 寄存器操作辅助函数 ==================== */

/**
 * @brief 读取-修改-写回控制寄存器
 * @param[in] mask 要修改的位掩码
 * @param[in] value 要设置的值（只修改mask指定的位）
 * @return DS3231_Status_t 错误码
 */
static DS3231_Status_t DS3231_ModifyControlRegister(uint8_t mask, uint8_t value)
{
    DS3231_Status_t status;
    uint8_t control_reg;
    
    DS3231_CHECK_INIT();
    
    status = DS3231_I2C_ReadReg(DS3231_REG_CONTROL, &control_reg);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    control_reg = (control_reg & ~mask) | (value & mask);
    
    return DS3231_I2C_WriteReg(DS3231_REG_CONTROL, control_reg);
}

/**
 * @brief 读取-修改-写回状态寄存器
 * @param[in] mask 要修改的位掩码
 * @param[in] value 要设置的值（只修改mask指定的位）
 * @return DS3231_Status_t 错误码
 */
static DS3231_Status_t DS3231_ModifyStatusRegister(uint8_t mask, uint8_t value)
{
    DS3231_Status_t status;
    uint8_t status_reg;
    
    DS3231_CHECK_INIT();
    
    status = DS3231_I2C_ReadReg(DS3231_REG_STATUS, &status_reg);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    status_reg = (status_reg & ~mask) | (value & mask);
    
    return DS3231_I2C_WriteReg(DS3231_REG_STATUS, status_reg);
}

/**
 * @brief 检查闹钟标志（内部函数）
 * @param[in] alarm_num 闹钟编号（1或2）
 * @param[out] flag 标志指针（1=触发，0=未触发）
 * @return DS3231_Status_t 错误码
 */
static DS3231_Status_t DS3231_CheckAlarmFlagInternal(uint8_t alarm_num, uint8_t *flag)
{
    DS3231_Status_t status;
    uint8_t status_reg;
    uint8_t flag_mask;
    
    DS3231_CHECK_PARAM(flag);
    DS3231_CHECK_INIT();
    
    if (alarm_num == 1)
    {
        flag_mask = DS3231_STATUS_A1F;
    }
    else if (alarm_num == 2)
    {
        flag_mask = DS3231_STATUS_A2F;
    }
    else
    {
        return DS3231_ERROR_INVALID_PARAM;
    }
    
    status = DS3231_I2C_ReadReg(DS3231_REG_STATUS, &status_reg);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    *flag = (status_reg & flag_mask) ? 1 : 0;
    return DS3231_OK;
}

/**
 * @brief 清除闹钟标志（内部函数）
 * @param[in] alarm_num 闹钟编号（1或2）
 * @return DS3231_Status_t 错误码
 */
static DS3231_Status_t DS3231_ClearAlarmFlagInternal(uint8_t alarm_num)
{
    uint8_t flag_mask;
    
    if (alarm_num == 1)
    {
        flag_mask = DS3231_STATUS_A1F;
    }
    else if (alarm_num == 2)
    {
        flag_mask = DS3231_STATUS_A2F;
    }
    else
    {
        return DS3231_ERROR_INVALID_PARAM;
    }
    
    return DS3231_ModifyStatusRegister(flag_mask, 0);
}

/**
 * @brief 设置闹钟中断使能（内部函数）
 * @param[in] alarm_num 闹钟编号（1或2）
 * @param[in] enable 使能标志（1=使能，0=禁用）
 * @return DS3231_Status_t 错误码
 */
static DS3231_Status_t DS3231_SetAlarmInterruptInternal(uint8_t alarm_num, uint8_t enable)
{
    uint8_t mask;
    uint8_t value;
    
    if (alarm_num == 1)
    {
        mask = DS3231_CTRL_A1IE;
    }
    else if (alarm_num == 2)
    {
        mask = DS3231_CTRL_A2IE;
    }
    else
    {
        return DS3231_ERROR_INVALID_PARAM;
    }
    
    value = enable ? mask : 0;
    return DS3231_ModifyControlRegister(mask, value);
}

/**
 * @brief DS3231初始化
 */
DS3231_Status_t DS3231_Init(const DS3231_Config_t *config)
{
    uint8_t status_reg;
    DS3231_Status_t status;
    
    /* ========== 参数校验 ========== */
    if (config == NULL)
    {
        return DS3231_ERROR_INVALID_PARAM;
    }
    
    /* 如果已初始化，直接返回成功 */
    if (g_ds3231_initialized)
    {
        return DS3231_OK;
    }
    
    /* 保存配置 */
    g_ds3231_config = *config;
    
    /* 根据接口类型进行初始化 */
    if (config->interface_type == DS3231_INTERFACE_HARDWARE)
    {
#if CONFIG_MODULE_I2C_ENABLED
        /* 硬件I2C模式：检查I2C是否已初始化 */
        if (config->config.hardware.i2c_instance >= I2C_INSTANCE_MAX)
        {
            return DS3231_ERROR_INVALID_PARAM;
        }
        
        if (!I2C_IsInitialized(config->config.hardware.i2c_instance))
        {
            return DS3231_ERROR_NOT_INITIALIZED;
        }
#else
        /* 硬件I2C模块未启用 */
        return DS3231_ERROR_INVALID_PARAM;
#endif
    }
    else if (config->interface_type == DS3231_INTERFACE_SOFTWARE)
    {
#if CONFIG_MODULE_SOFT_I2C_ENABLED
        SoftI2C_Status_t soft_i2c_status;
        
        /* 软件I2C模式：检查实例索引并初始化soft_i2c模块 */
        if (config->config.software.soft_i2c_instance >= SOFT_I2C_INSTANCE_MAX)
        {
            return DS3231_ERROR_INVALID_PARAM;
        }
        
        /* 初始化soft_i2c模块（如果尚未初始化） */
        soft_i2c_status = I2C_SW_Init(config->config.software.soft_i2c_instance);
        if (soft_i2c_status != SOFT_I2C_OK)
        {
            return DS3231_ERROR_GPIO_FAILED;
        }
        
        /* 检查soft_i2c是否已初始化 */
        if (!SoftI2C_IsInitialized(config->config.software.soft_i2c_instance))
        {
            return DS3231_ERROR_NOT_INITIALIZED;
        }
#else
        /* 软件I2C模块未启用 */
        return DS3231_ERROR_INVALID_PARAM;
#endif
    }
    else
    {
        return DS3231_ERROR_INVALID_PARAM;
    }
    
    /* 读取状态寄存器，检查设备是否响应 */
    status = DS3231_I2C_ReadReg(DS3231_REG_STATUS, &status_reg);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    /* 清除OSF标志（如果存在） */
    if (status_reg & DS3231_STATUS_OSF)
    {
        DS3231_ClearOSF();
    }
    
    /* 确保振荡器运行（清除EOSC位） */
    DS3231_Start();
    
    /* 标记为已初始化 */
    g_ds3231_initialized = true;
    
    return DS3231_OK;
}

/**
 * @brief DS3231反初始化
 */
DS3231_Status_t DS3231_Deinit(void)
{
    if (!g_ds3231_initialized)
    {
        return DS3231_OK;
    }
    
    g_ds3231_initialized = false;
    
    return DS3231_OK;
}

/**
 * @brief 检查DS3231是否已初始化
 */
uint8_t DS3231_IsInitialized(void)
{
    return g_ds3231_initialized ? 1 : 0;
}

/**
 * @brief 读取DS3231当前时间
 */
DS3231_Status_t DS3231_ReadTime(DS3231_Time_t *time)
{
    DS3231_Status_t status;
    uint8_t reg_data[7];
    uint8_t century_bit;
    
    /* ========== 参数校验 ========== */
    DS3231_CHECK_PARAM(time);
    DS3231_CHECK_INIT();
    
    /* 读取时间寄存器（0x00-0x06） */
    status = DS3231_I2C_ReadRegs(DS3231_REG_SECOND, reg_data, 7);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    /* 转换BCD到二进制 */
    time->second = DS3231_BCD2Bin(reg_data[0] & 0x7F);  /* 秒（忽略CH位） */
    time->minute = DS3231_BCD2Bin(reg_data[1] & 0x7F);  /* 分 */
    
    /* 时寄存器：bit6=12/24小时制（0=24小时制），bit5-4=小时的十位，bit3-0=小时的个位 */
    /* 确保是24小时制（bit6=0），然后读取bit5-0 */
    if (reg_data[2] & 0x40)
    {
        /* 12小时制模式，需要转换（当前实现只支持24小时制） */
        /* 这里假设bit5=1表示PM，需要+12 */
        uint8_t hour_bcd = reg_data[2] & 0x1F;
        uint8_t hour = DS3231_BCD2Bin(hour_bcd);
        if (reg_data[2] & 0x20)  /* PM标志 */
        {
            if (hour != 12) hour += 12;
        }
        else  /* AM */
        {
            if (hour == 12) hour = 0;
        }
        time->hour = hour;
    }
    else
    {
        /* 24小时制模式 */
        time->hour = DS3231_BCD2Bin(reg_data[2] & 0x3F);
    }
    
    time->weekday = DS3231_BCD2Bin(reg_data[3] & 0x07); /* 星期 */
    time->day = DS3231_BCD2Bin(reg_data[4] & 0x3F);     /* 日 */
    
    /* 月寄存器：bit7=世纪位（Century bit），bit5-4=月的十位，bit3-0=月的个位 */
    century_bit = (reg_data[5] & 0x80) >> 7;
    time->month = DS3231_BCD2Bin(reg_data[5] & 0x1F);
    
    /* 年寄存器：bit7-0=年的个位和十位（BCD） */
    /* 世纪位=0表示1900-1999，世纪位=1表示2000-2099 */
    time->year = (century_bit ? 2000 : 1900) + DS3231_BCD2Bin(reg_data[6]);
    
    return DS3231_OK;
}

/**
 * @brief 设置DS3231时间
 */
DS3231_Status_t DS3231_SetTime(const DS3231_Time_t *time)
{
    DS3231_Status_t status;
    uint8_t reg_data[7];
    uint8_t century_bit;
    
    /* ========== 参数校验 ========== */
    DS3231_CHECK_PARAM(time);
    DS3231_CHECK_INIT();
    
    /* 参数范围校验 */
    if (time->second > 59 || time->minute > 59 || time->hour > 23 ||
        time->weekday < 1 || time->weekday > 7 ||
        time->day < 1 || time->day > 31 ||
        time->month < 1 || time->month > 12 ||
        time->year < 1900 || time->year > 2099)
    {
        return DS3231_ERROR_INVALID_PARAM;
    }
    
    /* 日期有效性检查（考虑闰年和月份天数） */
    {
        uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        uint8_t max_days;
        
        /* 判断是否为闰年 */
        if ((time->year % 4 == 0 && time->year % 100 != 0) || (time->year % 400 == 0))
        {
            days_in_month[1] = 29;  /* 2月29天 */
        }
        else
        {
            days_in_month[1] = 28;  /* 2月28天 */
        }
        
        max_days = days_in_month[time->month - 1];
        if (time->day > max_days)
        {
            return DS3231_ERROR_INVALID_PARAM;
        }
    }
    
    /* 转换二进制到BCD */
    reg_data[0] = DS3231_Bin2BCD(time->second);  /* 秒（CH位保持0，表示运行） */
    reg_data[1] = DS3231_Bin2BCD(time->minute);    /* 分 */
    
    /* 时寄存器：bit6=0（24小时制），bit5-4=小时的十位，bit3-0=小时的个位 */
    reg_data[2] = DS3231_Bin2BCD(time->hour) & 0x3F;  /* 24小时制，清除bit6 */
    
    reg_data[3] = DS3231_Bin2BCD(time->weekday);  /* 星期 */
    reg_data[4] = DS3231_Bin2BCD(time->day);      /* 日 */
    
    /* 月寄存器：bit7=世纪位，bit5-4=月的十位，bit3-0=月的个位 */
    /* 世纪位：0=1900-1999，1=2000-2099 */
    century_bit = (time->year >= 2000) ? 0x80 : 0x00;
    reg_data[5] = DS3231_Bin2BCD(time->month) | century_bit;
    
    /* 年寄存器：bit7-0=年的个位和十位（BCD，0-99） */
    if (time->year >= 2000)
    {
        reg_data[6] = DS3231_Bin2BCD(time->year - 2000);
    }
    else
    {
        reg_data[6] = DS3231_Bin2BCD(time->year - 1900);
    }
    
    /* 写入时间寄存器（0x00-0x06） */
    status = DS3231_I2C_WriteRegs(DS3231_REG_SECOND, reg_data, 7);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    /* 清除OSF标志 */
    DS3231_ClearOSF();
    
    return DS3231_OK;
}

/**
 * @brief 读取DS3231温度（整数格式）
 */
DS3231_Status_t DS3231_ReadTemperature(int16_t *temperature)
{
    DS3231_Status_t status;
    uint8_t temp_msb, temp_lsb;
    int16_t temp_raw;
    
    /* ========== 参数校验 ========== */
    DS3231_CHECK_PARAM(temperature);
    DS3231_CHECK_INIT();
    
    /* 读取温度寄存器 */
    status = DS3231_I2C_ReadReg(DS3231_REG_TEMP_MSB, &temp_msb);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    status = DS3231_I2C_ReadReg(DS3231_REG_TEMP_LSB, &temp_lsb);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    /* 温度格式：高字节为有符号整数部分，低字节高2位为小数部分（0.25°C精度） */
    /* 高字节是有符号的，需要符号扩展 */
    /* 低字节bit7-6是小数部分，bit5-0未使用 */
    temp_raw = (int16_t)((int8_t)temp_msb << 8) | temp_lsb;
    temp_raw = temp_raw >> 6;  /* 右移6位，得到0.25°C精度的温度值 */
    
    *temperature = temp_raw;
    
    return DS3231_OK;
}

/**
 * @brief 读取DS3231温度（浮点数格式）
 */
DS3231_Status_t DS3231_ReadTemperatureFloat(float *temperature)
{
    int16_t temp_raw;
    DS3231_Status_t status;
    
    /* ========== 参数校验 ========== */
    if (temperature == NULL)
    {
        return DS3231_ERROR_INVALID_PARAM;
    }
    
    /* 读取整数格式温度 */
    status = DS3231_ReadTemperature(&temp_raw);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    /* 转换为浮点数（单位：°C） */
    *temperature = temp_raw / 4.0f;
    
    return DS3231_OK;
}

/**
 * @brief 检查DS3231振荡器停止标志（OSF）
 */
DS3231_Status_t DS3231_CheckOSF(uint8_t *osf_flag)
{
    DS3231_Status_t status;
    uint8_t status_reg;
    
    /* ========== 参数校验 ========== */
    DS3231_CHECK_PARAM(osf_flag);
    DS3231_CHECK_INIT();
    
    /* 读取状态寄存器 */
    status = DS3231_I2C_ReadReg(DS3231_REG_STATUS, &status_reg);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    /* 检查OSF位 */
    *osf_flag = (status_reg & DS3231_STATUS_OSF) ? 1 : 0;
    
    return DS3231_OK;
}

/**
 * @brief 清除DS3231振荡器停止标志（OSF）
 */
DS3231_Status_t DS3231_ClearOSF(void)
{
    return DS3231_ModifyStatusRegister(DS3231_STATUS_OSF, 0);
}

/**
 * @brief 检查DS3231是否正在运行
 */
DS3231_Status_t DS3231_IsRunning(uint8_t *running)
{
    DS3231_Status_t status;
    uint8_t control_reg;
    
    /* ========== 参数校验 ========== */
    DS3231_CHECK_PARAM(running);
    DS3231_CHECK_INIT();
    
    /* 读取控制寄存器 */
    status = DS3231_I2C_ReadReg(DS3231_REG_CONTROL, &control_reg);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    /* EOSC=0表示运行，EOSC=1表示停止 */
    *running = (control_reg & DS3231_CTRL_EOSC) ? 0 : 1;
    
    return DS3231_OK;
}

/**
 * @brief 启动DS3231（清除EOSC位）
 */
DS3231_Status_t DS3231_Start(void)
{
    return DS3231_ModifyControlRegister(DS3231_CTRL_EOSC, 0);
}

/**
 * @brief 停止DS3231（设置EOSC位）
 */
DS3231_Status_t DS3231_Stop(void)
{
    return DS3231_ModifyControlRegister(DS3231_CTRL_EOSC, DS3231_CTRL_EOSC);
}

/* ==================== 闹钟功能 ==================== */

/**
 * @brief 设置闹钟1
 */
DS3231_Status_t DS3231_SetAlarm1(const DS3231_Alarm_t *alarm)
{
    DS3231_Status_t status;
    uint8_t reg_data[4];
    
    /* ========== 参数校验 ========== */
    DS3231_CHECK_PARAM(alarm);
    DS3231_CHECK_INIT();
    
    /* 根据匹配模式设置寄存器值 */
    switch (alarm->mode)
    {
        case DS3231_ALARM_MODE_ONCE_PER_SECOND:
            /* 所有位设为1（0x7F） */
            reg_data[0] = 0x7F;  /* 秒 */
            reg_data[1] = 0x7F;  /* 分 */
            reg_data[2] = 0x7F;  /* 时 */
            reg_data[3] = 0x7F; /* 日/星期 */
            break;
            
        case DS3231_ALARM_MODE_SECOND_MATCH:
            /* 只有秒匹配，其他不匹配（bit7=1表示不匹配） */
            reg_data[0] = DS3231_Bin2BCD(alarm->second) & 0x7F;  /* 秒匹配：bit7=0 */
            reg_data[1] = DS3231_Bin2BCD(alarm->minute) | 0x80;  /* 分不匹配：bit7=1 */
            reg_data[2] = DS3231_Bin2BCD(alarm->hour) | 0x80;    /* 时不匹配：bit7=1 */
            reg_data[3] = DS3231_Bin2BCD(alarm->day_or_weekday) | 0x80;  /* 日不匹配：bit7=1 */
            break;
            
        case DS3231_ALARM_MODE_MIN_SEC_MATCH:
            /* 分秒匹配，其他不匹配 */
            reg_data[0] = DS3231_Bin2BCD(alarm->second) & 0x7F;  /* 秒匹配：bit7=0 */
            reg_data[1] = DS3231_Bin2BCD(alarm->minute) & 0x7F;  /* 分匹配：bit7=0 */
            reg_data[2] = DS3231_Bin2BCD(alarm->hour) | 0x80;    /* 时不匹配：bit7=1 */
            reg_data[3] = DS3231_Bin2BCD(alarm->day_or_weekday) | 0x80;  /* 日不匹配：bit7=1 */
            break;
            
        case DS3231_ALARM_MODE_HOUR_MIN_SEC_MATCH:
            /* 时分秒匹配，其他不匹配 */
            reg_data[0] = DS3231_Bin2BCD(alarm->second) & 0x7F;  /* 秒匹配：bit7=0 */
            reg_data[1] = DS3231_Bin2BCD(alarm->minute) & 0x7F;  /* 分匹配：bit7=0 */
            reg_data[2] = DS3231_Bin2BCD(alarm->hour) & 0x7F;    /* 时匹配：bit7=0 */
            reg_data[3] = DS3231_Bin2BCD(alarm->day_or_weekday) | 0x80;  /* 日不匹配：bit7=1 */
            break;
            
        case DS3231_ALARM_MODE_DAY_HOUR_MIN_SEC_MATCH:
            /* 日期时分秒匹配（所有字段都匹配，日期模式：bit7=0, DY=0） */
            reg_data[0] = DS3231_Bin2BCD(alarm->second) & 0x7F;  /* 秒匹配：bit7=0 */
            reg_data[1] = DS3231_Bin2BCD(alarm->minute) & 0x7F;  /* 分匹配：bit7=0 */
            reg_data[2] = DS3231_Bin2BCD(alarm->hour) & 0x7F;    /* 时匹配：bit7=0 */
            reg_data[3] = DS3231_Bin2BCD(alarm->day_or_weekday) & 0x7F;  /* 日期匹配：bit7=0, DY=0 */
            break;
            
        case DS3231_ALARM_MODE_WEEKDAY_HOUR_MIN_SEC_MATCH:
            /* 星期时分秒匹配（所有字段都匹配，星期模式：bit7=0, DY=1） */
            reg_data[0] = DS3231_Bin2BCD(alarm->second) & 0x7F;  /* 秒匹配：bit7=0 */
            reg_data[1] = DS3231_Bin2BCD(alarm->minute) & 0x7F;  /* 分匹配：bit7=0 */
            reg_data[2] = DS3231_Bin2BCD(alarm->hour) & 0x7F;    /* 时匹配：bit7=0 */
            /* 星期匹配：bit7=0, bit6=1(DY)，先清除bit6-7，再设置bit6 */
            reg_data[3] = (DS3231_Bin2BCD(alarm->day_or_weekday) & 0x3F) | 0x40;
            break;
            
        default:
            return DS3231_ERROR_INVALID_PARAM;
    }
    
    /* 写入闹钟1寄存器（0x07-0x0A） */
    status = DS3231_I2C_WriteRegs(DS3231_REG_ALARM1_SEC, reg_data, 4);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    return DS3231_OK;
}

/**
 * @brief 设置闹钟2（无秒字段）
 */
DS3231_Status_t DS3231_SetAlarm2(const DS3231_Alarm_t *alarm)
{
    DS3231_Status_t status;
    uint8_t reg_data[3];
    
    /* ========== 参数校验 ========== */
    DS3231_CHECK_PARAM(alarm);
    DS3231_CHECK_INIT();
    
    /* 根据匹配模式设置寄存器值 */
    switch (alarm->mode)
    {
        case DS3231_ALARM_MODE_ONCE_PER_SECOND:
        case DS3231_ALARM_MODE_SECOND_MATCH:
            /* 闹钟2不支持秒匹配，使用分匹配模式 */
            reg_data[0] = 0x7F;  /* 分 */
            reg_data[1] = 0x7F;  /* 时 */
            reg_data[2] = 0x7F;  /* 日/星期 */
            break;
            
        case DS3231_ALARM_MODE_MIN_SEC_MATCH:
            /* 分匹配（闹钟2无秒字段，所以MIN_SEC_MATCH对应分匹配） */
            reg_data[0] = DS3231_Bin2BCD(alarm->minute) & 0x7F;  /* 分匹配：bit7=0 */
            reg_data[1] = DS3231_Bin2BCD(alarm->hour) | 0x80;    /* 时不匹配：bit7=1 */
            reg_data[2] = DS3231_Bin2BCD(alarm->day_or_weekday) | 0x80;  /* 日不匹配：bit7=1 */
            break;
            
        case DS3231_ALARM_MODE_HOUR_MIN_SEC_MATCH:
            /* 时分匹配 */
            reg_data[0] = DS3231_Bin2BCD(alarm->minute) & 0x7F;  /* 分匹配：bit7=0 */
            reg_data[1] = DS3231_Bin2BCD(alarm->hour) & 0x7F;    /* 时匹配：bit7=0 */
            reg_data[2] = DS3231_Bin2BCD(alarm->day_or_weekday) | 0x80;  /* 日不匹配：bit7=1 */
            break;
            
        case DS3231_ALARM_MODE_DAY_HOUR_MIN_SEC_MATCH:
            /* 日期时分匹配 */
            reg_data[0] = DS3231_Bin2BCD(alarm->minute) & 0x7F;  /* 分匹配：bit7=0 */
            reg_data[1] = DS3231_Bin2BCD(alarm->hour) & 0x7F;    /* 时匹配：bit7=0 */
            reg_data[2] = DS3231_Bin2BCD(alarm->day_or_weekday) & 0x7F;  /* 日期匹配：bit7=0, DY=0 */
            break;
            
        case DS3231_ALARM_MODE_WEEKDAY_HOUR_MIN_SEC_MATCH:
            /* 星期时分匹配 */
            reg_data[0] = DS3231_Bin2BCD(alarm->minute) & 0x7F;  /* 分匹配：bit7=0 */
            reg_data[1] = DS3231_Bin2BCD(alarm->hour) & 0x7F;    /* 时匹配：bit7=0 */
            /* 星期匹配：bit7=0, bit6=1(DY)，先清除bit6-7，再设置bit6 */
            reg_data[2] = (DS3231_Bin2BCD(alarm->day_or_weekday) & 0x3F) | 0x40;
            break;
            
        default:
            return DS3231_ERROR_INVALID_PARAM;
    }
    
    /* 写入闹钟2寄存器（0x0B-0x0D） */
    status = DS3231_I2C_WriteRegs(DS3231_REG_ALARM2_MIN, reg_data, 3);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    return DS3231_OK;
}

/**
 * @brief 读取闹钟1
 */
DS3231_Status_t DS3231_ReadAlarm1(DS3231_Alarm_t *alarm)
{
    DS3231_Status_t status;
    uint8_t reg_data[4];
    
    /* ========== 参数校验 ========== */
    DS3231_CHECK_PARAM(alarm);
    DS3231_CHECK_INIT();
    
    /* 读取闹钟1寄存器（0x07-0x0A） */
    status = DS3231_I2C_ReadRegs(DS3231_REG_ALARM1_SEC, reg_data, 4);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    /* 解析寄存器值 */
    alarm->second = DS3231_BCD2Bin(reg_data[0] & 0x7F);
    alarm->minute = DS3231_BCD2Bin(reg_data[1] & 0x7F);
    alarm->hour = DS3231_BCD2Bin(reg_data[2] & 0x7F);
    /* 日/星期寄存器：bit7=A1M4/DY，bit6=DY（仅在bit7=0时有效），bit5-0=日/星期值 */
    /* 读取时清除bit7和bit6，只保留实际值 */
    alarm->day_or_weekday = DS3231_BCD2Bin(reg_data[3] & 0x3F);
    
    /* 判断匹配模式（根据bit7的值判断） */
    /* bit7=1表示该字段不匹配，bit7=0表示该字段匹配 */
    if ((reg_data[0] & 0x7F) == 0x7F && (reg_data[1] & 0x7F) == 0x7F && 
        (reg_data[2] & 0x7F) == 0x7F && (reg_data[3] & 0x7F) == 0x7F)
    {
        /* 所有字段的bit0-6都是1，表示每秒触发 */
        alarm->mode = DS3231_ALARM_MODE_ONCE_PER_SECOND;
    }
    else if (!(reg_data[0] & 0x80) && (reg_data[1] & 0x80) && 
             (reg_data[2] & 0x80) && (reg_data[3] & 0x80))
    {
        /* 只有秒匹配 */
        alarm->mode = DS3231_ALARM_MODE_SECOND_MATCH;
    }
    else if (!(reg_data[0] & 0x80) && !(reg_data[1] & 0x80) && 
             (reg_data[2] & 0x80) && (reg_data[3] & 0x80))
    {
        /* 分秒匹配 */
        alarm->mode = DS3231_ALARM_MODE_MIN_SEC_MATCH;
    }
    else if (!(reg_data[0] & 0x80) && !(reg_data[1] & 0x80) && 
             !(reg_data[2] & 0x80) && (reg_data[3] & 0x80))
    {
        /* 时分秒匹配 */
        alarm->mode = DS3231_ALARM_MODE_HOUR_MIN_SEC_MATCH;
    }
    else if (!(reg_data[0] & 0x80) && !(reg_data[1] & 0x80) && 
             !(reg_data[2] & 0x80) && !(reg_data[3] & 0x80))
    {
        /* 所有字段都匹配，根据bit6判断是日期还是星期 */
        if (reg_data[3] & 0x40)
        {
            /* bit6=1：星期匹配 */
            alarm->mode = DS3231_ALARM_MODE_WEEKDAY_HOUR_MIN_SEC_MATCH;
        }
        else
        {
            /* bit6=0：日期匹配 */
            alarm->mode = DS3231_ALARM_MODE_DAY_HOUR_MIN_SEC_MATCH;
        }
    }
    else
    {
        /* 无法识别的模式，默认为时分秒匹配 */
        alarm->mode = DS3231_ALARM_MODE_HOUR_MIN_SEC_MATCH;
    }
    
    return DS3231_OK;
}

/**
 * @brief 读取闹钟2
 */
DS3231_Status_t DS3231_ReadAlarm2(DS3231_Alarm_t *alarm)
{
    DS3231_Status_t status;
    uint8_t reg_data[3];
    
    /* ========== 参数校验 ========== */
    DS3231_CHECK_PARAM(alarm);
    DS3231_CHECK_INIT();
    
    /* 读取闹钟2寄存器（0x0B-0x0D） */
    status = DS3231_I2C_ReadRegs(DS3231_REG_ALARM2_MIN, reg_data, 3);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    /* 解析寄存器值 */
    alarm->second = 0;  /* 闹钟2无秒字段 */
    alarm->minute = DS3231_BCD2Bin(reg_data[0] & 0x7F);
    alarm->hour = DS3231_BCD2Bin(reg_data[1] & 0x7F);
    /* 日/星期寄存器：bit7=A2M4/DY，bit6=DY（仅在bit7=0时有效），bit5-0=日/星期值 */
    /* 读取时清除bit7和bit6，只保留实际值 */
    alarm->day_or_weekday = DS3231_BCD2Bin(reg_data[2] & 0x3F);
    
    /* 判断匹配模式（闹钟2无秒字段） */
    if ((reg_data[0] & 0x7F) == 0x7F && (reg_data[1] & 0x7F) == 0x7F && 
        (reg_data[2] & 0x7F) == 0x7F)
    {
        /* 所有字段的bit0-6都是1，表示每分钟触发 */
        alarm->mode = DS3231_ALARM_MODE_MIN_SEC_MATCH;  /* 对应分匹配 */
    }
    else if (!(reg_data[0] & 0x80) && (reg_data[1] & 0x80) && (reg_data[2] & 0x80))
    {
        /* 只有分匹配 */
        alarm->mode = DS3231_ALARM_MODE_MIN_SEC_MATCH;
    }
    else if (!(reg_data[0] & 0x80) && !(reg_data[1] & 0x80) && (reg_data[2] & 0x80))
    {
        /* 时分匹配 */
        alarm->mode = DS3231_ALARM_MODE_HOUR_MIN_SEC_MATCH;
    }
    else if (!(reg_data[0] & 0x80) && !(reg_data[1] & 0x80) && !(reg_data[2] & 0x80))
    {
        /* 所有字段都匹配，根据bit6判断是日期还是星期 */
        if (reg_data[2] & 0x40)
        {
            /* bit6=1：星期匹配 */
            alarm->mode = DS3231_ALARM_MODE_WEEKDAY_HOUR_MIN_SEC_MATCH;
        }
        else
        {
            /* bit6=0：日期匹配 */
            alarm->mode = DS3231_ALARM_MODE_DAY_HOUR_MIN_SEC_MATCH;
        }
    }
    else
    {
        /* 无法识别的模式，默认为时分匹配 */
        alarm->mode = DS3231_ALARM_MODE_HOUR_MIN_SEC_MATCH;
    }
    
    return DS3231_OK;
}

/**
 * @brief 检查闹钟1标志
 */
DS3231_Status_t DS3231_CheckAlarm1Flag(uint8_t *flag)
{
    return DS3231_CheckAlarmFlagInternal(1, flag);
}

/**
 * @brief 检查闹钟2标志
 */
DS3231_Status_t DS3231_CheckAlarm2Flag(uint8_t *flag)
{
    return DS3231_CheckAlarmFlagInternal(2, flag);
}

/**
 * @brief 清除闹钟1标志
 */
DS3231_Status_t DS3231_ClearAlarm1Flag(void)
{
    return DS3231_ClearAlarmFlagInternal(1);
}

/**
 * @brief 清除闹钟2标志
 */
DS3231_Status_t DS3231_ClearAlarm2Flag(void)
{
    return DS3231_ClearAlarmFlagInternal(2);
}

/**
 * @brief 使能闹钟1中断
 */
DS3231_Status_t DS3231_EnableAlarm1(void)
{
    return DS3231_SetAlarmInterruptInternal(1, 1);
}

/**
 * @brief 禁用闹钟1中断
 */
DS3231_Status_t DS3231_DisableAlarm1(void)
{
    return DS3231_SetAlarmInterruptInternal(1, 0);
}

/**
 * @brief 使能闹钟2中断
 */
DS3231_Status_t DS3231_EnableAlarm2(void)
{
    return DS3231_SetAlarmInterruptInternal(2, 1);
}

/**
 * @brief 禁用闹钟2中断
 */
DS3231_Status_t DS3231_DisableAlarm2(void)
{
    return DS3231_SetAlarmInterruptInternal(2, 0);
}

/* ==================== 方波输出功能 ==================== */

/**
 * @brief 设置方波输出频率和使能
 */
DS3231_Status_t DS3231_SetSquareWave(DS3231_SquareWaveFreq_t freq, uint8_t enable)
{
    DS3231_Status_t status;
    uint8_t mask;
    uint8_t value;
    
    DS3231_CHECK_INIT();
    
    /* 设置方波频率位（RS1和RS2） */
    mask = DS3231_CTRL_RS1 | DS3231_CTRL_RS2;
    value = freq & mask;
    
    status = DS3231_ModifyControlRegister(mask, value);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    /* 设置中断控制位（INTCN）：使能时设为0（方波输出），禁用时设为1（中断模式） */
    mask = DS3231_CTRL_INTCN;
    value = enable ? 0 : DS3231_CTRL_INTCN;
    
    return DS3231_ModifyControlRegister(mask, value);
}

/**
 * @brief 禁用方波输出
 */
DS3231_Status_t DS3231_DisableSquareWave(void)
{
    return DS3231_ModifyControlRegister(DS3231_CTRL_INTCN, DS3231_CTRL_INTCN);
}

/**
 * @brief 使能电池备份方波输出
 */
DS3231_Status_t DS3231_EnableBatteryBackupSQW(uint8_t enable)
{
    uint8_t value = enable ? DS3231_CTRL_BBSQW : 0;
    return DS3231_ModifyControlRegister(DS3231_CTRL_BBSQW, value);
}

/* ==================== 32kHz输出控制 ==================== */

/**
 * @brief 使能32kHz输出
 */
DS3231_Status_t DS3231_Enable32kHz(void)
{
    return DS3231_ModifyStatusRegister(DS3231_STATUS_EN32KHZ, DS3231_STATUS_EN32KHZ);
}

/**
 * @brief 禁用32kHz输出
 */
DS3231_Status_t DS3231_Disable32kHz(void)
{
    return DS3231_ModifyStatusRegister(DS3231_STATUS_EN32KHZ, 0);
}

/**
 * @brief 检查32kHz输出状态
 */
DS3231_Status_t DS3231_Is32kHzEnabled(uint8_t *enabled)
{
    DS3231_Status_t status;
    uint8_t status_reg;
    
    /* ========== 参数校验 ========== */
    DS3231_CHECK_PARAM(enabled);
    DS3231_CHECK_INIT();
    
    /* 读取状态寄存器 */
    status = DS3231_I2C_ReadReg(DS3231_REG_STATUS, &status_reg);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    /* 检查EN32KHZ位 */
    *enabled = (status_reg & DS3231_STATUS_EN32KHZ) ? 1 : 0;
    
    return DS3231_OK;
}

/* ==================== 温度转换控制 ==================== */

/**
 * @brief 触发温度转换
 */
DS3231_Status_t DS3231_TriggerTemperatureConversion(void)
{
    return DS3231_ModifyControlRegister(DS3231_CTRL_CONV, DS3231_CTRL_CONV);
}

/**
 * @brief 检查温度转换忙标志
 */
DS3231_Status_t DS3231_IsTemperatureBusy(uint8_t *busy)
{
    DS3231_Status_t status;
    uint8_t status_reg;
    
    /* ========== 参数校验 ========== */
    DS3231_CHECK_PARAM(busy);
    DS3231_CHECK_INIT();
    
    /* 读取状态寄存器 */
    status = DS3231_I2C_ReadReg(DS3231_REG_STATUS, &status_reg);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    /* 检查BSY位 */
    *busy = (status_reg & DS3231_STATUS_BSY) ? 1 : 0;
    
    return DS3231_OK;
}

/* ==================== 中断控制配置 ==================== */

/**
 * @brief 设置中断模式
 */
DS3231_Status_t DS3231_SetInterruptMode(DS3231_IntMode_t mode)
{
    uint8_t value = (mode == DS3231_INT_MODE_ALARM) ? DS3231_CTRL_INTCN : 0;
    return DS3231_ModifyControlRegister(DS3231_CTRL_INTCN, value);
}

/**
 * @brief 获取中断模式
 */
DS3231_Status_t DS3231_GetInterruptMode(DS3231_IntMode_t *mode)
{
    DS3231_Status_t status;
    uint8_t control_reg;
    
    /* ========== 参数校验 ========== */
    DS3231_CHECK_PARAM(mode);
    DS3231_CHECK_INIT();
    
    /* 读取控制寄存器 */
    status = DS3231_I2C_ReadReg(DS3231_REG_CONTROL, &control_reg);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    /* 检查INTCN位 */
    *mode = (control_reg & DS3231_CTRL_INTCN) ? DS3231_INT_MODE_ALARM : DS3231_INT_MODE_SQUARE_WAVE;
    
    return DS3231_OK;
}

/* ==================== 老化偏移调整 ==================== */

/**
 * @brief 读取老化偏移值
 */
DS3231_Status_t DS3231_ReadAgingOffset(int8_t *offset)
{
    DS3231_Status_t status;
    uint8_t reg_value;
    
    /* ========== 参数校验 ========== */
    DS3231_CHECK_PARAM(offset);
    DS3231_CHECK_INIT();
    
    /* 读取老化偏移寄存器 */
    status = DS3231_I2C_ReadReg(DS3231_REG_AGING_OFFSET, &reg_value);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    /* 转换为有符号数（-128到+127） */
    *offset = (int8_t)reg_value;
    
    return DS3231_OK;
}

/**
 * @brief 设置老化偏移值
 */
DS3231_Status_t DS3231_SetAgingOffset(int8_t offset)
{
    DS3231_Status_t status;
    
    if (!g_ds3231_initialized)
    {
        return DS3231_ERROR_NOT_INITIALIZED;
    }
    
    /* 参数范围校验（-128到+127） */
    if (offset < -128 || offset > 127)
    {
        return DS3231_ERROR_INVALID_PARAM;
    }
    
    /* 写入老化偏移寄存器 */
    status = DS3231_I2C_WriteReg(DS3231_REG_AGING_OFFSET, (uint8_t)offset);
    if (status != DS3231_OK)
    {
        return status;
    }
    
    return DS3231_OK;
}

/* ==================== 寄存器直接访问 ==================== */

/**
 * @brief 读取控制寄存器
 */
DS3231_Status_t DS3231_ReadControlRegister(uint8_t *value)
{
    DS3231_CHECK_PARAM(value);
    DS3231_CHECK_INIT();
    return DS3231_I2C_ReadReg(DS3231_REG_CONTROL, value);
}

/**
 * @brief 写入控制寄存器
 */
DS3231_Status_t DS3231_WriteControlRegister(uint8_t value)
{
    DS3231_CHECK_INIT();
    return DS3231_I2C_WriteReg(DS3231_REG_CONTROL, value);
}

/**
 * @brief 读取状态寄存器
 */
DS3231_Status_t DS3231_ReadStatusRegister(uint8_t *value)
{
    DS3231_CHECK_PARAM(value);
    DS3231_CHECK_INIT();
    return DS3231_I2C_ReadReg(DS3231_REG_STATUS, value);
}

/**
 * @brief 写入状态寄存器
 */
DS3231_Status_t DS3231_WriteStatusRegister(uint8_t value)
{
    DS3231_CHECK_INIT();
    return DS3231_I2C_WriteReg(DS3231_REG_STATUS, value);
}

#endif /* CONFIG_MODULE_DS3231_ENABLED */
