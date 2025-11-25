/**
 * @file log.h
 * @brief 分级日志系统（快速开发工程模板）
 * @version 2.0.0
 * @date 2024-01-01
 * @details 提供分级日志输出，支持DEBUG/INFO/WARN/ERROR级别，集成错误处理、配置管理
 * 
 * @note 配置管理：
 * - 模块开关：通过 system/config.h 中的 CONFIG_MODULE_LOG_ENABLED 控制
 * - 日志级别：通过 system/config.h 中的 CONFIG_LOG_LEVEL 控制
 * - 功能开关：通过 system/config.h 中的 CONFIG_LOG_TIMESTAMP_EN、CONFIG_LOG_MODULE_EN 控制
 */

#ifndef LOG_H
#define LOG_H

#include "error_code.h"
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 日志级别
 */
typedef enum {
    LOG_LEVEL_DEBUG = 0,  /**< 调试信息（最详细） */
    LOG_LEVEL_INFO,       /**< 一般信息 */
    LOG_LEVEL_WARN,       /**< 警告信息 */
    LOG_LEVEL_ERROR,      /**< 错误信息（最重要） */
    LOG_LEVEL_NONE,       /**< 禁用日志 */
} log_level_t;

/**
 * @brief 日志错误码
 */
typedef enum {
    LOG_OK = ERROR_OK,                                    /**< 操作成功 */
    LOG_ERROR_NOT_INITIALIZED = ERROR_BASE_LOG - 1,      /**< 未初始化 */
    LOG_ERROR_INVALID_PARAM = ERROR_BASE_LOG - 2,        /**< 参数非法 */
    LOG_ERROR_BUFFER_OVERFLOW = ERROR_BASE_LOG - 3,      /**< 缓冲区溢出 */
    LOG_ERROR_DEBUG_NOT_READY = ERROR_BASE_LOG - 4,      /**< 调试输出未就绪 */
} Log_Status_t;

/**
 * @brief 日志配置结构体
 */
typedef struct {
    log_level_t level;          /**< 当前日志级别（低于此级别的日志不输出） */
    uint8_t enable_timestamp;  /**< 是否启用时间戳（1=启用，0=禁用） */
    uint8_t enable_module;     /**< 是否显示模块名（1=启用，0=禁用） */
    uint8_t enable_color;      /**< 是否启用颜色输出（1=启用，0=禁用，需要终端支持） */
} log_config_t;

/**
 * @brief 初始化日志系统
 * @param[in] config 日志配置（可为NULL，使用默认配置）
 * @return Log_Status_t 错误码
 * @note 默认配置：LOG_LEVEL_INFO，禁用时间戳，启用模块名，禁用颜色
 */
Log_Status_t Log_Init(const log_config_t* config);

/**
 * @brief 反初始化日志系统
 * @return Log_Status_t 错误码
 */
Log_Status_t Log_Deinit(void);

/**
 * @brief 设置日志级别
 * @param[in] level 日志级别
 * @return Log_Status_t 错误码
 */
Log_Status_t Log_SetLevel(log_level_t level);

/**
 * @brief 获取当前日志级别
 * @return 日志级别
 */
log_level_t Log_GetLevel(void);

/**
 * @brief 检查日志系统是否已初始化
 * @return 1-已初始化，0-未初始化
 */
uint8_t Log_IsInitialized(void);

/* 模块开关检查（与log.c保持一致） */
#ifndef CONFIG_MODULE_LOG_ENABLED
#define CONFIG_MODULE_LOG_ENABLED 1  /* 默认启用 */
#endif

#if !CONFIG_MODULE_LOG_ENABLED
/* 如果日志模块被禁用，所有日志宏都为空操作 */
#define Log_Print(...) ((void)0)
#define LOG_DEBUG(...) ((void)0)
#define LOG_INFO(...)  ((void)0)
#define LOG_WARN(...)  ((void)0)
#define LOG_ERROR(...) ((void)0)
#else

/**
 * @brief 输出日志（内部使用，不直接调用）
 * @param[in] level 日志级别
 * @param[in] module 模块名称（可为NULL）
 * @param[in] format 格式字符串
 * @param[in] ... 参数列表
 * @note 此函数由日志宏调用，不建议直接使用
 */
void Log_Print(log_level_t level, const char* module, const char* format, ...);

/* ==================== 日志宏定义 ==================== */

/**
 * @brief DEBUG级别日志
 * @param[in] module 模块名称
 * @param[in] fmt 格式字符串
 * @param[in] ... 参数列表
 * @example LOG_DEBUG("MAIN", "System initialized, count=%d", count);
 */
#define LOG_DEBUG(module, fmt, ...)  Log_Print(LOG_LEVEL_DEBUG, module, fmt, ##__VA_ARGS__)

/**
 * @brief INFO级别日志
 * @param[in] module 模块名称
 * @param[in] fmt 格式字符串
 * @param[in] ... 参数列表
 * @example LOG_INFO("MAIN", "System ready");
 */
#define LOG_INFO(module, fmt, ...)   Log_Print(LOG_LEVEL_INFO, module, fmt, ##__VA_ARGS__)

/**
 * @brief WARN级别日志
 * @param[in] module 模块名称
 * @param[in] fmt 格式字符串
 * @param[in] ... 参数列表
 * @example LOG_WARN("UART", "Buffer full, dropping data");
 */
#define LOG_WARN(module, fmt, ...)   Log_Print(LOG_LEVEL_WARN, module, fmt, ##__VA_ARGS__)

/**
 * @brief ERROR级别日志
 * @param[in] module 模块名称
 * @param[in] fmt 格式字符串
 * @param[in] ... 参数列表
 * @example LOG_ERROR("ADC", "Conversion failed: %d", error_code);
 */
#define LOG_ERROR(module, fmt, ...)  Log_Print(LOG_LEVEL_ERROR, module, fmt, ##__VA_ARGS__)

#endif /* CONFIG_MODULE_LOG_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* LOG_H */
