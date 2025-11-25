/**
 * @file module_controller.c
 * @brief 模块开关总控实现（快速开发工程模板）
 * @version 1.0.0
 * @date 2024-01-01
 * @details 提供统一的模块状态查询、依赖检查、初始化顺序管理等功能
 * 
 * @details 实现说明：
 * - 维护模块名称表、依赖关系表、运行时状态表
 * - 编译时状态从config.h读取，运行时状态由模块更新
 * - 提供依赖关系检查，确保模块按正确顺序初始化
 * - 支持模块状态查询和统计
 */

#include "module_controller.h"
#include "error_code.h"
#include "error_handler.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <config.h>

/* 模块开关检查 */
#ifndef CONFIG_MODULE_MODULE_CTRL_ENABLED
#define CONFIG_MODULE_MODULE_CTRL_ENABLED 1  /* 默认启用 */
#endif

#if !CONFIG_MODULE_MODULE_CTRL_ENABLED
/* 如果模块开关总控被禁用，所有函数都为空操作 */
#else

/* 模块名称表 */
static const char* g_module_names[MODCTRL_ID_MAX] = {
    "BASE_TIMER",      /* MODCTRL_ID_BASE_TIMER */
    "DELAY",           /* MODCTRL_ID_DELAY */
    "GPIO",            /* MODCTRL_ID_GPIO */
    "LED",             /* MODCTRL_ID_LED */
    "OLED",            /* MODCTRL_ID_OLED */
    "CLOCK_MANAGER",   /* MODCTRL_ID_CLOCK_MANAGER */
    "ERROR_HANDLER",   /* MODCTRL_ID_ERROR_HANDLER */
    "LOG",             /* MODCTRL_ID_LOG */
    "IWDG",            /* MODCTRL_ID_IWDG */
    "SYSTEM_INIT",     /* MODCTRL_ID_SYSTEM_INIT */
    "SYSTEM_MONITOR",  /* MODCTRL_ID_SYSTEM_MONITOR */
};

/* 模块依赖关系表（依赖的模块ID数组，以MODCTRL_ID_MAX结尾） */
static const MODCTRL_ModuleID_t g_module_dependencies[MODCTRL_ID_MAX][4] = {
    {MODCTRL_ID_MAX},                          /* MODCTRL_ID_BASE_TIMER：无依赖 */
    {MODCTRL_ID_BASE_TIMER, MODCTRL_ID_MAX},   /* MODCTRL_ID_DELAY：依赖TIM2_TimeBase */
    {MODCTRL_ID_MAX},                          /* MODCTRL_ID_GPIO：无依赖 */
    {MODCTRL_ID_GPIO, MODCTRL_ID_MAX},         /* MODCTRL_ID_LED：依赖GPIO */
    {MODCTRL_ID_GPIO, MODCTRL_ID_MAX},         /* MODCTRL_ID_OLED：依赖GPIO */
    {MODCTRL_ID_GPIO, MODCTRL_ID_MAX},         /* MODCTRL_ID_CLOCK_MANAGER：依赖GPIO */
    {MODCTRL_ID_MAX},                          /* MODCTRL_ID_ERROR_HANDLER：无依赖 */
    {MODCTRL_ID_ERROR_HANDLER, MODCTRL_ID_BASE_TIMER, MODCTRL_ID_MAX}, /* MODCTRL_ID_LOG：依赖error_handler、TIM2_TimeBase */
    {MODCTRL_ID_MAX},                          /* MODCTRL_ID_IWDG：无依赖 */
    {MODCTRL_ID_BASE_TIMER, MODCTRL_ID_DELAY, MODCTRL_ID_GPIO, MODCTRL_ID_MAX}, /* MODCTRL_ID_SYSTEM_INIT：依赖多个模块 */
    {MODCTRL_ID_BASE_TIMER, MODCTRL_ID_ERROR_HANDLER, MODCTRL_ID_MAX}, /* MODCTRL_ID_SYSTEM_MONITOR：依赖TIM2_TimeBase、error_handler（可选log、clock_manager） */
};

/* 模块状态表 */
static struct {
    bool is_initialized;                       /**< 总控是否已初始化 */
    uint8_t runtime_initialized[MODCTRL_ID_MAX]; /**< 运行时初始化状态（1=已初始化，0=未初始化） */
} g_module_ctrl_state = {0};

