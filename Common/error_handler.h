/**
 * @file error_handler.h
 * @brief 统一错误处理模块
 * @version 2.1.0
 * @date 2024-01-01
 * @details 提供错误码转换、错误日志、错误回调等功能
 * 
 * @note 配置管理：
 * - 模块开关：通过 system/config.h 中的 CONFIG_MODULE_ERROR_HANDLER_ENABLED 控制
 * - 统计功能：通过 system/config.h 中的 CONFIG_ERROR_HANDLER_STATS_EN 控制
 * 
 * @warning 回调函数使用限制：
 * - 禁止在回调中调用 ErrorHandler_Handle()（会导致嵌套死锁）
 * - 禁止在回调中执行阻塞操作（影响实时性）
 * - 禁止在ISR中调用 ErrorHandler_Handle()（除非标记为ISR-safe）
 * - 回调函数应尽可能简短，避免影响错误处理性能
 */

#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include "error_code.h"

/* 系统配置（如果存在） */
#ifdef CONFIG_H
#include "../System/config.h"
#else
#include "config.h"
#endif

/* 错误统计功能开关（从config.h映射） */
#ifdef CONFIG_ERROR_HANDLER_STATS_EN
#if CONFIG_ERROR_HANDLER_STATS_EN
#define ERROR_HANDLER_ENABLE_STATS
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 错误回调函数类型
 * @param[in] error_code 错误码
 * @param[in] module_name 模块名称（可选）
 * 
 * @warning 回调函数限制：
 * - 禁止调用 ErrorHandler_Handle()（嵌套死锁风险）
 * - 禁止执行阻塞操作（影响实时性）
 * - 应尽可能简短，快速返回
 * 
 * @example
 * void MyErrorCallback(error_code_t code, const char* module) {
 *     // 正确：简单的LED闪烁
 *     LED1_On();
 *     Delay_ms(100);
 *     LED1_Off();
 *     
 *     // 错误：调用 ErrorHandler_Handle() 会导致嵌套
 *     // ErrorHandler_Handle(code, module);
 *     
 *     // 错误：阻塞操作影响实时性（死循环）
 *     // while(1) { }
 * }
 */
typedef void (*error_callback_t)(error_code_t error_code, const char* module_name);

/**
 * @brief 注册错误回调函数
 * @param[in] callback 回调函数指针，NULL表示取消注册
 * @return 错误码
 */
error_code_t ErrorHandler_RegisterCallback(error_callback_t callback);

/**
 * @brief 处理错误（记录日志并调用回调）
 * @param[in] error_code 错误码
 * @param[in] module_name 模块名称（可选，可为NULL）
 * @return 错误码（原样返回）
 */
error_code_t ErrorHandler_Handle(error_code_t error_code, const char* module_name);

/**
 * @brief 将错误码转换为字符串
 * @param[in] error_code 错误码
 * @return 错误描述字符串
 */
const char* ErrorHandler_GetString(error_code_t error_code);

/**
 * @brief 检查错误码并处理
 * @param[in] error_code 错误码
 * @param[in] module_name 模块名称
 * @return true-有错误，false-无错误
 */
uint8_t ErrorHandler_Check(error_code_t error_code, const char* module_name);

#ifdef ERROR_HANDLER_ENABLE_STATS
/**
 * @brief 获取错误统计总数
 * @return 错误总数
 */
uint32_t ErrorHandler_GetErrorCount(void);

/**
 * @brief 重置错误统计
 */
void ErrorHandler_ResetStats(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ERROR_HANDLER_H */
