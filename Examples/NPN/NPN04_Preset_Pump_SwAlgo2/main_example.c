/**
 * @file main_example.c
 * @brief NPN04 - 预设加油泵（OGM SwAlgo2 下降沿交替 + GD200A ModBus 控泵）
 * @example Examples/NPN/NPN04_Preset_Pump_SwAlgo2/main_example.c
 * @details PB6/PB7 EXTI 下降沿 + A/B 交替互锁（软件 EXTI，ISR 内状态机）
 *
 * 硬件：
 * - OGM A/B：PB6/PB7；按键 PA6=测试启动（5~50Hz 每档 1000cnt）
 * - OLED：PB8/PB9；LED：PB12；Debug UART1：PA9/PA10
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "exti.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "error_code.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include "modbus_rtu.h"
#include "led.h"
#include "gpio.h"
#include "board.h"
#include "TIM2_TimeBase.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ==================== 应用参数 ==================== */

#define PULSES_PER_LITER            1000u
#define OGM_PULSES_PER_REV          4u      /**< A/B 下降沿交替，约 NPN03 一半（8=四边沿） */
/* OGM_EXTI_LINE_A/B 见 board.h */
#define OGM_MIN_PULSE_INTERVAL_US   800u    /**< 有效脉冲最小间隔，滤毛刺 */
#define OGM_DEBUG_LOG_1S            0u
#define OGM_RATE_WINDOW_MS          1000u
#define OGM_FLOW_IDLE_MS            2000u   /**< 此时间内无脉冲视为空闲（LED/OLED 刷新用） */
#define PUMP_FREQ_MIN_HZ            0u
#define PUMP_FREQ_MAX_HZ            50u
#define PUMP_FREQ_STEP_HZ           5u
#define PUMP_FREQ_DEFAULT_HZ        5u

/** 分档自动测试：PA6 每按一次启动一档，达标自动停机，下一档须再按键 */
#define TEST_FREQ_START_HZ          5u
#define TEST_FREQ_STEP_HZ           5u
#define TEST_FREQ_MAX_HZ            50u
#define TEST_CNT_TARGET             1000u
#define TEST_FREQ_TAIL_HZ           5u

#define INVT_SLAVE_ADDRESS          1
#define INVT_REG_RUN_CMD            0x2000
#define INVT_REG_FREQ_SET           0x2001
#define INVT_REG_STATUS_WORD1       0x2100
#define INVT_CMD_REVERSE            0x0002
#define INVT_CMD_STOP               0x0005
#define INVT_CMD_RUN                INVT_CMD_REVERSE
#define INVT_STATUS_RUN_FORWARD     0x0001
#define INVT_STATUS_RUN_REVERSE     0x0002

#define INVT_WRITE_GAP_MS           80
#define INVT_MODBUS_TIMEOUT_MS      1000
#define INVT_WRITE_RETRY_COUNT      3u
#define INVT_WRITE_RETRY_DELAY_MS   120u

#define BTN_SCAN_MS                 10
#define BTN_DEBOUNCE_MS             80u     /**< 两次有效按键最小间隔；485 忙时由 g_modbus_busy 保护 */
#define OLED_REFRESH_RUN_MS         200
#define LED_BLINK_MS                400

/** 按键：按下沿触发；忙时挂起，空闲后补执行 */
typedef struct {
    uint8_t last_pressed;
} BtnKey_t;

static BtnKey_t g_btn_up;
static BtnKey_t g_btn_dn;
static BtnKey_t g_btn_run;

static uint32_t g_btn_lockout_start_ms = 0;

static uint8_t g_btn_pending_up = 0;
static uint8_t g_btn_pending_dn = 0;
static uint8_t g_btn_pending_run = 0;

static uint8_t g_modbus_busy = 0;
static uint8_t g_pump_cmd_busy = 0;     /**< 启停序列进行中，暂缓再次启停 */

/* ==================== OGM EXTI 下降沿 + A/B 交替互锁 ==================== */

static volatile uint32_t g_count = 0;
static volatile uint8_t  g_arm_a = 1u;
static volatile uint8_t  g_arm_b = 0u;
static volatile uint8_t  g_last_a = 1u;
static volatile uint8_t  g_last_b = 1u;
static volatile uint32_t g_last_valid_us = 0u;
static volatile uint32_t g_count_snap = 0;
static volatile uint32_t g_count_delta_1s = 0;
static uint32_t          g_display_delta_1s = 0;
static volatile uint32_t g_last_pulse_ms = 0;

