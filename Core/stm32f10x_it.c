/**
  ******************************************************************************
  * @file    Project/STM32F10x_StdPeriph_Template/stm32f10x_it.c 
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    08-April-2011
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"
#include "error_code.h"
#include "core_cm3.h"  /* 用于访问SCB寄存器 */

/* 使用 <config.h> 而不是 "config.h"，这样会优先在包含路径中查找 */
/* 案例工程的包含路径中，案例目录在最前面，所以会优先找到案例的config.h */
/* 主工程没有案例目录的config.h，会找到System/config.h */
#include <config.h>

#ifdef ERROR_HANDLER_H
#include "error_handler.h"
#endif

#ifdef LOG_H
#include "log.h"
#endif

#ifdef LED_H
#include "led.h"
#endif

#ifdef DELAY_H
#include "delay.h"
#endif

#if defined(CONFIG_MODULE_TIM_SW_ENABLED) && CONFIG_MODULE_TIM_SW_ENABLED
#include "TIM_sw.h"
#endif

#ifdef SYSTEM_MONITOR_H
#include "system_monitor.h"
#endif

#if defined(CONFIG_MODULE_TIMER_ENABLED) && CONFIG_MODULE_TIMER_ENABLED
#ifdef TIMER_ENCODER_H
#include "timer_encoder.h"
#endif /* TIMER_ENCODER_H */
#endif /* CONFIG_MODULE_TIMER_ENABLED */

/** @addtogroup STM32F10x_StdPeriph_Template
 * @{
 */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* 异常错误码定义 */
#define ERROR_SYSTEM_HARD_FAULT      (ERROR_BASE_SYSTEM - 20)  /**< HardFault异常 */
#define ERROR_SYSTEM_MEM_MANAGE      (ERROR_BASE_SYSTEM - 21)  /**< Memory Manage异常 */
#define ERROR_SYSTEM_BUS_FAULT       (ERROR_BASE_SYSTEM - 22)  /**< Bus Fault异常 */
#define ERROR_SYSTEM_USAGE_FAULT     (ERROR_BASE_SYSTEM - 23)  /**< Usage Fault异常 */

/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

/**
 * @brief 读取故障状态寄存器并记录异常信息
 * @param[in] fault_type 故障类型字符串
 * @param[in] error_code 错误码
 */
static void RecordFaultInfo(const char* fault_type, error_code_t error_code)
{
    uint32_t cfsr = SCB->CFSR;   /* Configurable Fault Status Register */
    uint32_t hfsr = SCB->HFSR;   /* Hard Fault Status Register */
    uint32_t mmfar = SCB->MMFAR; /* Memory Manage Fault Address Register */
    uint32_t bfar = SCB->BFAR;    /* Bus Fault Address Register */
    
    /* 通过错误处理系统记录 */
    #ifdef ERROR_HANDLER_H
    ErrorHandler_Handle(error_code, "Exception");
    #endif
    
    /* 记录异常到系统监控 */
    #ifdef SYSTEM_MONITOR_H
    SystemMonitor_RecordException();
    #endif
    
    /* 如果日志系统可用，输出详细信息 */
    #ifdef LOG_H
    LOG_ERROR("FAULT", "%s occurred!", fault_type);
    LOG_ERROR("FAULT", "CFSR=0x%08lX, HFSR=0x%08lX", cfsr, hfsr);
    
    /* 解析CFSR中的故障类型 */
    if (cfsr & 0x80) LOG_ERROR("FAULT", "  IACCVIOL: Instruction access violation");
    if (cfsr & 0x40) LOG_ERROR("FAULT", "  DACCVIOL: Data access violation");
    if (cfsr & 0x20) LOG_ERROR("FAULT", "  MUNSTKERR: Unstacking error");
    if (cfsr & 0x10) LOG_ERROR("FAULT", "  MSTKERR: Stacking error");
    if (cfsr & 0x08) LOG_ERROR("FAULT", "  MLSPERR: Memory access fault on FP lazy state");
    if (cfsr & 0x02) LOG_ERROR("FAULT", "  MMARVALID: MMFAR valid");
    if (cfsr & 0x01) LOG_ERROR("FAULT", "  IBUSERR: Instruction bus error");
    if (cfsr & 0x8000) LOG_ERROR("FAULT", "  PRECISERR: Precise data bus error");
    if (cfsr & 0x4000) LOG_ERROR("FAULT", "  IMPRECISERR: Imprecise data bus error");
    if (cfsr & 0x2000) LOG_ERROR("FAULT", "  UNSTKERR: Unstacking error");
    if (cfsr & 0x1000) LOG_ERROR("FAULT", "  STKERR: Stacking error");
    if (cfsr & 0x0800) LOG_ERROR("FAULT", "  LSPERR: FP lazy state error");
    if (cfsr & 0x0200) LOG_ERROR("FAULT", "  BFARVALID: BFAR valid");
    if (cfsr & 0x01000000) LOG_ERROR("FAULT", "  UNALIGNED: Unaligned access");
    if (cfsr & 0x02000000) LOG_ERROR("FAULT", "  DIVBYZERO: Division by zero");
    if (cfsr & 0x00020000) LOG_ERROR("FAULT", "  INVSTATE: Invalid state");
    if (cfsr & 0x00040000) LOG_ERROR("FAULT", "  INVPC: Invalid PC");
    if (cfsr & 0x00080000) LOG_ERROR("FAULT", "  NOCP: No coprocessor");
    
    if (mmfar != 0) {
        LOG_ERROR("FAULT", "MMFAR=0x%08lX", mmfar);
    }
    if (bfar != 0) {
        LOG_ERROR("FAULT", "BFAR=0x%08lX", bfar);
    }
    #endif
}

