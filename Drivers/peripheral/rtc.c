/**
 * @file rtc.c
 * @brief RTC实时时钟模块实现
 */

#include "rtc.h"
#include "board.h"
#include "config.h"
#include "error_handler.h"
#include "stm32f10x.h"
#include "stm32f10x_rtc.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_bkp.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_exti.h"
#include "misc.h"
#include <stdbool.h>
#include <string.h>

#if CONFIG_MODULE_RTC_ENABLED

/* RTC时钟源配置 */
#define RTC_CLOCK_SOURCE_LSE          RCC_RTCCLKSource_LSE
#define RTC_CLOCK_SOURCE_LSI          RCC_RTCCLKSource_LSI
#define RTC_CLOCK_SOURCE_HSE_DIV128   RCC_RTCCLKSource_HSE_Div128

/* RTC预分频器（用于1Hz时钟） */
/* LSE: 32768Hz -> 32767 (0x7FFF) */
/* LSI: ~40kHz -> 39999 (0x9C3F) */
/* HSE_Div128: 需要根据实际HSE频率计算 */
#define RTC_PRESCALER_LSE     0x7FFF  /* 32768 - 1 */
#define RTC_PRESCALER_LSI     0x9C3F  /* 40000 - 1 */
#define RTC_PRESCALER_HSE     0x7FFF  /* 默认值，需要根据实际频率计算 */

/* Unix时间戳基准：1970-01-01 00:00:00 */
#define RTC_UNIX_EPOCH_YEAR   1970

/* 初始化标志 */
static bool g_rtc_initialized = false;
static RTC_ClockSource_t g_rtc_clock_source = RTC_CLOCK_SOURCE_LSI;

/* 闹钟回调 */
static RTC_AlarmCallback_t g_rtc_alarm_callback = NULL;
static void *g_rtc_user_data = NULL;

/* 时间戳回调 */
static RTC_TimestampCallback_t g_rtc_timestamp_callback = NULL;
static void *g_rtc_timestamp_user_data = NULL;

/* 溢出回调 */
static RTC_OverflowCallback_t g_rtc_overflow_callback = NULL;
static void *g_rtc_overflow_user_data = NULL;

/* 月份天数表（平年） */
static const uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/**
 * @brief 判断是否为闰年
 */
static uint8_t RTC_IsLeapYear(uint16_t year)
{
    return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) ? 1 : 0;
}

/**
 * @brief 获取指定年份和月份的天数
 */
static uint8_t RTC_GetDaysInMonth(uint16_t year, uint8_t month)
{
    if (month < 1 || month > 12)
    {
        return 0;
    }
    
    if (month == 2 && RTC_IsLeapYear(year))
    {
        return 29;
    }
    
    return days_in_month[month - 1];
}

/**
 * @brief 计算从1970-01-01到指定日期的秒数
 */
static uint32_t RTC_TimeToCounter(const RTC_Time_t *time)
{
    uint32_t seconds = 0;
    uint16_t year;
    uint8_t month;
    
    /* 计算年份的秒数 */
    for (year = RTC_UNIX_EPOCH_YEAR; year < time->year; year++)
    {
        seconds += RTC_IsLeapYear(year) ? 31622400 : 31536000; /* 366天或365天的秒数 */
    }
    
    /* 计算月份的秒数 */
    for (month = 1; month < time->month; month++)
    {
        seconds += RTC_GetDaysInMonth(time->year, month) * 86400;
    }
    
    /* 计算天数的秒数 */
    seconds += (time->day - 1) * 86400;
    
    /* 计算小时的秒数 */
    seconds += time->hour * 3600;
    
    /* 计算分钟的秒数 */
    seconds += time->minute * 60;
    
    /* 加上秒数 */
    seconds += time->second;
    
    return seconds;
}

/**
 * @brief 从计数器值转换为时间结构体
 */
