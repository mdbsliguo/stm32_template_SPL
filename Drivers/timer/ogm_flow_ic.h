/**
 * @file ogm_flow_ic.h
 * @brief OGM 双脉冲线流量计 — 单定时器双通道输入捕获 + A/B 交替状态机
 * @details TIM CH1(A) / CH2(B) 输入捕获；默认下降沿 A/B 交替，可选四边沿互锁
 */

#ifndef OGM_FLOW_IC_H
#define OGM_FLOW_IC_H

#include "error_code.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OGM 输入捕获定时器实例
 */
typedef enum {
    OGM_FLOW_IC_TIM2 = 0,   /**< TIM2（默认 PA0/PA1，与系统时基共用） */
    OGM_FLOW_IC_TIM3 = 1,   /**< TIM3 */
    OGM_FLOW_IC_TIM4 = 2,   /**< TIM4（PB6/PB7） */
    OGM_FLOW_IC_MAX
} OGM_FlowIC_Instance_t;

/**
 * @brief 状态机：等待 A 线 / 等待 B 线
 */
typedef enum {
    OGM_FLOW_IC_STATE_WAIT_A = 0,
    OGM_FLOW_IC_STATE_WAIT_B = 1,
} OGM_FlowIC_State_t;

/**
 * @brief OGM FlowIC 错误码（基值 ERROR_BASE_TIMER）
 */
typedef enum {
    OGM_FLOW_IC_OK = ERROR_OK,
    OGM_FLOW_IC_ERROR_NOT_IMPLEMENTED = ERROR_BASE_TIMER - 90,
    OGM_FLOW_IC_ERROR_NOT_INIT = ERROR_BASE_TIMER - 91,
    OGM_FLOW_IC_ERROR_ALREADY_INIT = ERROR_BASE_TIMER - 92,
    OGM_FLOW_IC_ERROR_INVALID_INSTANCE = ERROR_BASE_TIMER - 93,
    OGM_FLOW_IC_ERROR_TIMEBASE = ERROR_BASE_TIMER - 94,
    OGM_FLOW_IC_ERROR_GPIO = ERROR_BASE_TIMER - 95,
    OGM_FLOW_IC_ERROR_NULL_PTR = ERROR_BASE_TIMER - 96,
} OGM_FlowIC_Status_t;

/**
 * @brief 初始化 OGM 双通道输入捕获（读 board.h 配置）
 * @return error_code_t OGM_FLOW_IC_OK 成功
 * @note TIM2 模式须在 System_Init() / TIM2_TimeBase_Init() 之后调用
 */
error_code_t OGM_FlowIC_Init(void);

/**
 * @brief 反初始化，关闭捕获中断
 */
error_code_t OGM_FlowIC_Deinit(void);

/**
 * @brief 输入捕获中断处理（由 Core/stm32f10x_it.c 调用）
 */
void OGM_FlowIC_IRQHandler(void);

/**
 * @brief 主循环周期调用，内部按 100ms 窗口计算瞬时流量
 */
error_code_t OGM_FlowIC_Process100ms(void);

/**
 * @brief 获取瞬时流量（L/min）
 */
error_code_t OGM_FlowIC_GetInstantFlow(float *flow_lpm);

/**
 * @brief 获取累计有效边沿计数
 */
error_code_t OGM_FlowIC_GetCount(uint32_t *count);

/**
 * @brief 获取当前状态（调试）
 * @note 下降沿交替：0=WAIT_A，1=WAIT_B；四边沿：返回锁位低 4 位
 */
error_code_t OGM_FlowIC_GetState(uint8_t *state);

/**
 * @brief 清零计数与状态机（恢复 WAIT_A）
 */
error_code_t OGM_FlowIC_ResetCount(void);

/**
 * @brief 近期是否有有效脉冲（用于 LED/OLED 刷新策略）
 * @param idle_ms 超过此毫秒数无脉冲视为空闲
 */
error_code_t OGM_FlowIC_IsFlowActive(uint32_t idle_ms, uint8_t *active);

/**
 * @brief 是否已初始化
 */
uint8_t OGM_FlowIC_IsInitialized(void);

/**
 * @brief 获取当前活动定时器实例（供 ISR 路由）
 */
OGM_FlowIC_Instance_t OGM_FlowIC_GetActiveInstance(void);

#if defined(CONFIG_OGM_FLOW_IC_TEST_INJECT) && CONFIG_OGM_FLOW_IC_TEST_INJECT
/**
 * @brief 单元测试：模拟边沿（下降沿交替：channel 0=A,1=B；四边沿：is_rising 0=降 1=升）
 */
error_code_t OGM_FlowIC_InjectPulse(uint8_t channel);
error_code_t OGM_FlowIC_InjectEdge(uint8_t channel, uint8_t is_rising);
#endif

#ifdef __cplusplus
}
#endif

#endif /* OGM_FLOW_IC_H */
