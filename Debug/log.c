/**
 * @file log.c
 * @brief 分级日志系统实现（支持多种输出模块）
 * @version 2.0.0
 * @date 2024-01-01
 * @details 提供分级日志功能，支持DEBUG/INFO/WARN/ERROR等级，集成多种输出方式及配置功能
 */

#include "log.h"
#include "error_code.h"
#include "error_handler.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* 系统配置（可选包含） */
#ifdef CONFIG_H
#include "../System/config.h"
#endif

/* Debug输出模块（可选包含） */
#include "debug.h"

/* 确保Debug_PutChar函数可用（外部声明，防止链接错误） */
extern int Debug_PutChar(int ch);

/* 时间戳支持（需要TIM2_TimeBase模块） */
#ifdef TIM2_TIMEBASE_H
#include "TIM2_TimeBase.h"
#endif

/* 模块开关检查 */
#ifndef CONFIG_MODULE_LOG_ENABLED
#define CONFIG_MODULE_LOG_ENABLED 1  /* 默认启用 */
#endif

#if !CONFIG_MODULE_LOG_ENABLED
/* 如果日志模块被禁用，所有日志宏都为空操作 */
#define Log_Print(...) ((void)0)
#else

/* 日志配置 */
static log_config_t g_log_config = {
    .level = LOG_LEVEL_INFO,
    .enable_timestamp = 0,
    .enable_module = 1,
    .enable_color = 0,
};

/* 初始化标志 */
static uint8_t g_log_initialized = 0;

/* 日志级别字符串 */
static const char* g_level_strings[] = {
    "DEBUG",
    "INFO ",
    "WARN ",
    "ERROR",
};

/* 日志级别颜色码（ANSI转义码，需要终端支持） */
static const char* g_level_colors[] = {
    "\033[36m",  /* DEBUG: 青色 */
    "\033[32m",  /* INFO:  绿色 */
    "\033[33m",  /* WARN:  黄色 */
    "\033[31m",  /* ERROR: 红色 */
};
static const char* g_color_reset = "\033[0m";

/**
 * @brief 初始化日志系统
 * @param[in] config 日志配置，若为NULL，使用默认配置
 * @return Log_Status_t 状态码
 */
Log_Status_t Log_Init(const log_config_t* config)
{
    /* 检查是否已经初始化（允许重复初始化，重新配置） */
    /* 如果需要禁止重复初始化，取消下面注释 */
    /* if (g_log_initialized) { return LOG_OK; } */
    
    /* 参数校验 */
    if (config != NULL)
    {
        /* 检查日志级别是否有效（开发阶段，检查范围） */
        if (config->level > LOG_LEVEL_NONE)
        {
            return LOG_ERROR_INVALID_PARAM;
        }
        /* 注意：枚举类型通常不会 < 0，但为了方便开发，也可以检查 */
        /* 因为 log_level_t 是枚举类型，编译器会保证值在有效范围内 */
        
        memcpy(&g_log_config, config, sizeof(log_config_t));
    }
    else
    {
        /* 使用默认配置 */
        g_log_config.level = LOG_LEVEL_INFO;
        g_log_config.enable_timestamp = 0;
        g_log_config.enable_module = 1;
        g_log_config.enable_color = 0;
        
        /* 从config.h获取配置（如果存在） */
#ifdef CONFIG_LOG_LEVEL
        log_level_t config_level = (log_level_t)CONFIG_LOG_LEVEL;
        /* 验证配置的日志级别是否有效（config_level <= LOG_LEVEL_NONE） */
        if (config_level <= LOG_LEVEL_NONE)
        {
            g_log_config.level = config_level;
        }
        /* 如果无效，使用默认值 LOG_LEVEL_INFO（不报错，使用默认） */
#endif
#ifdef CONFIG_LOG_TIMESTAMP_EN
        g_log_config.enable_timestamp = CONFIG_LOG_TIMESTAMP_EN;
#endif
#ifdef CONFIG_LOG_MODULE_EN
        g_log_config.enable_module = CONFIG_LOG_MODULE_EN;
#endif
#ifdef CONFIG_LOG_COLOR_EN
        g_log_config.enable_color = CONFIG_LOG_COLOR_EN;
#endif
    }
    
    g_log_initialized = 1;
    return LOG_OK;
}

/**
 * @brief 反初始化日志系统
 * @return Log_Status_t 状态码
 */
Log_Status_t Log_Deinit(void)
{
    if (!g_log_initialized)
    {
        return LOG_ERROR_NOT_INITIALIZED;
    }
    
    g_log_config.level = LOG_LEVEL_NONE;
    g_log_initialized = 0;
    return LOG_OK;
}

/**
 * @brief 设置日志级别
 * @param[in] level 日志级别
 * @return Log_Status_t 状态码
 */
Log_Status_t Log_SetLevel(log_level_t level)
{
    if (!g_log_initialized)
    {
        return LOG_ERROR_NOT_INITIALIZED;
    }
    
    if (level > LOG_LEVEL_NONE)
    {
        return LOG_ERROR_INVALID_PARAM;
    }
    
    g_log_config.level = level;
    return LOG_OK;
}

/**
 * @brief 获取当前日志级别
 * @return 日志级别
 */
log_level_t Log_GetLevel(void)
{
    return g_log_config.level;
}

