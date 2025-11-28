/**
 * @file board.h
 * @brief 硬件配置头文件（所有硬件相关配置的唯一真理源）
 * @details 此文件包含所有模块的硬件配置
 * 
 * 注意：由于项目中包含所有模块的源文件，即使不使用某些模块，
 * 也需要提供相应的配置，否则编译会失败。
 */

#ifndef BOARD_H
#define BOARD_H

#include "stm32f10x.h"
#include <stdint.h>

/* 前向声明（避免循环依赖） */
typedef enum EXTI_Line_t EXTI_Line_t;
typedef enum EXTI_Trigger_t EXTI_Trigger_t;
typedef enum EXTI_Mode_t EXTI_Mode_t;

/* ==================== LED配置 ==================== */

/* LED配置结构体 */
typedef struct
{
    GPIO_TypeDef *port;   /**< GPIO端口基地址 */
    uint16_t pin;         /**< 引脚号 */
    uint8_t active_level; /**< 有效电平（Bit_SET或Bit_RESET） */
    uint8_t enabled;      /**< 使能标志：1=启用，0=禁用 */
} LED_Config_t;

/* LED统一配置表 - 案例10配置（只需要LED1） */
#define LED_CONFIGS {                                                            \
    {GPIOA, GPIO_Pin_1, Bit_RESET, 1}, /* LED1：PA1，低电平点亮，启用（系统状态指示） */ \
}

/* ==================== OLED配置 ==================== */

/* OLED I2C接口类型 */
typedef enum {
    OLED_I2C_TYPE_SOFTWARE = 0,  /**< 软件I2C接口 */
    OLED_I2C_TYPE_HARDWARE = 1,  /**< 硬件I2C接口 */
} OLED_I2C_Type_t;

/* OLED I2C配置结构体（保留用于向后兼容） */
typedef struct
{
    GPIO_TypeDef *scl_port; /**< SCL引脚端口 */
    uint16_t scl_pin;       /**< SCL引脚号 */
    GPIO_TypeDef *sda_port; /**< SDA引脚端口 */
    uint16_t sda_pin;       /**< SDA引脚号 */
} OLED_I2C_Config_t;

/* OLED I2C配置 - 案例10配置 */
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

/* OLED I2C实例配置（根据接口类型选择） */
#ifndef OLED_I2C_SOFT_INSTANCE
#define OLED_I2C_SOFT_INSTANCE 0  /* SOFT_I2C_INSTANCE_1 */
#endif

#ifndef OLED_I2C_HARD_INSTANCE
#define OLED_I2C_HARD_INSTANCE 0  /* I2C_INSTANCE_1 */
#endif

/* ==================== I2C配置 ==================== */

/* I2C配置结构体 */
typedef struct
{
    I2C_TypeDef *i2c_periph;        /**< I2C外设（I2C1或I2C2） */
    GPIO_TypeDef *scl_port;         /**< SCL引脚端口 */
    uint16_t scl_pin;               /**< SCL引脚号 */
    GPIO_TypeDef *sda_port;         /**< SDA引脚端口 */
    uint16_t sda_pin;               /**< SDA引脚号 */
    uint32_t clock_speed;           /**< I2C时钟速度（Hz），标准模式<=100kHz，快速模式<=400kHz */
    uint16_t own_address;           /**< 本设备地址（7位地址，用于从模式） */
    uint8_t enabled;                /**< 使能标志：1=启用，0=禁用 */
} I2C_Config_t;

/* I2C统一配置表 - 默认配置（I2C1，PB6/7） */
#define I2C_CONFIGS {                                                                    \
    {I2C1, GPIOB, GPIO_Pin_6, GPIOB, GPIO_Pin_7, 100000, 0x00, 1}, /* I2C1：PB6(SCL), PB7(SDA)，100kHz，从地址0x00，启用 */ \
}

/* ==================== 软件I2C配置 ==================== */

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
    {GPIOB, GPIO_Pin_10, GPIOB, GPIO_Pin_11, 5, 1}, /* SoftI2C2：PB10(SCL), PB11(SDA)，5us延时，启用（DS3231使用） */ \
}

/* ==================== 软件SPI配置 ==================== */

