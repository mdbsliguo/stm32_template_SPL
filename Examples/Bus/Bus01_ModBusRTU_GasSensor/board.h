/**
 * @file board.h
 * @brief 硬件配置头文件（Bus01_ModBusRTU_GasSensor案例专用）
 * @details 此文件包含Bus01案例所需的硬件配置
 * 
 * 注意：这是独立工程的board.h，包含案例所需的硬件配置
 * 
 * 硬件连接：
 * - UART1：PA9(TX), PA10(RX)，115200波特率（用于Debug输出）
 * - UART2：PA2(TX), PA3(RX)，9600波特率（用于ModBusRTU通信，连接RS485）
 * - OLED I2C：PB8(SCL), PB9(SDA)，软件I2C
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

/* UART统一配置表 - Bus01案例配置 */
#define UART_CONFIGS {                                                                                    \
    {USART1, GPIOA, GPIO_Pin_9, GPIOA, GPIO_Pin_10, 115200, USART_WordLength_8b, USART_StopBits_1, USART_Parity_No, 1}, /* UART1：PA9(TX), PA10(RX)，115200，8N1，启用（Debug输出） */ \
    {USART2, GPIOA, GPIO_Pin_2, GPIOA, GPIO_Pin_3, 9600, USART_WordLength_8b, USART_StopBits_1, USART_Parity_No, 1}, /* UART2：PA2(TX), PA3(RX)，9600，8N1，启用（ModBusRTU通信） */ \
}

/* ==================== 其他模块配置（占位，避免编译错误） ==================== */

/* LED配置结构体（案例不使用LED，但需要定义以避免编译错误） */
typedef struct
{
    GPIO_TypeDef *port;   /**< GPIO端口基地址 */
    uint16_t pin;         /**< 引脚号 */
    uint8_t active_level; /**< 有效电平（Bit_SET或Bit_RESET） */
    uint8_t enabled;      /**< 使能标志：1=启用，0=禁用 */
} LED_Config_t;

/* LED统一配置表 - 空配置（案例不使用LED，但需要至少一个元素避免编译错误） */
#define LED_CONFIGS { \
    {NULL, 0, 0, 0}, /* 占位元素，禁用 */ \
}

/* ==================== OLED配置 ==================== */

/* OLED I2C接口类型 */
typedef enum {
    OLED_I2C_TYPE_SOFTWARE = 0,  /**< 软件I2C接口 */
    OLED_I2C_TYPE_HARDWARE = 1,  /**< 硬件I2C接口 */
} OLED_I2C_Type_t;

/* OLED I2C接口类型配置（使用软件I2C） */
#ifndef OLED_I2C_TYPE
#define OLED_I2C_TYPE OLED_I2C_TYPE_SOFTWARE
#endif

/* OLED I2C实例配置 */
#ifndef OLED_I2C_SOFT_INSTANCE
#define OLED_I2C_SOFT_INSTANCE 0  /* SOFT_I2C_INSTANCE_1 */
#endif

/* ==================== 软件I2C配置 ==================== */

/* 软件I2C配置结构体 */
typedef struct {
    GPIO_TypeDef *scl_port;  /**< SCL引脚端口 */
    uint16_t scl_pin;        /**< SCL引脚号 */
    GPIO_TypeDef *sda_port;  /**< SDA引脚端口 */
    uint16_t sda_pin;        /**< SDA引脚号 */
    uint32_t delay_us;       /**< 延时（微秒） */
    uint8_t enabled;         /**< 使能标志：1=启用，0=禁用 */
} SoftI2C_Config_t;

/* 软件I2C统一配置表 - OLED使用PB8(SCL)/PB9(SDA) */
#define SOFT_I2C_CONFIGS { \
    {GPIOB, GPIO_Pin_8, GPIOB, GPIO_Pin_9, 5, 1}, /* SoftI2C1：PB8(SCL), PB9(SDA)，OLED使用 */ \
}

#endif /* BOARD_H */





