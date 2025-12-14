/**
 * @file config.h
 * @brief Bus02案例专用配置（独立工程）
 * @details 只启用Bus02案例需要的模块，禁用其他模块以减小代码体积
 */

#ifndef EXAMPLE_CONFIG_H
#define EXAMPLE_CONFIG_H

/* ==================== 模块开关 ==================== */

/* 驱动层模块 */
#define CONFIG_MODULE_GPIO_ENABLED          1   /**< GPIO模块开关 - 必须（UART依赖） */
#define CONFIG_MODULE_UART_ENABLED          1   /**< UART模块开关 - 必须（案例使用） */
#define CONFIG_MODULE_LED_ENABLED           0   /**< LED模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_OLED_ENABLED          1   /**< OLED模块开关 - 启用（规范要求） */
#define CONFIG_MODULE_I2C_ENABLED           0   /**< I2C模块开关 - 禁用（OLED使用软I2C） */
#define CONFIG_MODULE_SOFT_I2C_ENABLED      1   /**< 软件I2C模块开关 - 启用（OLED需要） */
#define CONFIG_MODULE_SPI_ENABLED           0   /**< SPI模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_SOFT_SPI_ENABLED      0   /**< 软件SPI模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_TIMER_ENABLED         0   /**< 定时器模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_CAN_ENABLED          0   /**< CAN模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_ADC_ENABLED          0   /**< ADC模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_DMA_ENABLED          0   /**< DMA模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_EXTI_ENABLED         0   /**< EXTI模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_NVIC_ENABLED         0   /**< NVIC模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_DAC_ENABLED          0   /**< DAC模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_USB_ENABLED          0   /**< USB模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_RTC_ENABLED          0   /**< RTC模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_WWDG_ENABLED         0   /**< WWDG模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_BKP_ENABLED          0   /**< BKP模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_PWR_ENABLED          0   /**< PWR模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_FLASH_ENABLED        0   /**< FLASH模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_CRC_ENABLED          0   /**< CRC模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_DBGMCU_ENABLED       0   /**< DBGMCU模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_SDIO_ENABLED         0   /**< SDIO模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_FSMC_ENABLED         0   /**< FSMC模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_CEC_ENABLED          0   /**< CEC模块开关 - 禁用（案例不使用） */

/* 系统层模块 */
#define CONFIG_MODULE_DELAY_ENABLED        1   /**< 延时模块开关 - 必须（案例使用） */
#define CONFIG_MODULE_BASE_TIMER_ENABLED   1   /**< TIM2时间基准模块开关 - 必须（delay依赖） */
#define CONFIG_MODULE_TIM_SW_ENABLED       0   /**< 软件定时器模块开关 - 禁用（不再使用） */
#define CONFIG_MODULE_CLOCK_MANAGER_ENABLED 0  /**< 时钟管理模块开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_SYSTEM_INIT_ENABLED   1   /**< 系统初始化模块开关 - 必须（案例使用） */
#define CONFIG_MODULE_IWDG_ENABLED         0   /**< 独立看门狗模块开关 - 禁用（案例不使用） */

/* 通用模块 */
#define CONFIG_MODULE_ERROR_HANDLER_ENABLED 1   /**< 错误处理模块开关 - 必须（案例使用） */
#define CONFIG_MODULE_LOG_ENABLED           1   /**< 日志模块开关 - 必须（案例使用） */
#define CONFIG_MODULE_MODULE_CTRL_ENABLED   0   /**< 模块开关总控开关 - 禁用（案例不使用） */
#define CONFIG_MODULE_SYSTEM_MONITOR_ENABLED 0  /**< 系统监控模块开关 - 禁用（案例不使用） */

/* 中间件层模块 */
#define CONFIG_MODULE_MODBUS_RTU_ENABLED    1   /**< ModBusRTU协议栈模块开关 - 必须（案例使用） */

/* ==================== 功能开关 ==================== */

/* 错误处理模块功能 */
#define CONFIG_ERROR_HANDLER_STATS_EN       1   /**< 错误统计功能开关 - 启用（案例演示） */

/* 日志模块功能 */
#define CONFIG_LOG_LEVEL                    0   /**< 日志级别：0=DEBUG（显示所有日志） */
#define CONFIG_LOG_TIMESTAMP_EN             0   /**< 时间戳功能开关 - 禁用（简化输出） */
#define CONFIG_LOG_MODULE_EN                1   /**< 模块名显示开关 - 启用（案例演示） */
#define CONFIG_LOG_COLOR_EN                 0   /**< 颜色输出开关 - 禁用（串口助手可能不支持） */

#endif /* EXAMPLE_CONFIG_H */
