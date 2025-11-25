/**
 * @file main_example.c
 * @brief UART01 - 错误日志输出示例
 * @details 演示如何通过UART输出错误日志，展示UART模块、Debug模块、Log模块和ErrorHandler模块的完整集成使用
 * @version 1.0.0
 * @date 2024-01-01
 *
 * 功能说明：
 * - 演示UART模块的基本使用方法（初始化、发送）
 * - 演示Debug模块与UART的集成（实现UART输出功能）
 * - 演示Log模块的分级日志输出（DEBUG/INFO/WARN/ERROR）
 * - 演示ErrorHandler模块的错误处理机制
 * - 演示各种错误场景的处理方法（UART错误、参数错误、自定义错误等）
 * - 演示错误统计功能的使用
 *
 * 模块依赖：
 * - uart：UART驱动模块
 * - debug：Debug模块（UART输出功能已集成到Debug模块）
 * - log：日志模块
 * - error_handler：错误处理模块
 * - error_code：错误码定义模块
 * - delay：延时模块
 * - system_init：系统初始化模块
 * - oled_ssd1306：OLED显示模块
 *
 * 硬件要求：
 * - USART1：PA9(TX), PA10(RX)
 * - 串口助手：115200波特率，8N1格式，GB2312编码
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "error_code.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 测试阶段计数器 */
static uint8_t g_test_phase = 0;

/**
 * @brief 测试阶段1：系统初始化
 */
