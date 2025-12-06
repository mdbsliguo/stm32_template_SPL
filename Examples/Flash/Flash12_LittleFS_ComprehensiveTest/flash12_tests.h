/**
 * @file flash12_tests.h
 * @brief Flash12测试函数声明
 * @version 1.0.0
 * @date 2024-01-01
 * @details Flash12案例的所有测试函数声明
 */

#ifndef FLASH12_TESTS_H
#define FLASH12_TESTS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 包含依赖 ==================== */
#include "littlefs_wrapper.h"

/**
 * @brief 测试1：文件系统初始化测试
 */
void test1_filesystem_init(void);

/**
 * @brief 测试2：基础文件操作测试
 */
void test2_basic_file_ops(void);

/**
 * @brief 测试3：文件定位测试
 */
void test3_file_seek(void);

/**
 * @brief 测试4：追加写入测试
 */
void test4_append_write(void);

/**
 * @brief 测试5：文件截断测试
 */
void test5_file_truncate(void);

/**
 * @brief 测试6：文件重命名测试
 */
void test6_file_rename(void);

/**
 * @brief 测试7：文件删除测试
 */
void test7_file_delete(void);

/**
 * @brief 测试8：目录操作测试
 */
void test8_directory_ops(void);

/**
 * @brief 测试9：文件属性测试
 */
void test9_file_attributes(void);

/**
 * @brief 测试10：原子操作测试
 */
void test10_atomic_operations(void);

/**
 * @brief 测试11：断电保护测试
 */
void test11_power_protection(void);

/**
 * @brief 测试12：FS_WRAPPER接口验证（可选）
 */
void test12_fs_wrapper(void);

/**
 * @brief 运行所有Flash12测试
 */
void run_all_flash12_tests(void);

/* ==================== 辅助函数 ==================== */

/**
 * @brief 将LittleFS错误码转换为可读字符串
 */
const char* littlefs_errstr(LittleFS_Status_t err);

/**
 * @brief 检查文件系统状态并显示诊断信息
 */
void check_filesystem_status(void);

#ifdef __cplusplus
}
#endif

#endif /* FLASH12_TESTS_H */
