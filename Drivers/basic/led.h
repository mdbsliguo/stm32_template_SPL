#ifndef LED_H
#define LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "board.h"

/*******************************************************************************
 * @file led.h
 * @brief LED驱动模块头文件（商业级）
 * @version 1.2.0
 * @date 2024-01-01
 * @details 提供多LED管理、错误处理、动态时钟适配、断言日志功能
 ******************************************************************************/

/* 版本管理 */
#define LED_DRIVER_VERSION_MAJOR 1
#define LED_DRIVER_VERSION_MINOR 2
#define LED_DRIVER_VERSION_PATCH 0

/* 错误码定义 */
typedef enum {
    LED_OK = 0,               /**< 操作成功 */
    LED_ERROR_INVALID_ID,     /**< 无效LED编号 */
    LED_ERROR_DISABLED,       /**< LED未启用 */
    LED_ERROR_NULL_PTR,       /**< 空指针错误 */
    LED_ERROR_INIT_FAILED     /**< 初始化失败 */
} LED_Status_t;

/* LED编号枚举 */
typedef enum {
    LED_1 = 1,  /**< LED1编号 */
    LED_2 = 2,  /**< LED2编号 */
} LED_Number_t;

/* LED状态枚举 */
typedef enum {
    LED_STATE_OFF = 0,  /**< LED关闭状态 */
    LED_STATE_ON = 1    /**< LED点亮状态 */
} LED_State_t;

/* 功能开关 */
#define LED_DEBUG_MODE      1  /**< 0:量产模式 1:调试模式（控制断言和日志） */

/* 断言和日志（调试模式下启用） */
#if LED_DEBUG_MODE
    void LED_AssertHandler(const char* expr, const char* file, int line);
    void LED_Log(const char* fmt, ...);
    #define LED_ASSERT(expr)  ((expr) ? (void)0 : LED_AssertHandler(#expr, __FILE__, __LINE__))
    #define LED_LOG(...)      LED_Log(__VA_ARGS__)
#else
    #define LED_ASSERT(expr)  ((void)0)
    #define LED_LOG(...)      ((void)0)
#endif

/* 函数声明 */

/**
 * @brief 初始化LED驱动（根据配置表自动初始化所有启用的LED）
 * @return LED_Status_t 错误码
 * @note 只初始化enabled=1的LED，关闭其初始状态
 */
LED_Status_t LED_Init(void);

/**
 * @brief 反初始化LED驱动（关闭所有LED并释放资源）
 * @return LED_Status_t 错误码
 */
LED_Status_t LED_Deinit(void);

/**
 * @brief 设置指定LED的状态（自动处理有效电平）
 * @param[in] num LED编号（从1开始）
 * @param[in] state 目标状态（LED_STATE_ON/OFF）
 * @return LED_Status_t 错误码
 */
LED_Status_t LED_SetState(LED_Number_t num, LED_State_t state);

/**
 * @brief 获取LED当前状态
 * @param[in] num LED编号
 * @param[out] state 状态指针（返回当前状态）
 * @return LED_Status_t 错误码
 */
LED_Status_t LED_GetState(LED_Number_t num, LED_State_t* state);

/**
 * @brief 点亮指定LED
 * @param[in] num LED编号
 * @return LED_Status_t 错误码
 */
LED_Status_t LED_On(LED_Number_t num);

/**
 * @brief 熄灭指定LED
 * @param[in] num LED编号
 * @return LED_Status_t 错误码
 */
LED_Status_t LED_Off(LED_Number_t num);

/**
 * @brief 翻转指定LED电平状态
 * @param[in] num LED编号
 * @return LED_Status_t 错误码
 */
LED_Status_t LED_Toggle(LED_Number_t num);

/**
 * @brief LED闪烁一次（翻转+阻塞延时）
 * @param[in] num LED编号
 * @param[in] delay_ms 延时毫秒数
 * @return LED_Status_t 错误码
 * @note 阻塞式延时，执行期间CPU不处理其他任务
 */
LED_Status_t LED_Blink(LED_Number_t num, uint32_t delay_ms);

/* LED快捷操作宏 */
#define LED1_On()       LED_On(LED_1)
#define LED1_Off()      LED_Off(LED_1)
#define LED1_Toggle()   LED_Toggle(LED_1)
#define LED1_Blink(ms)  LED_Blink(LED_1, ms)

#define LED2_On()       LED_On(LED_2)
#define LED2_Off()      LED_Off(LED_2)
#define LED2_Toggle()   LED_Toggle(LED_2)
#define LED2_Blink(ms)  LED_Blink(LED_2, ms)

#ifdef __cplusplus
}
#endif

#endif /* LED_H */
