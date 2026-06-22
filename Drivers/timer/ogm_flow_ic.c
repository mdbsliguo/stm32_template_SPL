/**
 * @file ogm_flow_ic.c
 * @brief OGM 双脉冲线 — TIM 双通道输入捕获 + 四边沿互锁或下降沿交替
 */

#include "ogm_flow_ic.h"
#include "board.h"
#include "config.h"
#include "delay.h"
#include "error_handler.h"
#include "gpio.h"
#include "TIM2_TimeBase.h"
#include "stm32f10x.h"
#include "core_cm3.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_rcc.h"
#include "misc.h"
#include "system_stm32f10x.h"
#include <stdbool.h>
#include <stddef.h>

#if CONFIG_MODULE_OGM_FLOW_IC_ENABLED

#ifndef OGM_FLOW_IC_INSTANCE
#define OGM_FLOW_IC_INSTANCE    OGM_FLOW_IC_TIM2
#endif

#ifndef OGM_FLOW_IC_FILTER
#define OGM_FLOW_IC_FILTER      0x04u
#endif

#ifndef OGM_PULSE_FACTOR
#define OGM_PULSE_FACTOR        (1.0f / 450.0f)
#endif

#ifndef OGM_FLOW_IC_BOOT_MASK_MS
#define OGM_FLOW_IC_BOOT_MASK_MS  300u
#endif

#ifndef CONFIG_OGM_FLOW_IC_ALGO_FOUR_EDGE
#define CONFIG_OGM_FLOW_IC_ALGO_FOUR_EDGE  0
#endif

#if CONFIG_OGM_FLOW_IC_ALGO_FOUR_EDGE
#define OGM_LOCK_A_RISE  0x01u
#define OGM_LOCK_A_FALL  0x02u
#define OGM_LOCK_B_RISE  0x04u
#define OGM_LOCK_B_FALL  0x08u
#define OGM_LOCK_A_MASK  (OGM_LOCK_A_RISE | OGM_LOCK_A_FALL)
#define OGM_LOCK_B_MASK  (OGM_LOCK_B_RISE | OGM_LOCK_B_FALL)
#endif

#define OGM_FLOW_IC_PROCESS_MS    100u
#define OGM_FLOW_IC_ZERO_MS       1000u

/* ISR 与主循环共享 */
static volatile uint32_t g_pulse_count;
static volatile uint32_t g_last_pulse_ms;
#if CONFIG_OGM_FLOW_IC_ALGO_FOUR_EDGE
static volatile uint8_t  g_edge_locks;
#else
static volatile uint8_t  g_state;
#endif

/* 主循环内部 */
static float    s_flow_instant;
static uint32_t s_last_count;
static uint32_t s_last_tick;
static uint32_t s_last_zero_tick;
static uint32_t s_count_1s_snap;
static uint32_t s_boot_mask_until_ms;
static uint8_t  s_cc_it_enabled;

static bool g_initialized;
static OGM_FlowIC_Instance_t g_instance;
static TIM_TypeDef *g_tim;

static TIM_TypeDef *OGM_FlowIC_InstanceToPeriph(OGM_FlowIC_Instance_t inst)
{
    if (inst == OGM_FLOW_IC_TIM2) {
        return TIM2;
    }
    if (inst == OGM_FLOW_IC_TIM3) {
        return TIM3;
    }
    if (inst == OGM_FLOW_IC_TIM4) {
        return TIM4;
    }
    return NULL;
}

static uint32_t OGM_FlowIC_GetPeriphClock(TIM_TypeDef *tim)
{
    if (tim == TIM1) {
        return RCC_APB2Periph_TIM1;
    }
    if (tim == TIM2) {
        return RCC_APB1Periph_TIM2;
    }
    if (tim == TIM3) {
        return RCC_APB1Periph_TIM3;
    }
    if (tim == TIM4) {
        return RCC_APB1Periph_TIM4;
    }
    return 0u;
}

static IRQn_Type OGM_FlowIC_GetIRQn(OGM_FlowIC_Instance_t inst)
{
    if (inst == OGM_FLOW_IC_TIM2) {
        return TIM2_IRQn;
    }
    if (inst == OGM_FLOW_IC_TIM3) {
        return TIM3_IRQn;
    }
    if (inst == OGM_FLOW_IC_TIM4) {
        return TIM4_IRQn;
    }
    return (IRQn_Type)(-1);
}