/* 软件SPI配置结构体 */
typedef struct
{
    GPIO_TypeDef *sck_port;         /**< SCK引脚端口 */
    uint16_t sck_pin;               /**< SCK引脚号 */
    GPIO_TypeDef *miso_port;        /**< MISO引脚端口 */
    uint16_t miso_pin;              /**< MISO引脚号 */
    GPIO_TypeDef *mosi_port;        /**< MOSI引脚端口 */
    uint16_t mosi_pin;              /**< MOSI引脚号 */
    GPIO_TypeDef *nss_port;         /**< NSS引脚端口（可为NULL） */
    uint16_t nss_pin;               /**< NSS引脚号（可为0） */
    uint8_t cpol;                   /**< 时钟极性：0=CPOL_Low，1=CPOL_High */
    uint8_t cpha;                   /**< 时钟相位：0=CPHA_1Edge，1=CPHA_2Edge */
    uint8_t first_bit;              /**< 首字节：0=MSB，1=LSB */
    uint32_t delay_us;              /**< SPI时序延时（微秒），建议值：1-5us */
    uint8_t enabled;                /**< 使能标志：1=启用，0=禁用 */
} SoftSPI_Config_t;

/* 软件SPI统一配置表 */
#define SOFT_SPI_CONFIGS {                                                                                    \
    {GPIOA, GPIO_Pin_5, GPIOA, GPIO_Pin_6, GPIOA, GPIO_Pin_7, GPIOA, GPIO_Pin_4,                            \
     0, 0, 0, 2, 1}, /* SoftSPI1：PA5(SCK), PA6(MISO), PA7(MOSI), PA4(NSS)，模式0，2us延时，MSB，启用 */ \
}

/* ==================== SPI配置 ==================== */

/* SPI配置结构体 */
typedef struct
{
    SPI_TypeDef *spi_periph;            /**< SPI外设（SPI1、SPI2或SPI3） */
    GPIO_TypeDef *sck_port;            /**< SCK引脚端口 */
    uint16_t sck_pin;                  /**< SCK引脚号 */
    GPIO_TypeDef *miso_port;           /**< MISO引脚端口 */
    uint16_t miso_pin;                 /**< MISO引脚号 */
    GPIO_TypeDef *mosi_port;           /**< MOSI引脚端口 */
    uint16_t mosi_pin;                 /**< MOSI引脚号 */
    GPIO_TypeDef *nss_port;            /**< NSS引脚端口（软件NSS时可为NULL） */
    uint16_t nss_pin;                  /**< NSS引脚号（软件NSS时可为0） */
    uint16_t mode;                     /**< SPI模式：SPI_Mode_Master或SPI_Mode_Slave */
    uint16_t direction;                /**< 数据方向：SPI_Direction_2Lines_FullDuplex等 */
    uint16_t data_size;                /**< 数据大小：SPI_DataSize_8b或SPI_DataSize_16b */
    uint16_t cpol;                     /**< 时钟极性：SPI_CPOL_Low或SPI_CPOL_High */
    uint16_t cpha;                     /**< 时钟相位：SPI_CPHA_1Edge或SPI_CPHA_2Edge */
    uint16_t nss;                      /**< NSS管理：SPI_NSS_Soft或SPI_NSS_Hard */
    uint16_t baudrate_prescaler;       /**< 波特率预分频器：SPI_BaudRatePrescaler_2等 */
    uint16_t first_bit;                /**< 首字节：SPI_FirstBit_MSB或SPI_FirstBit_LSB */
    uint8_t enabled;                   /**< 使能标志：1=启用，0=禁用 */
} SPI_Config_t;

/* SPI统一配置表 - 默认配置（SPI1，主模式，全双工） */
#define SPI_CONFIGS {                                                                                    \
    {SPI1, GPIOA, GPIO_Pin_5, GPIOA, GPIO_Pin_6, GPIOA, GPIO_Pin_7, GPIOA, GPIO_Pin_4,                  \
     SPI_Mode_Master, SPI_Direction_2Lines_FullDuplex, SPI_DataSize_8b,                                \
     SPI_CPOL_Low, SPI_CPHA_1Edge, SPI_NSS_Soft, SPI_BaudRatePrescaler_8, SPI_FirstBit_MSB, 1},       \
    /* SPI1：PA5(SCK), PA6(MISO), PA7(MOSI), PA4(NSS)，主模式，全双工，8位，模式0，软件NSS，预分频8，MSB，启用 */ \
}

/* ==================== W25Q SPI Flash配置 ==================== */

/* W25Q SPI实例配置 */
#ifndef W25Q_SPI_INSTANCE
#define W25Q_SPI_INSTANCE SPI_INSTANCE_1  /**< W25Q使用的SPI实例，默认SPI1 */
#endif