/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
 * @brief  This function handles NMI exception.
 * @param  None
 * @retval None
 * @details NMI通常用于看门狗复位等，这里只记录并进入死循环
 */
void NMI_Handler(void)
{
    /* NMI通常用于看门狗复位等，这里只记录 */
    #ifdef LOG_H
    LOG_ERROR("FAULT", "NMI exception occurred!");
    #endif
    
    /* 进入死循环 */
    while (1)
    {
        #ifdef LED_H
        #ifdef DELAY_H
        LED_Toggle(LED_1);
        Delay_ms(100);
        #endif
        #endif
    }
}

/**
 * @brief  This function handles Hard Fault exception.
 * @param  None
 * @retval None
 * @details 读取故障寄存器，记录异常信息，LED指示
 */
void HardFault_Handler(void)
{
    /* 记录异常信息 */
    RecordFaultInfo("HardFault", ERROR_SYSTEM_HARD_FAULT);
    
    /* LED闪烁指示（如果可用） */
    #ifdef LED_H
    #ifdef DELAY_H
    while (1)
    {
        LED_Toggle(LED_1);
        Delay_ms(100);
    }
    #else
    while (1)
    {
        LED_Toggle(LED_1);
        volatile uint32_t delay = 100000;
        while (delay--);
    }
    #endif
    #else
    /* 没有LED，进入死循环 */
    while (1)
    {
    }
    #endif
}

/**
 * @brief  This function handles Memory Manage exception.
 * @param  None
 * @retval None
 * @details 读取故障寄存器，记录异常信息，LED指示（200ms闪烁）
 */
void MemManage_Handler(void)
{
    /* 记录异常信息 */
    RecordFaultInfo("Memory Manage Fault", ERROR_SYSTEM_MEM_MANAGE);
    
    /* LED闪烁指示（如果可用） */
    #ifdef LED_H
    #ifdef DELAY_H
    while (1)
    {
        LED_Toggle(LED_1);
        Delay_ms(200);  /* 稍慢的闪烁，区别于HardFault */
    }
    #else
    while (1)
    {
        LED_Toggle(LED_1);
        volatile uint32_t delay = 200000;
        while (delay--);
    }
    #endif
    #else
    while (1)
    {
    }
    #endif
}

/**
 * @brief  This function handles Bus Fault exception.
 * @param  None
 * @retval None
 * @details 读取故障寄存器，记录异常信息，LED指示（300ms闪烁）
 */
void BusFault_Handler(void)
{
    /* 记录异常信息 */
    RecordFaultInfo("Bus Fault", ERROR_SYSTEM_BUS_FAULT);
    
    /* LED闪烁指示（如果可用） */
    #ifdef LED_H
    #ifdef DELAY_H
    while (1)
    {
        LED_Toggle(LED_1);
        Delay_ms(300);  /* 更慢的闪烁，区别于其他异常 */
    }
    #else
    while (1)
    {
        LED_Toggle(LED_1);
        volatile uint32_t delay = 300000;
        while (delay--);
    }
    #endif
    #else
    while (1)
    {
    }
    #endif
}

