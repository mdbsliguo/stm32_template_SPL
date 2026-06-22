/**
 * @file config.h
 * @brief 模块开关配置文件（NPN05_Preset_Pump_HwAlgo 独立工程专用）
 * @details Bus04 ModBus + OGM TIM4 双边沿捕获四边沿互锁 + PA6 分档测试（2000cnt/档）
 */

#ifndef EXAMPLE_CONFIG_H
#define EXAMPLE_CONFIG_H

#define CONFIG_MODULE_SYSTEM_INIT_ENABLED   1
#define CONFIG_MODULE_GPIO_ENABLED          1
#define CONFIG_MODULE_DELAY_ENABLED         1
#define CONFIG_MODULE_BASE_TIMER_ENABLED    1

#define CONFIG_MODULE_LED_ENABLED           1
#define CONFIG_MODULE_OLED_ENABLED          1
#define CONFIG_MODULE_SOFT_I2C_ENABLED      1
#define CONFIG_MODULE_UART_ENABLED          1
#define CONFIG_MODULE_MODBUS_RTU_ENABLED    1
#define CONFIG_MODULE_TIMER_ENABLED         1
#define CONFIG_MODULE_OGM_FLOW_IC_ENABLED   1

#include "board.h"

#define CONFIG_MODULE_ERROR_HANDLER_ENABLED 1
#define CONFIG_MODULE_LOG_ENABLED           1

#define CONFIG_MODULE_EXTI_ENABLED          0
#define CONFIG_MODULE_I2C_ENABLED           0
#define CONFIG_MODULE_SPI_ENABLED           0
#define CONFIG_MODULE_SOFT_SPI_ENABLED      0
#define CONFIG_MODULE_CAN_ENABLED           0
#define CONFIG_MODULE_ADC_ENABLED           0
#define CONFIG_MODULE_DMA_ENABLED           0
#define CONFIG_MODULE_NVIC_ENABLED          1
#define CONFIG_MODULE_DAC_ENABLED           0
#define CONFIG_MODULE_USB_ENABLED           0
#define CONFIG_MODULE_RTC_ENABLED           0
#define CONFIG_MODULE_CLOCK_MANAGER_ENABLED 0
#define CONFIG_MODULE_IWDG_ENABLED          0
#define CONFIG_MODULE_MODULE_CTRL_ENABLED   0
#define CONFIG_MODULE_SYSTEM_MONITOR_ENABLED 0
#define CONFIG_MODULE_TIM_SW_ENABLED        0

#define CONFIG_ERROR_HANDLER_STATS_EN       1
#define CONFIG_LOG_LEVEL                    0
#define CONFIG_LOG_TIMESTAMP_EN             0
#define CONFIG_LOG_MODULE_EN                1
#define CONFIG_LOG_COLOR_EN                 0

#define CONFIG_OGM_FLOW_IC_TEST_INJECT      0

/** 1=TIM 双边沿捕获 + 四边沿互锁（同 NPN03）；0=仅下降沿 A/B 交替 */
#define CONFIG_OGM_FLOW_IC_ALGO_FOUR_EDGE   1

#endif /* EXAMPLE_CONFIG_H */
