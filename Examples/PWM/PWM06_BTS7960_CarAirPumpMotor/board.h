/**
 * @file board.h
 * @brief 硬件配置头文件（PWM06_BTS7960_CarAirPumpMotor案例独立工程专用）
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
/* 注意：UART1使用标准配置（PA9/PA10），LPWM使用TIM3 CH4 (PB1) */
#define UART_CONFIGS {                                                                                    \
    {USART1, GPIOA, GPIO_Pin_9, GPIOA, GPIO_Pin_10, 115200, USART_WordLength_8b, USART_StopBits_1, USART_Parity_No, 1}, /* UART1：PA9(TX), PA10(RX)，标准配置，115200，8N1，启用 */ \
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

/* LED统一配置表 - 案例配置 */
#define LED_CONFIGS {                                                    \
    {GPIOA, GPIO_Pin_1, Bit_RESET, 1}, /* LED1：PA1，低电平点亮，启用 */ \
}

/* ==================== PWM配置 ==================== */

/* PWM分辨率枚举 */
typedef enum {
    PWM_RESOLUTION_8BIT = 0,   /**< 8位分辨率：256级 */
    PWM_RESOLUTION_16BIT = 1,  /**< 16位分辨率：65536级 */
    PWM_RESOLUTION_MAX         /**< 最大分辨率值 */
} PWM_Resolution_t;

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

/* PWM统一配置表 - 案例配置（只使用正转方向） */
/* 注意：数组索引必须对应PWM_Instance_t枚举：0=TIM1, 1=TIM3, 2=TIM4 */
/* 只使用正转方向：RPWM使用TIM1 CH1 (PA8) */
/* TIM3只用于编码器接口（CH1/CH2用于编码器，PB4/PB5），不用于PWM */
/* 避免与UART1(PA9/PA10)、OLED(PB8/PB9)、LED1(PA1)、按钮(PA4)、编码器(PB4/PB5)冲突 */
#define PWM_CONFIGS {                                                                                    \
    {TIM1, {{GPIOA, GPIO_Pin_8, 1}, {NULL, 0, 0}, {NULL, 0, 0}, {NULL, 0, 0}}, 1}, /* TIM1：PA8(CH1用于RPWM，只使用正转)，启用 */ \
    {TIM3, {{NULL, 0, 0}, {NULL, 0, 0}, {NULL, 0, 0}, {NULL, 0, 0}}, 0}, /* TIM3：只用于编码器接口（CH1/CH2用于编码器，PB4/PB5），不用于PWM，禁用PWM配置 */ \
    {NULL, {{NULL, 0, 0}, {NULL, 0, 0}, {NULL, 0, 0}, {NULL, 0, 0}}, 0}, /* TIM4：未使用，占位 */ \
}

/* ==================== BTS7960配置 ==================== */

/* BTS7960配置结构体 */
typedef struct
{
    /* 使能引脚（必须配置，R_EN和L_EN必须同时接高电平才能工作） */
    GPIO_TypeDef *r_en_port;         /**< R_EN引脚端口（正转使能） */
    uint16_t r_en_pin;               /**< R_EN引脚号 */
    GPIO_TypeDef *l_en_port;          /**< L_EN引脚端口（反转使能） */
    uint16_t l_en_pin;               /**< L_EN引脚号 */
    
    /* PWM引脚（必须配置） */
    uint8_t rpwm_instance;           /**< RPWM实例：0=TIM1, 1=TIM3, 2=TIM4 */
    uint8_t rpwm_channel;            /**< RPWM通道：0=CH1, 1=CH2, 2=CH3, 3=CH4 */
    uint8_t lpwm_instance;           /**< LPWM实例：0=TIM1, 1=TIM3, 2=TIM4 */
    uint8_t lpwm_channel;            /**< LPWM通道：0=CH1, 1=CH2, 2=CH3, 3=CH4 */
    
    /* 电流报警输出（可选，用于过流检测） */
    GPIO_TypeDef *r_is_port;          /**< R_IS引脚端口（正转电流报警） */
    uint16_t r_is_pin;               /**< R_IS引脚号 */
    GPIO_TypeDef *l_is_port;          /**< L_IS引脚端口（反转电流报警） */
    uint16_t l_is_pin;               /**< L_IS引脚号 */
    
    uint8_t enabled;                 /**< 使能标志：1=启用，0=禁用 */
} BTS7960_Config_t;

/* BTS7960统一配置表 - 案例配置（只使用正转方向，L_EN硬件接VCC） */
/* 注意：数组索引必须对应BTS7960_Instance_t枚举：0=BTS7960实例1, 1=BTS7960实例2 */
/* 只使用正转方向：R_EN=PA5（STM32控制），L_EN=NULL（硬件直接接VCC 5V，BTS7960要求R_EN和L_EN必须同时为高电平） */
/* RPWM=TIM1 CH1 (PA8), R_IS=PA11（可选，用于过流检测） */
/* LPWM和L_IS不需要（配置为0/NULL），代码中LPWM保持为0 */
/* 重要：L_EN引脚必须硬件连接到VCC（5V），不能悬空！否则BTS7960不会工作 */
/* 重要：R_IS引脚配置说明：
 *   - 如果R_IS未连接：配置为NULL, 0来禁用电流检测（推荐，更干净）
 *   - 如果R_IS已连接：BTS7960的R_IS是5V逻辑输出（正常=低电平0V，过流=高电平5V），STM32 GPIO是3.3V逻辑输入
 *     必须使用电平转换电路（5V转3.3V，如分压电阻或电平转换芯片），否则5V高电平会损坏STM32 GPIO！
 *   - 驱动代码已配置为下拉输入（GPIO_Mode_IPD），解决了之前浮空输入（GPIO_Mode_IN_FLOATING）导致的误报问题
 *     未连接时读取低电平(0)，不会误报；已连接时正常读取（需要电平转换）
 */
/* 引脚分配避免与UART1(PA9/PA10)、OLED(PB8/PB9)、LED1(PA1)、按钮(PA4)、编码器(PB4/PB5)冲突 */
/* 注意：rpwm_instance=0表示TIM1，rpwm_channel=0表示CH1，lpwm_instance和lpwm_channel设为0（不使用） */
#define BTS7960_CONFIGS {                                                                                    \
    {GPIOA, GPIO_Pin_5, NULL, 0, 0, 0, 0, 0, NULL, 0, NULL, 0, 1}, /* BTS7960实例1：PA5(R_EN，STM32控制), L_EN=NULL（硬件接VCC 5V）, TIM1 CH1(RPWM=PA8), R_IS=NULL（未使用，禁用电流检测）, LPWM和L_IS不使用，启用 */ \
    {NULL, 0, NULL, 0, 0, 0, 0, 0, NULL, 0, NULL, 0, 0}, /* BTS7960实例2：未使用，占位 */ \
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

/* ==================== EXTI配置 ==================== */

/* 包含EXTI头文件（用于EXTI配置中的枚举值） */
#include "exti.h"

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

/* EXTI统一配置表 - PWM06案例配置 */
/* 注意：编码器使用编码器接口模式（TIM3，PB4/PB5，部分重映射），不使用EXTI */
/* 按钮：PA4（EXTI Line 4），下降沿触发 */
#define EXTI_CONFIGS {                                                                                    \
    {EXTI_LINE_4, GPIOA, GPIO_Pin_4, EXTI_TRIGGER_FALLING, EXTI_MODE_INTERRUPT, 1}, /* EXTI4：PA4（按钮），下降沿，中断模式，启用 */ \
}

#endif /* BOARD_H */
