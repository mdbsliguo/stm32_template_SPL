/**
 * @file main_example.c
 * @brief RTC02案例 - DS3231闹钟功能示例
 * @version 1.0.0
 * @date 2024-01-01
 * @details 本案例演示DS3231实时时钟模块的闹钟功能
 *          - 写入两个闹钟：使用随机日期和时间设置Alarm1和Alarm2
 *          - 读取两个闹钟：读取并显示两个闹钟设置，验证写入是否成功
 *          - 触发闹钟：检测DS3231硬件闹钟触发（Alarm1或Alarm2），显示提示信息
 *          - 显示闹钟值：实时显示两个闹钟的设置值
 *          - 按键清除闹钟标志
 * @note 使用软件I2C接口（SoftI2C2，PB10/11）连接DS3231
 * @note 使用GPIO（PA0）读取按键
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "led.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include "i2c_sw.h"
#include "ds3231.h"
#include "gpio.h"
#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief 星期名称数组
 * @note 索引0为空字符串，索引1-7对应Sunday-Saturday
 */
static const char *weekday_names[] = {
    "", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

/**
 * @brief 错误处理函数（显示错误信息并LED闪烁）
 * @param[in] msg 错误信息字符串
 * @param[in] error_code 错误代码（可选，如果为0则不显示）
 */
static void ErrorHandler(const char *msg, int error_code)
{
    OLED_Clear();
    OLED_ShowString(1, 1, msg);
    if (error_code != 0)
    {
        char err_buf[17];
        snprintf(err_buf, sizeof(err_buf), "Error: %d", error_code);
        OLED_ShowString(2, 1, err_buf);
    }
    Delay_ms(3000);
    while (1) 
    { 
        LED_Toggle(LED_1); 
        Delay_ms(200); 
    }
}

/**
 * @brief 在OLED上显示时间信息
 * @param[in] time 指向时间结构体的指针
 * @note 第1行显示日期和星期，第2行显示时间
 */
static void DisplayTime(const DS3231_Time_t *time)
{
    char buffer[17];
    const char *weekday_str;
    uint8_t i;
    
    /* 获取星期名称，防止数组越界 */
    if (time->weekday >= 1 && time->weekday <= 7)
    {
        weekday_str = weekday_names[time->weekday];
    }
    else
    {
        weekday_str = "???";
    }
    
    /* 第1行：显示日期和星期（格式：2024-01-01 Mon） */
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %s",
             time->year, time->month, time->day, weekday_str);
    OLED_ShowString(1, 1, buffer);
    /* 清除行尾残留字符 */
    for (i = strlen(buffer) + 1; i <= 16; i++)
    {
        OLED_ShowChar(1, i, ' ');
    }
    
    /* 第2行：显示时间（格式：12:00:30） */
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d",
             time->hour, time->minute, time->second);
    OLED_ShowString(2, 1, buffer);
    /* 清除行尾残留字符 */
    for (i = strlen(buffer) + 1; i <= 16; i++)
    {
        OLED_ShowChar(2, i, ' ');
    }
}

/**
 * @brief 在OLED上显示闹钟设置信息（Alarm1：年月日时分秒）
 * @param[in] alarm 指向闹钟结构体的指针
 * @param[in] current_time 当前时间（用于获取年月）
 * @param[in] row OLED行号（1-4）
 * @note 显示完整的年月日时分秒格式
 */
