/**
 * @file config.h
 * @brief 案例4专用配置（独立工程）
 * @details 只启用案例4需要的模块，禁用其他模块以减小代码体积
 */

#ifndef EXAMPLE_CONFIG_H
#define EXAMPLE_CONFIG_H

/* ==================== 模块开关 ==================== */

/* 驱动层模块 */
#define CONFIG_MODULE_GPIO_ENABLED          1   /**< GPIO模块开关 - 必须（LED、OLED依赖） */
#define CONFIG_MODULE_LED_ENABLED           1   /**< LED模块开关 - 必须（案例4使用） */
#define CONFIG_MODULE_OLED_ENABLED         1   /**< OLED模块开关 - 必须（案例4使用） */
#define CONFIG_MODULE_SOFT_I2C_ENABLED   1   /**< 软件I2C模块开关 - 必须（OLED使用） */

/* 系统层模块 */
#define CONFIG_MODULE_DELAY_ENABLED        1   /**< 延时模块开关 - 必须（案例4使用） */
#define CONFIG_MODULE_BASE_TIMER_ENABLED   1   /**< 基时定时器模块开关 - 必须（delay依赖） */
#define CONFIG_MODULE_CLOCK_MANAGER_ENABLED 1   /**< 时钟管理模块开关 - 必须（案例4使用） */
#define CONFIG_MODULE_SYSTEM_INIT_ENABLED   1   /**< 系统初始化模块开关 - 必须（案例4使用） */
#define CONFIG_MODULE_IWDG_ENABLED         0   /**< 独立看门狗模块开关 - 禁用（案例4不使用） */

/* 通用模块 */
#define CONFIG_MODULE_ERROR_HANDLER_ENABLED 0   /**< 错误处理模块开关 - 禁用（案例4不使用） */
#define CONFIG_MODULE_LOG_ENABLED           0   /**< 日志模块开关 - 禁用（案例4不使用） */
#define CONFIG_MODULE_MODULE_CTRL_ENABLED   0   /**< 模块开关总控开关 - 禁用（案例4不使用） */
#define CONFIG_MODULE_SYSTEM_MONITOR_ENABLED 0  /**< 系统监控模块开关 - 禁用（案例4不使用） */

/* ==================== 功能开关 ==================== */

/* 错误处理模块功能（已禁用，无需配置） */
/* #define CONFIG_ERROR_HANDLER_STATS_EN       1   */

/* 日志模块功能（已禁用，无需配置） */
/* #define CONFIG_LOG_LEVEL                    2   */
/* #define CONFIG_LOG_TIMESTAMP_EN             0   */
/* #define CONFIG_LOG_MODULE_EN                1   */
/* #define CONFIG_LOG_COLOR_EN                 0   */

/* 独立看门狗模块功能（已禁用，无需配置） */
/* #define CONFIG_IWDG_TIMEOUT_MS              1000  */

/* 系统监控模块功能（已禁用，无需配置） */
/* #define CONFIG_SYSTEM_MONITOR_CHECK_INTERVAL 1000  */
/* #define CONFIG_SYSTEM_MONITOR_LOG_INTERVAL   5000  */
/* #define CONFIG_SYSTEM_MONITOR_CPU_THRESHOLD  80    */
/* #define CONFIG_SYSTEM_MONITOR_HEAP_THRESHOLD  1024  */

#endif /* EXAMPLE_CONFIG_H */





