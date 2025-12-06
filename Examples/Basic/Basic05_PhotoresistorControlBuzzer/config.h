/**
 * @file config.h
 * @brief Basic05_PhotoresistorControlBuzzer案例专用配置（独立工程）
 * @details 只启用案例需要的模块，禁用其他模块以减小代码体积
 */

/* 先取消LED模块定义（案例不使用LED），即使System/config.h已经定义了 */
#undef CONFIG_MODULE_LED_ENABLED

#ifndef EXAMPLE_CONFIG_H
#define EXAMPLE_CONFIG_H

/* ==================== 模块开关 ==================== */

/* 驱动层模块 */
#define CONFIG_MODULE_GPIO_ENABLED          1   /**< GPIO模块开关 - 必须（Buzzer和光敏电阻DO引脚依赖） */
#define CONFIG_MODULE_BUZZER_ENABLED       1   /**< Buzzer模块开关 - 必须（案例使用） */
#define CONFIG_MODULE_OLED_ENABLED         1   /**< OLED模块开关 - 启用（用于显示光照状态和蜂鸣器状态） */
#define CONFIG_MODULE_SOFT_I2C_ENABLED     1   /**< 软件I2C模块开关 - 启用（OLED使用） */

/* 系统层模块 */
#define CONFIG_MODULE_DELAY_ENABLED        1   /**< 延时模块开关 - 必须（案例使用） */
#define CONFIG_MODULE_BASE_TIMER_ENABLED   1   /**< 基时定时器模块开关 - 必须（delay依赖） */
#define CONFIG_MODULE_SYSTEM_INIT_ENABLED   1   /**< 系统初始化模块开关 - 必须（案例使用） */

/* 通用模块 */
#undef CONFIG_MODULE_LOG_ENABLED                /**< 先取消可能存在的定义 */
#define CONFIG_MODULE_LOG_ENABLED           0   /**< 日志模块开关 - 禁用（案例不使用） */

#endif /* EXAMPLE_CONFIG_H */
