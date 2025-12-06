#ifndef BUZZER_H
#define BUZZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "board.h"
#include "error_code.h"
#include <stdint.h>

/*******************************************************************************
 * @file buzzer.h
 * @brief Buzzer驱动模块头文件（商业级）
 * @version 1.0.0
 * @date 2024-01-01
 * @details 提供多Buzzer管理、GPIO/PWM双模式支持、频率控制、音调播放功能
 * 
 * 双模式说明：
 * - GPIO模式：适用于有源蜂鸣器（内部有振荡电路，通电即响，频率固定）
 * - PWM模式：适用于无源蜂鸣器（需要外部驱动信号，通过PWM频率控制音调）
 ******************************************************************************/

/* 版本管理 */
#define BUZZER_DRIVER_VERSION_MAJOR 1
#define BUZZER_DRIVER_VERSION_MINOR 0
#define BUZZER_DRIVER_VERSION_PATCH 0

/* 错误码定义 */
typedef enum {
    BUZZER_OK = ERROR_OK,                    /**< 操作成功 */
    BUZZER_ERROR_INVALID_ID = ERROR_BASE_BUZZER,      /**< 无效Buzzer编号 */
    BUZZER_ERROR_DISABLED = ERROR_BASE_BUZZER - 1,    /**< Buzzer未启用 */
    BUZZER_ERROR_NULL_PTR = ERROR_BASE_BUZZER - 2,    /**< 空指针错误 */
    BUZZER_ERROR_INIT_FAILED = ERROR_BASE_BUZZER - 3, /**< 初始化失败 */
    BUZZER_ERROR_INVALID_MODE = ERROR_BASE_BUZZER - 4, /**< 无效驱动模式 */
    BUZZER_ERROR_PWM_NOT_AVAILABLE = ERROR_BASE_BUZZER - 5, /**< PWM功能不可用（非PWM模式） */
    BUZZER_ERROR_INVALID_FREQUENCY = ERROR_BASE_BUZZER - 6, /**< 无效频率 */
    BUZZER_ERROR_INVALID_TONE = ERROR_BASE_BUZZER - 7  /**< 无效音调 */
} Buzzer_Status_t;

/* Buzzer编号枚举 */
typedef enum {
    BUZZER_1 = 1,  /**< Buzzer1编号 */
    BUZZER_2 = 2,  /**< Buzzer2编号 */
} Buzzer_Number_t;

/* Buzzer状态枚举 */
typedef enum {
    BUZZER_STATE_OFF = 0,  /**< Buzzer关闭状态 */
    BUZZER_STATE_ON = 1    /**< Buzzer开启状态 */
} Buzzer_State_t;

/* 音调枚举（C4-C5一个八度） */
typedef enum {
    BUZZER_TONE_C4 = 0,   /**< C4 (261.63 Hz) */
    BUZZER_TONE_D4 = 1,   /**< D4 (293.66 Hz) */
    BUZZER_TONE_E4 = 2,   /**< E4 (329.63 Hz) */
    BUZZER_TONE_F4 = 3,   /**< F4 (349.23 Hz) */
    BUZZER_TONE_G4 = 4,   /**< G4 (392.00 Hz) */
    BUZZER_TONE_A4 = 5,   /**< A4 (440.00 Hz) */
    BUZZER_TONE_B4 = 6,   /**< B4 (493.88 Hz) */
    BUZZER_TONE_C5 = 7,   /**< C5 (523.25 Hz) */
    BUZZER_TONE_MAX       /**< 最大音调数 */
} Buzzer_Tone_t;

/* 功能开关 */
#define BUZZER_DEBUG_MODE      1  /**< 0:量产模式 1:调试模式（控制断言和日志） */

/* 断言和日志（调试模式下启用） */
#if BUZZER_DEBUG_MODE
    void Buzzer_AssertHandler(const char* expr, const char* file, int line);
    void Buzzer_Log(const char* fmt, ...);
    #define BUZZER_ASSERT(expr)  ((expr) ? (void)0 : Buzzer_AssertHandler(#expr, __FILE__, __LINE__))
    #define BUZZER_LOG(...)      Buzzer_Log(__VA_ARGS__)
#else
    #define BUZZER_ASSERT(expr)  ((void)0)
    #define BUZZER_LOG(...)      ((void)0)
#endif

/* 函数声明 */

