/**
 * @file rtc.c
 * @brief RTC 实时时钟模块实现（STM32F10x 内部 RTC，LSE/LSI）
 */

#include "rtc.h"
#include "board.h"
#include "config.h"
#include "stm32f10x.h"
#include "stm32f10x_rtc.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_bkp.h"
#include "stm32f10x_pwr.h"
#include <string.h>

#if CONFIG_MODULE_RTC_ENABLED

#define RTC_PRESCALER_LSE             0x7FFFu
#define RTC_PRESCALER_LSI             0x9C3Fu
#define RTC_PRESCALER_HSE             0x7FFFu

#define RTC_YEAR_MIN                1970u
#define RTC_YEAR_MAX                2099u
#define RTC_LSE_STARTUP_TIMEOUT     0x1FFFFFu

static const uint8_t s_days_in_month[] = {
    31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u
};

static const uint8_t s_week_adjust[] = {
    0u, 3u, 3u, 6u, 1u, 4u, 6u, 2u, 5u, 0u, 3u, 5u
};

static uint8_t g_rtc_initialized = 0u;
static RTC_ClockSource_t g_rtc_clock_source = RTC_CLOCK_SOURCE_LSI;

static uint8_t RTC_IsLeapYear(uint16_t year)
{
    if ((year % 4u) != 0u) {
        return 0u;
    }
    if ((year % 100u) == 0u) {
        return ((year % 400u) == 0u) ? 1u : 0u;
    }
    return 1u;
}

static uint8_t RTC_GetDaysInMonth(uint16_t year, uint8_t month)
{
    if (month < 1u || month > 12u) {
        return 0u;
    }
    if (month == 2u && RTC_IsLeapYear(year)) {
        return 29u;
    }
    return s_days_in_month[month - 1u];
}

static uint8_t RTC_CalcWeekday(uint16_t year, uint8_t month, uint8_t day)
{
    uint8_t year_h;
    uint8_t year_l;
    uint16_t temp;

    if (month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0u;
    }

    year_h = (uint8_t)(year / 100u);
    year_l = (uint8_t)(year % 100u);
    if (year_h > 19u) {
        year_l = (uint8_t)(year_l + 100u);
    }

    temp = (uint16_t)year_l + (uint16_t)(year_l / 4u);
    temp = (uint16_t)((temp % 7u) + day + s_week_adjust[month - 1u]);
    if ((year_l % 4u) == 0u && month < 3u) {
        temp--;
    }
    return (uint8_t)(temp % 7u);
}

static uint32_t RTC_TimeToCounter(const RTC_Time_t *time)
{
    uint32_t seccount = 0u;
    uint16_t year;

    if (time == NULL) {
        return 0u;
    }

    for (year = RTC_YEAR_MIN; year < time->year; year++) {
        seccount += RTC_IsLeapYear(year) ? 31622400u : 31536000u;
    }

    for (year = 1u; year < time->month; year++) {
        seccount += (uint32_t)RTC_GetDaysInMonth(time->year, (uint8_t)year) * 86400u;
    }

    seccount += (uint32_t)(time->day - 1u) * 86400u;
    seccount += (uint32_t)time->hour * 3600u;
    seccount += (uint32_t)time->minute * 60u;
    seccount += (uint32_t)time->second;
    return seccount;
}

static void RTC_CounterToTime(uint32_t counter, RTC_Time_t *time)
{
    uint32_t days;
    uint32_t remain;
    uint16_t year;
    uint8_t month;

    if (time == NULL) {
        return;
    }

    memset(time, 0, sizeof(RTC_Time_t));
    days = counter / 86400u;
    remain = counter % 86400u;

    time->hour = (uint8_t)(remain / 3600u);
    remain %= 3600u;
    time->minute = (uint8_t)(remain / 60u);
    time->second = (uint8_t)(remain % 60u);

    year = RTC_YEAR_MIN;
    while (days >= 365u) {
        if (RTC_IsLeapYear(year)) {
            if (days >= 366u) {
                days -= 366u;
            } else {
                break;
            }
        } else {
            days -= 365u;
        }
        year++;
    }

    time->year = year;
    month = 1u;
    while (days >= 28u) {
        uint8_t dim = RTC_GetDaysInMonth(year, month);
        if (days >= dim) {
            days -= dim;
            month++;
        } else {
            break;
        }
    }

    time->month = month;
    time->day = (uint8_t)(days + 1u);
    time->weekday = RTC_CalcWeekday(time->year, time->month, time->day);
}

