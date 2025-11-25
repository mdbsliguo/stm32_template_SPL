/**
 * @file module_controller.h
 * @brief 模块开关总控（快速开发工程模板）
 * @version 1.0.0
 * @date 2024-01-01
 * @details 提供统一的模块状态查询、依赖检查、初始化顺序管理等功能
 * 
 * @note 配置管理：
 * - 模块开关：通过 System/config.h 中的 CONFIG_MODULE_XXX_ENABLED 控制
 * 
 * @note 功能特性：
 * - 编译时模块开关状态查询（从config.h读取）
 * - 运行时模块初始化状态查询
 * - 模块依赖关系检查
 * - 模块初始化顺序管理
 * - 模块状态报告
 * 
 * @section 使用方法
 * 
 * @subsection 初始化
 * 初始化模块开关总控：
 * @code
 * MODCTRL_Status_t err = MODCTRL_Init();
 * if (err != MODCTRL_OK)
 * {
 *     // 初始化失败
 * }
 * @endcode
 * 
 * @subsection 查询模块状态
 * 查询模块是否启用和已初始化：
 * @code
 * // 检查模块是否在编译时启用
 * if (MODCTRL_IsModuleEnabled(MODCTRL_ID_LED))
 * {
 *     // LED模块已启用
 * }
 * 
 * // 检查模块是否在运行时已初始化
 * if (MODCTRL_IsModuleInitialized(MODCTRL_ID_LED))
 * {
 *     // LED模块已初始化
 * }
 * 
 * // 获取模块状态
 * MODCTRL_ModuleState_t state = MODCTRL_GetModuleState(MODCTRL_ID_LED);
 * switch (state)
 * {
 *     case MODCTRL_STATE_DISABLED:
 *         // 模块被禁用
 *         break;
 *     case MODCTRL_STATE_ENABLED:
 *         // 模块已启用但未初始化
 *         break;
 *     case MODCTRL_STATE_INITIALIZED:
 *         // 模块已初始化
 *         break;
 * }
 * @endcode
 * 
 * @subsection 获取模块信息
 * 获取模块详细信息：
 * @code
 * MODCTRL_ModuleInfo_t info;
 * if (MODCTRL_GetModuleInfo(MODCTRL_ID_LED, &info) == MODCTRL_OK)
 * {
 *     // info.id: 模块ID
 *     // info.name: 模块名称
 *     // info.config_enabled: 编译时是否启用
 *     // info.runtime_initialized: 运行时是否已初始化
 *     // info.state: 模块状态
 * }
 * @endcode
 * 
 * @subsection 更新模块状态
 * 模块初始化/反初始化时更新状态：
 * @code
 * // 模块初始化后
 * MODCTRL_UpdateModuleState(MODCTRL_ID_LED, 1);  // 标记为已初始化
 * 
 * // 模块反初始化后
 * MODCTRL_UpdateModuleState(MODCTRL_ID_LED, 0);  // 标记为未初始化
 * @endcode
 * 
 * @subsection 检查依赖关系
 * 检查模块依赖是否满足：
 * @code
 * // 初始化模块前检查依赖
 * if (MODCTRL_CheckDependencies(MODCTRL_ID_LED) == MODCTRL_OK)
 * {
 *     // 依赖满足，可以初始化
 *     LED_Init();
 *     MODCTRL_UpdateModuleState(MODCTRL_ID_LED, 1);
 * }
 * else
 * {
 *     // 依赖未满足，无法初始化
 * }
 * @endcode
 * 
 * @subsection 获取所有模块状态
 * 获取所有模块的状态（用于调试和报告）：
 * @code
 * MODCTRL_ModuleInfo_t info_array[MODCTRL_ID_MAX];
 * uint8_t count = MODCTRL_GetAllModuleStates(info_array, MODCTRL_ID_MAX);
 * 
 * for (uint8_t i = 0; i < count; i++)
 * {
 *     // 处理模块信息
 *     // info_array[i].name, info_array[i].state, etc.
 * }
 * @endcode
 * 
 * @subsection 统计信息
 * 获取模块统计信息：
 * @code
 * uint8_t enabled_count = MODCTRL_GetEnabledModuleCount();      // 已启用的模块数量
 * uint8_t initialized_count = MODCTRL_GetInitializedModuleCount();  // 已初始化的模块数量
 * @endcode
 * 
 * @section 模块依赖关系
 * 
 * 模块依赖关系表（按初始化顺序）：
 * - MODCTRL_ID_BASE_TIMER：无依赖（最基础）
 * - MODCTRL_ID_DELAY：依赖TIM2_TimeBase
 * - MODCTRL_ID_GPIO：无依赖
 * - MODCTRL_ID_LED：依赖GPIO
 * - MODCTRL_ID_OLED：依赖GPIO
 * - MODCTRL_ID_CLOCK_MANAGER：依赖GPIO
 * - MODCTRL_ID_ERROR_HANDLER：无依赖
 * - MODCTRL_ID_LOG：依赖error_handler、TIM2_TimeBase
 * - MODCTRL_ID_IWDG：无依赖
 * - MODCTRL_ID_SYSTEM_INIT：依赖TIM2_TimeBase、Delay、GPIO
 * - MODCTRL_ID_SYSTEM_MONITOR：依赖TIM2_TimeBase、error_handler
 * 
 * @section 注意事项
 * 
 * 1. **初始化顺序**：必须先调用MODCTRL_Init()，才能使用其他函数
 * 2. **状态更新**：模块初始化/反初始化后，需要调用MODCTRL_UpdateModuleState()更新状态
 * 3. **依赖检查**：初始化模块前建议检查依赖关系，确保依赖模块已初始化
 * 4. **编译时状态**：编译时模块开关状态从config.h读取，运行时无法修改
 * 5. **运行时状态**：运行时初始化状态需要手动更新，模块不会自动更新
 * 
 * @section 配置说明
 * 
 * 模块开关在System/config.h中配置：
 * - CONFIG_MODULE_MODULE_CTRL_ENABLED：模块开关总控开关（默认启用）
 * - 其他模块开关：CONFIG_MODULE_XXX_ENABLED
 * 
 * @section 相关模块
 * 
 * - ErrorHandler：错误处理模块（Common/error_handler.c/h）
 */

