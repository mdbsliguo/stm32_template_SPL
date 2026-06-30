/**
 * @file fuel_key.h
 * @brief PA4/PA5 EXTI keys: short/long on fall (long at hold>=LONG_MS)
 */

#ifndef FUEL_KEY_H
#define FUEL_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include "fuel_types.h"
#include <stdint.h>

#define FUEL_KEY_OK                         ERROR_OK
#define FUEL_KEY_ERROR_NOT_INIT             (ERROR_BASE_FUEL - 30)

#ifndef FUEL_KEY_LONG_PRESS_MS
/** 长按判定：自下降沿起按住 >= 此值（ms）触发 LONG */
#define FUEL_KEY_LONG_PRESS_MS              2000u
#endif

#ifndef FUEL_KEY_USE_EXTI
#define FUEL_KEY_USE_EXTI                   1u
#endif

error_code_t FuelKey_Init(void);
error_code_t FuelKey_Poll(uint32_t now_ms);
fuel_key_event_t FuelKey_ConsumeEvent(void);

uint8_t FuelKey_IsGreenDown(void);
uint8_t FuelKey_IsRedDown(void);

/** PA4=line4 PA5=line5，由 Core/stm32f10x_it.c 调用 */
void FuelKey_EXTI_IRQHandler(uint8_t exti_line);

#ifdef __cplusplus
}
#endif

#endif /* FUEL_KEY_H */
