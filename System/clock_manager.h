/**
 * @file clock_manager.h
 * @brief 时钟管理模块（动态频率调节DVFS）
 * @version 1.0.0
 * @date 2024-01-01
 * @details 
 * - 自动降频设计：CPU使用率低于阈值时自动降频，超过阈值时自动升频
 * - 支持手动/自动两种模式
 * - 递进式降频：系数1-9，系数1是72MHz，系数9是8MHz
 * - 频率范围：8MHz ~ 72MHz
 * 
 * @section 使用方法
 * 
 * @subsection 初始化
 * 初始化时钟管理模块：
 * @code
 * CLKM_ErrorCode_t err = CLKM_Init();
 * if (err != CLKM_OK)
 * {
 *     // 初始化失败
 * }
 * @endcode
 * 
 * @subsection 手动模式
 * 手动设置固定频率：
 * @code
 * // 设置为72MHz
 * CLKM_SetMode(CLKM_MODE_MANUAL, CLKM_LVL_72MHZ);
 * 
 * // 或使用便捷函数
 * CLKM_SetFixedLevel(CLKM_LVL_72MHZ);
 * 
 * // 递进式调整频率
 * CLKM_AdjustLevel(-1);  // 降频1档（72MHz -> 64MHz）
 * CLKM_AdjustLevel(1);   // 升频1档（64MHz -> 72MHz）
 * @endcode
 * 
 * @subsection 自动模式
 * 根据CPU负载自动调节频率：
 * @code
 * // 设置为自动模式，最低频率8MHz
 * CLKM_SetMode(CLKM_MODE_AUTO, CLKM_LVL_8MHZ);
 * 
 * // 在主循环中调用自适应任务
 * while (1)
 * {
 *     CLKM_AdaptiveTask();  // 每50ms检测一次CPU负载并自动调频
 *     
 *     // 在空闲时调用空闲钩子
 *     CLKM_IdleHook();
 *     
 *     // 在忙碌时调用忙碌钩子
 *     // CLKM_BusyHook();
 *     
 *     Delay_ms(100);
 * }
 * @endcode
 * 
 * @subsection 查询状态
 * 查询当前频率和模式：
 * @code
 * CLKM_FreqLevel_t level = CLKM_GetCurrentLevel();
 * CLKM_Mode_t mode = CLKM_GetCurrentMode();
 * uint32_t freq = CLKM_GetCurrentFrequency();
 * uint8_t cpu_load = CLKM_GetCPULoad();
 * @endcode
 * 
 * @section 频率档位
 * 
 * | 档位 | 频率 | 时钟源 | PLL倍频 | Flash等待周期 |
 * |------|------|--------|---------|--------------|
 * | CLKM_LVL_72MHZ | 72MHz | PLL | x9 | 2 |
 * | CLKM_LVL_64MHZ | 64MHz | PLL | x8 | 2 |
 * | CLKM_LVL_56MHZ | 56MHz | PLL | x7 | 2 |
 * | CLKM_LVL_48MHZ | 48MHz | PLL | x6 | 1 |
 * | CLKM_LVL_40MHZ | 40MHz | PLL | x5 | 1 |
 * | CLKM_LVL_32MHZ | 32MHz | PLL | x4 | 1 |
 * | CLKM_LVL_24MHZ | 24MHz | PLL | x3 | 0 |
 * | CLKM_LVL_16MHZ | 16MHz | PLL | x2 | 0 |
 * | CLKM_LVL_8MHZ  | 8MHz  | HSI | - | 0 |
 * 
 * @section 自动调频策略
 * 
 * 自动模式下的调频策略：
 * - CPU使用率 < 30%：降频1档，最低8MHz，降频间隔5秒
 * - CPU使用率 > 50%：升频3档，最高72MHz，升频间隔1秒
 * 
 * @section 注意事项
 * 
 * 1. **HSE要求**：除8MHz（HSI）外，其他频率需要外部8MHz晶振（HSE）
 * 2. **切换时间**：频率切换需要时间（PLL锁定时间），频繁切换可能影响性能
 * 3. **延时适配**：频率切换时，延时模块会自动适配，确保1秒永远是1秒
 * 4. **中断保护**：频率切换在临界区进行，会短暂禁用中断
 * 5. **切换间隔**：手动模式有最小切换间隔限制，防止切换过快
 * 6. **空闲钩子**：自动模式需要在主循环空闲处调用CLKM_IdleHook()统计CPU负载
 * 7. **自适应任务**：自动模式需要在主循环中定期调用CLKM_AdaptiveTask()
 * 
 * @section 配置说明
 * 
 * 模块开关在System/config.h中配置：
 * - CONFIG_MODULE_CLOCK_MANAGER_ENABLED：时钟管理模块开关（默认启用）
 * 
 * 功能开关（在clock_manager.c中定义）：
 * - CLKM_ADAPTIVE_ENABLE：自动调频功能开关（默认启用）
 * - CLKM_IDLE_HOOK_ENABLE：空闲钩子功能开关（默认启用）
 * - CLKM_LOAD_CHECK_INTERVAL：CPU负载检测间隔（默认50ms）
 * - CLKM_SWITCH_INTERVAL_UP：升频最小间隔（默认1秒）
 * - CLKM_SWITCH_INTERVAL_DOWN：降频最小间隔（默认5秒）
 * - CLKM_LOAD_THRESHOLD_HIGH：高负载阈值（默认50%）
 * - CLKM_LOAD_THRESHOLD_LOW：低负载阈值（默认30%）
 * 
 * @section 相关模块
 * 
 * - Delay：延时模块（System/delay.c/h），频率切换时自动适配
 * - TIM2_TimeBase：时间基准模块（Drivers/timer/TIM2_TimeBase.c/h），频率切换时自动适配
 */

