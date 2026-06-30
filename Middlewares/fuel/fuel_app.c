/**
 * @file fuel_app.c
 * @brief Fuel state machine: READY / FUELING / FAULT
 */

#include "fuel_app.h"
#include "fuel_meter.h"
#include "ogm_flow_ic.h"
#include "fuel_pump.h"
#include "fuel_key.h"
#include "fuel_store.h"
#include "fuel_runtime.h"
#include "gpio.h"
#include "board.h"
#include "delay.h"
#include "log.h"
#include "rtc.h"
#include "config.h"
#if defined(CONFIG_MODULE_LITER_RING_ENABLED) && CONFIG_MODULE_LITER_RING_ENABLED
#include "liter_ring.h"
#endif
#include <stddef.h>
#include <string.h>

static uint8_t  s_inited;
static uint8_t  s_oled_dirty;
static uint8_t  s_pump_stop_pending;
static uint32_t s_pulse_base;
static uint32_t s_txn_seq;
static uint32_t s_fault_clear_ms;
static uint32_t s_session_start_ms;
static uint32_t s_pump_stop_last_ms;
static uint32_t s_ring_last_whole_liter;
static uint8_t  s_pump_stop_fail;

#define FUEL_PUMP_STOP_RETRY_MS     400u
#define FUEL_PUMP_STOP_MAX_RETRY    8u

static void fuel_schedule_pump_stop(void)
{
    s_pump_stop_pending = 1u;
    s_pump_stop_fail = 0u;
}

static void fuel_service_pump_stop(uint32_t now_ms)
{
    if (!s_pump_stop_pending) {
        return;
    }
    if (FuelPump_IsBusy()) {
        return;
    }
    if (!FuelPump_IsRunning()) {
        s_pump_stop_pending = 0u;
        s_pump_stop_fail = 0u;
        return;
    }
    if (Delay_GetElapsed(now_ms, s_pump_stop_last_ms) < FUEL_PUMP_STOP_RETRY_MS) {
        return;
    }
    s_pump_stop_last_ms = now_ms;
    if (FuelPump_Stop() == FUEL_PUMP_OK) {
        s_pump_stop_pending = 0u;
        s_pump_stop_fail = 0u;
        return;
    }
    s_pump_stop_fail++;
    if (s_pump_stop_fail >= FUEL_PUMP_STOP_MAX_RETRY) {
        (void)FuelPump_ForceStop();
        s_pump_stop_pending = 0u;
        s_pump_stop_fail = 0u;
    }
}

static void fuel_valve_init(void)
{
    (void)GPIO_Config(RELAY1_PORT, RELAY1_PIN, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_2MHz);
    GPIO_WritePin(RELAY1_PORT, RELAY1_PIN, RELAY1_SAFE_LEVEL);
}

static void fuel_valve_set(uint8_t open)
{
    if (open) {
        GPIO_SetPin(RELAY1_PORT, RELAY1_PIN);
    } else {
        GPIO_WritePin(RELAY1_PORT, RELAY1_PIN, RELAY1_SAFE_LEVEL);
    }
}

static void fuel_idle_ogm_guard(void)
{
    uint32_t raw;

    if (OGM_FlowIC_GetCount(&raw) != OGM_FLOW_IC_OK) {
        return;
    }
    if (raw > 0u) {
        (void)FuelMeter_ResetCount();
    }
}

static void fuel_reset_ogm_hardware(void)
{
    s_pulse_base = 0u;
    (void)FuelMeter_ResetCount();
}

static void fuel_clear_session_meters(fuel_runtime_t *rt)
{
    if (rt == NULL) {
        return;
    }

    fuel_reset_ogm_hardware();
    rt->pulse_snap = 0u;
    rt->dispensed_liter_x100 = 0u;
}

static uint32_t fuel_total_pulse(void)
{
    return s_pulse_base + FuelMeter_GetCount();
}

#if defined(CONFIG_MODULE_LITER_RING_ENABLED) && CONFIG_MODULE_LITER_RING_ENABLED
static error_code_t fuel_ring_init(void)
{
    liter_ring_config_t cfg;
    error_code_t err;

    if (LiterRing_IsInitialized()) {
        return ERROR_OK;
    }

    cfg.file_path = FUEL_LITER_RING_FILE_PATH;
    cfg.capacity = FUEL_LITER_RING_CAPACITY;
    cfg.sync_every = FUEL_LITER_RING_SYNC_EVERY;
    err = LiterRing_Init(&cfg);
    if (err != ERROR_OK) {
        LOG_WARN("FUEL", "liter_ring init %d", (int)err);
    }
    return err;
}

