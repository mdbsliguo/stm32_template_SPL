/**
 * @file main_example.c
 * @brief I2C03 - I2C中断模式用法示例
 * @details 演示硬件I2C中断模式的使用方法
 * 
 * 硬件连接：
 * - DS3231模块连接到I2C2
 *   - SCL：PB10
 *   - SDA：PB11
 *   - INT/SQW：PA0（EXTI0，用于闹钟中断）⚠️ 必须连接
 *     - INT/SQW引脚需要上拉电阻（10kΩ）到3.3V
 *     - 连接方法：INT/SQW → PA0（信号线）
 *                  INT/SQW → [10kΩ] → 3.3V（上拉电阻）
 *   - VCC：3.3V
 *   - GND：GND
 * - OLED显示屏（SSD1306，软件I2C接口）
 *   - SCL：PB8
 *   - SDA：PB9
 * 
 * 功能演示：
 * 1. I2C中断模式配置
 * 2. DS3231闹钟中断触发（通过EXTI中断）
 * 3. 在中断回调中使用I2C中断模式读取时间
 * 4. 演示中断模式的非阻塞特性
 * 
 * @note 本案例必须通过EXTI中断触发，不能使用轮询方式检测闹钟标志
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "led.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include "i2c_hw.h"
#include "ds3231.h"
#include "exti.h"
#include "gpio.h"
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

/* 常量定义 */
#define DS3231_I2C_ADDR_7BIT     0x68    /* DS3231 I2C地址（7位） */
#define DS3231_I2C_ADDR_8BIT     (DS3231_I2C_ADDR_7BIT << 1)  /* DS3231 I2C地址（8位，用于I2C传输） */
#define DS3231_TIME_REG_ADDR     0x00    /* 时间寄存器起始地址 */
#define I2C_TIMEOUT_MS           500     /* I2C中断超时时间（毫秒） */

/* 全局变量 */
static volatile uint8_t g_alarm_triggered = 0;  /* 闹钟触发标志 */
static DS3231_Time_t g_latest_time;            /* 最新读取的时间 */
static volatile uint8_t g_i2c_read_complete = 0; /* I2C读取完成标志 */
static uint8_t g_time_regs[7];                 /* 时间寄存器缓冲区 */
static volatile uint8_t g_i2c_write_complete = 0; /* I2C写入完成标志 */

/**
 * @brief I2C中断回调函数
 * @param instance I2C实例
 * @param status I2C状态
 */
static void I2C_Callback(I2C_Instance_t instance, I2C_Status_t status)
{
    (void)instance;
    
    if (status == I2C_OK)
    {
        /* 根据当前操作设置完成标志 */
        if (g_i2c_write_complete == 0)
        {
            /* 写入完成，开始读取 */
            g_i2c_write_complete = 1;
        }
        else
        {
            /* 读取完成 */
            g_i2c_read_complete = 1;
        }
    }
    else
    {
        /* I2C错误处理 */
        LOG_ERROR("I2C", "I2C interrupt error: %d", status);
        g_i2c_write_complete = 0;
        g_i2c_read_complete = 0;
    }
}

/**
 * @brief EXTI中断回调函数（DS3231闹钟中断）
 * @param line EXTI线号
 * @param user_data 用户数据
 */
static void EXTI0_Callback(EXTI_Line_t line, void *user_data)
{
    (void)line;
    (void)user_data;
    
    /* 设置闹钟触发标志 */
    g_alarm_triggered = 1;
    
    /* 注意：在中断中不要执行耗时操作，只设置标志 */
}

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
        weekday_str = "???";
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
 * @brief 使用I2C中断模式读取DS3231时间
 * @return 0=成功，非0=失败
 */
