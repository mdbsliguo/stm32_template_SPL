/**
 * @file board.h
 * @brief 硬件配置头文件（Flash15独立工程专用）
 * @details 此文件包含Flash15案例所需的硬件配置
 * 
 * 注意：这是独立工程的board.h，包含案例所需的硬件配置
 * 
 * 硬件要求：
 * - LED1：PA1（系统状态指示）
 * - W25Q SPI Flash模块（SPI2接口）
 *   - CS：PA11
 *   - SCK：PB13（SPI2_SCK）
 *   - MISO：PB14（SPI2_MISO）
 *   - MOSI：PB15（SPI2_MOSI）
 * - OLED显示屏（软件I2C接口）
 *   - SCL：PB8（软件I2C）
 *   - SDA：PB9（软件I2C）
 * - UART1（用于详细日志输出）
 *   - TX：PA9
 *   - RX：PA10
 */

#ifndef BOARD_H
#define BOARD_H

#include "stm32f10x.h"
#include <stdint.h>

/* ==================== LED配置 ==================== */

/* LED配置结构体 */
typedef struct
{
    GPIO_TypeDef *port;   /**< GPIO端口基地址 */
    uint16_t pin;         /**< 引脚号 */
    uint8_t active_level; /**< 有效电平（Bit_SET或Bit_RESET） */
    uint8_t enabled;      /**< 使能标志：1=启用，0=禁用 */
} LED_Config_t;

/* LED统一配置表 - Flash15配置 */
#define LED_CONFIGS {                                                            \
    {GPIOA, GPIO_Pin_1, Bit_RESET, 1}, /* LED1：PA1，低电平点亮，启用（系统状态指示） */ \
}

/* ==================== SPI配置 ==================== */

/* SPI配置结构体 */
typedef struct
{
    SPI_TypeDef *spi_periph;          /**< SPI外设（SPI1/SPI2/SPI3） */
    GPIO_TypeDef *sck_port;            /**< SCK引脚端口 */
    uint16_t sck_pin;                  /**< SCK引脚号 */
    GPIO_TypeDef *miso_port;            /**< MISO引脚端口 */
    uint16_t miso_pin;                 /**< MISO引脚号 */
    GPIO_TypeDef *mosi_port;            /**< MOSI引脚端口 */
    uint16_t mosi_pin;                 /**< MOSI引脚号 */
    GPIO_TypeDef *nss_port;            /**< NSS引脚端口（软件NSS时为NULL） */
    uint16_t nss_pin;                  /**< NSS引脚号（软件NSS时为0） */
    uint16_t mode;                     /**< SPI模式：SPI_Mode_Master/SPI_Mode_Slave */
    uint16_t direction;                /**< 数据方向：SPI_Direction_2Lines_FullDuplex */
    uint16_t data_size;                /**< 数据大小：SPI_DataSize_8b/SPI_DataSize_16b */
    uint16_t cpol;                     /**< 时钟极性：SPI_CPOL_Low/SPI_CPOL_High */
    uint16_t cpha;                     /**< 时钟相位：SPI_CPHA_1Edge/SPI_CPHA_2Edge */
    uint16_t nss;                      /**< NSS管理：SPI_NSS_Soft/SPI_NSS_Hard */
    uint16_t baudrate_prescaler;       /**< 波特率预分频：SPI_BaudRatePrescaler_2 */
    uint16_t first_bit;                /**< 首字节：SPI_FirstBit_MSB/SPI_FirstBit_LSB */
    uint8_t enabled;                   /**< 使能标志：1=启用，0=禁用 */
} SPI_Config_t;