/**
 * @brief 检查模块是否在编译时启用（从config.h读取）
 * @param[in] module_id 模块ID
 * @return 1-已启用，0-未启用
 */
static uint8_t CheckModuleConfigEnabled(MODCTRL_ModuleID_t module_id)
{
    switch (module_id)
    {
        case MODCTRL_ID_BASE_TIMER:
#ifdef CONFIG_MODULE_BASE_TIMER_ENABLED
            return CONFIG_MODULE_BASE_TIMER_ENABLED ? 1 : 0;
#else
            return 0;
#endif
            
        case MODCTRL_ID_DELAY:
#ifdef CONFIG_MODULE_DELAY_ENABLED
            return CONFIG_MODULE_DELAY_ENABLED ? 1 : 0;
#else
            return 0;
#endif
            
        case MODCTRL_ID_GPIO:
#ifdef CONFIG_MODULE_GPIO_ENABLED
            return CONFIG_MODULE_GPIO_ENABLED ? 1 : 0;
#else
            return 0;
#endif
            
        case MODCTRL_ID_LED:
#ifdef CONFIG_MODULE_LED_ENABLED
            return CONFIG_MODULE_LED_ENABLED ? 1 : 0;
#else
            return 0;
#endif
            
        case MODCTRL_ID_OLED:
#ifdef CONFIG_MODULE_OLED_ENABLED
            return CONFIG_MODULE_OLED_ENABLED ? 1 : 0;
#else
            return 0;
#endif
            
        case MODCTRL_ID_CLOCK_MANAGER:
#ifdef CONFIG_MODULE_CLOCK_MANAGER_ENABLED
            return CONFIG_MODULE_CLOCK_MANAGER_ENABLED ? 1 : 0;
#else
            return 0;
#endif
            
        case MODCTRL_ID_ERROR_HANDLER:
#ifdef CONFIG_MODULE_ERROR_HANDLER_ENABLED
            return CONFIG_MODULE_ERROR_HANDLER_ENABLED ? 1 : 0;
#else
            return 0;
#endif
            
        case MODCTRL_ID_LOG:
#ifdef CONFIG_MODULE_LOG_ENABLED
            return CONFIG_MODULE_LOG_ENABLED ? 1 : 0;
#else
            return 0;
#endif
            
        case MODCTRL_ID_IWDG:
#ifdef CONFIG_MODULE_IWDG_ENABLED
            return CONFIG_MODULE_IWDG_ENABLED ? 1 : 0;
#else
            return 0;
#endif
            
        case MODCTRL_ID_SYSTEM_INIT:
#ifdef CONFIG_MODULE_SYSTEM_INIT_ENABLED
            return CONFIG_MODULE_SYSTEM_INIT_ENABLED ? 1 : 0;
#else
            return 0;
#endif
            
        case MODCTRL_ID_SYSTEM_MONITOR:
#ifdef CONFIG_MODULE_SYSTEM_MONITOR_ENABLED
            return CONFIG_MODULE_SYSTEM_MONITOR_ENABLED ? 1 : 0;
#else
            return 0;
#endif
            
        default:
            return 0;
    }
}

/**
 * @brief 初始化模块开关总控
 * @return MODCTRL_Status_t 错误码
 */
MODCTRL_Status_t MODCTRL_Init(void)
{
    if (g_module_ctrl_state.is_initialized)
    {
        return MODCTRL_OK;  /* 已初始化，直接返回成功 */
    }
    
    /* 初始化运行时状态表 */
    memset(g_module_ctrl_state.runtime_initialized, 0, sizeof(g_module_ctrl_state.runtime_initialized));
    
    g_module_ctrl_state.is_initialized = true;
    
    return MODCTRL_OK;
}

/**
 * @brief 反初始化模块开关总控
 * @return MODCTRL_Status_t 错误码
 */
MODCTRL_Status_t MODCTRL_Deinit(void)
{
    if (!g_module_ctrl_state.is_initialized)
    {
        return MODCTRL_ERROR_NOT_INITIALIZED;
    }
    
    /* 清除运行时状态表 */
    memset(g_module_ctrl_state.runtime_initialized, 0, sizeof(g_module_ctrl_state.runtime_initialized));
    
    g_module_ctrl_state.is_initialized = false;
    
    return MODCTRL_OK;
}