static int ReadTimeWithInterrupt(void)
{
    I2C_Status_t i2c_status;
    uint8_t reg_addr = DS3231_TIME_REG_ADDR;
    uint32_t timeout;
    
    /* 重置完成标志 */
    g_i2c_write_complete = 0;
    g_i2c_read_complete = 0;
    
    /* 步骤1：使用I2C中断模式写入寄存器地址 */
    i2c_status = I2C_MasterTransmitIT(I2C_INSTANCE_2, DS3231_I2C_ADDR_8BIT, &reg_addr, 1);
    if (i2c_status != I2C_OK)
    {
        LOG_ERROR("I2C", "I2C_MasterTransmitIT failed: %d", i2c_status);
        return -1;
    }
    
    /* 等待写入完成（超时保护） */
    timeout = Delay_GetTick() + I2C_TIMEOUT_MS;
    while (!g_i2c_write_complete)
    {
        if (Delay_GetTick() >= timeout)
        {
            LOG_ERROR("I2C", "I2C write interrupt timeout");
            return -2;
        }
    }
    
    /* 步骤2：使用I2C中断模式读取7个字节的时间数据 */
    i2c_status = I2C_MasterReceiveIT(I2C_INSTANCE_2, DS3231_I2C_ADDR_8BIT, g_time_regs, 7);
    if (i2c_status != I2C_OK)
    {
        LOG_ERROR("I2C", "I2C_MasterReceiveIT failed: %d", i2c_status);
        return -4;
    }
    
    /* 等待读取完成（超时保护） */
    timeout = Delay_GetTick() + I2C_TIMEOUT_MS;
    while (!g_i2c_read_complete)
    {
        if (Delay_GetTick() >= timeout)
        {
            LOG_ERROR("I2C", "I2C read interrupt timeout");
            return -5;
        }
    }
    
    /* 解析时间数据（BCD格式转换为二进制） */
    g_latest_time.second = ((g_time_regs[0] & 0x0F) + ((g_time_regs[0] >> 4) & 0x07) * 10);
    g_latest_time.minute = ((g_time_regs[1] & 0x0F) + ((g_time_regs[1] >> 4) & 0x07) * 10);
    g_latest_time.hour = ((g_time_regs[2] & 0x0F) + ((g_time_regs[2] >> 4) & 0x0F) * 10);
    g_latest_time.weekday = (g_time_regs[3] & 0x07);
    g_latest_time.day = ((g_time_regs[4] & 0x0F) + ((g_time_regs[4] >> 4) & 0x03) * 10);
    g_latest_time.month = ((g_time_regs[5] & 0x0F) + ((g_time_regs[5] >> 4) & 0x01) * 10);
    g_latest_time.year = ((g_time_regs[6] & 0x0F) + ((g_time_regs[6] >> 4) & 0x0F) * 10) + 2000;
    
    return 0;  /* 成功 */
}

/**
 * @brief 主函数
 */