/**
 * @brief  This function handles Usage Fault exception.
 * @param  None
 * @retval None
 * @details 读取故障寄存器，记录异常信息，LED指示（400ms闪烁）
 */
void UsageFault_Handler(void)
{
    /* 记录异常信息 */
    RecordFaultInfo("Usage Fault", ERROR_SYSTEM_USAGE_FAULT);
    
    /* LED闪烁指示（如果可用） */
    #ifdef LED_H
    #ifdef DELAY_H
    while (1)
    {
        LED_Toggle(LED_1);
        Delay_ms(400);  /* 最慢的闪烁，区别于其他异常 */
    }
    #else
    while (1)
    {
        LED_Toggle(LED_1);
        volatile uint32_t delay = 400000;
        while (delay--);
    }
    #endif
    #else
    while (1)
    {
    }
    #endif
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  This function handles TIM2 Handler.
  * @param  None
  * @retval None
  */
/* g_task_tick 在TIM2_TimeBase模块中定义，在中断中递增，作为时间基准 */
extern volatile uint32_t g_task_tick;

#ifdef CLOCK_MANAGER_H
/* 时钟管理模块的CPU使用率计算（1秒定时器） */
extern void CLKM_CalculateCPULoad1Sec(void);
#endif

#if defined(CONFIG_MODULE_TIM_SW_ENABLED) && CONFIG_MODULE_TIM_SW_ENABLED
/* 软件定时器模块更新函数 */
extern void TIM_SW_Update(void);
#endif

void TIM2_IRQHandler(void)
{
    /* ========== 处理TIM2_TimeBase中断（Update中断） ========== */
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        g_task_tick++;  /* TIM2_TimeBase模块的时间基准，1ms递增一次 */
        
        /* 1秒定时器：每1000ms计算一次CPU使用率 */
        #ifdef CLOCK_MANAGER_H
        if ((g_task_tick % 1000) == 0)  /* 每1秒（1000ms） */
        {
            CLKM_CalculateCPULoad1Sec();
        }
        #endif
        
        /* 更新软件定时器 */
        #if defined(CONFIG_MODULE_TIM_SW_ENABLED) && CONFIG_MODULE_TIM_SW_ENABLED
        TIM_SW_Update();
        #endif
    }
    
    /* ========== 处理编码器中断（如果使用TIM2作为编码器） ========== */
    #if defined(CONFIG_MODULE_TIMER_ENABLED) && CONFIG_MODULE_TIMER_ENABLED
    #ifdef TIMER_ENCODER_H
    /* 检查是否是编码器中断（CC1中断） */
    if (TIM_GetITStatus(TIM2, TIM_IT_CC1) != RESET) {
        ENCODER_IRQHandler(ENCODER_INSTANCE_TIM2);
    }
    #endif /* TIMER_ENCODER_H */
    #endif /* CONFIG_MODULE_TIMER_ENABLED */
}

/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f10x_xx.s).                                            */
/******************************************************************************/

/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
/*void PPP_IRQHandler(void)
{
}*/

/* ==================== EXTI外部中断处理 ==================== */

#if defined(CONFIG_MODULE_EXTI_ENABLED) && CONFIG_MODULE_EXTI_ENABLED
#include "exti.h"
#include "stm32f10x_exti.h"

/* ==================== EXTI独立中断向量（EXTI0-4） ==================== */

/**
 * @brief EXTI0中断服务程序
 */
void EXTI0_IRQHandler(void)
{
    EXTI_IRQHandler(EXTI_LINE_0);
}

/**
 * @brief EXTI1中断服务程序
 */
void EXTI1_IRQHandler(void)
{
    EXTI_IRQHandler(EXTI_LINE_1);
}

/**
 * @brief EXTI2中断服务程序
 */
void EXTI2_IRQHandler(void)
{
    EXTI_IRQHandler(EXTI_LINE_2);
}

/**
 * @brief EXTI3中断服务程序
 */
void EXTI3_IRQHandler(void)
{
    EXTI_IRQHandler(EXTI_LINE_3);
}

/**
 * @brief EXTI4中断服务程序
 */
void EXTI4_IRQHandler(void)
{
    EXTI_IRQHandler(EXTI_LINE_4);
}

/* ==================== EXTI共享中断向量 ==================== */

/**
 * @brief EXTI9_5中断服务程序（共享中断向量：EXTI5-9）
 * @note 需要检查所有可能的线，因为多个EXTI线共享同一个中断向量
 */
