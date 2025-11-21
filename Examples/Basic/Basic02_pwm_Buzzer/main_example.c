/**
 * @file main_example.c
 * @brief 案例 - PWM模式Buzzer控制示例
 * @example Examples/Basic/Basic02_pwm_Buzzer/main_example.c
 * @status 已调试完成 ✅
 * @details Buzzer PWM模式控制示例，演示频率控制、音调播放等功能，使用OLED显示提示信息
 *
 * 硬件要求：
 * - Buzzer（无源蜂鸣器）连接到PA6（TIM3 CH1）
 * - OLED显示屏（SSD1306，I2C接口，用于显示提示信息）
 *   - SCL连接到PB8
 *   - SDA连接到PB9
 *
 * 硬件配置：
 * 在 Examples/Basic/Basic02_pwm_Buzzer/board.h 中配置：
 * ```c
 * #define BUZZER_CONFIGS { \
 *     {BUZZER_MODE_PWM, NULL, 0, 1, 0, Bit_RESET, 1},  // PWM模式，TIM3(实例1)通道1，启用 \
 * }
 * ```
 *
 * 使用方法：
 * 1. 打开案例工程：双击 Examples/Basic/Basic02_pwm_Buzzer/Examples.uvprojx
 * 2. 根据实际硬件修改 Examples/Basic/Basic02_pwm_Buzzer/board.h 中的Buzzer配置
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
    OLED_ShowString(2, 1, "PWM Mode");
    OLED_ShowString(3, 1, "TIM3 CH1");
    Delay_ms(1000);

    /* 主循环：演示Buzzer的各种PWM功能 */
    while (1)
    {
        /* 示例1：频率控制 - 不同频率的鸣响 */
        OLED_Clear();
        OLED_ShowString(1, 1, "Example 1:");
        OLED_ShowString(2, 1, "Frequency");
        OLED_ShowString(3, 1, "500Hz...");
        Buzzer_SetFrequency(BUZZER_1, 500);  // 设置500Hz
        BUZZER1_On();
        Delay_ms(500);
        BUZZER1_Off();
        Delay_ms(200);
        
        OLED_ShowString(3, 1, "1000Hz...");
        Buzzer_SetFrequency(BUZZER_1, 1000);  // 设置1kHz
        BUZZER1_On();
        Delay_ms(500);
        BUZZER1_Off();
        Delay_ms(200);
        
        OLED_ShowString(3, 1, "2000Hz...");
        Buzzer_SetFrequency(BUZZER_1, 2000);  // 设置2kHz
        BUZZER1_On();
        Delay_ms(500);
        BUZZER1_Off();
        Delay_ms(500);
        
        /* 示例2：音调播放 - 播放C4-C5音阶 */
        OLED_Clear();
        OLED_ShowString(1, 1, "Example 2:");
        OLED_ShowString(2, 1, "Play Scale");
        OLED_ShowString(3, 1, "C4...");
        Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_C4, 300);
        Delay_ms(100);
        
        OLED_ShowString(3, 1, "D4...");
        Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_D4, 300);
        Delay_ms(100);
        
        OLED_ShowString(3, 1, "E4...");
        Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_E4, 300);
        Delay_ms(100);
        
        OLED_ShowString(3, 1, "F4...");
        Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_F4, 300);
        Delay_ms(100);
        
        OLED_ShowString(3, 1, "G4...");
        Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_G4, 300);
        Delay_ms(100);
        
        OLED_ShowString(3, 1, "A4...");
        Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_A4, 300);
        Delay_ms(100);
        
        OLED_ShowString(3, 1, "B4...");
        Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_B4, 300);
        Delay_ms(100);
        
        OLED_ShowString(3, 1, "C5...");
        Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_C5, 300);
        Delay_ms(500);
        
        /* 示例3：播放简单旋律（小星星前奏） */
        OLED_Clear();
        OLED_ShowString(1, 1, "Example 3:");
        OLED_ShowString(2, 1, "Melody");
        OLED_ShowString(3, 1, "Playing...");
        Delay_ms(500);
        
        /* 小星星：C4-C4-G4-G4-A4-A4-G4 */
        Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_C4, 300);
        Delay_ms(50);
        Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_C4, 300);
        Delay_ms(50);
        Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_G4, 300);
        Delay_ms(50);
        Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_G4, 300);
        Delay_ms(50);
        Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_A4, 300);
        Delay_ms(50);
        Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_A4, 300);
        Delay_ms(50);
        Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_G4, 600);
        Delay_ms(1000);
        
        /* 示例4：频率扫描效果 */
        OLED_Clear();
        OLED_ShowString(1, 1, "Example 4:");
        OLED_ShowString(2, 1, "Freq Sweep");
        OLED_ShowString(3, 1, "Sweeping...");
        Delay_ms(500);
        
        /* 从200Hz扫描到2000Hz */
        for (uint32_t freq = 200; freq <= 2000; freq += 50)
        {
            Buzzer_SetFrequency(BUZZER_1, freq);
            BUZZER1_On();
            Delay_ms(20);
            BUZZER1_Off();
            Delay_ms(5);
        }
        Delay_ms(500);
        
        /* 示例5：持续播放音调（需要手动停止） */
        OLED_Clear();
        OLED_ShowString(1, 1, "Example 5:");
        OLED_ShowString(2, 1, "Continuous");
        OLED_ShowString(3, 1, "A4 Tone...");
        Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_A4, 0);  // 0表示持续播放
        Delay_ms(1000);
        BUZZER1_Stop();  // 手动停止
        Delay_ms(500);
    }
}


