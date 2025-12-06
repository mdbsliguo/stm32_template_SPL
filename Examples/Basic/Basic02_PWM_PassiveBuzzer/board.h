/**
 * @file board.h
 * @brief 硬件配置头文件（Basic02_PWMPassiveBuzzer案例独立工程专用）
 * @details 此文件包含案例所需的硬件配置
 * 
 * 注意：这是独立工程的board.h，包含案例所需的硬件配置
 */

#ifndef BOARD_H
#define BOARD_H

#include "stm32f10x.h"
#include <stdint.h>

/* ==================== UART配置 ==================== */

/* UART配置结构体 */
typedef struct
{
    USART_TypeDef *uart_periph;  /**< UART外设（USART1/2/3） */
    GPIO_TypeDef *tx_port;       /**< TX引脚端口 */
    uint16_t tx_pin;             /**< TX引脚号 */
    GPIO_TypeDef *rx_port;       /**< RX引脚端口 */
    uint16_t rx_pin;             /**< RX引脚号 */
    uint32_t baudrate;           /**< 波特率（如115200、9600等） */
    uint16_t word_length;         /**< 数据位：USART_WordLength_8b或USART_WordLength_9b */
    uint16_t stop_bits;          /**< 停止位：USART_StopBits_1或USART_StopBits_2 */
    uint16_t parity;             /**< 校验位：USART_Parity_No、USART_Parity_Even或USART_Parity_Odd */
    uint8_t enabled;             /**< 使能标志：1=启用，0=禁用 */
} UART_Config_t;

/* UART统一配置表 - 标准配置：USART1，PA9/PA10，115200，8N1 */
#define UART_CONFIGS {                                                                                    \
    {USART1, GPIOA, GPIO_Pin_9, GPIOA, GPIO_Pin_10, 115200, USART_WordLength_8b, USART_StopBits_1, USART_Parity_No, 1}, /* UART1：PA9(TX), PA10(RX)，115200，8N1，启用 */ \
}

/* ==================== Buzzer配置 ==================== */

/* Buzzer驱动模式枚举 */
typedef enum {
    BUZZER_MODE_GPIO = 0,  /**< GPIO模式：简单开关，无频率控制，适用于有源蜂鸣器 */
    BUZZER_MODE_PWM = 1    /**< PWM模式：通过PWM控制频率，适用于无源蜂鸣器 */
} Buzzer_Mode_t;

/* Buzzer配置结构体 */
typedef struct
{
    Buzzer_Mode_t mode;        /**< 驱动模式（GPIO/PWM） */
    GPIO_TypeDef *port;        /**< GPIO端口（GPIO模式必需，PWM模式可为NULL） */
    uint16_t pin;              /**< GPIO引脚（GPIO模式必需，PWM模式可为0） */
    uint8_t pwm_instance;      /**< PWM实例（PWM模式必需，0=TIM1, 1=TIM3, 2=TIM4） */
    uint8_t pwm_channel;       /**< PWM通道（PWM模式必需，0=CH1, 1=CH2, 2=CH3, 3=CH4） */
    uint8_t active_level;      /**< 有效电平（Bit_SET或Bit_RESET） */
    uint8_t enabled;           /**< 使能标志：1=启用，0=禁用 */
} Buzzer_Config_t;

/* Buzzer统一配置表 - 案例配置（PWM模式，无源蜂鸣器） */
#define BUZZER_CONFIGS {                                                                                    \
    {BUZZER_MODE_PWM, NULL, 0, 1, 0, Bit_RESET, 1}, /* Buzzer1：PWM模式（无源蜂鸣器），TIM3(实例1)通道1，启用（GPIO端口和引脚在PWM模式下忽略） */ \
}

/* ==================== PWM配置 ==================== */

/* PWM通道配置结构体 */
typedef struct
{
    GPIO_TypeDef *port;      /**< GPIO端口 */
    uint16_t pin;            /**< 引脚号 */
    uint8_t enabled;         /**< 使能标志：1=启用，0=禁用 */
} PWM_Channel_Config_t;

/* PWM配置结构体 */
typedef struct
{
    TIM_TypeDef *tim_periph;  /**< 定时器外设（TIM1/TIM3/TIM4） */
    PWM_Channel_Config_t channels[4];  /**< 通道配置（通道1-4） */
    uint8_t enabled;         /**< 使能标志：1=启用，0=禁用 */
} PWM_Config_t;

/* PWM统一配置表 - 案例配置（TIM3，PA6，通道1） */
/* 注意：数组索引必须对应PWM_Instance_t枚举：0=TIM1, 1=TIM3, 2=TIM4 */
#define PWM_CONFIGS {                                                                                    \
    {NULL, {{NULL, 0, 0}, {NULL, 0, 0}, {NULL, 0, 0}, {NULL, 0, 0}}, 0}, /* TIM1：未使用，占位 */ \
    {TIM3, {{GPIOA, GPIO_Pin_6, 1}, {GPIOA, GPIO_Pin_0, 0}, {GPIOA, GPIO_Pin_0, 0}, {GPIOA, GPIO_Pin_0, 0}}, 1}, /* TIM3：PA6(CH1)，启用 */ \
    {NULL, {{NULL, 0, 0}, {NULL, 0, 0}, {NULL, 0, 0}, {NULL, 0, 0}}, 0}, /* TIM4：未使用，占位 */ \
}

/* ==================== OLED配置 ==================== */

/* OLED I2C接口类型 */
typedef enum {
    OLED_I2C_TYPE_SOFTWARE = 0,  /**< 软件I2C接口 */
    OLED_I2C_TYPE_HARDWARE = 1,  /**< 硬件I2C接口 */
} OLED_I2C_Type_t;

/* OLED I2C配置结构体 */
typedef struct
{
    GPIO_TypeDef *scl_port; /**< SCL引脚端口 */
    uint16_t scl_pin;       /**< SCL引脚号 */
    GPIO_TypeDef *sda_port; /**< SDA引脚端口 */
    uint16_t sda_pin;       /**< SDA引脚号 */
} OLED_I2C_Config_t;

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

/* OLED I2C配置 */
#define OLED_I2C_CONFIG {      \
    GPIOB,                     \
    GPIO_Pin_8, /* SCL: PB8 */ \
    GPIOB,                     \
    GPIO_Pin_9, /* SDA: PB9 */ \
}

/* ==================== 软件I2C配置（OLED使用） ==================== */

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

/* 软件I2C统一配置表 */
#define SOFT_I2C_CONFIGS {                                                                    \
    {GPIOB, GPIO_Pin_8, GPIOB, GPIO_Pin_9, 5, 1}, /* SoftI2C1：PB8(SCL), PB9(SDA)，5us延时，启用（OLED使用） */ \
}


#endif /* BOARD_H */
