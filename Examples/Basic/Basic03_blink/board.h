/**
 * @file board.h
 * @brief 硬件配置头文件（案例1独立工程专用）
 * @details 此文件包含案例1所需的硬件配置
 * 
 * 注意：这是独立工程的board.h，包含案例所需的硬件配置
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

/* LED统一配置表 - 案例1配置 */
#define LED_CONFIGS {                                                    \
    {GPIOA, GPIO_Pin_1, Bit_RESET, 1}, /* LED1：PA1，低电平点亮，启用 */ \
    {GPIOA, GPIO_Pin_2, Bit_RESET, 1}, /* LED2：PA2，低电平点亮，启用 */ \
}

/* ==================== OLED配置 ==================== */

/* OLED I2C配置结构体 */
typedef struct
{
    GPIO_TypeDef *scl_port; /**< SCL引脚端口 */
    uint16_t scl_pin;       /**< SCL引脚号 */
    GPIO_TypeDef *sda_port; /**< SDA引脚端口 */
    uint16_t sda_pin;       /**< SDA引脚号 */
} OLED_I2C_Config_t;

/* OLED I2C配置 */
#define OLED_I2C_CONFIG {      \
    GPIOB,                     \
    GPIO_Pin_8, /* SCL: PB8 */ \
    GPIOB,                     \
    GPIO_Pin_9, /* SDA: PB9 */ \
}

/* ==================== 时钟管理配置 ==================== */

/* 时钟管理配置 - 默认配置（案例1不使用，但需要定义以避免编译错误） */
#define CLKM_LOAD_CHECK_INTERVAL 50    /* 负载检测周期：50ms */
#define CLKM_LOAD_THRESHOLD_HIGH 50    /* 高负载阈值：50% */
#define CLKM_LOAD_THRESHOLD_LOW 30     /* 低负载阈值：30% */
#define CLKM_AUTO_POLICY_STEP 1        /* 降档步进：1档 */
#define CLKM_AUTO_POLICY_JUMP 3        /* 升档跳跃：3档 */
#define CLKM_SWITCH_INTERVAL_UP 1000   /* 升频间隔：1秒 */
#define CLKM_SWITCH_INTERVAL_DOWN 5000 /* 降频间隔：5秒 */
#define CLKM_ADAPTIVE_ENABLE 1         /* 启用自动调频 */
#define CLKM_IDLE_HOOK_ENABLE 1        /* 启用空闲钩子 */

#endif /* BOARD_H */
