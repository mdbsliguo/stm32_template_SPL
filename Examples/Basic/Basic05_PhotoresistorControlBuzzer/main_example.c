/**
 * @file main_example.c
 * @brief 案例 - 光敏电阻控制蜂鸣器示例
 * @example Examples/Basic/Basic05_PhotoresistorControlBuzzer/main_example.c
 * @status 已调试完成 ?
 * @details 光敏电阻控制蜂鸣器示例，演示GPIO读取光敏电阻DO引脚电平并根据光照状态实时控制蜂鸣器
 *
 * 硬件要求：
 * - 光敏电阻模块（四引脚）DO（数字输出）连接到PA5
 *   - VCC和GND已连接好，只需连接DO引脚到PA5
 *   - 光照暗时DO输出高电平，光照亮时DO输出低电平
 * - Buzzer（三引脚有源蜂鸣器模块）控制引脚连接到PA3
 *   - VCC和GND已连接好，只需连接控制引脚（中间引脚，SIG/IN）到PA3
 * - OLED显示屏（SSD1306，I2C接口，用于显示光照状态和蜂鸣器状态）
 *   - SCL连接到PB8
 *   - SDA连接到PB9
 *
 * 工作原理：
 * - 使用GPIO读取光敏电阻DO（数字输出）引脚的电平
 * - 光照暗时DO输出高电平，光照亮时DO输出低电平
 * - 根据DO引脚电平控制蜂鸣器：光照暗（高电平）时蜂鸣器响，光照亮（低电平）时蜂鸣器不响
 *
 * 硬件配置：
 * 在 Examples/Basic/Basic05_PhotoresistorControlBuzzer/board.h 中配置：
 * ```c
 * #define PHOTO_DO_PORT  GPIOA
 * #define PHOTO_DO_PIN   GPIO_Pin_5
 * 
 * #define BUZZER_CONFIGS { \
 *     {BUZZER_MODE_GPIO, GPIOA, GPIO_Pin_3, 0, 0, Bit_RESET, 1},  // GPIO模式，PA3（控制引脚），低电平有效，启用 \
 * }
 * 
 * 注意：
 * - 四引脚光敏电阻模块，VCC和GND已连接，只需连接DO（数字输出）引脚到PA5
 * - 三引脚有源蜂鸣器模块，VCC和GND已连接，只需连接控制引脚（中间引脚，SIG/IN）到PA3
 * ```
 *
 * 使用方法：
 * 1. 打开案例工程：双击 Examples/Basic/Basic05_PhotoresistorControlBuzzer/Examples.uvprojx
 * 2. 根据实际硬件修改 Examples/Basic/Basic05_PhotoresistorControlBuzzer/board.h 中的配置
 * 3. 编译运行（F7编译，F8下载）
 * 
 * 注意：本案例是独立工程，无需复制文件
 * 
 * 硬件连接说明：
 * - 光敏电阻模块（四引脚）：VCC和GND已连接，DO（数字输出）连接到PA5
 * - 蜂鸣器模块（三引脚）：VCC和GND已连接，控制引脚（中间引脚）连接到PA3
 */

#include "system_init.h"
#include "buzzer.h"
#include "gpio.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include "i2c_sw.h"
#include "stm32f10x.h"
#include "board.h"

int main(void)
{
    SoftI2C_Status_t i2c_status;
    OLED_Status_t oled_status;
    uint8_t photo_state = 0;  /* 光敏电阻DO引脚状态：0=光照亮（低电平），1=光照暗（高电平） */
    uint8_t last_photo_state = 0;
    uint8_t oled_initialized = 0;  /* OLED显示初始化标志 */
    
    /* 系统初始化
     * System_Init() 会自动初始化：
     * - SysTick延时模块（Delay_Init）
     */
    System_Init();

    /* 配置光敏电阻DO引脚为输入上拉模式
     * 上拉输入：光照亮时DO输出低电平，光照暗时DO输出高电平
     */
    GPIO_Config(PHOTO_DO_PORT, PHOTO_DO_PIN, GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_2MHz);

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
    OLED_ShowString(1, 1, "Photo Resistor");
    OLED_ShowString(2, 1, "Control Buzzer");
    OLED_ShowString(3, 1, "Reading...");
    Delay_ms(1000);

    /* 主循环：读取光敏电阻DO引脚状态并控制蜂鸣器（非阻塞式） */
    while (1)
    {
        /* 读取光敏电阻DO引脚电平
         * GPIO_ReadPin() 返回 Bit_SET（高电平）或 Bit_RESET（低电平）
         * 上拉输入模式下：
         *   - 光照亮：DO输出低电平，返回 Bit_RESET
         *   - 光照暗：DO输出高电平，返回 Bit_SET
         */
        photo_state = GPIO_ReadPin(PHOTO_DO_PORT, PHOTO_DO_PIN);
        
        /* 实时控制蜂鸣器（不阻塞，立即响应） */
        if (photo_state == Bit_SET)
        {
            /* 光照暗（高电平），蜂鸣器响 */
            BUZZER1_On();
        }
        else
        {
            /* 光照亮（低电平），蜂鸣器不响 */
            BUZZER1_Off();
        }
        
        /* 只在状态变化时更新OLED（避免刷频，显示更流畅） */
        if (photo_state != last_photo_state || !oled_initialized)
        {
            /* 状态变化或首次显示，更新OLED */
            if (!oled_initialized)
            {
                /* 首次显示，清屏并显示标题 */
                OLED_Clear();
                OLED_ShowString(1, 1, "Photo Sensor:");
                oled_initialized = 1;
            }
            
            /* 只更新变化的部分，不清屏（减少闪烁） */
            if (photo_state == Bit_SET)
            {
                /* 光照暗（高电平） */
                OLED_ShowString(2, 1, "Status: Dark  ");
                OLED_ShowString(3, 1, "Buzzer: ON   ");
            }
            else
            {
                /* 光照亮（低电平） */
                OLED_ShowString(2, 1, "Status: Bright");
                OLED_ShowString(3, 1, "Buzzer: OFF  ");
            }
            
            last_photo_state = photo_state;
        }
        
        /* 最小延时，避免CPU占用率过高 */
        Delay_ms(1);
    }
}