static uint32_t RTC_GetPrescalerValue(RTC_ClockSource_t clock_source)
{
    switch (clock_source) {
    case RTC_CLOCK_SOURCE_LSE:
        return RTC_PRESCALER_LSE;
    case RTC_CLOCK_SOURCE_LSI:
        return RTC_PRESCALER_LSI;
    case RTC_CLOCK_SOURCE_HSE_DIV128:
    default:
        return RTC_PRESCALER_HSE;
    }
}

static RTC_Status_t RTC_WaitOscReady(uint32_t flag, uint32_t timeout)
{
    while (RCC_GetFlagStatus(flag) == RESET) {
        if (timeout == 0u) {
            return RTC_ERROR_TIMEOUT;
        }
        timeout--;
    }
    return RTC_OK;
}

static RTC_Status_t RTC_ConfigClockSource(RTC_ClockSource_t request,
                                          RTC_ClockSource_t *actual)
{
    RTC_Status_t st;

    if (actual == NULL) {
        return RTC_ERROR_INVALID_PARAM;
    }

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);

    if (request == RTC_CLOCK_SOURCE_LSI) {
        RCC_LSICmd(ENABLE);
        st = RTC_WaitOscReady(RCC_FLAG_LSIRDY, RTC_LSE_STARTUP_TIMEOUT);
        if (st != RTC_OK) {
            return st;
        }
        RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
        *actual = RTC_CLOCK_SOURCE_LSI;
        return RTC_OK;
    }

    if (request == RTC_CLOCK_SOURCE_HSE_DIV128) {
        RCC_RTCCLKConfig(RCC_RTCCLKSource_HSE_Div128);
        *actual = RTC_CLOCK_SOURCE_HSE_DIV128;
        return RTC_OK;
    }

    RCC_LSEConfig(RCC_LSE_ON);
    st = RTC_WaitOscReady(RCC_FLAG_LSERDY, RTC_LSE_STARTUP_TIMEOUT);
    if (st == RTC_OK) {
        RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
        *actual = RTC_CLOCK_SOURCE_LSE;
        return RTC_OK;
    }

    RCC_LSEConfig(RCC_LSE_OFF);
    RCC_LSICmd(ENABLE);
    st = RTC_WaitOscReady(RCC_FLAG_LSIRDY, RTC_LSE_STARTUP_TIMEOUT);
    if (st != RTC_OK) {
        return st;
    }
    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
    *actual = RTC_CLOCK_SOURCE_LSI;
    return RTC_OK;
}

static RTC_Status_t RTC_EnsureHardwareReady(RTC_ClockSource_t request)
{
    RTC_ClockSource_t actual;
    RTC_Status_t st;
    uint16_t magic;

    st = RTC_ConfigClockSource(request, &actual);
    if (st != RTC_OK) {
        return st;
    }

    g_rtc_clock_source = actual;

    if ((RCC->BDCR & RCC_BDCR_RTCEN) == 0u) {
        RCC_RTCCLKCmd(ENABLE);
    }

    RTC_WaitForSynchro();
    RTC_WaitForLastTask();

    magic = BKP_ReadBackupRegister(RTC_BKP_DR);
    if (magic != RTC_BKP_INIT_MAGIC) {
        RTC_SetPrescaler(RTC_GetPrescalerValue(actual));
        RTC_WaitForLastTask();

        {
            RTC_Time_t default_time;
            uint32_t counter;

            default_time.second = (uint8_t)RTC_DEFAULT_SECOND;
            default_time.minute = (uint8_t)RTC_DEFAULT_MINUTE;
            default_time.hour = (uint8_t)RTC_DEFAULT_HOUR;
            default_time.day = (uint8_t)RTC_DEFAULT_DAY;
            default_time.month = (uint8_t)RTC_DEFAULT_MONTH;
            default_time.year = RTC_DEFAULT_YEAR;
            default_time.weekday = RTC_CalcWeekday(default_time.year,
                                                   default_time.month,
                                                   default_time.day);
            counter = RTC_TimeToCounter(&default_time);
            RTC_EnterConfigMode();
            RTC_SetCounter(counter);
            RTC_ExitConfigMode();
            RTC_WaitForLastTask();
        }

        BKP_WriteBackupRegister(RTC_BKP_DR, RTC_BKP_INIT_MAGIC);
    }

    return RTC_OK;
}

