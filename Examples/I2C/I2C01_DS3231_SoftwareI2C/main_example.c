/**
 * @file main_example.c
 * @brief 案例12 - DS3231实时时钟模块演示（软件I2C接口）
 * @details 演示DS3231的常规配置流程和常用功能
 * 
 * 硬件连接：
 * - DS3231模块连接到软件I2C
 *   - SCL：PB10
 *   - SDA：PB11
 *   - VCC：3.3V
 *   - GND：GND
 * 
 * 功能演示：
 * 1. DS3231初始化（软件I2C接口）
 * 2. 时间设置和读取
 * 3. 温度读取
 * 4. 闹钟配置（Alarm 1）
 * 5. 方波输出配置
 * 6. 32kHz输出控制
 * 7. 振荡器停止标志检查
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "led.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include "i2c_sw.h"
#include "ds3231.h"
#include "config.h"
#include "error_handler.h"
#include "log.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 星期名称数组 */
static const char *weekday_names[] = {
    "", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

/**
 * @brief 在OLED上显示时间信息
 */
static void DisplayTime(const DS3231_Time_t *time)
{
    char buffer[17];
    const char *weekday_str;
    uint8_t i;
    
    /* 安全获取星期名称（防止数组越界） */
    if (time->weekday >= 1 && time->weekday <= 7)
    {
        weekday_str = weekday_names[time->weekday];
    }
    else
    {
        weekday_str = "???";  /* 无效的weekday值 */
    }
    
    /* 第1行：日期（格式：2024-01-01 Sun） */
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %s",
             time->year, time->month, time->day, weekday_str);
    OLED_ShowString(1, 1, buffer);
    /* 清除行尾残留字符 */
    for (i = strlen(buffer) + 1; i <= 16; i++)
    {
        OLED_ShowChar(1, i, ' ');
    }
    
    /* 第2行：时间（格式：12:01:12） */
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
 * @brief 在OLED上显示温度信息
 */
static void DisplayTemperature(int16_t temperature)
{
    int8_t temp_int = temperature / 100;
    uint8_t temp_frac = (temperature < 0) ? (-temperature % 100) : (temperature % 100);
    char buffer[17];
    uint8_t i;
    
    /* 温度显示（格式：Temp: 1.05C） */
    if (temperature < 0)
    {
        snprintf(buffer, sizeof(buffer), "Temp:-%d.%02dC", temp_int, temp_frac);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "Temp: %d.%02dC", temp_int, temp_frac);
    }
    OLED_ShowString(3, 1, buffer);
    /* 清除行尾残留字符 */
    for (i = strlen(buffer) + 1; i <= 16; i++)
    {
        OLED_ShowChar(3, i, ' ');
    }
}

/**
 * @brief DS3231常规配置流程（在OLED上显示）
 */
static void DS3231_ConfigRoutine(void)
{
    DS3231_Status_t status;
    uint8_t osf_flag;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "DS3231 Config");
    Delay_ms(500);
    
    /* 1. Check and clear OSF flag */
    OLED_ShowString(2, 1, "1.Check OSF...");
    Delay_ms(300);
    status = DS3231_CheckOSF(&osf_flag);
    if (status != DS3231_OK)
    {
        OLED_ShowString(3, 1, "OSF Check Fail!");
        Delay_ms(2000);
        return;
    }
    
    if (osf_flag)
    {
        OLED_ShowString(3, 1, "OSF Detected!");
        Delay_ms(500);
        status = DS3231_ClearOSF();
        if (status != DS3231_OK)
        {
            OLED_ShowString(4, 1, "Clear Fail!");
            Delay_ms(2000);
            return;
        }
        OLED_ShowString(4, 1, "OSF Cleared");
        Delay_ms(500);
    }
    else
    {
        OLED_ShowString(3, 1, "OSF OK");
        Delay_ms(500);
    }
    
    /* 2. Start oscillator */
    OLED_ShowString(2, 1, "2.Start OSC...");
    Delay_ms(300);
    status = DS3231_Start();
    if (status != DS3231_OK)
    {
        OLED_ShowString(3, 1, "Start Fail!");
        Delay_ms(2000);
        return;
    }
    OLED_ShowString(3, 1, "OSC Started");
    Delay_ms(500);
    
    /* 3. Configure square wave output (1Hz) */
    OLED_ShowString(2, 1, "3.Config SQW...");
    Delay_ms(300);
    status = DS3231_SetSquareWave(DS3231_SQW_1HZ, 1);
    if (status != DS3231_OK)
    {
        OLED_ShowString(3, 1, "SQW Config Fail!");
        Delay_ms(2000);
        return;
    }
    OLED_ShowString(3, 1, "SQW: 1Hz OK");
    Delay_ms(500);
    
    /* 4. Disable 32kHz output (save power) */
    OLED_ShowString(2, 1, "4.Disable 32k...");
    Delay_ms(300);
    status = DS3231_Disable32kHz();
    if (status != DS3231_OK)
    {
        OLED_ShowString(3, 1, "32k Disable Fail!");
        Delay_ms(2000);
        return;
    }
    OLED_ShowString(3, 1, "32k Disabled");
    Delay_ms(500);
    
    /* 5. Configure interrupt mode to square wave */
    OLED_ShowString(2, 1, "5.Config INT...");
    Delay_ms(300);
    status = DS3231_SetInterruptMode(DS3231_INT_MODE_SQUARE_WAVE);
    if (status != DS3231_OK)
    {
        OLED_ShowString(3, 1, "INT Config Fail!");
        Delay_ms(2000);
        return;
    }
    OLED_ShowString(3, 1, "INT: SQW OK");
    Delay_ms(500);
    
    OLED_ShowString(4, 1, "Config Complete!");
    Delay_ms(1000);
}

