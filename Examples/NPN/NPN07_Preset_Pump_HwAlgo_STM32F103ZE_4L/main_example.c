/**
 * @file main_example.c
 * @brief NPN07 - 预设加油泵 4L（小精灵 F103ZE）
 * @example Examples/NPN/NPN07_Preset_Pump_HwAlgo_STM32F103ZE_4L/main_example.c
 * @details TIM3 双通道硬件捕获 + ISR 四边沿互锁；PA4 启动，加满 4L 自动停
 *
 * 板级（见 board.h）：OGM TIM3 PA6/PA7、启动键 PA4、硬 I2C2 OLED、LED PC4
 */

#include "stm32f10x.h"
#include "system_init.h"
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
#include "board_early_init.h"
#include "ogm_flow_ic.h"
#include "TIM2_TimeBase.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ==================== 应用参数（4L 预设：145 脉冲/升 × 4 = 580 cnt） ==================== */

#define PULSES_PER_LITER            145u
#define PRESET_LITERS               4u
#define PRESET_CNT_TARGET           (PULSES_PER_LITER * PRESET_LITERS)
#define OGM_PULSES_PER_REV          8u
#define OGM_RATE_WINDOW_MS          1000u
#define OGM_FLOW_IDLE_MS            2000u
#define PUMP_FREQ_DEFAULT_HZ        5u

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
#define BTN_DEBOUNCE_MS             80u
#define OLED_REFRESH_RUN_MS         200
#define LED_BLINK_MS                400
#define OLED_REFRESH_IDLE_MS        200u

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
static uint8_t g_pump_cmd_busy = 0;

static volatile uint32_t g_count_snap = 0;
static volatile uint32_t g_count_delta_1s = 0;
static uint32_t          g_display_delta_1s = 0;

static uint8_t  g_set_freq_hz = PUMP_FREQ_DEFAULT_HZ;
static volatile uint8_t  g_pump_running = 0;
static uint8_t  g_comm_ok = 0;
static uint8_t  g_oled_dirty = 1;
static uint32_t g_last_led_tick = 0;
static uint32_t g_last_oled_tick = 0;
static uint8_t  g_led_on = 0;
static uint32_t g_last_ogm_task_tick = 0;
static uint32_t g_last_ogm_ui_tick = 0;
static uint32_t g_last_display_count = 0;
static uint32_t g_run_start_ms = 0u;
static uint32_t g_run_elapsed_sec = 0u;

static void Pump_FreqUp(void);
static void Pump_FreqDown(void);
static void Pump_ScanButtons(void);
static uint8_t Pump_ApplyFrequency(void);
static void Pump_PresetStartOnPress(void);
static void Pump_CheckPresetAutoStop(void);
static uint8_t Pump_Start(void);
static uint8_t Pump_Stop(void);
static ModBusRTU_Status_t Pump_CommPreflight(void);
static uint8_t Pump_EnsureInverterStopped(void);
static void OGM_UpdateCountDelta(void);
static void OGM_Tick1s(uint32_t window_ms);
static uint8_t OGM_IsFlowActive(void);
static uint32_t OGM_GetCount(void);
static void OGM_ResetCount(void);
static void OGM_ErrorBlink(uint32_t on_ms, uint32_t off_ms);

/* ==================== ModBus ==================== */

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

static void Pump_DelayWithButtons(uint32_t ms)
{
    uint32_t start;

    start = Delay_GetTick();
    while (Delay_GetElapsed(Delay_GetTick(), start) < ms) {
        Pump_ScanButtons();
        Delay_ms(BTN_SCAN_MS);
    }
}

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
    g_run_start_ms = Delay_GetTick();
    g_run_elapsed_sec = 0u;
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
    if (g_run_start_ms != 0u) {
        g_run_elapsed_sec = Delay_GetElapsed(Delay_GetTick(), g_run_start_ms) / 1000u;
    }
    g_pump_cmd_busy = 0;
    g_oled_dirty = 1;
    LOG_INFO("PUMP", "泵停止");
    return 1;
}