/* ==================== TF_SPI配置 ==================== */

/* TF_SPI SPI实例配置 */
#ifndef TF_SPI_SPI_INSTANCE
#define TF_SPI_SPI_INSTANCE SPI_INSTANCE_1  /**< TF_SPI使用的SPI实例，默认SPI1 */
#endif

/* ==================== UART配置 ==================== */

/* UART配置结构体 */
typedef struct
{
    USART_TypeDef *uart_periph;        /**< UART外设（USART1/USART2/USART3） */
    GPIO_TypeDef *tx_port;              /**< TX引脚端口 */
    uint16_t tx_pin;                    /**< TX引脚号 */
    GPIO_TypeDef *rx_port;              /**< RX引脚端口 */
    uint16_t rx_pin;                    /**< RX引脚号 */
    uint32_t baudrate;                  /**< 波特率（Hz），常用值：9600, 115200等 */
    uint16_t word_length;                /**< 数据位：USART_WordLength_8b或USART_WordLength_9b */
    uint16_t stop_bits;                  /**< 停止位：USART_StopBits_1或USART_StopBits_2 */
    uint16_t parity;                     /**< 校验位：USART_Parity_No、USART_Parity_Even或USART_Parity_Odd */
    uint8_t enabled;                    /**< 使能标志：1=启用，0=禁用 */
} UART_Config_t;

/* UART统一配置表 - 默认配置（USART1，PA9/10，115200，8N1） */
#define UART_CONFIGS {                                                                                    \
    {USART1, GPIOA, GPIO_Pin_9, GPIOA, GPIO_Pin_10, 115200, USART_WordLength_8b, USART_StopBits_1, USART_Parity_No, 1}, /* UART1：PA9(TX), PA10(RX)，115200，8N1，启用 */ \
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

/* PWM统一配置表 - 默认配置（TIM3，PA6/7，通道1/2） */
/* 注意：数组索引必须对应PWM_Instance_t枚举：0=TIM1, 1=TIM3, 2=TIM4 */
#define PWM_CONFIGS {                                                                                    \
    {NULL, {{NULL, 0, 0}, {NULL, 0, 0}, {NULL, 0, 0}, {NULL, 0, 0}}, 0}, /* TIM1：未使用，占位 */ \
    {TIM3, {{GPIOA, GPIO_Pin_6, 1}, {GPIOA, GPIO_Pin_7, 1}, {GPIOA, GPIO_Pin_0, 0}, {GPIOA, GPIO_Pin_1, 0}}, 1}, /* TIM3：PA6(CH1), PA7(CH2)，启用 */ \
    {NULL, {{NULL, 0, 0}, {NULL, 0, 0}, {NULL, 0, 0}, {NULL, 0, 0}}, 0}, /* TIM4：未使用，占位 */ \
}

/* ==================== CAN配置 ==================== */

/* CAN配置结构体 */
typedef struct
{
    CAN_TypeDef *can_periph;  /**< CAN外设（CAN1或CAN2） */
    GPIO_TypeDef *tx_port;    /**< TX引脚端口 */
    uint16_t tx_pin;          /**< TX引脚号 */
    GPIO_TypeDef *rx_port;    /**< RX引脚端口 */
    uint16_t rx_pin;          /**< RX引脚号 */
    uint16_t prescaler;       /**< 预分频器（1-1024），用于计算波特率 */
    uint8_t mode;             /**< CAN模式：CAN_Mode_Normal、CAN_Mode_LoopBack等 */
    uint8_t sjw;              /**< 同步跳转宽度：CAN_SJW_1tq、CAN_SJW_2tq等 */
    uint8_t bs1;              /**< 位段1：CAN_BS1_1tq ~ CAN_BS1_16tq */
    uint8_t bs2;              /**< 位段2：CAN_BS2_1tq ~ CAN_BS2_8tq */
    uint8_t enabled;          /**< 使能标志：1=启用，0=禁用 */
} CAN_Config_t;

/* CAN统一配置表 - 默认配置（CAN1，PA11/12，500kHz） */
#define CAN_CONFIGS {                                                                                    \
    {CAN1, GPIOA, GPIO_Pin_12, GPIOA, GPIO_Pin_11, 9, CAN_Mode_Normal, CAN_SJW_1tq, CAN_BS1_4tq, CAN_BS2_3tq, 1}, /* CAN1：PA12(TX), PA11(RX)，500kHz，正常模式，启用 */ \
}

