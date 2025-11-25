/**
 * @file rtc.h
 * @brief RTC实时时钟模块
 * @details 提供STM32内部RTC实时时钟功能，时间读取/设置，闹钟功能
 * @note RTC使用备份域，需要配置时钟源（LSE/LSI/HSE_Div128）
 */

#ifndef RTC_H
#define RTC_H

#include "error_code.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RTC状态枚举
 */
typedef enum {
    RTC_OK = ERROR_OK,                                    /**< 操作成功 */
    RTC_ERROR_NOT_INITIALIZED = ERROR_BASE_RTC - 1,      /**< 未初始化 */
    RTC_ERROR_INVALID_PARAM = ERROR_BASE_RTC - 2,        /**< 参数错误 */
    RTC_ERROR_INVALID_TIME = ERROR_BASE_RTC - 3,         /**< 无效的时间值 */
    RTC_ERROR_BUSY = ERROR_BASE_RTC - 4,                 /**< RTC忙 */
    RTC_ERROR_TIMEOUT = ERROR_BASE_RTC - 5,              /**< 操作超时 */
    RTC_ERROR_ALREADY_INITIALIZED = ERROR_BASE_RTC - 6,  /**< 已初始化 */
} RTC_Status_t;

/**
 * @brief RTC时钟源枚举
 */
typedef enum {
    RTC_CLOCK_SOURCE_LSE = 0,        /**< LSE（外部32.768kHz晶振） */
    RTC_CLOCK_SOURCE_LSI = 1,        /**< LSI（内部32kHz RC振荡器） */
    RTC_CLOCK_SOURCE_HSE_DIV128 = 2, /**< HSE/128（外部高速时钟除以128） */
} RTC_ClockSource_t;

/**
 * @brief RTC时间结构体
 */
typedef struct {
    uint8_t second;     /**< 秒（0-59） */
    uint8_t minute;     /**< 分（0-59） */
    uint8_t hour;       /**< 时（0-23） */
    uint8_t day;        /**< 日（1-31） */
    uint8_t month;      /**< 月（1-12） */
    uint16_t year;      /**< 年（2000-2099） */
    uint8_t weekday;   /**< 星期（0=周日，1=周一，...，6=周六） */
} RTC_Time_t;

/**
 * @brief RTC闹钟回调函数类型
 * @param user_data 用户数据指针
 */
typedef void (*RTC_AlarmCallback_t)(void *user_data);

/**
 * @brief RTC初始化
 * @param[in] clock_source RTC时钟源
 * @return RTC_Status_t 状态码
 * @note 需要先配置RCC时钟源，然后调用此函数初始化RTC
 */
RTC_Status_t RTC_Init(RTC_ClockSource_t clock_source);

/**
 * @brief RTC反初始化
 * @return RTC_Status_t 状态码
 */
RTC_Status_t RTC_Deinit(void);

/**
 * @brief 设置RTC时间
 * @param[in] time 时间结构体指针
 * @return RTC_Status_t 状态码
 */
RTC_Status_t RTC_SetTime(const RTC_Time_t *time);

/**
 * @brief 获取RTC时间
 * @param[out] time 时间结构体指针
 * @return RTC_Status_t 状态码
 */
RTC_Status_t RTC_GetTime(RTC_Time_t *time);

/**
 * @brief 设置RTC闹钟
 * @param[in] time 闹钟时间结构体指针
 * @return RTC_Status_t 状态码
 */
RTC_Status_t RTC_SetAlarm(const RTC_Time_t *time);

/**
 * @brief 获取RTC闹钟时间
 * @param[out] time 闹钟时间结构体指针
 * @return RTC_Status_t 状态码
 */
RTC_Status_t RTC_GetAlarm(RTC_Time_t *time);

/**
 * @brief 使能RTC闹钟
 * @return RTC_Status_t 状态码
 */
RTC_Status_t RTC_EnableAlarm(void);

/**
 * @brief 禁用RTC闹钟
 * @return RTC_Status_t 状态码
 */
RTC_Status_t RTC_DisableAlarm(void);

/**
 * @brief 设置RTC闹钟回调函数
 * @param[in] callback 回调函数指针
 * @param[in] user_data 用户数据指针
 * @return RTC_Status_t 状态码
 */
RTC_Status_t RTC_SetAlarmCallback(RTC_AlarmCallback_t callback, void *user_data);

/**
 * @brief 检查RTC闹钟标志
 * @return uint8_t 1=闹钟触发，0=未触发
 */
uint8_t RTC_CheckAlarmFlag(void);

