#ifndef BOARD_H
#define BOARD_H

#include "stm32f10x.h"
#include <stdint.h>

/* ==================== LED配置 ==================== */
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t active_level;
    uint8_t enabled;
} LED_Config_t;

#define LED_CONFIGS {                                                    \
    {GPIOA, GPIO_Pin_1, Bit_RESET, 1}, /* LED1：PA1，低电平点亮，启用（系统状态指示） */ \
}

/* ==================== OLED配置 ==================== */
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
    GPIO_Pin_8, /* SCL: PB8 */ \
    GPIOB,                     \
    GPIO_Pin_9, /* SDA: PB9 */ \
}

#ifndef OLED_I2C_TYPE
#define OLED_I2C_TYPE OLED_I2C_TYPE_SOFTWARE
#endif

#ifndef OLED_I2C_SOFT_INSTANCE
#define OLED_I2C_SOFT_INSTANCE 0  /* SOFT_I2C_INSTANCE_1 */
#endif

#ifndef OLED_I2C_HARD_INSTANCE
#define OLED_I2C_HARD_INSTANCE 0  /* I2C_INSTANCE_1 */
#endif

/* ==================== 软件I2C配置 ==================== */
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
    {GPIOB, GPIO_Pin_8, GPIOB, GPIO_Pin_9, 5, 1}, /* SoftI2C1：PB8(SCL), PB9(SDA)，5us延时，启用（OLED使用） */ \
    {GPIOB, GPIO_Pin_10, GPIOB, GPIO_Pin_11, 5, 1}, /* SoftI2C2：PB10(SCL), PB11(SDA)，5us延时，启用（DS3231使用） */ \
}

/* ==================== 时钟管理配置 ==================== */
#define CLKM_LOAD_CHECK_INTERVAL 50
#define CLKM_LOAD_THRESHOLD_HIGH 50
#define CLKM_LOAD_THRESHOLD_LOW 30
#define CLKM_AUTO_POLICY_STEP 1
#define CLKM_AUTO_POLICY_JUMP 3
#define CLKM_SWITCH_INTERVAL_UP 1000
#define CLKM_SWITCH_INTERVAL_DOWN 5000
#define CLKM_ADAPTIVE_ENABLE 0
#define CLKM_IDLE_HOOK_ENABLE 0

#endif /* BOARD_H */
