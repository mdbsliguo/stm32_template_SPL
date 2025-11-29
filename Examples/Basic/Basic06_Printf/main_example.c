/**
 * @file main_example.c
 * @brief 案例 - printf封装模块使用示例
 * @example Examples/Basic/Basic06_Printf/main_example.c
 * @details printf封装模块使用示例，演示Printf_UART和Printf_OLED函数的使用
 *
 * 硬件要求：
 * - OLED显示屏（SSD1306，I2C接口，用于显示输出）
 *   - SCL连接到PB8
 *   - SDA连接到PB9
 * - USB转串口模块（用于UART输出）
 *   - TX连接到PA9
 *   - RX连接到PA10
 *
 * 硬件配置：
 * 在 Examples/Basic/Basic06_Printf/board.h 中配置
 *
 * 使用方法：
 * 1. 打开案例工程：双击 Examples/Basic/Basic06_Printf/Examples.uvprojx
 * 2. 根据实际硬件修改 Examples/Basic/Basic06_Printf/board.h 中的配置
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
#include "oled_ssd1306.h"
#include "i2c_sw.h"
#include "printf_wrapper.h"
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
    
    uint32_t counter = 0;
    float temperature = 25.6f;
    int32_t humidity = 65;
    uint16_t voltage = 3300;
    
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
    LOG_INFO("MAIN", "=== printf封装模块使用示例 ===");
    
    /* ========== 步骤7：初始化其他模块（按依赖顺序） ========== */
    
    /* 软件I2C初始化（OLED需要） */
    i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_1);
    if (i2c_status != SOFT_I2C_OK)
    {
        LOG_ERROR("MAIN", "软件I2C初始化失败: %d", i2c_status);
        ErrorHandler_Handle(i2c_status, "SOFT_I2C");
        /* I2C初始化失败，OLED无法使用，但UART仍可工作 */
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
        /* OLED初始化失败，但可以继续运行（UART仍可工作） */
    }
    else
    {
        OLED_Clear();
        OLED_ShowString(1, 1, "System Init OK");
        OLED_ShowString(2, 1, "UART Ready");
        OLED_ShowString(3, 1, "Printf Demo");
        LOG_INFO("MAIN", "OLED已初始化并显示");
    }
    
    Delay_ms(2000);
    
    /* ========== 步骤8：演示printf封装功能 ========== */
    LOG_INFO("MAIN", "=== 开始演示printf封装功能 ===");
    
    /* 示例1：UART输出演示 */
    Printf_UART1("\r\n=== Printf_UART1 演示 ===\r\n");
    Printf_UART1("计数器: %d\r\n", counter);
    Printf_UART1("温度: %.1f°C\r\n", temperature);
    Printf_UART1("湿度: %d%%\r\n", humidity);
    Printf_UART1("电压: %dmV\r\n", voltage);
    Printf_UART1("十六进制: 0x%04X\r\n", voltage);
    Printf_UART1("字符串: %s\r\n", "Hello World");
    
    /* 示例2：OLED输出演示（自动截断到16字符） */
    OLED_Clear();
    Printf_OLED1("Printf Demo");
    Printf_OLED2("Counter:%d", counter);
    Printf_OLED3("Temp:%.1fC", temperature);
    Printf_OLED4("Humidity:%d%%", humidity);
    
    Delay_ms(2000);
    
    /* 示例3：动态更新演示 */
    LOG_INFO("MAIN", "=== 开始动态更新演示 ===");
    
    while (1)
    {
        counter++;
        temperature += 0.1f;
        if (temperature > 30.0f) temperature = 20.0f;
        humidity = (humidity + 1) % 100;
        voltage = 3000 + (counter % 500);
        
        /* UART输出（详细日志） */
        Printf_UART1("[%d] Temp:%.1fC, Hum:%d%%, Vol:%dmV\r\n", 
                     counter, temperature, humidity, voltage);
        
        /* OLED输出（关键信息，自动截断） */
        Printf_OLED1("Counter:%d", counter);
        Printf_OLED2("Temp:%.1fC", temperature);
        Printf_OLED3("Humidity:%d%%", humidity);
        Printf_OLED4("Voltage:%dmV", voltage);
        
        Delay_ms(1000);
        
        /* 每10次循环输出一次分隔线 */
        if (counter % 10 == 0)
        {
            Printf_UART1("--- 循环 %d 次 ---\r\n", counter);
        }
    }
}

















