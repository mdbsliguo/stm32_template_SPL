/**
 * @file main_example.c
 * @brief NPN03 - 预设加油泵（OGM 计量 + GD200A ModBus 控泵）
 * @example Examples/NPN/NPN03_Preset_Pump_SwAlgo/main_example.c
 * @details 整合 Bus04 变频器 485 控泵与 NPN02 OGM 脉冲计量；
 *          三按键调频/启停，OLED 显示 Cnt 与 d/1s（体积标定待实现）
 *
 * 硬件：
 * - OGM A/B：PA0/PA1；RS485：PA2/PA3；按键：PA4/PA5/PA6
 * - OLED：PB8/PB9；LED：PB12；Debug UART1：PA9/PA10
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
#include "exti.h"
#include "led.h"
#include "gpio.h"
#include "board.h"
#include "TIM2_TimeBase.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ==================== 应用参数 ==================== */

#define PULSES_PER_LITER            1000u   /**< 标定：1000 脉冲 = 1L（与 OGM 规格一致时再启用 Vol） */
#define OGM_PULSES_PER_REV          8u      /**< NPN02 四边沿互锁：齿轮一圈 8 次计数 */
#define OGM_RATE_WINDOW_MS          1000u   /**< 速率显示窗口：1 秒 */
#define PUMP_FREQ_MIN_HZ            0u
#define PUMP_FREQ_MAX_HZ            50u
#define PUMP_FREQ_STEP_HZ           5u
#define PUMP_FREQ_DEFAULT_HZ        25u

#define INVT_SLAVE_ADDRESS          1
#define INVT_REG_RUN_CMD            0x2000
#define INVT_REG_FREQ_SET           0x2001
#define INVT_CMD_REVERSE            0x0002
#define INVT_CMD_STOP               0x0005
#define INVT_CMD_RUN                INVT_CMD_REVERSE  /**< 本案例默认反转运行 */

#define INVT_WRITE_GAP_MS           80
#define INVT_MODBUS_TIMEOUT_MS      1000
#define INVT_WRITE_RETRY_COUNT      3u
#define INVT_WRITE_RETRY_DELAY_MS   120u

#define BTN_SCAN_MS                 10
#define BTN_LOCKOUT_MS              800u    /**< 按下触发 485 后 800ms 内消抖并保护总线 */
#define OLED_REFRESH_RUN_MS         200
#define LED_BLINK_MS                400

/** 按键：按下沿触发，lockout 由全局 800ms 统一管理 */
typedef struct {
    uint8_t last_pressed;
} BtnKey_t;

static BtnKey_t g_btn_up;
static BtnKey_t g_btn_dn;
static BtnKey_t g_btn_run;

static uint32_t g_btn_lockout_start_ms = 0;

static uint8_t g_modbus_busy = 0;
static uint8_t g_pump_cmd_busy = 0;     /**< 启停序列进行中，暂缓再次启停 */

/* ==================== OGM 四边沿互锁（与 NPN02 相同，已调试算法） ==================== */

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
static volatile uint32_t g_count_delta_1s = 0;

static uint8_t  g_set_freq_hz = PUMP_FREQ_DEFAULT_HZ;
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
static void Pump_ToggleRunOnPress(void);
static uint8_t Pump_Start(void);
static uint8_t Pump_Stop(void);

static ModBusRTU_Status_t Pump_CommPreflight(void);

static uint32_t OGM_SnapshotCount(void);
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

/** ModBus 等待间隙内仅扫描按键（800ms 锁定期内不会重复发 485） */
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
    LOG_INFO("PUMP", "泵停止");
    return 1;
}

/* ==================== OGM 计数 ==================== */

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

