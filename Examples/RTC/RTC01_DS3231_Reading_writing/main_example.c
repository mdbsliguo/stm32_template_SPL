/**
 * @file main_example.c
 * @brief RTC01案例 - DS3231读写时间示例
 * @version 1.0.0
 * @date 2024-01-01
 * @details 本案例演示DS3231实时时钟模块的基本读写操作
 *          - 先读取DS3231当前时间并显示
 *          - 写入新时间到DS3231
 *          - 循环读取并实时显示时间
 * @note 使用软件I2C接口（SoftI2C2，PB10/11）连接DS3231
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "led.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include "i2c_sw.h"
#include "ds3231.h"
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
    
    /* 第2行：显示时间（格式：12:00:00） */
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
 * @brief 主函数
 * @return int 程序返回值（本程序不会返回）
 * @note 程序流程：初始化 → 读取当前时间 → 写入新时间 → 循环显示时间
 */
int main(void)
{
    SoftI2C_Status_t soft_i2c_status;
    DS3231_Status_t ds3231_status;
    DS3231_Config_t ds3231_config;
    DS3231_Time_t time_to_set;
    DS3231_Time_t time_read;
    OLED_Status_t oled_status;
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
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
    }
    
    OLED_Clear();
    OLED_ShowString(1, 1, "DS3231 Set Time");
    OLED_ShowString(2, 1, "Initializing...");
    Delay_ms(1000);
    
    /* 步骤2：初始化软件I2C2（用于DS3231） */
    OLED_Clear();
    OLED_ShowString(1, 1, "Init SoftI2C2...");
    Delay_ms(300);
    soft_i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_2);
    if (soft_i2c_status != SOFT_I2C_OK)
    {
        OLED_ShowString(2, 1, "SoftI2C Init Fail!");
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
    
    /* 步骤3：初始化DS3231模块 */
    OLED_ShowString(1, 1, "Init DS3231...");
    Delay_ms(300);
    ds3231_config.interface_type = DS3231_INTERFACE_SOFTWARE;
    ds3231_config.config.software.soft_i2c_instance = SOFT_I2C_INSTANCE_2;
    
    ds3231_status = DS3231_Init(&ds3231_config);
    if (ds3231_status != DS3231_OK)
    {
        OLED_ShowString(2, 1, "DS3231 Init Fail!");
        char err_buf[17];
        snprintf(err_buf, sizeof(err_buf), "Error: %d", ds3231_status);
        OLED_ShowString(3, 1, err_buf);
        Delay_ms(3000);
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
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
    
    /* 步骤5：先读取一次当前时间并显示 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Reading Time...");
    Delay_ms(500);
    
    ds3231_status = DS3231_ReadTime(&time_read);
    if (ds3231_status != DS3231_OK)
    {
        OLED_ShowString(2, 1, "Read Time Fail!");
        char err_buf[17];
        snprintf(err_buf, sizeof(err_buf), "Error: %d", ds3231_status);
        OLED_ShowString(3, 1, err_buf);
        Delay_ms(3000);
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
    }
    
    /* 显示读取到的当前时间 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Current Time:");
    DisplayTime(&time_read);
    Delay_ms(2000);
    
    /* 步骤6：写入新时间到DS3231 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Setting Time...");
    Delay_ms(500);
    
    /* 设置要写入的时间：2024-01-01 12:00:00 Monday */
    time_to_set.second = 0;
    time_to_set.minute = 0;
    time_to_set.hour = 12;
    time_to_set.weekday = 2;  /* Monday (1=Sunday, 2=Monday, ...) */
    time_to_set.day = 1;
    time_to_set.month = 1;
    time_to_set.year = 2024;
    
    /* 写入时间到DS3231 */
    ds3231_status = DS3231_SetTime(&time_to_set);
    if (ds3231_status != DS3231_OK)
    {
        OLED_ShowString(2, 1, "Set Time Fail!");
        char err_buf[17];
        snprintf(err_buf, sizeof(err_buf), "Error: %d", ds3231_status);
        OLED_ShowString(3, 1, err_buf);
        Delay_ms(3000);
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
    }
    
    OLED_ShowString(2, 1, "Time Set OK!");
    Delay_ms(1000);
    
    /* 步骤7：准备进入循环显示模式 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Time Updated!");
    Delay_ms(1000);
    OLED_Clear();
    
    /* 步骤8：主循环 - 每秒读取并显示时间 */
    while (1)
    {
        uint32_t current_tick = Delay_GetTick();
        
        /* 每秒更新一次显示 */
        if (Delay_GetElapsed(current_tick, last_display_tick) >= 1000)
        {
            last_display_tick = current_tick;
            
            /* 读取DS3231当前时间 */
            ds3231_status = DS3231_ReadTime(&time_read);
            if (ds3231_status == DS3231_OK)
            {
                DisplayTime(&time_read);
                LED_Toggle(LED_1);  /* LED闪烁指示系统运行 */
            }
            else
            {
                OLED_ShowString(3, 1, "Read Time Fail!");
            }
        }
        
        Delay_ms(10);
    }
}