static void DisplayAlarm(const DS3231_Alarm_t *alarm, const DS3231_Time_t *current_time, uint8_t row)
{
    char buffer[17];
    uint8_t i;
    
    /* 根据闹钟模式显示不同的信息 */
    if (alarm->mode == DS3231_ALARM_MODE_DAY_HOUR_MIN_SEC_MATCH)
    {
        /* 日期时分秒匹配模式：显示年月日时分秒（紧凑格式） */
        snprintf(buffer, sizeof(buffer), "%02d/%d/%d %02d:%02d:%02d",
                 current_time->year % 100, current_time->month, alarm->day_or_weekday,
                 alarm->hour, alarm->minute, alarm->second);
    }
    else if (alarm->mode == DS3231_ALARM_MODE_WEEKDAY_HOUR_MIN_SEC_MATCH)
    {
        /* 星期时分秒匹配模式：计算下一个该星期几的日期 */
        uint8_t target_weekday = alarm->day_or_weekday;
        uint8_t current_weekday = current_time->weekday;
        uint8_t days_offset = 0;
        
        /* 计算距离下一个目标星期几还有多少天 */
        if (target_weekday > current_weekday)
        {
            days_offset = target_weekday - current_weekday;
        }
        else if (target_weekday < current_weekday)
        {
            days_offset = 7 - current_weekday + target_weekday;
        }
        else
        {
            /* 如果今天就是目标星期，检查时间是否已过 */
            if (alarm->hour < current_time->hour || 
                (alarm->hour == current_time->hour && alarm->minute < current_time->minute) ||
                (alarm->hour == current_time->hour && alarm->minute == current_time->minute && alarm->second <= current_time->second))
            {
                days_offset = 7;  /* 下周 */
            }
        }
        
        /* 计算目标日期 */
        uint16_t target_year = current_time->year;
        uint8_t target_month = current_time->month;
        uint8_t target_day = current_time->day + days_offset;
        
        /* 处理日期溢出（简化处理，假设每月最多31天） */
        uint8_t days_in_month = 31;  /* 简化处理 */
        if (target_day > days_in_month)
        {
            target_day -= days_in_month;
            target_month++;
            if (target_month > 12)
            {
                target_month = 1;
                target_year++;
            }
        }
        
        snprintf(buffer, sizeof(buffer), "%02d/%d/%d %02d:%02d:%02d",
                 target_year % 100, target_month, target_day,
                 alarm->hour, alarm->minute, alarm->second);
    }
    else if (alarm->mode == DS3231_ALARM_MODE_HOUR_MIN_SEC_MATCH)
    {
        /* 时分秒匹配模式：使用当前日期 */
        snprintf(buffer, sizeof(buffer), "%02d/%d/%d %02d:%02d:%02d",
                 current_time->year % 100, current_time->month, current_time->day,
                 alarm->hour, alarm->minute, alarm->second);
    }
    else if (alarm->mode == DS3231_ALARM_MODE_MIN_SEC_MATCH)
    {
        /* 分秒匹配模式：使用当前日期和小时 */
        snprintf(buffer, sizeof(buffer), "%02d/%d/%d %02d:%02d:%02d",
                 current_time->year % 100, current_time->month, current_time->day,
                 current_time->hour, alarm->minute, alarm->second);
    }
    else if (alarm->mode == DS3231_ALARM_MODE_SECOND_MATCH)
    {
        /* 秒匹配模式：使用当前日期和时分 */
        snprintf(buffer, sizeof(buffer), "%02d/%d/%d %02d:%02d:%02d",
                 current_time->year % 100, current_time->month, current_time->day,
                 current_time->hour, current_time->minute, alarm->second);
    }
    else
    {
        /* 其他模式 */
        snprintf(buffer, sizeof(buffer), "Mode %d", alarm->mode);
    }
    
    OLED_ShowString(row, 1, buffer);
    /* 清除行尾残留字符 */
    for (i = strlen(buffer) + 1; i <= 16; i++)
    {
        OLED_ShowChar(row, i, ' ');
    }
}

/**
 * @brief 在OLED上显示闹钟2设置信息（Alarm2：年月日时分秒，秒字段为00）
 * @param[in] alarm 指向闹钟结构体的指针
 * @param[in] current_time 当前时间（用于获取年月）
 * @param[in] row OLED行号（1-4）
 * @note 显示完整的年月日时分秒格式（秒字段固定为00）
 */
