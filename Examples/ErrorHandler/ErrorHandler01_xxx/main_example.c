/**
 * @file main_example.c
 * @brief 案例6 - 错误处理模块测试
 * @details 测试error_handler模块的完整错误处理流程
 * @status 待测试
 *
 * 功能说明：
 * - 测试OLED模块的错误处理（无效参数、未初始化等）
 * - 测试LED模块的错误处理（无效ID、禁用LED等）
 * - 测试clock_manager模块的错误处理（未初始化、无效频率等）
 * - 展示错误回调功能
 * - 展示错误码到字符串转换
 * - OLED显示错误信息和错误码
 * - LED1：错误发生时闪烁提示
 *
 * 模块依赖：
 * - error_handler：统一错误处理模块
 * - error_code：错误码定义
 * - OLED：SSD1306软件I2C驱动
 * - LED：LED驱动模块
 * - clock_manager：时钟管理模块
 * - delay：延时模块
 *
 * 硬件要求：
 * - LED1连接到PA1（错误提示）
 * - OLED显示屏（SSD1306，I2C接口）
 *   - SCL连接到PB8
 *   - SDA连接到PB9
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "led.h"
#include "delay.h"
#include "clock_manager.h"
#include "TIM2_TimeBase.h"
#include "oled_ssd1306.h"
#include "error_handler.h"
#include "error_code.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 测试阶段计数器 */
static uint8_t g_test_phase = 0;

/* 错误回调函数 */
static void ErrorCallback(error_code_t error_code, const char *module_name)
{
    /* 错误发生时，LED1闪烁提示 */
    LED1_On();
    Delay_ms(100);
    LED1_Off();
}

/**
 * @brief 在OLED上显示错误信息
 * @param[in] line 行号（1~4）
 * @param[in] error_code 错误码
 * @param[in] module_name 模块名称
 */
static void DisplayError(uint8_t line, error_code_t error_code, const char *module_name)
{
    char buf[17];
    const char *err_str = ErrorHandler_GetString(error_code);

    /* 第1行：模块名称和错误码 */
    snprintf(buf, sizeof(buf), "%s:%d", module_name ? module_name : "UNK", (int)error_code);
    OLED_ShowString(line, 1, buf);

    /* 第2行：错误描述（如果空间足够，可以显示在下一行） */
    if (line < 4)
    {
        /* 截断错误描述以适应OLED显示（最多16个字符） */
        char desc[17];
        strncpy(desc, err_str, 16);
        desc[16] = '\0'; /* 确保字符串结束 */
        OLED_ShowString(line + 1, 1, desc);
    }
}

/**
 * @brief 处理并显示错误（辅助函数，减少代码重复）
 * @param[in] status 错误码（可以是任何模块的错误码）
 * @param[in] module_name 模块名称
 * @param[in] display_line OLED显示行号
 * @return 1-有错误，0-无错误
 */
static uint8_t HandleAndDisplayError(error_code_t status, const char *module_name, uint8_t display_line)
{
    if (status != ERROR_OK)
    {
        ErrorHandler_Handle(status, module_name);
        DisplayError(display_line, status, module_name);
        return 1;
    }
    return 0;
}

/**
 * @brief 测试阶段1：OLED模块错误处理测试
 */
static void TestPhase1_OLEDErrors(void)
{
    OLED_Status_t status;

    /* 清屏 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Test1: OLED");

    /* 测试1：无效行号（行号>4） */
    Delay_ms(1000);
    OLED_ShowString(1, 1, "Test1.1: Invalid");
    status = OLED_ShowString(5, 1, "Test"); /* 行号5无效 */
    HandleAndDisplayError(status, "OLED", 2);
    Delay_ms(2000);

    /* 测试2：无效列号（列号>16） */
    OLED_ShowString(1, 1, "Test1.2: Invalid");
    status = OLED_ShowString(1, 20, "Test"); /* 列号20无效 */
    HandleAndDisplayError(status, "OLED", 2);
    Delay_ms(2000);

    /* 测试3：空指针参数 */
    OLED_ShowString(1, 1, "Test1.3: NULL");
    status = OLED_ShowString(1, 1, NULL); /* 空指针 */
    HandleAndDisplayError(status, "OLED", 2);
    Delay_ms(2000);
}

/**
 * @brief 测试阶段2：LED模块错误处理测试
 */