/* ==================== OGM 硬件输入捕获 ==================== */

static void OGM_UpdateCountDelta(void)
{
    uint32_t now;
    uint32_t delta;
    error_code_t ret;

    ret = OGM_FlowIC_GetCount(&now);
    if (ret != OGM_FLOW_IC_OK) {
        return;
    }

    delta = now - g_count_snap;
    g_count_snap = now;
    g_count_delta_1s += delta;
}

static uint32_t OGM_GetCount(void)
{
    uint32_t snap;
    error_code_t ret;

    ret = OGM_FlowIC_GetCount(&snap);
    if (ret != OGM_FLOW_IC_OK) {
        return 0u;
    }
    return snap;
}

static void OGM_Tick1s(uint32_t window_ms)
{
    uint32_t delta_snap;

    if (window_ms == 0u) {
        return;
    }
    delta_snap = g_count_delta_1s;
    g_count_delta_1s = 0u;
    g_display_delta_1s = (delta_snap * 1000u) / window_ms;
}

static uint8_t OGM_IsFlowActive(void)
{
    uint8_t active;
    error_code_t ret;

    ret = OGM_FlowIC_IsFlowActive(OGM_FLOW_IDLE_MS, &active);
    if (ret != OGM_FLOW_IC_OK) {
        return 0u;
    }
    return active;
}

static void OGM_ResetCount(void)
{
    (void)OGM_FlowIC_ResetCount();
    g_count_snap = 0u;
    g_count_delta_1s = 0u;
    g_display_delta_1s = 0u;
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

/* ==================== 按键 ==================== */

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
        Pump_PresetStartOnPress();
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
        Pump_PresetStartOnPress();
    }
}

static void Pump_PresetStartOnPress(void)
{
    if (g_pump_running) {
        return;
    }
    if (!g_comm_ok) {
        LOG_WARN("PUMP", "485 未就绪，无法启动");
        return;
    }

    g_set_freq_hz = PUMP_FREQ_DEFAULT_HZ;
    g_oled_dirty = 1;

    if (!Pump_Start()) {
        LOG_ERROR("PUMP", "4L 启动失败 freq=%u Hz", (unsigned int)g_set_freq_hz);
        return;
    }

    LOG_INFO("PUMP", "4L 预设启动 %u Hz，目标 %u cnt (%u/L x %uL)",
             (unsigned int)g_set_freq_hz,
             (unsigned int)PRESET_CNT_TARGET,
             (unsigned int)PULSES_PER_LITER,
             (unsigned int)PRESET_LITERS);
}