static void fuel_ring_try_append(fuel_runtime_t *rt)
{
    liter_ring_record_t rec;
    uint32_t whole;

    if (rt == NULL || !rt->session_active) {
        return;
    }
    if (!LiterRing_IsInitialized()) {
        return;
    }

    whole = rt->dispensed_liter_x100 / 100u;
    while (s_ring_last_whole_liter < whole) {
        s_ring_last_whole_liter++;
        memset(&rec, 0, sizeof(rec));
        rec.liter_x1000 = s_ring_last_whole_liter * 1000u;
        rec.pulse = rt->pulse_snap;
        rec.txn_id = rt->txn_id;
        (void)LiterRing_Append(&rec);
    }
}
#else
static error_code_t fuel_ring_init(void)
{
    return ERROR_OK;
}

static void fuel_ring_try_append(fuel_runtime_t *rt)
{
    (void)rt;
}
#endif

static void fuel_sync_dispensed(fuel_runtime_t *rt)
{
    const fuel_machine_config_t *mc;

    if (rt == NULL || !rt->session_active) {
        return;
    }

    mc = &rt->machine_cache;
    rt->pulse_snap = fuel_total_pulse();
    rt->dispensed_liter_x100 = Fuel_LiterX100FromPulse(rt->pulse_snap,
                                                       mc->pulses_per_liter);
    fuel_ring_try_append(rt);
}

static void fuel_build_checkpoint(fuel_runtime_t *rt, fuel_checkpoint_t *cp)
{
    if (rt == NULL || cp == NULL) {
        return;
    }

    memset(cp, 0, sizeof(*cp));
    cp->txn_id = rt->txn_id;
    cp->saved_state = (uint8_t)rt->state;
    cp->saved_sub = (uint8_t)rt->run_sub;
    cp->pause_reason = (uint8_t)rt->pause_reason;
    cp->session_active = rt->session_active;
    cp->target_session_liter_x100 = rt->target_session_liter_x100;
    cp->dispensed_liter_x100 = rt->dispensed_liter_x100;
    cp->pulse_count = rt->pulse_snap;
    cp->target_pulse = rt->target_pulse;
    cp->rtc_counter = rt->session_start_rtc;
}

static error_code_t fuel_save_checkpoint(fuel_runtime_t *rt)
{
    fuel_checkpoint_t cp;
    error_code_t ret;

    if (rt == NULL || !rt->session_active) {
        return FUEL_APP_OK;
    }

    fuel_build_checkpoint(rt, &cp);
    ret = FuelStore_SaveCheckpoint(&cp);
    if (ret != FUEL_STORE_OK) {
        rt->fault_code |= FUEL_FAULT_CHECKPOINT;
        return FUEL_APP_ERROR_STORAGE;
    }
    rt->checkpoint_dirty = 0u;
    rt->last_checkpoint_ms = Delay_GetTick();
    return FUEL_APP_OK;
}

static void fuel_save_display_checkpoint(fuel_runtime_t *rt)
{
    fuel_checkpoint_t cp;

    if (rt == NULL) {
        return;
    }

    memset(&cp, 0, sizeof(cp));
    cp.txn_id = rt->txn_id;
    cp.saved_state = (uint8_t)FUEL_STATE_READY;
    cp.saved_sub = (uint8_t)FUEL_RUN_ACTIVE;
    cp.pause_reason = (uint8_t)FUEL_PAUSE_NONE;
    cp.session_active = 0u;
    cp.target_session_liter_x100 = rt->target_session_liter_x100;
    cp.dispensed_liter_x100 = rt->dispensed_liter_x100;
    cp.pulse_count = rt->pulse_snap;
    cp.target_pulse = rt->target_pulse;
    cp.rtc_counter = rt->session_start_rtc;
    (void)FuelStore_SaveCheckpoint(&cp);
}