static void TestPhase1_SystemInit(void)
{
    OLED_Status_t oled_status;
    
    LOG_INFO("MAIN", "=== UART01 错误日志输出示例 ===");
    LOG_INFO("MAIN", "系统初始化完成");
    LOG_INFO("MAIN", "UART1已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "Debug模块已初始化: UART模式");
    LOG_INFO("MAIN", "Log模块已初始化");
    LOG_INFO("MAIN", "ErrorHandler模块已初始化");
    
    /* 初始化OLED */
    oled_status = OLED_Init();
    if (oled_status == OLED_OK)
    {
        OLED_Clear();
        OLED_ShowString(1, 1, "UART01 Demo");
        OLED_ShowString(2, 1, "Error Log");
        OLED_ShowString(3, 1, "Output Test");
        OLED_ShowString(4, 1, "OLED OK!");
        LOG_INFO("MAIN", "OLED已初始化并显示");
    }
    else
    {
        LOG_ERROR("MAIN", "OLED初始化失败: %d", oled_status);
        ErrorHandler_Handle(oled_status, "OLED");
    }
    
    Delay_ms(2000);
}

/**
 * @brief 测试阶段2：各种级别的日志输出
 */
static void TestPhase2_LogLevels(void)
{
    LOG_INFO("MAIN", "=== 测试阶段2: 日志级别 ===");
    
    /* 更新OLED显示 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Phase 2:");
    OLED_ShowString(2, 1, "Log Levels");
    OLED_ShowString(3, 1, "Testing");
    
    Delay_ms(1000);
    
    LOG_DEBUG("MAIN", "这是一条DEBUG消息 (级别 0)");
    OLED_ShowString(4, 1, "DEBUG OK");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "这是一条INFO消息 (级别 1)");
    OLED_ShowString(4, 1, "INFO  OK");
    Delay_ms(500);
    
    LOG_WARN("MAIN", "这是一条WARN消息 (级别 2)");
    OLED_ShowString(4, 1, "WARN  OK");
    Delay_ms(500);
    
    LOG_ERROR("MAIN", "这是一条ERROR消息 (级别 3)");
    OLED_ShowString(4, 1, "ERROR OK");
    Delay_ms(2000);
    
    LOG_INFO("MAIN", "所有日志级别已演示");
    OLED_ShowString(4, 1, "All Done!");
    Delay_ms(1000);
}

/**
 * @brief 测试阶段3：UART模块错误处理
 */
static void TestPhase3_UARTErrors(void)
{
    UART_Status_t status;
    
    LOG_INFO("MAIN", "=== 测试阶段3: UART错误处理 ===");
    Delay_ms(1000);
    
    /* 测试1：无效实例 */
    LOG_INFO("MAIN", "测试 3.1: 无效实例");
    status = UART_Transmit((UART_Instance_t)99, (const uint8_t *)"test", 4, 0);
    if (status != UART_OK)
    {
        LOG_ERROR("MAIN", "UART错误: 无效实例 (预期结果)");
        ErrorHandler_Handle(status, "UART");
    }
    Delay_ms(1500);
    
    /* 测试2：未初始化（先反初始化UART1） */
    LOG_INFO("MAIN", "测试 3.2: 未初始化");
    UART_Deinit(UART_INSTANCE_1);
    status = UART_Transmit(UART_INSTANCE_1, (const uint8_t *)"test", 4, 0);
    if (status != UART_OK)
    {
        LOG_ERROR("MAIN", "UART错误: 未初始化 (预期结果)");
        ErrorHandler_Handle(status, "UART");
    }
    /* 重新初始化UART */
    UART_Init(UART_INSTANCE_1);
    Delay_ms(1500);
    
    /* 测试3：空指针 */
    LOG_INFO("MAIN", "测试 3.3: 空指针");
    status = UART_Transmit(UART_INSTANCE_1, NULL, 4, 0);
    if (status != UART_OK)
    {
        LOG_ERROR("MAIN", "UART错误: 空指针 (预期结果)");
        ErrorHandler_Handle(status, "UART");
    }
    Delay_ms(1500);
    
    /* 测试4：零长度 */
    LOG_INFO("MAIN", "测试 3.4: 零长度");
    status = UART_Transmit(UART_INSTANCE_1, (const uint8_t *)"test", 0, 0);
    if (status != UART_OK)
    {
        LOG_ERROR("MAIN", "UART错误: 零长度 (预期结果)");
        ErrorHandler_Handle(status, "UART");
    }
    Delay_ms(1500);
    
    LOG_INFO("MAIN", "UART错误测试完成");
    Delay_ms(1000);
}

/**
 * @brief 测试阶段4：参数错误处理
 */
static void TestPhase4_ParamErrors(void)
{
    LOG_INFO("MAIN", "=== 测试阶段4: 参数错误处理 ===");
    Delay_ms(1000);
    
    /* 测试1：Log空指针 */
    LOG_INFO("MAIN", "测试 4.1: Log空指针");
    Log_Status_t log_status = Log_Init(NULL);
    if (log_status != LOG_OK)
    {
        ErrorHandler_Handle(log_status, "LOG");
    }
    Delay_ms(1500);
    
    /* 测试2：ErrorHandler空指针 */
    LOG_INFO("MAIN", "测试 4.2: ErrorHandler空指针");
    ErrorHandler_Handle(ERROR_OK, NULL);  /* 空指针模块名，应该能正常处理 */
    Delay_ms(1500);
    
    LOG_INFO("MAIN", "参数错误测试完成");
    Delay_ms(1000);
}

/**
 * @brief 测试阶段5：自定义错误码
 */
static void TestPhase5_CustomErrors(void)
{
    LOG_INFO("MAIN", "=== 测试阶段5: 自定义错误码 ===");
    Delay_ms(1000);
    
    /* 测试1：UART错误码 */
    LOG_INFO("MAIN", "测试 5.1: UART错误码");
    ErrorHandler_Handle(UART_ERROR_TIMEOUT, "UART");
    Delay_ms(1000);
    ErrorHandler_Handle(UART_ERROR_BUSY, "UART");
    Delay_ms(1000);
    ErrorHandler_Handle(UART_ERROR_ORE, "UART");
    Delay_ms(1000);
    ErrorHandler_Handle(UART_ERROR_FE, "UART");
    Delay_ms(1500);
    
    /* 测试2：通用错误码 */
    LOG_INFO("MAIN", "测试 5.2: 通用错误码");
    ErrorHandler_Handle(ERROR_BASE_UART - 1, "CUSTOM");
    Delay_ms(1000);
    ErrorHandler_Handle(ERROR_NOT_IMPLEMENTED, "CUSTOM");
    Delay_ms(1500);
    
    LOG_INFO("MAIN", "自定义错误测试完成");
    Delay_ms(1000);
}

/**
 * @brief 测试阶段6：错误统计功能
 */
static void TestPhase6_ErrorStats(void)
{
    LOG_INFO("MAIN", "=== 测试阶段6: 错误统计功能 ===");
    Delay_ms(1000);
    
    LOG_INFO("MAIN", "错误统计:");
    
#if defined(CONFIG_ERROR_HANDLER_STATS_EN) && CONFIG_ERROR_HANDLER_STATS_EN
    uint32_t total_errors = ErrorHandler_GetErrorCount();
    LOG_INFO("MAIN", "  总错误数: %lu", (unsigned long)total_errors);
    
    LOG_INFO("MAIN", "错误码字符串:");
    LOG_INFO("MAIN", "  UART_TIMEOUT: %s", ErrorHandler_GetString(UART_ERROR_TIMEOUT));
    LOG_INFO("MAIN", "  UART_BUSY: %s", ErrorHandler_GetString(UART_ERROR_BUSY));
    LOG_INFO("MAIN", "  UART_ORE: %s", ErrorHandler_GetString(UART_ERROR_ORE));
#else
    LOG_INFO("MAIN", "  错误统计功能未启用");
#endif
    
    Delay_ms(2000);
}

/**
 * @brief 测试阶段7：实时日志输出循环
 * @return 1-已完成，0-未完成
 */
static uint8_t TestPhase7_RealtimeLog(void)
{
    static uint32_t counter = 0;
    uint8_t completed = 0;
    char oled_buf[17];
    
    if (counter == 0)
    {
        LOG_INFO("MAIN", "=== 测试阶段7: 实时日志输出 ===");
        LOG_INFO("MAIN", "连续日志输出 (10个循环)");
        
        /* 更新OLED显示 */
        OLED_Clear();
        OLED_ShowString(1, 1, "Phase 7:");
        OLED_ShowString(2, 1, "Real-Time Log");
        Delay_ms(1000);
    }
    
    counter++;
    
    /* 输出不同级别的日志 */
    if (counter % 4 == 1)
    {
        LOG_DEBUG("MAIN", "循环 %lu: DEBUG消息", (unsigned long)counter);
        snprintf(oled_buf, sizeof(oled_buf), "Cycle %lu: DEBUG", (unsigned long)counter);
    }
    else if (counter % 4 == 2)
    {
        LOG_INFO("MAIN", "循环 %lu: INFO消息", (unsigned long)counter);
        snprintf(oled_buf, sizeof(oled_buf), "Cycle %lu: INFO ", (unsigned long)counter);
    }
    else if (counter % 4 == 3)
    {
        LOG_WARN("MAIN", "循环 %lu: WARN消息", (unsigned long)counter);
        snprintf(oled_buf, sizeof(oled_buf), "Cycle %lu: WARN ", (unsigned long)counter);
    }
    else
    {
        LOG_ERROR("MAIN", "循环 %lu: ERROR消息", (unsigned long)counter);
        snprintf(oled_buf, sizeof(oled_buf), "Cycle %lu: ERROR", (unsigned long)counter);
    }
    
    /* 更新OLED显示 */
    OLED_ShowString(3, 1, oled_buf);
    snprintf(oled_buf, sizeof(oled_buf), "Total: %lu/10", (unsigned long)counter);
    OLED_ShowString(4, 1, oled_buf);
    
    Delay_ms(500);
    
    /* 完成10个循环后，进入下一阶段 */
    if (counter >= 10)
    {
        counter = 0;
        LOG_INFO("MAIN", "实时日志测试完成");
        OLED_ShowString(4, 1, "Completed!");
        Delay_ms(1000);
        completed = 1;
    }
    
    return completed;
}

int main(void)
{
    UART_Status_t uart_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    
    /* 1. 系统初始化 */
    System_Init();
    
    /* 2. UART初始化 */
    uart_status = UART_Init(UART_INSTANCE_1);
    if (uart_status != UART_OK)
    {
        /* UART初始化失败，死循环 */
        while(1) { Delay_ms(1000); }
    }
    
    /* 3. Debug模块初始化（UART模式） */
    debug_status = Debug_Init(DEBUG_MODE_UART, 115200);
    if (debug_status != 0)
    {
        /* Debug初始化失败，死循环 */
        while(1) { Delay_ms(1000); }
    }
    
    /* 4. Log模块初始化 */
    memset(&log_config, 0, sizeof(log_config));
    log_config.level = LOG_LEVEL_DEBUG;
    log_config.enable_timestamp = 0;
    log_config.enable_module = 1;
    log_config.enable_color = 0;  /* 禁用颜色输出 */
    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK)
    {
        ErrorHandler_Handle(log_status, "LOG");
    }
    
    /* 5. 主循环：循环执行7个测试阶段 */
    while (1)
    {
        switch (g_test_phase)
        {
            case 0:
                TestPhase1_SystemInit();
                g_test_phase = 1;
                break;
                
            case 1:
                TestPhase2_LogLevels();
                g_test_phase = 2;
                break;
                
            case 2:
                TestPhase3_UARTErrors();
                g_test_phase = 3;
                break;
                
            case 3:
                TestPhase4_ParamErrors();
                g_test_phase = 4;
                break;
                
            case 4:
                TestPhase5_CustomErrors();
                g_test_phase = 5;
                break;
                
            case 5:
                TestPhase6_ErrorStats();
                g_test_phase = 6;
                break;
                
            case 6:
                if (TestPhase7_RealtimeLog())
                {
                    g_test_phase = 0;  /* 循环回到阶段1 */
                }
                break;
                
            default:
                g_test_phase = 0;
                break;
        }
        
        Delay_ms(10);
    }
}
