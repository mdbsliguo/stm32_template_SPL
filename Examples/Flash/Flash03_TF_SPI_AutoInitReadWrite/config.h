/**
 * @file config.h
 * @brief Flash03专用配置（独立工程）
 * @details 只启用Flash03需要的模块，禁用其他模块以减小代码体积
 */

#ifndef EXAMPLE_CONFIG_H
#define EXAMPLE_CONFIG_H

/* ==================== 模块开关 ==================== */

/* 驱动层模块 */
#define CONFIG_MODULE_GPIO_ENABLED          1   /**< GPIO模块开关 - 必须（LED、SPI、软件I2C、OLED依赖） */
#define CONFIG_MODULE_LED_ENABLED           1   /**< LED模块开关 - 必须（Flash03使用） */
#define CONFIG_MODULE_OLED_ENABLED         1   /**< OLED模块开关 - 必须（Flash03使用，默认显示器） */
#define CONFIG_MODULE_SPI_ENABLED           1   /**< SPI模块开关 - 必须（TF_SPI使用SPI2） */
#define CONFIG_MODULE_TF_SPI_ENABLED        1   /**< TF_SPI模块开关 - 必须（Flash03使用） */
#define CONFIG_MODULE_UART_ENABLED          1   /**< UART模块开关 - 必须（Flash03使用，详细日志输出） */
#define CONFIG_MODULE_SOFT_I2C_ENABLED     1   /**< 软件I2C模块开关 - 必须（OLED使用软件I2C） */
#define CONFIG_MODULE_I2C_ENABLED           0   /**< 硬件I2C模块开关 - 禁用（Flash03使用软件I2C） */

/* 系统层模块 */
#define CONFIG_MODULE_DELAY_ENABLED        1   /**< 延时模块开关 - 必须（Flash03使用） */
#define CONFIG_MODULE_BASE_TIMER_ENABLED   1   /**< 基时定时器模块开关 - 必须（delay依赖） */
#define CONFIG_MODULE_SYSTEM_INIT_ENABLED   1   /**< 系统初始化模块开关 - 必须（Flash03使用） */
#define CONFIG_MODULE_IWDG_ENABLED         0   /**< 独立看门狗模块开关 - 禁用（Flash03不使用） */

/* 通用模块 */
#define CONFIG_MODULE_ERROR_HANDLER_ENABLED 1   /**< 错误处理模块开关 - 必须（Flash03使用） */
#define CONFIG_MODULE_LOG_ENABLED           1   /**< 日志模块开关 - 必须（Flash03使用） */
#define CONFIG_MODULE_MODULE_CTRL_ENABLED   0   /**< 模块开关总控开关 - 禁用（Flash03不使用） */

/* ==================== 功能开关 ==================== */

/* 错误处理模块功能 */
#define CONFIG_ERROR_HANDLER_STATS_EN       1   /**< 错误统计功能开关 */

/* 日志模块功能 */
#define CONFIG_LOG_LEVEL                    1   /**< 日志级别：0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=NONE */
#define CONFIG_LOG_TIMESTAMP_EN             0   /**< 时间戳功能开关（需要TIM2_TimeBase模块） */
#define CONFIG_LOG_MODULE_EN                1   /**< 模块名显示开关 */
#define CONFIG_LOG_COLOR_EN                 0   /**< 颜色输出开关（需要终端支持ANSI转义码） */

/* TF_SPI模块调试开关 */
#define TF_SPI_DEBUG_ENABLED                0   /**< TF_SPI调试输出开关 - 关闭详细初始化过程日志 */

#endif /* EXAMPLE_CONFIG_H */