static void fuel_end_session(fuel_runtime_t *rt, uint8_t persist_display)
{
    if (rt == NULL) {
        return;
    }

    if (rt->session_active) {
        fuel_sync_dispensed(rt);
    }

    fuel_schedule_pump_stop();
    fuel_valve_set(0u);
    rt->pump_running = 0u;
    rt->valve_open = 0u;
    rt->session_active = 0u;
    rt->state = FUEL_STATE_READY;
    rt->run_sub = FUEL_RUN_ACTIVE;
    rt->pause_reason = FUEL_PAUSE_NONE;
    /* ?? Add/c ? OLED???? fuel_begin_session ??? */
    fuel_reset_ogm_hardware();

    if (persist_display) {
        fuel_save_display_checkpoint(rt);
    } else {
        (void)FuelStore_ClearCheckpoint();
    }
    s_oled_dirty = 1u;
}

static error_code_t fuel_start_pump_run(fuel_runtime_t *rt)
{
    uint8_t freq_hz;
    error_code_t ret;
    const fuel_machine_config_t *mc;

    if (rt == NULL) {
        return FUEL_APP_ERROR_INVALID_PARAM;
    }

    mc = &rt->machine_cache;
    freq_hz = (uint8_t)(mc->vfd_run_freq_x100 / 100u);
    if (freq_hz == 0u) {
        freq_hz = FUEL_PUMP_DEFAULT_FREQ_HZ;
    }

    if (mc->valve_open_delay_ms > 0u) {
        Delay_ms(mc->valve_open_delay_ms);
    }
    fuel_valve_set(1u);
    rt->valve_open = 1u;
    rt->pump_running = 1u;
    rt->run_sub = FUEL_RUN_ACTIVE;
    rt->state = FUEL_STATE_FUELING;
    s_oled_dirty = 1u;

    ret = FuelPump_Start(freq_hz);
    if (ret != FUEL_PUMP_OK) {
        fuel_schedule_pump_stop();
        fuel_valve_set(0u);
        rt->valve_open = 0u;
        rt->pump_running = 0u;
        rt->fault_code |= FUEL_FAULT_VFD_COMM;
        rt->state = FUEL_STATE_FAULT;
        return ret;
    }

    rt->pause_reason = FUEL_PAUSE_NONE;
    rt->fault_code &= ~(FUEL_FAULT_VFD_COMM | FUEL_FAULT_OGM_STALL);
    s_oled_dirty = 1u;
    return FUEL_APP_OK;
}

static error_code_t fuel_pause_run(fuel_runtime_t *rt)
{
    const fuel_machine_config_t *mc;

    if (rt == NULL) {
        return FUEL_APP_ERROR_INVALID_PARAM;
    }

    mc = &rt->machine_cache;
    rt->pump_running = 0u;
    rt->run_sub = FUEL_RUN_PAUSED;
    fuel_schedule_pump_stop();

    if (mc->valve_close_on_pause) {
        fuel_valve_set(0u);
        rt->valve_open = 0u;
    }

    fuel_sync_dispensed(rt);
    rt->checkpoint_dirty = 1u;
    s_session_start_ms = 0u;
    s_oled_dirty = 1u;
    return FUEL_APP_OK;
}