/**
 * @brief DS3231常用功能示例（在OLED上显示）
 */
static void DS3231_FunctionDemo(void)
{
    DS3231_Status_t status;
    DS3231_Time_t time;
    int16_t temperature;
    uint8_t alarm_flag;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "DS3231 Demo");
    Delay_ms(500);
    
    /* Example 1: Read current time */
    OLED_ShowString(2, 1, "1.Read Time...");
    Delay_ms(300);
    status = DS3231_ReadTime(&time);
    if (status == DS3231_OK)
    {
        DisplayTime(&time);
        Delay_ms(2000);
    }
    else
    {
        OLED_ShowString(3, 1, "Read Fail!");
        Delay_ms(2000);
    }
    
    /* Example 2: Set time */
    OLED_Clear();
    OLED_ShowString(1, 1, "2.Set Time...");
    Delay_ms(300);
    time.year = 2024;
    time.month = 1;
    time.day = 1;
    time.weekday = 1;  /* Monday */
    time.hour = 12;
    time.minute = 0;
    time.second = 0;
    
    status = DS3231_SetTime(&time);
    if (status == DS3231_OK)
    {
        OLED_ShowString(2, 1, "Time Set OK");
        Delay_ms(500);
        
        /* Verify setting */
        Delay_ms(100);
        status = DS3231_ReadTime(&time);
        if (status == DS3231_OK)
        {
            DisplayTime(&time);
            Delay_ms(2000);
        }
    }
    else
    {
        OLED_ShowString(2, 1, "Set Fail!");
        Delay_ms(2000);
    }
    
    /* Example 3: Read temperature */
    OLED_Clear();
    OLED_ShowString(1, 1, "3.Read Temp...");
    Delay_ms(300);
    status = DS3231_ReadTemperature(&temperature);
    if (status == DS3231_OK)
    {
        DisplayTemperature(temperature);
        Delay_ms(2000);
    }
    else
    {
        OLED_ShowString(2, 1, "Read Fail!");
        Delay_ms(2000);
    }
    
    /* Example 4: Configure alarm 1 (trigger at 30th second every minute) */
    OLED_Clear();
    OLED_ShowString(1, 1, "4.Set Alarm1...");
    Delay_ms(300);
    DS3231_Alarm_t alarm1;
    alarm1.mode = DS3231_ALARM_MODE_SECOND_MATCH;
    alarm1.second = 30;
    alarm1.minute = 0x80;  /* Match mode */
    alarm1.hour = 0x80;    /* Match mode */
    alarm1.day_or_weekday = 0x80;  /* Match mode */
    
    status = DS3231_SetAlarm1(&alarm1);
    if (status == DS3231_OK)
    {
        OLED_ShowString(2, 1, "Alarm1 Set OK");
        Delay_ms(500);
        
        /* Enable alarm 1 interrupt */
        status = DS3231_EnableAlarm1();
        if (status == DS3231_OK)
        {
            OLED_ShowString(3, 1, "Alarm1 Enabled");
            Delay_ms(1000);
        }
    }
    else
    {
        OLED_ShowString(2, 1, "Set Fail!");
        Delay_ms(2000);
    }
    
    /* Example 5: Check alarm flag */
    OLED_Clear();
    OLED_ShowString(1, 1, "5.Wait Alarm...");
    OLED_ShowString(2, 1, "Wait 30s...");
    uint32_t start_tick = Delay_GetTick();
    uint32_t elapsed = 0;
    while (1)
    {
        status = DS3231_CheckAlarm1Flag(&alarm_flag);
        if (status == DS3231_OK && alarm_flag)
        {
            OLED_ShowString(3, 1, "Alarm Trigger!");
            Delay_ms(1000);
            DS3231_ClearAlarm1Flag();
            OLED_ShowString(4, 1, "Flag Cleared");
            Delay_ms(1000);
            break;
        }
        
        /* Timeout check (35 seconds) */
        elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
        if (elapsed > 35000)
        {
            OLED_ShowString(3, 1, "Timeout!");
            Delay_ms(2000);
            break;
        }
        
        /* Show countdown */
        if (elapsed % 1000 < 100)
        {
            uint8_t remaining = 35 - (elapsed / 1000);
            OLED_ShowString(3, 1, "Remain:");
            OLED_ShowNum(3, 9, remaining, 2);
            OLED_ShowString(3, 11, "s");
        }
        
        Delay_ms(100);
    }
    
    /* Example 6: Disable alarm 1 */
    OLED_Clear();
    OLED_ShowString(1, 1, "6.Disable A1...");
    Delay_ms(300);
    status = DS3231_DisableAlarm1();
    if (status == DS3231_OK)
    {
        OLED_ShowString(2, 1, "Alarm1 Disabled");
        Delay_ms(1000);
    }
    
    /* Example 7: Disable square wave output */
    OLED_ShowString(1, 1, "7.Disable SQW");
    Delay_ms(300);
    status = DS3231_DisableSquareWave();
    if (status == DS3231_OK)
    {
        OLED_ShowString(2, 1, "SQW Disabled");
        Delay_ms(1000);
    }
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Demo Complete!");
    Delay_ms(1000);
}