static void RTC_CounterToTime(uint32_t counter, RTC_Time_t *time)
{
    uint32_t days, seconds;
    uint16_t year = RTC_UNIX_EPOCH_YEAR;
    uint8_t month = 1;
    uint8_t day = 1;
    
    /* 计算天数 */
    days = counter / 86400;
    seconds = counter % 86400;
    
    /* 计算年份 */
    while (days >= (RTC_IsLeapYear(year) ? 366 : 365))
    {
        days -= RTC_IsLeapYear(year) ? 366 : 365;
        year++;
    }
    
    /* 计算月份 */
    while (days >= RTC_GetDaysInMonth(year, month))
    {
        days -= RTC_GetDaysInMonth(year, month);
        month++;
    }
    
    /* 计算日期 */
    day = days + 1;
    
    /* 计算时分秒 */
    time->hour = seconds / 3600;
    time->minute = (seconds % 3600) / 60;
    time->second = seconds % 60;
    
    /* 设置年月日 */
    time->year = year;
    time->month = month;
    time->day = day;
    
    /* 计算星期（Zeller公式简化版） */
    /* 这里使用简化算法，实际应用中可能需要更精确的算法 */
    uint32_t y = year;
    uint32_t m = month;
    uint32_t d = day;
    
    if (m < 3)
    {
        m += 12;
        y--;
    }
    
    uint32_t k = y % 100;
    uint32_t j = y / 100;
    uint32_t h = (d + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
    time->weekday = (h + 5) % 7; /* 转换为0=周日，1=周一，...，6=周六 */
}

/**
 * @brief 获取RTC预分频器值
 */
static uint32_t RTC_GetPrescalerValue(RTC_ClockSource_t clock_source)
{
    switch (clock_source)
    {
        case RTC_CLOCK_SOURCE_LSE:
            return RTC_PRESCALER_LSE;
        case RTC_CLOCK_SOURCE_LSI:
            return RTC_PRESCALER_LSI;
        case RTC_CLOCK_SOURCE_HSE_DIV128:
            return RTC_PRESCALER_HSE; /* 需要根据实际HSE频率计算 */
        default:
            return RTC_PRESCALER_LSI;
    }
}

/**
 * @brief RTC初始化
 */
RTC_Status_t RTC_Init(RTC_ClockSource_t clock_source)
{
    uint32_t prescaler;
    
    if (g_rtc_initialized)
    {
        ERROR_HANDLER_Report(ERROR_BASE_RTC, __FILE__, __LINE__, "RTC already initialized");
        return RTC_ERROR_ALREADY_INITIALIZED;
    }
    
    /* 使能PWR和BKP时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    
    /* 使能备份域访问 */
    PWR_BackupAccessCmd(ENABLE);
    
    /* 检查RTC是否已经配置过 */
    if (BKP_ReadBackupRegister(BKP_DR1) != 0xA5A5)
    {
        /* RTC未配置，进行初始化 */
        
        /* 复位备份域 */
        BKP_DeInit();
        
        /* 配置RTC时钟源 */
        switch (clock_source)
        {
            case RTC_CLOCK_SOURCE_LSE:
                /* 使能LSE */
                RCC_LSEConfig(RCC_LSE_ON);
                /* 等待LSE就绪 */
                while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)
                {
                    /* 超时处理 */
                }
                RCC_RTCCLKConfig(RTC_CLOCK_SOURCE_LSE);
                break;
                
            case RTC_CLOCK_SOURCE_LSI:
                /* 使能LSI */
                RCC_LSICmd(ENABLE);
                /* 等待LSI就绪 */
                while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
                {
                    /* 超时处理 */
                }
                RCC_RTCCLKConfig(RTC_CLOCK_SOURCE_LSI);
                break;
                
            case RTC_CLOCK_SOURCE_HSE_DIV128:
                /* 假设HSE已经配置 */
                RCC_RTCCLKConfig(RTC_CLOCK_SOURCE_HSE_DIV128);
                break;
                
            default:
                ERROR_HANDLER_Report(ERROR_BASE_RTC, __FILE__, __LINE__, "Invalid RTC clock source");
                return RTC_ERROR_INVALID_PARAM;
        }
        
        /* 使能RTC */
        RCC_RTCCLKCmd(ENABLE);
        
        /* 等待RTC寄存器同步 */
        RTC_WaitForSynchro();
        
        /* 配置预分频器 */
        prescaler = RTC_GetPrescalerValue(clock_source);
        RTC_SetPrescaler(prescaler);
        
        /* 等待配置完成 */
        RTC_WaitForLastTask();
        
        /* 标记RTC已配置 */
        BKP_WriteBackupRegister(BKP_DR1, 0xA5A5);
    }
    else
    {
        /* RTC已配置，只需使能RTC时钟 */
        RCC_RTCCLKCmd(ENABLE);
        
        /* 等待RTC寄存器同步 */
        RTC_WaitForSynchro();
    }
    
    /* 配置RTC中断（如果需要） */
    /* 注意：RTC中断需要通过EXTI线17（RTCAlarm_IRQn） */
    
    g_rtc_initialized = true;
    g_rtc_clock_source = clock_source;
    
    return RTC_OK;
}