RTC_Status_t RTC_Init(RTC_ClockSource_t clock_source)
{
    RTC_Status_t st;

    if (g_rtc_initialized) {
        return RTC_ERROR_ALREADY_INITIALIZED;
    }

    st = RTC_EnsureHardwareReady(clock_source);
    if (st != RTC_OK) {
        return st;
    }

    g_rtc_initialized = 1u;
    return RTC_OK;
}

RTC_Status_t RTC_Deinit(void)
{
    g_rtc_initialized = 0u;
    return RTC_OK;
}

RTC_Status_t RTC_SetTime(const RTC_Time_t *time)
{
    uint32_t counter;
    uint8_t dim;

    if (!g_rtc_initialized) {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    if (time == NULL) {
        return RTC_ERROR_INVALID_PARAM;
    }
    if (time->year < RTC_YEAR_MIN || time->year > RTC_YEAR_MAX ||
        time->month < 1u || time->month > 12u ||
        time->day < 1u || time->hour > 23u ||
        time->minute > 59u || time->second > 59u) {
        return RTC_ERROR_INVALID_TIME;
    }

    dim = RTC_GetDaysInMonth(time->year, time->month);
    if (time->day > dim) {
        return RTC_ERROR_INVALID_TIME;
    }

    counter = RTC_TimeToCounter(time);

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);

    RTC_EnterConfigMode();
    RTC_SetCounter(counter);
    RTC_ExitConfigMode();
    RTC_WaitForLastTask();

    return RTC_OK;
}

