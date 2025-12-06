/**
 * @file test_framework.h
 * @brief 测试框架头文件（断言、测试用例管理、结果统计）
 * @version 1.0.0
 * @date 2024-01-01
 * @details 提供统一的测试基础设施，包括断言宏、测试用例管理、结果统计等功能
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 测试结果统计 ==================== */

/**
 * @brief 测试结果结构体
 */
typedef struct {
    uint32_t total;     /**< 总测试数 */
    uint32_t passed;    /**< 通过数 */
    uint32_t failed;    /**< 失败数 */
} TestResult_t;

/**
 * @brief 测试用例结构体
 */
typedef struct {
    const char* name;        /**< 测试用例名称 */
    void (*test_func)(void); /**< 测试函数指针 */
    uint8_t enabled;         /**< 是否启用（1=启用，0=禁用） */
} TestCase_t;

/* ==================== 测试框架接口 ==================== */

/**
 * @brief 初始化测试框架
 * @note 在运行测试前必须调用此函数
 */
void TestFramework_Init(void);

/**
 * @brief 运行单个测试用例
 * @param[in] name 测试用例名称
 * @param[in] test_func 测试函数指针
 * @note 会自动更新测试结果统计
 */
void TestFramework_RunTestCase(const char* name, void (*test_func)(void));

/**
 * @brief 运行所有测试用例
 * @param[in] cases 测试用例数组
 * @param[in] count 测试用例数量
 * @note 只运行enabled=1的测试用例
 */
void TestFramework_RunAll(TestCase_t* cases, uint32_t count);

/**
 * @brief 获取测试结果
 * @return TestResult_t 测试结果结构体
 */
TestResult_t TestFramework_GetResult(void);

/**
 * @brief 打印测试结果（通过OLED或日志）
 */
void TestFramework_PrintResult(void);

/**
 * @brief 重置测试结果统计
 */
void TestFramework_ResetResult(void);

/* ==================== 断言宏 ==================== */

/**
 * @brief 测试失败处理（内部函数）
 * @param[in] file 文件名
 * @param[in] line 行号
 * @param[in] condition 条件字符串
 * @param[in] msg 错误消息
 */
void TestFramework_Fail(const char* file, uint32_t line, const char* condition, const char* msg);

/**
 * @brief 测试通过处理（内部函数）
 */
void TestFramework_Pass(void);

/* 断言宏定义 */
#define TEST_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            TestFramework_Fail(__FILE__, __LINE__, #condition, msg); \
            return; \
        } \
        TestFramework_Pass(); \
    } while(0)

#define TEST_ASSERT_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            TestFramework_Fail(__FILE__, __LINE__, #a " == " #b, msg); \
            return; \
        } \
        TestFramework_Pass(); \
    } while(0)

#define TEST_ASSERT_NE(a, b, msg) \
    do { \
        if ((a) == (b)) { \
            TestFramework_Fail(__FILE__, __LINE__, #a " != " #b, msg); \
            return; \
        } \
        TestFramework_Pass(); \
    } while(0)

#define TEST_ASSERT_GT(a, b, msg) \
    do { \
        if ((a) <= (b)) { \
            TestFramework_Fail(__FILE__, __LINE__, #a " > " #b, msg); \
            return; \
        } \
        TestFramework_Pass(); \
    } while(0)

#define TEST_ASSERT_GE(a, b, msg) \
    do { \
        if ((a) < (b)) { \
            TestFramework_Fail(__FILE__, __LINE__, #a " >= " #b, msg); \
            return; \
        } \
        TestFramework_Pass(); \
    } while(0)

#define TEST_ASSERT_LT(a, b, msg) \
    do { \
        if ((a) >= (b)) { \
            TestFramework_Fail(__FILE__, __LINE__, #a " < " #b, msg); \
            return; \
        } \
        TestFramework_Pass(); \
    } while(0)

#define TEST_ASSERT_LE(a, b, msg) \
    do { \
        if ((a) > (b)) { \
            TestFramework_Fail(__FILE__, __LINE__, #a " <= " #b, msg); \
            return; \
        } \
        TestFramework_Pass(); \
    } while(0)

#define TEST_ASSERT_NULL(ptr, msg) \
    do { \
        if ((ptr) != NULL) { \
            TestFramework_Fail(__FILE__, __LINE__, #ptr " == NULL", msg); \
            return; \
        } \
        TestFramework_Pass(); \
    } while(0)

#define TEST_ASSERT_NOT_NULL(ptr, msg) \
    do { \
        if ((ptr) == NULL) { \
            TestFramework_Fail(__FILE__, __LINE__, #ptr " != NULL", msg); \
            return; \
        } \
        TestFramework_Pass(); \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* TEST_FRAMEWORK_H */