static error_code_t fuel_begin_session(fuel_runtime_t *rt)
{
    const fuel_machine_config_t *mc;
    RTC_Time_t t;
    error_code_t ret;

    if (rt == NULL) {
        return FUEL_APP_ERROR_INVALID_PARAM;
    }

    mc = &rt->machine_cache;
    if (!Fuel_IsLiterX100InRange(rt->target_next_liter_x100, 1u,
                                 mc->max_target_liter_x100)) {
        LOG_WARN("FUEL", "target out of range");
        return FUEL_APP_ERROR_INVALID_PARAM;
    }

    s_txn_seq++;
    rt->txn_id = s_txn_seq;
    rt->target_session_liter_x100 = rt->target_next_liter_x100;
    rt->target_pulse = Fuel_PulseTargetFromLiterX100(rt->target_session_liter_x100,
                                                     mc->pulses_per_liter,
                                                     mc->hose_comp_liter_x100);
    rt->dispensed_liter_x100 = 0u;
    fuel_clear_session_meters(rt);
    s_ring_last_whole_liter = 0u;

    rt->state = FUEL_STATE_FUELING;
    rt->run_sub = FUEL_RUN_ACTIVE;
    rt->session_active = 1u;
    rt->pause_reason = FUEL_PAUSE_NONE;
    rt->fault_code = FUEL_FAULT_NONE;
    s_oled_dirty = 1u;

    if (RTC_IsInitialized() && RTC_GetTime(&t) == RTC_OK) {
        rt->session_start_rtc = (uint32_t)t.hour * 3600u +
                                (uint32_t)t.minute * 60u + (uint32_t)t.second;
    }

    fuel_sync_dispensed(rt);
    LOG_INFO("FUEL", "session start cnt=%lu tgt=%lu",
             (unsigned long)rt->pulse_snap, (unsigned long)rt->target_pulse);
    if (rt->pulse_snap >= rt->target_pulse && rt->target_pulse > 0u) {
        LOG_WARN("FUEL", "OGM not zero at start, reset again");
        fuel_clear_session_meters(rt);
        fuel_sync_dispensed(rt);
    }
    if (rt->pulse_snap >= rt->target_pulse && rt->target_pulse > 0u) {
        LOG_ERROR("FUEL", "OGM cnt=%lu >= tgt %lu, abort start",
                  (unsigned long)rt->pulse_snap, (unsigned long)rt->target_pulse);
        rt->session_active = 0u;
        rt->state = FUEL_STATE_READY;
        rt->run_sub = FUEL_RUN_ACTIVE;
        return FUEL_APP_ERROR_INVALID_PARAM;
    }

    ret = fuel_start_pump_run(rt);
    if (ret == FUEL_APP_OK) {
        s_session_start_ms = Delay_GetTick();
        rt->checkpoint_dirty = 1u;
        LOG_INFO("FUEL", "pump running, stall timer from here");
        s_oled_dirty = 1u;
        return FUEL_APP_OK;
    }

    LOG_WARN("FUEL", "pump start fail %d, rollback to Ready", (int)ret);
    fuel_schedule_pump_stop();
    rt->session_active = 0u;
    rt->state = FUEL_STATE_READY;
    rt->run_sub = FUEL_RUN_ACTIVE;
    rt->pause_reason = FUEL_PAUSE_NONE;
    fuel_valve_set(0u);
    rt->valve_open = 0u;
    rt->pump_running = 0u;
    s_oled_dirty = 1u;
    return ret;
}

static void fuel_check_auto_stop(fuel_runtime_t *rt)
{
    if (rt == NULL) {
        return;
    }

    if (rt->state != FUEL_STATE_FUELING || rt->run_sub != FUEL_RUN_ACTIVE) {
        return;
    }

    fuel_sync_dispensed(rt);
    if (rt->target_pulse > 0u && rt->pulse_snap >= rt->target_pulse) {
        LOG_INFO("FUEL", "target reached cnt=%lu/%lu",
                 (unsigned long)rt->pulse_snap, (unsigned long)rt->target_pulse);
        fuel_end_session(rt, 1u);
    }
}

