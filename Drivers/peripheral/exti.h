/**
 * @file exti.h
 * @brief EXTI外部中断模块
 * @details 提供外部中断配置、中断回调等功能
 */

#ifndef EXTI_H
#define EXTI_H

#include "error_code.h"
#include "stm32f10x.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief EXTI状态枚举
 */
typedef enum {
    EXTI_OK = ERROR_OK,                                    /**< 操作成功 */
    EXTI_ERROR_INVALID_PARAM = ERROR_BASE_EXTI - 1,      /**< 参数错误 */
    EXTI_ERROR_INVALID_LINE = ERROR_BASE_EXTI - 2,        /**< 无效的EXTI线 */
    EXTI_ERROR_NOT_INITIALIZED = ERROR_BASE_EXTI - 3,    /**< 未初始化 */
    EXTI_ERROR_ALREADY_INITIALIZED = ERROR_BASE_EXTI - 4, /**< 已初始化 */
} EXTI_Status_t;

/**
 * @brief EXTI线枚举（0-19）
 */
typedef enum {
    EXTI_LINE_0 = 0,   /**< EXTI线0 */
    EXTI_LINE_1 = 1,   /**< EXTI线1 */
    EXTI_LINE_2 = 2,   /**< EXTI线2 */
    EXTI_LINE_3 = 3,   /**< EXTI线3 */
    EXTI_LINE_4 = 4,   /**< EXTI线4 */
    EXTI_LINE_5 = 5,   /**< EXTI线5 */
    EXTI_LINE_6 = 6,   /**< EXTI线6 */
    EXTI_LINE_7 = 7,   /**< EXTI线7 */
    EXTI_LINE_8 = 8,   /**< EXTI线8 */
    EXTI_LINE_9 = 9,   /**< EXTI线9 */
    EXTI_LINE_10 = 10, /**< EXTI线10 */
    EXTI_LINE_11 = 11, /**< EXTI线11 */
    EXTI_LINE_12 = 12, /**< EXTI线12 */
    EXTI_LINE_13 = 13, /**< EXTI线13 */
    EXTI_LINE_14 = 14, /**< EXTI线14 */
    EXTI_LINE_15 = 15, /**< EXTI线15 */
    EXTI_LINE_16 = 16, /**< EXTI线16（PVD） */
    EXTI_LINE_17 = 17, /**< EXTI线17（RTC Alarm） */
    EXTI_LINE_18 = 18, /**< EXTI线18（USB Wakeup） */
    EXTI_LINE_19 = 19, /**< EXTI线19（Ethernet Wakeup） */
    EXTI_LINE_MAX = 20 /**< 最大线数 */
} EXTI_Line_t;

/**
 * @brief EXTI触发模式枚举
 */
typedef enum {
    EXTI_TRIGGER_RISING = 0,        /**< 上升沿触发 */
    EXTI_TRIGGER_FALLING = 1,      /**< 下降沿触发 */
    EXTI_TRIGGER_RISING_FALLING = 2 /**< 上升沿和下降沿触发 */
} EXTI_Trigger_t;

/**
 * @brief EXTI模式枚举
 */
typedef enum {
    EXTI_MODE_INTERRUPT = 0, /**< 中断模式 */
    EXTI_MODE_EVENT = 1      /**< 事件模式 */
} EXTI_Mode_t;

/**
 * @brief EXTI中断回调函数类型
 * @param[in] line EXTI线号
 * @param[in] user_data 用户数据指针
 */
typedef void (*EXTI_Callback_t)(EXTI_Line_t line, void *user_data);

/**
 * @brief EXTI初始化
 * @param[in] line EXTI线号（0-19）
 * @param[in] trigger 触发模式
 * @param[in] mode 模式（中断/事件）
 * @return EXTI_Status_t 状态码
 */
EXTI_Status_t EXTI_HW_Init(EXTI_Line_t line, EXTI_Trigger_t trigger, EXTI_Mode_t mode);

/**
 * @brief EXTI反初始化
 * @param[in] line EXTI线号
 * @return EXTI_Status_t 状态码
 */
EXTI_Status_t EXTI_Deinit(EXTI_Line_t line);

/**
 * @brief 设置EXTI中断回调函数
 * @param[in] line EXTI线号
 * @param[in] callback 回调函数指针
 * @param[in] user_data 用户数据指针
 * @return EXTI_Status_t 状态码
 */
EXTI_Status_t EXTI_SetCallback(EXTI_Line_t line, EXTI_Callback_t callback, void *user_data);

/**
 * @brief 使能EXTI中断
 * @param[in] line EXTI线号
 * @return EXTI_Status_t 状态码
 */
EXTI_Status_t EXTI_Enable(EXTI_Line_t line);

/**
 * @brief 禁用EXTI中断
 * @param[in] line EXTI线号
 * @return EXTI_Status_t 状态码
 */
EXTI_Status_t EXTI_Disable(EXTI_Line_t line);

/**
 * @brief 清除EXTI挂起标志
 * @param[in] line EXTI线号
 * @return EXTI_Status_t 状态码
 */
EXTI_Status_t EXTI_ClearPending(EXTI_Line_t line);

/**
 * @brief 获取EXTI挂起标志状态
 * @param[in] line EXTI线号
 * @return uint8_t 1=挂起，0=未挂起
 */
uint8_t EXTI_GetPendingStatus(EXTI_Line_t line);

/**
 * @brief 生成软件中断
 * @param[in] line EXTI线号
 * @return EXTI_Status_t 状态码
 */
EXTI_Status_t EXTI_HW_GenerateSWInterrupt(EXTI_Line_t line);

/**
 * @brief 检查EXTI是否已初始化
 * @param[in] line EXTI线号
 * @return uint8_t 1=已初始化，0=未初始化
 */
uint8_t EXTI_IsInitialized(EXTI_Line_t line);

/**
 * @brief EXTI中断处理函数（应在中断服务程序中调用）
 * @param[in] line EXTI线号
 */
void EXTI_IRQHandler(EXTI_Line_t line);

#ifdef __cplusplus
}
#endif

#endif /* EXTI_H */

