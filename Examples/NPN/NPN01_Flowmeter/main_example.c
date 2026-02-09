/**
 * @file main_example.c
 * @brief NPN01 - 流量计 NPN 脉冲计数示例
 * @example Examples/NPN/NPN01_Flowmeter/main_example.c
 * @status 待调试
 * @details 使用 EXTI 对 NPN 开漏流量计的脉冲输出进行计数，可选 OLED 显示累计值
 *
 * 硬件要求：
 * - 流量计 NPN 信号线接 PA0（EXTI Line 0，下降沿触发）
 * - LED1 接 PA1（状态指示）
 * - 可选：OLED 接 PB8/PB9（软件 I2C）
 *
 * 使用方法：
 * 1. 打开案例工程：复制 EXTI01 的 Examples.uvprojx/uvoptx 后改为本目录（见 README）
 * 2. 按实际硬件修改本目录下的 board.h 配置
 * 3. 编译运行（F7 编译，F8 下载）
 *
 * 注意：本示例为独立工程，无需复制其他文件
 */

#include "system_init.h"
#include "exti.h"
#include "led.h"
#include "oled_ssd1306.h"
#include "delay.h"
#include "board.h"
#include "gpio.h"
#include "stm32f10x.h"
#include "stm32f10x_gpio.h"  /* GPIO_EXTILineConfig */
#include <stdint.h>
#include <stddef.h>

/* 每升脉冲数配置：按流量计规格修改（示例：450 脉冲/L） */
#define PULSES_PER_LITER_DEFAULT 72u

/* 计数器：ISR 和主循环均访问，需使用 volatile */
static volatile uint32_t g_counter = 0;
static uint32_t g_pulses_per_liter = PULSES_PER_LITER_DEFAULT;

/**
 * @brief EXTI 中断回调（流量计脉冲）
 * @param line EXTI 线号
 * @param user_data 用户数据
 */
static void Flowmeter_Callback(EXTI_Line_t line, void *user_data)
{
    (void)line;
    (void)user_data;
    g_counter++;  /* 仅计数，避免在中断内做耗时操作 */
}

int main(void)
{
    /* 系统初始化
     * System_Init() 会自动初始化：
     * - SysTick 延时模块（Delay_Init）
     * - LED 驱动（按配置表初始化启用的 LED）
     */
    System_Init();

    /* OLED 初始化（可选） */
    if (OLED_Init() != OLED_OK)
    {
        /* OLED 初始化失败，快闪提示并停留 */
        while (1)
        {
            LED1_On();
            Delay_ms(80);
            LED1_Off();
            Delay_ms(80);
        }
    }
    OLED_Clear();
    OLED_ShowString(1, 1, "NPN Flowmeter");
    OLED_ShowString(2, 1, "Count: 0");
    OLED_ShowString(3, 1, "P/L:");
    OLED_ShowNum(3, 6, g_pulses_per_liter, 5);
    OLED_ShowString(4, 1, "Vol:0.000L");

    /* 初始化 EXTI0：PA0，下降沿，中断模式 */
    if (EXTI_HW_Init(EXTI_LINE_0, EXTI_TRIGGER_FALLING, EXTI_MODE_INTERRUPT) != EXTI_OK)
    {
        /* 初始化失败，LED 快闪提示 */
        while (1)
        {
            LED1_On();
            Delay_ms(100);
            LED1_Off();
            Delay_ms(100);
        }
    }

    /* 重新配置 PA0 为上拉输入（EXTI_HW_Init 默认配置为浮空输入） */
    GPIO_Config(GPIOA, GPIO_Pin_0, GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_50MHz);

    /* 设置 EXTI 回调 */
    if (EXTI_SetCallback(EXTI_LINE_0, Flowmeter_Callback, NULL) != EXTI_OK)
    {
        while (1)
        {
            LED1_On();
            Delay_ms(50);
            LED1_Off();
            Delay_ms(50);
        }
    }

    /* 使能 EXTI 中断 */
    if (EXTI_Enable(EXTI_LINE_0) != EXTI_OK)
    {
        while (1)
        {
            LED1_On();
            Delay_ms(50);
            LED1_Off();
            Delay_ms(50);
        }
    }

    /* 主循环：显示计数并闪烁 LED 指示有新脉冲 */
    uint32_t last_counter = 0;
    while (1)
    {
        if (g_counter != last_counter)
        {
            last_counter = g_counter;

            /* 保护脉冲/升配置不为 0 */
            if (g_pulses_per_liter == 0u)
            {
                g_pulses_per_liter = 1u;
            }

            /* 计算体积：L 和 mL（整数运算避免浮点） */
            uint32_t liter_whole = last_counter / g_pulses_per_liter;
            uint32_t ml = (last_counter % g_pulses_per_liter) * 1000u / g_pulses_per_liter;

            /* 更新 OLED 显示 */
            /* 列号+长度需<=16，避免参数错误导致不更新显示 */
            OLED_ShowNum(2, 6, g_counter, 10);
            OLED_ShowNum(3, 6, g_pulses_per_liter, 5);
            OLED_ShowNum(4, 6, liter_whole, 5);
            OLED_ShowString(4, 11, ".");
            OLED_ShowNum(4, 12, ml, 3);

            /* LED 闪烁提示 */
            LED1_Toggle();
        }

        /* 软件限速，降低 CPU 占用；也可作为简单消抖显示节拍 */
        Delay_ms(10);
    }
}
