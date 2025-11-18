/**
 * @file pwr.c
 * @brief PWR电源管理模块实现
 */

#include "pwr.h"
#include "board.h"
#include "config.h"
#include "error_handler.h"
#include "stm32f10x.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_rcc.h"
#include <stdbool.h>

#if CONFIG_MODULE_PWR_ENABLED

/* PVD电平映射 */
static const uint32_t pwr_pvd_level_map[] = {
    PWR_PVDLevel_2V2,  /* PWR_PVD_LEVEL_2V2 */
    PWR_PVDLevel_2V3,  /* PWR_PVD_LEVEL_2V3 */
    PWR_PVDLevel_2V4,  /* PWR_PVD_LEVEL_2V4 */
    PWR_PVDLevel_2V5,  /* PWR_PVD_LEVEL_2V5 */
    PWR_PVDLevel_2V6,  /* PWR_PVD_LEVEL_2V6 */
    PWR_PVDLevel_2V7,  /* PWR_PVD_LEVEL_2V7 */
    PWR_PVDLevel_2V8,  /* PWR_PVD_LEVEL_2V8 */
    PWR_PVDLevel_2V9,  /* PWR_PVD_LEVEL_2V9 */
};

/**
 * @brief PWR初始化
 */
PWR_Status_t PWR_Init(void)
{
    /* 使能PWR时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    
    return PWR_OK;
}

/**
 * @brief 配置PVD（电源电压检测）
 */
PWR_Status_t PWR_ConfigPVD(PWR_PVDLevel_t level, uint8_t enable)
{
    if (level > PWR_PVD_LEVEL_2V9)
    {
        return PWR_ERROR_INVALID_PARAM;
    }
    
    /* 配置PVD电平 */
    PWR_PVDLevelConfig(pwr_pvd_level_map[level]);
    
    /* 使能/禁用PVD */
    PWR_PVDCmd(enable ? ENABLE : DISABLE);
    
    return PWR_OK;
}

/**
 * @brief 使能唤醒引脚
 */
PWR_Status_t PWR_EnableWakeUpPin(uint8_t enable)
{
    PWR_WakeUpPinCmd(enable ? ENABLE : DISABLE);
    return PWR_OK;
}

/**
 * @brief 进入STOP模式
 */
PWR_Status_t PWR_EnterSTOPMode(PWR_Regulator_t regulator)
{
    uint32_t regulator_value;
    
    if (regulator == PWR_REGULATOR_ON)
    {
        regulator_value = PWR_Regulator_ON;
    }
    else
    {
        regulator_value = PWR_Regulator_LowPower;
    }
    
    /* 进入STOP模式（使用WFI指令） */
    PWR_EnterSTOPMode(regulator_value, PWR_STOPEntry_WFI);
    
    /* 唤醒后，系统时钟需要重新配置 */
    /* 注意：唤醒后，系统会从STOP模式返回，但时钟配置可能丢失 */
    /* 这里需要重新配置系统时钟（如果需要） */
    
    /* 此函数不会立即返回，会等待唤醒 */
    /* 唤醒后继续执行 */
    
    return PWR_OK;
}

/**
 * @brief 进入STANDBY模式
 */
PWR_Status_t PWR_EnterSTANDBYMode(void)
{
    /* 进入STANDBY模式 */
    PWR_EnterSTANDBYMode();
    
    /* 注意：此函数不会返回，系统会复位 */
    
    return PWR_OK;
}

/**
 * @brief 检查唤醒标志
 */
uint8_t PWR_CheckWakeUpFlag(void)
{
    return (PWR_GetFlagStatus(PWR_FLAG_WU) == SET) ? 1 : 0;
}

/**
 * @brief 清除唤醒标志
 */
PWR_Status_t PWR_ClearWakeUpFlag(void)
{
    PWR_ClearFlag(PWR_FLAG_WU);
    return PWR_OK;
}

/**
 * @brief 检查待机标志
 */
uint8_t PWR_CheckStandbyFlag(void)
{
    return (PWR_GetFlagStatus(PWR_FLAG_SB) == SET) ? 1 : 0;
}

/**
 * @brief 清除待机标志
 */
PWR_Status_t PWR_ClearStandbyFlag(void)
{
    PWR_ClearFlag(PWR_FLAG_SB);
    return PWR_OK;
}

/**
 * @brief 检查PVD输出标志
 */
uint8_t PWR_CheckPVDFlag(void)
{
    return (PWR_GetFlagStatus(PWR_FLAG_PVDO) == SET) ? 1 : 0;
}

#endif /* CONFIG_MODULE_PWR_ENABLED */