static uint8_t  g_set_freq_hz = PUMP_FREQ_DEFAULT_HZ;
static uint8_t  g_test_next_freq_hz = TEST_FREQ_START_HZ;
static uint8_t  g_test_run_freq_hz = TEST_FREQ_START_HZ;
static uint8_t  g_test_tail_applied = 0u;
static volatile uint8_t  g_pump_running = 0;

static uint8_t  g_comm_ok = 0;
static uint8_t  g_oled_dirty = 1;

static uint32_t g_last_led_tick = 0;
static uint32_t g_last_oled_tick = 0;
static uint8_t  g_led_on = 0;

static void Pump_FreqUp(void);
static void Pump_FreqDown(void);
static void Pump_ScanButtons(void);
static uint8_t Pump_ApplyFrequency(void);
static void Pump_TestStartOnPress(void);
static void Pump_CheckTestAutoStop(void);
static uint8_t Pump_Start(void);
static uint8_t Pump_Stop(void);

static ModBusRTU_Status_t Pump_CommPreflight(void);
static uint8_t Pump_EnsureInverterStopped(void);

static void OGM_ProcessChannel(uint8_t is_channel_a);
static uint8_t OGM_InitExti(void);
static void OGM_UpdateCountDelta(void);
static void OGM_Tick1s(void);
static uint8_t OGM_IsFlowActive(void);
static uint32_t OGM_GetCount(void);
static void OGM_ResetCount(void);

/* ==================== ModBus 工具 ==================== */

static const char *GetModBusErrorString(ModBusRTU_Status_t status)
{
    switch (status) {
        case ModBusRTU_OK: return "OK";
        case ModBusRTU_ERROR_TIMEOUT: return "Timeout";
        case ModBusRTU_ERROR_CRC: return "CRC";
        case ModBusRTU_ERROR_INVALID_RESPONSE: return "BadRsp";
        case ModBusRTU_ERROR_EXCEPTION: return "Exception";
        default: return "Error";
    }
}

static ModBusRTU_Status_t WriteRegOnce(uint16_t reg_addr, uint16_t value, uint32_t timeout_ms)
{
    return ModBusRTU_WriteSingleRegister(UART_INSTANCE_2, INVT_SLAVE_ADDRESS,
                                         reg_addr, value, timeout_ms);
}

/** ModBus 等待间隙内仅扫描按键（400ms 锁定期内不会重复发 485） */
static void Pump_DelayWithButtons(uint32_t ms)
{
    uint32_t start;

    start = Delay_GetTick();
    while (Delay_GetElapsed(Delay_GetTick(), start) < ms) {
        Pump_ScanButtons();
        Delay_ms(BTN_SCAN_MS);
    }
}

/** 写 0x2001 设频；leading_gap=1 时先等 80ms（启停序列），按键调频=0（同 Bus04） */
static ModBusRTU_Status_t InvtWriteFrequencyHzRetry(uint8_t freq_hz, uint8_t leading_gap)
{
    uint8_t retry;
    ModBusRTU_Status_t status;
    uint16_t raw;

    raw = (uint16_t)freq_hz * 100U;

    if (leading_gap) {
        Pump_DelayWithButtons(INVT_WRITE_GAP_MS);
    }

    status = ModBusRTU_ERROR_TIMEOUT;
    for (retry = 0; retry < INVT_WRITE_RETRY_COUNT; retry++) {
        g_modbus_busy = 1;
        status = WriteRegOnce(INVT_REG_FREQ_SET, raw, INVT_MODBUS_TIMEOUT_MS);
        g_modbus_busy = 0;
        if (status == ModBusRTU_OK) {
            break;
        }
        Pump_DelayWithButtons(INVT_WRITE_RETRY_DELAY_MS);
    }
    return status;
}

static ModBusRTU_Status_t InvtSendRunCmd(uint16_t cmd)
{
    uint8_t retry;
    ModBusRTU_Status_t status;

    /* 启动前需间隔；停机立即写，避免用户感觉停不了 */
    if (cmd != INVT_CMD_STOP) {
        Pump_DelayWithButtons(INVT_WRITE_GAP_MS);
    }

    for (retry = 0; retry < INVT_WRITE_RETRY_COUNT; retry++) {
        g_modbus_busy = 1;
        status = WriteRegOnce(INVT_REG_RUN_CMD, cmd, INVT_MODBUS_TIMEOUT_MS);
        g_modbus_busy = 0;
        if (status == ModBusRTU_OK) {
            return ModBusRTU_OK;
        }
        if (cmd != INVT_CMD_STOP) {
            Pump_DelayWithButtons(INVT_WRITE_RETRY_DELAY_MS);
        } else {
            Delay_ms(INVT_WRITE_RETRY_DELAY_MS);
        }
    }
    return status;
}

