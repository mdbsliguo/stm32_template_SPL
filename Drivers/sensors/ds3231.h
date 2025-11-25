/**
 * @file ds3231.h
 * @brief DS3231实时时钟模块驱动
 * @version 3.0.0
 * @date 2024-01-01
 * @details 支持硬件I2C和软件I2C两种接口的DS3231 RTC驱动，完整功能实现
 *          - 时间读取/设置
 *          - 温度读取
 *          - 闹钟功能（Alarm 1 & Alarm 2）
 *          - 方波输出配置
 *          - 32kHz输出控制
 *          - 温度转换控制
 *          - 中断控制配置
 *          - 老化偏移调整
 *          - 寄存器直接访问
 * @note 解耦版本：软件I2C部分已解耦，直接使用soft_i2c模块，不再重复实现I2C时序
 */

#ifndef DS3231_H
#define DS3231_H

#ifdef __cplusplus
extern "C" {
#endif

#include "board.h"
#include "error_code.h"
#include "gpio.h"
#include <stdint.h>

/* ==================== 模块依赖处理（解耦设计） ==================== */
/* 
 * 原则：模块不应定义其他模块的类型，只应使用
 * - I2C_Instance_t 由 i2c_hw.h 定义
 * - SoftI2C_Instance_t 由 i2c_sw.h 定义
 * - ds3231.h 只负责使用这些类型，不负责定义
 * 
 * 使用规则：
 * - 使用硬件I2C模式前，使用者必须先包含 i2c_hw.h
 * - 使用软件I2C模式前，使用者必须先包含 i2c_sw.h
 */

/* 条件编译：硬件I2C模块 */
/* 如果硬件I2C模块启用，尝试包含i2c_hw.h（使用者应该先包含i2c_hw.h） */
#ifdef CONFIG_MODULE_I2C_ENABLED
#if CONFIG_MODULE_I2C_ENABLED
/* 如果i2c_hw.h未被包含，这里会包含它（但通常使用者应该先包含） */
#ifndef I2C_HW_H
#include "i2c_hw.h"
#endif
#endif
#endif

/* 条件编译：软件I2C模块 */
/* 如果软件I2C模块启用，尝试包含i2c_sw.h（使用者应该先包含i2c_sw.h） */
#ifdef CONFIG_MODULE_SOFT_I2C_ENABLED
#if CONFIG_MODULE_SOFT_I2C_ENABLED
/* 如果i2c_sw.h未被包含，这里会包含它（但通常使用者应该先包含） */
#ifndef I2C_SW_H
#include "i2c_sw.h"
#endif
#endif
#endif

/**
 * @brief DS3231错误码
 */
typedef enum {
    DS3231_OK = ERROR_OK,                              /**< 操作成功 */
    DS3231_ERROR_NOT_INITIALIZED = ERROR_BASE_DS3231 - 1,  /**< 未初始化 */
    DS3231_ERROR_INVALID_PARAM = ERROR_BASE_DS3231 - 2,   /**< 参数非法 */
    DS3231_ERROR_I2C_FAILED = ERROR_BASE_DS3231 - 3,     /**< I2C通信失败 */
    DS3231_ERROR_TIMEOUT = ERROR_BASE_DS3231 - 4,         /**< 操作超时 */
    DS3231_ERROR_GPIO_FAILED = ERROR_BASE_DS3231 - 5,     /**< GPIO配置失败 */
} DS3231_Status_t;

/**
 * @brief DS3231接口类型
 */
typedef enum {
    DS3231_INTERFACE_HARDWARE = 0,  /**< 硬件I2C接口 */
    DS3231_INTERFACE_SOFTWARE = 1,  /**< 软件I2C接口 */
} DS3231_InterfaceType_t;

/**
 * @brief DS3231硬件I2C配置结构体
 * @note 仅当硬件I2C模块启用（CONFIG_MODULE_I2C_ENABLED=1）时可用
 * @note 使用前必须先包含 i2c_hw.h
 */
#ifdef I2C_HW_H
typedef struct {
    I2C_Instance_t i2c_instance;    /**< I2C实例（I2C_INSTANCE_1或I2C_INSTANCE_2） */
} DS3231_HardwareI2C_Config_t;
#else
/* 硬件I2C模块未启用，使用不完整类型（仅用于编译，不应使用） */
typedef struct DS3231_HardwareI2C_Config DS3231_HardwareI2C_Config_t;
#endif

/**
 * @brief DS3231软件I2C配置结构体
 * @note 仅当软件I2C模块启用（CONFIG_MODULE_SOFT_I2C_ENABLED=1）时可用
 * @note 使用前必须先包含 i2c_sw.h
 * @note 解耦版本：直接使用soft_i2c模块，不再重复实现I2C时序
 */
#ifdef I2C_SW_H
typedef struct {
    SoftI2C_Instance_t soft_i2c_instance;  /**< 软件I2C实例索引（SOFT_I2C_INSTANCE_1等） */
} DS3231_SoftwareI2C_Config_t;
#else
/* 软件I2C模块未启用，使用不完整类型（仅用于编译，不应使用） */
typedef struct DS3231_SoftwareI2C_Config DS3231_SoftwareI2C_Config_t;
#endif

/**
 * @brief DS3231配置结构体（统一配置接口）
 * @note 根据 interface_type 选择对应的配置结构体
 */
typedef struct {
    DS3231_InterfaceType_t interface_type;  /**< 接口类型（硬件I2C或软件I2C） */
    union {
#ifdef I2C_HW_H
        DS3231_HardwareI2C_Config_t hardware;  /**< 硬件I2C配置（仅当硬件I2C模块启用时可用） */
#endif
#ifdef I2C_SW_H
        DS3231_SoftwareI2C_Config_t software;   /**< 软件I2C配置（仅当软件I2C模块启用时可用） */
#endif
        uint8_t _placeholder[8];  /**< 占位符，确保union不为空（当两个模块都未启用时） */
    } config;                                 /**< 接口配置（根据interface_type选择） */
} DS3231_Config_t;

/**
 * @brief DS3231时间结构体
 */
typedef struct {
    uint8_t second;    /**< 秒（0-59） */
    uint8_t minute;    /**< 分（0-59） */
    uint8_t hour;      /**< 时（0-23，24小时制） */
    uint8_t weekday;   /**< 星期（1-7，1=Sunday） */
    uint8_t day;       /**< 日（1-31） */
    uint8_t month;     /**< 月（1-12） */
    uint16_t year;     /**< 年（2000-2099） */
} DS3231_Time_t;

/**
 * @brief DS3231闹钟匹配模式
 */
typedef enum {
    DS3231_ALARM_MODE_ONCE_PER_SECOND = 0x0F,              /**< 每秒触发（所有位设为1） */
    DS3231_ALARM_MODE_SECOND_MATCH = 0x0E,                  /**< 秒匹配（bit0-3=1） */
    DS3231_ALARM_MODE_MIN_SEC_MATCH = 0x0C,                /**< 分秒匹配（bit0-3和bit4-6=1） */
    DS3231_ALARM_MODE_HOUR_MIN_SEC_MATCH = 0x08,            /**< 时分秒匹配（bit0-3、bit4-6、bit7-8=1） */
    DS3231_ALARM_MODE_DAY_HOUR_MIN_SEC_MATCH = 0x00,       /**< 日期时分秒匹配（bit7=0，其他匹配） */
    DS3231_ALARM_MODE_WEEKDAY_HOUR_MIN_SEC_MATCH = 0x80,   /**< 星期时分秒匹配（bit7=1，其他匹配） */
} DS3231_AlarmMode_t;

/**
 * @brief DS3231闹钟结构体
 */
typedef struct {
    uint8_t second;              /**< 秒（0-59），或0x80-0x8F表示匹配模式 */
    uint8_t minute;             /**< 分（0-59），或0x80-0xBF表示匹配模式 */
    uint8_t hour;               /**< 时（0-23），或0x80-0xFF表示匹配模式 */
    uint8_t day_or_weekday;     /**< 日期（1-31）或星期（1-7），bit7=1表示星期匹配 */
    DS3231_AlarmMode_t mode;    /**< 闹钟匹配模式 */
} DS3231_Alarm_t;

/**
 * @brief DS3231方波输出频率
 */
typedef enum {
    DS3231_SQW_1HZ = 0x00,      /**< 1Hz */
    DS3231_SQW_1024HZ = 0x08,   /**< 1.024kHz */
    DS3231_SQW_4096HZ = 0x10,   /**< 4.096kHz */
    DS3231_SQW_8192HZ = 0x18,   /**< 8.192kHz */
} DS3231_SquareWaveFreq_t;

/**
 * @brief DS3231中断模式
 */
typedef enum {
    DS3231_INT_MODE_SQUARE_WAVE = 0,  /**< 方波输出模式（INTCN=0） */
    DS3231_INT_MODE_ALARM = 1,        /**< 闹钟中断模式（INTCN=1） */
} DS3231_IntMode_t;

/**
 * @brief DS3231初始化
 * @param[in] config 配置结构体指针（包含接口类型和硬件参数）
 * @return DS3231_Status_t 错误码
 * @note 硬件I2C模式：需要先初始化I2C模块
 * @note 软件I2C模式：自动配置GPIO引脚
 * 
 * @example 硬件I2C初始化
 * DS3231_Config_t config = {
 *     .interface_type = DS3231_INTERFACE_HARDWARE,
 *     .config.hardware = {
 *         .i2c_instance = I2C_INSTANCE_1
 *     }
 * };
 * DS3231_Init(&config);
 * 
 * @example 软件I2C初始化
 * DS3231_Config_t config = {
 *     .interface_type = DS3231_INTERFACE_SOFTWARE,
 *     .config.software = {
 *         .scl_port = GPIOB,
 *         .scl_pin = GPIO_Pin_6,
 *         .sda_port = GPIOB,
 *         .sda_pin = GPIO_Pin_7,
 *         .delay_us = 5
 *     }
 * };
 * DS3231_Init(&config);
 */
DS3231_Status_t DS3231_Init(const DS3231_Config_t *config);

/**
 * @brief DS3231反初始化
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_Deinit(void);

/**
 * @brief 检查DS3231是否已初始化
 * @return uint8_t 1=已初始化，0=未初始化
 */
uint8_t DS3231_IsInitialized(void);

/**
 * @brief 读取DS3231当前时间
 * @param[out] time 时间结构体指针
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_ReadTime(DS3231_Time_t *time);

/**
 * @brief 设置DS3231时间
 * @param[in] time 时间结构体指针
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_SetTime(const DS3231_Time_t *time);

/**
 * @brief 读取DS3231温度（摄氏度）
 * @param[out] temperature 温度值指针（单位：0.25°C，例如25.00°C = 100）
 * @return DS3231_Status_t 错误码
 * @note 温度值 = temperature / 4.0，精度0.25°C
 */
DS3231_Status_t DS3231_ReadTemperature(int16_t *temperature);

/**
 * @brief 读取DS3231温度（浮点数，摄氏度）
 * @param[out] temperature 温度值指针（单位：°C）
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_ReadTemperatureFloat(float *temperature);

/**
 * @brief 检查DS3231振荡器停止标志（OSF）
 * @param[out] osf_flag OSF标志指针（1=振荡器已停止，0=正常）
 * @return DS3231_Status_t 错误码
 * @note OSF=1表示RTC可能丢失时间，需要重新设置时间
 */
DS3231_Status_t DS3231_CheckOSF(uint8_t *osf_flag);

/**
 * @brief 清除DS3231振荡器停止标志（OSF）
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_ClearOSF(void);

/**
 * @brief 检查DS3231是否正在运行
 * @param[out] running 运行状态指针（1=运行，0=停止）
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_IsRunning(uint8_t *running);

/**
 * @brief 启动DS3231（清除EOSC位）
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_Start(void);

/**
 * @brief 停止DS3231（设置EOSC位）
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_Stop(void);

/* ==================== 闹钟功能 ==================== */

/**
 * @brief 设置闹钟1
 * @param[in] alarm 闹钟结构体指针
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_SetAlarm1(const DS3231_Alarm_t *alarm);

/**
 * @brief 设置闹钟2（无秒字段）
 * @param[in] alarm 闹钟结构体指针（second字段忽略）
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_SetAlarm2(const DS3231_Alarm_t *alarm);

/**
 * @brief 读取闹钟1
 * @param[out] alarm 闹钟结构体指针
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_ReadAlarm1(DS3231_Alarm_t *alarm);

/**
 * @brief 读取闹钟2
 * @param[out] alarm 闹钟结构体指针（second字段为0）
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_ReadAlarm2(DS3231_Alarm_t *alarm);

/**
 * @brief 检查闹钟1标志
 * @param[out] flag 标志指针（1=触发，0=未触发）
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_CheckAlarm1Flag(uint8_t *flag);

/**
 * @brief 检查闹钟2标志
 * @param[out] flag 标志指针（1=触发，0=未触发）
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_CheckAlarm2Flag(uint8_t *flag);

/**
 * @brief 清除闹钟1标志
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_ClearAlarm1Flag(void);

/**
 * @brief 清除闹钟2标志
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_ClearAlarm2Flag(void);

/**
 * @brief 使能闹钟1中断
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_EnableAlarm1(void);

/**
 * @brief 禁用闹钟1中断
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_DisableAlarm1(void);

/**
 * @brief 使能闹钟2中断
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_EnableAlarm2(void);

/**
 * @brief 禁用闹钟2中断
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_DisableAlarm2(void);

/* ==================== 方波输出功能 ==================== */

/**
 * @brief 设置方波输出频率和使能
 * @param[in] freq 方波频率
 * @param[in] enable 使能标志（1=使能，0=禁用）
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_SetSquareWave(DS3231_SquareWaveFreq_t freq, uint8_t enable);

/**
 * @brief 禁用方波输出
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_DisableSquareWave(void);

/**
 * @brief 使能电池备份方波输出
 * @param[in] enable 使能标志（1=使能，0=禁用）
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_EnableBatteryBackupSQW(uint8_t enable);

/* ==================== 32kHz输出控制 ==================== */

/**
 * @brief 使能32kHz输出
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_Enable32kHz(void);

/**
 * @brief 禁用32kHz输出
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_Disable32kHz(void);

/**
 * @brief 检查32kHz输出状态
 * @param[out] enabled 使能状态指针（1=使能，0=禁用）
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_Is32kHzEnabled(uint8_t *enabled);

/* ==================== 温度转换控制 ==================== */

/**
 * @brief 触发温度转换
 * @return DS3231_Status_t 错误码
 * @note 转换需要约1秒时间，可通过DS3231_IsTemperatureBusy()检查
 */
DS3231_Status_t DS3231_TriggerTemperatureConversion(void);

/**
 * @brief 检查温度转换忙标志
 * @param[out] busy 忙标志指针（1=忙，0=空闲）
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_IsTemperatureBusy(uint8_t *busy);

/* ==================== 中断控制配置 ==================== */

/**
 * @brief 设置中断模式
 * @param[in] mode 中断模式
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_SetInterruptMode(DS3231_IntMode_t mode);

/**
 * @brief 获取中断模式
 * @param[out] mode 中断模式指针
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_GetInterruptMode(DS3231_IntMode_t *mode);

/* ==================== 老化偏移调整 ==================== */

/**
 * @brief 读取老化偏移值
 * @param[out] offset 偏移值指针（-128到+127，单位：0.1ppm）
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_ReadAgingOffset(int8_t *offset);

/**
 * @brief 设置老化偏移值
 * @param[in] offset 偏移值（-128到+127，单位：0.1ppm）
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_SetAgingOffset(int8_t offset);

/* ==================== 寄存器直接访问 ==================== */

/**
 * @brief 读取控制寄存器
 * @param[out] value 寄存器值指针
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_ReadControlRegister(uint8_t *value);

/**
 * @brief 写入控制寄存器
 * @param[in] value 寄存器值
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_WriteControlRegister(uint8_t value);

/**
 * @brief 读取状态寄存器
 * @param[out] value 寄存器值指针
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_ReadStatusRegister(uint8_t *value);

/**
 * @brief 写入状态寄存器
 * @param[in] value 寄存器值
 * @return DS3231_Status_t 错误码
 */
DS3231_Status_t DS3231_WriteStatusRegister(uint8_t value);

#ifdef __cplusplus
}
#endif

#endif /* DS3231_H */

