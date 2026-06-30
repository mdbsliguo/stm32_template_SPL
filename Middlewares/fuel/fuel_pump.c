/**
 * @file fuel_pump.c
 * @brief ModBus pump control (NPN07, fixed 10Hz run)
 */

#include "fuel_pump.h"
#include "fuel_key.h"
#include "fuel_app.h"
#include "modbus_rtu.h"
#include "uart.h"
#include "delay.h"
#include "log.h"
#include "error_handler.h"
#include "config.h"

#define INVT_SLAVE_ADDRESS          1u
#define INVT_REG_RUN_CMD            0x2000u
#define INVT_REG_FREQ_SET           0x2001u
#define INVT_REG_STATUS_WORD1       0x2100u
#define INVT_CMD_REVERSE            0x0002u
#define INVT_CMD_STOP               0x0005u
#define INVT_CMD_RUN                INVT_CMD_REVERSE
#define INVT_STATUS_RUN_FORWARD     0x0001u
#define INVT_STATUS_RUN_REVERSE     0x0002u

#define INVT_WRITE_GAP_MS           80u
#define INVT_MODBUS_TIMEOUT_MS      1000u
#define INVT_MODBUS_STOP_TIMEOUT_MS 300u
#define INVT_WRITE_RETRY_COUNT      3u
#define INVT_WRITE_RETRY_DELAY_MS   120u
#define FUEL_PUMP_KEY_SCAN_MS       10u

static uint8_t s_standby_freq_hz = FUEL_PUMP_DEFAULT_FREQ_HZ;
static uint8_t s_comm_ok;
static volatile uint8_t s_running;
static uint8_t s_cmd_busy;
static uint8_t s_modbus_busy;
static uint8_t s_inited;
static uint8_t s_in_force_stop;
static fuel_key_event_t s_comm_defer_evt;

static void fuel_pump_flush_deferred_key(void)
{
    fuel_key_event_t evt;

    evt = s_comm_defer_evt;
    s_comm_defer_evt = FUEL_KEY_EVT_NONE;
    if (evt != FUEL_KEY_EVT_NONE) {
        (void)FuelApp_ProcessKeyEvent(evt);
    }
}

static const char *fuel_pump_modbus_str(ModBusRTU_Status_t status)
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

static ModBusRTU_Status_t fuel_pump_write_reg(uint16_t reg, uint16_t val, uint32_t to_ms)
{
    return ModBusRTU_WriteSingleRegister(UART_INSTANCE_2, INVT_SLAVE_ADDRESS,
                                         reg, val, to_ms);
}

static void fuel_pump_service_keys(void)
{
    fuel_key_event_t evt;

    (void)FuelKey_Poll(0u);
    for (;;) {
        evt = FuelKey_ConsumeEvent();
        if (evt == FUEL_KEY_EVT_NONE) {
            break;
        }
        if ((s_modbus_busy || s_cmd_busy || s_in_force_stop) &&
            (evt == FUEL_KEY_EVT_RED_SHORT || evt == FUEL_KEY_EVT_RED_LONG)) {
            s_comm_defer_evt = evt;
            UART_RequestAbort();
            return;
        }
        (void)FuelApp_ProcessKeyEvent(evt);
    }
}

void UART_PollHook(void)
{
    fuel_pump_service_keys();
}

static void fuel_pump_delay_poll(uint32_t ms)
{
    uint32_t start;

    start = Delay_GetTick();
    while (Delay_GetElapsed(Delay_GetTick(), start) < ms) {
        fuel_pump_service_keys();
        Delay_ms(FUEL_PUMP_KEY_SCAN_MS);
    }
}

static ModBusRTU_Status_t fuel_pump_write_freq(uint8_t freq_hz, uint8_t leading_gap)
{
    uint8_t retry;
    ModBusRTU_Status_t st;
    uint16_t raw;

    raw = (uint16_t)freq_hz * 100u;
    if (leading_gap) {
        fuel_pump_delay_poll(INVT_WRITE_GAP_MS);
    }

    st = ModBusRTU_ERROR_TIMEOUT;
    for (retry = 0u; retry < INVT_WRITE_RETRY_COUNT; retry++) {
        s_modbus_busy = 1u;
        st = fuel_pump_write_reg(INVT_REG_FREQ_SET, raw, INVT_MODBUS_TIMEOUT_MS);
        s_modbus_busy = 0u;
        fuel_pump_flush_deferred_key();
        if (st == ModBusRTU_OK) {
            break;
        }
        fuel_pump_delay_poll(INVT_WRITE_RETRY_DELAY_MS);
    }
    return st;
}