/**
 * @brief RTC反初始化
 */
RTC_Status_t RTC_Deinit(void)
{
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    /* 禁用RTC中断 */
    RTC_ITConfig(RTC_IT_ALR | RTC_IT_SEC | RTC_IT_OW, DISABLE);
    
    /* 禁用RTC */
    RCC_RTCCLKCmd(DISABLE);
    
    g_rtc_initialized = false;
    
    return RTC_OK;
}

/**
 * @brief 设置RTC时间
 */
RTC_Status_t RTC_SetTime(const RTC_Time_t *time)
{
    uint32_t counter;
    
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    if (time == NULL)
    {
        return RTC_ERROR_INVALID_PARAM;
    }
    
    /* 参数校验 */
    if (time->year < RTC_UNIX_EPOCH_YEAR || time->year > 2099 ||
        time->month < 1 || time->month > 12 ||
        time->day < 1 || time->day > RTC_GetDaysInMonth(time->year, time->month) ||
        time->hour > 23 || time->minute > 59 || time->second > 59)
    {
        return RTC_ERROR_INVALID_TIME;
    }
    
    /* 转换为计数器值 */
    counter = RTC_TimeToCounter(time);
    
    /* 设置RTC计数器 */
    RTC_SetCounter(counter);
    RTC_WaitForLastTask();
    
    return RTC_OK;
}

/**
 * @brief 获取RTC时间
 */
RTC_Status_t RTC_GetTime(RTC_Time_t *time)
{
    uint32_t counter;
    
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    if (time == NULL)
    {
        return RTC_ERROR_INVALID_PARAM;
    }
    
    /* 等待RTC寄存器同步 */
    RTC_WaitForSynchro();
    
    /* 读取计数器值 */
    counter = RTC_GetCounter();
    
    /* 转换为时间结构体 */
    RTC_CounterToTime(counter, time);
    
    return RTC_OK;
}

/**
 * @brief 设置RTC闹钟
 */
RTC_Status_t RTC_SetAlarm(const RTC_Time_t *time)
{
    uint32_t alarm_counter;
    
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    if (time == NULL)
    {
        return RTC_ERROR_INVALID_PARAM;
    }
    
    /* 参数校验 */
    if (time->year < RTC_UNIX_EPOCH_YEAR || time->year > 2099 ||
        time->month < 1 || time->month > 12 ||
        time->day < 1 || time->day > RTC_GetDaysInMonth(time->year, time->month) ||
        time->hour > 23 || time->minute > 59 || time->second > 59)
    {
        return RTC_ERROR_INVALID_TIME;
    }
    
    /* 转换为计数器值 */
    alarm_counter = RTC_TimeToCounter(time);
    
    /* 设置RTC闹钟 */
    RTC_SetAlarm(alarm_counter);
    RTC_WaitForLastTask();
    
    return RTC_OK;
}

