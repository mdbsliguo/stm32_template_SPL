/**
 * @file main_example.c
 * @brief 案例 - 按钮控制LED示例
 * @example Examples/Basic/Basic04_ButtonControlLED/main_example.c
 * @status 已调试完成 ?
 * @details 按钮控制LED示例，演示GPIO输入读取和LED控制
 *
 * 硬件要求：
 * - LED1连接到PA1
 * - 按钮连接到PA4（按下为低电平，使用上拉输入）
 *
 * 硬件配置：
 * 在 Examples/Basic/Basic04_ButtonControlLED/board.h 中配置：
 * ```c
 * #define LED_CONFIGS { \
 *     {GPIOA, GPIO_Pin_1, Bit_RESET, 1},  // LED1：PA1，低电平点亮，启用 \
 * }
 * 
 * #define BUTTON_PORT  GPIOA
 * #define BUTTON_PIN   GPIO_Pin_4
 * ```
 *
 * 使用方法：
 * 1. 打开案例工程：双击 Examples/Basic/Basic04_ButtonControlLED/Examples.uvprojx
 * 2. 根据实际硬件修改 Examples/Basic/Basic04_ButtonControlLED/board.h 中的配置
 * 3. 编译运行（F7编译，F8下载）
 * 
 * 注意：本案例是独立工程，无需复制文件
 */

#include "system_init.h"
#include "led.h"
#include "gpio.h"
#include "delay.h"
#include "stm32f10x.h"
#include "board.h"

int main(void)
{
    uint8_t button_state;
    uint8_t last_button_state = Bit_SET;  // 初始状态为未按下（上拉输入，高电平）
    
    /* 系统初始化
     * System_Init() 会自动初始化：
     * - SysTick延时模块（Delay_Init）
     * - LED驱动（根据配置表自动初始化所有enabled=1的LED）
     */
    System_Init();

    /* 配置按钮引脚为输入上拉模式
     * 上拉输入：按钮未按下时引脚为高电平，按下时引脚为低电平
     */
    GPIO_Config(BUTTON_PORT, BUTTON_PIN, GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_2MHz);

    /* 主循环：读取按钮状态并控制LED */
    while (1)
    {
        /* 读取按钮引脚电平
         * GPIO_ReadPin() 返回 Bit_SET（高电平）或 Bit_RESET（低电平）
         * 上拉输入模式下：
         *   - 按钮未按下：返回 Bit_SET（高电平）
         *   - 按钮按下：返回 Bit_RESET（低电平）
         */
        button_state = GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN);

        /* 检测按钮按下（从高电平变为低电平） */
        if (button_state == Bit_RESET && last_button_state == Bit_SET)
        {
            /* 按钮按下：点亮LED */
            LED1_On();
        }
        /* 检测按钮释放（从低电平变为高电平） */
        else if (button_state == Bit_SET && last_button_state == Bit_RESET)
        {
            /* 按钮释放：熄灭LED */
            LED1_Off();
        }

        /* 保存当前按钮状态，用于下次比较 */
        last_button_state = button_state;

        /* 延时10ms，用于消抖和降低CPU占用率 */
        Delay_ms(10);
    }
}

