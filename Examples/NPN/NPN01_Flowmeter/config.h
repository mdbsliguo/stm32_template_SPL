/**
 * @file config.h
 * @brief 模块开关配置头文件（NPN01_Flowmeter 独立工程专用）
 * @details 仅启用本案例所需的模块
 */

#ifndef CONFIG_H
#define CONFIG_H

/* ==================== 模块开关 ==================== */

/* 系统模块（必须启用） */
#define CONFIG_MODULE_SYSTEM_INIT_ENABLED 1
#define CONFIG_MODULE_GPIO_ENABLED 1
#define CONFIG_MODULE_DELAY_ENABLED 1
#define CONFIG_MODULE_TIM2_TIMEBASE_ENABLED 1

/* LED 模块 */
#define CONFIG_MODULE_LED_ENABLED 1

/* EXTI 模块（核心计数功能） */
#define CONFIG_MODULE_EXTI_ENABLED 1

/* OLED 模块（用于显示计数，可选但默认启用） */
#define CONFIG_MODULE_OLED_ENABLED 1
#define CONFIG_MODULE_SOFT_I2C_ENABLED 1

/* 错误处理模块（EXTI/驱动依赖） */
#define CONFIG_MODULE_ERROR_HANDLER_ENABLED 1

/* 其他模块（禁用） */
#undef CONFIG_MODULE_LOG_ENABLED
#define CONFIG_MODULE_LOG_ENABLED 0

#endif /* CONFIG_H */