static void TestPhase2_LEDErrors(void)
{
    LED_Status_t status;

    /* 清屏 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Test2: LED");

    /* 测试1：无效LED ID（LED_3不存在） */
    Delay_ms(1000);
    OLED_ShowString(1, 1, "Test2.1: Invalid");
    status = LED_On((LED_Number_t)3); /* LED_3不存在，强制类型转换以测试错误处理 */
    HandleAndDisplayError(status, "LED", 2);
    Delay_ms(2000);

    /* 测试2：无效LED ID（LED_0不存在） */
    OLED_ShowString(1, 1, "Test2.2: Invalid");
    status = LED_On((LED_Number_t)0); /* LED_0不存在，强制类型转换以测试错误处理 */
    HandleAndDisplayError(status, "LED", 2);
    Delay_ms(2000);

    /* 测试3：空指针参数（LED_GetState） */
    OLED_ShowString(1, 1, "Test2.3: NULL");
    status = LED_GetState(LED_1, NULL); /* 空指针 */
    HandleAndDisplayError(status, "LED", 2);
    Delay_ms(2000);
}

/**
 * @brief 测试阶段3：clock_manager模块错误处理测试
 */
static void TestPhase3_CLKMErrors(void)
{
    CLKM_ErrorCode_t status;

    /* 清屏 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Test3: CLKM");

    /* 测试1：未初始化时调用函数 */
    Delay_ms(1000);
    OLED_ShowString(1, 1, "Test3.1: NotInit");
    /* 注意：如果CLKM已经初始化，这个测试可能不会触发错误 */
    /* 但我们可以测试其他错误 */
    status = CLKM_SetFixedLevel(CLKM_LVL_72MHZ);
    HandleAndDisplayError(status, "CLKM", 2);
    Delay_ms(2000);

    /* 测试2：模式冲突（在自动模式下设置固定频率） */
    OLED_ShowString(1, 1, "Test3.2: Conflict");
    CLKM_SetMode(CLKM_MODE_AUTO, 0); /* 先设置为自动模式 */
    Delay_ms(100);
    status = CLKM_SetFixedLevel(CLKM_LVL_72MHZ); /* 在自动模式下设置固定频率会失败 */
    HandleAndDisplayError(status, "CLKM", 2);
    Delay_ms(2000);

    /* 恢复手动模式 */
    CLKM_SetMode(CLKM_MODE_MANUAL, 0);
    Delay_ms(100);
}

/**
 * @brief 测试阶段4：错误回调功能测试
 */
static void TestPhase4_ErrorCallback(void)
{
    OLED_Status_t status;

    /* 清屏 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Test4: Callback");

    /* 注意：回调已在main函数中注册，这里测试取消和重新注册 */
    OLED_ShowString(2, 1, "Callback OK");
    Delay_ms(1000);

    /* 触发一个错误，观察回调是否被调用（LED1应该闪烁） */
    OLED_ShowString(1, 1, "Test4.1: Trigger");
    status = OLED_ShowString(5, 1, "Test"); /* 无效行号 */
    HandleAndDisplayError(status, "OLED", 3);
    /* 回调函数应该被调用，LED1应该闪烁 */
    Delay_ms(2000);

    /* 取消注册回调 */
    ErrorHandler_RegisterCallback(NULL);
    OLED_ShowString(1, 1, "Test4.2: Cancel");
    OLED_ShowString(2, 1, "Callback Cancel");
    Delay_ms(2000);

    /* 再次触发错误，回调不应该被调用（LED1不应该闪烁） */
    status = OLED_ShowString(5, 1, "Test");
    HandleAndDisplayError(status, "OLED", 3);
    /* 回调函数不应该被调用，LED1不应该闪烁 */
    Delay_ms(2000);

    /* 重新注册回调（恢复错误提示功能） */
    ErrorHandler_RegisterCallback(ErrorCallback);
    OLED_ShowString(1, 1, "Test4.3: ReReg");
    OLED_ShowString(2, 1, "Callback ReReg");
    Delay_ms(2000);
}

/**
 * @brief 显示错误码字符串（辅助函数）
 * @param[in] error_code 错误码
 * @param[in] line OLED显示行号
 */
static void ShowErrorString(error_code_t error_code, uint8_t line)
{
    char buf[17];
    snprintf(buf, sizeof(buf), "%s", ErrorHandler_GetString(error_code));
    OLED_ShowString(line, 1, buf);
}

/**
 * @brief 测试阶段5：错误码转换功能测试
 */
