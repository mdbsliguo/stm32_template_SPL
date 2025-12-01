/**
 * @file oled02_tests.c
 * @brief OLED02中文显示测试函数实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details OLED02案例的所有中文显示测试函数实现
 */

#include "config.h"  /* 必须在包含oled_ssd1306.h之前，以定义CONFIG_MODULE_FS_WRAPPER_ENABLED */
#include "oled02_tests.h"
#include "oled_ssd1306.h"
#include "oled_font_chinese16x16.h"
#include "log.h"
#include "delay.h"

/* ==================== 测试函数实现 ==================== */

/**
 * @brief 演示：中文显示演示（自动循环方法1-15）
 * 第1行：方法编号（由OLED_ShowChineseChar自动更新）
 * 第2行：空白
 * 第3行：中文
 * 第4行：显示
 */
void test1_single_chinese_char(void)
{
    LOG_INFO("DEMO", "中文显示演示：自动循环方法1-15");
    
    OLED_Clear();
    
    /* 显示初始方法编号（方法1，会在第一次调用OLED_ShowChineseChar时自动更新第1行） */
    OLED_ShowString(1, 1, "Method:  1         ");
    
    LOG_INFO("DEMO", "演示开始：每3秒自动切换方法，循环显示方法1-15");
    
    /* 一直循环，让方法自动切换（方法1-15，每3秒切换一次） */
    while (1)
    {
        /* 显示中文（使用当前方法，内部会自动检查和切换方法，并更新第1行） */
        OLED_ShowChineseChar(3, 1, 0xD6D0);  /* 中 */
        OLED_ShowChineseChar(3, 3, 0xCEC4);  /* 文 */
        OLED_ShowChineseChar(4, 1, 0xCFD4);  /* 显 */
        OLED_ShowChineseChar(4, 3, 0xCABE);  /* 示 */
        
        Delay_ms(200);  /* 短暂延时，避免过于频繁刷新 */
    }
}

/**
 * @brief 运行OLED02中文显示演示
 */
void run_all_oled02_tests(void)
{
    LOG_INFO("OLED02", "=== 开始中文显示演示 ===");
    
    /* 只运行一个演示 */
    test1_single_chinese_char();
    
    LOG_INFO("OLED02", "=== 中文显示演示完成 ===");
}

