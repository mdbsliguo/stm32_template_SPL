/**
 * @file config.h
 * @brief PWM06_BTS7960_CarAirPumpMotor案例专用配置（独立工程）
 * @details 只启用案例需要的模块，禁用其他模块以减小代码体积
 * 
 * 案例功能：通过旋转编码器和按键手动调节BTS7960电机驱动的PWM占空比和方向
 * 使用硬件：OLED、串口、旋钮编码器、按钮、LED1、BTS7960驱动
 */

#ifndef EXAMPLE_CONFIG_H
#define EXAMPLE_CONFIG_H

/* ==================== 模块开关 ==================== */

/* 驱动层模块 */
#define CONFIG_MODULE_GPIO_ENABLED          1   /**< GPIO模块开关 - 必须（所有模块依赖） */
#define CONFIG_MODULE_UART_ENABLED         1   /**< UART模块开关 - 必须（串口调试，新项目必须） */
#define CONFIG_MODULE_LED_ENABLED           1   /**< LED模块开关 - 必须（LED1状态指示） */
#define CONFIG_MODULE_OLED_ENABLED         1   /**< OLED模块开关 - 启用（参数显示） */
#define CONFIG_MODULE_SOFT_I2C_ENABLED    1   /**< 软件I2C模块开关 - 启用（OLED需要） */
#define CONFIG_MODULE_TIMER_ENABLED      1   /**< 定时器模块开关 - 必须（PWM和编码器接口依赖） */
#define CONFIG_MODULE_BTS7960_ENABLED      1   /**< BTS7960模块开关 - 必须（案例使用） */
#define CONFIG_MODULE_EXTI_ENABLED       1   /**< EXTI模块开关 - 必须（按钮中断） */
#define CONFIG_MODULE_NVIC_ENABLED       1   /**< NVIC模块开关 - 必须（编码器中断需要） */

/* 系统层模块 */
#define CONFIG_MODULE_DELAY_ENABLED        1   /**< 延时模块开关 - 必须（案例使用） */
#define CONFIG_MODULE_BASE_TIMER_ENABLED   1   /**< TIM2时间基准模块开关 - 必须（delay依赖） */
#define CONFIG_MODULE_SYSTEM_INIT_ENABLED   1   /**< 系统初始化模块开关 - 必须（案例使用） */

/* 通用模块 */
#define CONFIG_MODULE_ERROR_HANDLER_ENABLED 1   /**< 错误处理模块开关 - 必须（错误处理，新项目必须） */
#define CONFIG_MODULE_LOG_ENABLED           1   /**< 日志模块开关 - 必须（日志输出，新项目必须） */

/* ==================== 功能开关 ==================== */

/* 日志模块功能（如果启用） */
#define CONFIG_LOG_LEVEL                    0   /**< 日志级别：0=DEBUG（显示所有日志） */
#define CONFIG_LOG_TIMESTAMP_EN             0   /**< 时间戳功能开关 - 禁用（简化输出） */
#define CONFIG_LOG_MODULE_EN                1   /**< 模块名显示开关 - 启用（案例演示） */
#define CONFIG_LOG_COLOR_EN                 0   /**< 颜色输出开关 - 禁用（串口助手可能不支持） */

#endif /* EXAMPLE_CONFIG_H */