#ifndef CLOCK_MANAGER_H
#define CLOCK_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 时钟管理错误码
 */
typedef enum {
    CLKM_OK = ERROR_OK,                                    /**< 操作成功 */
    CLKM_ERROR_NOT_INIT = ERROR_BASE_SYSTEM - 10,        /**< 未初始化 */
    CLKM_ERROR_INVALID_FREQ = ERROR_BASE_SYSTEM - 11,     /**< 无效频率 */
    CLKM_ERROR_PLL_LOCK_TIMEOUT = ERROR_BASE_SYSTEM - 12, /**< PLL锁定超时 */
    CLKM_ERROR_SWITCH_TOO_FAST = ERROR_BASE_SYSTEM - 13,  /**< 切换过快 */
    CLKM_ERROR_HSE_NOT_FOUND = ERROR_BASE_SYSTEM - 14,     /**< HSE未找到 */
    CLKM_ERROR_MODE_CONFLICT = ERROR_BASE_SYSTEM - 15,     /**< 模式冲突 */
} CLKM_ErrorCode_t;

/**
 * @brief 频率档位（系数1-9）
 * @note 系数1=72MHz，系数9=8MHz
 */
typedef enum {
    CLKM_LVL_72MHZ = 0,  /**< 系数1：72MHz */
    CLKM_LVL_64MHZ = 1,  /**< 系数2：64MHz */
    CLKM_LVL_56MHZ = 2,  /**< 系数3：56MHz */
    CLKM_LVL_48MHZ = 3,  /**< 系数4：48MHz */
    CLKM_LVL_40MHZ = 4,  /**< 系数5：40MHz */
    CLKM_LVL_32MHZ = 5,  /**< 系数6：32MHz */
    CLKM_LVL_24MHZ = 6,  /**< 系数7：24MHz */
    CLKM_LVL_16MHZ = 7,  /**< 系数8：16MHz */
    CLKM_LVL_8MHZ = 8,   /**< 系数9：8MHz */
    CLKM_LVL_MAX = 9     /**< 最大档位数 */
} CLKM_FreqLevel_t;

/**
 * @brief 运行模式
 */
