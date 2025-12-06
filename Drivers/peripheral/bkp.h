/**
 * @file bkp.h
 * @brief BKP备份寄存器模块
 * @details 提供备份寄存器读写功能，数据在系统复位或掉电后仍能保持（需要VBAT供电）
 */

#ifndef BKP_H
#define BKP_H

#include "error_code.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BKP状态枚举
 */
typedef enum {
    BKP_OK = ERROR_OK,                                    /**< 操作成功 */
    BKP_ERROR_NOT_IMPLEMENTED = ERROR_BASE_RTC - 99,     /**< 功能未实现（占位空函数） */
    BKP_ERROR_NULL_PTR = ERROR_BASE_RTC - 10,             /**< 空指针错误 */
    BKP_ERROR_INVALID_PARAM = ERROR_BASE_RTC - 11,       /**< 参数错误（通用） */
    BKP_ERROR_INVALID_REGISTER = ERROR_BASE_RTC - 12,     /**< 无效的寄存器 */
    BKP_ERROR_NOT_INITIALIZED = ERROR_BASE_RTC - 13,      /**< 未初始化 */
    BKP_ERROR_ALREADY_INITIALIZED = ERROR_BASE_RTC - 14,  /**< 已初始化 */
} BKP_Status_t;

/**
 * @brief BKP寄存器枚举（DR1-DR42）
 */
typedef enum {
    BKP_REG_DR1 = 1,
    BKP_REG_DR2 = 2,
    BKP_REG_DR3 = 3,
    BKP_REG_DR4 = 4,
    BKP_REG_DR5 = 5,
    BKP_REG_DR6 = 6,
    BKP_REG_DR7 = 7,
    BKP_REG_DR8 = 8,
    BKP_REG_DR9 = 9,
    BKP_REG_DR10 = 10,
    BKP_REG_DR11 = 11,
    BKP_REG_DR12 = 12,
    BKP_REG_DR13 = 13,
    BKP_REG_DR14 = 14,
    BKP_REG_DR15 = 15,
    BKP_REG_DR16 = 16,
    BKP_REG_DR17 = 17,
    BKP_REG_DR18 = 18,
    BKP_REG_DR19 = 19,
    BKP_REG_DR20 = 20,
    BKP_REG_DR21 = 21,
    BKP_REG_DR22 = 22,
    BKP_REG_DR23 = 23,
    BKP_REG_DR24 = 24,
    BKP_REG_DR25 = 25,
    BKP_REG_DR26 = 26,
    BKP_REG_DR27 = 27,
    BKP_REG_DR28 = 28,
    BKP_REG_DR29 = 29,
    BKP_REG_DR30 = 30,
    BKP_REG_DR31 = 31,
    BKP_REG_DR32 = 32,
    BKP_REG_DR33 = 33,
    BKP_REG_DR34 = 34,
    BKP_REG_DR35 = 35,
    BKP_REG_DR36 = 36,
    BKP_REG_DR37 = 37,
    BKP_REG_DR38 = 38,
    BKP_REG_DR39 = 39,
    BKP_REG_DR40 = 40,
    BKP_REG_DR41 = 41,
    BKP_REG_DR42 = 42,
    BKP_REG_MAX = 42
} BKP_Register_t;

/* Include board.h, which includes stm32f10x.h and stm32f10x_bkp.h */
#include "board.h"

/**
 * @brief BKP初始化
 * @return BKP_Status_t 状态码
 * @note 使能PWR和BKP时钟，使能备份域访问
 */
BKP_Status_t BKP_Init(void);

/**
 * @brief BKP反初始化
 * @return BKP_Status_t 状态码
 */
BKP_Status_t BKP_Deinit(void);

/**
 * @brief 写入备份寄存器
 * @param[in] reg 寄存器编号（BKP_REG_DR1到BKP_REG_DR42）
 * @param[in] data 数据值（16位）
 * @return BKP_Status_t 状态码
 */
BKP_Status_t BKP_WriteRegister(BKP_Register_t reg, uint16_t data);

/**
 * @brief 读取备份寄存器
 * @param[in] reg 寄存器编号（BKP_REG_DR1到BKP_REG_DR42）
 * @param[out] data 数据值指针（16位）
 * @return BKP_Status_t 状态码
 */
BKP_Status_t BKP_ReadRegister(BKP_Register_t reg, uint16_t *data);

/**
 * @brief 检查BKP是否已初始化
 * @return uint8_t 1=已初始化，0=未初始化
 */
uint8_t BKP_IsInitialized(void);

/* ========== Tamper引脚功能 ========== */

/**
 * @brief Tamper引脚电平枚举
 */
typedef enum {
    BKP_TAMPER_LEVEL_LOW = 0,   /**< 低电平有效 */
    BKP_TAMPER_LEVEL_HIGH = 1,   /**< 高电平有效 */
} BKP_TamperLevel_t;

/**
 * @brief BKP Tamper中断回调函数类型
 * @param user_data 用户数据指针
 */
typedef void (*BKP_TamperCallback_t)(void *user_data);

/**
 * @brief 配置BKP Tamper引脚
 * @param[in] level Tamper引脚有效电平
 * @param[in] callback Tamper中断回调函数
 * @param[in] user_data 用户数据指针
 * @return BKP_Status_t 状态码
 * @note Tamper引脚用于检测篡改事件，触发时会清除所有备份寄存器
 */
BKP_Status_t BKP_ConfigTamperPin(BKP_TamperLevel_t level, BKP_TamperCallback_t callback, void *user_data);

/**
 * @brief 使能BKP Tamper引脚
 * @return BKP_Status_t 状态码
 */
BKP_Status_t BKP_EnableTamperPin(void);

/**
 * @brief 禁用BKP Tamper引脚
 * @return BKP_Status_t 状态码
 */
BKP_Status_t BKP_DisableTamperPin(void);

#ifdef __cplusplus
}
#endif

#endif /* BKP_H */