static error_code_t OGM_FlowIC_ConfigGPIO(GPIO_TypeDef *port, uint16_t pin)
{
    if (port == NULL || pin == 0u) {
        return OGM_FLOW_IC_ERROR_GPIO;
    }

    /* OGM 开漏输出：MCU 侧上拉，与 NPN03/NPN04 EXTI 案例一致 */
    if (GPIO_Config(port, pin, GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_50MHz) != GPIO_OK) {
        return OGM_FLOW_IC_ERROR_GPIO;
    }

    return OGM_FLOW_IC_OK;
}

static void OGM_FlowIC_ClearPendingCapture(void)
{
    if (g_tim == NULL) {
        return;
    }
    if (TIM_GetFlagStatus(g_tim, TIM_FLAG_CC1) != RESET) {
        (void)TIM_GetCapture1(g_tim);
    }
    if (TIM_GetFlagStatus(g_tim, TIM_FLAG_CC2) != RESET) {
        (void)TIM_GetCapture2(g_tim);
    }
}

static void OGM_FlowIC_EnableCaptureIT(void)
{
    if (s_cc_it_enabled || !g_initialized || g_tim == NULL) {
        return;
    }

    OGM_FlowIC_ClearPendingCapture();
    TIM_ITConfig(g_tim, TIM_IT_CC1 | TIM_IT_CC2, ENABLE);
    s_cc_it_enabled = 1u;
    s_boot_mask_until_ms = 0u;
}

static void OGM_FlowIC_ConfigChannelIC(TIM_TypeDef *tim, uint16_t channel,
                                      uint16_t polarity, uint8_t filter)
{
    TIM_ICInitTypeDef ic;

    TIM_ICStructInit(&ic);
    ic.TIM_Channel = channel;
    ic.TIM_ICPolarity = polarity;
    ic.TIM_ICSelection = TIM_ICSelection_DirectTI;
    ic.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    ic.TIM_ICFilter = filter;
    TIM_ICInit(tim, &ic);
    TIM_CCxCmd(tim, channel, TIM_CCx_Enable);
}

static error_code_t OGM_FlowIC_InitTimeBaseStandalone(TIM_TypeDef *tim)
{
    TIM_TimeBaseInitTypeDef tb;
    uint32_t tim_clk;
    uint32_t apb1_prescaler;
    uint32_t apb_clk;
    uint8_t presc_table[] = {0, 0, 0, 0, 1, 2, 3, 4};

    apb1_prescaler = RCC->CFGR & RCC_CFGR_PPRE1;
    apb1_prescaler = (apb1_prescaler >> 8) & 0x07u;
    if (apb1_prescaler < 4u) {
        apb_clk = SystemCoreClock;
    } else {
        apb_clk = SystemCoreClock >> presc_table[apb1_prescaler];
    }
    if (apb1_prescaler >= 4u) {
        tim_clk = apb_clk * 2u;
    } else {
        tim_clk = apb_clk;
    }
    (void)tim_clk;

    TIM_TimeBaseStructInit(&tb);
    tb.TIM_Period = 0xFFFFu;
    tb.TIM_Prescaler = 71u;
    tb.TIM_ClockDivision = TIM_CKD_DIV1;
    tb.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(tim, &tb);
    TIM_Cmd(tim, ENABLE);

    return OGM_FLOW_IC_OK;
}

static void OGM_FlowIC_OnValidEdge(void)
{
    g_pulse_count++;
    g_last_pulse_ms = Delay_GetTick();
}

#if CONFIG_OGM_FLOW_IC_ALGO_FOUR_EDGE

static uint8_t OGM_FlowIC_ReadPinLevel(GPIO_TypeDef *port, uint16_t pin)
{
    return GPIO_ReadPin(port, pin) ? 1u : 0u;
}

static uint16_t OGM_FlowIC_PolarityForNextEdge(uint8_t level_high)
{
    return level_high ? TIM_ICPolarity_Falling : TIM_ICPolarity_Rising;
}

static uint8_t OGM_FlowIC_CCIsRisingCapture(uint16_t channel)
{
    if (g_tim == NULL) {
        return 0u;
    }
    if (channel == TIM_Channel_1) {
        return (g_tim->CCER & TIM_CCER_CC1P) == 0u ? 1u : 0u;
    }
    return (g_tim->CCER & TIM_CCER_CC2P) == 0u ? 1u : 0u;
}

static void OGM_FlowIC_ToggleCCPolarity(uint16_t channel)
{
    if (g_tim == NULL) {
        return;
    }
    if (channel == TIM_Channel_1) {
        g_tim->CCER ^= TIM_CCER_CC1P;
    } else {
        g_tim->CCER ^= TIM_CCER_CC2P;
    }
}