/** 上电 485 预检：单次短超时，不阻塞按键进入主循环 */
static ModBusRTU_Status_t Pump_CommPreflight(void)
{
    uint16_t word;

    return ModBusRTU_ReadHoldingRegisters(UART_INSTANCE_2, INVT_SLAVE_ADDRESS,
                                          INVT_REG_FREQ_SET, 1, &word, 300);
}

static uint8_t Pump_EnsureInverterStopped(void)
{
    uint16_t status_word1;
    ModBusRTU_Status_t status;

    status = ModBusRTU_ReadHoldingRegisters(UART_INSTANCE_2, INVT_SLAVE_ADDRESS,
                                            INVT_REG_STATUS_WORD1, 1, &status_word1,
                                            INVT_MODBUS_TIMEOUT_MS);
    if (status != ModBusRTU_OK) {
        LOG_WARN("PUMP", "读 2100H 失败: %s", GetModBusErrorString(status));
        return 0u;
    }

    if (status_word1 == INVT_STATUS_RUN_FORWARD ||
        status_word1 == INVT_STATUS_RUN_REVERSE) {
        LOG_WARN("PUMP", "检测到变频器运行中(2100H=0x%04X)，执行停机",
                 (unsigned int)status_word1);
        if (!Pump_Stop()) {
            LOG_ERROR("PUMP", "初始化强制停机失败");
            return 0u;
        }
        LOG_INFO("PUMP", "初始化已强制停机");
        return 1u;
    }

    LOG_INFO("PUMP", "变频器未运行(2100H=0x%04X)", (unsigned int)status_word1);
    return 1u;
}

static uint8_t Pump_ApplyFrequency(void)
{
    ModBusRTU_Status_t status;

    status = InvtWriteFrequencyHzRetry(g_set_freq_hz, 0u);

    if (status != ModBusRTU_OK) {
        LOG_ERROR("PUMP", "设频 %uHz 失败: %s", (unsigned int)g_set_freq_hz,
                  GetModBusErrorString(status));
        ErrorHandler_Handle(status, "INVT_FREQ");
        g_comm_ok = 0;
        g_oled_dirty = 1;
        return 0;
    }

    g_comm_ok = 1;
    g_oled_dirty = 1;
    LOG_INFO("PUMP", "485 写设频 %u Hz OK", (unsigned int)g_set_freq_hz);
    return 1;
}

static uint8_t Pump_Start(void)
{
    ModBusRTU_Status_t status;

    if (g_set_freq_hz == 0) {
        LOG_WARN("PUMP", "频率为 0，无法启动，请先升频");
        return 0;
    }

    g_pump_cmd_busy = 1;

    status = InvtWriteFrequencyHzRetry(g_set_freq_hz, 1u);
    if (status != ModBusRTU_OK) {
        LOG_ERROR("PUMP", "启动设频失败: %s", GetModBusErrorString(status));
        ErrorHandler_Handle(status, "INVT_START");
        g_comm_ok = 0;
        g_pump_cmd_busy = 0;
        return 0;
    }

    status = InvtSendRunCmd(INVT_CMD_RUN);
    if (status != ModBusRTU_OK) {
        LOG_ERROR("PUMP", "启动命令失败: %s", GetModBusErrorString(status));
        ErrorHandler_Handle(status, "INVT_START");
        g_comm_ok = 0;
        g_pump_cmd_busy = 0;
        return 0;
    }

    OGM_ResetCount();
    g_pump_running = 1;
    g_comm_ok = 1;
    g_pump_cmd_busy = 0;
    LOG_INFO("PUMP", "泵启动（反转），频率 %u Hz，Cnt 清零", (unsigned int)g_set_freq_hz);
    return 1;
}

static uint8_t Pump_Stop(void)
{
    ModBusRTU_Status_t status;

    g_pump_cmd_busy = 1;

    status = InvtSendRunCmd(INVT_CMD_STOP);
    if (status != ModBusRTU_OK) {
        LOG_ERROR("PUMP", "停机失败: %s", GetModBusErrorString(status));
        ErrorHandler_Handle(status, "INVT_STOP");
        g_comm_ok = 0;
        g_pump_cmd_busy = 0;
        return 0;
    }

    g_pump_running = 0;
    g_pump_cmd_busy = 0;
    g_oled_dirty = 1;
    LOG_INFO("PUMP", "泵停止");
    return 1;
}

/* ==================== OGM：EXTI 双边沿 + 仅下降沿交替互锁（同 NPN03 读电平） ==================== */