static void fuel_check_faults(fuel_runtime_t *rt, uint32_t now_ms)
{
    const fuel_machine_config_t *mc;
    uint32_t debounce;

    if (rt == NULL) {
        return;
    }

    mc = &rt->machine_cache;
    debounce = mc->fault_recovery_debounce_ms;

    if (!FuelPump_IsCommOk()) {
        fuel_state_t prev_st = rt->state;

        rt->fault_code |= FUEL_FAULT_VFD_COMM;
        if (rt->state == FUEL_STATE_FUELING && rt->run_sub == FUEL_RUN_ACTIVE) {
            (void)fuel_pause_run(rt);
            rt->pause_reason = FUEL_PAUSE_FAULT;
            rt->state = FUEL_STATE_FAULT;
            LOG_WARN("FUEL", "485 lost, pause+fault");
            if (prev_st != FUEL_STATE_FAULT) {
                s_oled_dirty = 1u;
            }
        }
        return;
    }

    if ((rt->fault_code & FUEL_FAULT_VFD_COMM) != 0u) {
        rt->fault_code &= ~FUEL_FAULT_VFD_COMM;
        if (rt->state == FUEL_STATE_FAULT && rt->fault_code == FUEL_FAULT_NONE) {
            rt->state = rt->session_active ? FUEL_STATE_FUELING : FUEL_STATE_READY;
            s_oled_dirty = 1u;
        }
    }

    if (FuelMeter_IsFlowActive()) {
        rt->fault_code &= ~FUEL_FAULT_OGM_STALL;
    }

    if (rt->state == FUEL_STATE_FUELING && rt->run_sub == FUEL_RUN_ACTIVE &&
        rt->pump_running && s_session_start_ms != 0u) {
        uint32_t run_ms = Delay_GetElapsed(now_ms, s_session_start_ms);
        uint8_t flow;

        flow = FuelMeter_IsFlowActive();
        if (!flow && run_ms >= mc->ogm_stall_timeout_ms) {
            (void)fuel_pause_run(rt);
            rt->pause_reason = FUEL_PAUSE_FAULT;
            rt->fault_code |= FUEL_FAULT_OGM_STALL;
            LOG_WARN("FUEL", "OGM no flow %ums, paused (green resume)",
                     (unsigned int)run_ms);
            s_oled_dirty = 1u;
            return;
        }
    }

    if (rt->state == FUEL_STATE_FAULT) {
        fuel_state_t prev_st = rt->state;

        if (rt->fault_code == FUEL_FAULT_NONE) {
            rt->state = rt->session_active ? FUEL_STATE_FUELING : FUEL_STATE_READY;
            if (prev_st != rt->state) {
                s_oled_dirty = 1u;
            }
        } else if (FuelPump_IsCommOk() &&
                   Delay_GetElapsed(now_ms, s_fault_clear_ms) >= debounce) {
            if ((rt->fault_code & FUEL_FAULT_VFD_COMM) != 0u && FuelPump_IsCommOk()) {
                rt->fault_code &= ~FUEL_FAULT_VFD_COMM;
            }
            if (rt->fault_code == FUEL_FAULT_NONE) {
                rt->state = rt->session_active ? FUEL_STATE_FUELING : FUEL_STATE_READY;
                LOG_INFO("FUEL", "fault cleared");
                if (prev_st != rt->state) {
                    s_oled_dirty = 1u;
                }
            }
        }
    }
}

static void fuel_log_key_ignored(fuel_key_event_t evt, const fuel_runtime_t *rt)
{
    if (rt == NULL) {
        return;
    }

    LOG_WARN("KEY", "ignore %s st=%u sub=%u sess=%u",
             (evt == FUEL_KEY_EVT_GREEN_LONG) ? "green LONG" :
             (evt == FUEL_KEY_EVT_GREEN_SHORT) ? "green SHORT" :
             (evt == FUEL_KEY_EVT_RED_LONG) ? "red LONG" : "red SHORT",
             (unsigned int)rt->state,
             (unsigned int)rt->run_sub,
             (unsigned int)rt->session_active);
}

static void fuel_handle_key(fuel_runtime_t *rt, fuel_key_event_t evt)
{
    error_code_t ret;

    if (rt == NULL || evt == FUEL_KEY_EVT_NONE) {
        return;
    }

    switch (evt) {
    case FUEL_KEY_EVT_GREEN_SHORT:
        if (Fuel_CanResume(rt)) {
            if (fuel_start_pump_run(rt) == FUEL_APP_OK) {
                s_session_start_ms = Delay_GetTick();
                rt->checkpoint_dirty = 1u;
                s_oled_dirty = 1u;
                LOG_INFO("FUEL", "resume by green short");
            }
        } else if (rt->state == FUEL_STATE_READY && !rt->session_active) {
            LOG_INFO("KEY", "Ready: hold green >=%ums to start",
                     (unsigned int)FUEL_KEY_LONG_PRESS_MS);
        } else {
            fuel_log_key_ignored(evt, rt);
        }
        break;

    case FUEL_KEY_EVT_GREEN_LONG:
        if (Fuel_CanStartNewSession(rt)) {
            ret = fuel_begin_session(rt);
            if (ret == FUEL_APP_OK) {
                rt->checkpoint_dirty = 1u;
                s_oled_dirty = 1u;
                LOG_INFO("FUEL", "new round target=%lu.%02lu L",
                         (unsigned long)(rt->target_session_liter_x100 / 100u),
                         (unsigned long)(rt->target_session_liter_x100 % 100u));
            } else {
                LOG_WARN("FUEL", "new round fail %d", (int)ret);
            }
        } else {
            fuel_log_key_ignored(evt, rt);
        }
        break;

    case FUEL_KEY_EVT_RED_SHORT:
        if (Fuel_CanPause(rt)) {
            if (rt->session_active) {
                rt->pause_reason = FUEL_PAUSE_USER;
                (void)fuel_pause_run(rt);
                if (rt->state == FUEL_STATE_FAULT) {
                    rt->state = FUEL_STATE_FUELING;
                }
                LOG_INFO("FUEL", "paused by red short");
            } else {
                LOG_WARN("FUEL", "orphan stop by red short cnt=%lu",
                         (unsigned long)rt->pulse_snap);
                fuel_schedule_pump_stop();
                fuel_valve_set(0u);
                rt->pump_running = 0u;
                rt->valve_open = 0u;
            }
            s_oled_dirty = 1u;
        } else if (rt->session_active && rt->run_sub == FUEL_RUN_PAUSED) {
            LOG_INFO("KEY", "already paused, green short to resume");
        } else {
            fuel_log_key_ignored(evt, rt);
        }
        break;

    case FUEL_KEY_EVT_RED_LONG:
        if (Fuel_CanStopSession(rt)) {
            LOG_INFO("FUEL", "session stopped by red long");
            fuel_end_session(rt, 1u);
        } else if (rt->state == FUEL_STATE_FAULT && rt->session_active) {
            LOG_INFO("FUEL", "fault: red long force end");
            fuel_end_session(rt, 1u);
        } else {
            fuel_log_key_ignored(evt, rt);
        }
        break;

    default:
        break;
    }
}

