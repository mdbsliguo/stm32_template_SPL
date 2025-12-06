/**
 * @file board.h
 * @brief 硬件配置头文件（Basic04_ButtonControlLED案例独立工程专用）
 * @details 此文件包含案例所需的硬件配置
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

/* LED统一配置表 - Basic04案例配置 */
#define LED_CONFIGS {                                                    \
    {GPIOA, GPIO_Pin_1, Bit_RESET, 1}, /* LED1：PA1，低电平点亮，启用 */ \
}

/* ==================== 按钮配置 ==================== */

/* 按钮引脚定义
 * 注意：按钮使用上拉输入模式，按下时为低电平
 * 如果硬件连接不同，请修改以下定义
 */
#define BUTTON_PORT  GPIOA   /**< 按钮GPIO端口 */
#define BUTTON_PIN   GPIO_Pin_4  /**< 按钮GPIO引脚（PA4） */

#endif /* BOARD_H */