static uint32_t OGM_GetTimeUs(void)
{
    return (g_task_tick * 1000u) + (uint32_t)TIM_GetCounter(TIM2);
}

static uint8_t OGM_AcceptInterval(void)
{
    uint32_t now;
    uint32_t elapsed;

    now = OGM_GetTimeUs();
    if (g_last_valid_us == 0u) {
        g_last_valid_us = now;
        return 1u;
    }
    elapsed = now - g_last_valid_us;
    if (elapsed >= OGM_MIN_PULSE_INTERVAL_US) {
        g_last_valid_us = now;
        return 1u;
    }
    return 0u;
}

/**
 * @brief 读 GPIO 判真实边沿，仅下降沿参与 A/B 交替计数
 * @details 同 NPN03 ProcessChannel；上升沿只更新 last，不计数
 */
static void OGM_ProcessChannel(uint8_t is_channel_a)
{
    uint8_t level_new;
    uint8_t level_last;

    if (is_channel_a) {
        level_new = GPIO_ReadPin(OGM_CH_A_PORT, OGM_CH_A_PIN) ? 1u : 0u;
        level_last = g_last_a;
    } else {
        level_new = GPIO_ReadPin(OGM_CH_B_PORT, OGM_CH_B_PIN) ? 1u : 0u;
        level_last = g_last_b;
    }

    if (level_new == level_last) {
        return;
    }

    if (is_channel_a) {
        g_last_a = level_new;
    } else {
        g_last_b = level_new;
    }

    /* 仅下降沿（1→0） */
    if (level_new != 0u) {
        return;
    }

    if (is_channel_a) {
        if (g_arm_a == 0u) {
            return;
        }
    } else {
        if (g_arm_b == 0u) {
            return;
        }
    }

    if (!OGM_AcceptInterval()) {
        return;
    }

    g_count++;
    g_last_pulse_ms = g_task_tick;
    if (is_channel_a) {
        g_arm_a = 0u;
        g_arm_b = 1u;
    } else {
        g_arm_b = 0u;
        g_arm_a = 1u;
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
    if (EXTI_HW_Init(line, EXTI_TRIGGER_RISING_FALLING, EXTI_MODE_INTERRUPT) != EXTI_OK) {
        return 0u;
    }
    if (GPIO_Config(port, pin, GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_50MHz) != GPIO_OK) {
        return 0u;
    }
    if (EXTI_SetCallback(line, callback, NULL) != EXTI_OK) {
        return 0u;
    }
    if (EXTI_Enable(line) != EXTI_OK) {
        return 0u;
    }
    EXTI_ClearPending(line);
    return 1u;
}

static void OGM_UpdateCountDelta(void)
{
    uint32_t now;
    uint32_t delta;

    __disable_irq();
    now = g_count;
    delta = now - g_count_snap;
    g_count_snap = now;
    g_count_delta_1s += delta;
    __enable_irq();
}

static uint32_t OGM_GetCount(void)
{
    uint32_t snap;

    __disable_irq();
    snap = g_count;
    __enable_irq();
    return snap;
}

static void OGM_Tick1s(void)
{
    uint32_t delta_snap;

    __disable_irq();
    delta_snap = g_count_delta_1s;
    g_count_delta_1s = 0u;
    __enable_irq();
    g_display_delta_1s = delta_snap;
}

static uint8_t OGM_IsFlowActive(void)
{
    if (g_last_pulse_ms == 0u) {
        return 0u;
    }
    return (Delay_GetElapsed(Delay_GetTick(), g_last_pulse_ms) < OGM_FLOW_IDLE_MS) ? 1u : 0u;
}

static void OGM_ResetCount(void)
{
    __disable_irq();
    g_count = 0u;
    g_count_snap = 0u;
    g_count_delta_1s = 0u;
    g_arm_a = 1u;
    g_arm_b = 0u;
    g_last_a = GPIO_ReadPin(OGM_CH_A_PORT, OGM_CH_A_PIN) ? 1u : 0u;
    g_last_b = GPIO_ReadPin(OGM_CH_B_PORT, OGM_CH_B_PIN) ? 1u : 0u;
    g_last_valid_us = 0u;
    __enable_irq();
    g_display_delta_1s = 0u;
    g_last_pulse_ms = 0u;
}

static uint8_t OGM_InitExti(void)
{
    g_arm_a = 1u;
    g_arm_b = 0u;
    g_last_a = GPIO_ReadPin(OGM_CH_A_PORT, OGM_CH_A_PIN) ? 1u : 0u;
    g_last_b = GPIO_ReadPin(OGM_CH_B_PORT, OGM_CH_B_PIN) ? 1u : 0u;
    g_last_valid_us = 0u;
    g_count = 0u;
    g_count_snap = 0u;
    g_count_delta_1s = 0u;

    if (!OGM_InitExtiChannel(OGM_EXTI_LINE_A, OGM_ChannelA_Callback,
                             OGM_CH_A_PORT, OGM_CH_A_PIN)) {
        return 0u;
    }
    if (!OGM_InitExtiChannel(OGM_EXTI_LINE_B, OGM_ChannelB_Callback,
                             OGM_CH_B_PORT, OGM_CH_B_PIN)) {
        return 0u;
    }

    LOG_INFO("OGM", "EXTI fall alt-lock + lvl chk, min=%uus",
             (unsigned int)OGM_MIN_PULSE_INTERVAL_US);
    return 1u;
}

static void OGM_ErrorBlink(uint32_t on_ms, uint32_t off_ms)
{
    while (1) {
        LED1_On();
        Delay_ms(on_ms);
        LED1_Off();
        Delay_ms(off_ms);
    }
}

/* ==================== 按键：按下沿写 485；忙时挂起，空闲后补执行 ==================== */

static uint8_t Pump_ButtonPressed(GPIO_TypeDef *port, uint16_t pin)
{
    return (GPIO_ReadPin(port, pin) == Bit_RESET) ? 1u : 0u;
}

static uint8_t Pump_ButtonLockoutActive(uint32_t now)
{
    if (g_btn_lockout_start_ms == 0u) {
        return 0u;
    }
    return (Delay_GetElapsed(now, g_btn_lockout_start_ms) < BTN_DEBOUNCE_MS) ? 1u : 0u;
}

static void Pump_ButtonLockoutBegin(uint32_t now)
{
    g_btn_lockout_start_ms = now;
}

/** 返回 1=本次出现按下沿（由低跳变） */
static uint8_t Pump_ButtonPressEdge(BtnKey_t *key, uint8_t pressed)
{
    uint8_t edge;

    edge = (pressed && !key->last_pressed) ? 1u : 0u;
    key->last_pressed = pressed;
    return edge;
}

static void Pump_InitButtons(void)
{
    (void)GPIO_Config(BTN_FREQ_UP_PORT, BTN_FREQ_UP_PIN,
                      GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_2MHz);
    (void)GPIO_Config(BTN_FREQ_DOWN_PORT, BTN_FREQ_DOWN_PIN,
                      GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_2MHz);
    (void)GPIO_Config(BTN_RUN_STOP_PORT, BTN_RUN_STOP_PIN,
                      GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_2MHz);

    g_btn_up.last_pressed = Pump_ButtonPressed(BTN_FREQ_UP_PORT, BTN_FREQ_UP_PIN);
    g_btn_dn.last_pressed = Pump_ButtonPressed(BTN_FREQ_DOWN_PORT, BTN_FREQ_DOWN_PIN);
    g_btn_run.last_pressed = Pump_ButtonPressed(BTN_RUN_STOP_PORT, BTN_RUN_STOP_PIN);
    g_btn_lockout_start_ms = 0u;
    g_btn_pending_up = 0u;
    g_btn_pending_dn = 0u;
    g_btn_pending_run = 0u;
}

static uint8_t Pump_ButtonCanAct(uint32_t now)
{
    if (g_modbus_busy || g_pump_cmd_busy) {
        return 0u;
    }
    if (Pump_ButtonLockoutActive(now)) {
        return 0u;
    }
    return 1u;
}

/** 485/启停序列忙时：记下按下意图，不丢弃边沿 */
static void Pump_ButtonLatchPending(uint8_t up, uint8_t dn, uint8_t run)
{
    if (up && !g_btn_up.last_pressed) {
        g_btn_pending_up = 1u;
    }
    if (dn && !g_btn_dn.last_pressed) {
        g_btn_pending_dn = 1u;
    }
    if (run && !g_btn_run.last_pressed) {
        g_btn_pending_run = 1u;
    }
    g_btn_up.last_pressed = up;
    g_btn_dn.last_pressed = dn;
    g_btn_run.last_pressed = run;
}

static void Pump_ButtonSyncState(uint8_t up, uint8_t dn, uint8_t run)
{
    g_btn_up.last_pressed = up;
    g_btn_dn.last_pressed = dn;
    g_btn_run.last_pressed = run;
}

static void Pump_ScanButtons(void)
{
    uint32_t now;
    uint8_t up;
    uint8_t dn;
    uint8_t run;
    uint8_t edge_up;
    uint8_t edge_dn;
    uint8_t edge_run;

    now = Delay_GetTick();
    up = Pump_ButtonPressed(BTN_FREQ_UP_PORT, BTN_FREQ_UP_PIN);
    dn = Pump_ButtonPressed(BTN_FREQ_DOWN_PORT, BTN_FREQ_DOWN_PIN);
    run = Pump_ButtonPressed(BTN_RUN_STOP_PORT, BTN_RUN_STOP_PIN);

    if (!Pump_ButtonCanAct(now)) {
        Pump_ButtonLatchPending(up, dn, run);
        return;
    }

    if (g_btn_pending_up) {
        g_btn_pending_up = 0u;
        Pump_ButtonSyncState(up, dn, run);
        Pump_ButtonLockoutBegin(now);
        Pump_FreqUp();
        return;
    }
    if (g_btn_pending_dn) {
        g_btn_pending_dn = 0u;
        Pump_ButtonSyncState(up, dn, run);
        Pump_ButtonLockoutBegin(now);
        Pump_FreqDown();
        return;
    }
    if (g_btn_pending_run) {
        g_btn_pending_run = 0u;
        Pump_ButtonSyncState(up, dn, run);
        Pump_ButtonLockoutBegin(now);
        Pump_TestStartOnPress();
        return;
    }

    edge_up = Pump_ButtonPressEdge(&g_btn_up, up);
    edge_dn = Pump_ButtonPressEdge(&g_btn_dn, dn);
    edge_run = Pump_ButtonPressEdge(&g_btn_run, run);

    if (edge_up) {
        Pump_ButtonLockoutBegin(now);
        Pump_FreqUp();
        return;
    }

    if (edge_dn) {
        Pump_ButtonLockoutBegin(now);
        Pump_FreqDown();
        return;
    }

    if (edge_run) {
        Pump_ButtonLockoutBegin(now);
        Pump_TestStartOnPress();
    }
}

static void Pump_TestStartOnPress(void)
{
    if (g_pump_running) {
        return;
    }
    if (!g_comm_ok) {
        LOG_WARN("TEST", "485 未就绪，无法启动测试");
        return;
    }

    if (g_test_next_freq_hz < TEST_FREQ_START_HZ ||
        g_test_next_freq_hz > TEST_FREQ_MAX_HZ) {
        g_test_next_freq_hz = TEST_FREQ_START_HZ;
    }

    g_set_freq_hz = g_test_next_freq_hz;
    g_test_run_freq_hz = g_test_next_freq_hz;
    g_test_tail_applied = 0u;
    g_oled_dirty = 1;

    if (!Pump_Start()) {
        LOG_ERROR("TEST", "启动失败 freq=%u Hz", (unsigned int)g_set_freq_hz);
        return;
    }

    LOG_INFO("TEST", "档位启动 %u Hz，目标 %u cnt", (unsigned int)g_set_freq_hz,
             (unsigned int)TEST_CNT_TARGET);
}

static uint32_t Pump_TestGetTailPulseCount(uint8_t test_freq_hz)
{
    if (test_freq_hz <= TEST_FREQ_TAIL_HZ || test_freq_hz < 10u) {
        return 0u;
    }
    return ((uint32_t)test_freq_hz - 10u) / TEST_FREQ_STEP_HZ * 20u + 20u;
}

static uint32_t Pump_TestGetTailSwitchCount(uint8_t test_freq_hz)
{
    uint32_t tail;

    tail = Pump_TestGetTailPulseCount(test_freq_hz);
    if (tail == 0u || tail >= TEST_CNT_TARGET) {
        return TEST_CNT_TARGET;
    }
    return TEST_CNT_TARGET - tail;
}

static void Pump_CheckTestAutoStop(void)
{
    uint32_t cnt;
    uint32_t switch_at;

    if (!g_pump_running) {
        return;
    }

    cnt = OGM_GetCount();

    if (!g_test_tail_applied) {
        switch_at = Pump_TestGetTailSwitchCount(g_test_run_freq_hz);
        if (switch_at < TEST_CNT_TARGET && cnt >= switch_at) {
            g_set_freq_hz = TEST_FREQ_TAIL_HZ;
            if (Pump_ApplyFrequency()) {
                g_test_tail_applied = 1u;
                g_oled_dirty = 1;
                LOG_INFO("TEST", "Cnt>=%lu 尾段改 %u Hz (本档 %u Hz)",
                         (unsigned long)switch_at,
                         (unsigned int)TEST_FREQ_TAIL_HZ,
                         (unsigned int)g_test_run_freq_hz);
            }
        }
    }

    if (cnt < TEST_CNT_TARGET) {
        return;
    }

    (void)Pump_Stop();

    g_test_tail_applied = 0u;
    g_test_next_freq_hz = (uint8_t)(g_test_next_freq_hz + TEST_FREQ_STEP_HZ);
    if (g_test_next_freq_hz > TEST_FREQ_MAX_HZ) {
        g_test_next_freq_hz = TEST_FREQ_START_HZ;
    }

    g_oled_dirty = 1;
    LOG_INFO("TEST", "已达 %u cnt 停机，下次按键启动 %u Hz",
             (unsigned int)TEST_CNT_TARGET, (unsigned int)g_test_next_freq_hz);
}

static void Pump_FreqUp(void)
{
    (void)0;
}

static void Pump_FreqDown(void)
{
    (void)0;
}

static void Pump_PollButtons(void)
{
    Pump_ScanButtons();
}

/* ==================== OLED 显示（脉冲为主，与 NPN02 一致） ==================== */

static void Pump_ShowPulseCount(uint32_t cnt)
{
    OLED_ShowString(1, 1, "Cnt:");
    OLED_ShowNum(1, 5, cnt, 8);
}

static void Pump_ShowPulseRate(uint32_t delta_1s)
{
    OLED_ShowString(3, 1, "d/1s:");
    OLED_ShowNum(3, 6, delta_1s, 6);
}

static void Pump_ShowCommLine(uint8_t comm_ok)
{
    if (comm_ok) {
        OLED_ShowString(4, 1, "485:OK          ");
    } else {
        OLED_ShowString(4, 1, "485:ERR         ");
    }
}

static void Pump_RefreshOLED(uint32_t delta_1s)
{
    char buffer[20];
    uint32_t cnt;

    cnt = OGM_GetCount();
    Pump_ShowPulseCount(cnt);

    if (g_pump_running) {
        if (g_test_tail_applied) {
            snprintf(buffer, sizeof(buffer), "F:%02u>%02u T:%04u",
                     (unsigned int)g_test_run_freq_hz,
                     (unsigned int)TEST_FREQ_TAIL_HZ,
                     (unsigned int)TEST_CNT_TARGET);
        } else {
            snprintf(buffer, sizeof(buffer), "F:%02u T:%04u    ",
                     (unsigned int)g_test_run_freq_hz,
                     (unsigned int)TEST_CNT_TARGET);
        }
    } else {
        snprintf(buffer, sizeof(buffer), "Nx:%02uHz idle    ",
                 (unsigned int)g_test_next_freq_hz);
    }
    OLED_ShowString(2, 1, buffer);

    Pump_ShowPulseRate(delta_1s);
    Pump_ShowCommLine(g_comm_ok);
}

static void Pump_UpdateLed(void)
{
    uint32_t now;

    now = Delay_GetTick();

    if (!g_comm_ok) {
        if (Delay_GetElapsed(now, g_last_led_tick) >= 100u) {
            g_last_led_tick = now;
            g_led_on = (uint8_t)!g_led_on;
            if (g_led_on) {
                LED1_On();
            } else {
                LED1_Off();
            }
        }
        return;
    }

    if (OGM_IsFlowActive()) {
        if (Delay_GetElapsed(now, g_last_led_tick) >= LED_BLINK_MS) {
            g_last_led_tick = now;
            g_led_on = (uint8_t)!g_led_on;
            if (g_led_on) {
                LED1_On();
            } else {
                LED1_Off();
            }
        }
    } else {
        LED1_Off();
        g_led_on = 0;
    }
}

/* ==================== 初始化 ==================== */

static void Pump_InitComm(void)
{
    UART_Status_t uart_status;
    int debug_status;
    Log_Status_t log_status;
    log_config_t log_config;
    ModBusRTU_Status_t status;

    uart_status = UART_Init(UART_INSTANCE_1);
    if (uart_status != UART_OK) {
        while (1) { Delay_ms(1000); }
    }

    uart_status = UART_Init(UART_INSTANCE_2);
    if (uart_status != UART_OK) {
        while (1) { Delay_ms(1000); }
    }

    debug_status = Debug_Init(DEBUG_MODE_UART, 115200);
    if (debug_status != 0) {
        while (1) { Delay_ms(1000); }
    }

    memset(&log_config, 0, sizeof(log_config));
    log_config.level = LOG_LEVEL_DEBUG;
    log_config.enable_timestamp = 0;
    log_config.enable_module = 1;
    log_config.enable_color = 0;

    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK) {
        ErrorHandler_Handle(log_status, "LOG");
    }

    LOG_INFO("MAIN", "=== NPN04 Preset Pump SwAlgo2 ===");
    LOG_INFO("MAIN", "UART2: 19200 8E1 Addr=%d", INVT_SLAVE_ADDRESS);
    LOG_INFO("MAIN", "按键：忙时挂起、空闲补执行；防抖 %ums", (unsigned int)BTN_DEBOUNCE_MS);
    LOG_INFO("MAIN", "请确认 GD200A P00.01=2 P00.02=0 P00.06=8 P00.09=0");

    LOG_INFO("MAIN", "TEST: PA6 start 5~50Hz step5, %u cnt/auto stop",
             (unsigned int)TEST_CNT_TARGET);

    status = Pump_CommPreflight();
    if (status == ModBusRTU_OK) {
        g_comm_ok = 1;
        LOG_INFO("MAIN", "485 通讯预检通过");
        (void)Pump_EnsureInverterStopped();
    } else {
        g_comm_ok = 0;
        LOG_WARN("MAIN", "485 预检失败: %s，按键仍可用", GetModBusErrorString(status));
    }
}

