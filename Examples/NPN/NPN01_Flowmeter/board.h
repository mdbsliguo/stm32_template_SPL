/**
 * @file board.h
 * @brief 硬件配置头文件（NPN01_Flowmeter 独立工程专用）
 * @details 包含本案例所需的硬件配置（流量计 NPN 脉冲输入、LED、可选 OLED/I2C）
 */

#ifndef BOARD_H
#define BOARD_H

#include "stm32f10x.h"
#include <stdint.h>

/* 包含 EXTI 头文件（用于 EXTI 枚举值） */
#include "exti.h"

/* ==================== LED 配置 ==================== */

typedef struct
{
    GPIO_TypeDef *port;   /**< GPIO 端口 */
    uint16_t pin;         /**< 引脚号 */
    uint8_t active_level; /**< 有效电平：Bit_SET 或 Bit_RESET */
    uint8_t enabled;      /**< 使能标志：1=启用，0=禁用 */
} LED_Config_t;

/* LED 配置表 - PA1 作为状态指示灯 */
#define LED_CONFIGS {                                                    \
    {GPIOA, GPIO_Pin_1, Bit_RESET, 1}, /* LED1：PA1，低电平点亮，启用 */ \
}

/* ==================== OLED 配置（可选） ==================== */

typedef enum {
    OLED_I2C_TYPE_SOFTWARE = 0,  /**< 软件 I2C 接口 */
    OLED_I2C_TYPE_HARDWARE = 1,  /**< 硬件 I2C 接口 */
} OLED_I2C_Type_t;

typedef struct
{
    GPIO_TypeDef *scl_port;
    uint16_t scl_pin;
    GPIO_TypeDef *sda_port;
    uint16_t sda_pin;
} OLED_I2C_Config_t;

/* OLED I2C 引脚：PB8/PB9（软件 I2C） */
#define OLED_I2C_CONFIG {      \
    GPIOB,                     \
    GPIO_Pin_8, /* SCL: PB8 */ \
    GPIOB,                     \
    GPIO_Pin_9, /* SDA: PB9 */ \
}

#ifndef OLED_I2C_TYPE
#define OLED_I2C_TYPE OLED_I2C_TYPE_SOFTWARE
#endif

#ifndef OLED_I2C_SOFT_INSTANCE
#define OLED_I2C_SOFT_INSTANCE 0  /* SOFT_I2C_INSTANCE_1 */
#endif

/* ==================== 软件 I2C 配置 ==================== */

typedef struct
{
    GPIO_TypeDef *scl_port;
    uint16_t scl_pin;
    GPIO_TypeDef *sda_port;
    uint16_t sda_pin;
    uint32_t delay_us;  /**< 时序延时（微秒） */
    uint8_t enabled;    /**< 1=启用，0=禁用 */
} SoftI2C_Config_t;

/* 软件 I2C 配置表 */
#define SOFT_I2C_CONFIGS {                                                                    \
    {GPIOB, GPIO_Pin_8, GPIOB, GPIO_Pin_9, 5, 1}, /* SoftI2C1：PB8(SCL), PB9(SDA)，5us 延时，启用（OLED） */ \
}

/* ==================== EXTI 配置（流量计脉冲输入） ==================== */

typedef struct
{
    EXTI_Line_t line;       /**< EXTI 线号 0-19 */
    GPIO_TypeDef *port;     /**< GPIO 端口（Line 0-15 需要） */
    uint16_t pin;           /**< GPIO 引脚号（Line 0-15 需要） */
    EXTI_Trigger_t trigger; /**< 触发模式：上升/下降/双沿 */
    EXTI_Mode_t mode;       /**< 模式：中断/事件 */
    uint8_t enabled;        /**< 使能标志：1=启用，0=禁用 */
} EXTI_Config_t;

/* EXTI 配置表 - PA0 下降沿触发（NPN 开漏拉低输出） */
/* 若传感器输出双沿波形，可改为 EXTI_TRIGGER_RISING_FALLING */
#define EXTI_CONFIGS {                                                                                  \
    {EXTI_LINE_0, GPIOA, GPIO_Pin_0, EXTI_TRIGGER_FALLING, EXTI_MODE_INTERRUPT, 1}, /* EXTI0：PA0，下降沿，中断模式，启用 */ \
}

#endif /* BOARD_H */
