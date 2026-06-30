/**
 * @file fuel_app.h
 * @brief Fuel dispensing state machine (stage 6)
 */

#ifndef FUEL_APP_H
#define FUEL_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include "fuel_types.h"
#include <stdint.h>

#define FUEL_APP_OK                         ERROR_OK
#define FUEL_APP_ERROR_NOT_INIT             (ERROR_BASE_FUEL - 10)
#define FUEL_APP_ERROR_INVALID_PARAM        (ERROR_BASE_FUEL - 11)
#define FUEL_APP_ERROR_STORAGE              (ERROR_BASE_FUEL - 12)

error_code_t FuelApp_Init(void);
error_code_t FuelApp_Tick(uint32_t now_ms);
error_code_t FuelApp_ProcessKeyEvent(fuel_key_event_t evt);

/** 设置新一轮目标升数（liter_x100），写 user.dat；Ready 时同步 OLED 行 Tgt */
error_code_t FuelApp_SetTargetNextLiterX100(uint32_t liter_x100);

uint8_t FuelApp_IsInited(void);
uint8_t FuelApp_NeedOledRefresh(void);
void FuelApp_ClearOledDirty(void);

#ifdef __cplusplus
}
#endif

#endif /* FUEL_APP_H */
