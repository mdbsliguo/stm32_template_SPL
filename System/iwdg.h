/**
 * @file iwdg.h
 * @brief 独立看门狗管理模块（快速开发工程模板）
 * @version 1.0.0
 * @date 2024-01-01
 * @details 提供独立看门狗（IWDG）的初始化、喂狗、配置等功能
 * 
 * @note 配置管理：
 * - 模块开关：通过 System/config.h 中的 CONFIG_MODULE_IWDG_ENABLED 控制
 * - 默认超时：通过 System/config.h 中的 CONFIG_IWDG_TIMEOUT_MS 控制
 * 
 * @note IWDG特性：
 * - 独立时钟源（LSI，约40kHz）
 * - 超时时间 = (4 * 2^PR) * (RLR + 1) / LSI_freq
 * - 最小超时约1ms，最大约32s
 * - 一旦启用，只能通过系统复位禁用
 * 
 * @section 使用方法
 * 
 * @subsection 初始化
 * 初始化看门狗模块（使用默认配置）：
 * @code
 * IWDG_Status_t err = IWDG_Init(NULL);  // 使用默认配置（1秒超时）
 * if (err != IWDG_OK)
 * {
 *     // 初始化失败
 * }
 * @endcode
 * 
 * 使用自定义配置初始化：
 * @code
 * iwdg_config_t config = {
 *     .timeout_ms = 2000,  // 2秒超时
 *     .prescaler = 0,      // 自动计算
 *     .reload = 0          // 自动计算
 * };
 * IWDG_Init(&config);
 * @endcode
 * 
 * @subsection 启用看门狗
 * 初始化后需要调用IWDG_Start()启用看门狗：
 * @code
 * IWDG_Status_t err = IWDG_Start();
 * if (err != IWDG_OK)
 * {
 *     // 启用失败
 * }
 * @endcode
 * 
 * @warning 一旦启用看门狗，只能通过系统复位禁用！
 * 
 * @subsection 喂狗
 * 必须在超时时间内定期调用IWDG_Refresh()喂狗：
 * @code
 * while (1)
 * {
 *     // 业务逻辑
 *     // ...
 *     
 *     // 喂狗（建议在主循环中定期调用）
 *     IWDG_Refresh();
 *     
 *     Delay_ms(100);
 * }
 * @endcode
 * 
 * @subsection 配置超时时间
 * 只能在启用前配置超时时间：
 * @code
 * // 先初始化
 * IWDG_Init(NULL);
 * 
 * // 配置超时时间为3秒
 * IWDG_SetTimeout(3000);
 * 
 * // 启用看门狗
 * IWDG_Start();
 * 
 * // 启用后无法修改配置
 * @endcode
 * 
 * @subsection 查询状态
 * 查询看门狗状态：
 * @code
 * if (IWDG_IsInitialized())
 * {
 *     uint32_t timeout = IWDG_GetTimeout();
 *     if (IWDG_IsEnabled())
 * {
 *         // 看门狗已启用，需要定期喂狗
 *     }
 * }
 * @endcode
 * 
 * @subsection 辅助函数
 * 计算超时时间或参数：
 * @code
 * // 计算超时时间
 * uint32_t timeout = IWDG_CalculateTimeout(4, 1000);  // PR=4, RLR=1000
 * 
 * // 根据超时时间计算参数
 * uint8_t prescaler;
 * uint16_t reload;
 * IWDG_CalculateParams(2000, &prescaler, &reload);  // 2秒超时
 * @endcode
 * 
 * @section 注意事项
 * 
 * 1. **必须定期喂狗**：启用后必须在超时时间内定期调用IWDG_Refresh()，否则系统会复位
 * 2. **无法禁用**：一旦启用看门狗，只能通过系统复位禁用，无法通过软件禁用
 * 3. **配置时机**：只能在启用前配置超时时间，启用后无法修改
 * 4. **超时范围**：超时时间范围1ms~32768ms，超出范围会返回错误
 * 5. **LSI频率**：使用LSI（约40kHz）作为时钟源，实际频率可能有偏差
 * 6. **初始化顺序**：看门狗初始化不依赖其他模块，可以在System_Init()之前初始化
 * 7. **喂狗频率**：建议在主循环中定期喂狗，频率应远高于超时时间（如超时2秒，每100ms喂一次）
 * 
 * @section 配置说明
 * 
 * 模块开关在System/config.h中配置：
 * - CONFIG_MODULE_IWDG_ENABLED：看门狗模块开关（默认启用）
 * - CONFIG_IWDG_TIMEOUT_MS：默认超时时间（毫秒），范围1ms~32768ms（默认1000ms）
 * 
 * @section 超时时间计算
 * 
 * 超时时间计算公式：
 * timeout_ms = (4 * 2^PR) * (RLR + 1) * 1000 / LSI_freq
 * 
 * 其中：
 * - PR：预分频器（0-6），对应4/8/16/32/64/128/256分频
 * - RLR：重装载值（0-4095）
 * - LSI_freq：LSI频率（约40kHz）
 * 
 * 示例：
 * - PR=4（64分频），RLR=1000：timeout = (4 * 64) * 1001 * 1000 / 40000 ≈ 6406ms
 * - PR=6（256分频），RLR=4095：timeout = (4 * 256) * 4096 * 1000 / 40000 ≈ 104857ms（超出32秒限制）
 * 
 * @section 相关模块
 * 
 * - ErrorHandler：错误处理模块（Common/error_handler.c/h）
 */

#ifndef IWDG_H
#define IWDG_H

#include "error_code.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 看门狗错误码
 */