/**
 * @brief 获取RTC闹钟时间
 */
RTC_Status_t RTC_GetAlarm(RTC_Time_t *time)
{
    uint32_t alarm_counter;
    
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    if (time == NULL)
    {
        return RTC_ERROR_INVALID_PARAM;
    }
    
    /* 读取闹钟值（需要从寄存器直接读取） */
    /* 注意：SPL库没有提供RTC_GetAlarm函数，需要直接读取寄存器 */
    uint16_t alr_l = RTC->ALRL;
    uint16_t alr_h = RTC->ALRH;
    alarm_counter = ((uint32_t)alr_h << 16) | alr_l;
    
    /* 转换为时间结构体 */
    RTC_CounterToTime(alarm_counter, time);
    
    return RTC_OK;
}

/**
 * @brief 使能RTC闹钟
 */
RTC_Status_t RTC_EnableAlarm(void)
{
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    /* 使能闹钟中断 */
    RTC_ITConfig(RTC_IT_ALR, ENABLE);
    
    /* 配置EXTI线17（RTC闹钟） */
    EXTI_InitTypeDef EXTI_InitStructure;
    EXTI_InitStructure.EXTI_Line = EXTI_Line17;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);
    
    /* 配置NVIC */
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = RTCAlarm_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    return RTC_OK;
}

/**
 * @brief 禁用RTC闹钟
 */
RTC_Status_t RTC_DisableAlarm(void)
{
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    /* 禁用闹钟中断 */
    RTC_ITConfig(RTC_IT_ALR, DISABLE);
    
    return RTC_OK;
}

/**
 * @brief 设置RTC闹钟回调函数
 */
RTC_Status_t RTC_SetAlarmCallback(RTC_AlarmCallback_t callback, void *user_data)
{
    g_rtc_alarm_callback = callback;
    g_rtc_user_data = user_data;
    return RTC_OK;
}

/**
 * @brief 检查RTC闹钟标志
 */
uint8_t RTC_CheckAlarmFlag(void)
{
    if (!g_rtc_initialized)
    {
        return 0;
    }
    
    return (RTC_GetFlagStatus(RTC_FLAG_ALR) == SET) ? 1 : 0;
}

/**
 * @brief 清除RTC闹钟标志
 */
RTC_Status_t RTC_ClearAlarmFlag(void)
{
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    RTC_ClearFlag(RTC_FLAG_ALR);
    return RTC_OK;
}

/**
 * @brief 使能RTC秒中断
 */
RTC_Status_t RTC_EnableSecondInterrupt(void)
{
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    RTC_ITConfig(RTC_IT_SEC, ENABLE);
    
    /* 配置NVIC */
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = RTC_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    return RTC_OK;
}

/**
 * @brief 禁用RTC秒中断
 */
RTC_Status_t RTC_DisableSecondInterrupt(void)
{
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    RTC_ITConfig(RTC_IT_SEC, DISABLE);
    return RTC_OK;
}

/**
 * @brief 检查RTC是否已初始化
 */
uint8_t RTC_IsInitialized(void)
{
    return g_rtc_initialized ? 1 : 0;
}

/**
 * @brief 获取RTC计数器值（封装函数，避免与SPL库函数名冲突）
 */
uint32_t RTC_GetCounterValue(void)
{
    if (!g_rtc_initialized)
    {
        return 0;
    }
    
    RTC_WaitForSynchro();
    return RTC_GetCounter(); /* 调用SPL库函数 */
}

/**
 * @brief 设置RTC计数器值（封装函数，避免与SPL库函数名冲突）
 */
