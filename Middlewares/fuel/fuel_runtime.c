/**
 * @file fuel_runtime.c
 * @brief 定量加油运行态辅助（按键条件判断）
 */

#include "fuel_runtime.h"
#include "fuel_pump.h"
#include <stddef.h>

static fuel_runtime_t s_fuel_rt;

fuel_ui_phase_t Fuel_GetUiPhase(const fuel_runtime_t *rt)
{
    if (rt == NULL) {
        return FUEL_UI_READY;
    }
    if (rt->session_active) {
        if (rt->state == FUEL_STATE_FAULT) {
            return FUEL_UI_FAULT;
        }
        if (rt->run_sub == FUEL_RUN_PAUSED) {
            return FUEL_UI_PAUSE;
        }
        return FUEL_UI_RUN;
    }
    /* 到量结束会话后停泵失败，或会话未建立但泵在转 */
    if (FuelPump_IsRunning() || rt->pump_running || rt->valve_open) {
        return FUEL_UI_RUN;
    }
    return FUEL_UI_READY;
}

const fuel_runtime_t *Fuel_GetRuntime(void)
{
    return &s_fuel_rt;
}

fuel_runtime_t *Fuel_RuntimeMutable(void)
{
    return &s_fuel_rt;
}

uint8_t Fuel_CanStartNewSession(const fuel_runtime_t *rt)
{
    if (rt == NULL) {
        return 0u;
    }
    return (rt->state == FUEL_STATE_READY && !rt->session_active) ? 1u : 0u;
}

uint8_t Fuel_CanPause(const fuel_runtime_t *rt)
{
    if (rt == NULL) {
        return 0u;
    }
    if (rt->session_active && rt->run_sub == FUEL_RUN_ACTIVE) {
        if (rt->state == FUEL_STATE_FAULT) {
            return (rt->pump_running || FuelPump_IsRunning()) ? 1u : 0u;
        }
        return 1u;
    }
    /* 无会话但泵仍在转：红键短按作紧急停泵 */
    if (FuelPump_IsRunning() || rt->pump_running || rt->valve_open) {
        return 1u;
    }
    return 0u;
}

uint8_t Fuel_CanResume(const fuel_runtime_t *rt)
{
    if (rt == NULL) {
        return 0u;
    }
    if (!rt->session_active || rt->run_sub != FUEL_RUN_PAUSED) {
        return 0u;
    }
    return (rt->state == FUEL_STATE_FUELING || rt->state == FUEL_STATE_FAULT) ? 1u : 0u;
}

uint8_t Fuel_CanStopSession(const fuel_runtime_t *rt)
{
    if (rt == NULL) {
        return 0u;
    }
    return (rt->state == FUEL_STATE_FUELING) ? 1u : 0u;
}