typedef enum {
    CLKM_MODE_MANUAL = 0,  /**< 手动模式：固定频率 */
    CLKM_MODE_AUTO = 1,    /**< 自动模式：根据负载自动调节 */
} CLKM_Mode_t;

/**
 * @brief 时钟源
 */
typedef enum {
    CLKM_SRC_HSI = 0,  /**< HSI内部时钟 */
    CLKM_SRC_HSE = 1,  /**< HSE外部时钟 */
    CLKM_SRC_PLL = 2,  /**< PLL倍频时钟 */
} CLKM_ClockSource_t;

/**
 * @brief 时钟配置结构体
 */
typedef struct {
    uint32_t target_freq;        /**< 目标频率（Hz） */
    CLKM_ClockSource_t source;  /**< 时钟源 */
    uint8_t pll_mul;            /**< PLL倍频系数 */
    uint8_t flash_latency;      /**< Flash等待周期 */
} CLKM_Config_t;

/**
 * @brief 初始化时钟管理模块
 * @return CLKM_ErrorCode_t 错误码
 * @note 默认频率为72MHz（系数1）
 */
CLKM_ErrorCode_t CLKM_Init(void);

/**
 * @brief 设置运行模式
 * @param[in] mode 模式（手动/自动）
 * @param[in] param 参数：
 *   - 手动模式：频率档位（CLKM_LVL_72MHZ ~ CLKM_LVL_8MHZ）
 *   - 自动模式：最低频率档位（允许降频到的最低档位）
 * @return CLKM_ErrorCode_t 错误码
 */
CLKM_ErrorCode_t CLKM_SetMode(CLKM_Mode_t mode, uint8_t param);

/**
 * @brief 手动设置固定频率档位
 * @param[in] level 频率档位（CLKM_LVL_72MHZ ~ CLKM_LVL_8MHZ）
 * @return CLKM_ErrorCode_t 错误码
 * @note 仅在手动模式下有效
 */
CLKM_ErrorCode_t CLKM_SetFixedLevel(CLKM_FreqLevel_t level);

/**
 * @brief 手动调整频率档位（递进式）
 * @param[in] step 调整步数（正数=降频，负数=升频）
 * @return CLKM_ErrorCode_t 错误码
 * @note 仅在手动模式下有效
 */
CLKM_ErrorCode_t CLKM_AdjustLevel(int8_t step);

/**
 * @brief 获取当前频率档位
 * @return CLKM_FreqLevel_t 当前档位
 */
CLKM_FreqLevel_t CLKM_GetCurrentLevel(void);

/**
 * @brief 获取当前运行模式
 * @return CLKM_Mode_t 当前模式
 */
CLKM_Mode_t CLKM_GetCurrentMode(void);

/**
 * @brief 获取当前频率（Hz）
 * @return uint32_t 当前频率
 */
uint32_t CLKM_GetCurrentFrequency(void);

/**
 * @brief 1秒定时器计算CPU使用率（在TIM2中断中调用）
 * @note 每1秒调用一次，计算CPU使用率
 * @note 此函数在TIM2中断中自动调用，无需手动调用
 */
void CLKM_CalculateCPULoad1Sec(void);

/**
 * @brief 获取CPU负载（%）
 * @return uint8_t CPU负载（0-100）
 */
uint8_t CLKM_GetCPULoad(void);

/**
 * @brief 自适应任务（自动调频）
 * @note 需要在主循环中定期调用（建议每50ms）
 * @note 仅在自动模式下有效
 */
void CLKM_AdaptiveTask(void);

/**
 * @brief 空闲钩子函数
 * @note 在空闲时调用，用于统计CPU负载
 * @note 需要在主循环的空闲处调用
 */
void CLKM_IdleHook(void);

/**
 * @brief 忙碌钩子函数
 * @note 在忙碌时调用，用于统计CPU负载
 * @note 需要在主循环的忙碌处调用（如模拟程序运行时）
 */
void CLKM_BusyHook(void);

#ifdef __cplusplus
}
#endif

#endif /* CLOCK_MANAGER_H */
