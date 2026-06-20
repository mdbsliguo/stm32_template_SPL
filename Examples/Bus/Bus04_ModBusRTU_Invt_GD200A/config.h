/**
 * @file config.h
 * @brief Bus04案例专用配置（独立工程）
 * @details 只启用Bus04案例需要的模块，禁用其他模块以减小代码体积
 */

#ifndef EXAMPLE_CONFIG_H
#define EXAMPLE_CONFIG_H

/* ==================== 模块开关 ==================== */

#define CONFIG_MODULE_GPIO_ENABLED          1
#define CONFIG_MODULE_UART_ENABLED          1
#define CONFIG_MODULE_LED_ENABLED           0
#define CONFIG_MODULE_OLED_ENABLED          1
#define CONFIG_MODULE_I2C_ENABLED           0
#define CONFIG_MODULE_SOFT_I2C_ENABLED      1
#define CONFIG_MODULE_SPI_ENABLED           0
#define CONFIG_MODULE_SOFT_SPI_ENABLED      0
#define CONFIG_MODULE_TIMER_ENABLED         0
#define CONFIG_MODULE_CAN_ENABLED           0
#define CONFIG_MODULE_ADC_ENABLED           0
#define CONFIG_MODULE_DMA_ENABLED           0
#define CONFIG_MODULE_EXTI_ENABLED          0
#define CONFIG_MODULE_NVIC_ENABLED          0
#define CONFIG_MODULE_DAC_ENABLED           0
#define CONFIG_MODULE_USB_ENABLED           0
#define CONFIG_MODULE_RTC_ENABLED           0
#define CONFIG_MODULE_WWDG_ENABLED          0
#define CONFIG_MODULE_BKP_ENABLED           0
#define CONFIG_MODULE_PWR_ENABLED           0
#define CONFIG_MODULE_FLASH_ENABLED         0
#define CONFIG_MODULE_CRC_ENABLED           0
#define CONFIG_MODULE_DBGMCU_ENABLED        0
#define CONFIG_MODULE_SDIO_ENABLED          0
#define CONFIG_MODULE_FSMC_ENABLED          0
#define CONFIG_MODULE_CEC_ENABLED           0

#define CONFIG_MODULE_DELAY_ENABLED         1
#define CONFIG_MODULE_BASE_TIMER_ENABLED    1
#define CONFIG_MODULE_TIM_SW_ENABLED        0
#define CONFIG_MODULE_CLOCK_MANAGER_ENABLED 0
#define CONFIG_MODULE_SYSTEM_INIT_ENABLED   1
#define CONFIG_MODULE_IWDG_ENABLED          0

#define CONFIG_MODULE_ERROR_HANDLER_ENABLED 1
#define CONFIG_MODULE_LOG_ENABLED           1
#define CONFIG_MODULE_MODULE_CTRL_ENABLED   0
#define CONFIG_MODULE_SYSTEM_MONITOR_ENABLED 0

#define CONFIG_MODULE_MODBUS_RTU_ENABLED    1

/* ==================== 功能开关 ==================== */

#define CONFIG_ERROR_HANDLER_STATS_EN       1
#define CONFIG_LOG_LEVEL                    0
#define CONFIG_LOG_TIMESTAMP_EN             0
#define CONFIG_LOG_MODULE_EN                1
#define CONFIG_LOG_COLOR_EN                 0

#endif /* EXAMPLE_CONFIG_H */