RTC_Status_t RTC_SetCounterValue(uint32_t counter)
{
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    RTC_SetCounter(counter); /* 调用SPL库函数 */
    RTC_WaitForLastTask();
    
    return RTC_OK;
}

/**
 * @brief RTC中断服务函数
 */
void RTC_IRQHandler(void)
{
    if (RTC_GetITStatus(RTC_IT_SEC) != RESET)
    {
        RTC_ClearITPendingBit(RTC_IT_SEC);
        /* 秒中断处理 */
    }
    
    if (RTC_GetITStatus(RTC_IT_ALR) != RESET)
    {
        RTC_ClearITPendingBit(RTC_IT_ALR);
        /* 闹钟中断处理 */
        if (g_rtc_alarm_callback != NULL)
        {
            g_rtc_alarm_callback(g_rtc_user_data);
        }
    }
    
    if (RTC_GetITStatus(RTC_IT_OW) != RESET)
    {
        RTC_ClearITPendingBit(RTC_IT_OW);
        /* 溢出中断处理 */
    }
}

/**
 * @brief RTC闹钟中断服务函数（EXTI线17）
 */
void RTCAlarm_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line17) != RESET)
    {
        EXTI_ClearITPendingBit(EXTI_Line17);
        
        if (RTC_GetITStatus(RTC_IT_ALR) != RESET)
        {
            RTC_ClearITPendingBit(RTC_IT_ALR);
            
            if (g_rtc_alarm_callback != NULL)
            {
                g_rtc_alarm_callback(g_rtc_user_data);
            }
        }
    }
}

/* ========== RTC校准功能实现 ========== */

/**
 * @brief 设置RTC校准值
 */
RTC_Status_t RTC_SetCalibration(int16_t calibration_value)
{
    uint16_t cal_value;
    
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    if (calibration_value < -511 || calibration_value > 512)
    {
        return RTC_ERROR_INVALID_PARAM;
    }
    
    /* 转换校准值：BKP_SetRTCCalibrationValue需要0-1023的值 */
    /* 校准值范围：-511到+512，映射到0-1023 */
    if (calibration_value >= 0)
    {
        cal_value = (uint16_t)calibration_value;  /* 0-512 */
    }
    else
    {
        cal_value = (uint16_t)(1024 + calibration_value);  /* 513-1023（对应-511到-1） */
    }
    
    /* 设置RTC校准值 */
    BKP_SetRTCCalibrationValue((uint8_t)cal_value);
    
    return RTC_OK;
}

/**
 * @brief 获取RTC校准值
 */
RTC_Status_t RTC_GetCalibration(int16_t *calibration_value)
{
    uint16_t cal_value;
    
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    if (calibration_value == NULL)
    {
        return RTC_ERROR_INVALID_PARAM;
    }
    
    /* 读取RTC校准值（从BKP数据寄存器读取，校准值存储在BKP_DR1的低10位） */
    cal_value = (uint16_t)(BKP_ReadBackupRegister(BKP_DR1) & 0x3FF);
    
    /* 转换校准值：0-1023映射到-511到+512 */
    if (cal_value <= 512)
    {
        *calibration_value = (int16_t)cal_value;  /* 0-512 */
    }
    else
    {
        *calibration_value = (int16_t)(cal_value - 1024);  /* 513-1023映射到-511到-1 */
    }
    
    return RTC_OK;
}

/* ========== RTC时间戳功能实现 ========== */

/**
 * @brief 使能RTC时间戳功能
 * @note STM32F10x的RTC时间戳功能需要通过EXTI线18（PC13）或EXTI线0（PA0）触发
 * @note 时间戳值存储在RTC->TSRL和RTC->TSRH寄存器中
 */
