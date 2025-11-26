/**
 * @file error_code.h
 * @brief 全局错误码定义
 * @details 定义标准错误码类型与模块基值，所有模块错误码枚举应基于此文件
 */

#ifndef ERROR_CODE_H
#define ERROR_CODE_H

#include <stdint.h>

/**
 * @brief 标准错误码类型（所有模块必须返回此类型）
 */
typedef int32_t error_code_t;

/**
 * @brief 操作成功返回值
 */
#define ERROR_OK 0

/**
 * @brief 功能未实现错误码（占位空函数）
 * @note 占位空函数应返回此错误码或模块特定的NOT_IMPLEMENTED错误码
 * @note 禁止占位空函数返回成功码（ERROR_OK），必须返回错误码
 */
#define ERROR_NOT_IMPLEMENTED -9999

/**
 * @defgroup Error_Base 模块错误码基值
 * @brief 各模块错误码枚举值应在对应基值上递减定义
 * @note  每个模块预留100个错误码空间
 * @{
 */
#define ERROR_BASE_OLED -100         /**< OLED模块错误码基值 */
#define ERROR_BASE_SYSTICK -200      /**< SYSTICK模块错误码基值 */
#define ERROR_BASE_GPIO -300         /**< GPIO模块错误码基值 */
#define ERROR_BASE_LED -400           /**< LED模块错误码基值 */
#define ERROR_BASE_SYSTEM -500       /**< 系统初始化模块错误码基值 */
#define ERROR_BASE_CLOCK_MANAGER -600 /**< 时钟管理模块错误码基值 */
#define ERROR_BASE_DELAY -700        /**< 延时模块错误码基值 */
#define ERROR_BASE_BASE_TIMER -800   /**< 基时定时器模块错误码基值 */
#define ERROR_BASE_UART -900         /**< UART模块错误码基值 */
#define ERROR_BASE_I2C -950          /**< I2C模块错误码基值 */
#define ERROR_BASE_TIMER -1000       /**< 定时器模块错误码基值 */
#define ERROR_BASE_ADC -1100          /**< ADC模块错误码基值 */
#define ERROR_BASE_LOG -1200          /**< 日志模块错误码基值 */
#define ERROR_BASE_IWDG -1300         /**< 独立看门狗模块错误码基值 */
#define ERROR_BASE_MODULE_CTRL -1400  /**< 模块开关总控错误码基值 */
#define ERROR_BASE_SYSTEM_MONITOR -1500  /**< 系统监控模块错误码基值 */
#define ERROR_BASE_DS3231 -1600          /**< DS3231模块错误码基值 */
#define ERROR_BASE_SOFT_I2C -1700        /**< 软件I2C模块错误码基值 */
#define ERROR_BASE_SPI -1800             /**< SPI模块错误码基值 */
#define ERROR_BASE_SOFT_SPI -1900        /**< 软件SPI模块错误码基值 */
#define ERROR_BASE_CAN -2000             /**< CAN模块错误码基值 */
#define ERROR_BASE_DAC -2100             /**< DAC模块错误码基值 */
#define ERROR_BASE_NVIC -2200            /**< NVIC模块错误码基值 */
#define ERROR_BASE_EXTI -2300            /**< EXTI模块错误码基值 */
#define ERROR_BASE_RTC -2400             /**< RTC模块错误码基值 */
#define ERROR_BASE_DMA -2500             /**< DMA模块错误码基值 */
#define ERROR_BASE_USB -2600             /**< USB模块错误码基值 */
#define ERROR_BASE_FLASH -2700            /**< FLASH模块错误码基值 */
#define ERROR_BASE_CRC -2800              /**< CRC模块错误码基值 */
#define ERROR_BASE_DBGMCU -2900           /**< DBGMCU模块错误码基值 */
#define ERROR_BASE_SDIO -3000              /**< SDIO模块错误码基值 */
#define ERROR_BASE_FSMC -3100             /**< FSMC模块错误码基值 */
#define ERROR_BASE_CEC -3200              /**< CEC模块错误码基值 */
#define ERROR_BASE_MAX31856 -3300         /**< MAX31856模块错误码基值 */
#define ERROR_BASE_BUZZER -3400            /**< Buzzer模块错误码基值 */
#define ERROR_BASE_TB6612 -3500            /**< TB6612模块错误码基值 */
/** @} */

#endif /* ERROR_CODE_H */