#ifndef MODULE_CONTROLLER_H
#define MODULE_CONTROLLER_H

#include "error_code.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 模块开关总控错误码
 */
typedef enum {
    MODCTRL_OK = ERROR_OK,                                    /**< 操作成功 */
    MODCTRL_ERROR_NOT_INITIALIZED = ERROR_BASE_MODULE_CTRL - 1, /**< 未初始化 */
    MODCTRL_ERROR_INVALID_MODULE = ERROR_BASE_MODULE_CTRL - 2,  /**< 无效模块ID */
    MODCTRL_ERROR_DEPENDENCY_NOT_MET = ERROR_BASE_MODULE_CTRL - 3, /**< 依赖未满足 */
    MODCTRL_ERROR_ALREADY_INITIALIZED = ERROR_BASE_MODULE_CTRL - 4, /**< 已初始化 */
} MODCTRL_Status_t;

/**
 * @brief 模块ID枚举
 * @note 按初始化顺序排列，依赖关系：后面的模块可能依赖前面的模块
 */
typedef enum {
    MODCTRL_ID_BASE_TIMER = 0,      /**< TIM2_TimeBase时间基准（最基础，无依赖） */
    MODCTRL_ID_DELAY,               /**< delay延时模块（依赖TIM2_TimeBase） */
    MODCTRL_ID_GPIO,                /**< GPIO模块（基础驱动，无依赖） */
    MODCTRL_ID_LED,                 /**< LED模块（依赖GPIO） */
    MODCTRL_ID_OLED,                /**< OLED模块（依赖GPIO） */
    MODCTRL_ID_CLOCK_MANAGER,       /**< clock_manager时钟管理（依赖GPIO） */
    MODCTRL_ID_ERROR_HANDLER,       /**< error_handler错误处理（基础模块，无依赖） */
    MODCTRL_ID_LOG,                 /**< log日志模块（依赖error_handler、TIM2_TimeBase） */
    MODCTRL_ID_IWDG,                /**< iwdg独立看门狗（无依赖） */
    MODCTRL_ID_SYSTEM_INIT,         /**< system_init系统初始化（依赖其他模块） */
    MODCTRL_ID_SYSTEM_MONITOR,      /**< system_monitor系统监控（依赖TIM2_TimeBase、error_handler、log、clock_manager） */
    MODCTRL_ID_MAX                  /**< 最大模块数量 */
} MODCTRL_ModuleID_t;

/**
 * @brief 模块状态
 */
typedef enum {
    MODCTRL_STATE_DISABLED = 0,     /**< 模块被禁用（编译时） */
    MODCTRL_STATE_ENABLED,          /**< 模块已启用（编译时）但未初始化 */
    MODCTRL_STATE_INITIALIZED,      /**< 模块已初始化（运行时） */
} MODCTRL_ModuleState_t;

/**
 * @brief 模块信息结构体
 */
typedef struct {
    MODCTRL_ModuleID_t id;          /**< 模块ID */
    const char* name;               /**< 模块名称 */
    uint8_t config_enabled;         /**< 编译时是否启用（从config.h读取） */
    uint8_t runtime_initialized;    /**< 运行时是否已初始化 */
    MODCTRL_ModuleState_t state;    /**< 模块状态 */
} MODCTRL_ModuleInfo_t;

/**
 * @brief 初始化模块开关总控
 * @return MODCTRL_Status_t 错误码
 * @note 必须在使用其他函数前调用
 */
