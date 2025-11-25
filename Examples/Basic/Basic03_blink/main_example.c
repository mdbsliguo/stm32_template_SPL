/**
 * @file main_example.c
 * @brief 案例1 - 点亮PA1灯、PA2灯
 * @example Examples/example1_led_blink/main_example.c
 * @status 已调试完成 ?
 * @details LED1（PA1）和LED2（PA2）交替闪烁示例
 *
 * 硬件要求：
 * - LED1连接到PA1
 * - LED2连接到PA2
 *
 * 硬件配置：
 * 在 BSP/board.h 中配置：
 * ```c
 * #define LED_CONFIGS { \
 *     {GPIOA, GPIO_Pin_1, Bit_RESET, 1},  // LED1：PA1，低电平点亮，启用 \
 *     {GPIOA, GPIO_Pin_2, Bit_RESET, 1},  // LED2：PA2，低电平点亮，启用 \
 * }
 * ```
 *
 * 使用方法：
 * 1. 根据实际硬件修改 BSP/board.h 中的LED配置
 * 2. 复制此文件到 Core/main.c
 * 3. 编译运行
 */

#include "system_init.h"
#include "led.h"
#include "delay.h"
#include "stm32f10x.h"

int main(void)
{
    /* 系统初始化
     * System_Init() 会自动初始化：
     * - SysTick延时模块（Delay_Init）
     * - LED驱动（根据配置表自动初始化所有enabled=1的LED）
     */
    System_Init();

    /* 主循环：LED1和LED2交替闪烁 */
    while (1)
    {
        /* LED1点亮，LED2熄灭 */
        LED1_On(); // 使用快捷宏
        LED2_Off();
        Delay_ms(500); // 延时500ms

        /* LED1熄灭，LED2点亮 */
        LED1_Off();
        LED2_On();
        Delay_ms(500); // 延时500ms

        /* 也可以使用函数接口替代快捷宏：
         * LED_On(LED_1);
         * LED_Off(LED_2);
         * Delay_ms(500);
         *
         * LED_Off(LED_1);
         * LED_On(LED_2);
         * Delay_ms(500);
         */
    }
}
