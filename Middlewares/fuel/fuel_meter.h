/**
 * @file fuel_meter.h
 * @brief OGM flow meter wrapper (NPN06 algo)
 */

#ifndef FUEL_METER_H
#define FUEL_METER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include <stdint.h>

#define FUEL_METER_OK                       ERROR_OK
#define FUEL_METER_ERROR_NOT_INIT           (ERROR_BASE_FUEL - 20)
#define FUEL_METER_ERROR_INVALID_PARAM      (ERROR_BASE_FUEL - 21)

#ifndef FUEL_METER_RATE_WINDOW_MS
#define FUEL_METER_RATE_WINDOW_MS           1000u
#endif

#ifndef FUEL_METER_FLOW_IDLE_MS
#define FUEL_METER_FLOW_IDLE_MS             2000u
#endif

error_code_t FuelMeter_Init(void);
error_code_t FuelMeter_Task(uint32_t now_ms);

uint32_t FuelMeter_GetCount(void);
uint32_t FuelMeter_GetDeltaPerSec(void);
uint8_t FuelMeter_IsFlowActive(void);

error_code_t FuelMeter_ResetCount(void);

#ifdef __cplusplus
}
#endif

#endif /* FUEL_METER_H */