static void DisplayAlarm2(const DS3231_Alarm_t *alarm, const DS3231_Time_t *current_time, uint8_t row)
{
    char buffer[17];
    uint8_t i;
    
    /* Alarm2没有秒字段，秒固定显示为00 */
    if (alarm->mode == DS3231_ALARM_MODE_WEEKDAY_HOUR_MIN_SEC_MATCH)
    {
        /* 星期时分匹配模式：计算下一个该星期几的日期 */
        uint8_t target_weekday = alarm->day_or_weekday;
        uint8_t current_weekday = current_time->weekday;
        uint8_t days_offset = 0;
        
        /* 计算距离下一个目标星期几还有多少天 */
        if (target_weekday > current_weekday)
        {
            days_offset = target_weekday - current_weekday;
        }
        else if (target_weekday < current_weekday)
        {
            days_offset = 7 - current_weekday + target_weekday;
        }
        else
        {
            /* 如果今天就是目标星期，检查时间是否已过 */
            if (alarm->hour < current_time->hour || 
                (alarm->hour == current_time->hour && alarm->minute < current_time->minute))
            {
                days_offset = 7;  /* 下周 */
            }
        }
        
        /* 计算目标日期 */
        uint16_t target_year = current_time->year;
        uint8_t target_month = current_time->month;
        uint8_t target_day = current_time->day + days_offset;
        
        /* 处理日期溢出（简化处理，假设每月最多31天） */
        uint8_t days_in_month = 31;  /* 简化处理 */
        if (target_day > days_in_month)
        {
            target_day -= days_in_month;
            target_month++;
            if (target_month > 12)
            {
                target_month = 1;
                target_year++;
            }
        }
        
        snprintf(buffer, sizeof(buffer), "%02d/%d/%d %02d:%02d:00",
                 target_year % 100, target_month, target_day,
                 alarm->hour, alarm->minute);
    }
    else if (alarm->mode == DS3231_ALARM_MODE_DAY_HOUR_MIN_SEC_MATCH)
    {
        /* 日期时分匹配模式：显示年月日时分秒（秒为00，紧凑格式） */
        snprintf(buffer, sizeof(buffer), "%02d/%d/%d %02d:%02d:00",
                 current_time->year % 100, current_time->month, alarm->day_or_weekday,
                 alarm->hour, alarm->minute);
    }
    else if (alarm->mode == DS3231_ALARM_MODE_HOUR_MIN_SEC_MATCH)
    {
        /* 时分匹配模式：使用当前日期 */
        snprintf(buffer, sizeof(buffer), "%02d/%d/%d %02d:%02d:00",
                 current_time->year % 100, current_time->month, current_time->day,
                 alarm->hour, alarm->minute);
    }
    else if (alarm->mode == DS3231_ALARM_MODE_MIN_SEC_MATCH)
    {
        /* 分匹配模式：使用当前日期和小时 */
        snprintf(buffer, sizeof(buffer), "%02d/%d/%d %02d:%02d:00",
                 current_time->year % 100, current_time->month, current_time->day,
                 current_time->hour, alarm->minute);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "Mode %d", alarm->mode);
    }
    
    OLED_ShowString(row, 1, buffer);
    /* 清除行尾残留字符 */
    for (i = strlen(buffer) + 1; i <= 16; i++)
    {
        OLED_ShowChar(row, i, ' ');
    }
}

/**
 * @brief 主函数
 * @return int 程序返回值（本程序不会返回）
 * @note 程序流程：初始化 → 写入两个闹钟（随机日期时间） → 读取两个闹钟 → 显示闹钟值 → 检测触发 → 按键清除
 */