/**
 * @brief 检查日志系统是否已初始化
 * @return 1-已初始化，0-未初始化
 */
uint8_t Log_IsInitialized(void)
{
    return g_log_initialized;
}

/**
 * @brief 打印日志（内部使用）
 * @param[in] level 日志级别
 * @param[in] module 模块名称，可为NULL
 * @param[in] format 格式字符串
 * @param[in] ... 参数列表
 */
void Log_Print(log_level_t level, const char* module, const char* format, ...)
{
    char buffer[256];
    int len = 0;
    va_list args;
    
    /* 检查是否已初始化 */
    if (!g_log_initialized)
    {
        return;  /* 未初始化，静默失败 */
    }
    
    /* 检查日志级别 */
    /* 检查1：level 必须 < LOG_LEVEL_NONE */
    if (level >= LOG_LEVEL_NONE)
    {
        return;  /* 无效的日志级别，不输出 */
    }
    /* 检查2：level 必须 >= g_log_config.level（级别越高，数值越小） */
    if (level < g_log_config.level)
    {
        return;  /* 级别太低，不输出 */
    }
    
    /* 参数校验 */
    if (format == NULL)
    {
        ErrorHandler_Handle(LOG_ERROR_INVALID_PARAM, "LOG");
        return;
    }
    
    /* 构建日志前缀 */
    /* 1. 颜色码（如果启用） */
    if (g_log_config.enable_color && level < LOG_LEVEL_NONE)
    {
        len = snprintf(buffer, sizeof(buffer), "%s", g_level_colors[level]);
        if (len < 0 || len >= (int)sizeof(buffer))
        {
            ErrorHandler_Handle(LOG_ERROR_BUFFER_OVERFLOW, "LOG");
            return;
        }
    }
    
    /* 2. 日志级别 */
    len += snprintf(buffer + len, sizeof(buffer) - len, "[%s]", g_level_strings[level]);
    if (len < 0 || len >= (int)sizeof(buffer))
    {
        ErrorHandler_Handle(LOG_ERROR_BUFFER_OVERFLOW, "LOG");
        return;
    }
    
    /* 3. 模块名称（如果启用） */
    if (g_log_config.enable_module && module != NULL)
    {
        len += snprintf(buffer + len, sizeof(buffer) - len, "[%s] ", module);
        if (len < 0 || len >= (int)sizeof(buffer))
        {
            ErrorHandler_Handle(LOG_ERROR_BUFFER_OVERFLOW, "LOG");
            return;
        }
    }
    
    /* 4. 时间戳（如果启用） */
    if (g_log_config.enable_timestamp)
    {
#ifdef BASE_TIM2_H
        uint32_t tick = TIM2_TimeBase_GetTick();
        len += snprintf(buffer + len, sizeof(buffer) - len, "[T+%lu] ", (unsigned long)tick);
#else
        len += snprintf(buffer + len, sizeof(buffer) - len, "[T+0] ");
#endif
        if (len < 0 || len >= (int)sizeof(buffer))
        {
            ErrorHandler_Handle(LOG_ERROR_BUFFER_OVERFLOW, "LOG");
            return;
        }
    }
    
    /* 5. 格式化日志内容 */
    va_start(args, format);
    len += vsnprintf(buffer + len, sizeof(buffer) - len, format, args);
    
    if (len < 0 || len >= (int)sizeof(buffer))
    {
        va_end(args);  /* 出错时也要清理va_list */
        ErrorHandler_Handle(LOG_ERROR_BUFFER_OVERFLOW, "LOG");
        return;
    }
    
    va_end(args);  /* 正常结束也要清理va_list */
    
    /* 6. 颜色重置（如果启用） */
    if (g_log_config.enable_color && level < LOG_LEVEL_NONE)
    {
        len += snprintf(buffer + len, sizeof(buffer) - len, "%s", g_color_reset);
        if (len < 0 || len >= (int)sizeof(buffer))
        {
            ErrorHandler_Handle(LOG_ERROR_BUFFER_OVERFLOW, "LOG");
            return;
        }
    }
    
    /* 7. 添加换行符（Windows风格\r\n） */
    if (len < (int)sizeof(buffer) - 2)
    {
        buffer[len++] = '\r';
        buffer[len++] = '\n';
        buffer[len] = '\0';
    }
    else if (len < (int)sizeof(buffer) - 1)
    {
        /* 只能添加一个字符 */
        buffer[len++] = '\n';
        buffer[len] = '\0';
        ErrorHandler_Handle(LOG_ERROR_BUFFER_OVERFLOW, "LOG");
    }
    else
    {
        /* 缓冲区已满，强制截断 */
        buffer[sizeof(buffer) - 3] = '\r';
        buffer[sizeof(buffer) - 2] = '\n';
        buffer[sizeof(buffer) - 1] = '\0';
        ErrorHandler_Handle(LOG_ERROR_BUFFER_OVERFLOW, "LOG");
    }
    
    /* 8. 输出日志（通过debug模块） */
    /* 注意：debug.h已经包含，可以直接使用Debug_PutChar */
    const char* p = buffer;
    while (*p != '\0')
    {
        Debug_PutChar(*p++);
    }
}

#endif /* CONFIG_MODULE_LOG_ENABLED */
