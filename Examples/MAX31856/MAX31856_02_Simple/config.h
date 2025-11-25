/**
 * @file config.h
 * @brief 模块配置 - 最简单的MAX31856案例
 */

#ifndef EXAMPLE_CONFIG_H
#define EXAMPLE_CONFIG_H

/* ==================== 模块开关 ==================== */

/* 驱动层模块 */
#define CONFIG_MODULE_GPIO_ENABLED          1   /**< GPIO模块开关 - 必须（LED、OLED、SPI依赖） */
#define CONFIG_MODULE_LED_ENABLED          1   /**< LED模块开关 - 必须（系统状态指示） */
#define CONFIG_MODULE_OLED_ENABLED         1   /**< OLED模块开关 - 必须（显示温度） */
#define CONFIG_MODULE_SPI_ENABLED           1   /**< SPI模块开关 - 必须（MAX31856使用硬件SPI） */
#define CONFIG_MODULE_SOFT_I2C_ENABLED     1   /**< 软件I2C模块开关 - 必须（OLED使用） */
#define CONFIG_MODULE_MAX31856_ENABLED     1   /**< MAX31856模块开关 - 必须（本案例核心功能） */

/* 系统层模块 */
#define CONFIG_MODULE_DELAY_ENABLED        1   /**< 延时模块开关 - 必须（案例使用） */
#define CONFIG_MODULE_BASE_TIMER_ENABLED   1   /**< 基时定时器模块开关 - 必须（delay依赖） */
#define CONFIG_MODULE_SYSTEM_INIT_ENABLED   1   /**< 系统初始化模块开关 - 必须（案例使用） */

/* 通用模块 */
#define CONFIG_MODULE_ERROR_HANDLER_ENABLED 1   /**< 错误处理模块开关 - 必须（案例使用） */
#define CONFIG_MODULE_LOG_ENABLED          1   /**< 日志模块开关 - 必须（案例使用） */
#define CONFIG_MODULE_MODULE_CTRL_ENABLED   0   /**< 模块开关总控开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_SYSTEM_MONITOR_ENABLED 0  /**< 系统监控模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_IWDG_ENABLED         0   /**< 独立看门狗模块开关 - 禁用（案例不使用） */

/* 其他模块（禁用） */
#define CONFIG_MODULE_SOFT_SPI_ENABLED     0   /**< 软件SPI模块开关 - 禁用（案例使用硬件SPI） */
#define CONFIG_MODULE_I2C_ENABLED          0   /**< 硬件I2C模块开关 - 禁用（案例使用软件I2C） */
#define CONFIG_MODULE_DS3231_ENABLED       0   /**< DS3231模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_EEPROM_ENABLED       0   /**< EEPROM模块开关 - 禁用（案例不使用） */

#endif /* EXAMPLE_CONFIG_H */