int main(void)
{
    SoftI2C_Status_t soft_i2c_status;
    DS3231_Status_t ds3231_status;
    DS3231_Config_t ds3231_config;
    DS3231_Time_t time_read;
    DS3231_Alarm_t alarm1;
    DS3231_Alarm_t alarm2;
    DS3231_Alarm_t alarm1_read;  /* 用于存储读取到的Alarm1设置 */
    DS3231_Alarm_t alarm2_read;  /* 用于存储读取到的Alarm2设置 */
    OLED_Status_t oled_status;
    GPIO_Status_t gpio_status;
    uint8_t alarm1_flag = 0;
    uint8_t alarm2_flag = 0;
    uint8_t alarm_triggered = 0;
    uint32_t last_display_tick = 0;
    
    /* 步骤1：系统初始化 */
    System_Init();
    
    /* 初始化LED模块 */
    if (LED_Init() != LED_OK)
    {
        while (1) { }
    }
    
    /* 初始化OLED显示模块 */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        ErrorHandler("OLED Init Fail!", oled_status);
    }
    
    OLED_Clear();
    OLED_ShowString(1, 1, "DS3231 Alarm");
    OLED_ShowString(2, 1, "Initializing...");
    Delay_ms(1000);
    
    /* 步骤2：初始化软件I2C2（用于DS3231） */
    OLED_Clear();
    OLED_ShowString(1, 1, "Init SoftI2C2...");
    Delay_ms(300);
    soft_i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_2);
    if (soft_i2c_status != SOFT_I2C_OK)
    {
        ErrorHandler("SoftI2C Init Fail!", soft_i2c_status);
    }
    OLED_ShowString(2, 1, "SoftI2C2 OK");
    Delay_ms(500);
    
    /* 步骤3：初始化DS3231模块 */
    OLED_ShowString(1, 1, "Init DS3231...");
    Delay_ms(300);
    ds3231_config.interface_type = DS3231_INTERFACE_SOFTWARE;
    ds3231_config.config.software.soft_i2c_instance = SOFT_I2C_INSTANCE_2;
    
    ds3231_status = DS3231_Init(&ds3231_config);
    if (ds3231_status != DS3231_OK)
    {
        ErrorHandler("DS3231 Init Fail!", ds3231_status);
    }
    OLED_ShowString(2, 1, "DS3231 OK");
    Delay_ms(1000);
    
    /* 步骤4：检查并清除OSF标志（振荡器停止标志） */
    OLED_Clear();
    OLED_ShowString(1, 1, "Check OSF...");
    Delay_ms(300);
    uint8_t osf_flag;
    ds3231_status = DS3231_CheckOSF(&osf_flag);
    if (ds3231_status == DS3231_OK && osf_flag)
    {
        OLED_ShowString(2, 1, "OSF Detected!");
        Delay_ms(500);
        DS3231_ClearOSF();
        OLED_ShowString(3, 1, "OSF Cleared");
        Delay_ms(1000);
    }
    else
    {
        OLED_ShowString(2, 1, "OSF OK");
        Delay_ms(500);
    }
    
    /* 启动DS3231振荡器 */
    DS3231_Start();
    
    /* 清除可能已存在的闹钟标志（防止启动时误触发）
     * 
     * 原因：DS3231芯片可能之前已经触发过闹钟，状态寄存器(0x0F)的A1F标志位可能已经是1
     *      如果不清除，程序启动时立即检测到标志位为1，会误认为闹钟已触发
     * 方法：通过I2C写入DS3231状态寄存器(0x0F)，将A1F位写0
     * 时机：在DS3231初始化后立即清除，确保从干净的状态开始
     * 注意：即使没有接SQW线，状态寄存器中的标志位仍可能被设置，需要软件清除
     */
    DS3231_ClearAlarm1Flag();
    DS3231_ClearAlarm2Flag();  /* 同时清除Alarm2标志，确保干净 */
    
    /* 步骤5：配置按键GPIO（PA0，下拉输入） */
    OLED_Clear();
    OLED_ShowString(1, 1, "Init Key...");
    Delay_ms(300);
    gpio_status = GPIO_Config(GPIOA, GPIO_Pin_0, GPIO_MODE_INPUT_PULLDOWN, GPIO_SPEED_50MHz);
    if (gpio_status != GPIO_OK)
    {
        ErrorHandler("Key Init Fail!", gpio_status);
    }
    OLED_ShowString(2, 1, "Key OK (PA0)");
    Delay_ms(500);
    
    /* 步骤6：第一次读取闹钟值（读取DS3231中已设置的闹钟值） */
    OLED_Clear();
    OLED_ShowString(1, 1, "Reading Alarms...");
    Delay_ms(300);
    
    /* 读取当前时间（用于显示闹钟的完整日期和计算新闹钟） */
    DS3231_Time_t current_time;
    ds3231_status = DS3231_ReadTime(&current_time);
    if (ds3231_status != DS3231_OK)
    {
        ErrorHandler("Read Time Fail!", ds3231_status);
    }
    
    /* 第一次读取：读取DS3231中已设置的闹钟值 */
    if (DS3231_ReadAlarm1(&alarm1_read) != DS3231_OK)
    {
        ErrorHandler("Read Alarm1 Fail!", ds3231_status);
    }
    if (DS3231_ReadAlarm2(&alarm2_read) != DS3231_OK)
    {
        ErrorHandler("Read Alarm2 Fail!", ds3231_status);
    }
    
    /* 显示第一次读取的闹钟值（读取DS3231中已存在的值） */
    DisplayAlarm(&alarm1_read, &current_time, 1);
    DisplayAlarm2(&alarm2_read, &current_time, 2);
    OLED_ShowString(3, 1, "1st Read");
    Delay_ms(2000);
    OLED_Clear();
    
    /* 注意：此时alarm1_read和alarm2_read保存的是第一次读取的值（旧值） */
    
    /* 步骤7：设置两个闹钟（使用当前时间+5分钟和+10分钟） */
    OLED_ShowString(1, 1, "Setting Alarms...");
    Delay_ms(500);
    
    /* 计算Alarm1时间：当前时间+5分钟 */
    uint8_t alarm1_minute = current_time.minute + 5;
    uint8_t alarm1_hour = current_time.hour;
    uint8_t alarm1_day = current_time.day;
    
    /* 处理分钟溢出（超过59分钟） */
    if (alarm1_minute >= 60)
    {
        alarm1_minute -= 60;
        alarm1_hour++;
    }
    
    /* 处理小时溢出（超过23小时） */
    if (alarm1_hour >= 24)
    {
        alarm1_hour -= 24;
        alarm1_day++;
        /* 简化处理：如果日期超过28，设为1（避免月份差异） */
        if (alarm1_day > 28)
        {
            alarm1_day = 1;
        }
    }
    
    /* 配置闹钟1：日期时分秒匹配模式（当前时间+5分钟） */
    alarm1.mode = DS3231_ALARM_MODE_DAY_HOUR_MIN_SEC_MATCH;
    alarm1.second = current_time.second;  /* 使用当前秒数 */
    alarm1.minute = alarm1_minute;
    alarm1.hour = alarm1_hour;
    alarm1.day_or_weekday = alarm1_day;
    
    /* 计算Alarm2时间：当前时间+10分钟（星期匹配模式） */
    uint8_t alarm2_minute = current_time.minute + 10;
    uint8_t alarm2_hour = current_time.hour;
    
    /* 处理分钟溢出 */
    if (alarm2_minute >= 60)
    {
        alarm2_minute -= 60;
        alarm2_hour++;
    }
    
    /* 处理小时溢出 */
    if (alarm2_hour >= 24)
    {
        alarm2_hour -= 24;
    }
    
    /* 配置闹钟2：星期时分匹配模式（当前时间+10分钟，Alarm2没有秒字段） */
    alarm2.mode = DS3231_ALARM_MODE_WEEKDAY_HOUR_MIN_SEC_MATCH;
    alarm2.second = 0;  /* Alarm2忽略秒字段 */
    alarm2.minute = alarm2_minute;
    alarm2.hour = alarm2_hour;
    alarm2.day_or_weekday = current_time.weekday;  /* 使用当前星期值（1-7），DS3231_SetAlarm2会自动设置bit6=1表示星期匹配 */
    
    /* 写入闹钟1和2 */
    if (DS3231_SetAlarm1(&alarm1) != DS3231_OK)
    {
        ErrorHandler("Set Alarm1 Fail!", ds3231_status);
    }
    if (DS3231_SetAlarm2(&alarm2) != DS3231_OK)
    {
        ErrorHandler("Set Alarm2 Fail!", ds3231_status);
    }
    
    OLED_ShowString(2, 1, "Write Alarms OK!");
    Delay_ms(1000);
    
    /* 第二次读取闹钟值（读取刚写入的新值） */
    if (DS3231_ReadAlarm1(&alarm1_read) != DS3231_OK)
    {
        ErrorHandler("Read Alarm1 Fail!", ds3231_status);
    }
    if (DS3231_ReadAlarm2(&alarm2_read) != DS3231_OK)
    {
        ErrorHandler("Read Alarm2 Fail!", ds3231_status);
    }
    
    OLED_ShowString(2, 1, "Read Alarms OK!");
    /* 第二次读取后，重新读取当前时间（用于显示闹钟的完整日期） */
    ds3231_status = DS3231_ReadTime(&current_time);
    if (ds3231_status != DS3231_OK)
    {
        OLED_ShowString(2, 1, "Read Time Fail!");
        Delay_ms(2000);
    }
    
    /* 显示第二次读取的闹钟值（新设置的） */
    DisplayAlarm(&alarm1_read, &current_time, 1);
    DisplayAlarm2(&alarm2_read, &current_time, 2);
    OLED_ShowString(3, 1, "2nd Read");
    Delay_ms(3000);
    OLED_Clear();
    
    /* 再次清除闹钟标志（确保在使能前标志位为0）
     * 
     * 原因：在设置闹钟的过程中，如果当前时间正好匹配闹钟时间，
     *       DS3231硬件可能会立即设置A1F/A2F标志位
     * 方法：通过I2C写入DS3231状态寄存器(0x0F)，将A1F/A2F位写0
     * 时机：在使能闹钟前清除，确保从干净的状态开始
     */
    DS3231_ClearAlarm1Flag();
    DS3231_ClearAlarm2Flag();
    
    /* 使能两个闹钟并设置中断模式 */
    if (DS3231_EnableAlarm1() != DS3231_OK)
    {
        ErrorHandler("Enable A1 Fail!", ds3231_status);
    }
    if (DS3231_EnableAlarm2() != DS3231_OK)
    {
        ErrorHandler("Enable A2 Fail!", ds3231_status);
    }
    if (DS3231_SetInterruptMode(DS3231_INT_MODE_ALARM) != DS3231_OK)
    {
        ErrorHandler("Set INT Fail!", ds3231_status);
    }
    
    OLED_ShowString(2, 1, "Alarms Enable OK!");
    Delay_ms(1000);
    OLED_Clear();
    
    /* LED1开机闪烁3次 */
    LED_Off(LED_1);  /* 先关闭 */
    Delay_ms(100);
    LED_On(LED_1);
    Delay_ms(200);
    LED_Off(LED_1);
    Delay_ms(200);
    LED_On(LED_1);
    Delay_ms(200);
    LED_Off(LED_1);
    Delay_ms(200);
    LED_On(LED_1);
    Delay_ms(200);
    LED_Off(LED_1);  /* 最后关闭，之后不再亮（除非触发） */
    Delay_ms(200);
    
    /* 初始化显示时间（立即显示一次，避免等待1秒） */
    ds3231_status = DS3231_ReadTime(&time_read);
    if (ds3231_status == DS3231_OK)
    {
        DisplayTime(&time_read);
        /* 显示两个闹钟的值 */
        DisplayAlarm(&alarm1_read, &time_read, 3);
        DisplayAlarm2(&alarm2_read, &time_read, 4);
    }
    
    /* 初始化last_display_tick为当前tick，避免第一次立即触发 */
    last_display_tick = Delay_GetTick();
    
    /* 步骤8：主循环 - 显示时间、倒计时、检测闹钟、处理响铃 */
    while (1)
    {
        uint32_t current_tick = Delay_GetTick();
        
        /* 每秒更新一次时间显示和倒计时 */
        if (Delay_GetElapsed(current_tick, last_display_tick) >= 1000)
        {
            last_display_tick = current_tick;
            
            /* 读取DS3231当前时间 */
            ds3231_status = DS3231_ReadTime(&time_read);
            if (ds3231_status == DS3231_OK)
            {
                DisplayTime(&time_read);
                
                /* 显示两个闹钟的值（仅在未触发时显示） */
                if (!alarm_triggered)
                {
                    DisplayAlarm(&alarm1_read, &time_read, 3);
                    DisplayAlarm2(&alarm2_read, &time_read, 4);
                }
            }
            else
            {
                OLED_ShowString(3, 1, "Read Time Fail!");
            }
        }
        
        /* 检查DS3231硬件闹钟1标志（由硬件控制触发） */
        ds3231_status = DS3231_CheckAlarm1Flag(&alarm1_flag);
        if (ds3231_status == DS3231_OK && alarm1_flag)
        {
            /* DS3231硬件闹钟1触发 */
            if (!alarm_triggered)
            {
                alarm_triggered = 1;
                /* LED1常亮（触发时） */
                LED_On(LED_1);
                /* 显示闹钟触发信息 */
                OLED_ShowString(3, 1, "Alarm1 Triggered!");
                OLED_ShowString(4, 1, "Press Key Clear  ");
            }
        }
        
        /* 检查DS3231硬件闹钟2标志（由硬件控制触发） */
        ds3231_status = DS3231_CheckAlarm2Flag(&alarm2_flag);
        if (ds3231_status == DS3231_OK && alarm2_flag)
        {
            /* DS3231硬件闹钟2触发 */
            if (!alarm_triggered)
            {
                alarm_triggered = 1;
                /* LED1常亮（触发时） */
                LED_On(LED_1);
                /* 显示闹钟触发信息 */
                OLED_ShowString(3, 1, "Alarm2 Triggered!");
                OLED_ShowString(4, 1, "Press Key Clear  ");
            }
        }
        
        /* 检测按键按下（清除闹钟标志） */
        if (GPIO_ReadPin(GPIOA, GPIO_Pin_0) == Bit_SET)
        {
            /* 按键按下，清除两个闹钟标志 */
            DS3231_ClearAlarm1Flag();
            DS3231_ClearAlarm2Flag();
            alarm_triggered = 0;
            
            /* 关闭LED1（清除后不再亮） */
            LED_Off(LED_1);
            
            /* 清除显示 */
            OLED_ShowString(3, 1, "Alarms Cleared   ");
            OLED_ShowString(4, 1, "                ");
            Delay_ms(1000);
            OLED_ShowString(3, 1, "                ");
        }
        
        Delay_ms(10);
    }
}

