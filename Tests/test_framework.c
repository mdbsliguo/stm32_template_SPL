/**
 * @file test_framework.c
 * @brief 测试框架实现
 * @version 1.0.0
 * @date 2024-01-01
 */

#include "test_framework.h"
#include <string.h>
#include <stdio.h>

/* 直接包含必要的头文件，不依赖条件编译 */
#include "oled_ssd1306.h"
#include "led.h"
#include "delay.h"

#ifdef LOG_H
#include "log.h"
#endif

/* 测试框架状态 */
static struct {
    TestResult_t result;         /**< 测试结果统计 */
    uint8_t is_initialized;     /**< 是否已初始化 */
    char current_test[64];       /**< 当前测试用例名称 */
} g_test_framework = {0};

/**
 * @brief 初始化测试框架
 */
void TestFramework_Init(void)
{
    memset(&g_test_framework, 0, sizeof(g_test_framework));
    g_test_framework.is_initialized = 1;
    
    #ifdef LOG_H
    LOG_INFO("TEST", "Test Framework Initialized");
    #endif
}

/**
 * @brief 测试失败处理
 */
void TestFramework_Fail(const char* file, uint32_t line, const char* condition, const char* msg)
{
    if (!g_test_framework.is_initialized)
    {
        TestFramework_Init();
    }
    
    g_test_framework.result.total++;
    g_test_framework.result.failed++;
    
    #ifdef LOG_H
    LOG_ERROR("TEST", "[FAIL] %s:%lu - %s: %s (%s)", 
              file, (unsigned long)line, g_test_framework.current_test, msg, condition);
    #endif
    
    OLED_Clear();
    OLED_ShowString(1, 1, "TEST FAIL");
    OLED_ShowString(2, 1, g_test_framework.current_test);
    if (msg != NULL)
    {
        OLED_ShowString(3, 1, msg);
    }
}

/**
 * @brief 测试通过处理
 */
void TestFramework_Pass(void)
{
    if (!g_test_framework.is_initialized)
    {
        TestFramework_Init();
    }
    
    g_test_framework.result.total++;
    g_test_framework.result.passed++;
    
    #ifdef LOG_H
    LOG_INFO("TEST", "[PASS] %s", g_test_framework.current_test);
    #endif
}

/**
 * @brief 运行单个测试用例
 */
void TestFramework_RunTestCase(const char* name, void (*test_func)(void))
{
    if (name == NULL || test_func == NULL)
    {
        return;
    }
    
    if (!g_test_framework.is_initialized)
    {
        TestFramework_Init();
    }
    
    /* 保存当前测试用例名称 */
    strncpy(g_test_framework.current_test, name, sizeof(g_test_framework.current_test) - 1);
    g_test_framework.current_test[sizeof(g_test_framework.current_test) - 1] = '\0';
    
    #ifdef LOG_H
    LOG_INFO("TEST", "Running: %s", name);
    #endif
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Testing:");
    OLED_ShowString(2, 1, name);
    
    /* 运行测试函数 */
    test_func();
    
    /* 测试完成后短暂延时，确保OLED显示可见 */
    Delay_ms(100);
}

/**
 * @brief 运行所有测试用例
 */
void TestFramework_RunAll(TestCase_t* cases, uint32_t count)
{
    if (cases == NULL || count == 0)
    {
        return;
    }
    
    if (!g_test_framework.is_initialized)
    {
        TestFramework_Init();
    }
    
    for (uint32_t i = 0; i < count; i++)
    {
        if (cases[i].enabled)
        {
            TestFramework_RunTestCase(cases[i].name, cases[i].test_func);
        }
    }
}

/**
 * @brief 获取测试结果
 */
TestResult_t TestFramework_GetResult(void)
{
    return g_test_framework.result;
}

/**
 * @brief 打印测试结果
 */
void TestFramework_PrintResult(void)
{
    TestResult_t result = TestFramework_GetResult();
    
    #ifdef LOG_H
    LOG_INFO("TEST", "=== Test Results ===");
    LOG_INFO("TEST", "Total: %lu", (unsigned long)result.total);
    LOG_INFO("TEST", "Passed: %lu", (unsigned long)result.passed);
    LOG_INFO("TEST", "Failed: %lu", (unsigned long)result.failed);
    
    if (result.total > 0)
    {
        uint32_t pass_rate = (result.passed * 100) / result.total;
        LOG_INFO("TEST", "Pass Rate: %lu%%", (unsigned long)pass_rate);
    }
    #endif
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Test Results:");
    char buf[17];
    
    snprintf(buf, sizeof(buf), "Total: %lu", (unsigned long)result.total);
    OLED_ShowString(2, 1, buf);
    
    if (result.total > 0)
    {
        snprintf(buf, sizeof(buf), "Pass:%lu Fail:%lu", 
                 (unsigned long)result.passed, (unsigned long)result.failed);
        OLED_ShowString(3, 1, buf);
        
        uint32_t pass_rate = (result.passed * 100) / result.total;
        snprintf(buf, sizeof(buf), "Rate: %lu%%", (unsigned long)pass_rate);
        OLED_ShowString(4, 1, buf);
    }
    else
    {
        OLED_ShowString(3, 1, "No tests run");
    }
}

/**
 * @brief 重置测试结果统计
 */
void TestFramework_ResetResult(void)
{
    memset(&g_test_framework.result, 0, sizeof(g_test_framework.result));
    g_test_framework.current_test[0] = '\0';
    
    #ifdef LOG_H
    LOG_INFO("TEST", "Test results reset");
    #endif
}
