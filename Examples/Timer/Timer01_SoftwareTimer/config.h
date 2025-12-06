/**
 * @file config.h
 * @brief Timer01_SoftwareTimer案例专用配置（独立工程）
 * @details 只启用案例需要的模块，禁用其他模块以减小代码体积
 */

#ifndef EXAMPLE_CONFIG_H
#define EXAMPLE_CONFIG_H

/* ==================== 模块开关 ==================== */

/* 驱动层模块 */
#define CONFIG_MODULE_GPIO_ENABLED          1   /**< GPIO模块开关 - 必须（LED、按钮、OLED依赖） */
#define CONFIG_MODULE_LED_ENABLED           1   /**< LED模块开关 - 必须（案例使用） */
#define CONFIG_MODULE_OLED_ENABLED         1   /**< OLED模块开关 - 必须（定时器状态显示） */
#define CONFIG_MODULE_SOFT_I2C_ENABLED   1   /**< 软件I2C模块开关 - 必须（OLED使用） */

/* 系统层模块 */
#define CONFIG_MODULE_DELAY_ENABLED        1   /**< 延时模块开关 - 必须（案例使用） */
#define CONFIG_MODULE_BASE_TIMER_ENABLED   1   /**< 基时定时器模块开关 - 必须（TIM_SW和delay依赖） */
#define CONFIG_MODULE_TIM_SW_ENABLED       1   /**< 软件定时器模块开关 - 必须（案例核心功能） */
#define CONFIG_MODULE_SYSTEM_INIT_ENABLED   1   /**< 系统初始化模块开关 - 必须（案例使用） */

/* 通用模块 */
#undef CONFIG_MODULE_LOG_ENABLED                /**< 先取消可能存在的定义 */
#define CONFIG_MODULE_LOG_ENABLED           0   /**< 日志模块开关 - 禁用（案例不使用） */

#endif /* EXAMPLE_CONFIG_H */
