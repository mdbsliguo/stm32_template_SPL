/**
 * @file storage_guard.h
 * @brief 存储容错守卫：Mount/IO/sync 失败时降级，避免业务挂死
 */

#ifndef STORAGE_GUARD_H
#define STORAGE_GUARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include "config.h"
#include <stdint.h>

#ifdef CONFIG_MODULE_STORAGE_GUARD_ENABLED
#if CONFIG_MODULE_STORAGE_GUARD_ENABLED

#ifdef CONFIG_MODULE_LITTLEFS_ENABLED
#if CONFIG_MODULE_LITTLEFS_ENABLED
#include "littlefs_wrapper.h"
#endif
#endif

/** Mount 耗时超过该阈值则进入 DEGRADED（毫秒） */
#ifndef STORAGE_GUARD_MOUNT_TIMEOUT_MS
#define STORAGE_GUARD_MOUNT_TIMEOUT_MS      3000u
#endif

/**
 * @brief 存储守卫运行状态
 */
typedef enum {
    STORAGE_GUARD_STATE_OFFLINE = 0,   /**< W25Q 不可用或未初始化 */
    STORAGE_GUARD_STATE_DEGRADED,      /**< Flash/LFS 异常，禁止写盘 */
    STORAGE_GUARD_STATE_READY          /**< 可正常读写 */
} storage_guard_state_t;

#define STORAGE_GUARD_OK                    ERROR_OK
#define STORAGE_GUARD_ERROR_NOT_INIT        (ERROR_BASE_STORAGE_GUARD - 1)
#define STORAGE_GUARD_ERROR_INVALID_PARAM   (ERROR_BASE_STORAGE_GUARD - 2)
#define STORAGE_GUARD_ERROR_OFFLINE         (ERROR_BASE_STORAGE_GUARD - 3)
#define STORAGE_GUARD_ERROR_DEGRADED        (ERROR_BASE_STORAGE_GUARD - 4)
#define STORAGE_GUARD_ERROR_W25Q            (ERROR_BASE_STORAGE_GUARD - 5)
#define STORAGE_GUARD_ERROR_LITTLEFS        (ERROR_BASE_STORAGE_GUARD - 6)

error_code_t StorageGuard_Init(void);
error_code_t StorageGuard_Shutdown(void);
error_code_t StorageGuard_TryRecover(void);

storage_guard_state_t StorageGuard_GetState(void);
uint8_t StorageGuard_IsReady(void);
uint32_t StorageGuard_GetLastMountMs(void);

error_code_t StorageGuard_NotifyLfsError(int32_t lfs_status);

const char *StorageGuard_StateName(storage_guard_state_t state);

error_code_t StorageGuard_SetTestSkipW25q(uint8_t skip);
error_code_t StorageGuard_SetTestForceMountFail(uint8_t enable,
                                                LittleFS_Status_t fake_status);

#endif /* CONFIG_MODULE_STORAGE_GUARD_ENABLED */
#endif /* CONFIG_MODULE_STORAGE_GUARD_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_GUARD_H */
