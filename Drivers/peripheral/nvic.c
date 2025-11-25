/**
 * @file nvic.c
 * @brief NVIC中断控制器模块实现
 */

#include "nvic.h"
#include "config.h"
#include "error_handler.h"
#include "misc.h"
#include <stdbool.h>

#if CONFIG_MODULE_NVIC_ENABLED

/* 优先级分组配置标志 */
static bool g_nvic_priority_group_configured = false;
static NVIC_PriorityGroup_t g_nvic_current_group = NVIC_PRIORITY_GROUP_2;

/**
 * @brief 配置NVIC优先级分组
 */
NVIC_Status_t NVIC_ConfigPriorityGroup(NVIC_PriorityGroup_t group)
{
    uint32_t nvic_group;
    
    if (group > NVIC_PRIORITY_GROUP_4)
    {
        ErrorHandler_Handle(NVIC_ERROR_INVALID_PARAM, "NVIC");
        return NVIC_ERROR_INVALID_PARAM;
    }
    
    /* 转换为SPL库的优先级分组值 */
    switch (group)
    {
        case NVIC_PRIORITY_GROUP_0: nvic_group = NVIC_PriorityGroup_0; break;
        case NVIC_PRIORITY_GROUP_1: nvic_group = NVIC_PriorityGroup_1; break;
        case NVIC_PRIORITY_GROUP_2: nvic_group = NVIC_PriorityGroup_2; break;
        case NVIC_PRIORITY_GROUP_3: nvic_group = NVIC_PriorityGroup_3; break;
        case NVIC_PRIORITY_GROUP_4: nvic_group = NVIC_PriorityGroup_4; break;
        default: return NVIC_ERROR_INVALID_PARAM;
    }
    
    /* 配置优先级分组 */
    NVIC_PriorityGroupConfig(nvic_group);
    
    g_nvic_current_group = group;
    g_nvic_priority_group_configured = true;
    
    return NVIC_OK;
}

/**
 * @brief 配置中断优先级和使能/禁用
 */
NVIC_Status_t NVIC_ConfigIRQ(IRQn_Type irq, uint8_t preemption_priority, uint8_t sub_priority, uint8_t enable)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    
    /* 参数校验 */
    if (irq < 0)
    {
        /* 系统中断（负数）也支持 */
    }
    
    /* 检查优先级分组是否已配置 */
    if (!g_nvic_priority_group_configured)
    {
        /* 使用默认优先级分组（Group 2） */
        NVIC_ConfigPriorityGroup(NVIC_PRIORITY_GROUP_2);
    }
    
    /* 根据优先级分组校验优先级范围 */
    switch (g_nvic_current_group)
    {
        case NVIC_PRIORITY_GROUP_0:
            if (preemption_priority > 0 || sub_priority > 15)
            {
                return NVIC_ERROR_INVALID_PARAM;
            }
            break;
        case NVIC_PRIORITY_GROUP_1:
            if (preemption_priority > 1 || sub_priority > 7)
            {
                return NVIC_ERROR_INVALID_PARAM;
            }
            break;
        case NVIC_PRIORITY_GROUP_2:
            if (preemption_priority > 3 || sub_priority > 3)
            {
                return NVIC_ERROR_INVALID_PARAM;
            }
            break;
        case NVIC_PRIORITY_GROUP_3:
            if (preemption_priority > 7 || sub_priority > 1)
            {
                return NVIC_ERROR_INVALID_PARAM;
            }
            break;
        case NVIC_PRIORITY_GROUP_4:
            if (preemption_priority > 15 || sub_priority > 0)
            {
                return NVIC_ERROR_INVALID_PARAM;
            }
            break;
        default:
            return NVIC_ERROR_INVALID_PARAM;
    }
    
    /* 配置NVIC */
    NVIC_InitStructure.NVIC_IRQChannel = irq;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = preemption_priority;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = sub_priority;
    NVIC_InitStructure.NVIC_IRQChannelCmd = enable ? ENABLE : DISABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    return NVIC_OK;
}

/**
 * @brief 使能中断
 */
NVIC_Status_t NVIC_HW_EnableIRQ(IRQn_Type irq)
{
    if (irq < 0)
    {
        /* 系统中断不支持此操作 */
        return NVIC_ERROR_INVALID_IRQ;
    }
    /* 直接调用CMSIS函数 */
    NVIC->ISER[((uint32_t)(irq) >> 5)] = (1 << ((uint32_t)(irq) & 0x1F));
    return NVIC_OK;
}

/**
 * @brief 禁用中断
 */
NVIC_Status_t NVIC_HW_DisableIRQ(IRQn_Type irq)
{
    if (irq < 0)
    {
        /* 系统中断不支持此操作 */
        return NVIC_ERROR_INVALID_IRQ;
    }
    /* 直接调用CMSIS函数 */
    NVIC->ICER[((uint32_t)(irq) >> 5)] = (1 << ((uint32_t)(irq) & 0x1F));
    return NVIC_OK;
}

/**
 * @brief 设置中断挂起标志
 */
NVIC_Status_t NVIC_HW_SetPendingIRQ(IRQn_Type irq)
{
    if (irq < 0)
    {
        return NVIC_ERROR_INVALID_IRQ;
    }
    /* 直接调用CMSIS函数 */
    NVIC->ISPR[((uint32_t)(irq) >> 5)] = (1 << ((uint32_t)(irq) & 0x1F));
    return NVIC_OK;
}

/**
 * @brief 清除中断挂起标志
 */