int main(void)
{
    I2C_Status_t i2c_status;
    DS3231_Status_t ds3231_status;
    DS3231_Config_t ds3231_config;
    DS3231_Alarm_t alarm1;
    EXTI_Status_t exti_status;
    OLED_Status_t oled_status;
    uint32_t last_display_tick = 0;
    
    /* 系统初始化 */
    System_Init();
    
    /* LED初始化 */
    if (LED_Init() != LED_OK)
    {
        ErrorHandler_Handle(ERROR_BASE_LED, "LED");
        while (1) { /* 初始化失败，死循环 */ }
    }
    
    /* LED测试：初始化后测试LED（闪烁3次，每次200ms，确认LED能正常工作） */
    for (int i = 0; i < 3; i++)
    {
        LED_On(LED_1);
        Delay_ms(200);
        LED_Off(LED_1);
        Delay_ms(200);
    }
    
    /* LED初始化后关闭（正常运行时LED不亮） */
    LED_Off(LED_1);
    
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
    OLED_ShowString(1, 1, "I2C03");
    OLED_ShowString(2, 1, "I2C Interrupt");
    OLED_ShowString(3, 1, "Mode Demo");
    Delay_ms(1500);
    
    /* Initialize I2C2 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Init I2C2...");
    Delay_ms(300);
    i2c_status = I2C_HW_Init(I2C_INSTANCE_2);
    if (i2c_status != I2C_OK)
    {
        OLED_ShowString(2, 1, "I2C2 Init Fail!");
        ErrorHandler_Handle(ERROR_BASE_I2C + i2c_status, "I2C2");
        Delay_ms(3000);
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
    }
    OLED_ShowString(2, 1, "I2C2 OK");
    Delay_ms(500);
    
    /* Initialize DS3231 (hardware I2C interface) - 使用轮询模式初始化 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Init DS3231...");
    Delay_ms(300);
    ds3231_config.interface_type = DS3231_INTERFACE_HARDWARE;
    ds3231_config.config.hardware.i2c_instance = I2C_INSTANCE_2;
    
    ds3231_status = DS3231_Init(&ds3231_config);
    if (ds3231_status != DS3231_OK)
    {
        OLED_ShowString(2, 1, "DS3231 Init Fail!");
        ErrorHandler_Handle(ERROR_BASE_DS3231 + ds3231_status, "DS3231");
        Delay_ms(2000);
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
    }
    OLED_ShowString(2, 1, "DS3231 OK");
    Delay_ms(500);
    
    /* 先禁用并清除旧的闹钟配置 */
    OLED_ShowString(1, 1, "Clear Old Alarm");
    Delay_ms(500);
    
    /* 禁用闹钟1 */
    DS3231_DisableAlarm1();
    Delay_ms(100);
    
    /* 清除闹钟标志 */
    DS3231_ClearAlarm1Flag();
    Delay_ms(100);
    
    /* 设置一个无效的闹钟配置（秒=99，不会匹配）来覆盖旧配置 */
    DS3231_Alarm_t clear_alarm;
    clear_alarm.mode = DS3231_ALARM_MODE_SECOND_MATCH;
    clear_alarm.second = 99;  /* 无效值，不会触发 */
    clear_alarm.minute = 0x80;
    clear_alarm.hour = 0x80;
    clear_alarm.day_or_weekday = 0x80;
    DS3231_SetAlarm1(&clear_alarm);
    Delay_ms(100);
    
    /* 再次禁用和清除 */
    DS3231_DisableAlarm1();
    DS3231_ClearAlarm1Flag();
    Delay_ms(200);
    
    /* 配置DS3231闹钟1（每30秒触发一次：30秒、0秒） - 使用轮询模式配置 */
    OLED_ShowString(1, 1, "Set Alarm1...");
    Delay_ms(300);
    alarm1.mode = DS3231_ALARM_MODE_SECOND_MATCH;  /* 秒匹配模式 */
    alarm1.second = 30;    /* 每30秒触发一次（30秒、0秒） */
    alarm1.minute = 0x80;  /* Match mode */
    alarm1.hour = 0x80;    /* Match mode */
    alarm1.day_or_weekday = 0x80;  /* Match mode */
    
    ds3231_status = DS3231_SetAlarm1(&alarm1);
    if (ds3231_status != DS3231_OK)
    {
        OLED_ShowString(2, 1, "Set Alarm Fail!");
        ErrorHandler_Handle(ERROR_BASE_DS3231 + ds3231_status, "DS3231");
        Delay_ms(2000);
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
    }
    OLED_ShowString(2, 1, "Alarm1 Set OK");
    Delay_ms(1000);
    
    /* 再次清除闹钟标志，确保干净状态 */
    DS3231_ClearAlarm1Flag();
    Delay_ms(100);
    
    /* 先配置中断模式为闹钟中断（必须在使能闹钟中断之前） */
    OLED_ShowString(1, 1, "Set INT Mode...");
    Delay_ms(300);
    ds3231_status = DS3231_SetInterruptMode(DS3231_INT_MODE_ALARM);
    if (ds3231_status != DS3231_OK)
    {
        OLED_ShowString(2, 1, "Set INT Mode Fail!");
        ErrorHandler_Handle(ERROR_BASE_DS3231 + ds3231_status, "DS3231");
        Delay_ms(2000);
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
    }
    OLED_ShowString(2, 1, "INT Mode OK");
    Delay_ms(1000);
    
    /* 使能闹钟1中断（必须在设置中断模式之后） */
    OLED_ShowString(1, 1, "Enable Alarm1...");
    Delay_ms(300);
    ds3231_status = DS3231_EnableAlarm1();
    if (ds3231_status != DS3231_OK)
    {
        OLED_ShowString(2, 1, "Enable Alarm Fail!");
        ErrorHandler_Handle(ERROR_BASE_DS3231 + ds3231_status, "DS3231");
        Delay_ms(2000);
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
    }
    OLED_ShowString(2, 1, "Alarm1 Enabled");
    Delay_ms(1000);
    
    /* 验证控制寄存器配置（用于调试） */
    OLED_Clear();
    OLED_ShowString(1, 1, "Check DS3231...");
    Delay_ms(500);
    uint8_t ctrl_reg = 0;
    DS3231_ReadControlRegister(&ctrl_reg);
    char ctrl_str[17];
    snprintf(ctrl_str, sizeof(ctrl_str), "CTRL: 0x%02X", ctrl_reg);
    OLED_ShowString(2, 1, ctrl_str);
    /* CTRL寄存器应该包含：INTCN(0x04)和A1IE(0x01)，即0x05 */
    if ((ctrl_reg & 0x05) == 0x05)
    {
        OLED_ShowString(3, 1, "Config OK");
    }
    else
    {
        OLED_ShowString(3, 1, "Config Error!");
    }
    Delay_ms(2000);
    
    /* 最后清除一次闹钟标志 */
    DS3231_ClearAlarm1Flag();
    g_alarm_triggered = 0;
    
    /* 初始化EXTI0（PA0，DS3231 INT/SQW引脚） */
    OLED_Clear();
    OLED_ShowString(1, 1, "Init EXTI0...");
    Delay_ms(300);
    exti_status = EXTI_HW_Init(EXTI_LINE_0, EXTI_TRIGGER_FALLING, EXTI_MODE_INTERRUPT);
    if (exti_status != EXTI_OK)
    {
        OLED_ShowString(2, 1, "EXTI Init Fail!");
        ErrorHandler_Handle(ERROR_BASE_EXTI + exti_status, "EXTI");
        Delay_ms(2000);
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
    }
    
    /* 设置EXTI中断回调函数 */
    exti_status = EXTI_SetCallback(EXTI_LINE_0, EXTI0_Callback, NULL);
    if (exti_status != EXTI_OK)
    {
        OLED_ShowString(2, 1, "Set Callback Fail!");
        ErrorHandler_Handle(ERROR_BASE_EXTI + exti_status, "EXTI");
        Delay_ms(2000);
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
    }
    
    /* 使能EXTI中断（使能NVIC中断） */
    exti_status = EXTI_Enable(EXTI_LINE_0);
    if (exti_status != EXTI_OK)
    {
        OLED_ShowString(2, 1, "Enable EXTI Fail!");
        ErrorHandler_Handle(ERROR_BASE_EXTI + exti_status, "EXTI");
        Delay_ms(2000);
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
    }
    OLED_ShowString(2, 1, "EXTI0 OK");
    Delay_ms(1000);
    
    /* 测试EXTI中断是否正常工作（生成软件中断） */
    OLED_Clear();
    OLED_ShowString(1, 1, "Test EXTI SW...");
    Delay_ms(500);
    g_alarm_triggered = 0;  /* 重置标志 */
    exti_status = EXTI_HW_GenerateSWInterrupt(EXTI_LINE_0);
    Delay_ms(100);  /* 等待中断触发 */
    if (g_alarm_triggered)
    {
        OLED_ShowString(2, 1, "EXTI SW Test OK");
        OLED_ShowString(3, 1, "Interrupt Works");
    }
    else
    {
        OLED_ShowString(2, 1, "EXTI SW Test Fail");
        OLED_ShowString(3, 1, "Interrupt Broken");
    }
    Delay_ms(2000);
    
    /* 检查PA0引脚状态（用于调试硬件连接） */
    OLED_Clear();
    OLED_ShowString(1, 1, "Check PA0 State");
    Delay_ms(500);
    uint8_t pa0_state = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);
    char pa0_str[17];
    snprintf(pa0_str, sizeof(pa0_str), "PA0: %d (1=OK)", pa0_state);
    OLED_ShowString(2, 1, pa0_str);
    if (pa0_state == 0)
    {
        OLED_ShowString(3, 1, "PA0 LOW! Check");
        OLED_ShowString(4, 1, "INT/SQW Connect");
    }
    else
    {
        OLED_ShowString(3, 1, "PA0 HIGH OK");
        OLED_ShowString(4, 1, "Wait Alarm...");
    }
    Delay_ms(2000);
    
    /* 现在切换到I2C中断模式（DS3231初始化已完成，后续读取使用中断模式） */
    OLED_Clear();
    OLED_ShowString(1, 1, "Set I2C IT Mode...");
    Delay_ms(300);
    i2c_status = I2C_SetTransferMode(I2C_INSTANCE_2, I2C_MODE_INTERRUPT);
    if (i2c_status != I2C_OK)
    {
        OLED_ShowString(2, 1, "Set IT Mode Fail!");
        ErrorHandler_Handle(ERROR_BASE_I2C + i2c_status, "I2C2");
        Delay_ms(3000);
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
    }
    
    /* 设置I2C中断回调函数 */
    i2c_status = I2C_SetCallback(I2C_INSTANCE_2, I2C_Callback);
    if (i2c_status != I2C_OK)
    {
        OLED_ShowString(2, 1, "Set Callback Fail!");
        ErrorHandler_Handle(ERROR_BASE_I2C + i2c_status, "I2C2");
        Delay_ms(3000);
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
    }
    OLED_ShowString(2, 1, "I2C IT Mode OK");
    Delay_ms(1000);
    
    /* 显示提示信息 */
    OLED_Clear();
    OLED_ShowString(1, 1, "I2C Interrupt");
    OLED_ShowString(2, 1, "Mode Ready");
    OLED_ShowString(3, 1, "Wait Alarm...");
    Delay_ms(1000);
    
    /* 临时切换回轮询模式，用于读取时间 */
    i2c_status = I2C_SetTransferMode(I2C_INSTANCE_2, I2C_MODE_POLLING);
    if (i2c_status != I2C_OK)
    {
        OLED_ShowString(3, 1, "Switch Fail!  ");
        Delay_ms(2000);
    }
    
    /* 立即读取一次时间，确保显示正常 */
    DS3231_Time_t time;
    ds3231_status = DS3231_ReadTime(&time);
    
    if (ds3231_status == DS3231_OK)
    {
        DisplayTime(&time);
        char sec_str[17];
        snprintf(sec_str, sizeof(sec_str), "Sec: %02d (30s)", time.second);
        OLED_ShowString(4, 1, sec_str);
        
        /* 清除可能存在的闹钟标志（避免初始化时触发闹钟中断） */
        DS3231_ClearAlarm1Flag();
    }
    else
    {
        /* 读取失败，显示错误 */
        OLED_ShowString(3, 1, "Read Fail!    ");
        char err_str[17];
        snprintf(err_str, sizeof(err_str), "Err: %d       ", ds3231_status);
        OLED_ShowString(4, 1, err_str);
        Delay_ms(2000);
    }
    
    /* 清除闹钟触发标志（确保初始化时不会误触发） */
    g_alarm_triggered = 0;
    
    /* 切换回中断模式（用于闹钟触发时的中断读取） */
    I2C_SetTransferMode(I2C_INSTANCE_2, I2C_MODE_INTERRUPT);
    
    /* 初始化显示时间 - 设置为1秒前，强制主循环立即执行一次更新 */
    last_display_tick = Delay_GetTick() - 1000;
    
    /* 主循环 - 简化版本，只测试EXTI中断 */
    OLED_Clear();
    OLED_ShowString(1, 1, "EXTI Test Mode");
    OLED_ShowString(2, 1, "Wait Alarm...");
    OLED_ShowString(3, 1, "IT:0 C:0");
    OLED_ShowString(4, 1, "SR:00");
    
    while (1)
    {
        uint32_t current_tick = Delay_GetTick();
        uint32_t elapsed = Delay_GetElapsed(current_tick, last_display_tick);
        
        /* 检查闹钟是否触发 */
        if (g_alarm_triggered)
        {
            g_alarm_triggered = 0;  /* 清除标志 */
            
            /* LED指示闹钟触发 */
            LED_On(LED_1);
            Delay_ms(500);
            LED_Off(LED_1);
            
            /* 临时切换回轮询模式，用于清除闹钟标志 */
            I2C_SetTransferMode(I2C_INSTANCE_2, I2C_MODE_POLLING);
            
            /* 清除DS3231闹钟标志 */
            DS3231_ClearAlarm1Flag();
            
            /* 切换回中断模式 */
            I2C_SetTransferMode(I2C_INSTANCE_2, I2C_MODE_INTERRUPT);
            
            /* 等待I2C模式切换完成 */
            Delay_ms(10);
            
            /* 使用I2C中断模式读取时间 */
            if (ReadTimeWithInterrupt() == 0)
            {
                /* 读取成功，更新显示 */
                DisplayTime(&g_latest_time);
                OLED_ShowString(3, 1, "Alarm Trigger!");
                OLED_ShowString(4, 1, "I2C IT Read OK");
            }
            else
            {
                /* 读取失败，显示错误 */
                OLED_ShowString(3, 1, "Alarm Trigger!");
                OLED_ShowString(4, 1, "I2C IT Read Fail");
            }
            
            /* 显示5秒后恢复 */
            Delay_ms(5000);
            
            /* 恢复显示时间 */
            last_display_tick = Delay_GetTick();
        }
        
        /* 每秒更新一次显示（使用轮询模式读取时间作为对比） */
        if (elapsed >= 1000)
        {
            last_display_tick = current_tick;
            
            /* 临时切换回轮询模式读取时间 */
            I2C_SetTransferMode(I2C_INSTANCE_2, I2C_MODE_POLLING);
            
            /* 使用轮询模式读取时间（用于对比） */
            DS3231_Time_t time;
            ds3231_status = DS3231_ReadTime(&time);
            
            /* 切换回中断模式 */
            I2C_SetTransferMode(I2C_INSTANCE_2, I2C_MODE_INTERRUPT);
            
            if (ds3231_status == DS3231_OK)
            {
                DisplayTime(&time);
                OLED_ShowString(3, 1, "Wait Alarm...  ");
                char sec_str[17];
                snprintf(sec_str, sizeof(sec_str), "Sec: %02d (30s)", time.second);
                OLED_ShowString(4, 1, sec_str);
            }
        }
        
        /* 延时钩子 - 确保主循环正常运行 */
        Delay_ms(10);
    }
}

/* EXTI0中断服务程序 */
void EXTI0_IRQHandler(void)
{
    EXTI_IRQHandler(EXTI_LINE_0);
}

