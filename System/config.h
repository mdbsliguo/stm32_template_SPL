/**
 * @file config.h
 * @brief 系统模块配置统一管理
 * @version 1.0.0
 * @date 2024-01-01
 * @details 所有模块的开关和功能配置在此统一管理，避免条件编译失控
 * 
 * @note 使用原则：
 * - 所有模块开关以 CONFIG_MODULE_XXX_ENABLED 命名
 * - 所有功能开关以 CONFIG_XXX_YYY_EN 命名
 * - 值为1表示启用，0表示禁用
 * - 未定义的宏视为禁用（通过 #ifdef 检查）
 */

#ifndef CONFIG_H
#define CONFIG_H

/* ==================== 模块开关 ==================== */

/* 驱动层模块 */
#define CONFIG_MODULE_GPIO_ENABLED          1   /**< GPIO模块开关 */
#define CONFIG_MODULE_LED_ENABLED           1   /**< LED模块开关 */
#define CONFIG_MODULE_BUZZER_ENABLED        1   /**< Buzzer模块开关 */
#define CONFIG_MODULE_OLED_ENABLED         1   /**< OLED模块开关 */
#define CONFIG_MODULE_I2C_ENABLED          1   /**< I2C模块开关 */
#define CONFIG_MODULE_DS3231_ENABLED      1   /**< DS3231模块开关 */
#define CONFIG_MODULE_MAX31856_ENABLED   1   /**< MAX31856模块开关 */
#define CONFIG_MODULE_SOFT_I2C_ENABLED   1   /**< 软件I2C模块开关 */
#define CONFIG_MODULE_SPI_ENABLED         1   /**< SPI模块开关 */
#define CONFIG_MODULE_SOFT_SPI_ENABLED   1   /**< 软件SPI模块开关 */
#define CONFIG_MODULE_UART_ENABLED       1   /**< UART模块开关 */
#define CONFIG_MODULE_TIMER_ENABLED      1   /**< 定时器模块开关 */
#define CONFIG_MODULE_CAN_ENABLED       1   /**< CAN模块开关 */
#define CONFIG_MODULE_ADC_ENABLED       1   /**< ADC模块开关 */
#define CONFIG_MODULE_DMA_ENABLED       1   /**< DMA模块开关 */
#define CONFIG_MODULE_EXTI_ENABLED     1   /**< EXTI模块开关 */
#define CONFIG_MODULE_NVIC_ENABLED     1   /**< NVIC模块开关 */
#define CONFIG_MODULE_DAC_ENABLED      1   /**< DAC模块开关（仅HD/CL/HD_VL/MD_VL型号支持） */
#define CONFIG_MODULE_USB_ENABLED      1   /**< USB模块开关（仅MD/HD/CL/XL型号支持，需要外部48MHz晶振） */
#define CONFIG_MODULE_RTC_ENABLED      1   /**< RTC模块开关（内部实时时钟，需要配置时钟源） */
#define CONFIG_MODULE_WWDG_ENABLED      1   /**< WWDG模块开关（窗口看门狗） */
#define CONFIG_MODULE_BKP_ENABLED      1   /**< BKP模块开关（备份寄存器） */
#define CONFIG_MODULE_PWR_ENABLED      1   /**< PWR模块开关（低功耗模式） */
#define CONFIG_MODULE_FLASH_ENABLED    1   /**< FLASH模块开关（Flash编程/擦除） */
#define CONFIG_MODULE_CRC_ENABLED      1   /**< CRC模块开关（CRC32计算） */
#define CONFIG_MODULE_DBGMCU_ENABLED   1   /**< DBGMCU模块开关（调试模式配置） */
#define CONFIG_MODULE_SDIO_ENABLED     1   /**< SDIO模块开关（SD卡接口，需要外部SD卡槽） */
#define CONFIG_MODULE_FSMC_ENABLED    1   /**< FSMC模块开关（外部存储器接口，需要外部存储器） */
#define CONFIG_MODULE_CEC_ENABLED     1   /**< CEC模块开关（消费电子控制协议，仅CL型号） */
#define CONFIG_MODULE_TB6612_ENABLED  1   /**< TB6612模块开关（双路直流电机驱动） */

