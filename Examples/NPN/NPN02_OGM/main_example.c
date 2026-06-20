/**
 * @file main_example.c
 * @brief NPN02 - OGM 双通道 NPN 正交互锁计数示例
 * @example Examples/NPN/NPN02_OGM/main_example.c
 * @status 已调试
 * @details PA0/PA1 双边沿 EXTI + 四边沿互锁（一圈 8 边沿，仅累加）
 *
 * 硬件要求：
 * - 通道 A：PA0（EXTI0）
 * - 通道 B：PA1（EXTI1）
 * - LED1：PB12
 * - OLED：PB8/PB9
 */

#include "system_init.h"
#include "exti.h"
#include "led.h"
#include "oled_ssd1306.h"
#include "delay.h"
#include "board.h"
#include "gpio.h"
#include "TIM2_TimeBase.h"
#include "stm32f10x.h"
#include <stdint.h>
#include <stddef.h>

/* 四边沿互锁位（A↑/A↓/B↑/B↓） */
#define OGM_LOCK_A_RISE  0x01u
#define OGM_LOCK_A_FALL  0x02u
#define OGM_LOCK_B_RISE  0x04u
#define OGM_LOCK_B_FALL  0x08u
#define OGM_LOCK_A_MASK  (OGM_LOCK_A_RISE | OGM_LOCK_A_FALL)
#define OGM_LOCK_B_MASK  (OGM_LOCK_B_RISE | OGM_LOCK_B_FALL)

static volatile uint32_t g_count = 0;
static volatile uint8_t  g_last_a = 0;
static volatile uint8_t  g_last_b = 0;
static volatile uint8_t  g_edge_locks = 0;

static volatile uint32_t g_count_snap = 0;
static volatile uint32_t g_count_delta_ms = 0;

static uint8_t OGM_ReadLevel(GPIO_TypeDef *port, uint16_t pin)
{
    return GPIO_ReadPin(port, pin) ? 1u : 0u;
}

static uint32_t OGM_SnapshotCount(void)
{
    uint32_t snap;

    __disable_irq();
    snap = g_count;
    __enable_irq();
    return snap;
}

/**
 * @brief 单通道边沿：对侧解锁 + 本边沿互锁 + 计数
 */
static void OGM_ProcessChannel(uint8_t is_channel_a)
{
    uint8_t level_new;
    uint8_t level_last;
    uint8_t lock_bit;

    if (is_channel_a)
    {
        level_new = OGM_ReadLevel(OGM_CH_A_PORT, OGM_CH_A_PIN);
        level_last = g_last_a;
        lock_bit = level_new ? OGM_LOCK_A_RISE : OGM_LOCK_A_FALL;
    }
    else
    {
        level_new = OGM_ReadLevel(OGM_CH_B_PORT, OGM_CH_B_PIN);
        level_last = g_last_b;
        lock_bit = level_new ? OGM_LOCK_B_RISE : OGM_LOCK_B_FALL;
    }

    if (level_new == level_last)
    {
        return;
    }

    if (is_channel_a)
    {
        g_edge_locks &= (uint8_t)(~OGM_LOCK_B_MASK);
    }
    else
    {
        g_edge_locks &= (uint8_t)(~OGM_LOCK_A_MASK);
    }

    if ((g_edge_locks & lock_bit) == 0u)
    {
        g_count++;
        g_edge_locks |= lock_bit;
    }

    if (is_channel_a)
    {
        g_last_a = level_new;
    }
    else
    {
        g_last_b = level_new;
    }
}

static void OGM_ChannelA_Callback(EXTI_Line_t line, void *user_data)
{
    (void)line;
    (void)user_data;
    OGM_ProcessChannel(1u);
}

static void OGM_ChannelB_Callback(EXTI_Line_t line, void *user_data)
{
    (void)line;
    (void)user_data;
    OGM_ProcessChannel(0u);
}

static uint8_t OGM_InitExtiChannel(EXTI_Line_t line, EXTI_Callback_t callback,
                                   GPIO_TypeDef *port, uint16_t pin)
{
    if (EXTI_HW_Init(line, EXTI_TRIGGER_RISING_FALLING, EXTI_MODE_INTERRUPT) != EXTI_OK)
    {
        return 0u;
    }

    if (GPIO_Config(port, pin, GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_50MHz) != GPIO_OK)
    {
        return 0u;
    }

    if (EXTI_SetCallback(line, callback, NULL) != EXTI_OK)
    {
        return 0u;
    }

    if (EXTI_Enable(line) != EXTI_OK)
    {
        return 0u;
    }

    EXTI_ClearPending(line);
    return 1u;
}