/* ==================== DAC配置 ==================== */

/* DAC配置结构体（仅HD/CL/HD_VL/MD_VL型号支持） */
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL) || defined(STM32F10X_MD_VL)
typedef struct
{
    DAC_Channel_t channel;          /**< DAC通道（1或2） */
    DAC_Trigger_t trigger;         /**< 触发模式 */
    DAC_OutputBuffer_t output_buffer; /**< 输出缓冲使能 */
    uint8_t enabled;               /**< 使能标志：1=启用，0=禁用 */
} DAC_Config_t;

/* DAC统一配置表 - 默认配置（禁用） */
#define DAC_CONFIGS {                                                                                    \
    {DAC_CHANNEL_1, DAC_TRIGGER_NONE, DAC_OUTPUT_BUFFER_ENABLE, 0}, /* DAC1：PA4，无触发，使能输出缓冲，禁用 */ \
    {DAC_CHANNEL_2, DAC_TRIGGER_NONE, DAC_OUTPUT_BUFFER_ENABLE, 0}, /* DAC2：PA5，无触发，使能输出缓冲，禁用 */ \
}
#endif

/* ==================== ADC配置 ==================== */

/* ADC配置结构体 */
typedef struct
{
    ADC_TypeDef *adc_periph;  /**< ADC外设（ADC1） */
    uint8_t channels[16];     /**< ADC通道数组（ADC_Channel_0 ~ ADC_Channel_17） */
    uint8_t channel_count;    /**< 通道数量（1-16） */
    uint8_t sample_time;      /**< 采样时间：ADC_SampleTime_1Cycles5 ~ ADC_SampleTime_239Cycles5 */
    uint8_t enabled;          /**< 使能标志：1=启用，0=禁用 */
} ADC_Config_t;

/* ADC统一配置表 - 默认配置（ADC1，PA0，单通道） */
#define ADC_CONFIGS {                                                                                    \
    {ADC1, {ADC_Channel_0}, 1, ADC_SampleTime_55Cycles5, 1}, /* ADC1：PA0，单通道，55.5周期采样，启用 */ \
}

/* ==================== DMA配置 ==================== */

/* DMA配置结构体 */
typedef struct
{
    uint32_t peripheral_addr;  /**< 外设地址 */
    uint32_t direction;        /**< 传输方向：DMA_DIR_PeripheralSRC或DMA_DIR_PeripheralDST */
    uint32_t peripheral_inc;   /**< 外设地址递增：DMA_PeripheralInc_Enable或DMA_PeripheralInc_Disable */
    uint32_t memory_inc;       /**< 内存地址递增：DMA_MemoryInc_Enable或DMA_MemoryInc_Disable */
    uint32_t data_size;        /**< 数据大小：DMA_PeripheralDataSize_Byte、DMA_PeripheralDataSize_HalfWord、DMA_PeripheralDataSize_Word */
    uint32_t mode;             /**< 模式：DMA_Mode_Normal或DMA_Mode_Circular */
    uint32_t priority;         /**< 优先级：DMA_Priority_Low、DMA_Priority_Medium、DMA_Priority_High、DMA_Priority_VeryHigh */
    uint8_t enabled;          /**< 使能标志：1=启用，0=禁用 */
} DMA_Config_t;