static ModBusRTU_Status_t fuel_pump_send_run_cmd(uint16_t cmd)
{
    uint8_t retry;
    ModBusRTU_Status_t st;

    if (cmd != INVT_CMD_STOP) {
        fuel_pump_delay_poll(INVT_WRITE_GAP_MS);
    }

    for (retry = 0u; retry < INVT_WRITE_RETRY_COUNT; retry++) {
        uint32_t to_ms = (cmd == INVT_CMD_STOP) ?
                         INVT_MODBUS_STOP_TIMEOUT_MS : INVT_MODBUS_TIMEOUT_MS;

        s_modbus_busy = 1u;
        st = fuel_pump_write_reg(INVT_REG_RUN_CMD, cmd, to_ms);
        s_modbus_busy = 0u;
        fuel_pump_flush_deferred_key();
        if (st == ModBusRTU_OK) {
            return ModBusRTU_OK;
        }
        if (cmd != INVT_CMD_STOP) {
            fuel_pump_delay_poll(INVT_WRITE_RETRY_DELAY_MS);
        } else {
            Delay_ms(INVT_WRITE_RETRY_DELAY_MS);
        }
    }
    return st;
}

static uint8_t fuel_pump_ensure_stopped(void)
{
    uint16_t word;
    ModBusRTU_Status_t st;

    st = ModBusRTU_ReadHoldingRegisters(UART_INSTANCE_2, INVT_SLAVE_ADDRESS,
                                        INVT_REG_STATUS_WORD1, 1u, &word,
                                        INVT_MODBUS_TIMEOUT_MS);
    if (st != ModBusRTU_OK) {
        LOG_WARN("PUMP", "read 2100H fail: %s", fuel_pump_modbus_str(st));
        return 0u;
    }

    if (word == INVT_STATUS_RUN_FORWARD || word == INVT_STATUS_RUN_REVERSE) {
        LOG_WARN("PUMP", "VFD running 0x%04X, force stop", (unsigned int)word);
        if (FuelPump_Stop() != FUEL_PUMP_OK) {
            return 0u;
        }
    }
    return 1u;
}

error_code_t FuelPump_Init(uint8_t standby_freq_hz)
{
    ModBusRTU_Status_t st;
    uint16_t word;

    if (s_inited) {
        return FUEL_PUMP_OK;
    }

    if (standby_freq_hz == 0u) {
        standby_freq_hz = FUEL_PUMP_DEFAULT_FREQ_HZ;
    }
    s_standby_freq_hz = standby_freq_hz;
    s_running = 0u;
    s_inited = 1u;

    st = ModBusRTU_ReadHoldingRegisters(UART_INSTANCE_2, INVT_SLAVE_ADDRESS,
                                        INVT_REG_FREQ_SET, 1u, &word, 300u);
    if (st != ModBusRTU_OK) {
        LOG_WARN("PUMP", "485 preflight fail: %s (skip stop/freq)", fuel_pump_modbus_str(st));
        s_comm_ok = 0u;
        return FUEL_PUMP_OK;
    }

    s_comm_ok = 1u;
    LOG_INFO("PUMP", "485 preflight OK");

    if (!fuel_pump_ensure_stopped()) {
        s_comm_ok = 0u;
    }

    if (FuelPump_ApplyFrequency(s_standby_freq_hz) != FUEL_PUMP_OK) {
        LOG_WARN("PUMP", "standby freq apply fail");
    }

    return FUEL_PUMP_OK;
}

error_code_t FuelPump_SetStandbyFreq(uint8_t freq_hz)
{
    if (freq_hz == 0u) {
        return FUEL_PUMP_ERROR_INVALID_PARAM;
    }
    s_standby_freq_hz = freq_hz;
    return FUEL_PUMP_OK;
}

uint8_t FuelPump_IsCommOk(void)
{
    return s_comm_ok;
}

uint8_t FuelPump_IsRunning(void)
{
    return s_running;
}