/**
 * @brief 主函数
 */
int main(void)
{
    SoftI2C_Status_t soft_i2c_status;
    DS3231_Status_t ds3231_status;
    DS3231_Config_t ds3231_config;
    DS3231_Time_t time;
    OLED_Status_t oled_status;
    
    /* 系统初始化 */
    System_Init();
    
    /* LED初始化 */
    if (LED_Init() != LED_OK)
    {
        while (1) { /* 初始化失败，死循环 */ }
    }
    
    /* OLED初始化（默认显示器） */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        /* OLED初始化失败，LED快速闪烁 */
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
    }
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Example 12");
    OLED_ShowString(2, 1, "DS3231 Demo");
    OLED_ShowString(3, 1, "SoftI2C2 PB10/11");
    Delay_ms(1500);
    
    /* Initialize software I2C (SoftI2C2 for DS3231: PB10/11) */
    OLED_Clear();
    OLED_ShowString(1, 1, "Init SoftI2C2...");
    Delay_ms(300);
    soft_i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_2);  /* SoftI2C2: PB10(SCL), PB11(SDA) */
    if (soft_i2c_status != SOFT_I2C_OK)
    {
        OLED_ShowString(2, 1, "SoftI2C Init Fail!");
        /* 显示错误码 */
        char err_buf[17];
        snprintf(err_buf, sizeof(err_buf), "Error: %d", soft_i2c_status);
        OLED_ShowString(3, 1, err_buf);
        Delay_ms(3000);
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
    }
    OLED_ShowString(2, 1, "SoftI2C2 OK");
    Delay_ms(500);
    
    /* Initialize DS3231 (software I2C interface) */
    OLED_ShowString(1, 1, "Init DS3231...");
    Delay_ms(300);
    ds3231_config.interface_type = DS3231_INTERFACE_SOFTWARE;
    ds3231_config.config.software.soft_i2c_instance = SOFT_I2C_INSTANCE_2;  /* 使用SoftI2C2实例 */
    
    ds3231_status = DS3231_Init(&ds3231_config);
    if (ds3231_status != DS3231_OK)
    {
        OLED_ShowString(2, 1, "DS3231 Init Fail!");
        Delay_ms(2000);
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
    }
    OLED_ShowString(2, 1, "DS3231 OK");
    Delay_ms(1000);
    
    /* Execute DS3231 configuration routine */
    DS3231_ConfigRoutine();
    
    /* Execute DS3231 function demo */
    DS3231_FunctionDemo();
    
    /* Main loop: Read and display time every second */
    OLED_Clear();
    OLED_ShowString(1, 1, "Real-Time Clock");
    uint32_t last_display_tick = 0;
    
    while (1)
    {
        uint32_t current_tick = Delay_GetTick();
        
        /* Update display every second */
        if (Delay_GetElapsed(current_tick, last_display_tick) >= 1000)
        {
            last_display_tick = current_tick;
            
            /* Read and display time */
            ds3231_status = DS3231_ReadTime(&time);
            if (ds3231_status == DS3231_OK)
            {
                DisplayTime(&time);
                
                /* Read and display temperature every 5 seconds */
                static uint8_t temp_counter = 0;
                temp_counter++;
                if (temp_counter >= 5)
                {
                    temp_counter = 0;
                    int16_t temperature;
                    if (DS3231_ReadTemperature(&temperature) == DS3231_OK)
                    {
                        DisplayTemperature(temperature);
                    }
                }
                
                /* LED blink indicates system running */
                LED_Toggle(LED_1);
            }
            else
            {
                OLED_ShowString(3, 1, "Read Time Fail!");
            }
        }
        
        /* Delay hook */
        Delay_ms(10);
    }
}