/* DMA统一配置表 - 默认配置（DMA1 Channel1，用于UART1 TX） */
/* 注意：DMA2仅在HD/CL/HD_VL型号上可用，MD/LD型号只有DMA1 */
#define DMA_CONFIGS {                                                                                    \
    {(uint32_t)&USART1->DR, DMA_DIR_PeripheralDST, DMA_PeripheralInc_Disable, DMA_MemoryInc_Enable, DMA_PeripheralDataSize_Byte, DMA_Mode_Normal, DMA_Priority_Medium, 0}, /* DMA1_CH1：UART1 TX，内存到外设，字节，正常模式，中等优先级，禁用 */ \
    {(uint32_t)&USART1->DR, DMA_DIR_PeripheralSRC, DMA_PeripheralInc_Disable, DMA_MemoryInc_Enable, DMA_PeripheralDataSize_Byte, DMA_Mode_Normal, DMA_Priority_Medium, 0}, /* DMA1_CH2：UART1 RX，外设到内存，字节，正常模式，中等优先级，禁用 */ \
    {(uint32_t)&USART2->DR, DMA_DIR_PeripheralDST, DMA_PeripheralInc_Disable, DMA_MemoryInc_Enable, DMA_PeripheralDataSize_Byte, DMA_Mode_Normal, DMA_Priority_Medium, 0}, /* DMA1_CH3：UART2 TX，内存到外设，字节，正常模式，中等优先级，禁用 */ \
    {(uint32_t)&USART2->DR, DMA_DIR_PeripheralSRC, DMA_PeripheralInc_Disable, DMA_MemoryInc_Enable, DMA_PeripheralDataSize_Byte, DMA_Mode_Normal, DMA_Priority_Medium, 0}, /* DMA1_CH4：UART2 RX，外设到内存，字节，正常模式，中等优先级，禁用 */ \
    {(uint32_t)&USART3->DR, DMA_DIR_PeripheralDST, DMA_PeripheralInc_Disable, DMA_MemoryInc_Enable, DMA_PeripheralDataSize_Byte, DMA_Mode_Normal, DMA_Priority_Medium, 0}, /* DMA1_CH5：UART3 TX，内存到外设，字节，正常模式，中等优先级，禁用 */ \
    {(uint32_t)&USART3->DR, DMA_DIR_PeripheralSRC, DMA_PeripheralInc_Disable, DMA_MemoryInc_Enable, DMA_PeripheralDataSize_Byte, DMA_Mode_Normal, DMA_Priority_Medium, 0}, /* DMA1_CH6：UART3 RX，外设到内存，字节，正常模式，中等优先级，禁用 */ \
    {0, DMA_DIR_PeripheralSRC, DMA_PeripheralInc_Enable, DMA_MemoryInc_Enable, DMA_PeripheralDataSize_Byte, DMA_Mode_Normal, DMA_Priority_Medium, 0}, /* DMA1_CH7：内存到内存，字节，正常模式，中等优先级，禁用 */ \
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    {0, DMA_DIR_PeripheralSRC, DMA_PeripheralInc_Disable, DMA_MemoryInc_Enable, DMA_PeripheralDataSize_HalfWord, DMA_Mode_Circular, DMA_Priority_High, 0}, /* DMA2_CH1：ADC1，外设到内存，半字，循环模式，高优先级，禁用（仅HD/CL/HD_VL） */ \
    {0, DMA_DIR_PeripheralSRC, DMA_PeripheralInc_Disable, DMA_MemoryInc_Enable, DMA_PeripheralDataSize_Byte, DMA_Mode_Normal, DMA_Priority_Medium, 0}, /* DMA2_CH2：SPI1 RX，外设到内存，字节，正常模式，中等优先级，禁用（仅HD/CL/HD_VL） */ \
    {0, DMA_DIR_PeripheralDST, DMA_PeripheralInc_Disable, DMA_MemoryInc_Enable, DMA_PeripheralDataSize_Byte, DMA_Mode_Normal, DMA_Priority_Medium, 0}, /* DMA2_CH3：SPI1 TX，内存到外设，字节，正常模式，中等优先级，禁用（仅HD/CL/HD_VL） */ \
    {0, DMA_DIR_PeripheralSRC, DMA_PeripheralInc_Disable, DMA_MemoryInc_Enable, DMA_PeripheralDataSize_Byte, DMA_Mode_Normal, DMA_Priority_Medium, 0}, /* DMA2_CH4：SPI2 RX，外设到内存，字节，正常模式，中等优先级，禁用（仅HD/CL/HD_VL） */ \
    {0, DMA_DIR_PeripheralDST, DMA_PeripheralInc_Disable, DMA_MemoryInc_Enable, DMA_PeripheralDataSize_Byte, DMA_Mode_Normal, DMA_Priority_Medium, 0}, /* DMA2_CH5：SPI2 TX，内存到外设，字节，正常模式，中等优先级，禁用（仅HD/CL/HD_VL） */ \
#endif
}

/* ==================== EXTI配置 ==================== */

/* EXTI配置结构体 */
typedef struct
{
    EXTI_Line_t line;           /**< EXTI线号（0-19） */
    GPIO_TypeDef *port;         /**< GPIO端口（Line 0-15需要，Line 16-19为NULL） */
    uint16_t pin;               /**< GPIO引脚号（Line 0-15需要，Line 16-19为0） */
    EXTI_Trigger_t trigger;     /**< 触发模式：上升沿/下降沿/双边沿 */
    EXTI_Mode_t mode;           /**< 模式：中断/事件 */
    uint8_t enabled;            /**< 使能标志：1=启用，0=禁用 */
} EXTI_Config_t;

