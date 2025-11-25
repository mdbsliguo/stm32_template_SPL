/**
 * @file config.h
 * @brief ClockManager01_AdaptiveFrequency案例专用配置（独立工程）
 * @details 只启用案例需要的模块，禁用其他模块以减小代码体积
 */

#ifndef CONFIG_H
#define CONFIG_H

/* ==================== 模块开关 ==================== */

/* 驱动层模块 */
#define CONFIG_MODULE_GPIO_ENABLED          1   /**< GPIO模块开关 - 必须（UART、LED、OLED依赖） */
#define CONFIG_MODULE_UART_ENABLED         1   /**< UART模块开关 - 必须（串口调试，新项目必须） */
#define CONFIG_MODULE_LED_ENABLED           1   /**< LED模块开关 - 必须（案例使用） */
#define CONFIG_MODULE_OLED_ENABLED         1   /**< OLED模块开关 - 必须（案例使用） */
#define CONFIG_MODULE_SOFT_I2C_ENABLED   1   /**< 软件I2C模块开关 - 必须（OLED使用） */

/* 系统层模块 */
#define CONFIG_MODULE_DELAY_ENABLED        1   /**< 延时模块开关 - 必须（案例使用） */
#define CONFIG_MODULE_BASE_TIMER_ENABLED   1   /**< 基时定时器模块开关 - 必须（delay依赖） */
#define CONFIG_MODULE_CLOCK_MANAGER_ENABLED 1   /**< 时钟管理模块开关 - 必须（案例使用） */
#define CONFIG_MODULE_SYSTEM_INIT_ENABLED   1   /**< 系统初始化模块开关 - 必须（案例使用） */
#define CONFIG_MODULE_IWDG_ENABLED         0   /**< 独立看门狗模块开关 - 禁用（案例不使用） */

/* 通用模块 */
#define CONFIG_MODULE_ERROR_HANDLER_ENABLED 1   /**< 错误处理模块开关 - 必须（错误处理，新项目必须） */
#define CONFIG_MODULE_LOG_ENABLED           1   /**< 日志模块开关 - 必须（日志输出，新项目必须） */
#define CONFIG_MODULE_MODULE_CTRL_ENABLED   0   /**< 模块开关总控开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_SYSTEM_MONITOR_ENABLED 0  /**< 系统监控模块开关 - 禁用（案例不使用） */

/* ==================== 功能开关 ==================== */

/* 错误处理模块功能（已禁用，无需配置） */
/* #define CONFIG_ERROR_HANDLER_STATS_EN       1   */

/* 日志模块功能 */
#define CONFIG_LOG_LEVEL                    0   /**< 日志级别：0=DEBUG（显示所有日志） */
#define CONFIG_LOG_TIMESTAMP_EN             0   /**< 时间戳功能开关 - 禁用（简化输出） */
#define CONFIG_LOG_MODULE_EN                1   /**< 模块名显示开关 - 启用（案例演示） */
#define CONFIG_LOG_COLOR_EN                 0   /**< 颜色输出开关 - 禁用（串口助手可能不支持） */

/* 独立看门狗模块功能（已禁用，无需配置） */
/* #define CONFIG_IWDG_TIMEOUT_MS              1000  */

/* 系统监控模块功能（已禁用，无需配置） */
/* #define CONFIG_SYSTEM_MONITOR_CHECK_INTERVAL 1000  */
/* #define CONFIG_SYSTEM_MONITOR_LOG_INTERVAL   5000  */
/* #define CONFIG_SYSTEM_MONITOR_CPU_THRESHOLD  80    */
/* #define CONFIG_SYSTEM_MONITOR_HEAP_THRESHOLD  1024  */

#endif /* CONFIG_H */





