/**
 * @file oled02_tests.h
 * @brief OLED02中文显示测试函数声明
 * @version 1.0.0
 * @date 2024-01-01
 * @details OLED02案例的所有中文显示测试函数声明
 */

#ifndef OLED02_TESTS_H
#define OLED02_TESTS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 包含依赖 ==================== */
#include "config.h"  /* 必须在包含oled_ssd1306.h之前，以定义CONFIG_MODULE_FS_WRAPPER_ENABLED */
#include "oled_ssd1306.h"
#include "oled_font_chinese16x16.h"

/**
 * @brief 演示：中文显示演示（自动循环方法1-15）
 */
void test1_single_chinese_char(void);

/**
 * @brief 运行OLED02中文显示演示
 */
void run_all_oled02_tests(void);

#ifdef __cplusplus
}
#endif

#endif /* OLED02_TESTS_H */