/* 系统层模块 */
#define CONFIG_MODULE_DELAY_ENABLED        1   /**< 延时模块开关 */
#define CONFIG_MODULE_BASE_TIMER_ENABLED   1   /**< TIM2时间基准模块开关 */
#define CONFIG_MODULE_TIM_SW_ENABLED       1   /**< 软件定时器模块开关 */
#define CONFIG_MODULE_CLOCK_MANAGER_ENABLED 1   /**< 时钟管理模块开关 */
#define CONFIG_MODULE_SYSTEM_INIT_ENABLED   1   /**< 系统初始化模块开关 */
#define CONFIG_MODULE_IWDG_ENABLED         1   /**< 独立看门狗模块开关 */

/* 通用模块 */
#define CONFIG_MODULE_ERROR_HANDLER_ENABLED 1   /**< 错误处理模块开关 */
#define CONFIG_MODULE_LOG_ENABLED           1   /**< 日志模块开关 */
#define CONFIG_MODULE_MODULE_CTRL_ENABLED   1   /**< 模块开关总控开关 */
#define CONFIG_MODULE_SYSTEM_MONITOR_ENABLED 1  /**< 系统监控模块开关 */

/* ==================== 功能开关 ==================== */

/* 错误处理模块功能 */
#define CONFIG_ERROR_HANDLER_STATS_EN       1   /**< 错误统计功能开关 */
/* 注意：如果启用统计功能，需要在 error_handler.c 中定义 ERROR_HANDLER_ENABLE_STATS */
/* 建议：在 error_handler.c 中统一处理：
 *   #ifdef CONFIG_ERROR_HANDLER_STATS_EN
 *   #define ERROR_HANDLER_ENABLE_STATS
 *   #endif
 */

/* 日志模块功能 */
#define CONFIG_LOG_LEVEL                    2   /**< 日志级别：0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=NONE */
#define CONFIG_LOG_TIMESTAMP_EN             0   /**< 时间戳功能开关（需要TIM2_TimeBase模块） */
#define CONFIG_LOG_MODULE_EN                1   /**< 模块名显示开关 */
#define CONFIG_LOG_COLOR_EN                 0   /**< 颜色输出开关（需要终端支持ANSI转义码） */

/* 独立看门狗模块功能 */
#define CONFIG_IWDG_TIMEOUT_MS              1000  /**< 默认超时时间（毫秒），范围：1ms ~ 32768ms */

/* 系统监控模块功能 */
#define CONFIG_SYSTEM_MONITOR_CHECK_INTERVAL 1000  /**< 监控检查间隔（毫秒），默认1秒 */
#define CONFIG_SYSTEM_MONITOR_LOG_INTERVAL   5000  /**< 日志输出间隔（毫秒），默认5秒 */
#define CONFIG_SYSTEM_MONITOR_CPU_THRESHOLD  80    /**< CPU使用率告警阈值（%），超过此值告警 */
#define CONFIG_SYSTEM_MONITOR_HEAP_THRESHOLD  1024  /**< 堆内存告警阈值（字节），低于此值告警 */
#define CONFIG_STACK_SIZE                    0x400 /**< 栈总大小（字节），默认1KB，需与startup.s中的Stack_Size一致 */

/* ==================== 未来扩展预留 ==================== */

/* RTOS支持（预留） */
/* #define CONFIG_RTOS_ENABLED             1   */  /**< RTOS支持开关 */
/* #define CONFIG_RTOS_FREERTOS            1   */  /**< FreeRTOS支持 */

/* 断言系统（预留） */
/* #define CONFIG_ASSERT_ENABLED           1   */  /**< 断言系统开关 */

#endif /* CONFIG_H */