RTC_Status_t RTC_EnableTimestamp(uint8_t trigger_pin)
{
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    /* STM32F10x的RTC时间戳功能需要配置EXTI线18（PC13）或EXTI线0（PA0） */
    /* 注意：STM32F10x SPL库可能没有提供时间戳封装函数，需要直接操作寄存器 */
    /* 这里提供一个基本框架，实际使用时需要根据具体需求配置EXTI */
    
    /* 注意：STM32F10x的RTC不支持时间戳功能 */
    /* 时间戳功能在STM32F2/F4系列中才支持 */
    /* 这里提供一个占位实现，实际使用时需要根据具体需求调整 */
    
    /* 配置NVIC */
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = RTC_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    return RTC_OK;
}

/**
 * @brief 禁用RTC时间戳功能
 */
RTC_Status_t RTC_DisableTimestamp(void)
{
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    /* 注意：STM32F10x的RTC不支持时间戳功能 */
    
    return RTC_OK;
}

/**
 * @brief 设置RTC时间戳回调函数
 */
RTC_Status_t RTC_SetTimestampCallback(RTC_TimestampCallback_t callback, void *user_data)
{
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    g_rtc_timestamp_callback = callback;
    g_rtc_timestamp_user_data = user_data;
    
    return RTC_OK;
}

/**
 * @brief 获取RTC时间戳
 * @note STM32F10x的RTC时间戳值存储在RTC->TSRL和RTC->TSRH寄存器中
 */
RTC_Status_t RTC_GetTimestamp(RTC_Time_t *timestamp_time)
{
    uint32_t timestamp_counter;
    
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    if (timestamp_time == NULL)
    {
        return RTC_ERROR_INVALID_PARAM;
    }
    
    /* 等待RTC寄存器同步 */
    RTC_WaitForSynchro();
    
    /* 注意：STM32F10x的RTC不支持时间戳功能 */
    /* 时间戳功能在STM32F2/F4系列中才支持 */
    /* 这里返回当前RTC计数器值作为占位实现 */
    timestamp_counter = RTC_GetCounter();
    
    /* 转换为时间结构体 */
    RTC_CounterToTime(timestamp_counter, timestamp_time);
    
    return RTC_OK;
}

/**
 * @brief 检查RTC时间戳标志
 */
uint8_t RTC_CheckTimestampFlag(void)
{
    if (!g_rtc_initialized)
    {
        return 0;
    }
    
    /* 注意：STM32F10x的RTC不支持时间戳功能 */
    /* 时间戳功能在STM32F2/F4系列中才支持 */
    /* 这里返回0作为占位实现 */
    (void)0;  /* 避免未使用变量警告 */
    if (0)
    {
        return 1;
    }
    
    return 0;
}

/**
 * @brief 清除RTC时间戳标志
 */
RTC_Status_t RTC_ClearTimestampFlag(void)
{
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    /* 注意：STM32F10x的RTC不支持时间戳功能 */
    /* 时间戳功能在STM32F2/F4系列中才支持 */
    
    return RTC_OK;
}

/* ========== RTC溢出中断功能实现 ========== */

/**
 * @brief 使能RTC溢出中断
 */
RTC_Status_t RTC_EnableOverflowInterrupt(void)
{
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    /* 使能溢出中断 */
    RTC_ITConfig(RTC_IT_OW, ENABLE);
    
    /* 配置NVIC */
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = RTC_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    return RTC_OK;
}

/**
 * @brief 禁用RTC溢出中断
 */
RTC_Status_t RTC_DisableOverflowInterrupt(void)
{
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    /* 禁用溢出中断 */
    RTC_ITConfig(RTC_IT_OW, DISABLE);
    
    return RTC_OK;
}

/**
 * @brief 设置RTC溢出回调函数
 */
RTC_Status_t RTC_SetOverflowCallback(RTC_OverflowCallback_t callback, void *user_data)
{
    if (!g_rtc_initialized)
    {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    
    g_rtc_overflow_callback = callback;
    g_rtc_overflow_user_data = user_data;
    
    return RTC_OK;
}

#endif /* CONFIG_MODULE_RTC_ENABLED */

