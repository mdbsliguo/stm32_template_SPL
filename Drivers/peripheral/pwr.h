/**
 * @file pwr.h
 * @brief PWR电源管理模块
 * @details 提供低功耗模式功能，包括STOP模式和STANDBY模式
 */

#ifndef PWR_H
#define PWR_H

#include "error_code.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PWR状态枚举
 */
typedef enum {
    PWR_OK = ERROR_OK,                                    /**< 操作成功 */
    PWR_ERROR_INVALID_PARAM = ERROR_BASE_RTC - 20,       /**< 参数错误 */
} PWR_Status_t;

/**
 * @brief PVD检测电平枚举
 */
typedef enum {
    PWR_PVD_LEVEL_2V2 = 0,   /**< 2.2V */
    PWR_PVD_LEVEL_2V3 = 1,   /**< 2.3V */
    PWR_PVD_LEVEL_2V4 = 2,   /**< 2.4V */
    PWR_PVD_LEVEL_2V5 = 3,   /**< 2.5V */
    PWR_PVD_LEVEL_2V6 = 4,   /**< 2.6V */
    PWR_PVD_LEVEL_2V7 = 5,   /**< 2.7V */
    PWR_PVD_LEVEL_2V8 = 6,   /**< 2.8V */
    PWR_PVD_LEVEL_2V9 = 7,   /**< 2.9V */
} PWR_PVDLevel_t;

/**
 * @brief 低功耗模式枚举
 */
typedef enum {
    PWR_MODE_STOP = 0,        /**< STOP模式（停止模式） */
    PWR_MODE_STANDBY = 1,     /**< STANDBY模式（待机模式） */
} PWR_Mode_t;

/**
 * @brief 稳压器状态枚举（STOP模式）
 */
typedef enum {
    PWR_REGULATOR_ON = 0,         /**< 稳压器开启 */
    PWR_REGULATOR_LOW_POWER = 1,  /**< 稳压器低功耗模式 */
} PWR_Regulator_t;

/* Include board.h, which includes stm32f10x.h and stm32f10x_pwr.h */
#include "board.h"

/**
 * @brief PWR初始化
 * @return PWR_Status_t 状态码
 */
PWR_Status_t PWR_Init(void);

/**
 * @brief 配置PVD（电源电压检测）
 * @param[in] level PVD检测电平
 * @param[in] enable 是否使能PVD
 * @return PWR_Status_t 状态码
 */
PWR_Status_t PWR_ConfigPVD(PWR_PVDLevel_t level, uint8_t enable);

/**
 * @brief 使能唤醒引脚
 * @param[in] enable 是否使能
 * @return PWR_Status_t 状态码
 */
PWR_Status_t PWR_EnableWakeUpPin(uint8_t enable);

/**
 * @brief 进入STOP模式
 * @param[in] regulator 稳压器状态
 * @return PWR_Status_t 状态码
 * @note 进入STOP模式后，系统会停止运行，需要通过中断或事件唤醒
 * @warning 此函数不会返回，直到系统被唤醒
 */
PWR_Status_t PWR_EnterSTOPMode(PWR_Regulator_t regulator);

/**
 * @brief 进入STANDBY模式
 * @return PWR_Status_t 状态码
 * @note 进入STANDBY模式后，系统会完全关闭，只能通过WKUP引脚或RTC闹钟唤醒
 * @warning 此函数不会返回，直到系统被唤醒（系统会复位）
 */
PWR_Status_t PWR_EnterSTANDBYMode(void);

/**
 * @brief 检查唤醒标志
 * @return uint8_t 1=唤醒标志置位，0=未置位
 */
uint8_t PWR_CheckWakeUpFlag(void);

/**
 * @brief 清除唤醒标志
 * @return PWR_Status_t 状态码
 */
PWR_Status_t PWR_ClearWakeUpFlag(void);

/**
 * @brief 检查待机标志
 * @return uint8_t 1=待机标志置位，0=未置位
 */
uint8_t PWR_CheckStandbyFlag(void);

/**
 * @brief 清除待机标志
 * @return PWR_Status_t 状态码
 */
PWR_Status_t PWR_ClearStandbyFlag(void);

/**
 * @brief 检查PVD输出标志
 * @return uint8_t 1=PVD输出高，0=PVD输出低
 */
uint8_t PWR_CheckPVDFlag(void);

#ifdef __cplusplus
}
#endif

#endif /* PWR_H */