NVIC_Status_t NVIC_HW_ClearPendingIRQ(IRQn_Type irq)
{
    if (irq < 0)
    {
        return NVIC_ERROR_INVALID_IRQ;
    }
    /* 直接调用CMSIS函数 */
    NVIC->ICPR[((uint32_t)(irq) >> 5)] = (1 << ((uint32_t)(irq) & 0x1F));
    return NVIC_OK;
}

/**
 * @brief 获取中断挂起状态
 */
uint8_t NVIC_HW_GetPendingIRQ(IRQn_Type irq)
{
    if (irq < 0)
    {
        return 0;
    }
    /* 直接调用CMSIS函数 */
    return ((NVIC->ISPR[(uint32_t)(irq) >> 5] & (1 << ((uint32_t)(irq) & 0x1F))) ? 1 : 0);
}

/**
 * @brief 获取中断活动状态
 */
uint8_t NVIC_GetActiveIRQ(IRQn_Type irq)
{
    if (irq < 0)
    {
        return 0;
    }
    /* 直接调用CMSIS函数 */
    return ((NVIC->IABR[(uint32_t)(irq) >> 5] & (1 << ((uint32_t)(irq) & 0x1F))) ? 1 : 0);
}

/**
 * @brief 获取当前优先级分组
 */
NVIC_PriorityGroup_t NVIC_GetPriorityGroup(void)
{
    if (!g_nvic_priority_group_configured)
    {
        /* 返回默认值 */
        return NVIC_PRIORITY_GROUP_2;
    }
    return g_nvic_current_group;
}

/**
 * @brief 设置中断优先级（简化接口）
 */
NVIC_Status_t NVIC_HW_SetPriority(IRQn_Type irq, uint8_t priority)
{
    uint8_t preemption_priority, sub_priority;
    
    if (priority > 15)
    {
        return NVIC_ERROR_INVALID_PARAM;
    }
    
    /* 检查优先级分组是否已配置 */
    if (!g_nvic_priority_group_configured)
    {
        /* 使用默认优先级分组（Group 2） */
        NVIC_ConfigPriorityGroup(NVIC_PRIORITY_GROUP_2);
    }
    
    /* 根据优先级分组分配抢占优先级和子优先级 */
    switch (g_nvic_current_group)
    {
        case NVIC_PRIORITY_GROUP_0:
            preemption_priority = 0;
            sub_priority = priority;
            break;
        case NVIC_PRIORITY_GROUP_1:
            preemption_priority = priority >> 3;
            sub_priority = priority & 0x07;
            break;
        case NVIC_PRIORITY_GROUP_2:
            preemption_priority = priority >> 2;
            sub_priority = priority & 0x03;
            break;
        case NVIC_PRIORITY_GROUP_3:
            preemption_priority = priority >> 1;
            sub_priority = priority & 0x01;
            break;
        case NVIC_PRIORITY_GROUP_4:
            preemption_priority = priority;
            sub_priority = 0;
            break;
        default:
            return NVIC_ERROR_INVALID_PARAM;
    }
    
    return NVIC_ConfigIRQ(irq, preemption_priority, sub_priority, 1);
}

/**
 * @brief 获取中断优先级
 */
uint8_t NVIC_HW_GetPriority(IRQn_Type irq)
{
    uint32_t priority;
    uint8_t preemption_priority, sub_priority;
    
    /* 直接读取优先级寄存器 */
    if (irq < 0)
    {
        /* 系统中断 */
        priority = SCB->SHP[((uint32_t)(irq) & 0xF) - 4];
    }
    else
    {
        /* 外设中断 */
        priority = NVIC->IP[(uint32_t)(irq)];
    }
    
    /* STM32F10x使用4位优先级 */
    priority = priority >> 4;
    
    /* 从优先级寄存器中提取抢占优先级和子优先级 */
    if (!g_nvic_priority_group_configured)
    {
        return (uint8_t)priority;
    }
    
    /* 根据优先级分组提取 */
    switch (g_nvic_current_group)
    {
        case NVIC_PRIORITY_GROUP_0:
            preemption_priority = 0;
            sub_priority = priority & 0x0F;
            break;
        case NVIC_PRIORITY_GROUP_1:
            preemption_priority = (priority >> 3) & 0x01;
            sub_priority = priority & 0x07;
            break;
        case NVIC_PRIORITY_GROUP_2:
            preemption_priority = (priority >> 2) & 0x03;
            sub_priority = priority & 0x03;
            break;
        case NVIC_PRIORITY_GROUP_3:
            preemption_priority = (priority >> 1) & 0x07;
            sub_priority = priority & 0x01;
            break;
        case NVIC_PRIORITY_GROUP_4:
            preemption_priority = priority & 0x0F;
            sub_priority = 0;
            break;
        default:
            return (uint8_t)priority;
    }
    
    /* 合并为单一优先级值 */
    switch (g_nvic_current_group)
    {
        case NVIC_PRIORITY_GROUP_0:
            return sub_priority;
        case NVIC_PRIORITY_GROUP_1:
            return (preemption_priority << 3) | sub_priority;
        case NVIC_PRIORITY_GROUP_2:
            return (preemption_priority << 2) | sub_priority;
        case NVIC_PRIORITY_GROUP_3:
            return (preemption_priority << 1) | sub_priority;
        case NVIC_PRIORITY_GROUP_4:
            return preemption_priority;
        default:
            return (uint8_t)priority;
    }
}

#endif /* CONFIG_MODULE_NVIC_ENABLED */