/* EXTI统一配置表 - 默认配置（禁用） */
#define EXTI_CONFIGS {                                                                                    \
    {EXTI_LINE_0, GPIOA, GPIO_Pin_0, EXTI_TRIGGER_RISING, EXTI_MODE_INTERRUPT, 0}, /* EXTI0：PA0，上升沿，中断模式，禁用 */ \
}

/* ==================== Buzzer配置 ==================== */

/* Buzzer驱动模式枚举 */
typedef enum {
    BUZZER_MODE_GPIO = 0,  /**< GPIO模式：简单开关，无频率控制 */
    BUZZER_MODE_PWM = 1    /**< PWM模式：通过PWM控制频率 */
} Buzzer_Mode_t;

/* Buzzer配置结构体 */
typedef struct
{
    Buzzer_Mode_t mode;        /**< 驱动模式（GPIO/PWM） */
    GPIO_TypeDef *port;        /**< GPIO端口（GPIO模式必需） */
    uint16_t pin;              /**< GPIO引脚（GPIO模式必需） */
    uint8_t pwm_instance;      /**< PWM实例（PWM模式必需，0=TIM1, 1=TIM3, 2=TIM4） */
    uint8_t pwm_channel;       /**< PWM通道（PWM模式必需，0=CH1, 1=CH2, 2=CH3, 3=CH4） */
    uint8_t active_level;      /**< 有效电平（Bit_SET或Bit_RESET） */
    uint8_t enabled;           /**< 使能标志：1=启用，0=禁用 */
} Buzzer_Config_t;

/* Buzzer统一配置表 - 默认配置（GPIO模式，PA2，低电平有效） */
#define BUZZER_CONFIGS {                                                                                    \
    {BUZZER_MODE_GPIO, GPIOA, GPIO_Pin_2, 1, 0, Bit_RESET, 0}, /* Buzzer1：GPIO模式，PA2，低电平有效，禁用（PWM实例和通道在GPIO模式下忽略） */ \
}

/* ==================== TB6612配置 ==================== */

/* TB6612配置结构体 */
typedef struct
{
    GPIO_TypeDef *ain1_port;      /**< AIN1引脚端口（方向控制） */
    uint16_t ain1_pin;            /**< AIN1引脚号 */
    GPIO_TypeDef *ain2_port;      /**< AIN2引脚端口（方向控制） */
    uint16_t ain2_pin;            /**< AIN2引脚号 */
    GPIO_TypeDef *stby_port;      /**< STBY引脚端口（待机控制） */
    uint16_t stby_pin;            /**< STBY引脚号 */
    uint8_t pwm_instance;        /**< PWM实例（0=TIM1, 1=TIM3, 2=TIM4） */
    uint8_t pwm_channel;          /**< PWM通道（0=CH1, 1=CH2, 2=CH3, 3=CH4） */
    uint8_t enabled;              /**< 使能标志：1=启用，0=禁用 */
} TB6612_Config_t;

/* TB6612统一配置表 - 默认配置（未使用，占位） */
#define TB6612_CONFIGS {                                                                                    \
    {NULL, 0, NULL, 0, NULL, 0, 0, 0, 0}, /* TB6612实例1：未使用，占位 */ \
    {NULL, 0, NULL, 0, NULL, 0, 0, 0, 0}, /* TB6612实例2：未使用，占位 */ \
}

/* ==================== 时钟管理配置 ==================== */

/* 时钟管理配置 - 案例10配置（自动模式，用于CPU负载监控） */
#define CLKM_LOAD_CHECK_INTERVAL   50      /* 负载检测周期：50ms */
#define CLKM_LOAD_THRESHOLD_HIGH   50      /* 高负载阈值：50% */
#define CLKM_LOAD_THRESHOLD_LOW    30      /* 低负载阈值：30% */
#define CLKM_AUTO_POLICY_STEP      1       /* 降档步进：1档 */
#define CLKM_AUTO_POLICY_JUMP      3       /* 升档跳跃：3档 */
#define CLKM_SWITCH_INTERVAL_UP    1000    /* 升频间隔：1秒 */
#define CLKM_SWITCH_INTERVAL_DOWN  5000    /* 降频间隔：5秒 */
#define CLKM_ADAPTIVE_ENABLE       1       /* 启用自动调频（可选，用于测试CPU负载监控） */
#define CLKM_IDLE_HOOK_ENABLE      1       /* 启用空闲钩子（用于CPU负载统计） */

#endif /* BOARD_H */