RTC_Status_t RTC_GetTime(RTC_Time_t *time)
{
    if (!g_rtc_initialized) {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    if (time == NULL) {
        return RTC_ERROR_INVALID_PARAM;
    }

    RTC_CounterToTime(RTC_GetCounter(), time);
    return RTC_OK;
}

RTC_Status_t RTC_SetAlarmTime(const RTC_Time_t *time)
{
    uint32_t counter;

    if (!g_rtc_initialized) {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    if (time == NULL) {
        return RTC_ERROR_INVALID_PARAM;
    }

    counter = RTC_TimeToCounter(time);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
    RTC_EnterConfigMode();
    RTC_SetAlarm(counter);
    RTC_ExitConfigMode();
    RTC_WaitForLastTask();
    return RTC_OK;
}

RTC_Status_t RTC_GetAlarmTime(RTC_Time_t *time)
{
    if (!g_rtc_initialized) {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    if (time == NULL) {
        return RTC_ERROR_INVALID_PARAM;
    }

    RTC_CounterToTime((uint32_t)(((uint32_t)RTC->ALRH << 16) | RTC->ALRL), time);
    return RTC_OK;
}

RTC_Status_t RTC_EnableAlarm(void)
{
    if (!g_rtc_initialized) {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    RTC_ITConfig(RTC_IT_ALR, ENABLE);
    return RTC_OK;
}

RTC_Status_t RTC_DisableAlarm(void)
{
    if (!g_rtc_initialized) {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    RTC_ITConfig(RTC_IT_ALR, DISABLE);
    return RTC_OK;
}

RTC_Status_t RTC_SetAlarmCallback(RTC_AlarmCallback_t callback, void *user_data)
{
    (void)callback;
    (void)user_data;
    return RTC_ERROR_NOT_IMPLEMENTED;
}

uint8_t RTC_CheckAlarmFlag(void)
{
    return (RTC_GetITStatus(RTC_IT_ALR) != RESET) ? 1u : 0u;
}

RTC_Status_t RTC_ClearAlarmFlag(void)
{
    if (RTC_GetITStatus(RTC_IT_ALR) != RESET) {
        RTC_ClearITPendingBit(RTC_IT_ALR);
    }
    return RTC_OK;
}

RTC_Status_t RTC_EnableSecondInterrupt(void)
{
    if (!g_rtc_initialized) {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    RTC_ITConfig(RTC_IT_SEC, ENABLE);
    return RTC_OK;
}

RTC_Status_t RTC_DisableSecondInterrupt(void)
{
    if (!g_rtc_initialized) {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    RTC_ITConfig(RTC_IT_SEC, DISABLE);
    return RTC_OK;
}

uint8_t RTC_IsInitialized(void)
{
    return g_rtc_initialized;
}

uint32_t RTC_GetCounterValue(void)
{
    if (!g_rtc_initialized) {
        return 0u;
    }
    return RTC_GetCounter();
}

RTC_Status_t RTC_SetCounterValue(uint32_t counter)
{
    if (!g_rtc_initialized) {
        return RTC_ERROR_NOT_INITIALIZED;
    }

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
    RTC_EnterConfigMode();
    RTC_SetCounter(counter);
    RTC_ExitConfigMode();
    RTC_WaitForLastTask();
    return RTC_OK;
}

RTC_ClockSource_t RTC_GetActiveClockSource(void)
{
    return g_rtc_clock_source;
}

uint16_t RTC_ReadBackupData(uint16_t bkp_dr)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
    return BKP_ReadBackupRegister(bkp_dr);
}

RTC_Status_t RTC_WriteBackupData(uint16_t bkp_dr, uint16_t data)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
    BKP_WriteBackupRegister(bkp_dr, data);
    return RTC_OK;
}

void RTC_ProcessIRQ(void)
{
    if (RTC_GetITStatus(RTC_IT_SEC) != RESET) {
        RTC_ClearITPendingBit(RTC_IT_SEC);
    }
    if (RTC_GetITStatus(RTC_IT_ALR) != RESET) {
        RTC_ClearITPendingBit(RTC_IT_ALR);
    }
    if (RTC_GetITStatus(RTC_IT_OW) != RESET) {
        RTC_ClearITPendingBit(RTC_IT_OW);
    }
}

RTC_Status_t RTC_SetCalibration(int16_t calibration_value)
{
    (void)calibration_value;
    return RTC_ERROR_NOT_IMPLEMENTED;
}

RTC_Status_t RTC_GetCalibration(int16_t *calibration_value)
{
    (void)calibration_value;
    return RTC_ERROR_NOT_IMPLEMENTED;
}

RTC_Status_t RTC_EnableTimestamp(uint8_t trigger_pin)
{
    (void)trigger_pin;
    return RTC_ERROR_NOT_IMPLEMENTED;
}

RTC_Status_t RTC_DisableTimestamp(void)
{
    return RTC_ERROR_NOT_IMPLEMENTED;
}

RTC_Status_t RTC_SetTimestampCallback(RTC_TimestampCallback_t callback, void *user_data)
{
    (void)callback;
    (void)user_data;
    return RTC_ERROR_NOT_IMPLEMENTED;
}

RTC_Status_t RTC_GetTimestamp(RTC_Time_t *timestamp_time)
{
    (void)timestamp_time;
    return RTC_ERROR_NOT_IMPLEMENTED;
}

uint8_t RTC_CheckTimestampFlag(void)
{
    return 0u;
}

RTC_Status_t RTC_ClearTimestampFlag(void)
{
    return RTC_ERROR_NOT_IMPLEMENTED;
}

RTC_Status_t RTC_EnableOverflowInterrupt(void)
{
    if (!g_rtc_initialized) {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    RTC_ITConfig(RTC_IT_OW, ENABLE);
    return RTC_OK;
}

RTC_Status_t RTC_DisableOverflowInterrupt(void)
{
    if (!g_rtc_initialized) {
        return RTC_ERROR_NOT_INITIALIZED;
    }
    RTC_ITConfig(RTC_IT_OW, DISABLE);
    return RTC_OK;
}

RTC_Status_t RTC_SetOverflowCallback(RTC_OverflowCallback_t callback, void *user_data)
{
    (void)callback;
    (void)user_data;
    return RTC_ERROR_NOT_IMPLEMENTED;
}

#endif /* CONFIG_MODULE_RTC_ENABLED */
