/**
 * @file assert.h
 * @brief 断言系统
 * @version 1.0.0
 * @date 2024-01-01
 * @details 提供断言宏，支持调试/发布模式切换
 */

#ifndef ASSERT_H
#define ASSERT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 断言失败处理函数
 * @param[in] expr 断言表达式字符串
 * @param[in] file 文件名
 * @param[in] line 行号
 */
void Assert_Failed(const char* expr, const char* file, uint32_t line);

/**
 * @brief 断言失败处理函数（带消息）
 * @param[in] expr 断言表达式字符串
 * @param[in] file 文件名
 * @param[in] line 行号
 * @param[in] msg 自定义消息
 */
void Assert_FailedWithMsg(const char* expr, const char* file, uint32_t line, const char* msg);

/* 断言配置 */
#ifndef ASSERT_ENABLE
#define ASSERT_ENABLE 1  /**< 默认启用断言（调试模式） */
#endif

#if ASSERT_ENABLE
    /* 调试模式：启用断言 */
    #define ASSERT(expr) \
        do { \
            if (!(expr)) { \
                Assert_Failed(#expr, __FILE__, __LINE__); \
            } \
        } while(0)
    
    #define ASSERT_MSG(expr, msg) \
        do { \
            if (!(expr)) { \
                Assert_FailedWithMsg(#expr, __FILE__, __LINE__, msg); \
            } \
        } while(0)
#else
    /* 发布模式：禁用断言 */
    #define ASSERT(expr) ((void)0)
    #define ASSERT_MSG(expr, msg) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* ASSERT_H */