/* SPI统一配置表 - Flash15配置（SPI2，PB13/14/15，CS=PA11） */
/* 注意：数组索引对应SPI_INSTANCE枚举值，SPI_INSTANCE_1=0，SPI_INSTANCE_2=1，SPI_INSTANCE_3=2 */
#define SPI_CONFIGS {                                                                                    \
    {NULL, NULL, 0, NULL, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* SPI1：未使用，禁用 */  \
    {SPI2, GPIOB, GPIO_Pin_13, GPIOB, GPIO_Pin_14, GPIOB, GPIO_Pin_15, GPIOA, GPIO_Pin_11,              \
     SPI_Mode_Master, SPI_Direction_2Lines_FullDuplex, SPI_DataSize_8b,                                \
     SPI_CPOL_Low, SPI_CPHA_1Edge, SPI_NSS_Soft, SPI_BaudRatePrescaler_2, SPI_FirstBit_MSB, 1},         \
    /* SPI2：PB13(SCK), PB14(MISO), PB15(MOSI), PA11(CS)，主模式，全双工，8位，模式0，软件NSS，预分频2（约18MHz），MSB优先，启用 */ \
}

/* ==================== W25Q SPI Flash配置 ==================== */

/* W25Q SPI实例配置 */
#ifndef W25Q_SPI_INSTANCE
#define W25Q_SPI_INSTANCE SPI_INSTANCE_2  /**< W25Q使用的SPI实例，使用SPI2 */
#endif

/* SPI2 NSS引脚宏定义（用于案例代码） */
#define SPI2_NSS_PORT  GPIOA
#define SPI2_NSS_PIN   GPIO_Pin_11

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

/* 软件I2C统一配置表 - Flash15配置 */
/* 注意：数组索引对应SoftI2C_Instance枚举值，SoftI2C_INSTANCE_1=0 */
#define SOFT_I2C_CONFIGS {                                                                    \
    {GPIOB, GPIO_Pin_8, GPIOB, GPIO_Pin_9, 5, 1}, /* SoftI2C1：PB8(SCL), PB9(SDA)，5us延时，启用（OLED使用） */ \
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

/* OLED I2C配置 - Flash15配置（软件I2C，PB8/9） */
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
#define OLED_I2C_SOFT_INSTANCE 0  /* SOFT_I2C_INSTANCE_1（PB8/9） */
#endif

#ifndef OLED_I2C_HARD_INSTANCE
#define OLED_I2C_HARD_INSTANCE 0  /* I2C_INSTANCE_1 */
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
    uint32_t baudrate;                  /**< 波特率（Hz），常用值：9600, 115200 */
    uint16_t word_length;                /**< 数据位：USART_WordLength_8b/USART_WordLength_9b */
    uint16_t stop_bits;                  /**< 停止位：USART_StopBits_1/USART_StopBits_2 */
    uint16_t parity;                     /**< 校验位：USART_Parity_No/USART_Parity_Even/USART_Parity_Odd */
    uint8_t enabled;                    /**< 使能标志：1=启用，0=禁用 */
} UART_Config_t;

/* UART统一配置表 - Flash15配置（UART1，PA9/10，115200） */
#define UART_CONFIGS {                                                                                    \
    {USART1, GPIOA, GPIO_Pin_9, GPIOA, GPIO_Pin_10, 115200, USART_WordLength_8b, USART_StopBits_1, USART_Parity_No, 1}, /* UART1：PA9(TX), PA10(RX)，115200，8N1，启用 */ \
}

/* ==================== 模拟写入错误配置 ==================== */
/* 
 * ?? 警告：此功能仅用于验证代码逻辑，正常测试时请勿开启！
 * 
 * 模拟写入错误功能会在主测试中随机选择指定数量的字节地址，故意写入错误数据（0xFF），
 * 用于验证读取验证功能是否能正确检测到错误，并验证误码率计算是否正确。
 * 在正常寿命测试中，应该禁用此功能（设置为0），以确保测试结果的准确性。
 */

/**
 * @brief 模拟写入错误功能开关
 * @note 启用后，在主测试写入循环中会随机选择指定数量的字节地址写入0xFF（模拟写入失败），
 *       用于测试读取验证功能是否能正确检测到数据错误，并验证误码率计算是否正确
 * @warning ?? 此功能仅用于验证代码逻辑，正常测试时请勿开启！应设置为0。
 * @details
 *   - 1：启用模拟写入错误功能（仅用于代码验证）
 *   - 0：禁用模拟写入错误功能（正常测试时使用，默认）
 */
