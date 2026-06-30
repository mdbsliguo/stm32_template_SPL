/**
 * @file board.h
 * @brief Net01 小精灵 F103ZE + W5500 硬件配置
 *
 * W5500：PB3 SCK / PB4 MISO / PB5 MOSI（SPI1 重映射）
 *        PF11 SCSn，PF9 INTn（本案例轮询），RST 上电自动复位
 * 调试：USART1 PA9/PA10；LED PC4
 */

#ifndef BOARD_H
#define BOARD_H

#include "stm32f10x.h"
#include <stdint.h>

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

#define DEBUG_UART_BAUDRATE  115200u

#define UART_CONFIGS {                                                                                    \
    {USART1, GPIOA, GPIO_Pin_9, GPIOA, GPIO_Pin_10, DEBUG_UART_BAUDRATE,                                  \
     USART_WordLength_8b, USART_StopBits_1, USART_Parity_No, 1},                                          \
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
    {GPIOC, GPIO_Pin_4, Bit_RESET, 1}, /* PC4 板载灯，低电平点亮 */         \
}

/* ==================== SPI 配置（硬件 SPI1 重映射 PB3/4/5） ==================== */

typedef struct
{
    SPI_TypeDef *spi_periph;
    GPIO_TypeDef *sck_port;
    uint16_t sck_pin;
    GPIO_TypeDef *miso_port;
    uint16_t miso_pin;
    GPIO_TypeDef *mosi_port;
    uint16_t mosi_pin;
    GPIO_TypeDef *nss_port;
    uint16_t nss_pin;
    uint16_t mode;
    uint16_t direction;
    uint16_t data_size;
    uint16_t cpol;
    uint16_t cpha;
    uint16_t nss;
    uint16_t baudrate_prescaler;
    uint16_t first_bit;
    uint8_t enabled;
} SPI_Config_t;

#define SPI_CONFIGS {                                                                                    \
    {SPI1, GPIOB, GPIO_Pin_3, GPIOB, GPIO_Pin_4, GPIOB, GPIO_Pin_5,                                     \
     NULL, 0, SPI_Mode_Master, SPI_Direction_2Lines_FullDuplex, SPI_DataSize_8b,                          \
     SPI_CPOL_Low, SPI_CPHA_1Edge, SPI_NSS_Soft, SPI_BaudRatePrescaler_2, SPI_FirstBit_MSB, 1},          \
}

/* ==================== W5500 配置 ==================== */

typedef struct
{
    uint8_t spi_instance;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    GPIO_TypeDef *rst_port;
    uint16_t rst_pin;
    GPIO_TypeDef *int_port;
    uint16_t int_pin;
    uint8_t enabled;
} W5500_Config_t;

#ifndef W5500_SPI_INSTANCE
#define W5500_SPI_INSTANCE  0
#endif

#define W5500_CONFIG {                                                                                   \
    0, GPIOF, GPIO_Pin_11, NULL, 0, GPIOF, GPIO_Pin_9, 1                                                 \
}

/* ==================== 占位配置（本案例未启用） ==================== */

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
    {I2C2, GPIOB, GPIO_Pin_10, GPIOB, GPIO_Pin_11, 50000, 0x00, 1},                 \
}

#define OLED_I2C_CONFIG {      \
    GPIOB,                     \
    GPIO_Pin_10,               \
    GPIOB,                     \
    GPIO_Pin_11,               \
}

#ifndef OLED_I2C_HARD_INSTANCE
#define OLED_I2C_HARD_INSTANCE 1   /* I2C2 */
#endif

#ifndef OLED_I2C_SOFT_INSTANCE
#define OLED_I2C_SOFT_INSTANCE 0
#endif

typedef enum {
    OLED_I2C_TYPE_SOFTWARE = 0,
    OLED_I2C_TYPE_HARDWARE = 1,
} OLED_I2C_Type_t;

#ifndef OLED_I2C_TYPE
#define OLED_I2C_TYPE OLED_I2C_TYPE_HARDWARE
#endif

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

typedef struct
{
    GPIO_TypeDef *sck_port;
    uint16_t sck_pin;
    GPIO_TypeDef *miso_port;
    uint16_t miso_pin;
    GPIO_TypeDef *mosi_port;
    uint16_t mosi_pin;
    GPIO_TypeDef *nss_port;
    uint16_t nss_pin;
    uint8_t cpol;
    uint8_t cpha;
    uint8_t first_bit;
    uint32_t delay_us;
    uint8_t enabled;
} SoftSPI_Config_t;

#define SOFT_SPI_CONFIGS {                                                                                    \
    {NULL, 0, NULL, 0, NULL, 0, NULL, 0, 0, 0, 0, 2, 0},                                                    \
}

#endif /* BOARD_H */