static void OGM_FlowIC_FourEdgeProcess(uint8_t is_channel_a, uint8_t is_rising)
{
    uint8_t lock_bit;

    if (is_channel_a) {
        lock_bit = is_rising ? OGM_LOCK_A_RISE : OGM_LOCK_A_FALL;
        g_edge_locks &= (uint8_t)(~OGM_LOCK_B_MASK);
    } else {
        lock_bit = is_rising ? OGM_LOCK_B_RISE : OGM_LOCK_B_FALL;
        g_edge_locks &= (uint8_t)(~OGM_LOCK_A_MASK);
    }

    if ((g_edge_locks & lock_bit) == 0u) {
        OGM_FlowIC_OnValidEdge();
        g_edge_locks |= lock_bit;
    }
}

static void OGM_FlowIC_ProcessCapture(uint16_t channel, uint8_t is_channel_a)
{
    uint8_t is_rising;

    is_rising = OGM_FlowIC_CCIsRisingCapture(channel);
    OGM_FlowIC_FourEdgeProcess(is_channel_a, is_rising);
    OGM_FlowIC_ToggleCCPolarity(channel);
}

#else /* 仅下降沿 A/B 交替 */

static void OGM_FlowIC_ProcessChannelA(void)
{
    if (g_state == (uint8_t)OGM_FLOW_IC_STATE_WAIT_A) {
        OGM_FlowIC_OnValidEdge();
        g_state = (uint8_t)OGM_FLOW_IC_STATE_WAIT_B;
    }
}

static void OGM_FlowIC_ProcessChannelB(void)
{
    if (g_state == (uint8_t)OGM_FLOW_IC_STATE_WAIT_B) {
        OGM_FlowIC_OnValidEdge();
        g_state = (uint8_t)OGM_FLOW_IC_STATE_WAIT_A;
    }
}

#endif /* CONFIG_OGM_FLOW_IC_ALGO_FOUR_EDGE */

OGM_FlowIC_Instance_t OGM_FlowIC_GetActiveInstance(void)
{
    return g_instance;
}

uint8_t OGM_FlowIC_IsInitialized(void)
{
    return g_initialized ? 1u : 0u;
}

error_code_t OGM_FlowIC_Init(void)
{
    NVIC_InitTypeDef nvic;
    uint32_t tim_clock;
    uint8_t filter;

    if (g_initialized) {
        return OGM_FLOW_IC_ERROR_ALREADY_INIT;
    }

    g_instance = (OGM_FlowIC_Instance_t)OGM_FLOW_IC_INSTANCE;
    if (g_instance >= OGM_FLOW_IC_MAX) {
        return OGM_FLOW_IC_ERROR_INVALID_INSTANCE;
    }

    g_tim = OGM_FlowIC_InstanceToPeriph(g_instance);
    if (g_tim == NULL) {
        return OGM_FLOW_IC_ERROR_INVALID_INSTANCE;
    }

    filter = (uint8_t)OGM_FLOW_IC_FILTER;

    if (OGM_FlowIC_ConfigGPIO(OGM_CH_A_PORT, OGM_CH_A_PIN) != OGM_FLOW_IC_OK) {
        return OGM_FLOW_IC_ERROR_GPIO;
    }
    if (OGM_FlowIC_ConfigGPIO(OGM_CH_B_PORT, OGM_CH_B_PIN) != OGM_FLOW_IC_OK) {
        return OGM_FLOW_IC_ERROR_GPIO;
    }

    tim_clock = OGM_FlowIC_GetPeriphClock(g_tim);
    if (tim_clock == 0u) {
        return OGM_FLOW_IC_ERROR_INVALID_INSTANCE;
    }

    if (g_tim == TIM1) {
        RCC_APB2PeriphClockCmd(tim_clock, ENABLE);
    } else {
        RCC_APB1PeriphClockCmd(tim_clock, ENABLE);
    }

    if (g_instance == OGM_FLOW_IC_TIM2) {
        if (!TIM2_TimeBase_IsInitialized()) {
            return OGM_FLOW_IC_ERROR_TIMEBASE;
        }
    } else {
        if (OGM_FlowIC_InitTimeBaseStandalone(g_tim) != OGM_FLOW_IC_OK) {
            return OGM_FLOW_IC_ERROR_TIMEBASE;
        }
    }

#if CONFIG_OGM_FLOW_IC_ALGO_FOUR_EDGE
    {
        uint8_t level_a;
        uint8_t level_b;
        uint16_t pol_a;
        uint16_t pol_b;

        level_a = OGM_FlowIC_ReadPinLevel(OGM_CH_A_PORT, OGM_CH_A_PIN);
        level_b = OGM_FlowIC_ReadPinLevel(OGM_CH_B_PORT, OGM_CH_B_PIN);
        pol_a = OGM_FlowIC_PolarityForNextEdge(level_a);
        pol_b = OGM_FlowIC_PolarityForNextEdge(level_b);
        OGM_FlowIC_ConfigChannelIC(g_tim, TIM_Channel_1, pol_a, filter);
        OGM_FlowIC_ConfigChannelIC(g_tim, TIM_Channel_2, pol_b, filter);
        g_edge_locks = 0u;
    }
#else
    OGM_FlowIC_ConfigChannelIC(g_tim, TIM_Channel_1, TIM_ICPolarity_Falling, filter);
    OGM_FlowIC_ConfigChannelIC(g_tim, TIM_Channel_2, TIM_ICPolarity_Falling, filter);
    g_state = (uint8_t)OGM_FLOW_IC_STATE_WAIT_A;
#endif

    g_pulse_count = 0u;
    g_last_pulse_ms = 0u;
    s_flow_instant = 0.0f;
    s_last_count = 0u;
    s_last_tick = Delay_GetTick();
    s_last_zero_tick = s_last_tick;
    s_count_1s_snap = 0u;
    s_cc_it_enabled = 0u;
    s_boot_mask_until_ms = Delay_GetTick() + OGM_FLOW_IC_BOOT_MASK_MS;

    /* 上电屏蔽期内不使能 CC 中断，避免毛刺占满 CPU 导致 OLED/I2C 无响应 */
    TIM_ITConfig(g_tim, TIM_IT_CC1 | TIM_IT_CC2, DISABLE);
    OGM_FlowIC_ClearPendingCapture();

    nvic.NVIC_IRQChannel = OGM_FlowIC_GetIRQn(g_instance);
    nvic.NVIC_IRQChannelPreemptionPriority = 2;
    nvic.NVIC_IRQChannelSubPriority = 2;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    g_initialized = true;
    return OGM_FLOW_IC_OK;
}