#define FUEL_CKPT_RESTORE_NONE     0u
#define FUEL_CKPT_RESTORE_ACTIVE   1u
#define FUEL_CKPT_RESTORE_DISPLAY  2u

static uint8_t fuel_restore_checkpoint(fuel_runtime_t *rt)
{
    fuel_checkpoint_t cp;
    const fuel_machine_config_t *mc;

    if (rt == NULL) {
        return FUEL_CKPT_RESTORE_NONE;
    }

    mc = &rt->machine_cache;
    (void)FuelStore_LoadCheckpoint(&cp);

    if (cp.magic != FUEL_CHECKPOINT_MAGIC) {
        rt->state = FUEL_STATE_READY;
        return FUEL_CKPT_RESTORE_NONE;
    }

    if (!cp.session_active) {
        if (cp.dispensed_liter_x100 > 0u || cp.pulse_count > 0u) {
            rt->txn_id = cp.txn_id;
            rt->target_session_liter_x100 = cp.target_session_liter_x100;
            rt->target_pulse = cp.target_pulse;
            rt->dispensed_liter_x100 = cp.dispensed_liter_x100;
            rt->pulse_snap = cp.pulse_count;
            rt->state = FUEL_STATE_READY;
            rt->session_active = 0u;
            LOG_INFO("FUEL", "display restored Add=%lu.%02lu c=%lu",
                     (unsigned long)(rt->dispensed_liter_x100 / 100u),
                     (unsigned long)(rt->dispensed_liter_x100 % 100u),
                     (unsigned long)rt->pulse_snap);
            return FUEL_CKPT_RESTORE_DISPLAY;
        }
        rt->state = FUEL_STATE_READY;
        return FUEL_CKPT_RESTORE_NONE;
    }

    rt->txn_id = cp.txn_id;
    rt->target_session_liter_x100 = cp.target_session_liter_x100;
    rt->dispensed_liter_x100 = cp.dispensed_liter_x100;
    rt->target_pulse = cp.target_pulse;
    rt->session_active = 1u;
    rt->state = FUEL_STATE_FUELING;
    rt->run_sub = (fuel_run_substate_t)cp.saved_sub;
    rt->pause_reason = (fuel_pause_reason_t)cp.pause_reason;
    rt->session_start_rtc = cp.rtc_counter;

    s_pulse_base = cp.pulse_count;
    (void)FuelMeter_ResetCount();
    rt->pulse_snap = cp.pulse_count;

    s_ring_last_whole_liter = rt->dispensed_liter_x100 / 100u;

    rt->pump_running = 0u;
    fuel_valve_set(0u);

    if (mc->pause_on_powerup_active &&
        cp.saved_state == (uint8_t)FUEL_STATE_FUELING &&
        cp.saved_sub == (uint8_t)FUEL_RUN_ACTIVE) {
        rt->run_sub = FUEL_RUN_PAUSED;
        rt->pause_reason = FUEL_PAUSE_POWER;
        LOG_INFO("FUEL", "power-up: force pause, green short to resume");
    }

    LOG_INFO("FUEL", "checkpoint restored txn=%lu %.2lu/%.2lu L sub=%u",
             (unsigned long)rt->txn_id,
             (unsigned long)(rt->dispensed_liter_x100 / 100u),
             (unsigned long)(rt->target_session_liter_x100 / 100u),
             (unsigned int)rt->run_sub);
    return FUEL_CKPT_RESTORE_ACTIVE;
}

