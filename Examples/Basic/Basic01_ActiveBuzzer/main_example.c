/**
 * @file main_example.c
 * @brief 案例 - GPIO模式Buzzer基础控制示例
 * @example Examples/Basic/Basic01_ActiveBuzzer/main_example.c
 * @status 已调试完成 ?
 * @details Buzzer基础控制示例，演示GPIO模式的使用，使用OLED显示提示信息
 *
 * 硬件要求：
 * - Buzzer（有源蜂鸣器）连接到PA3
 * - OLED显示屏（SSD1306，I2C接口，用于显示提示信息）
 *   - SCL连接到PB8
 *   - SDA连接到PB9
 *
 * 硬件配置：
 * 在 Examples/Basic/Basic01_ActiveBuzzer/board.h 中配置：
 * ```c
 * #define BUZZER_CONFIGS { \
 *     {BUZZER_MODE_GPIO, GPIOA, GPIO_Pin_3, 1, 0, Bit_RESET, 1},  // GPIO模式，PA3，低电平有效，启用 \
 * }
 * ```
 *
 * 使用方法：
 * 1. 打开案例工程：双击 Examples/Basic/Basic01_ActiveBuzzer/Examples.uvprojx
 * 2. 根据实际硬件修改 Examples/Basic/Basic01_ActiveBuzzer/board.h 中的Buzzer配置
 * 3. 编译运行（F7编译，F8下载）
 * 
 * 注意：本案例是独立工程，无需复制文件
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "error_code.h"
#include "delay.h"
#include "buzzer.h"
#include "oled_ssd1306.h"
#include "i2c_sw.h"
#include <stdint.h>
#include <string.h>

int main(void)
{
    UART_Status_t uart_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    SoftI2C_Status_t i2c_status;
    OLED_Status_t oled_status;
    Buzzer_Status_t buzzer_status;
    
    /* ========== 步骤1：系统初始化 ========== */
    System_Init();
    
    /* ========== 步骤2：UART初始化 ========== */
    uart_status = UART_Init(UART_INSTANCE_1);
    if (uart_status != UART_OK)
    {
        /* UART初始化失败，无法继续，进入死循环 */
        while(1) { Delay_ms(1000); }
    }
    
    /* 等待UART稳定 */
    Delay_ms(100);
    
    /* ========== 步骤3：Debug模块初始化（UART模式） ========== */
    debug_status = Debug_Init(DEBUG_MODE_UART, 115200);
    if (debug_status != 0)
    {
        /* Debug初始化失败，无法继续，进入死循环 */
        while(1) { Delay_ms(1000); }
    }
    
    /* 等待Debug模块稳定 */
    Delay_ms(100);
    
    /* ========== 步骤4：Log模块初始化 ========== */
    memset(&log_config, 0, sizeof(log_config));
    log_config.level = LOG_LEVEL_INFO;         /* 日志级别：INFO（简化输出） */
    log_config.enable_timestamp = 0;          /* 禁用时间戳 */
    log_config.enable_module = 1;              /* 启用模块名显示 */
    log_config.enable_color = 0;               /* 禁用颜色输出 */
    
    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK)
    {
        ErrorHandler_Handle(log_status, "LOG");
    }
    
    /* ========== 步骤5：错误处理模块 ========== */
    /* 注意：ErrorHandler模块在编译时自动初始化，无需显式调用 */
    
    /* ========== 步骤6：输出初始化信息 ========== */
    LOG_INFO("MAIN", "=== 系统初始化完成 ===");
    LOG_INFO("MAIN", "UART1已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "Debug模块已初始化: UART模式");
    LOG_INFO("MAIN", "Log模块已初始化");
    LOG_INFO("MAIN", "ErrorHandler模块已就绪");
    LOG_INFO("MAIN", "=== 有源蜂鸣器GPIO模式控制示例 ===");
    
    /* ========== 步骤7：初始化其他模块（按依赖顺序） ========== */
    
    /* Buzzer初始化（根据配置表自动初始化所有enabled=1的Buzzer） */
    LOG_INFO("MAIN", "正在初始化Buzzer...");
    buzzer_status = Buzzer_Init();
    if (buzzer_status != BUZZER_OK)
    {
        LOG_ERROR("MAIN", "Buzzer初始化失败: %d", buzzer_status);
        ErrorHandler_Handle(buzzer_status, "BUZZER");
        /* Buzzer初始化失败，但可以继续运行（OLED仍可显示） */
    }
    else
    {
        LOG_INFO("MAIN", "Buzzer已初始化: GPIO模式，PA3");
    }

    /* 软件I2C初始化（OLED需要） */
    i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_1);
    if (i2c_status != SOFT_I2C_OK)
    {
        LOG_ERROR("MAIN", "软件I2C初始化失败: %d", i2c_status);
        ErrorHandler_Handle(i2c_status, "SOFT_I2C");
        /* I2C初始化失败，OLED无法使用，但Buzzer仍可工作 */
        while (1)
        {
            BUZZER1_Beep(100);
            Delay_ms(200);
        }
    }
    else
    {
        LOG_INFO("MAIN", "软件I2C已初始化: PB8(SCL), PB9(SDA)");
    }

    /* OLED初始化 */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        LOG_ERROR("MAIN", "OLED初始化失败: %d", oled_status);
        ErrorHandler_Handle(oled_status, "OLED");
        /* OLED初始化失败，但可以继续运行（Buzzer仍可工作） */
    }
    else
    {
        OLED_Clear();
        OLED_ShowString(1, 1, "System Init OK");
        OLED_ShowString(2, 1, "UART Ready");
        OLED_ShowString(3, 1, "Log Ready");
        LOG_INFO("MAIN", "OLED已初始化并显示");
    }

    /* 清屏并显示标题 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Buzzer Demo");
    OLED_ShowString(2, 1, "GPIO Mode");
    OLED_ShowString(3, 1, "PA3 Active");
    LOG_INFO("MAIN", "=== Buzzer GPIO模式演示开始 ===");
    Delay_ms(1000);

    /* ========== 步骤8：主循环 ========== */
    /* 主循环：演示Buzzer的各种功能 */
    while (1)
    {
        /* 示例1：简单开关控制 */
        LOG_INFO("MAIN", "示例1：简单开关控制");
        OLED_Clear();
        OLED_ShowString(1, 1, "Example 1:");
        OLED_ShowString(2, 1, "On/Off Ctrl");
        OLED_ShowString(3, 1, "Buzzer: ON ");
        LOG_DEBUG("BUZZER", "开启Buzzer");
        BUZZER1_On();      // 开启Buzzer
        Delay_ms(500);     // 延时500ms
        OLED_ShowString(3, 1, "Buzzer: OFF");
        LOG_DEBUG("BUZZER", "关闭Buzzer");
        BUZZER1_Off();     // 关闭Buzzer
        Delay_ms(500);
        
        /* 示例2：鸣响功能（开启+延时+关闭） */
        LOG_INFO("MAIN", "示例2：鸣响功能");
        OLED_Clear();
        OLED_ShowString(1, 1, "Example 2:");
        OLED_ShowString(2, 1, "Beep 300ms");
        OLED_ShowString(3, 1, "Beeping...");
        LOG_DEBUG("BUZZER", "鸣响300ms");
        BUZZER1_Beep(300); // 鸣响300ms
        OLED_ShowString(3, 1, "Done      ");
        Delay_ms(500);
        
        OLED_Clear();
        OLED_ShowString(1, 1, "Example 2:");
        OLED_ShowString(2, 1, "3x Short");
        OLED_ShowString(3, 1, "Beep 1...");
        LOG_DEBUG("BUZZER", "三声短鸣响");
        BUZZER1_Beep(100); // 短鸣响100ms
        Delay_ms(200);
        OLED_ShowString(3, 1, "Beep 2...");
        BUZZER1_Beep(100); // 短鸣响100ms
        Delay_ms(200);
        OLED_ShowString(3, 1, "Beep 3...");
        BUZZER1_Beep(100); // 短鸣响100ms
        OLED_ShowString(3, 1, "Done      ");
        Delay_ms(500);
        
        /* 示例3：报警音效（三短一长） */
        LOG_INFO("MAIN", "示例3：报警音效（三短一长）");
        OLED_Clear();
        OLED_ShowString(1, 1, "Example 3:");
        OLED_ShowString(2, 1, "Alarm Sound");
        OLED_ShowString(3, 1, "Beeping...");
        LOG_DEBUG("BUZZER", "播放报警音：三短一长");
        Delay_ms(500);
        
        /* 三短一长报警音 */
        BUZZER1_Beep(100);  // 短
        Delay_ms(100);
        BUZZER1_Beep(100);  // 短
        Delay_ms(100);
        BUZZER1_Beep(100);  // 短
        Delay_ms(100);
        BUZZER1_Beep(500);  // 长
        Delay_ms(1000);
        
        LOG_INFO("MAIN", "=== 一轮演示完成，开始下一轮 ===");
        
    }
}
