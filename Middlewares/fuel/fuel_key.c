/**
 * @file fuel_key.c
 * @brief PA4/PA5 keys: poll debounce, short on release, long on hold or release
 */

#include "fuel_key.h"
#include "gpio.h"
#include "board.h"
#include "delay.h"
#include "log.h"
#include "config.h"
#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_exti.h"
#include "misc.h"
#include <stddef.h>

#define FUEL_KEY_EVT_Q_SIZE    8u

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint8_t       last_down;
    uint8_t       long_fired;
    uint32_t      press_start_ms;
} fuel_key_slot_t;

static fuel_key_slot_t s_green;
static fuel_key_slot_t s_red;
static uint8_t s_inited;

static fuel_key_event_t s_evt_q[FUEL_KEY_EVT_Q_SIZE];
static volatile uint8_t s_evt_q_head;
static volatile uint8_t s_evt_q_tail;

static uint8_t fuel_key_is_down(const fuel_key_slot_t *k)
{
    uint8_t level;

    if (k == NULL) {
        return 0u;
    }

    level = GPIO_ReadPin(k->port, k->pin);
#if BTN_ACTIVE_LOW
    return (level == Bit_RESET) ? 1u : 0u;
#else
    return (level == Bit_SET) ? 1u : 0u;
#endif
}

static const char *fuel_key_evt_label(fuel_key_event_t evt)
{
    switch (evt) {
        case FUEL_KEY_EVT_GREEN_SHORT: return "PA4 green SHORT";
        case FUEL_KEY_EVT_GREEN_LONG:  return "PA4 green LONG";
        case FUEL_KEY_EVT_RED_SHORT:   return "PA5 red SHORT";
        case FUEL_KEY_EVT_RED_LONG:    return "PA5 red LONG";
        default:                       return "unknown";
    }
}

static void fuel_key_enqueue(fuel_key_event_t evt)
{
    uint8_t next;

    if (evt == FUEL_KEY_EVT_NONE) {
        return;
    }

    next = (uint8_t)((s_evt_q_tail + 1u) % FUEL_KEY_EVT_Q_SIZE);
    if (next == s_evt_q_head) {
        LOG_WARN("KEY", "evt queue full, drop %s", fuel_key_evt_label(evt));
        return;
    }

    s_evt_q[s_evt_q_tail] = evt;
    s_evt_q_tail = next;
}

static void fuel_key_log_event(fuel_key_event_t evt, uint32_t held_ms)
{
#if defined(CONFIG_MODULE_LOG_ENABLED) && CONFIG_MODULE_LOG_ENABLED
    LOG_INFO("KEY", ">>> %s (held %ums)", fuel_key_evt_label(evt),
             (unsigned int)held_ms);
#else
    (void)evt;
    (void)held_ms;
#endif
}

static void fuel_key_fire_long(fuel_key_slot_t *k, uint8_t is_green, uint32_t held_ms)
{
    fuel_key_event_t long_evt;

    if (k->long_fired) {
        return;
    }

    k->long_fired = 1u;
    long_evt = is_green ? FUEL_KEY_EVT_GREEN_LONG : FUEL_KEY_EVT_RED_LONG;
    fuel_key_enqueue(long_evt);
    fuel_key_log_event(long_evt, held_ms);
}

static void fuel_key_on_fall(fuel_key_slot_t *k, uint32_t now_ms)
{
    k->press_start_ms = now_ms;
    k->last_down = 1u;
    k->long_fired = 0u;
}

static void fuel_key_on_release(fuel_key_slot_t *k, uint8_t is_green, uint32_t now_ms)
{
    fuel_key_event_t short_evt;
    uint32_t held;

    if (!k->last_down || k->press_start_ms == 0u) {
        k->last_down = 0u;
        k->press_start_ms = 0u;
        k->long_fired = 0u;
        return;
    }

    held = Delay_GetElapsed(now_ms, k->press_start_ms);

    if (!k->long_fired) {
        if (held >= FUEL_KEY_LONG_PRESS_MS) {
            /* 主循环曾被 OLED/ModBus 阻塞时，补发 LONG */
            fuel_key_fire_long(k, is_green, held);
        } else {
            short_evt = is_green ? FUEL_KEY_EVT_GREEN_SHORT : FUEL_KEY_EVT_RED_SHORT;
            fuel_key_enqueue(short_evt);
            fuel_key_log_event(short_evt, held);
        }
    }

    k->last_down = 0u;
    k->press_start_ms = 0u;
    k->long_fired = 0u;
}

static void fuel_key_check_long_hold(fuel_key_slot_t *k, uint8_t is_green,
                                     uint32_t now_ms)
{
    uint32_t held;

    if (!k->last_down || k->press_start_ms == 0u || k->long_fired) {
        return;
    }

    held = Delay_GetElapsed(now_ms, k->press_start_ms);
    if (held >= FUEL_KEY_LONG_PRESS_MS) {
        fuel_key_fire_long(k, is_green, held);
    }
}