/**
 * @brief 检查模块是否在编译时启用（从config.h读取）
 * @param[in] module_id 模块ID
 * @return 1-已启用，0-未启用或无效模块ID
 */
uint8_t MODCTRL_IsModuleEnabled(MODCTRL_ModuleID_t module_id)
{
    if (module_id >= MODCTRL_ID_MAX)
    {
        return 0;
    }
    
    return CheckModuleConfigEnabled(module_id);
}

/**
 * @brief 检查模块是否在运行时已初始化
 * @param[in] module_id 模块ID
 * @return 1-已初始化，0-未初始化或无效模块ID
 */
uint8_t MODCTRL_IsModuleInitialized(MODCTRL_ModuleID_t module_id)
{
    if (!g_module_ctrl_state.is_initialized)
    {
        return 0;
    }
    
    if (module_id >= MODCTRL_ID_MAX)
    {
        return 0;
    }
    
    return g_module_ctrl_state.runtime_initialized[module_id];
}

/**
 * @brief 获取模块状态
 * @param[in] module_id 模块ID
 * @return MODCTRL_ModuleState_t 模块状态
 */
MODCTRL_ModuleState_t MODCTRL_GetModuleState(MODCTRL_ModuleID_t module_id)
{
    if (module_id >= MODCTRL_ID_MAX)
    {
        return MODCTRL_STATE_DISABLED;
    }
    
    uint8_t config_enabled = CheckModuleConfigEnabled(module_id);
    if (!config_enabled)
    {
        return MODCTRL_STATE_DISABLED;
    }
    
    if (g_module_ctrl_state.is_initialized && g_module_ctrl_state.runtime_initialized[module_id])
    {
        return MODCTRL_STATE_INITIALIZED;
    }
    
    return MODCTRL_STATE_ENABLED;
}

/**
 * @brief 获取模块信息
 * @param[in] module_id 模块ID
 * @param[out] info 模块信息结构体指针
 * @return MODCTRL_Status_t 错误码
 */
MODCTRL_Status_t MODCTRL_GetModuleInfo(MODCTRL_ModuleID_t module_id, MODCTRL_ModuleInfo_t* info)
{
    if (info == NULL)
    {
        return MODCTRL_ERROR_INVALID_MODULE;
    }
    
    if (module_id >= MODCTRL_ID_MAX)
    {
        return MODCTRL_ERROR_INVALID_MODULE;
    }
    
    info->id = module_id;
    info->name = g_module_names[module_id];
    info->config_enabled = CheckModuleConfigEnabled(module_id);
    info->runtime_initialized = g_module_ctrl_state.is_initialized ? 
                                g_module_ctrl_state.runtime_initialized[module_id] : 0;
    info->state = MODCTRL_GetModuleState(module_id);
    
    return MODCTRL_OK;
}

/**
 * @brief 更新模块初始化状态
 * @param[in] module_id 模块ID
 * @param[in] initialized 是否已初始化（1=已初始化，0=未初始化）
 * @return MODCTRL_Status_t 错误码
 */
MODCTRL_Status_t MODCTRL_UpdateModuleState(MODCTRL_ModuleID_t module_id, uint8_t initialized)
{
    if (!g_module_ctrl_state.is_initialized)
    {
        return MODCTRL_ERROR_NOT_INITIALIZED;
    }
    
    if (module_id >= MODCTRL_ID_MAX)
    {
        return MODCTRL_ERROR_INVALID_MODULE;
    }
    
    g_module_ctrl_state.runtime_initialized[module_id] = initialized ? 1 : 0;
    
    return MODCTRL_OK;
}

/**
 * @brief 检查模块依赖关系
 * @param[in] module_id 模块ID
 * @return MODCTRL_Status_t 错误码（MODCTRL_OK表示依赖满足，MODCTRL_ERROR_DEPENDENCY_NOT_MET表示依赖未满足）
 */