MODCTRL_Status_t MODCTRL_Init(void);

/**
 * @brief 反初始化模块开关总控
 * @return MODCTRL_Status_t 错误码
 */
MODCTRL_Status_t MODCTRL_Deinit(void);

/**
 * @brief 检查模块是否在编译时启用（从config.h读取）
 * @param[in] module_id 模块ID
 * @return 1-已启用，0-未启用或无效模块ID
 */
uint8_t MODCTRL_IsModuleEnabled(MODCTRL_ModuleID_t module_id);

/**
 * @brief 检查模块是否在运行时已初始化
 * @param[in] module_id 模块ID
 * @return 1-已初始化，0-未初始化或无效模块ID
 * @note 需要先调用MODCTRL_UpdateModuleState()更新状态
 */
uint8_t MODCTRL_IsModuleInitialized(MODCTRL_ModuleID_t module_id);

/**
 * @brief 获取模块状态
 * @param[in] module_id 模块ID
 * @return MODCTRL_ModuleState_t 模块状态
 */
MODCTRL_ModuleState_t MODCTRL_GetModuleState(MODCTRL_ModuleID_t module_id);

/**
 * @brief 获取模块信息
 * @param[in] module_id 模块ID
 * @param[out] info 模块信息结构体指针
 * @return MODCTRL_Status_t 错误码
 */
MODCTRL_Status_t MODCTRL_GetModuleInfo(MODCTRL_ModuleID_t module_id, MODCTRL_ModuleInfo_t* info);

/**
 * @brief 更新模块初始化状态
 * @param[in] module_id 模块ID
 * @param[in] initialized 是否已初始化（1=已初始化，0=未初始化）
 * @return MODCTRL_Status_t 错误码
 * @note 各模块初始化/反初始化时调用此函数更新状态
 */
MODCTRL_Status_t MODCTRL_UpdateModuleState(MODCTRL_ModuleID_t module_id, uint8_t initialized);

/**
 * @brief 检查模块依赖关系
 * @param[in] module_id 模块ID
 * @return MODCTRL_Status_t 错误码（MODCTRL_OK表示依赖满足，MODCTRL_ERROR_DEPENDENCY_NOT_MET表示依赖未满足）
 * @note 检查该模块的所有依赖模块是否已启用并初始化
 */
MODCTRL_Status_t MODCTRL_CheckDependencies(MODCTRL_ModuleID_t module_id);

/**
 * @brief 获取模块名称
 * @param[in] module_id 模块ID
 * @return 模块名称字符串，无效模块ID返回"UNKNOWN"
 */
const char* MODCTRL_GetModuleName(MODCTRL_ModuleID_t module_id);

/**
 * @brief 获取所有模块状态（用于调试和报告）
 * @param[out] info_array 模块信息数组（必须至少MODCTRL_ID_MAX个元素）
 * @param[in] max_count 数组最大元素数
 * @return 实际填充的模块数量
 */
uint8_t MODCTRL_GetAllModuleStates(MODCTRL_ModuleInfo_t* info_array, uint8_t max_count);

/**
 * @brief 获取已启用的模块数量
 * @return 已启用的模块数量
 */
uint8_t MODCTRL_GetEnabledModuleCount(void);

/**
 * @brief 获取已初始化的模块数量
 * @return 已初始化的模块数量
 */
uint8_t MODCTRL_GetInitializedModuleCount(void);

/* 模块开关检查（与module_controller.c保持一致） */
#ifndef CONFIG_MODULE_MODULE_CTRL_ENABLED
#define CONFIG_MODULE_MODULE_CTRL_ENABLED 1  /* 默认启用 */
#endif

#if !CONFIG_MODULE_MODULE_CTRL_ENABLED
/* 如果模块开关总控被禁用，所有函数都为空操作 */
#define MODCTRL_Init()                      ((MODCTRL_Status_t)ERROR_OK)
#define MODCTRL_Deinit()                    ((MODCTRL_Status_t)ERROR_OK)
#define MODCTRL_IsModuleEnabled(...)        (0)
#define MODCTRL_IsModuleInitialized(...)    (0)
#define MODCTRL_GetModuleState(...)         (MODCTRL_STATE_DISABLED)
#define MODCTRL_GetModuleInfo(...)          ((MODCTRL_Status_t)ERROR_OK)
#define MODCTRL_UpdateModuleState(...)      ((MODCTRL_Status_t)ERROR_OK)
#define MODCTRL_CheckDependencies(...)     ((MODCTRL_Status_t)ERROR_OK)
#define MODCTRL_GetModuleName(...)          ("UNKNOWN")
#define MODCTRL_GetAllModuleStates(...)     (0)
#define MODCTRL_GetEnabledModuleCount()     (0)
#define MODCTRL_GetInitializedModuleCount() (0)
#endif /* CONFIG_MODULE_MODULE_CTRL_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* MODULE_CONTROLLER_H */

