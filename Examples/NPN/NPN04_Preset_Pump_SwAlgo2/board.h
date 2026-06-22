/**
 * @file board.h
 * @brief 硬件配置头文件（NPN04_Preset_Pump_SwAlgo2 独立工程专用）
 * @details OGM EXTI 下降沿 A/B 交替互锁 PB6/PB7 + GD200A RS485
 *
 * 硬件连接：
 * - OGM A/B：PB6/PB7（EXTI6/EXTI7 双边沿，与 NPN03/NPN05 同接线）
 * - UART2 RS485：PA2(TX)/PA3(RX)，19200 8E1
 * - UART1 Debug：PA9/PA10，115200 8N1
 * - 按键：PA4 升频 / PA5 降频 / PA6 启停
 * - OLED I2C：PB8/PB9（软件 I2C）
 * - LED1：PB12
 */

#ifndef BOARD_H
#define BOARD_H

#include "stm32f10x.h"
#include <stdint.h>

#include "exti.h"

/* ==================== OGM 双通道脉冲引脚（与 NPN03 一致） ==================== */

#ifndef OGM_CH_A_PORT
#define OGM_CH_A_PORT GPIOB
#endif
#ifndef OGM_CH_A_PIN
#define OGM_CH_A_PIN GPIO_Pin_6
#endif
#ifndef OGM_CH_B_PORT
#define OGM_CH_B_PORT GPIOB
#endif
#ifndef OGM_CH_B_PIN
#define OGM_CH_B_PIN GPIO_Pin_7
#endif

#ifndef OGM_EXTI_LINE_A
#define OGM_EXTI_LINE_A EXTI_LINE_6
#endif
#ifndef OGM_EXTI_LINE_B
#define OGM_EXTI_LINE_B EXTI_LINE_7
#endif

/* ==================== 按键引脚（上拉输入，按下=低电平） ==================== */

#define BTN_FREQ_UP_PORT   GPIOA
#define BTN_FREQ_UP_PIN     GPIO_Pin_4
#define BTN_FREQ_DOWN_PORT GPIOA
#define BTN_FREQ_DOWN_PIN   GPIO_Pin_5
#define BTN_RUN_STOP_PORT  GPIOA
#define BTN_RUN_STOP_PIN   GPIO_Pin_6

/* ==================== UART 配置 ==================== */

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
    {USART1, GPIOA, GPIO_Pin_9, GPIOA, GPIO_Pin_10, 115200, USART_WordLength_8b, USART_StopBits_1, USART_Parity_No, 1},   \
    {USART2, GPIOA, GPIO_Pin_2, GPIOA, GPIO_Pin_3, 19200, USART_WordLength_9b, USART_StopBits_1, USART_Parity_Even, 1},  \
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
    {GPIOB, GPIO_Pin_12, Bit_RESET, 1}, /* LED1：PB12，低电平点亮 */       \
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
    GPIO_Pin_8,                \
    GPIOB,                     \
    GPIO_Pin_9,                \
}

#ifndef OLED_I2C_TYPE
#define OLED_I2C_TYPE OLED_I2C_TYPE_SOFTWARE
#endif

#ifndef OLED_I2C_SOFT_INSTANCE
#define OLED_I2C_SOFT_INSTANCE 0
#endif

/* ==================== 软件 I2C 配置 ==================== */

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
    {GPIOB, GPIO_Pin_8, GPIOB, GPIO_Pin_9, 5, 1},                                             \
}

/* ==================== EXTI 配置（OGM 双通道，与 NPN03 一致） ==================== */

typedef struct
{
    EXTI_Line_t line;
    GPIO_TypeDef *port;
    uint16_t pin;
    EXTI_Trigger_t trigger;
    EXTI_Mode_t mode;
    uint8_t enabled;
} EXTI_Config_t;

#define EXTI_CONFIGS {                                                                                            \
    [EXTI_LINE_6] = {EXTI_LINE_6, GPIOB, GPIO_Pin_6, EXTI_TRIGGER_RISING_FALLING, EXTI_MODE_INTERRUPT, 1},        \
    [EXTI_LINE_7] = {EXTI_LINE_7, GPIOB, GPIO_Pin_7, EXTI_TRIGGER_RISING_FALLING, EXTI_MODE_INTERRUPT, 1},        \
}

#endif /* BOARD_H */