static void OGM_ProcessChannel(uint8_t is_channel_a)
{
    uint8_t level_new;
    uint8_t level_last;
    uint8_t lock_bit;

    if (is_channel_a) {
        level_new = OGM_ReadLevel(OGM_CH_A_PORT, OGM_CH_A_PIN);
        level_last = g_last_a;
        lock_bit = level_new ? OGM_LOCK_A_RISE : OGM_LOCK_A_FALL;
    } else {
        level_new = OGM_ReadLevel(OGM_CH_B_PORT, OGM_CH_B_PIN);
        level_last = g_last_b;
        lock_bit = level_new ? OGM_LOCK_B_RISE : OGM_LOCK_B_FALL;
    }

    if (level_new == level_last) {
        return;
    }

    if (is_channel_a) {
        g_edge_locks &= (uint8_t)(~OGM_LOCK_B_MASK);
    } else {
        g_edge_locks &= (uint8_t)(~OGM_LOCK_A_MASK);
    }

    if ((g_edge_locks & lock_bit) == 0u) {
        g_count++;
        g_edge_locks |= lock_bit;
    }

    if (is_channel_a) {
        g_last_a = level_new;
    } else {
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

static void OGM_ResetCount(void)
{
    __disable_irq();
    g_count = 0u;
    g_count_snap = 0u;
    g_count_delta_1s = 0u;
    g_edge_locks = 0u;
    g_last_a = OGM_ReadLevel(OGM_CH_A_PORT, OGM_CH_A_PIN);
    g_last_b = OGM_ReadLevel(OGM_CH_B_PORT, OGM_CH_B_PIN);
    __enable_irq();
}

static uint8_t OGM_InitInputs(void)
{
    g_last_a = OGM_ReadLevel(OGM_CH_A_PORT, OGM_CH_A_PIN);
    g_last_b = OGM_ReadLevel(OGM_CH_B_PORT, OGM_CH_B_PIN);
    g_edge_locks = 0u;
    g_count = 0u;
    g_count_snap = 0u;
    g_count_delta_1s = 0u;

    if (!OGM_InitExtiChannel(EXTI_LINE_0, OGM_ChannelA_Callback, OGM_CH_A_PORT, OGM_CH_A_PIN)) {
        return 0u;
    }
    if (!OGM_InitExtiChannel(EXTI_LINE_1, OGM_ChannelB_Callback, OGM_CH_B_PORT, OGM_CH_B_PIN)) {
        return 0u;
    }
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

/* ==================== 按键：按下沿立即写 485，800ms 全局锁定 ==================== */

static uint8_t Pump_ButtonPressed(GPIO_TypeDef *port, uint16_t pin)
{
    return (GPIO_ReadPin(port, pin) == Bit_RESET) ? 1u : 0u;
}

static uint8_t Pump_ButtonLockoutActive(uint32_t now)
{
    if (g_btn_lockout_start_ms == 0u) {
        return 0u;
    }
    return (Delay_GetElapsed(now, g_btn_lockout_start_ms) < BTN_LOCKOUT_MS) ? 1u : 0u;
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

    edge_up = Pump_ButtonPressEdge(&g_btn_up, up);
    edge_dn = Pump_ButtonPressEdge(&g_btn_dn, dn);
    edge_run = Pump_ButtonPressEdge(&g_btn_run, run);

    if (g_modbus_busy || g_pump_cmd_busy) {
        return;
    }

    if (Pump_ButtonLockoutActive(now)) {
        return;
    }

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
        Pump_ToggleRunOnPress();
    }
}

static void Pump_ToggleRunOnPress(void)
{
    if (g_pump_running) {
        (void)Pump_Stop();
    } else {
        (void)Pump_Start();
    }
    g_oled_dirty = 1;
}

static void Pump_FreqUp(void)
{
    uint8_t prev = g_set_freq_hz;

    if (g_set_freq_hz + PUMP_FREQ_STEP_HZ <= PUMP_FREQ_MAX_HZ) {
        g_set_freq_hz = (uint8_t)(g_set_freq_hz + PUMP_FREQ_STEP_HZ);
    } else {
        g_set_freq_hz = PUMP_FREQ_MAX_HZ;
    }

    if (g_set_freq_hz == prev) {
        return;
    }

    g_oled_dirty = 1;
    LED1_Toggle();
    (void)Pump_ApplyFrequency();
}

static void Pump_FreqDown(void)
{
    uint8_t prev = g_set_freq_hz;

    if (g_set_freq_hz == 0) {
        return;
    }

    if (g_set_freq_hz <= PUMP_FREQ_STEP_HZ) {
        g_set_freq_hz = PUMP_FREQ_MIN_HZ;
    } else {
        g_set_freq_hz = (uint8_t)(g_set_freq_hz - PUMP_FREQ_STEP_HZ);
    }

    if (g_set_freq_hz == prev) {
        return;
    }

    g_oled_dirty = 1;
    LED1_Toggle();

    if (g_set_freq_hz == 0 && g_pump_running) {
        (void)Pump_Stop();
    }

    (void)Pump_ApplyFrequency();
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

static void Pump_ShowAbLevels(uint8_t level_a, uint8_t level_b)
{
    OLED_ShowString(4, 1, "A:");
    OLED_ShowString(4, 3, level_a ? "H" : "L");
    OLED_ShowString(4, 5, "B:");
    OLED_ShowString(4, 7, level_b ? "H" : "L");
}

static void Pump_RefreshOLED(uint32_t delta_1s)
{
    char buffer[20];
    uint32_t cnt;
    uint8_t level_a;
    uint8_t level_b;

    cnt = OGM_SnapshotCount();
    Pump_ShowPulseCount(cnt);

    snprintf(buffer, sizeof(buffer), "F:%02u ", (unsigned int)g_set_freq_hz);
    OLED_ShowString(2, 1, buffer);
    OLED_ShowString(2, 6, g_pump_running ? "RUN " : "STOP");

    Pump_ShowPulseRate(delta_1s);

    level_a = OGM_ReadLevel(OGM_CH_A_PORT, OGM_CH_A_PIN);
    level_b = OGM_ReadLevel(OGM_CH_B_PORT, OGM_CH_B_PIN);
    Pump_ShowAbLevels(level_a, level_b);

    if (g_comm_ok) {
        snprintf(buffer, sizeof(buffer), " Set:%02uHz", (unsigned int)g_set_freq_hz);
        OLED_ShowString(4, 9, buffer);
    } else {
        OLED_ShowString(4, 9, " 485:ERR");
    }

    /* 按键电平调试：L=按下 H=松开，便于现场确认接线 */
    OLED_ShowString(3, 13, "K");
    OLED_ShowString(3, 14, Pump_ButtonPressed(BTN_FREQ_UP_PORT, BTN_FREQ_UP_PIN) ? "0" : "1");
    OLED_ShowString(3, 15, Pump_ButtonPressed(BTN_FREQ_DOWN_PORT, BTN_FREQ_DOWN_PIN) ? "0" : "1");
    OLED_ShowString(3, 16, Pump_ButtonPressed(BTN_RUN_STOP_PORT, BTN_RUN_STOP_PIN) ? "0" : "1");
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

    if (g_pump_running) {
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

    LOG_INFO("MAIN", "=== NPN03 Preset Pump SwAlgo ===");
    LOG_INFO("MAIN", "UART2: 19200 8E1 Addr=%d", INVT_SLAVE_ADDRESS);
    LOG_INFO("MAIN", "按键：按下沿立即写485，800ms内消抖/保护总线");
    LOG_INFO("MAIN", "请确认 GD200A P00.01=2 P00.02=0 P00.06=8 P00.09=0");

    status = Pump_CommPreflight();
    if (status == ModBusRTU_OK) {
        g_comm_ok = 1;
        LOG_INFO("MAIN", "485 通讯预检通过");
    } else {
        g_comm_ok = 0;
        LOG_WARN("MAIN", "485 预检失败: %s，按键仍可用", GetModBusErrorString(status));
    }
}

/* ==================== 主函数 ==================== */

static uint32_t g_last_ogm_task_tick = 0;
static uint32_t g_last_ogm_ui_tick = 0;
static uint32_t g_display_delta_1s = 0;
static uint32_t g_last_display_count = 0;

#define OLED_REFRESH_IDLE_MS        200u

int main(void)
{
    System_Init();

    if (OLED_Init() != OLED_OK) {
        OGM_ErrorBlink(80, 80);
    }

    if (!OGM_InitInputs()) {
        OGM_ErrorBlink(100, 100);
    }

    Pump_InitComm();
    Pump_InitButtons();

    /* 上电将本地默认频率（25Hz）写入 0x2001，避免 OLED 显示与变频器不一致 */
    if (g_comm_ok) {
        Delay_ms(INVT_WRITE_GAP_MS);
        if (Pump_ApplyFrequency()) {
            LOG_INFO("MAIN", "上电已同步设频 %u Hz 至变频器 0x2001", (unsigned int)g_set_freq_hz);
        } else {
            LOG_WARN("MAIN", "上电同步设频失败，请按键升/降频或启停重试");
        }
    }

    OLED_Clear();
    g_oled_dirty = 1;

    g_last_led_tick = Delay_GetTick();
    g_last_ogm_task_tick = g_task_tick;
    g_last_ogm_ui_tick = g_task_tick;
    g_count_snap = g_count;

    LOG_INFO("MAIN", "OGM 计数同 NPN02 四边沿互锁，一圈 %u 计数", (unsigned int)OGM_PULSES_PER_REV);
    LOG_INFO("MAIN", "485 无后台轮询；上电/按键/启停时写 0x2001 设频");

    while (1) {
        while (g_last_ogm_task_tick != g_task_tick) {
            g_last_ogm_task_tick++;
            OGM_UpdateCountDelta();
        }

        if ((uint32_t)(g_task_tick - g_last_ogm_ui_tick) >= OGM_RATE_WINDOW_MS) {
            uint32_t delta_snap;

            g_last_ogm_ui_tick = g_task_tick;
            __disable_irq();
            delta_snap = g_count_delta_1s;
            g_count_delta_1s = 0u;
            __enable_irq();
            g_display_delta_1s = delta_snap;
            g_oled_dirty = 1;
        }

        Pump_PollButtons();

        {
            uint32_t now_count;

            now_count = OGM_SnapshotCount();
            if (now_count != g_last_display_count) {
                g_last_display_count = now_count;
                g_oled_dirty = 1;
            }
        }

        {
            uint32_t now = Delay_GetTick();
            uint32_t refresh_ms = g_pump_running ? OLED_REFRESH_RUN_MS : OLED_REFRESH_IDLE_MS;

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
