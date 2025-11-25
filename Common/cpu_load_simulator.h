/**
 * @file cpu_load_simulator.h
 * @brief CPU负载模拟器（用于测试和调试CPU负载统计功能）
 * @version 1.0.0
 * @date 2024-01-01
 * @details 提供CPU负载模拟功能，用于测试时钟管理模块的CPU负载统计
 * 
 * @note 此模块仅用于测试和调试，不用于生产环境
 * @note 需要时钟管理模块支持（CLOCK_MANAGER_H）和空闲钩子功能（CLKM_IDLE_HOOK_ENABLE）
 */

#ifndef CPU_LOAD_SIMULATOR_H
#define CPU_LOAD_SIMULATOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 模拟CPU高负载（60-70%使用率，每次50ms，不阻塞）
 * @note ?? 使用技巧：
 *   1. 每次调用只模拟50ms，要模拟更长时间需要循环调用
 *   2. 必须在主循环中调用，不能只调用一次
 *   3. 必须配合 CLKM_AdaptiveTask() 使用（在主循环中定期调用）
 *   4. 空闲时必须调用 CLKM_IdleHook()，否则CPU负载统计不准确
 *   5. 需要时钟管理模块支持（CLOCK_MANAGER_H）和空闲钩子功能（CLKM_IDLE_HOOK_ENABLE）
 * 
 * @example
 * // 完整示例：每5秒，前2秒60%负载，后3秒空闲
 * // static uint32_t last_load_tick = 0;
 * // static uint8_t load_running = 0;
 * // uint32_t current_tick = Delay_GetTick();
 * // 
 * // if (!load_running)
 * // {
 * //     uint32_t elapsed = (current_tick >= last_load_tick) ? 
 * //                       (current_tick - last_load_tick) : 
 * //                       (0xFFFFFFFF - last_load_tick + current_tick + 1);
 * //     if (elapsed >= 5000)  // 5秒到了
 * //     {
 * //         load_running = 1;
 * //         last_load_tick = current_tick;
 * //     }
 * //     else
 * //     {
 * //         CLKM_IdleHook();  // ?? 空闲时必须调用
 * //     }
 * // }
 * // else
 * // {
 * //     uint32_t elapsed = (current_tick >= last_load_tick) ? 
 * //                       (current_tick - last_load_tick) : 
 * //                       (0xFFFFFFFF - last_load_tick + current_tick + 1);
 * //     if (elapsed >= 2000)  // 2秒到了
 * //     {
 * //         load_running = 0;
 * //         last_load_tick = current_tick;
 * //     }
 * //     else
 * //     {
 * //         CPU_SimulateHighLoad50ms();  // 循环调用，每次50ms
 * //     }
 * // }
 * // CLKM_AdaptiveTask();  // ?? 必须调用，用于计算CPU负载
 */
void CPU_SimulateHighLoad50ms(void);


#ifdef __cplusplus
}
#endif

#endif /* CPU_LOAD_SIMULATOR_H */