uint8_t FuelPump_IsBusy(void)
{
    return (s_cmd_busy || s_modbus_busy) ? 1u : 0u;
}

error_code_t FuelPump_ApplyFrequency(uint8_t freq_hz)
{
    ModBusRTU_Status_t st;

    if (!s_inited) {
        return FUEL_PUMP_ERROR_NOT_INIT;
    }
    if (freq_hz == 0u) {
        return FUEL_PUMP_ERROR_INVALID_PARAM;
    }

    st = fuel_pump_write_freq(freq_hz, 0u);
    if (st != ModBusRTU_OK) {
        LOG_ERROR("PUMP", "set freq %uHz fail: %s", (unsigned int)freq_hz,
                  fuel_pump_modbus_str(st));
        ErrorHandler_Handle(st, "INVT_FREQ");
        s_comm_ok = 0u;
        return FUEL_PUMP_ERROR_COMM;
    }

    s_comm_ok = 1u;
    LOG_INFO("PUMP", "freq %u Hz OK", (unsigned int)freq_hz);
    return FUEL_PUMP_OK;
}

error_code_t FuelPump_Start(uint8_t freq_hz)
{
    ModBusRTU_Status_t st;

    if (!s_inited) {
        return FUEL_PUMP_ERROR_NOT_INIT;
    }
    if (s_cmd_busy) {
        return FUEL_PUMP_ERROR_BUSY;
    }
    if (freq_hz == 0u) {
        freq_hz = s_standby_freq_hz;
    }

    s_cmd_busy = 1u;
    st = fuel_pump_write_freq(freq_hz, 1u);
    if (st != ModBusRTU_OK) {
        s_comm_ok = 0u;
        s_cmd_busy = 0u;
        return FUEL_PUMP_ERROR_COMM;
    }

    st = fuel_pump_send_run_cmd(INVT_CMD_RUN);
    if (st != ModBusRTU_OK) {
        s_comm_ok = 0u;
        s_cmd_busy = 0u;
        return FUEL_PUMP_ERROR_COMM;
    }

    s_running = 1u;
    s_comm_ok = 1u;
    s_cmd_busy = 0u;
    LOG_INFO("PUMP", "start %u Hz", (unsigned int)freq_hz);
    return FUEL_PUMP_OK;
}

error_code_t FuelPump_Stop(void)
{
    ModBusRTU_Status_t st;

    if (!s_inited) {
        return FUEL_PUMP_ERROR_NOT_INIT;
    }
    if (s_cmd_busy) {
        return FUEL_PUMP_ERROR_BUSY;
    }

    s_cmd_busy = 1u;
    st = fuel_pump_send_run_cmd(INVT_CMD_STOP);
    if (st != ModBusRTU_OK) {
        s_comm_ok = 0u;
        s_cmd_busy = 0u;
        return FUEL_PUMP_ERROR_COMM;
    }

    s_running = 0u;
    s_cmd_busy = 0u;
    LOG_INFO("PUMP", "stopped");
    return FUEL_PUMP_OK;
}

error_code_t FuelPump_ForceStop(void)
{
    uint8_t retry;
    error_code_t ret;

    if (!s_inited) {
        return FUEL_PUMP_ERROR_NOT_INIT;
    }
    if (s_in_force_stop) {
        return FUEL_PUMP_ERROR_BUSY;
    }

    s_in_force_stop = 1u;
    UART_RequestAbort();
    ret = FUEL_PUMP_ERROR_COMM;
    for (retry = 0u; retry < INVT_WRITE_RETRY_COUNT + 2u; retry++) {
        if (s_cmd_busy) {
            UART_RequestAbort();
            Delay_ms(INVT_WRITE_RETRY_DELAY_MS);
        }
        ret = FuelPump_Stop();
        if (ret == FUEL_PUMP_OK) {
            s_in_force_stop = 0u;
            return FUEL_PUMP_OK;
        }
        Delay_ms(INVT_WRITE_RETRY_DELAY_MS);
    }

    s_running = 0u;
    s_cmd_busy = 0u;
    s_modbus_busy = 0u;
    s_in_force_stop = 0u;
    LOG_WARN("PUMP", "force stop gave up, mark stopped");
    return ret;
}
