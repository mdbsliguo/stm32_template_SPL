/**
 * @file board.h
 * @brief 硬件配置头文件（EXTI02_RotaryEncoder_Counter案例独立工程专用）
 * @details 此文件包含案例所需的硬件配置
 * 
 * 注意：这是独立工程的board.h，包含案例所需的硬件配置
 */

#ifndef BOARD_H
#define BOARD_H

#include "stm32f10x.h"
#include <stdint.h>

/* 包含EXTI头文件（用于EXTI配置中的枚举值） */
#include "exti.h"

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

/* UART统一配置表 - EXTI02案例配置（USART1，PA9/10，115200，8N1） */
#define UART_CONFIGS {                                                                                    \
    {USART1, GPIOA, GPIO_Pin_9, GPIOA, GPIO_Pin_10, 115200, USART_WordLength_8b, USART_StopBits_1, USART_Parity_No, 1}, /* UART1：PA9(TX), PA10(RX)，115200，8N1，启用 */ \
}

/* ==================== LED配置 ==================== */

/* LED配置结构体 */
typedef struct
{
    GPIO_TypeDef *port;   /**< GPIO端口基地址 */
    uint16_t pin;         /**< 引脚号 */
    uint8_t active_level; /**< 有效电平（Bit_SET或Bit_RESET） */
    uint8_t enabled;      /**< 使能标志：1=启用，0=禁用 */
} LED_Config_t;

/* LED统一配置表 - EXTI02案例配置 */
#define LED_CONFIGS {                                                    \
    {GPIOA, GPIO_Pin_1, Bit_RESET, 1}, /* LED1：PA1，低电平点亮，启用（状态指示） */ \
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

/* OLED I2C配置 - EXTI02案例配置 */
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

/* OLED I2C实例配置 */
#ifndef OLED_I2C_SOFT_INSTANCE
#define OLED_I2C_SOFT_INSTANCE 0  /* SOFT_I2C_INSTANCE_1 */
#endif

/* ==================== 软件I2C配置 ==================== */

/* 软件I2C配置结构体 */
typedef struct
{
    GPIO_TypeDef *scl_port;         /**< SCL引脚端口 */
    uint16_t scl_pin;               /**< SCL引脚号 */
    GPIO_TypeDef *sda_port;         /**< SDA引脚端口 */
    uint16_t sda_pin;               /**< SDA引脚号 */
    uint32_t delay_us;              /**< I2C时序延时（微秒） */
    uint8_t enabled;                /**< 使能标志：1=启用，0=禁用 */
} SoftI2C_Config_t;

/* 软件I2C统一配置表 */
#define SOFT_I2C_CONFIGS {                                                                    \
    {GPIOB, GPIO_Pin_8, GPIOB, GPIO_Pin_9, 5, 1}, /* SoftI2C1：PB8(SCL), PB9(SDA)，5us延时，启用（OLED使用） */ \
}

/* ==================== EXTI配置 ==================== */

/* EXTI配置结构体 */
typedef struct
{
    EXTI_Line_t line;           /**< EXTI线号（0-19） */
    GPIO_TypeDef *port;         /**< GPIO端口（Line 0-15需要） */
    uint16_t pin;               /**< GPIO引脚号（Line 0-15需要） */
    EXTI_Trigger_t trigger;     /**< 触发模式：上升沿/下降沿/双边沿 */
    EXTI_Mode_t mode;           /**< 模式：中断/事件 */
    uint8_t enabled;            /**< 使能标志：1=启用，0=禁用 */
} EXTI_Config_t;

/* EXTI统一配置表 - EXTI02案例配置（PB0和PB1，双边沿触发） */
/* 编码器通道A：PB0（EXTI Line 0） */
/* 编码器通道B：PB1（EXTI Line 1） */
/* 使用双边沿触发，检测所有边沿变化，通过触发先后顺序判断旋转方向 */
#define EXTI_CONFIGS {                                                                                    \
    {EXTI_LINE_0, GPIOB, GPIO_Pin_0, EXTI_TRIGGER_RISING_FALLING, EXTI_MODE_INTERRUPT, 1}, /* EXTI0：PB0（编码器通道A），双边沿，中断模式，启用 */ \
    {EXTI_LINE_1, GPIOB, GPIO_Pin_1, EXTI_TRIGGER_RISING_FALLING, EXTI_MODE_INTERRUPT, 1}, /* EXTI1：PB1（编码器通道B），双边沿，中断模式，启用 */ \
}

#endif /* BOARD_H */