/* ==================== 主函数 ==================== */

static uint32_t g_last_ogm_task_tick = 0;
static uint32_t g_last_ogm_ui_tick = 0;
static uint32_t g_last_display_count = 0;

#define OLED_REFRESH_IDLE_MS        200u

int main(void)
{
    System_Init();

    if (OLED_Init() != OLED_OK) {
        OGM_ErrorBlink(80, 80);
    }

    Pump_InitComm();
    Pump_InitButtons();

    if (!OGM_InitExti()) {
        OGM_ErrorBlink(100, 100);
    }

    /* 上电将本地默认频率（25Hz）写入 0x2001，避免 OLED 显示与变频器不一致 */
    if (g_comm_ok) {
        Pump_DelayWithButtons(INVT_WRITE_GAP_MS);
        g_set_freq_hz = TEST_FREQ_START_HZ;
        g_test_next_freq_hz = TEST_FREQ_START_HZ;
        if (Pump_ApplyFrequency()) {
            LOG_INFO("MAIN", "上电设频 %u Hz（待机，按 PA6 开始首档测试）",
                     (unsigned int)g_set_freq_hz);
        } else {
            LOG_WARN("MAIN", "上电设频失败，请检查 485 后按 PA6 重试");
        }
    }

    OLED_Clear();
    g_oled_dirty = 1;

    g_last_led_tick = Delay_GetTick();
    g_last_ogm_task_tick = g_task_tick;
    g_last_ogm_ui_tick = g_task_tick;
    g_count_snap = g_count;
    g_last_display_count = OGM_GetCount();

    LOG_INFO("MAIN", "OGM EXTI6/7 fall A/B alt-lock PB6/PB7");
    LOG_INFO("MAIN", "PA6=测试启动；每档 %u cnt 自动停，下一档须再按 PA6",
             (unsigned int)TEST_CNT_TARGET);

    while (1) {
        while (g_last_ogm_task_tick != g_task_tick) {
            g_last_ogm_task_tick++;
            OGM_UpdateCountDelta();
        }

        if ((uint32_t)(g_task_tick - g_last_ogm_ui_tick) >= OGM_RATE_WINDOW_MS) {
            g_last_ogm_ui_tick = g_task_tick;
            OGM_Tick1s();
            g_oled_dirty = 1;
#if OGM_DEBUG_LOG_1S
            LOG_INFO("OGM", "Cnt=%u d/1s=%u",
                     (unsigned int)OGM_GetCount(),
                     (unsigned int)g_display_delta_1s);
#endif
        }

        Pump_PollButtons();
        Pump_CheckTestAutoStop();

        {
            uint32_t now_count;

            now_count = OGM_GetCount();
            if (now_count != g_last_display_count) {
                g_last_display_count = now_count;
                g_oled_dirty = 1;
            }
        }

        {
            uint32_t now = Delay_GetTick();
            uint32_t refresh_ms = OGM_IsFlowActive() ? OLED_REFRESH_RUN_MS : OLED_REFRESH_IDLE_MS;

            if (g_oled_dirty ||
                Delay_GetElapsed(now, g_last_oled_tick) >= refresh_ms) {
                Pump_RefreshOLED(g_display_delta_1s);
                g_oled_dirty = 0;
                g_last_oled_tick = now;
            }
        }

        Pump_UpdateLed();

        Delay_ms(BTN_SCAN_MS);
    }
}
