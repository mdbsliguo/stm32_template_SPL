/**
 * @file fuel_pump.h
 * @brief GD200A VFD pump control via ModBus RTU (NPN07)
 */

#ifndef FUEL_PUMP_H
#define FUEL_PUMP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include <stdint.h>

#define FUEL_PUMP_OK                        ERROR_OK
#define FUEL_PUMP_ERROR_NOT_INIT            (ERROR_BASE_FUEL - 40)
#define FUEL_PUMP_ERROR_COMM                (ERROR_BASE_FUEL - 41)
#define FUEL_PUMP_ERROR_BUSY                (ERROR_BASE_FUEL - 42)
#define FUEL_PUMP_ERROR_INVALID_PARAM       (ERROR_BASE_FUEL - 43)

#ifndef FUEL_PUMP_DEFAULT_FREQ_HZ
#define FUEL_PUMP_DEFAULT_FREQ_HZ           10u
#endif

error_code_t FuelPump_Init(uint8_t standby_freq_hz);
error_code_t FuelPump_SetStandbyFreq(uint8_t freq_hz);

uint8_t FuelPump_IsCommOk(void);
uint8_t FuelPump_IsRunning(void);
uint8_t FuelPump_IsBusy(void);

error_code_t FuelPump_ApplyFrequency(uint8_t freq_hz);
error_code_t FuelPump_Start(uint8_t freq_hz);
error_code_t FuelPump_Stop(void);
/** 中止当前 ModBus 并多次重发停机，用于暂停/到量/红键紧急停泵 */
error_code_t FuelPump_ForceStop(void);

#ifdef __cplusplus
}
#endif

#endif /* FUEL_PUMP_H */
