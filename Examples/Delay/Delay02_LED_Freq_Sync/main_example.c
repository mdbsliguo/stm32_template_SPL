/**
 * @file main_example.c
 * @brief 案例4 - delay模块手动调频测试
 * @status 已调试完成 ?
 * @details 测试delay模块在不同频率下的延时准确性，验证频率变化时1秒永远是1秒
 *
 * 测试内容：
 * 1. 在不同频率下测试阻塞式延时（Delay_ms，基于SysTick，频率切换时自动适配）
 * 2. 在不同频率下测试非阻塞式延时（Delay_ms_nonblock，基于TIM2_TimeBase，频率切换时自动适配）
 * 3. 验证频率切换时，延时时间是否保持准确（1秒永远是1秒）
 *
 * 模块依赖：
 * - TIM2_TimeBase：TIM2外设定时器，提供1ms时间基准，频率切换时自动适配
 * - delay：阻塞式延时（SysTick）+ 非阻塞式延时（TIM2_TimeBase），频率切换时自动适配
 * - clock_manager：时钟管理模块，支持手动和自动调频
 *
 * 硬件要求：
 * - LED1连接到PA1（用于可视化延时效果）
 * - LED2连接到PA2（用于显示当前频率状态）
 * - OLED显示屏（SSD1306，I2C接口）
 *   - SCL连接到PB8
 *   - SDA连接到PB9
 *
 * 硬件配置：
 * 在 BSP/board.h 中配置：
 * - LED_CONFIGS: LED配置（PA1和PA2）
 * - OLED_I2C_CONFIG: OLED I2C配置（PB8和PB9）
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "led.h"
#include "delay.h"
#include "clock_manager.h"
#include "TIM2_TimeBase.h"
#include "oled_ssd1306.h"

int main(void)
{
    OLED_Status_t oled_status;

    /* 系统初始化 */
    System_Init();

    /* OLED初始化 */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        /* 初始化失败，根据错误码显示不同的LED模式 */
        if (oled_status == OLED_ERROR_INVALID_PARAM)
        {
            /* 参数错误：LED1和LED2交替闪烁 */
            while (1)
            {
                LED1_On();
                LED2_Off();
                Delay_ms(200);
                LED1_Off();
                LED2_On();
                Delay_ms(200);
            }
        }
        else if (oled_status == OLED_ERROR_GPIO_FAILED)
        {
            /* GPIO错误：LED1快速闪烁 */
            while (1)
            {
                LED1_On();
                Delay_ms(50);
                LED1_Off();
                Delay_ms(50);
            }
        }
        else
        {
            /* 其他错误：LED1和LED2同时闪烁 */
            while (1)
            {
                LED1_On();
                LED2_On();
                Delay_ms(200);
                LED1_Off();
                LED2_Off();
                Delay_ms(200);
            }
        }
    }

    /* 初始化时钟管理模块 */
    CLKM_Init();

    /* 设置为手动模式，初始频率72MHz */
    CLKM_SetMode(CLKM_MODE_MANUAL, CLKM_LVL_72MHZ);

    /* 等待时钟稳定 */
    Delay_ms(100);

    /* OLED显示初始化信息 */
    /* 先清屏，等待一下确保OLED稳定 */
    OLED_Clear();
    Delay_ms(100);

    /* 显示初始化信息 */
    OLED_ShowString(1, 1, "Delay Freq Test");
    Delay_ms(50);
    OLED_ShowString(2, 1, "Init OK");
    Delay_ms(50);

    /* 等待1秒后清屏进入测试 */
    Delay_ms(1000);
    OLED_Clear();
    Delay_ms(100);

    /* ========== 测试1：在不同频率下测试阻塞式延时 ========== */

    /* 测试频率列表（9档，从高到低） */
    CLKM_FreqLevel_t test_freqs[] = {
        CLKM_LVL_72MHZ, /* 系数1：72MHz */
        CLKM_LVL_64MHZ, /* 系数2：64MHz */
        CLKM_LVL_56MHZ, /* 系数3：56MHz */
        CLKM_LVL_48MHZ, /* 系数4：48MHz */
        CLKM_LVL_40MHZ, /* 系数5：40MHz */
        CLKM_LVL_32MHZ, /* 系数6：32MHz */
        CLKM_LVL_24MHZ, /* 系数7：24MHz */
        CLKM_LVL_16MHZ, /* 系数8：16MHz */
        CLKM_LVL_8MHZ,  /* 系数9：8MHz */
    };
    const uint8_t test_freq_count = sizeof(test_freqs) / sizeof(test_freqs[0]);

    /* OLED显示测试1信息 */
    OLED_ShowString(1, 1, "Test1:Block Delay");
    OLED_ShowString(2, 1, "Freq:");
    OLED_ShowString(3, 1, "Coeff:");
    OLED_ShowString(4, 1, "Wait 10s");

    uint8_t i;
    for (i = 0; i < test_freq_count; i++)
    {
        /* 切换到测试频率 */
        CLKM_SetFixedLevel(test_freqs[i]);

        /* 等待频率切换完成 */
        Delay_ms(200);

        /* 获取当前频率和系数并显示 */
        uint32_t current_freq = CLKM_GetCurrentFrequency();
        uint8_t coefficient = CLKM_GetCurrentLevel() + 1; /* level 0-8 对应系数 1-9 */

        OLED_ShowString(2, 6, "     ");                /* 清空频率显示区域 */
        OLED_ShowNum(2, 6, current_freq / 1000000, 2); /* 显示MHz */
        OLED_ShowString(2, 8, "MHz");

        OLED_ShowString(3, 7, "  ");        /* 清空系数显示区域 */
        OLED_ShowNum(3, 7, coefficient, 1); /* 显示系数 */

        /* 每个频率停留10秒，倒计时显示 */
        /* LED2根据系数闪烁（72MHz闪9次/秒，64MHz闪8次/秒，...，8MHz闪1次/秒） */
        uint8_t led2_blinks_per_sec = 10 - coefficient; /* 系数1闪9次/秒，系数9闪1次/秒 */
        uint16_t led2_interval_ms = 1000 / led2_blinks_per_sec;
        uint32_t led2_timer = Delay_GetTick();
        uint8_t led2_state = 0;
        uint32_t led1_timer = Delay_GetTick();
        uint8_t led1_state = 0;

        uint8_t countdown;
        for (countdown = 10; countdown > 0; countdown--)
        {
            OLED_ShowString(4, 5, "   ");     /* 清空倒计时显示区域 */
            OLED_ShowNum(4, 5, countdown, 2); /* 显示倒计时 */
            OLED_ShowString(4, 7, "s");

            /* 在1秒内：LED1闪烁1次，LED2闪烁对应次数 */
            uint32_t loop_start = Delay_GetTick();
            while ((Delay_GetTick() - loop_start) < 1000)
            {
                uint32_t current_tick = Delay_GetTick();

                /* LED1：每1秒闪烁一次（亮500ms，灭500ms） */
                if (Delay_ms_nonblock(led1_timer, 500))
                {
                    if (led1_state == 0)
                    {
                        LED1_On();
                        led1_state = 1;
                    }
                    else
                    {
                        LED1_Off();
                        led1_state = 0;
                    }
                    led1_timer = current_tick;
                }

                /* LED2：根据系数闪烁 */
                if (Delay_ms_nonblock(led2_timer, led2_interval_ms))
                {
                    if (led2_state == 0)
                    {
                        LED2_On();
                        led2_state = 1;
                    }
                    else
                    {
                        LED2_Off();
                        led2_state = 0;
                    }
                    led2_timer = current_tick;
                }
            }
        }
    }

    /* ========== 测试2：在不同频率下测试非阻塞式延时 ========== */

    /* OLED显示测试2信息 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Test2:NonBlock");
    OLED_ShowString(2, 1, "Freq:");
    OLED_ShowString(3, 1, "Coeff:");
    OLED_ShowString(4, 1, "Wait 10s");

    /* 切换到72MHz */
    CLKM_SetFixedLevel(CLKM_LVL_72MHZ);
    Delay_ms(200);

    /* LED1：非阻塞式延时，1000ms（1秒）闪烁一次 */
    /* LED2：根据系数闪烁（72MHz闪9次/秒，64MHz闪8次/秒，...，8MHz闪1次/秒） */
    static uint32_t led1_start = 0;
    static uint32_t led2_start = 0;
    static uint8_t led1_state = 0;
    static uint8_t led2_state = 0;
    static uint32_t freq_switch_timer = 0;
    static uint32_t oled_update_timer = 0;
    static uint8_t current_freq_index = 0;

    /* 初始化开始时间 */
    led1_start = Delay_GetTick();
    led2_start = Delay_GetTick();
    freq_switch_timer = Delay_GetTick();
    oled_update_timer = Delay_GetTick();

    /* 主循环：测试非阻塞式延时 + 周期性切换频率 */
    while (1)
    {
        uint32_t current_tick = Delay_GetTick();

        /* 每500ms更新一次OLED显示（避免刷新过快） */
        if (Delay_ms_nonblock(oled_update_timer, 500))
        {
            /* 获取当前频率和系数并显示 */
            uint32_t current_freq = CLKM_GetCurrentFrequency();
            uint8_t coefficient = CLKM_GetCurrentLevel() + 1; /* level 0-8 对应系数 1-9 */

            OLED_ShowString(2, 6, "     ");                /* 清空频率显示区域 */
            OLED_ShowNum(2, 6, current_freq / 1000000, 2); /* 显示MHz */
            OLED_ShowString(2, 8, "MHz");

            OLED_ShowString(3, 7, "  ");        /* 清空系数显示区域 */
            OLED_ShowNum(3, 7, coefficient, 1); /* 显示系数 */

            /* 显示距离下次频率切换的倒计时 */
            uint32_t switch_elapsed = Delay_GetElapsed(current_tick, freq_switch_timer);
            uint32_t switch_remaining = (switch_elapsed < 10000) ? (10000 - switch_elapsed) : 0;
            OLED_ShowString(4, 5, "   ");                   /* 清空倒计时显示区域 */
            OLED_ShowNum(4, 5, switch_remaining / 1000, 2); /* 显示秒数 */
            OLED_ShowString(4, 7, "s");

            oled_update_timer = current_tick;
        }

        /* 每10秒切换一次频率（测试频率变化时延时是否准确） */
        if (Delay_ms_nonblock(freq_switch_timer, 10000))
        {
            /* 切换到下一个频率 */
            current_freq_index = (current_freq_index + 1) % test_freq_count;
            CLKM_SetFixedLevel(test_freqs[current_freq_index]);

            /* 等待频率切换完成 */
            Delay_ms(200);

            /* 重新开始计时 */
            freq_switch_timer = current_tick;
        }

        /* LED1：非阻塞式延时，1000ms（1秒）闪烁一次 */
        /* 无论频率如何变化，LED1应该始终1秒闪烁一次 */
        if (Delay_ms_nonblock(led1_start, 1000))
        {
            if (led1_state == 0)
            {
                LED1_On();
                led1_state = 1;
            }
            else
            {
                LED1_Off();
                led1_state = 0;
            }
            led1_start = current_tick;
        }

        /* LED2：根据系数闪烁（系数1闪9次/秒，系数9闪1次/秒） */
        /* 频率高闪得快，频率低闪得慢 */
        /* 每次循环都重新计算LED2闪烁间隔，确保与当前频率匹配 */
        CLKM_FreqLevel_t current_level = CLKM_GetCurrentLevel();
        uint8_t coefficient = current_level + 1;        /* level 0-8 对应系数 1-9 */
        uint8_t led2_blinks_per_sec = 10 - coefficient; /* 系数1闪9次/秒，系数9闪1次/秒 */
        uint16_t led2_interval_ms = 1000 / led2_blinks_per_sec;

        if (Delay_ms_nonblock(led2_start, led2_interval_ms))
        {
            if (led2_state == 0)
            {
                LED2_On();
                led2_state = 1;
            }
            else
            {
                LED2_Off();
                led2_state = 0;
            }
            led2_start = current_tick;
        }

        /* 短暂延时，避免CPU占用过高 */
        Delay_us(100);
    }
}
