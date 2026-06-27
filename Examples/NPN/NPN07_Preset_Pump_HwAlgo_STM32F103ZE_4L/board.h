/**
 * @file board.h
 * @brief 硬件配置（NPN07 小精灵 F103ZE 4L 预设加油泵）
 * @details OGM TIM3 PA6/PA7 + GD200A RS485 + 硬件 I2C2 OLED；145 脉冲/升
 *
 * 硬件连接（均为板子引出脚）：
 * - OGM A/B：PA6/PA7（编码器②，TIM3 CH1/CH2）
 * - 485② 变频器：PA2/PA3（USART2，19200 8E1）
 * - 485① 扩展：PC10/PC11（USART3 PartialRemap）
 * - TTL 调试：PA9/PA10（USART1，115200 8N1）
 * - OLED：PB10/PB11（硬件 I2C2）
 * - 启动键：PA4（15 路 IO，上拉，按下低）
 * - LED：PC4（板载灯）
 * - 继电器：PD3（8 路隔离输出之一）
 * - 网口 W5500：PB3/4/5、PF9、PF11（本期固件未用，须先 JTAG 释放）
 */

#ifndef BOARD_H
#define BOARD_H

#include "stm32f10x.h"
#include <stdint.h>

/* ==================== OGM 输入捕获（TIM3 PA6/PA7） ==================== */

#ifndef OGM_FLOW_IC_INSTANCE
#define OGM_FLOW_IC_INSTANCE    1   /* OGM_FLOW_IC_TIM3 */
#endif

#ifndef OGM_CH_A_PORT
#define OGM_CH_A_PORT GPIOA
#endif
#ifndef OGM_CH_A_PIN
#define OGM_CH_A_PIN GPIO_Pin_6
#endif
#ifndef OGM_CH_B_PORT
#define OGM_CH_B_PORT GPIOA
#endif
#ifndef OGM_CH_B_PIN
#define OGM_CH_B_PIN GPIO_Pin_7
#endif

#ifndef OGM_FLOW_IC_FILTER
#define OGM_FLOW_IC_FILTER      0x04u
#endif

#ifndef OGM_PULSE_FACTOR
#define OGM_PULSE_FACTOR        (1.0f / 145.0f)
#endif

/* ==================== 按键（上拉输入，按下=低电平；逻辑同 NPN05） ==================== */

#define BTN_FREQ_UP_PORT   GPIOA
#define BTN_FREQ_UP_PIN    GPIO_Pin_5   /* 小精灵未引出，仅占位 */
#define BTN_FREQ_DOWN_PORT GPIOA
#define BTN_FREQ_DOWN_PIN  GPIO_Pin_5   /* 小精灵未引出，仅占位 */
#define BTN_RUN_STOP_PORT  GPIOA
#define BTN_RUN_STOP_PIN   GPIO_Pin_4   /* 15 路 IO 启动键 */

/* ==================== 继电器（隔离输出，默认安全态） ==================== */

#define RELAY1_PORT        GPIOD
#define RELAY1_PIN         GPIO_Pin_3
/** 继电器断开/安全态电平（现场极性不同时改此处） */
#define RELAY1_SAFE_LEVEL  Bit_RESET

/* ==================== UART 配置 ==================== */

/** USART3 使用 PC10/PC11 时须在 UART_Init 前 PartialRemap */
#define UART3_USE_PC10_PC11_REMAP  1

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

/** TTL 调试串口波特率（USART1 PA9/PA10） */
#define DEBUG_UART_BAUDRATE     115200u

#define UART_CONFIGS {                                                                                    \
    {USART1, GPIOA, GPIO_Pin_9,  GPIOA, GPIO_Pin_10, DEBUG_UART_BAUDRATE, USART_WordLength_8b, USART_StopBits_1, USART_Parity_No, 1},   \
    {USART2, GPIOA, GPIO_Pin_2,  GPIOA, GPIO_Pin_3,  19200,  USART_WordLength_9b, USART_StopBits_1, USART_Parity_Even, 1}, \
    {USART3, GPIOC, GPIO_Pin_10, GPIOC, GPIO_Pin_11, 9600,   USART_WordLength_8b, USART_StopBits_1, USART_Parity_No, 1},   \
}

/* ==================== LED 配置 ==================== */

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t active_level;
    uint8_t enabled;
} LED_Config_t;

#define LED_CONFIGS {                                                      \
    {GPIOC, GPIO_Pin_4, Bit_RESET, 1}, /* LED1：PC4 板载灯，低电平点亮 */   \
}

/* ==================== I2C 配置（OLED 硬件 I2C2） ==================== */

typedef struct
{
    I2C_TypeDef *i2c_periph;
    GPIO_TypeDef *scl_port;
    uint16_t scl_pin;
    GPIO_TypeDef *sda_port;
    uint16_t sda_pin;
    uint32_t clock_speed;
    uint16_t own_address;
    uint8_t enabled;
} I2C_Config_t;

#define I2C_CONFIGS {                                                                    \
    {NULL, NULL, 0, NULL, 0, 0, 0, 0},                                                 \
    {I2C2, GPIOB, GPIO_Pin_10, GPIOB, GPIO_Pin_11, 100000, 0x00, 1},                 \
}

/* ==================== OLED 配置 ==================== */

typedef enum {
    OLED_I2C_TYPE_SOFTWARE = 0,
    OLED_I2C_TYPE_HARDWARE = 1,
} OLED_I2C_Type_t;

typedef struct
{
    GPIO_TypeDef *scl_port;
    uint16_t scl_pin;
    GPIO_TypeDef *sda_port;
    uint16_t sda_pin;
} OLED_I2C_Config_t;

#define OLED_I2C_CONFIG {      \
    GPIOB,                     \
    GPIO_Pin_10,               \
    GPIOB,                     \
    GPIO_Pin_11,               \
}

#ifndef OLED_I2C_TYPE
#define OLED_I2C_TYPE OLED_I2C_TYPE_HARDWARE
#endif

#ifndef OLED_I2C_HARD_INSTANCE
#define OLED_I2C_HARD_INSTANCE 1   /* I2C2 */
#endif

#ifndef OLED_I2C_SOFT_INSTANCE
#define OLED_I2C_SOFT_INSTANCE 0
#endif

/* ==================== 软件 I2C 占位（本案例未启用） ==================== */

typedef struct
{
    GPIO_TypeDef *scl_port;
    uint16_t scl_pin;
    GPIO_TypeDef *sda_port;
    uint16_t sda_pin;
    uint32_t delay_us;
    uint8_t enabled;
} SoftI2C_Config_t;

#define SOFT_I2C_CONFIGS {                                                                    \
    {NULL, 0, NULL, 0, 5, 0},                                                                 \
}

#endif /* BOARD_H */
