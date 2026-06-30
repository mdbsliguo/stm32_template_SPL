/**
 * @file fuel_meter.c
 * @brief OGM flow meter wrapper (NPN06)
 */

#include "fuel_meter.h"
#include "ogm_flow_ic.h"
#include "delay.h"
#include "config.h"

static uint32_t s_count_snap;
static uint32_t s_count_delta_1s;
static uint32_t s_display_delta_1s;
static uint32_t s_last_task_ms;
static uint8_t  s_inited;

error_code_t FuelMeter_Init(void)
{
    error_code_t ret;

    if (s_inited) {
        return FUEL_METER_OK;
    }

    ret = OGM_FlowIC_Init();
    if (ret != OGM_FLOW_IC_OK) {
        return ret;
    }

    s_count_snap = 0u;
    s_count_delta_1s = 0u;
    s_display_delta_1s = 0u;
    s_last_task_ms = Delay_GetTick();
    s_inited = 1u;
    return FUEL_METER_OK;
}

static void fuel_meter_update_delta(void)
{
    uint32_t now;
    uint32_t delta;
    error_code_t ret;

    ret = OGM_FlowIC_GetCount(&now);
    if (ret != OGM_FLOW_IC_OK) {
        return;
    }

    delta = now - s_count_snap;
    s_count_snap = now;
    s_count_delta_1s += delta;
}

error_code_t FuelMeter_Task(uint32_t now_ms)
{
    uint32_t elapsed;
    uint32_t delta_snap;

    if (!s_inited) {
        return FUEL_METER_ERROR_NOT_INIT;
    }

    fuel_meter_update_delta();

    elapsed = now_ms - s_last_task_ms;
    if (elapsed >= FUEL_METER_RATE_WINDOW_MS) {
        delta_snap = s_count_delta_1s;
        s_count_delta_1s = 0u;
        if (elapsed > 0u) {
            s_display_delta_1s = (delta_snap * 1000u) / elapsed;
        }
        s_last_task_ms = now_ms;
    }

    (void)OGM_FlowIC_Process100ms();
    return FUEL_METER_OK;
}

uint32_t FuelMeter_GetCount(void)
{
    uint32_t snap;
    error_code_t ret;

    if (!s_inited) {
        return 0u;
    }

    ret = OGM_FlowIC_GetCount(&snap);
    if (ret != OGM_FLOW_IC_OK) {
        return 0u;
    }
    return snap;
}

uint32_t FuelMeter_GetDeltaPerSec(void)
{
    return s_display_delta_1s;
}

uint8_t FuelMeter_IsFlowActive(void)
{
    uint8_t active;
    error_code_t ret;

    if (!s_inited) {
        return 0u;
    }

    ret = OGM_FlowIC_IsFlowActive(FUEL_METER_FLOW_IDLE_MS, &active);
    if (ret != OGM_FLOW_IC_OK) {
        return 0u;
    }
    return active;
}

error_code_t FuelMeter_ResetCount(void)
{
    error_code_t ret;

    if (!s_inited) {
        return FUEL_METER_ERROR_NOT_INIT;
    }

    ret = OGM_FlowIC_ResetCount();
    s_count_snap = 0u;
    s_count_delta_1s = 0u;
    s_display_delta_1s = 0u;
    return ret;
}