void EXTI9_5_IRQHandler(void)
{
    /* 检查EXTI5 */
    if (EXTI_GetITStatus(EXTI_Line5) != RESET)
    {
        EXTI_IRQHandler(EXTI_LINE_5);
    }
    
    /* 检查EXTI6 */
    if (EXTI_GetITStatus(EXTI_Line6) != RESET)
    {
        EXTI_IRQHandler(EXTI_LINE_6);
    }
    
    /* 检查EXTI7 */
    if (EXTI_GetITStatus(EXTI_Line7) != RESET)
    {
        EXTI_IRQHandler(EXTI_LINE_7);
    }
    
    /* 检查EXTI8 */
    if (EXTI_GetITStatus(EXTI_Line8) != RESET)
    {
        EXTI_IRQHandler(EXTI_LINE_8);
    }
    
    /* 检查EXTI9 */
    if (EXTI_GetITStatus(EXTI_Line9) != RESET)
    {
        EXTI_IRQHandler(EXTI_LINE_9);
    }
}

/**
 * @brief EXTI15_10中断服务程序（共享中断向量：EXTI10-15）
 * @note 需要检查所有可能的线，因为多个EXTI线共享同一个中断向量
 */
void EXTI15_10_IRQHandler(void)
{
    /* 检查EXTI10 */
    if (EXTI_GetITStatus(EXTI_Line10) != RESET)
    {
        EXTI_IRQHandler(EXTI_LINE_10);
    }
    
    /* 检查EXTI11 */
    if (EXTI_GetITStatus(EXTI_Line11) != RESET)
    {
        EXTI_IRQHandler(EXTI_LINE_11);
    }
    
    /* 检查EXTI12 */
    if (EXTI_GetITStatus(EXTI_Line12) != RESET)
    {
        EXTI_IRQHandler(EXTI_LINE_12);
    }
    
    /* 检查EXTI13 */
    if (EXTI_GetITStatus(EXTI_Line13) != RESET)
    {
        EXTI_IRQHandler(EXTI_LINE_13);
    }
    
    /* 检查EXTI14 */
    if (EXTI_GetITStatus(EXTI_Line14) != RESET)
    {
        EXTI_IRQHandler(EXTI_LINE_14);
    }
    
    /* 检查EXTI15 */
    if (EXTI_GetITStatus(EXTI_Line15) != RESET)
    {
        EXTI_IRQHandler(EXTI_LINE_15);
    }
}

/* ==================== EXTI特殊功能中断向量（EXTI16-19） ==================== */

/**
 * @brief PVD中断服务程序（EXTI Line 16）
 * @note PVD (Programmable Voltage Detector) 可编程电压检测器
 */
void PVD_IRQHandler(void)
{
    EXTI_IRQHandler(EXTI_LINE_16);
}

/**
 * @brief RTC Alarm中断服务程序（EXTI Line 17）
 * @note RTC Alarm通过EXTI Line 17触发
 */
void RTCAlarm_IRQHandler(void)
{
    EXTI_IRQHandler(EXTI_LINE_17);
}

/**
 * @brief USB Wakeup中断服务程序（EXTI Line 18）
 * @note USB设备从挂起状态唤醒通过EXTI Line 18触发
 */
void USBWakeUp_IRQHandler(void)
{
    EXTI_IRQHandler(EXTI_LINE_18);
}

/* 注意：Ethernet Wakeup (EXTI Line 19) 只在STM32F10X_HD和STM32F10X_XL型号上可用 */
#if defined(STM32F10X_HD) || defined(STM32F10X_XL) || defined(STM32F10X_HD_VL)
/**
 * @brief Ethernet Wakeup中断服务程序（EXTI Line 19）
 * @note 以太网唤醒通过EXTI Line 19触发
 * @note 仅在HD/XL型号上可用
 */
void ETH_WKUP_IRQHandler(void)
{
    EXTI_IRQHandler(EXTI_LINE_19);
}
#endif /* STM32F10X_HD || STM32F10X_XL || STM32F10X_HD_VL */

#endif /* CONFIG_MODULE_EXTI_ENABLED */

/* ==================== UART中断服务程序 ==================== */

#if defined(CONFIG_MODULE_UART_ENABLED) && CONFIG_MODULE_UART_ENABLED
#ifdef UART_H
#include "uart.h"

/**
 * @brief USART1中断服务程序
 * @note 独立中断向量，直接调用模块处理函数
 */