/**
 * @brief 清除RTC闹钟标志
 * @return RTC_Status_t 状态码
 */
RTC_Status_t RTC_ClearAlarmFlag(void);

/**
 * @brief 使能RTC秒中断
 * @return RTC_Status_t 状态码
 */
RTC_Status_t RTC_EnableSecondInterrupt(void);

/**
 * @brief 禁用RTC秒中断
 * @return RTC_Status_t 状态码
 */
RTC_Status_t RTC_DisableSecondInterrupt(void);

/**
 * @brief 检查RTC是否已初始化
 * @return uint8_t 1=已初始化，0=未初始化
 */
uint8_t RTC_IsInitialized(void);

/**
 * @brief 获取RTC计数器值（秒数，从1970-01-01 00:00:00开始）
 * @return uint32_t RTC计数器值
 */
uint32_t RTC_GetCounterValue(void);

/**
 * @brief 设置RTC计数器值（秒数，从1970-01-01 00:00:00开始）
 * @param[in] counter 计数器值
 * @return RTC_Status_t 状态码
 */
RTC_Status_t RTC_SetCounterValue(uint32_t counter);

/* ========== RTC校准功能 ========== */

/**
 * @brief 设置RTC校准值
 * @param[in] calibration_value 校准值（-511到+512，单位：ppm）
 * @return RTC_Status_t 状态码
 * @note 校准值用于补偿RTC时钟源的频率偏差
 * @note 正值表示时钟偏快，负值表示时钟偏慢
 */
RTC_Status_t RTC_SetCalibration(int16_t calibration_value);

/**
 * @brief 获取RTC校准值
 * @param[out] calibration_value 校准值（-511到+512，单位：ppm）
 * @return RTC_Status_t 状态码
 */
RTC_Status_t RTC_GetCalibration(int16_t *calibration_value);

/* ========== RTC时间戳功能 ========== */

/**
 * @brief RTC时间戳回调函数类型
 * @param[in] timestamp_time 时间戳时间
 * @param[in] user_data 用户数据指针
 */
typedef void (*RTC_TimestampCallback_t)(const RTC_Time_t *timestamp_time, void *user_data);

/**
 * @brief 使能RTC时间戳功能
 * @param[in] trigger_pin 触发引脚（PC13或PA0）
 * @return RTC_Status_t 状态码
 * @note 时间戳功能用于记录外部事件发生的时间
 */
RTC_Status_t RTC_EnableTimestamp(uint8_t trigger_pin);

/**
 * @brief 禁用RTC时间戳功能
 * @return RTC_Status_t 状态码
 */
RTC_Status_t RTC_DisableTimestamp(void);

/**
 * @brief 设置RTC时间戳回调函数
 * @param[in] callback 回调函数指针，NULL表示禁用回调
 * @param[in] user_data 用户数据指针
 * @return RTC_Status_t 状态码
 */
RTC_Status_t RTC_SetTimestampCallback(RTC_TimestampCallback_t callback, void *user_data);

/**
 * @brief 获取RTC时间戳
 * @param[out] timestamp_time 时间戳时间结构体指针
 * @return RTC_Status_t 状态码
 */
RTC_Status_t RTC_GetTimestamp(RTC_Time_t *timestamp_time);

/**
 * @brief 检查RTC时间戳标志
 * @return uint8_t 1=时间戳触发，0=未触发
 */
uint8_t RTC_CheckTimestampFlag(void);

/**
 * @brief 清除RTC时间戳标志
 * @return RTC_Status_t 状态码
 */
RTC_Status_t RTC_ClearTimestampFlag(void);

/* ========== RTC溢出中断功能 ========== */

/**
 * @brief RTC溢出回调函数类型
 * @param[in] user_data 用户数据指针
 */
typedef void (*RTC_OverflowCallback_t)(void *user_data);

/**
 * @brief 使能RTC溢出中断
 * @return RTC_Status_t 状态码
 * @note 溢出中断在RTC计数器溢出时触发（约136年）
 */
RTC_Status_t RTC_EnableOverflowInterrupt(void);

/**
 * @brief 禁用RTC溢出中断
 * @return RTC_Status_t 状态码
 */
RTC_Status_t RTC_DisableOverflowInterrupt(void);

/**
 * @brief 设置RTC溢出回调函数
 * @param[in] callback 回调函数指针，NULL表示禁用回调
 * @param[in] user_data 用户数据指针
 * @return RTC_Status_t 状态码
 */
RTC_Status_t RTC_SetOverflowCallback(RTC_OverflowCallback_t callback, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* RTC_H */

