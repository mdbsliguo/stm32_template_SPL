/**
 * @file config.h
 * @brief 模块配置头文件（EXTI01_Infrared_Counter案例独立工程专用）
 * @details 此文件包含案例所需的模块开关配置
 * 
 * 注意：这是独立工程的config.h，只启用本案例需要的模块
 */

#ifndef CONFIG_H
#define CONFIG_H

/* ==================== 模块开关 ==================== */

/* 系统模块（必须启用） */
#define CONFIG_MODULE_SYSTEM_INIT_ENABLED 1
#define CONFIG_MODULE_GPIO_ENABLED 1
#define CONFIG_MODULE_DELAY_ENABLED 1
#define CONFIG_MODULE_TIM2_TIMEBASE_ENABLED 1

/* LED模块 */
#define CONFIG_MODULE_LED_ENABLED 1

/* EXTI模块（核心功能） */
#define CONFIG_MODULE_EXTI_ENABLED 1

/* OLED模块（用于显示计数） */
#define CONFIG_MODULE_OLED_ENABLED 1
#define CONFIG_MODULE_SOFT_I2C_ENABLED 1

/* 错误处理模块（EXTI模块依赖） */
#define CONFIG_MODULE_ERROR_HANDLER_ENABLED 1

/* 其他模块（禁用） */
#undef CONFIG_MODULE_LOG_ENABLED
#define CONFIG_MODULE_LOG_ENABLED 0

#endif /* CONFIG_H */