MODCTRL_Status_t MODCTRL_CheckDependencies(MODCTRL_ModuleID_t module_id)
{
    if (!g_module_ctrl_state.is_initialized)
    {
        return MODCTRL_ERROR_NOT_INITIALIZED;
    }
    
    if (module_id >= MODCTRL_ID_MAX)
    {
        return MODCTRL_ERROR_INVALID_MODULE;
    }
    
    const MODCTRL_ModuleID_t* deps = g_module_dependencies[module_id];
    
    /* 检查每个依赖模块 */
    for (uint8_t i = 0; i < 4; i++)  /* 最多4个依赖 */
    {
        if (deps[i] == MODCTRL_ID_MAX)
        {
            break;  /* 依赖列表结束 */
        }
        
        MODCTRL_ModuleID_t dep_id = deps[i];
        
        /* 检查依赖模块ID是否有效（防御性编程） */
        if (dep_id >= MODCTRL_ID_MAX)
        {
            ErrorHandler_Handle(MODCTRL_ERROR_INVALID_MODULE, "MODCTRL");
            return MODCTRL_ERROR_INVALID_MODULE;
        }
        
        /* 检查自依赖（模块不能依赖自己） */
        if (dep_id == module_id)
        {
            ErrorHandler_Handle(MODCTRL_ERROR_DEPENDENCY_NOT_MET, "MODCTRL");
            return MODCTRL_ERROR_DEPENDENCY_NOT_MET;
        }
        
        /* 检查依赖模块是否启用 */
        if (!MODCTRL_IsModuleEnabled(dep_id))
        {
            ErrorHandler_Handle(MODCTRL_ERROR_DEPENDENCY_NOT_MET, "MODCTRL");
            return MODCTRL_ERROR_DEPENDENCY_NOT_MET;
        }
        
        /* 检查依赖模块是否已初始化 */
        if (!MODCTRL_IsModuleInitialized(dep_id))
        {
            ErrorHandler_Handle(MODCTRL_ERROR_DEPENDENCY_NOT_MET, "MODCTRL");
            return MODCTRL_ERROR_DEPENDENCY_NOT_MET;
        }
    }
    
    return MODCTRL_OK;
}

/**
 * @brief 获取模块名称
 * @param[in] module_id 模块ID
 * @return 模块名称字符串，无效模块ID返回"UNKNOWN"
 */
const char* MODCTRL_GetModuleName(MODCTRL_ModuleID_t module_id)
{
    if (module_id >= MODCTRL_ID_MAX)
    {
        return "UNKNOWN";
    }
    
    return g_module_names[module_id];
}

/**
 * @brief 获取所有模块状态（用于调试和报告）
 * @param[out] info_array 模块信息数组（必须至少MODCTRL_ID_MAX个元素）
 * @param[in] max_count 数组最大元素数
 * @return 实际填充的模块数量
 */
uint8_t MODCTRL_GetAllModuleStates(MODCTRL_ModuleInfo_t* info_array, uint8_t max_count)
{
    if (info_array == NULL || max_count < MODCTRL_ID_MAX)
    {
        return 0;
    }
    
    uint8_t count = 0;
    for (int i = 0; i < (int)MODCTRL_ID_MAX; i++)
    {
        MODCTRL_ModuleID_t id = (MODCTRL_ModuleID_t)i;
        if (MODCTRL_GetModuleInfo(id, &info_array[count]) == MODCTRL_OK)
        {
            count++;
        }
    }
    
    return count;
}

/**
 * @brief 获取已启用的模块数量
 * @return 已启用的模块数量
 */
uint8_t MODCTRL_GetEnabledModuleCount(void)
{
    uint8_t count = 0;
    for (int i = 0; i < (int)MODCTRL_ID_MAX; i++)
    {
        MODCTRL_ModuleID_t id = (MODCTRL_ModuleID_t)i;
        if (MODCTRL_IsModuleEnabled(id))
        {
            count++;
        }
    }
    return count;
}

/**
 * @brief 获取已初始化的模块数量
 * @return 已初始化的模块数量
 */
uint8_t MODCTRL_GetInitializedModuleCount(void)
{
    if (!g_module_ctrl_state.is_initialized)
    {
        return 0;
    }
    
    uint8_t count = 0;
    for (int i = 0; i < (int)MODCTRL_ID_MAX; i++)
    {
        MODCTRL_ModuleID_t id = (MODCTRL_ModuleID_t)i;
        if (MODCTRL_IsModuleInitialized(id))
        {
            count++;
        }
    }
    return count;
}

#endif /* CONFIG_MODULE_MODULE_CTRL_ENABLED */

