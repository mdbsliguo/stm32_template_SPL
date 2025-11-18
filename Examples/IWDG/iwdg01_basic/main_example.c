/**
 * @file main_example.c
 * @brief 案例 - 独立看门狗基础使用
 * @example Examples/iwdg/iwdg01_basic/main_example.c
 * @status 已创建
 * @details 演示独立看门狗（IWDG）的初始化、启用和喂狗操作
 *
 * 硬件要求：
 * - OLED显示屏（SSD1306，I2C接口）- 用于显示看门狗状态
 *   - SCL连接到PB8
 *   - SDA连接到PB9
 * - LED1连接到PA1（可选，用于状态指示）
 *
 * 硬件配置：
 * 在 board.h 中配置：
 * - OLED_I2C_CONFIG: OLED I2C配置（PB8和PB9）
 * - LED_CONFIGS: LED配置（PA1）
 *
 * 使用方法：
 * 1. 根据实际硬件修改 board.h 中的配置
 * 2. 打开 Examples.uvprojx 编译运行
 * 3. 观察OLED显示看门狗状态
 * 4. 测试：注释掉IWDG_Refresh()调用，观察系统是否复位
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "oled_ssd1306.h"
#include "delay.h"
#include "iwdg.h"
#include "led.h"

int main(void)
{
    IWDG_Status_t iwdg_status;
    OLED_Status_t oled_status;
    uint32_t timeout_ms;
    uint32_t counter = 0;
    char str_buf[32];

    /* 系统初始化 */
    System_Init();

    /* OLED初始化 */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        /* 初始化失败，LED快速闪烁提示 */
        while (1)
        {
            LED1_On();
            Delay_ms(100);
            LED1_Off();
            Delay_ms(100);
        }
    }

    /* 清屏 */
    OLED_Clear();

    /* 显示标题 */
    OLED_ShowString(1, 1, "IWDG Demo");
    OLED_ShowString(2, 1, "Init...");

    /* 初始化看门狗（使用默认配置：1000ms超时） */
    iwdg_status = IWDG_Init(NULL);
    if (iwdg_status != IWDG_OK)
    {
        OLED_ShowString(2, 1, "Init Failed!");
        while (1)
        {
            Delay_ms(1000);
        }
    }

    /* 获取超时时间 */
    timeout_ms = IWDG_GetTimeout();

    /* 显示超时时间 */
    OLED_ShowString(2, 1, "Timeout: ");
    OLED_ShowNum(2, 10, timeout_ms, 5);
    OLED_ShowString(2, 15, "ms");

    /* 启用看门狗 */
    iwdg_status = IWDG_Start();
    if (iwdg_status != IWDG_OK)
    {
        OLED_ShowString(3, 1, "Start Failed!");
        while (1)
        {
            Delay_ms(1000);
        }
    }

    OLED_ShowString(3, 1, "IWDG Started");
    OLED_ShowString(4, 1, "Refresh OK");

    /* 主循环：定期喂狗并显示状态 */
    while (1)
    {
        /* 喂狗（刷新看门狗计数器） */
        iwdg_status = IWDG_Refresh();
        if (iwdg_status == IWDG_OK)
        {
            /* 显示刷新计数 */
            OLED_ShowString(4, 1, "Refresh: ");
            OLED_ShowNum(4, 10, counter, 5);
            counter++;
        }
        else
        {
            OLED_ShowString(4, 1, "Refresh Failed!");
        }

        /* LED闪烁指示系统运行 */
        LED1_On();
        Delay_ms(100);
        LED1_Off();

        /* 延时900ms（小于看门狗超时时间1000ms） */
        Delay_ms(900);
    }
}

