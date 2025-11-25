/**
 * @file assert.c
 * @brief 断言系统实现
 */

#include "assert.h"
#include <stdio.h>

#ifdef LOG_H
#include "log.h"
#endif

void Assert_Failed(const char* expr, const char* file, uint32_t line)
{
    #ifdef LOG_H
    LOG_ERROR("ASSERT", "Assertion failed: %s\n  File: %s\n  Line: %lu", 
              expr, file, (unsigned long)line);
    #else
    /* 如果日志模块未启用，使用空操作 */
    (void)expr;
    (void)file;
    (void)line;
    #endif
    
    /* 进入死循环，等待调试器 */
    while (1)
    {
        /* 可以在这里添加LED闪烁等指示 */
    }
}

void Assert_FailedWithMsg(const char* expr, const char* file, uint32_t line, const char* msg)
{
    #ifdef LOG_H
    LOG_ERROR("ASSERT", "Assertion failed: %s\n  File: %s\n  Line: %lu\n  Message: %s", 
              expr, file, (unsigned long)line, msg);
    #else
    /* 如果日志模块未启用，使用空操作 */
    (void)expr;
    (void)file;
    (void)line;
    (void)msg;
    #endif
    
    /* 进入死循环，等待调试器 */
    while (1)
    {
        /* 可以在这里添加LED闪烁等指示 */
    }
}