static void TestPhase5_ErrorString(void)
{
    /* 清屏 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Test5: Strings");

    /* 测试OLED错误码转换 */
    Delay_ms(1000);
    OLED_ShowString(1, 1, "OLED Errors:");
    ShowErrorString(OLED_ERROR_NOT_INITIALIZED, 2);
    Delay_ms(2000);

    ShowErrorString(OLED_ERROR_INVALID_PARAM, 2);
    Delay_ms(2000);

    /* 测试LED错误码转换 */
    OLED_ShowString(1, 1, "LED Errors:");
    ShowErrorString(LED_ERROR_INVALID_ID, 2);
    Delay_ms(2000);

    ShowErrorString(LED_ERROR_DISABLED, 2);
    Delay_ms(2000);

    /* 测试CLKM错误码转换 */
    OLED_ShowString(1, 1, "CLKM Errors:");
    ShowErrorString(CLKM_ERROR_NOT_INIT, 2);
    Delay_ms(2000);

    ShowErrorString(CLKM_ERROR_INVALID_FREQ, 2);
    Delay_ms(2000);
}

/**
 * @brief 测试阶段6：ErrorHandler_Check功能测试
 */
static void TestPhase6_ErrorCheck(void)
{
    OLED_Status_t status;

    /* 清屏 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Test6: Check");

    /* 测试ErrorHandler_Check：成功情况 */
    Delay_ms(1000);
    OLED_ShowString(1, 1, "Test6.1: OK");
    status = OLED_Clear(); /* 应该成功 */
    if (!ErrorHandler_Check(status, "OLED"))
    {
        OLED_ShowString(2, 1, "No Error");
    }
    Delay_ms(2000);

    /* 测试ErrorHandler_Check：错误情况 */
    OLED_ShowString(1, 1, "Test6.2: Error");
    status = OLED_ShowString(5, 1, "Test"); /* 应该失败 */
    if (ErrorHandler_Check(status, "OLED"))
    {
        OLED_ShowString(2, 1, "Error Found");
        DisplayError(3, status, "OLED");
    }
    Delay_ms(2000);
}

int main(void)
{
    OLED_Status_t oled_status;

    /* 系统初始化 */
    System_Init();

    /* LED初始化（在OLED之前，以便OLED失败时可以使用LED提示） */
    LED_Init();

    /* OLED初始化 */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        /* OLED初始化失败，使用LED提示 */
        LED1_On();
        while (1)
        {
            Delay_ms(500);
            LED1_Off();
            Delay_ms(500);
        }
    }

    /* clock_manager初始化 */
    CLKM_Init();
    CLKM_SetMode(CLKM_MODE_MANUAL, 0); /* 设置为手动模式 */

    /* 注册错误回调函数（所有测试阶段的错误都会触发LED1闪烁） */
    ErrorHandler_RegisterCallback(ErrorCallback);

    /* 显示欢迎信息 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Error Handler");
    OLED_ShowString(2, 1, "Test Case 6");
    OLED_ShowString(3, 1, "Press Reset");
    OLED_ShowString(4, 1, "to Start");
    Delay_ms(3000);

    /* 主循环：循环执行各个测试阶段 */
    while (1)
    {
        switch (g_test_phase)
        {
        case 0:
            /* 显示测试阶段信息 */
            OLED_Clear();
            OLED_ShowString(1, 1, "Error Handler");
            OLED_ShowString(2, 1, "Test Case 6");
            OLED_ShowString(3, 1, "6 Test Phases");
            OLED_ShowString(4, 1, "~7s per phase");
            Delay_ms(5000); /* 显示5秒 */
            g_test_phase = 1;
            break;

        case 1:
            TestPhase1_OLEDErrors(); /* 内部包含延时，会阻塞执行 */
            g_test_phase = 2;
            break;

        case 2:
            TestPhase2_LEDErrors(); /* 内部包含延时，会阻塞执行 */
            g_test_phase = 3;
            break;

        case 3:
            TestPhase3_CLKMErrors(); /* 内部包含延时，会阻塞执行 */
            g_test_phase = 4;
            break;

        case 4:
            TestPhase4_ErrorCallback(); /* 内部包含延时，会阻塞执行 */
            g_test_phase = 5;
            break;

        case 5:
            TestPhase5_ErrorString(); /* 内部包含延时，会阻塞执行 */
            g_test_phase = 6;
            break;

        case 6:
            TestPhase6_ErrorCheck(); /* 内部包含延时，会阻塞执行 */
            g_test_phase = 0;        /* 循环回到阶段0 */
            break;

        default:
            g_test_phase = 0;
            break;
        }
    }
}