#if FUEL_KEY_USE_EXTI
static error_code_t fuel_key_init_exti(void)
{
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource4);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource5);

    EXTI_StructInit(&exti);
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
#if BTN_ACTIVE_LOW
    exti.EXTI_Trigger = EXTI_Trigger_Falling;
#else
    exti.EXTI_Trigger = EXTI_Trigger_Rising;
#endif
    exti.EXTI_LineCmd = ENABLE;

    exti.EXTI_Line = EXTI_Line4;
    EXTI_Init(&exti);

    exti.EXTI_Line = EXTI_Line5;
    EXTI_Init(&exti);

    nvic.NVIC_IRQChannel = EXTI4_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1u;
    nvic.NVIC_IRQChannelSubPriority = 0u;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    nvic.NVIC_IRQChannel = EXTI9_5_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1u;
    nvic.NVIC_IRQChannelSubPriority = 1u;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    return FUEL_KEY_OK;
}
#endif

void FuelKey_EXTI_IRQHandler(uint8_t exti_line)
{
    fuel_key_slot_t *k;
    uint32_t line_bit;
    uint32_t now_ms;

    if (!s_inited) {
        return;
    }

    if (exti_line == 4u) {
        k = &s_green;
        line_bit = EXTI_Line4;
    } else if (exti_line == 5u) {
        k = &s_red;
        line_bit = EXTI_Line5;
    } else {
        return;
    }

    if (EXTI_GetITStatus(line_bit) == RESET) {
        return;
    }

    EXTI_ClearITPendingBit(line_bit);

    if (!fuel_key_is_down(k) || k->last_down) {
        return;
    }

    now_ms = Delay_GetTick();
    fuel_key_on_fall(k, now_ms);
}

error_code_t FuelKey_Init(void)
{
    error_code_t ret;
    GPIO_Mode_t mode;

#if BTN_ACTIVE_LOW
    mode = GPIO_MODE_INPUT_PULLUP;
#else
    mode = GPIO_MODE_INPUT_PULLDOWN;
#endif

    ret = GPIO_Config(BTN_GREEN_PORT, BTN_GREEN_PIN, mode, GPIO_SPEED_2MHz);
    if (ret != ERROR_OK) {
        return ret;
    }

    ret = GPIO_Config(BTN_RED_PORT, BTN_RED_PIN, mode, GPIO_SPEED_2MHz);
    if (ret != ERROR_OK) {
        return ret;
    }

    s_green.port = BTN_GREEN_PORT;
    s_green.pin = BTN_GREEN_PIN;
    s_red.port = BTN_RED_PORT;
    s_red.pin = BTN_RED_PIN;

    s_green.last_down = 0u;
    s_red.last_down = 0u;
    s_green.long_fired = 0u;
    s_red.long_fired = 0u;
    s_green.press_start_ms = 0u;
    s_red.press_start_ms = 0u;
    s_evt_q_head = 0u;
    s_evt_q_tail = 0u;
    s_inited = 0u;

#if FUEL_KEY_USE_EXTI
    (void)fuel_key_init_exti();
    LOG_INFO("KEY", "init PA4/PA5 EXTI+poll long@%ums", (unsigned int)FUEL_KEY_LONG_PRESS_MS);
#else
    LOG_INFO("KEY", "init PA4/PA5 poll-only long@%ums", (unsigned int)FUEL_KEY_LONG_PRESS_MS);
#endif

    s_inited = 1u;
    return FUEL_KEY_OK;
}

static void fuel_key_poll_slot(fuel_key_slot_t *k, uint8_t is_green, uint32_t now_ms)
{
    uint8_t down;

    down = fuel_key_is_down(k);

    if (down && !k->last_down) {
        fuel_key_on_fall(k, now_ms);
    } else if (!down && k->last_down) {
        fuel_key_on_release(k, is_green, now_ms);
    } else if (down) {
        fuel_key_check_long_hold(k, is_green, now_ms);
    }
}

error_code_t FuelKey_Poll(uint32_t now_ms)
{
    if (!s_inited) {
        return FUEL_KEY_ERROR_NOT_INIT;
    }

    if (now_ms == 0u) {
        now_ms = Delay_GetTick();
    }

    fuel_key_poll_slot(&s_green, 1u, now_ms);
    fuel_key_poll_slot(&s_red, 0u, now_ms);
    return FUEL_KEY_OK;
}

fuel_key_event_t FuelKey_ConsumeEvent(void)
{
    fuel_key_event_t evt;

    if (s_evt_q_head == s_evt_q_tail) {
        return FUEL_KEY_EVT_NONE;
    }

    evt = s_evt_q[s_evt_q_head];
    s_evt_q_head = (uint8_t)((s_evt_q_head + 1u) % FUEL_KEY_EVT_Q_SIZE);
    return evt;
}

uint8_t FuelKey_IsGreenDown(void)
{
    return fuel_key_is_down(&s_green);
}

uint8_t FuelKey_IsRedDown(void)
{
    return fuel_key_is_down(&s_red);
}