static uint8_t OGM_InitInputs(void)
{
    g_last_a = OGM_ReadLevel(OGM_CH_A_PORT, OGM_CH_A_PIN);
    g_last_b = OGM_ReadLevel(OGM_CH_B_PORT, OGM_CH_B_PIN);
    g_edge_locks = 0u;
    g_count = 0u;
    g_count_snap = 0u;
    g_count_delta_ms = 0u;

    if (!OGM_InitExtiChannel(EXTI_LINE_0, OGM_ChannelA_Callback, OGM_CH_A_PORT, OGM_CH_A_PIN))
    {
        return 0u;
    }

    if (!OGM_InitExtiChannel(EXTI_LINE_1, OGM_ChannelB_Callback, OGM_CH_B_PORT, OGM_CH_B_PIN))
    {
        return 0u;
    }

    return 1u;
}

static void OGM_ErrorBlink(uint32_t on_ms, uint32_t off_ms)
{
    while (1)
    {
        LED1_On();
        Delay_ms(on_ms);
        LED1_Off();
        Delay_ms(off_ms);
    }
}

static void OGM_ShowDelta(uint32_t delta)
{
    OLED_ShowString(3, 1, "d/1ms:");
    OLED_ShowNum(3, 7, delta, 4);
}

static void OGM_ShowLevels(uint8_t level_a, uint8_t level_b)
{
    OLED_ShowString(4, 1, "A:");
    OLED_ShowString(4, 3, level_a ? "H" : "L");
    OLED_ShowString(4, 5, "B:");
    OLED_ShowString(4, 7, level_b ? "H" : "L");
}

static void OGM_UpdateCountDelta(void)
{
    uint32_t now;
    uint32_t delta;

    __disable_irq();
    now = g_count;
    delta = now - g_count_snap;
    g_count_snap = now;
    g_count_delta_ms = delta;
    __enable_irq();
}

int main(void)
{
    uint32_t last_count = 0;
    uint32_t last_delta = 0xFFFFFFFFu;
    uint32_t last_flow_tick = 0;
    uint32_t last_ui_tick = 0;
    uint8_t  last_level_a = 0xFFu;
    uint8_t  last_level_b = 0xFFu;
    uint8_t  level_a;
    uint8_t  level_b;

    System_Init();

    if (OLED_Init() != OLED_OK)
    {
        OGM_ErrorBlink(80, 80);
    }

    if (!OGM_InitInputs())
    {
        OGM_ErrorBlink(100, 100);
    }

    OLED_Clear();
    OLED_ShowString(1, 1, "OGM Dual");
    OLED_ShowString(2, 1, "Cnt:");
    OLED_ShowNum(2, 5, 0, 8);
    OGM_ShowDelta(0);

    level_a = OGM_ReadLevel(OGM_CH_A_PORT, OGM_CH_A_PIN);
    level_b = OGM_ReadLevel(OGM_CH_B_PORT, OGM_CH_B_PIN);
    last_level_a = level_a;
    last_level_b = level_b;
    OGM_ShowLevels(level_a, level_b);

    last_flow_tick = g_task_tick;
    last_ui_tick = g_task_tick;
    g_count_snap = g_count;

    while (1)
    {
        uint32_t now_count;

        while (last_flow_tick != g_task_tick)
        {
            last_flow_tick++;
            OGM_UpdateCountDelta();
        }

        now_count = OGM_SnapshotCount();
        if (now_count != last_count)
        {
            last_count = now_count;
            OLED_ShowNum(2, 5, last_count, 8);
            LED1_Toggle();
        }

        if ((uint32_t)(g_task_tick - last_ui_tick) >= 100u)
        {
            last_ui_tick = g_task_tick;

            if (g_count_delta_ms != last_delta)
            {
                last_delta = g_count_delta_ms;
                OGM_ShowDelta(last_delta);
            }

            level_a = OGM_ReadLevel(OGM_CH_A_PORT, OGM_CH_A_PIN);
            level_b = OGM_ReadLevel(OGM_CH_B_PORT, OGM_CH_B_PIN);

            if (level_a != last_level_a || level_b != last_level_b)
            {
                last_level_a = level_a;
                last_level_b = level_b;
                OGM_ShowLevels(level_a, level_b);
            }
        }

        Delay_ms(1);
    }
}