#ifndef ENDURANCE_TEST_SIMULATE_WRITE_ERROR_ENABLED
#define ENDURANCE_TEST_SIMULATE_WRITE_ERROR_ENABLED  0  /**< 模拟写入错误功能开关：1=启用（仅验证代码），0=禁用（正常测试） */
#endif

/**
 * @brief 模拟写入错误的字节数量
 * @note 指定在主测试写入循环中随机选择多少个字节地址写入0xFF（模拟写入失败）
 * @warning ?? 此功能仅用于验证代码逻辑，正常测试时请勿开启！
 * @details
 *   - 范围：1-10（建议值：3）
 *   - 这些字节的错误计入误码率计算，用于验证误码率计算是否正确
 *   - 如果设置为0或功能被禁用，则不进行模拟写入错误测试
 */
#ifndef ENDURANCE_TEST_SIMULATE_WRITE_ERROR_COUNT
#define ENDURANCE_TEST_SIMULATE_WRITE_ERROR_COUNT    3  /**< 模拟写入错误的字节数量：1-10（默认3，仅用于代码验证） */
#endif

/* ==================== 连续擦除配置 ==================== */

/**
 * @brief 连续擦除次数配置
 * @note 指定连续擦除多少次后才进行一次写入和读取操作，用于加快测试速度
 * @details
 *   - 范围：1-100（默认：1）
 *   - 设置为1：每次擦除后立即进行写入和读取（正常模式）
 *   - 设置为10：连续擦除10次后，才进行一次写入和读取操作
 *   - 这样可以加快测试速度，因为擦除操作是Flash寿命测试的主要时间消耗
 *   - 注意：连续擦除会增加Flash的磨损，但不会影响测试结果的准确性
 *   - 重要：第1轮（total_cycles == 0）不受此配置影响，总是执行完整的擦除-写入-读取流程
 */
#ifndef ENDURANCE_TEST_CONSECUTIVE_ERASE_COUNT
#define ENDURANCE_TEST_CONSECUTIVE_ERASE_COUNT    10  /**< 连续擦除次数：1-100（默认1，即每次擦除后立即写入和读取） */
#endif

/* ==================== 误码率报废阈值配置 ==================== */

/**
 * @brief 误码率报废阈值配置
 * @note 当误码率超过此阈值时，芯片将被判定为报废
 * @details
 *   不同行业的标准建议：
 *   - 一般应用：1e-3 (0.1%) - 默认值，适用于大多数消费电子产品
 *   - 工业应用：1e-4 (0.01%) - 适用于工业控制、汽车电子等
 *   - 医疗设备（一般）：1e-6 (0.0001% 或 1 PPM) - 适用于一般医疗设备
 *   - 医疗设备（关键）：1e-9 (0.0000001% 或 1 PPB) - 适用于生命支持、植入式设备等关键医疗设备
 *   - 航空航天：1e-9 (0.0000001% 或 1 PPB) - 适用于航空航天等高可靠性应用
 * 
 *   设置方法：
 *   - 一般应用：使用默认值 1e-3f
 *   - 医疗设备（一般）：设置为 1e-6f
 *   - 医疗设备（关键）/航空航天：设置为 1e-9f
 *   - 自定义：设置为任意浮点数值，例如 5e-4f (0.05%)
 * 
 * @example
 *   // 医疗设备（一般）
 *   #define ENDURANCE_TEST_ERROR_RATE_THRESHOLD  1e-6f
 * 
 *   // 医疗设备（关键）
 *   #define ENDURANCE_TEST_ERROR_RATE_THRESHOLD  1e-9f
 */
#ifndef ENDURANCE_TEST_ERROR_RATE_THRESHOLD
#define ENDURANCE_TEST_ERROR_RATE_THRESHOLD  1e-3f  /**< 误码率报废阈值（默认：1e-3，即0.1%，适用于一般应用） */
#endif

#endif /* BOARD_H */

