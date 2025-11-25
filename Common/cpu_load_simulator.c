/**
 * @file cpu_load_simulator.c
 * @brief CPU负载模拟器实现（用于测试和调试CPU负载统计功能）
 * @version 1.0.0
 * @date 2024-01-01
 */

#include "cpu_load_simulator.h"
#include "delay.h"
#include "clock_manager.h"

/**
 * @brief 模拟CPU高负载（60-70%使用率，每次50ms，不阻塞）
 * @note 每次调用只模拟50ms，需要循环调用
 * @note 用于在主循环中使用，不会阻塞主循环
 */
void CPU_SimulateHighLoad50ms(void)
{
    uint32_t end_tick = Delay_GetTick() + 50;
    
    while (Delay_GetTick() < end_tick)
    {
        /* 60-70%的时间：执行空循环（高负载） */
        /* 在空循环执行前调用忙碌钩子，表示开始高负载工作 */
        CLKM_BusyHook();
        volatile uint32_t i;
        for (i = 0; i < 50000; i++)
            ; /* 约占用60-70%的时间 */
        /* 空循环执行后再次调用忙碌钩子，确保统计到高负载 */
        CLKM_BusyHook();

        /* 30-40%的时间：调用空闲钩子（低负载） */
        CLKM_IdleHook();
        Delay_us(100); /* 短暂延时，避免完全占用CPU */
    }
}




