/**
 * @file config.h
 * @brief 模块配置 - 最简单的MAX31856案例
 */

#ifndef CONFIG_H
#define CONFIG_H

/* ==================== 模块开关 ==================== */

/* 启用SPI模块 */
#define CONFIG_MODULE_SPI_ENABLED           1

/* 启用MAX31856模块 */
#define CONFIG_MODULE_MAX31856_ENABLED     1

/* 启用LED模块 */
#define CONFIG_MODULE_LED_ENABLED          1

/* 启用OLED模块 */
#define CONFIG_MODULE_OLED_ENABLED         1

/* 启用延时模块 */
#define CONFIG_MODULE_DELAY_ENABLED        1

/* 启用错误处理模块 */
#define CONFIG_MODULE_ERROR_HANDLER_ENABLED 1

/* 启用日志模块 */
#define CONFIG_MODULE_LOG_ENABLED          1

/* 禁用不需要的模块 */
#define CONFIG_MODULE_SOFT_SPI_ENABLED     0
#define CONFIG_MODULE_SOFT_I2C_ENABLED     1
#define CONFIG_MODULE_I2C_ENABLED          0
#define CONFIG_MODULE_DS3231_ENABLED       0
#define CONFIG_MODULE_EEPROM_ENABLED       0

#endif /* CONFIG_H */