void USART1_IRQHandler(void)
{
    UART_IRQHandler(UART_INSTANCE_1);
}

/**
 * @brief USART2中断服务程序
 * @note 独立中断向量，直接调用模块处理函数
 */
void USART2_IRQHandler(void)
{
    UART_IRQHandler(UART_INSTANCE_2);
}

/**
 * @brief USART3中断服务程序
 * @note 独立中断向量，直接调用模块处理函数
 */
void USART3_IRQHandler(void)
{
    UART_IRQHandler(UART_INSTANCE_3);
}

#endif /* UART_H */
#endif /* CONFIG_MODULE_UART_ENABLED */

/* ==================== DMA中断服务程序 ==================== */

#if defined(CONFIG_MODULE_DMA_ENABLED) && CONFIG_MODULE_DMA_ENABLED
#ifdef DMA_H
#include "dma.h"

/**
 * @brief DMA1 Channel1中断服务程序
 * @note 独立中断向量，直接调用模块处理函数
 */
void DMA1_Channel1_IRQHandler(void)
{
    DMA_IRQHandler(DMA_CHANNEL_1_1);
}

/**
 * @brief DMA1 Channel2中断服务程序
 * @note 独立中断向量，直接调用模块处理函数
 */
void DMA1_Channel2_IRQHandler(void)
{
    DMA_IRQHandler(DMA_CHANNEL_1_2);
}

/**
 * @brief DMA1 Channel3中断服务程序
 * @note 独立中断向量，直接调用模块处理函数
 */
void DMA1_Channel3_IRQHandler(void)
{
    DMA_IRQHandler(DMA_CHANNEL_1_3);
}

/**
 * @brief DMA1 Channel4中断服务程序
 * @note 独立中断向量，直接调用模块处理函数
 */
void DMA1_Channel4_IRQHandler(void)
{
    DMA_IRQHandler(DMA_CHANNEL_1_4);
}

/**
 * @brief DMA1 Channel5中断服务程序
 * @note 独立中断向量，直接调用模块处理函数
 */
void DMA1_Channel5_IRQHandler(void)
{
    DMA_IRQHandler(DMA_CHANNEL_1_5);
}

/**
 * @brief DMA1 Channel6中断服务程序
 * @note 独立中断向量，直接调用模块处理函数
 */
void DMA1_Channel6_IRQHandler(void)
{
    DMA_IRQHandler(DMA_CHANNEL_1_6);
}

/**
 * @brief DMA1 Channel7中断服务程序
 * @note 独立中断向量，直接调用模块处理函数
 */
void DMA1_Channel7_IRQHandler(void)
{
    DMA_IRQHandler(DMA_CHANNEL_1_7);
}

#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
/**
 * @brief DMA2 Channel1中断服务程序
 * @note 独立中断向量，直接调用模块处理函数（仅HD/CL/HD_VL型号）
 */
void DMA2_Channel1_IRQHandler(void)
{
    DMA_IRQHandler(DMA_CHANNEL_2_1);
}

/**
 * @brief DMA2 Channel2中断服务程序
 * @note 独立中断向量，直接调用模块处理函数（仅HD/CL/HD_VL型号）
 */
void DMA2_Channel2_IRQHandler(void)
{
    DMA_IRQHandler(DMA_CHANNEL_2_2);
}

/**
 * @brief DMA2 Channel3中断服务程序
 * @note 独立中断向量，直接调用模块处理函数（仅HD/CL/HD_VL型号）
 */
void DMA2_Channel3_IRQHandler(void)
{
    DMA_IRQHandler(DMA_CHANNEL_2_3);
}

/**
 * @brief DMA2 Channel4中断服务程序
 * @note 独立中断向量，直接调用模块处理函数（仅HD/CL/HD_VL型号）
 */
void DMA2_Channel4_IRQHandler(void)
{
    DMA_IRQHandler(DMA_CHANNEL_2_4);
}

/**
 * @brief DMA2 Channel5中断服务程序
 * @note 独立中断向量，直接调用模块处理函数（仅HD/CL/HD_VL型号）
 */
void DMA2_Channel5_IRQHandler(void)
{
    DMA_IRQHandler(DMA_CHANNEL_2_5);
}
#endif /* STM32F10X_HD || STM32F10X_CL || STM32F10X_HD_VL */

#endif /* DMA_H */
#endif /* CONFIG_MODULE_DMA_ENABLED */

/**
  * @}
  */ 


/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