/**
 * @brief 初始化Buzzer驱动（根据配置表自动初始化所有启用的Buzzer）
 * @return Buzzer_Status_t 错误码
 * @note 只初始化enabled=1的Buzzer，关闭其初始状态
 * @note GPIO模式：初始化GPIO为推挽输出
 * @note PWM模式：初始化PWM外设和GPIO
 */
Buzzer_Status_t Buzzer_Init(void);

/**
 * @brief 反初始化Buzzer驱动（关闭所有Buzzer并释放资源）
 * @return Buzzer_Status_t 错误码
 */
Buzzer_Status_t Buzzer_Deinit(void);

/**
 * @brief 设置指定Buzzer的状态（自动处理有效电平）
 * @param[in] num Buzzer编号（从1开始）
 * @param[in] state 目标状态（BUZZER_STATE_ON/OFF）
 * @return Buzzer_Status_t 错误码
 * @note GPIO模式：直接控制GPIO电平
 * @note PWM模式：使能/禁用PWM通道
 */
Buzzer_Status_t Buzzer_SetState(Buzzer_Number_t num, Buzzer_State_t state);

/**
 * @brief 获取Buzzer当前状态
 * @param[in] num Buzzer编号
 * @param[out] state 状态指针（返回当前状态）
 * @return Buzzer_Status_t 错误码
 */
Buzzer_Status_t Buzzer_GetState(Buzzer_Number_t num, Buzzer_State_t* state);

/**
 * @brief 开启指定Buzzer
 * @param[in] num Buzzer编号
 * @return Buzzer_Status_t 错误码
 */
Buzzer_Status_t Buzzer_On(Buzzer_Number_t num);

/**
 * @brief 关闭指定Buzzer
 * @param[in] num Buzzer编号
 * @return Buzzer_Status_t 错误码
 */
Buzzer_Status_t Buzzer_Off(Buzzer_Number_t num);

/**
 * @brief Buzzer鸣响一次（开启+阻塞延时+关闭）
 * @param[in] num Buzzer编号
 * @param[in] duration_ms 鸣响持续时间（毫秒）
 * @return Buzzer_Status_t 错误码
 * @note 阻塞式延时，执行期间CPU不处理其他任务
 */
Buzzer_Status_t Buzzer_Beep(Buzzer_Number_t num, uint32_t duration_ms);

/**
 * @brief 停止Buzzer（关闭）
 * @param[in] num Buzzer编号
 * @return Buzzer_Status_t 错误码
 * @note 等同于Buzzer_Off()
 */
Buzzer_Status_t Buzzer_Stop(Buzzer_Number_t num);

/**
 * @brief 设置Buzzer频率（仅PWM模式）
 * @param[in] num Buzzer编号
 * @param[in] frequency 频率（Hz），范围：1Hz ~ 72MHz（取决于系统时钟）
 * @return Buzzer_Status_t 错误码
 * @note 仅在PWM模式下有效，GPIO模式返回BUZZER_ERROR_PWM_NOT_AVAILABLE
 */
Buzzer_Status_t Buzzer_SetFrequency(Buzzer_Number_t num, uint32_t frequency);

/**
 * @brief 播放指定音调（仅PWM模式）
 * @param[in] num Buzzer编号
 * @param[in] tone 音调枚举（BUZZER_TONE_C4 ~ BUZZER_TONE_C5）
 * @param[in] duration_ms 播放持续时间（毫秒），0表示持续播放直到调用Buzzer_Stop()
 * @return Buzzer_Status_t 错误码
 * @note 仅在PWM模式下有效，GPIO模式返回BUZZER_ERROR_PWM_NOT_AVAILABLE
 * @note duration_ms为0时，需要手动调用Buzzer_Stop()停止
 */
Buzzer_Status_t Buzzer_PlayTone(Buzzer_Number_t num, Buzzer_Tone_t tone, uint32_t duration_ms);

/* Buzzer快捷操作宏 */
#define BUZZER1_On()       Buzzer_On(BUZZER_1)
#define BUZZER1_Off()      Buzzer_Off(BUZZER_1)
#define BUZZER1_Beep(ms)   Buzzer_Beep(BUZZER_1, ms)
#define BUZZER1_Stop()     Buzzer_Stop(BUZZER_1)

#define BUZZER2_On()       Buzzer_On(BUZZER_2)
#define BUZZER2_Off()      Buzzer_Off(BUZZER_2)
#define BUZZER2_Beep(ms)   Buzzer_Beep(BUZZER_2, ms)
#define BUZZER2_Stop()     Buzzer_Stop(BUZZER_2)

#ifdef __cplusplus
}
#endif

#endif /* BUZZER_H */