error_code_t FuelApp_Init(void)
{
    fuel_runtime_t *rt;
    error_code_t ret;
    uint8_t freq_hz;
    uint8_t ckpt_kind;

    if (s_inited) {
        return FUEL_APP_OK;
    }

    rt = Fuel_RuntimeMutable();
    memset(rt, 0, sizeof(*rt));

    fuel_valve_init();
    (void)FuelStore_Init();
    (void)fuel_ring_init();

    ret = FuelStore_LoadUser(&rt->user_cache);
    if (ret != FUEL_STORE_OK) {
        return ret;
    }
    ret = FuelStore_LoadMachine(&rt->machine_cache);
    if (ret != FUEL_STORE_OK) {
        return ret;
    }

#if defined(FUEL_OGM_STALL_TIMEOUT_MS)
    rt->machine_cache.ogm_stall_timeout_ms = FUEL_OGM_STALL_TIMEOUT_MS;
#endif

    rt->target_next_liter_x100 = rt->user_cache.target_next_liter_x100;
    if (rt->target_next_liter_x100 == 0u) {
        rt->target_next_liter_x100 = rt->machine_cache.factory_target_next_liter_x100;
    }

    ret = FuelMeter_Init();
    if (ret != FUEL_METER_OK) {
        return ret;
    }

    ret = FuelKey_Init();
    if (ret != FUEL_KEY_OK) {
        return ret;
    }

#if defined(PRESET_STAGE6_CLEAR_CKPT_ON_BOOT) && PRESET_STAGE6_CLEAR_CKPT_ON_BOOT
    (void)FuelStore_ClearCheckpoint();
    rt->target_next_liter_x100 = FUEL_DEFAULT_TARGET_NEXT_X100;
    rt->user_cache.target_next_liter_x100 = FUEL_DEFAULT_TARGET_NEXT_X100;
    rt->machine_cache.hose_comp_liter_x100 = FUEL_HOSE_COMP_LITER_X100_DEFAULT;
    LOG_INFO("FUEL", "boot: tgt 20.00L comp 0.55L");
#endif

    freq_hz = (uint8_t)(rt->machine_cache.vfd_run_freq_x100 / 100u);
    ret = FuelPump_Init(freq_hz);
    if (ret != FUEL_PUMP_OK) {
        LOG_WARN("FUEL", "pump init warn %d", (int)ret);
    }

    ckpt_kind = fuel_restore_checkpoint(rt);

    if (!rt->session_active) {
        const fuel_machine_config_t *mc = &rt->machine_cache;

        rt->target_session_liter_x100 = rt->target_next_liter_x100;
        rt->target_pulse = Fuel_PulseTargetFromLiterX100(rt->target_session_liter_x100,
                                                         mc->pulses_per_liter,
                                                         mc->hose_comp_liter_x100);
        if (ckpt_kind != FUEL_CKPT_RESTORE_DISPLAY) {
            rt->pulse_snap = 0u;
            rt->dispensed_liter_x100 = 0u;
        }
        rt->state = FUEL_STATE_READY;
    }
    s_fault_clear_ms = Delay_GetTick();
    s_inited = 1u;
    s_oled_dirty = 1u;

    LOG_INFO("FUEL", "init OK tgt=%lu.%02luL stop@%lu pls",
             (unsigned long)(rt->target_session_liter_x100 / 100u),
             (unsigned long)(rt->target_session_liter_x100 % 100u),
             (unsigned long)rt->target_pulse);
    return FUEL_APP_OK;
}

