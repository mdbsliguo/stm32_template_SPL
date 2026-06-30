/**
 * @file fuel_runtime.h
 * @brief 定量加油运行态 RAM 结构（状态机唯一权威数据源）
 */

#ifndef FUEL_RUNTIME_H
#define FUEL_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include "fuel_types.h"
#include "fuel_persist.h"
#include <stdint.h>

#define FUEL_RUNTIME_OK                     ERROR_OK
#define FUEL_RUNTIME_ERROR_NOT_INIT         (ERROR_BASE_FUEL - 1)
#define FUEL_RUNTIME_ERROR_INVALID_PARAM    (ERROR_BASE_FUEL - 2)
#define FUEL_RUNTIME_ERROR_INVALID_STATE    (ERROR_BASE_FUEL - 3)
#define FUEL_RUNTIME_ERROR_NOT_IMPLEMENTED  (ERROR_BASE_FUEL - 4)

/**
 * @brief 运行态（仅 RAM；持久化通过 user/checkpoint 模块同步）
 */
typedef struct {
    fuel_state_t        state;
    fuel_run_substate_t run_sub;

    uint32_t txn_id;

    /** 新一轮加注量（镜像 user.dat，本笔进行中可改，仅影响下一笔） */
    uint32_t target_next_liter_x100;
    /** 本笔目标加注量（开笔时从 target_next 拷贝，断电由 checkpoint 恢复） */
    uint32_t target_session_liter_x100;
    /** 本笔已加升数 liter_x100（2 位小数） */
    uint32_t dispensed_liter_x100;

    uint32_t target_pulse;
    volatile uint32_t pulse_isr;
    uint32_t pulse_snap;

    uint32_t fault_code;
    uint32_t fault_latched;

    uint8_t  valve_open;
    uint8_t  pump_running;
    fuel_pause_reason_t pause_reason;

    uint8_t  session_active;
    uint8_t  checkpoint_dirty;
    uint32_t last_checkpoint_ms;
    uint32_t session_start_rtc;

    fuel_key_event_t key_event;

    fuel_user_settings_t user_cache;
    fuel_machine_config_t machine_cache;
    fuel_checkpoint_t checkpoint_cache;
} fuel_runtime_t;

/** OLED/串口显示用阶段（结合 session、pump_running、变频器实际状态） */
typedef enum {
    FUEL_UI_READY = 0,
    FUEL_UI_RUN,
    FUEL_UI_PAUSE,
    FUEL_UI_FAULT
} fuel_ui_phase_t;

const fuel_runtime_t *Fuel_GetRuntime(void);

fuel_runtime_t *Fuel_RuntimeMutable(void);

uint8_t Fuel_CanStartNewSession(const fuel_runtime_t *rt);
uint8_t Fuel_CanPause(const fuel_runtime_t *rt);
uint8_t Fuel_CanResume(const fuel_runtime_t *rt);
uint8_t Fuel_CanStopSession(const fuel_runtime_t *rt);
fuel_ui_phase_t Fuel_GetUiPhase(const fuel_runtime_t *rt);

#ifdef __cplusplus
}
#endif

#endif /* FUEL_RUNTIME_H */
