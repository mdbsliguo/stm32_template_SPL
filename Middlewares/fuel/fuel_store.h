/**
 * @file fuel_store.h
 * @brief LittleFS persistence for user/machine/checkpoint
 */

#ifndef FUEL_STORE_H
#define FUEL_STORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include "fuel_persist.h"
#include <stdint.h>

#define FUEL_STORE_OK                       ERROR_OK
#define FUEL_STORE_ERROR_NOT_INIT           (ERROR_BASE_FUEL - 50)
#define FUEL_STORE_ERROR_INVALID_PARAM      (ERROR_BASE_FUEL - 51)
#define FUEL_STORE_ERROR_IO                 (ERROR_BASE_FUEL - 52)
#define FUEL_STORE_ERROR_STORAGE            (ERROR_BASE_FUEL - 53)

#define FUEL_STORE_PATH_USER                "config/user.dat"
#define FUEL_STORE_PATH_MACHINE             "config/machine.dat"
#define FUEL_STORE_PATH_CHECKPOINT          "session/checkpoint.dat"

error_code_t FuelStore_Init(void);
void FuelStore_FillMachineDefaults(fuel_machine_config_t *mc);

error_code_t FuelStore_LoadUser(fuel_user_settings_t *out);
error_code_t FuelStore_SaveUser(const fuel_user_settings_t *in);

error_code_t FuelStore_LoadMachine(fuel_machine_config_t *out);
error_code_t FuelStore_SaveMachine(const fuel_machine_config_t *in);

error_code_t FuelStore_LoadCheckpoint(fuel_checkpoint_t *out);
error_code_t FuelStore_SaveCheckpoint(const fuel_checkpoint_t *in);
error_code_t FuelStore_ClearCheckpoint(void);

uint16_t FuelStore_Crc16(const void *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* FUEL_STORE_H */
