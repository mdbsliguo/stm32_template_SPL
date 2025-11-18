/**
 * @file delay.h
 * @brief 延时模块接口（阻塞式+非阻塞式，基于base_TIM2）
 * @version 2.0.0
 * @date 2024-01-01
 * @note 基于base_TIM2模块，确保频率变化时1秒永远是1秒
 */

#ifndef DELAY_H
#define DELAY_H

#include <stdint.h>

/**
 * @brief 初始化延时模块（基于base_TIM2）
 * @note 初始化base_TIM2（如果未初始化），然后计算阻塞式延时的系数
 * @note 阻塞式延时基于SysTick寄存器，非阻塞式延时基于base_TIM2
 */
void Delay_Init(void);

/**
 * @brief 重新配置延时模块（频率切换时调用）
 * @param[in] new_freq 新的系统频率（Hz）
 * @note 频率切换时，调用base_TIM2的重新配置，然后更新阻塞式延时的系数
 * @note base_TIM2会自动保持1ms中断间隔不变，确保1秒永远是1秒
 */
void Delay_Reconfig(uint32_t new_freq);

/**
 * @brief 微秒级精确延时（阻塞式，基于SysTick寄存器）
 * @param[in] us 延时微秒数，范围：0~1864135（@72MHz）
 * @warning 此函数为阻塞式，执行期间CPU不处理其他任务
 * @note 用于精确延时（<1ms），频率切换时自动适配
 */
void Delay_us(uint32_t us);

/**
 * @brief 毫秒级精确延时（阻塞式，基于SysTick寄存器）
 * @param[in] ms 延时毫秒数，范围：0~4294967295
 * @warning 此函数为阻塞式，执行期间CPU不处理其他任务
 * @note 用于精确延时（<100ms），频率切换时自动适配
 */
void Delay_ms(uint32_t ms);

/**
 * @brief 毫秒级非阻塞延时（基于base_TIM2，动态降频自适应）
 * @param[in] start_tick 开始时间（Delay_GetTick()的返回值）
 * @param[in] delay_ms 延时毫秒数
 * @return 1=延时完成，0=延时未完成
 * @note 基于base_TIM2，频率切换时自动适配（1秒永远是1秒）
 * @note 用于长时间延时（>100ms），不阻塞CPU
 * 
 * @example
 * uint32_t start = Delay_GetTick();
 * while (!Delay_ms_nonblock(start, 1000))  // 延时1秒
 * {
 *     // 执行其他任务
 * }
 */
uint8_t Delay_ms_nonblock(uint32_t start_tick, uint32_t delay_ms);

/**
 * @brief 获取当前tick（用于非阻塞延时）
 * @return 当前tick值（代表真实时间，毫秒）
 * @note 基于base_TIM2，频率切换时自动适配
 */
uint32_t Delay_GetTick(void);

/**
 * @brief 秒级延时（阻塞式，循环调用Delay_ms）
 * @param[in] s 延时秒数
 * @warning 长时间阻塞，实时性要求高的场景禁用
 * @note 建议使用Delay_ms_nonblock()实现非阻塞延时
 */
void Delay_s(uint32_t s);

/**
 * @brief 计算时间差（处理溢出）
 * @param[in] current_tick 当前tick值
 * @param[in] previous_tick 之前的tick值
 * @return 时间差（毫秒），如果previous_tick为0则返回UINT32_MAX
 * @note 自动处理tick溢出情况（32位无符号整数溢出）
 * @note 如果previous_tick为0，表示从未设置过，返回UINT32_MAX（表示已经过了很长时间）
 * 
 * @example
 * static uint32_t last_check_tick = 0;
 * uint32_t current_tick = Delay_GetTick();
 * uint32_t elapsed = Delay_GetElapsed(current_tick, last_check_tick);
 * if (elapsed >= 1000)  // 1秒到了
 * {
 *     // 执行任务
 *     last_check_tick = current_tick;
 * }
 */
uint32_t Delay_GetElapsed(uint32_t current_tick, uint32_t previous_tick);

#endif /* DELAY_H */
