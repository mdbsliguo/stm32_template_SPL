/**
 * @file system_init.h
 * @brief 系统初始化框架（统一入口）
 * @version 1.0.0
 * @date 2024-01-01
 * @details 提供统一的系统初始化流程，按顺序初始化各个模块
 * 
 * @section 使用方法
 * 
 * @subsection 基本使用
 * 在main()函数开始处调用System_Init()进行系统初始化：
 * @code
 * int main(void)
 * {
 *     // 系统初始化（必须最先调用）
 *     sys_init_error_t err = System_Init();
 *     if (err != SYS_INIT_OK)
 *     {
 *         // 初始化失败，处理错误
 *         while(1);
 *     }
 *     
 *     // 其他模块初始化（可选）
 *     // ...
 *     
 *     // 主循环
 *     while(1)
 *     {
 *         // 业务逻辑
 *     }
 * }
 * @endcode
 * 
 * @subsection 初始化顺序
 * System_Init()按以下顺序初始化模块：
 * 1. TIM2_TimeBase（时间基准，1ms中断）
 * 2. Delay（延时模块，基于TIM2_TimeBase）
 * 3. TIM_SW（软件定时器模块，基于TIM2_TimeBase，可选）
 * 4. GPIO（GPIO驱动，使用时自动使能时钟）
 * 5. LED（LED驱动，如果启用）
 * 
 * @subsection 检查初始化状态
 * 使用System_IsInitialized()检查系统是否已初始化：
 * @code
 * if (System_IsInitialized())
 * {
 *     // 系统已初始化，可以安全使用其他模块
 * }
 * @endcode
 * 
 * @subsection 反初始化
 * 通常不需要反初始化，但在特殊场景下可以使用：
 * @code
 * System_Deinit();  // 按相反顺序反初始化各个模块
 * @endcode
 * 
 * @section 注意事项
 * 
 * 1. **必须最先调用**：System_Init()必须在main()开始处调用，其他模块依赖系统初始化
 * 2. **不要跳过**：不要跳过系统初始化，否则其他模块可能无法正常工作
 * 3. **重复初始化**：重复调用System_Init()是安全的，会自动检测并跳过
 * 4. **模块依赖**：系统初始化会按正确顺序初始化依赖的模块，无需手动管理
 * 5. **配置驱动**：模块开关通过System/config.h中的CONFIG_MODULE_XXX_ENABLED控制
 * 
 * @section 配置说明
 * 
 * 模块开关在System/config.h中配置：
 * - CONFIG_MODULE_SYSTEM_INIT_ENABLED：系统初始化模块开关（默认启用）
 * - CONFIG_MODULE_DELAY_ENABLED：延时模块开关（默认启用）
 * - CONFIG_MODULE_BASE_TIMER_ENABLED：TIM2时间基准模块开关（默认启用）
 * - CONFIG_MODULE_TIM_SW_ENABLED：软件定时器模块开关（默认启用）
 * - CONFIG_MODULE_LED_ENABLED：LED模块开关（默认启用）
 * 
 * @section 相关模块
 * 
 * - TIM2_TimeBase：时间基准模块（Drivers/timer/TIM2_TimeBase.c/h）
 * - Delay：延时模块（System/delay.c/h）
 * - TIM_SW：软件定时器模块（System/TIM_sw.c/h）
 * - GPIO：GPIO驱动（Drivers/basic/gpio.c/h）
 * - LED：LED驱动（Drivers/basic/led.c/h）
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

