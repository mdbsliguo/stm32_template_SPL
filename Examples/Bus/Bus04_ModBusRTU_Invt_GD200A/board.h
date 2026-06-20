/**
 * @file board.h
 * @brief 硬件配置头文件（Bus04_ModBusRTU_Invt_GD200A案例专用）
 * @details 此文件包含Bus04案例所需的硬件配置
 *
 * 硬件连接：
 * - UART1：PA9(TX), PA10(RX)，115200波特率（用于Debug输出）
 * - UART2：PA2(TX), PA3(RX)，19200波特率 8E1（用于ModBusRTU通信，连接RS485）
 * - OLED I2C：PB8(SCL), PB9(SDA)，软件I2C
 */

#ifndef BOARD_H
#define BOARD_H

#include "stm32f10x.h"
#include <stdint.h>

/* ==================== UART配置 ==================== */

typedef struct
{
    USART_TypeDef *uart_periph;
    GPIO_TypeDef *tx_port;
    uint16_t tx_pin;
    GPIO_TypeDef *rx_port;
    uint16_t rx_pin;
    uint32_t baudrate;
    uint16_t word_length;
    uint16_t stop_bits;
    uint16_t parity;
    uint8_t enabled;
} UART_Config_t;

#define UART_CONFIGS {                                                                                    \
    {USART1, GPIOA, GPIO_Pin_9, GPIOA, GPIO_Pin_10, 115200, USART_WordLength_8b, USART_StopBits_1, USART_Parity_No, 1},   /* UART1：PA9/PA10，115200，8N1，Debug */ \
    {USART2, GPIOA, GPIO_Pin_2, GPIOA, GPIO_Pin_3, 19200, USART_WordLength_9b, USART_StopBits_1, USART_Parity_Even, 1},  /* UART2：19200 8E1（P14.01=4 P14.02=1） */ \
}

/* ==================== 其他模块配置（占位，避免编译错误） ==================== */

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t active_level;
    uint8_t enabled;
} LED_Config_t;

#define LED_CONFIGS { \
    {NULL, 0, 0, 0}, \
}

/* ==================== OLED配置 ==================== */

typedef enum {
    OLED_I2C_TYPE_SOFTWARE = 0,
    OLED_I2C_TYPE_HARDWARE = 1,
} OLED_I2C_Type_t;

#ifndef OLED_I2C_TYPE
#define OLED_I2C_TYPE OLED_I2C_TYPE_SOFTWARE
#endif

#ifndef OLED_I2C_SOFT_INSTANCE
#define OLED_I2C_SOFT_INSTANCE 0
#endif

/* ==================== 软件I2C配置 ==================== */

typedef struct {
    GPIO_TypeDef *scl_port;
    uint16_t scl_pin;
    GPIO_TypeDef *sda_port;
    uint16_t sda_pin;
    uint32_t delay_us;
    uint8_t enabled;
} SoftI2C_Config_t;

#define SOFT_I2C_CONFIGS { \
    {GPIOB, GPIO_Pin_8, GPIOB, GPIO_Pin_9, 5, 1}, \
}

#endif /* BOARD_H */
