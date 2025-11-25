/**
 * @file main_example.c
 * @brief 案例2 - Delay延时功能测试（带OLED显示）
 * @example Examples/example2_delay_test/main_example.c
 * @status 已调试完成 ?
 * @details 测试Delay模块的各种延时功能，包括毫秒、微秒、秒级延时
 *          使用OLED显示当前测试状态，让延时效果更直观
 *
 * 硬件要求：
 * - LED1连接到PA1（用于可视化延时效果）
 * - LED2连接到PA2（可选，用于对比）
 * - OLED显示屏（SSD1306，I2C接口）
 *   - SCL连接到PB8
 *   - SDA连接到PB9
 *
 * 硬件配置：
 * 在 BSP/board.h 中配置：
 * - LED_CONFIGS: LED配置（PA1和PA2）
 * - OLED_I2C_CONFIG: OLED I2C配置（PB8和PB9）
 *
 * 使用方法：
 * 1. 根据实际硬件修改 BSP/board.h 中的LED和OLED配置
 * 2. 复制此文件到 Core/main.c
 * 3. 编译运行
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "led.h"
#include "delay.h"
#include "oled_ssd1306.h"

int main(void)
{
    OLED_Status_t oled_status;

    /* 系统初始化
     * System_Init() 会自动初始化：
     * - SysTick延时模块（Delay_Init）
     * - LED驱动（根据配置表自动初始化所有enabled=1的LED）
     */
    System_Init();

    /* OLED初始化 */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        /* 初始化失败，LED闪烁提示 */
        while (1)
        {
            LED1_On();
            Delay_ms(100);
            LED1_Off();
            Delay_ms(100);
        }
    }

    /* 清屏并显示标题 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Delay Test");
    Delay_ms(500);

    /* ========== 测试1：毫秒级延时（Delay_ms） ========== */

    OLED_ShowString(1, 1, "Test 1: ms");

    /* 快速闪烁：100ms间隔 - LED1和LED2交替 */
    OLED_ShowString(2, 1, "Delay: 100ms");
    OLED_ShowString(3, 1, "LED1:ON LED2:OFF");
    LED1_On();
    LED2_Off();
    Delay_ms(100);
    OLED_ShowString(3, 1, "LED1:OFF LED2:ON");
    LED1_Off();
    LED2_On();
    Delay_ms(100);

    /* 中速闪烁：500ms间隔 - LED1和LED2交替 */
    OLED_ShowString(2, 1, "Delay: 500ms");
    OLED_ShowString(3, 1, "LED1:ON LED2:OFF");
    LED1_On();
    LED2_Off();
    Delay_ms(500);
    OLED_ShowString(3, 1, "LED1:OFF LED2:ON");
    LED1_Off();
    LED2_On();
    Delay_ms(500);

    /* 慢速闪烁：1000ms间隔 - LED1和LED2交替 */
    OLED_ShowString(2, 1, "Delay:1000ms");
    OLED_ShowString(3, 1, "LED1:ON LED2:OFF");
    LED1_On();
    LED2_Off();
    Delay_ms(1000);
    OLED_ShowString(3, 1, "LED1:OFF LED2:ON");
    LED1_Off();
    LED2_On();
    Delay_ms(1000);

    /* ========== 测试2：微秒级延时（Delay_us） ========== */

    OLED_ShowString(1, 1, "Test 2: us");

    /* 快速闪烁：10ms间隔（使用微秒延时）- LED1和LED2交替 */
    OLED_ShowString(2, 1, "Delay:10000us");
    OLED_ShowString(3, 1, "LED1:ON LED2:OFF");
    LED1_On();
    LED2_Off();
    Delay_us(10000); // 10ms = 10000us
    OLED_ShowString(3, 1, "LED1:OFF LED2:ON");
    LED1_Off();
    LED2_On();
    Delay_us(10000);

    /* 中速闪烁：50ms间隔 - LED1和LED2交替 */
    OLED_ShowString(2, 1, "Delay:50000us");
    OLED_ShowString(3, 1, "LED1:ON LED2:OFF");
    LED1_On();
    LED2_Off();
    Delay_us(50000); // 50ms = 50000us
    OLED_ShowString(3, 1, "LED1:OFF LED2:ON");
    LED1_Off();
    LED2_On();
    Delay_us(50000);

    /* ========== 测试3：秒级延时（Delay_s） ========== */

    OLED_ShowString(1, 1, "Test 3: s ");

    /* 慢速闪烁：1秒间隔 - LED1和LED2交替 */
    OLED_ShowString(2, 1, "Delay: 1s  ");
    OLED_ShowString(3, 1, "LED1:ON LED2:OFF");
    LED1_On();
    LED2_Off();
    Delay_s(1); // 延时1秒
    OLED_ShowString(3, 1, "LED1:OFF LED2:ON");
    LED1_Off();
    LED2_On();
    Delay_s(1);

    /* ========== 测试4：不同延时时间组合（主循环） ========== */

    OLED_ShowString(1, 1, "Test 4: Loop");
    Delay_ms(500);

    /* 主循环：演示不同延时时间 */
    while (1)
    {
        /* 快速闪烁3次（100ms）- LED1和LED2交替 */
        {
            int i;
            OLED_ShowString(2, 1, "Fast: 100ms");
            for (i = 0; i < 3; i++)
            {
                OLED_ShowString(4, 1, "Count: ");
                OLED_ShowNum(4, 8, i + 1, 1);
                OLED_ShowString(3, 1, "LED1:ON LED2:OFF");
                LED1_On();
                LED2_Off();
                Delay_ms(100);
                OLED_ShowString(3, 1, "LED1:OFF LED2:ON");
                LED1_Off();
                LED2_On();
                Delay_ms(100);
            }
        }

        /* 延时500ms */
        OLED_ShowString(2, 1, "Wait: 500ms");
        OLED_ShowString(3, 1, "LED1:OFF LED2:OFF");
        OLED_ShowString(4, 1, "              ");
        LED1_Off();
        LED2_Off();
        Delay_ms(500);

        /* 中速闪烁3次（500ms）- LED1和LED2交替 */
        {
            int i;
            OLED_ShowString(2, 1, "Mid:  500ms");
            for (i = 0; i < 3; i++)
            {
                OLED_ShowString(4, 1, "Count: ");
                OLED_ShowNum(4, 8, i + 1, 1);
                OLED_ShowString(3, 1, "LED1:ON LED2:OFF");
                LED1_On();
                LED2_Off();
                Delay_ms(500);
                OLED_ShowString(3, 1, "LED1:OFF LED2:ON");
                LED1_Off();
                LED2_On();
                Delay_ms(500);
            }
        }

        /* 延时1秒 */
        OLED_ShowString(2, 1, "Wait: 1s   ");
        OLED_ShowString(3, 1, "LED1:OFF LED2:OFF");
        OLED_ShowString(4, 1, "              ");
        LED1_Off();
        LED2_Off();
        Delay_s(1);

        /* LED1和LED2交替闪烁（200ms） */
        {
            int i;
            OLED_ShowString(2, 1, "Alt:  200ms");
            for (i = 0; i < 5; i++)
            {
                OLED_ShowString(4, 1, "Count: ");
                OLED_ShowNum(4, 8, i + 1, 1);
                OLED_ShowString(3, 1, "LED1:ON LED2:OFF");
                LED1_On();
                LED2_Off();
                Delay_ms(200);
                OLED_ShowString(3, 1, "LED1:OFF LED2:ON");
                LED1_Off();
                LED2_On();
                Delay_ms(200);
            }
        }
    }
}