static void Pump_CheckPresetAutoStop(void)
{
    uint32_t cnt;

    if (!g_pump_running) {
        return;
    }

    cnt = OGM_GetCount();
    if (cnt < PRESET_CNT_TARGET) {
        return;
    }

    (void)Pump_Stop();
    g_oled_dirty = 1;
    LOG_INFO("PUMP", "已达 %u cnt（4L），停机；再按 PA4 可重复",
             (unsigned int)PRESET_CNT_TARGET);
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

/* ==================== OLED ==================== */

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

static void Pump_ShowStatusLine(uint8_t comm_ok, uint32_t run_sec)
{
    char buffer[20];

    if (comm_ok) {
        snprintf(buffer, sizeof(buffer), "485:OK %4lus    ", (unsigned long)run_sec);
    } else {
        snprintf(buffer, sizeof(buffer), "485:ERR %4lus   ", (unsigned long)run_sec);
    }
    OLED_ShowString(4, 1, buffer);
}

static uint32_t Pump_GetRunElapsedSec(void)
{
    if (g_pump_running && g_run_start_ms != 0u) {
        return Delay_GetElapsed(Delay_GetTick(), g_run_start_ms) / 1000u;
    }
    return g_run_elapsed_sec;
}

static void Pump_RefreshOLED(uint32_t delta_1s)
{
    char buffer[20];
    uint32_t cnt;
    uint32_t run_sec;
    float flow_lpm = 0.0f;

    cnt = OGM_GetCount();
    Pump_ShowPulseCount(cnt);

    if (g_pump_running) {
        snprintf(buffer, sizeof(buffer), "4L F:%02u T:%04u",
                 (unsigned int)g_set_freq_hz,
                 (unsigned int)PRESET_CNT_TARGET);
    } else {
        snprintf(buffer, sizeof(buffer), "PA4:4L %3u/L idle",
                 (unsigned int)PULSES_PER_LITER);
    }
    OLED_ShowString(2, 1, buffer);

    Pump_ShowPulseRate(delta_1s);
    run_sec = Pump_GetRunElapsedSec();
    (void)OGM_FlowIC_GetInstantFlow(&flow_lpm);
    (void)flow_lpm;
    Pump_ShowStatusLine(g_comm_ok, run_sec);
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

    LOG_INFO("MAIN", "=== NPN07 Preset Pump 4L F103ZE ===");
    LOG_INFO("MAIN", "UART2: 19200 8E1 Addr=%d", INVT_SLAVE_ADDRESS);
    LOG_INFO("MAIN", "OGM: TIM3 IC 4-edge lock PA6/PA7");
    LOG_INFO("MAIN", "PA4 start: %uL x %u/L = %u cnt",
             (unsigned int)PRESET_LITERS,
             (unsigned int)PULSES_PER_LITER,
             (unsigned int)PRESET_CNT_TARGET);

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

int main(void)
{
    error_code_t ogm_ret;

    if (Board_EarlyInit() != ERROR_OK) {
        while (1) { }
    }

    System_Init();

    if (OLED_Init() != OLED_OK) {
        OGM_ErrorBlink(80, 80);
    }

    Pump_InitComm();
    Pump_InitButtons();

    OLED_Clear();
    g_oled_dirty = 1;
    Pump_RefreshOLED(0);

    ogm_ret = OGM_FlowIC_Init();
    if (ogm_ret != OGM_FLOW_IC_OK) {
        LOG_ERROR("OGM", "FlowIC init fail: %d", (int)ogm_ret);
        OGM_ErrorBlink(100, 100);
    }

    if (g_comm_ok) {
        Pump_DelayWithButtons(INVT_WRITE_GAP_MS);
        g_set_freq_hz = PUMP_FREQ_DEFAULT_HZ;
        if (Pump_ApplyFrequency()) {
            LOG_INFO("MAIN", "上电设频 %u Hz（待机，按 PA4 加 4L）",
                     (unsigned int)g_set_freq_hz);
        } else {
            LOG_WARN("MAIN", "上电设频失败，请检查 485 后按 PA4 重试");
        }
    }

    g_oled_dirty = 1;

    g_last_led_tick = Delay_GetTick();
    g_last_ogm_task_tick = g_task_tick;
    g_last_ogm_ui_tick = g_task_tick;
    g_count_snap = OGM_GetCount();
    g_last_display_count = g_count_snap;

    LOG_INFO("MAIN", "PA4=启动 4L；%u cnt 自动停，可重复按键",
             (unsigned int)PRESET_CNT_TARGET);

    while (1) {
        while (g_last_ogm_task_tick != g_task_tick) {
            g_last_ogm_task_tick++;
            OGM_UpdateCountDelta();
        }

        (void)OGM_FlowIC_Process100ms();

        if ((uint32_t)(g_task_tick - g_last_ogm_ui_tick) >= OGM_RATE_WINDOW_MS) {
            uint32_t window_ms;

            window_ms = g_task_tick - g_last_ogm_ui_tick;
            g_last_ogm_ui_tick = g_task_tick;
            OGM_Tick1s(window_ms);
            g_oled_dirty = 1;
        }

        Pump_PollButtons();
        Pump_CheckPresetAutoStop();

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