error_code_t OGM_FlowIC_Deinit(void)
{
    if (!g_initialized || g_tim == NULL) {
        return OGM_FLOW_IC_ERROR_NOT_INIT;
    }

    TIM_ITConfig(g_tim, TIM_IT_CC1 | TIM_IT_CC2, DISABLE);
    TIM_CCxCmd(g_tim, TIM_Channel_1, TIM_CCx_Disable);
    TIM_CCxCmd(g_tim, TIM_Channel_2, TIM_CCx_Disable);

    s_cc_it_enabled = 0u;
    g_initialized = false;
    g_tim = NULL;
    return OGM_FLOW_IC_OK;
}

void OGM_FlowIC_IRQHandler(void)
{
    uint32_t sr;

    if (!g_initialized || g_tim == NULL || !s_cc_it_enabled) {
        return;
    }

    sr = g_tim->SR;

    if (sr & TIM_SR_CC1IF) {
        (void)g_tim->CCR1;
        TIM_ClearITPendingBit(g_tim, TIM_IT_CC1);
#if CONFIG_OGM_FLOW_IC_ALGO_FOUR_EDGE
        OGM_FlowIC_ProcessCapture(TIM_Channel_1, 1u);
#else
        OGM_FlowIC_ProcessChannelA();
#endif
    }

    if (sr & TIM_SR_CC2IF) {
        (void)g_tim->CCR2;
        TIM_ClearITPendingBit(g_tim, TIM_IT_CC2);
#if CONFIG_OGM_FLOW_IC_ALGO_FOUR_EDGE
        OGM_FlowIC_ProcessCapture(TIM_Channel_2, 0u);
#else
        OGM_FlowIC_ProcessChannelB();
#endif
    }
}

error_code_t OGM_FlowIC_Process100ms(void)
{
    uint32_t now_count;
    uint32_t now_tick;
    uint32_t diff_count;
    uint32_t diff_ms;

    if (!g_initialized) {
        return OGM_FLOW_IC_ERROR_NOT_INIT;
    }

    if (!s_cc_it_enabled && (Delay_GetTick() >= s_boot_mask_until_ms)) {
        OGM_FlowIC_EnableCaptureIT();
    }

    now_tick = Delay_GetTick();
    if (Delay_GetElapsed(now_tick, s_last_tick) < OGM_FLOW_IC_PROCESS_MS) {
        return OGM_FLOW_IC_OK;
    }

    now_count = g_pulse_count;
    diff_count = now_count - s_last_count;
    diff_ms = now_tick - s_last_tick;

    if (diff_ms > 0u) {
        float counts_per_sec;

        counts_per_sec = ((float)diff_count * 1000.0f) / (float)diff_ms;
        s_flow_instant = counts_per_sec * OGM_PULSE_FACTOR * 60.0f;
    }

    s_last_count = now_count;
    s_last_tick = now_tick;

    if (Delay_GetElapsed(now_tick, s_last_zero_tick) >= OGM_FLOW_IC_ZERO_MS) {
        uint32_t zero_diff;

        zero_diff = now_count - s_count_1s_snap;
        if (zero_diff == 0u) {
            s_flow_instant = 0.0f;
        }
        s_count_1s_snap = now_count;
        s_last_zero_tick = now_tick;
    }

    return OGM_FLOW_IC_OK;
}

