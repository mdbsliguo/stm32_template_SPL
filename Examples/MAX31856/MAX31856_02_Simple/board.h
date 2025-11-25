/**
 * @file board.h
 * @brief 硬件配置 - 最简单的MAX31856案例
 */

#ifndef BOARD_H
#define BOARD_H

#include "stm32f10x.h"

/* ==================== SPI配置 ==================== */

/* SPI配置结构体 */
typedef struct
{
    SPI_TypeDef *spi_periph;        /**< SPI外设指针 */
    GPIO_TypeDef *sck_port;         /**< SCK引脚端口 */
    uint16_t sck_pin;               /**< SCK引脚号 */
    GPIO_TypeDef *miso_port;        /**< MISO引脚端口 */
    uint16_t miso_pin;              /**< MISO引脚号 */
    GPIO_TypeDef *mosi_port;       /**< MOSI引脚端口 */
    uint16_t mosi_pin;              /**< MOSI引脚号 */
    GPIO_TypeDef *nss_port;         /**< NSS引脚端口（可为NULL） */
    uint16_t nss_pin;               /**< NSS引脚号（可为0） */
    uint16_t mode;                  /**< SPI模式：SPI_Mode_Master或SPI_Mode_Slave */
    uint16_t direction;             /**< SPI方向：SPI_Direction_2Lines_FullDuplex等 */
    uint16_t data_size;             /**< 数据大小：SPI_DataSize_8b或SPI_DataSize_16b */
    uint16_t cpol;                  /**< 时钟极性：SPI_CPOL_Low或SPI_CPOL_High */
    uint16_t cpha;                  /**< 时钟相位：SPI_CPHA_1Edge或SPI_CPHA_2Edge */
    uint16_t nss;                   /**< NSS管理：SPI_NSS_Soft或SPI_NSS_Hard */
    uint16_t baudrate_prescaler;    /**< 波特率预分频：SPI_BaudRatePrescaler_2等 */
    uint16_t first_bit;             /**< 首字节：SPI_FirstBit_MSB或SPI_FirstBit_LSB */
    uint8_t enabled;                /**< 使能标志：1=启用，0=禁用 */
} SPI_Config_t;

/* SPI统一配置表 - 最简单配置（SPI2，PB13/14/15，软件NSS） */
#define SPI_CONFIGS {                                                                                    \
    {NULL, NULL, 0, NULL, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* SPI1：未使用 */  \
    {SPI2, GPIOB, GPIO_Pin_13, GPIOB, GPIO_Pin_14, GPIOB, GPIO_Pin_15, NULL, 0,                         \
     SPI_Mode_Master, SPI_Direction_2Lines_FullDuplex, SPI_DataSize_8b,                                \
     SPI_CPOL_High, SPI_CPHA_2Edge, SPI_NSS_Soft, SPI_BaudRatePrescaler_32, SPI_FirstBit_MSB, 1},       \
    /* SPI2：PB13(SCK), PB14(MISO), PB15(MOSI)，主模式，全双工，8位，模式3(CPOL=High, CPHA=2Edge)，软件NSS，预分频32(2.25MHz，降低速度提高稳定性)，MSB，启用 */ \
    {NULL, NULL, 0, NULL, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* SPI3：未使用 */  \
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

/* OLED I2C配置 - 软件I2C，PB8/9 */
#define OLED_I2C_CONFIG {      \
    GPIOB,                     \
    GPIO_Pin_8, /* SCL: PB8 */ \
    GPIOB,                     \
    GPIO_Pin_9, /* SDA: PB9 */ \
}

/* OLED I2C接口类型配置 */
#ifndef OLED_I2C_TYPE
#define OLED_I2C_TYPE OLED_I2C_TYPE_SOFTWARE
#endif

/* OLED I2C实例配置 */
#ifndef OLED_I2C_SOFT_INSTANCE
#define OLED_I2C_SOFT_INSTANCE 0
#endif

#ifndef OLED_I2C_HARD_INSTANCE
#define OLED_I2C_HARD_INSTANCE 0
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

/* ==================== LED配置 ==================== */

/* LED配置结构体 */
typedef struct
{
    GPIO_TypeDef *port;   /**< GPIO端口基地址 */
    uint16_t pin;         /**< 引脚号 */
    uint8_t active_level; /**< 有效电平（Bit_SET或Bit_RESET） */
    uint8_t enabled;      /**< 使能标志：1=启用，0=禁用 */
} LED_Config_t;

/* LED统一配置表 */
#define LED_CONFIGS {                                                            \
    {GPIOA, GPIO_Pin_1, Bit_RESET, 1}, /* LED1：PA1，低电平点亮，启用（系统状态指示） */ \
}

#endif /* BOARD_H */