error_code_t FuelApp_SetTargetNextLiterX100(uint32_t liter_x100)
{
    fuel_runtime_t *rt;
    const fuel_machine_config_t *mc;
    error_code_t ret;

    if (!s_inited) {
        return FUEL_APP_ERROR_NOT_INIT;
    }

    rt = Fuel_RuntimeMutable();
    mc = &rt->machine_cache;

    if (!Fuel_IsLiterX100InRange(liter_x100, FUEL_MIN_TARGET_LITER_X100,
                                 mc->max_target_liter_x100)) {
        return FUEL_APP_ERROR_INVALID_PARAM;
    }

    rt->target_next_liter_x100 = liter_x100;
    rt->user_cache.target_next_liter_x100 = liter_x100;
    ret = FuelStore_SaveUser(&rt->user_cache);
    if (ret != FUEL_STORE_OK) {
        return FUEL_APP_ERROR_STORAGE;
    }

    if (!rt->session_active) {
        rt->target_session_liter_x100 = liter_x100;
        rt->target_pulse = Fuel_PulseTargetFromLiterX100(liter_x100,
                                                         mc->pulses_per_liter,
                                                         mc->hose_comp_liter_x100);
    }

    LOG_INFO("FUEL", "target_next=%lu.%02lu L",
             (unsigned long)(liter_x100 / 100u),
             (unsigned long)(liter_x100 % 100u));
    s_oled_dirty = 1u;
    return FUEL_APP_OK;
}

error_code_t FuelApp_ProcessKeyEvent(fuel_key_event_t evt)
{
    fuel_runtime_t *rt;

    if (!s_inited) {
        return FUEL_APP_ERROR_NOT_INIT;
    }
    if (evt == FUEL_KEY_EVT_NONE) {
        return FUEL_APP_OK;
    }

    rt = Fuel_RuntimeMutable();
    fuel_handle_key(rt, evt);
    return FUEL_APP_OK;
}

static void fuel_ensure_pump_stopped(fuel_runtime_t *rt)
{
    uint8_t should_run;

    if (rt == NULL) {
        return;
    }

    should_run = (rt->session_active &&
                  rt->run_sub == FUEL_RUN_ACTIVE &&
                  rt->state != FUEL_STATE_FAULT) ? 1u : 0u;
    if (should_run) {
        return;
    }

    if (FuelPump_IsRunning() || rt->pump_running || rt->valve_open) {
        fuel_schedule_pump_stop();
        fuel_valve_set(0u);
        rt->pump_running = 0u;
        rt->valve_open = 0u;
        s_oled_dirty = 1u;
    }
}

static void fuel_sync_pump_state(fuel_runtime_t *rt)
{
    if (rt == NULL || !rt->session_active) {
        return;
    }

    if (FuelPump_IsRunning()) {
        if (!rt->pump_running) {
            rt->pump_running = 1u;
            s_oled_dirty = 1u;
        }
        if (rt->state == FUEL_STATE_READY) {
            rt->state = FUEL_STATE_FUELING;
            rt->run_sub = FUEL_RUN_ACTIVE;
            s_oled_dirty = 1u;
        }
    }
}

error_code_t FuelApp_Tick(uint32_t now_ms)
{
    fuel_runtime_t *rt;
    fuel_key_event_t evt;
    const fuel_machine_config_t *mc;

    if (!s_inited) {
        return FUEL_APP_ERROR_NOT_INIT;
    }

    if (now_ms == 0u) {
        now_ms = Delay_GetTick();
    }

    (void)FuelKey_Poll(now_ms);

    rt = Fuel_RuntimeMutable();
    mc = &rt->machine_cache;

    if (!rt->session_active) {
        fuel_idle_ogm_guard();
    }

    (void)FuelMeter_Task(now_ms);
    fuel_sync_dispensed(rt);
    fuel_sync_pump_state(rt);

    evt = FuelKey_ConsumeEvent();
    while (evt != FUEL_KEY_EVT_NONE) {
        fuel_handle_key(rt, evt);
        evt = FuelKey_ConsumeEvent();
    }

    fuel_ensure_pump_stopped(rt);

    now_ms = Delay_GetTick();
    fuel_service_pump_stop(now_ms);

    fuel_check_auto_stop(rt);
    fuel_check_faults(rt, now_ms);

    if (rt->session_active && rt->state == FUEL_STATE_FUELING) {
        if (rt->checkpoint_dirty ||
            Delay_GetElapsed(now_ms, rt->last_checkpoint_ms) >= mc->checkpoint_interval_ms) {
            (void)fuel_save_checkpoint(rt);
        }
    }

    return FUEL_APP_OK;
}

uint8_t FuelApp_IsInited(void)
{
    return s_inited;
}

uint8_t FuelApp_NeedOledRefresh(void)
{
    return s_oled_dirty;
}

void FuelApp_ClearOledDirty(void)
{
    s_oled_dirty = 0u;
}