typedef enum {
    IWDG_OK = ERROR_OK,                                    /**< 操作成功 */
    IWDG_ERROR_NOT_INITIALIZED = ERROR_BASE_IWDG - 1,    /**< 未初始化 */
    IWDG_ERROR_INVALID_PARAM = ERROR_BASE_IWDG - 2,       /**< 参数非法 */
    IWDG_ERROR_TIMEOUT_TOO_SHORT = ERROR_BASE_IWDG - 3,   /**< 超时时间太短 */
    IWDG_ERROR_TIMEOUT_TOO_LONG = ERROR_BASE_IWDG - 4,    /**< 超时时间太长 */
    IWDG_ERROR_ALREADY_ENABLED = ERROR_BASE_IWDG - 5,     /**< 看门狗已启用 */
    IWDG_ERROR_CONFIG_FAILED = ERROR_BASE_IWDG - 6,      /**< 配置失败 */
} IWDG_Status_t;

/**
 * @brief 看门狗配置结构体
 */
typedef struct {
    uint32_t timeout_ms;  /**< 超时时间（毫秒），范围：1ms ~ 32768ms */
    uint8_t prescaler;    /**< 预分频器（0-6，对应4/8/16/32/64/128/256），0表示自动计算 */
    uint16_t reload;      /**< 重装载值（0-4095），0表示自动计算 */
} iwdg_config_t;

/**
 * @brief 初始化看门狗模块
 * @param[in] config 看门狗配置（可为NULL，使用默认配置）
 * @return IWDG_Status_t 错误码
 * @note 如果config为NULL，使用System/config.h中的默认配置
 * @note 初始化后不会自动启用看门狗，需要调用IWDG_Start()启用
 */
IWDG_Status_t IWDG_Init(const iwdg_config_t* config);

/**
 * @brief 反初始化看门狗模块
 * @return IWDG_Status_t 错误码
 * @note 注意：一旦启用看门狗，只能通过系统复位禁用，此函数仅清除模块状态
 */
IWDG_Status_t IWDG_Deinit(void);

/**
 * @brief 启用看门狗
 * @return IWDG_Status_t 错误码
 * @warning 一旦启用，只能通过系统复位禁用！
 * @note 启用后必须定期调用IWDG_Refresh()喂狗，否则系统会复位
 * @note 注意：此函数与SPL库的IWDG_Enable()函数名冲突，使用IWDG_Start()代替
 */
IWDG_Status_t IWDG_Start(void);

/**
 * @brief 喂狗（刷新看门狗计数器）
 * @return IWDG_Status_t 错误码
 * @note 必须在看门狗超时前调用，否则系统会复位
 * @note 建议在主循环或关键任务中定期调用
 */
IWDG_Status_t IWDG_Refresh(void);

/**
 * @brief 配置看门狗超时时间
 * @param[in] timeout_ms 超时时间（毫秒），范围：1ms ~ 32768ms
 * @return IWDG_Status_t 错误码
 * @note 只能在看门狗启用前配置，启用后无法修改
 */
IWDG_Status_t IWDG_SetTimeout(uint32_t timeout_ms);

/**
 * @brief 获取当前超时时间
 * @return 超时时间（毫秒），0表示未初始化
 */
uint32_t IWDG_GetTimeout(void);

/**
 * @brief 检查看门狗是否已初始化
 * @return 1-已初始化，0-未初始化
 */
uint8_t IWDG_IsInitialized(void);

/**
 * @brief 检查看门狗是否已启用
 * @return 1-已启用，0-未启用
 * @note 一旦启用，只能通过系统复位禁用
 */
uint8_t IWDG_IsEnabled(void);

/**
 * @brief 计算看门狗超时时间（辅助函数）
 * @param[in] prescaler 预分频器（0-6）
 * @param[in] reload 重装载值（0-4095）
 * @return 超时时间（毫秒），基于LSI=40kHz计算
 * @note 实际超时时间可能因LSI频率偏差而略有不同
 */
uint32_t IWDG_CalculateTimeout(uint8_t prescaler, uint16_t reload);

/**
 * @brief 根据超时时间计算预分频器和重装载值（辅助函数）
 * @param[in] timeout_ms 目标超时时间（毫秒）
 * @param[out] prescaler 计算得到的预分频器（0-6）
 * @param[out] reload 计算得到的重装载值（0-4095）
 * @return IWDG_Status_t 错误码
 * @note 基于LSI=40kHz计算，实际值可能因LSI频率偏差而略有不同
 */
IWDG_Status_t IWDG_CalculateParams(uint32_t timeout_ms, uint8_t* prescaler, uint16_t* reload);

/* 模块开关检查（与iwdg.c保持一致） */
#ifndef CONFIG_MODULE_IWDG_ENABLED
#define CONFIG_MODULE_IWDG_ENABLED 1  /* 默认启用 */
#endif

#if !CONFIG_MODULE_IWDG_ENABLED
/* 如果看门狗模块被禁用，所有函数都为空操作 */
#define IWDG_Init(...)          ((IWDG_Status_t)ERROR_OK)
#define IWDG_Deinit()           ((IWDG_Status_t)ERROR_OK)
#define IWDG_Start()            ((IWDG_Status_t)ERROR_OK)
#define IWDG_Refresh()          ((IWDG_Status_t)ERROR_OK)
#define IWDG_SetTimeout(...)    ((IWDG_Status_t)ERROR_OK)
#define IWDG_GetTimeout()       (0)
#define IWDG_IsInitialized()   (0)
#define IWDG_IsEnabled()        (0)
#define IWDG_CalculateTimeout(...) (0)
#define IWDG_CalculateParams(...)  ((IWDG_Status_t)ERROR_OK)
#endif /* CONFIG_MODULE_IWDG_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* IWDG_H */

