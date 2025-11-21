/**
 * @file main_example.c
 * @brief 案例 - GPIO模式Buzzer基础控制示例
 * @example Examples/Basic/Basic01_gpio_Buzzer/main_example.c
 * @status 已调试完成 ✅
 * @details Buzzer基础控制示例，演示GPIO模式的使用，使用OLED显示提示信息
 *
 * 硬件要求：
 * - Buzzer（有源蜂鸣器）连接到PA3
 * - OLED显示屏（SSD1306，I2C接口，用于显示提示信息）
 *   - SCL连接到PB8
 *   - SDA连接到PB9
 *
 * 硬件配置：
 * 在 Examples/Basic/Basic01_gpio_Buzzer/board.h 中配置：
 * ```c
 * #define BUZZER_CONFIGS { \
 *     {BUZZER_MODE_GPIO, GPIOA, GPIO_Pin_3, 1, 0, Bit_RESET, 1},  // GPIO模式，PA3，低电平有效，启用 \
 * }
 * ```
 *
 * 使用方法：
 * 1. 打开案例工程：双击 Examples/Basic/Basic01_gpio_Buzzer/Examples.uvprojx
 * 2. 根据实际硬件修改 Examples/Basic/Basic01_gpio_Buzzer/board.h 中的Buzzer配置
 * 3. 编译运行（F7编译，F8下载）
 * 
 * 注意：本案例是独立工程，无需复制文件
 */

#include "system_init.h"
#include "buzzer.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include "i2c_sw.h"
#include "stm32f10x.h"

int main(void)
{
    SoftI2C_Status_t i2c_status;
    OLED_Status_t oled_status;
    
    /* 系统初始化
     * System_Init() 会自动初始化：
     * - SysTick延时模块（Delay_Init）
     */
    System_Init();

    /* Buzzer初始化（根据配置表自动初始化所有enabled=1的Buzzer） */
    Buzzer_Init();

    /* 软件I2C初始化（OLED需要） */
    i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_1);
    if (i2c_status != SOFT_I2C_OK)
    {
        /* I2C初始化失败，Buzzer快速鸣响提示 */
        while (1)
        {
            BUZZER1_Beep(100);
            Delay_ms(200);
        }
    }

    /* OLED初始化 */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        /* OLED初始化失败，Buzzer快速鸣响提示 */
        while (1)
        {
            BUZZER1_Beep(100);
            Delay_ms(200);
        }
    }

    /* 清屏并显示标题 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Buzzer Demo");
    OLED_ShowString(2, 1, "GPIO Mode");
    OLED_ShowString(3, 1, "PA3 Active");
    Delay_ms(1000);

    /* 主循环：演示Buzzer的各种功能 */
    while (1)
    {
        /* 示例1：简单开关控制 */
        OLED_Clear();
        OLED_ShowString(1, 1, "Example 1:");
        OLED_ShowString(2, 1, "On/Off Ctrl");
        OLED_ShowString(3, 1, "Buzzer: ON ");
        BUZZER1_On();      // 开启Buzzer
        Delay_ms(500);     // 延时500ms
        OLED_ShowString(3, 1, "Buzzer: OFF");
        BUZZER1_Off();     // 关闭Buzzer
        Delay_ms(500);
        
        /* 示例2：鸣响功能（开启+延时+关闭） */
        OLED_Clear();
        OLED_ShowString(1, 1, "Example 2:");
        OLED_ShowString(2, 1, "Beep 300ms");
        OLED_ShowString(3, 1, "Beeping...");
        BUZZER1_Beep(300); // 鸣响300ms
        OLED_ShowString(3, 1, "Done      ");
        Delay_ms(500);
        
        OLED_Clear();
        OLED_ShowString(1, 1, "Example 2:");
        OLED_ShowString(2, 1, "3x Short");
        OLED_ShowString(3, 1, "Beep 1...");
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
        OLED_Clear();
        OLED_ShowString(1, 1, "Example 3:");
        OLED_ShowString(2, 1, "Alarm Sound");
        OLED_ShowString(3, 1, "Beeping...");
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
        
    }
}

