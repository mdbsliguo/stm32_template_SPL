/**
 * @file system_init.h
 * @brief 系统初始化框架（统一入口）
 * @version 1.0.0
 * @date 2024-01-01
 * @details 提供统一的系统初始化流程，按顺序初始化各个模块
 */

#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H

#include "error_code.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 系统初始化错误码
 */
typedef enum {
    SYS_INIT_OK = ERROR_OK,                              /**< 初始化成功 */
    SYS_INIT_ERROR_CLOCK = ERROR_BASE_SYSTEM - 1,       /**< 时钟初始化失败 */
    SYS_INIT_ERROR_SYSTICK = ERROR_BASE_SYSTEM - 2,      /**< SysTick初始化失败 */
    SYS_INIT_ERROR_BSP = ERROR_BASE_SYSTEM - 3,          /**< BSP初始化失败 */
    SYS_INIT_ERROR_DRIVER = ERROR_BASE_SYSTEM - 4,      /**< 驱动初始化失败 */
} sys_init_error_t;

/**
 * @brief 系统初始化（统一入口）
 * @return 错误码
 * @note 按顺序初始化：
 *       1. 系统时钟（已在启动文件中初始化）
 *       2. SysTick延时模块
 *       3. BSP层（板级配置）
 *       4. 驱动层（GPIO、LED等）
 */
sys_init_error_t System_Init(void);

/**
 * @brief 系统反初始化
 * @return 错误码
 * @note 按相反顺序反初始化各个模块
 */
sys_init_error_t System_Deinit(void);

/**
 * @brief 获取系统初始化状态
 * @return true-已初始化，false-未初始化
 */
uint8_t System_IsInitialized(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_INIT_H */