error_code_t OGM_FlowIC_GetInstantFlow(float *flow_lpm)
{
    if (flow_lpm == NULL) {
        return OGM_FLOW_IC_ERROR_NULL_PTR;
    }
    if (!g_initialized) {
        return OGM_FLOW_IC_ERROR_NOT_INIT;
    }
    *flow_lpm = s_flow_instant;
    return OGM_FLOW_IC_OK;
}

error_code_t OGM_FlowIC_GetCount(uint32_t *count)
{
    if (count == NULL) {
        return OGM_FLOW_IC_ERROR_NULL_PTR;
    }
    if (!g_initialized) {
        return OGM_FLOW_IC_ERROR_NOT_INIT;
    }
    *count = g_pulse_count;
    return OGM_FLOW_IC_OK;
}

error_code_t OGM_FlowIC_GetState(uint8_t *state)
{
    if (state == NULL) {
        return OGM_FLOW_IC_ERROR_NULL_PTR;
    }
    if (!g_initialized) {
        return OGM_FLOW_IC_ERROR_NOT_INIT;
    }
#if CONFIG_OGM_FLOW_IC_ALGO_FOUR_EDGE
    *state = g_edge_locks;
#else
    *state = g_state;
#endif
    return OGM_FLOW_IC_OK;
}

error_code_t OGM_FlowIC_ResetCount(void)
{
    if (!g_initialized) {
        return OGM_FLOW_IC_ERROR_NOT_INIT;
    }

    __disable_irq();
    g_pulse_count = 0u;
#if CONFIG_OGM_FLOW_IC_ALGO_FOUR_EDGE
    g_edge_locks = 0u;
#else
    g_state = (uint8_t)OGM_FLOW_IC_STATE_WAIT_A;
#endif
    g_last_pulse_ms = 0u;
    s_last_count = 0u;
    s_count_1s_snap = 0u;
    s_flow_instant = 0.0f;
    __enable_irq();

    return OGM_FLOW_IC_OK;
}

error_code_t OGM_FlowIC_IsFlowActive(uint32_t idle_ms, uint8_t *active)
{
    if (active == NULL) {
        return OGM_FLOW_IC_ERROR_NULL_PTR;
    }
    if (!g_initialized) {
        return OGM_FLOW_IC_ERROR_NOT_INIT;
    }

    if (g_last_pulse_ms == 0u) {
        *active = 0u;
        return OGM_FLOW_IC_OK;
    }

    *active = (Delay_GetElapsed(Delay_GetTick(), g_last_pulse_ms) < idle_ms) ? 1u : 0u;
    return OGM_FLOW_IC_OK;
}

#if defined(CONFIG_OGM_FLOW_IC_TEST_INJECT) && CONFIG_OGM_FLOW_IC_TEST_INJECT
error_code_t OGM_FlowIC_InjectEdge(uint8_t channel, uint8_t is_rising)
{
    if (!g_initialized) {
        return OGM_FLOW_IC_ERROR_NOT_INIT;
    }
#if CONFIG_OGM_FLOW_IC_ALGO_FOUR_EDGE
    if (channel == 0u) {
        OGM_FlowIC_FourEdgeProcess(1u, is_rising);
    } else if (channel == 1u) {
        OGM_FlowIC_FourEdgeProcess(0u, is_rising);
    } else {
        return OGM_FLOW_IC_ERROR_INVALID_INSTANCE;
    }
#else
    (void)is_rising;
    if (channel == 0u) {
        OGM_FlowIC_ProcessChannelA();
    } else if (channel == 1u) {
        OGM_FlowIC_ProcessChannelB();
    } else {
        return OGM_FLOW_IC_ERROR_INVALID_INSTANCE;
    }
#endif
    return OGM_FLOW_IC_OK;
}

error_code_t OGM_FlowIC_InjectPulse(uint8_t channel)
{
    return OGM_FlowIC_InjectEdge(channel, 0u);
}
#endif

#endif /* CONFIG_MODULE_OGM_FLOW_IC_ENABLED */
