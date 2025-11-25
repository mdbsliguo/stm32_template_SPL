/**
 * @file board.h
 * @brief 硬件配置头文件（案例11独立工程专用）
 * @details 此文件包含案例11所需的硬件配置
 * 
 * 注意：这是独立工程的board.h，包含案例所需的硬件配置
 * 
 * 硬件要求：
 * - LED1：PA1（系统状态指示）
 * - DS3231实时时钟模块（I2C接口）
 *   - SCL：PB10（I2C2）
 *   - SDA：PB11（I2C2）
 */

#ifndef BOARD_H
#define BOARD_H

#include "stm32f10x.h"
#include <stdint.h>

/* ==================== LED配置 ==================== */

/* LED配置结构体 */
typedef struct
{
    GPIO_TypeDef *port;   /**< GPIO端口基地址 */
    uint16_t pin;         /**< 引脚号 */
    uint8_t active_level; /**< 有效电平（Bit_SET或Bit_RESET） */
    uint8_t enabled;      /**< 使能标志：1=启用，0=禁用 */
} LED_Config_t;

/* LED统一配置表 - 案例11配置 */
#define LED_CONFIGS {                                                            \
    {GPIOA, GPIO_Pin_1, Bit_RESET, 1}, /* LED1：PA1，低电平点亮，启用（系统状态指示） */ \
}

/* ==================== I2C配置 ==================== */

/* I2C配置结构体 */
typedef struct
{
    I2C_TypeDef *i2c_periph;        /**< I2C外设（I2C1或I2C2） */
    GPIO_TypeDef *scl_port;         /**< SCL引脚端口 */
    uint16_t scl_pin;               /**< SCL引脚号 */
    GPIO_TypeDef *sda_port;         /**< SDA引脚端口 */
    uint16_t sda_pin;               /**< SDA引脚号 */
    uint32_t clock_speed;           /**< I2C时钟速度（Hz），标准模式<=100kHz，快速模式<=400kHz */
    uint16_t own_address;           /**< 本设备地址（7位地址，用于从模式） */
    uint8_t enabled;                /**< 使能标志：1=启用，0=禁用 */
} I2C_Config_t;

/* I2C统一配置表 - 案例11配置（I2C2，PB10/11） */
/* 注意：数组索引对应I2C_INSTANCE枚举值，I2C_INSTANCE_1=0，I2C_INSTANCE_2=1 */
#define I2C_CONFIGS {                                                                    \
    {NULL, NULL, 0, NULL, 0, 0, 0, 0}, /* I2C1：未使用，禁用 */                        \
    {I2C2, GPIOB, GPIO_Pin_10, GPIOB, GPIO_Pin_11, 100000, 0x00, 1}, /* I2C2：PB10(SCL), PB11(SDA)，100kHz，从地址0x00，启用 */ \
}

/* ==================== OLED配置 ==================== */

/* OLED I2C接口类型 */
typedef enum {
    OLED_I2C_TYPE_SOFTWARE = 0,  /**< 软件I2C接口 */
    OLED_I2C_TYPE_HARDWARE = 1,  /**< 硬件I2C接口 */
} OLED_I2C_Type_t;

/* OLED I2C配置结构体（保留用于向后兼容） */
typedef struct
{
    GPIO_TypeDef *scl_port; /**< SCL引脚端口 */
    uint16_t scl_pin;       /**< SCL引脚号 */
    GPIO_TypeDef *sda_port; /**< SDA引脚端口 */
    uint16_t sda_pin;       /**< SDA引脚号 */
} OLED_I2C_Config_t;

/* OLED I2C配置 - 案例11配置（软件I2C，PB8/9） */
#define OLED_I2C_CONFIG {      \
    GPIOB,                     \
    GPIO_Pin_8, /* SCL: PB8 */ \
    GPIOB,                     \
    GPIO_Pin_9, /* SDA: PB9 */ \
}

/* OLED I2C接口类型配置（默认使用软件I2C） */
#ifndef OLED_I2C_TYPE
#define OLED_I2C_TYPE OLED_I2C_TYPE_SOFTWARE
#endif

/* OLED I2C实例配置（根据接口类型选择） */
#ifndef OLED_I2C_SOFT_INSTANCE
#define OLED_I2C_SOFT_INSTANCE 0  /* SOFT_I2C_INSTANCE_1 */
#endif

#ifndef OLED_I2C_HARD_INSTANCE
#define OLED_I2C_HARD_INSTANCE 0  /* I2C_INSTANCE_1 */
#endif

/* ==================== 软件I2C配置 ==================== */

/* 软件I2C配置结构体 */
typedef struct
{
    GPIO_TypeDef *scl_port;         /**< SCL引脚端口 */
    uint16_t scl_pin;               /**< SCL引脚号 */
    GPIO_TypeDef *sda_port;         /**< SDA引脚端口 */
    uint16_t sda_pin;               /**< SDA引脚号 */
    uint32_t delay_us;              /**< I2C时序延时（微秒），建议值：5-10us（标准模式），2-5us（快速模式） */
    uint8_t enabled;                /**< 使能标志：1=启用，0=禁用 */
} SoftI2C_Config_t;

/* 软件I2C统一配置表 - 案例11配置 */
/* 注意：数组索引对应SoftI2C_Instance枚举值，SoftI2C_INSTANCE_1=0 */
#define SOFT_I2C_CONFIGS {                                                                    \
    {GPIOB, GPIO_Pin_8, GPIOB, GPIO_Pin_9, 5, 1}, /* SoftI2C1：PB8(SCL), PB9(SDA)，5us延时，启用（OLED使用） */ \
}

#endif /* BOARD_H */

