                                                                                                            /**
 * @file main_simple.c
 * @brief 最简单的MAX31856读取案例 - 极简版本
 * @details 去掉所有复杂逻辑，只保留核心功能，快速验证SPI通信
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "led.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include "spi_hw.h"
#include "max31856.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief 主函数 - 极简版本
 */
int main(void)
{
    MAX31856_Config_t config;
    MAX31856_Status_t status;
    float tc_temp = 0.0f;
    float cj_temp = 0.0f;
    uint8_t fault = 0;
    
    /* 系统初始化 */
    System_Init();
    
    /* LED初始化 */
    LED_Init();
    
    /* OLED初始化 */
    OLED_Init();
    OLED_Clear();
    OLED_ShowString(1, 1, "MAX31856 Simple");
    OLED_ShowString(2, 1, "Initializing...");
    Delay_ms(1000);
    
    /* 1. 初始化SPI2 */
    OLED_ShowString(3, 1, "SPI2 Init...");
    if (SPI_HW_Init(SPI_INSTANCE_2) != SPI_OK)
    {
        OLED_ShowString(4, 1, "SPI2 FAIL!");
        while(1) { LED_Toggle(LED_1); Delay_ms(200); }
    }
    OLED_ShowString(3, 1, "SPI2 OK      ");
    Delay_ms(500);
    
    /* 2. 配置MAX31856 */
    OLED_ShowString(4, 1, "MAX31856 Init");
    config.interface_type = MAX31856_INTERFACE_HARDWARE;
    config.config.hardware.spi_instance = SPI_INSTANCE_2;
    config.config.hardware.cs_port = GPIOB;
    config.config.hardware.cs_pin = GPIO_Pin_12;
    
    status = MAX31856_Init(&config);
    if (status != MAX31856_OK)
    {
        OLED_Clear();
        OLED_ShowString(1, 1, "MAX31856 FAIL!");
        char buf[17];
        snprintf(buf, sizeof(buf), "Error: %d", status);
        OLED_ShowString(2, 1, buf);
        OLED_ShowString(3, 1, "Wait 2s retry");
        Delay_ms(2000);
        
        /* 重试一次初始化 */
        status = MAX31856_Init(&config);
        if (status != MAX31856_OK)
        {
            OLED_ShowString(4, 1, "Retry FAIL!");
            while(1) { LED_Toggle(LED_1); Delay_ms(200); }
        }
    }
    OLED_ShowString(4, 1, "MAX31856 OK  ");
    Delay_ms(1000);  /* 增加等待时间，确保芯片完全稳定 */
    
    /* 2.5. 测试SPI通信 - 多次读取CR0寄存器验证 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Test SPI...");
    {
        uint8_t cr0_value = 0;
        uint8_t test_count = 0;
        uint8_t success_count = 0;
        uint8_t last_cr0 = 0;
        
        for (test_count = 0; test_count < 10; test_count++)
        {
            status = MAX31856_ReadCR0(&cr0_value);
            if (status == MAX31856_OK)
            {
                if (cr0_value != 0xFF)
                {
                    success_count++;
                    last_cr0 = cr0_value;
                }
            }
            Delay_ms(100);
        }
        
        char buf[17];
        if (success_count > 0)
        {
            snprintf(buf, sizeof(buf), "SPI OK: %d/10", success_count);
            OLED_ShowString(2, 1, buf);
            snprintf(buf, sizeof(buf), "CR0: 0x%02X", last_cr0);
            OLED_ShowString(3, 1, buf);
            Delay_ms(2000);
        }
        else
        {
            OLED_ShowString(2, 1, "SPI FAIL!");
            snprintf(buf, sizeof(buf), "CR0: 0x%02X", cr0_value);
            OLED_ShowString(3, 1, buf);
            OLED_ShowString(4, 1, "Check Hardware");
            OLED_ShowString(1, 1, "MISO/PB14?");
            OLED_ShowString(2, 1, "CS/PB12?");
            OLED_ShowString(3, 1, "GND?");
            OLED_ShowString(4, 1, "3.3V?");
            while(1) { LED_Toggle(LED_1); Delay_ms(200); }
        }
    }
    
    /* 3. 配置MAX31856 - 最简单配置 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Configuring...");
    
    /* 先清除故障 */
    MAX31856_ClearFault();
    Delay_ms(200);
    
    /* 设置K型热电偶 */
    status = MAX31856_SetThermocoupleType(MAX31856_TC_TYPE_K);
    if (status != MAX31856_OK)
    {
        OLED_ShowString(2, 1, "Set Type FAIL!");
        char buf[17];
        snprintf(buf, sizeof(buf), "Err: %d", status);
        OLED_ShowString(3, 1, buf);
        while(1) { LED_Toggle(LED_1); Delay_ms(200); }
    }
    OLED_ShowString(2, 1, "Type: K OK");
    Delay_ms(300);
    
    /* 验证配置：读取CR1寄存器 */
    {
        uint8_t cr1_value = 0;
        status = MAX31856_ReadCR1(&cr1_value);
        if (status != MAX31856_OK)
        {
            OLED_ShowString(3, 1, "Read CR1 FAIL!");
            char buf[17];
            snprintf(buf, sizeof(buf), "Err: %d", status);
            OLED_ShowString(4, 1, buf);
            while(1) { LED_Toggle(LED_1); Delay_ms(200); }
        }
        char buf[17];
        snprintf(buf, sizeof(buf), "CR1: 0x%02X", cr1_value);
        OLED_ShowString(3, 1, buf);
        Delay_ms(500);
    }
    
    /* 设置4次平均（快速） */
    status = MAX31856_SetAvgMode(MAX31856_AVG_SEL_4);
    if (status != MAX31856_OK)
    {
        OLED_ShowString(4, 1, "Set Avg FAIL!");
        while(1) { LED_Toggle(LED_1); Delay_ms(200); }
    }
    OLED_ShowString(4, 1, "Avg: 4 OK");
    Delay_ms(300);
    
    /* 设置连续转换模式 */
    status = MAX31856_SetConvMode(MAX31856_CONV_MODE_CONTINUOUS);
    if (status != MAX31856_OK)
    {
        OLED_Clear();
        OLED_ShowString(1, 1, "Set Mode FAIL!");
        char buf[17];
        snprintf(buf, sizeof(buf), "Err: %d", status);
        OLED_ShowString(2, 1, buf);
        while(1) { LED_Toggle(LED_1); Delay_ms(200); }
    }
    OLED_Clear();
    OLED_ShowString(1, 1, "Mode: Cont OK");
    Delay_ms(300);
    
    /* 再次验证SPI通信 */
    OLED_ShowString(2, 1, "Verify SPI...");
    {
        uint8_t cr0_value = 0;
        uint8_t verify_count = 0;
        uint8_t success_count = 0;
        
        for (verify_count = 0; verify_count < 5; verify_count++)
        {
            status = MAX31856_ReadCR0(&cr0_value);
            if (status == MAX31856_OK && cr0_value != 0xFF)
            {
                success_count++;
            }
            Delay_ms(100);
        }
        
        char buf[17];
        if (success_count > 0)
        {
            snprintf(buf, sizeof(buf), "SPI: %d/5 OK", success_count);
            OLED_ShowString(3, 1, buf);
        }
        else
        {
            OLED_ShowString(3, 1, "SPI Verify FAIL!");
            OLED_ShowString(4, 1, "Check Hardware");
            while(1) { LED_Toggle(LED_1); Delay_ms(200); }
        }
        Delay_ms(1000);
    }
    
    /* 再次清除故障 */
    MAX31856_ClearFault();
    Delay_ms(500);
    
    /* 等待转换完成（4次平均约250ms，等待3秒确保稳定） */
    OLED_Clear();
    OLED_ShowString(1, 1, "Wait 3s...");
    Delay_ms(3000);
    
    /* 最后一次SPI通信验证 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Final Check...");
    {
        uint8_t cr0_final = 0;
        uint8_t cr1_final = 0;
        
        status = MAX31856_ReadCR0(&cr0_final);
        if (status == MAX31856_OK)
        {
            char buf[17];
            snprintf(buf, sizeof(buf), "CR0: 0x%02X", cr0_final);
            OLED_ShowString(2, 1, buf);
        }
        else
        {
            char buf[17];
            snprintf(buf, sizeof(buf), "CR0 Err: %d", status);
            OLED_ShowString(2, 1, buf);
        }
        
        Delay_ms(200);
        
        status = MAX31856_ReadCR1(&cr1_final);
        if (status == MAX31856_OK)
        {
            char buf[17];
            snprintf(buf, sizeof(buf), "CR1: 0x%02X", cr1_final);
            OLED_ShowString(3, 1, buf);
        }
        else
        {
            char buf[17];
            snprintf(buf, sizeof(buf), "CR1 Err: %d", status);
            OLED_ShowString(3, 1, buf);
        }
        
        Delay_ms(2000);
    }
    
    /* 主循环 - 简单读取 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Reading...");
    Delay_ms(500);
    
    while(1)
    {
        OLED_Clear();
        
        /* 先测试SPI通信 - 读取CR0 */
        uint8_t cr0_test = 0;
        status = MAX31856_ReadCR0(&cr0_test);
        if (status != MAX31856_OK)
        {
            char buf[17];
            snprintf(buf, sizeof(buf), "CR0 Err: %d", status);
            OLED_ShowString(1, 1, buf);
            OLED_ShowString(2, 1, "SPI Comm Fail!");
            LED_Toggle(LED_1);
            Delay_ms(1000);
            continue;  /* 跳过本次循环，重试 */
        }
        
        /* SPI通信正常，显示CR0值 */
        char buf[17];
        snprintf(buf, sizeof(buf), "CR0: 0x%02X", cr0_test);
        OLED_ShowString(1, 1, buf);
        
        /* 读取故障 - 添加调试信息 */
        status = MAX31856_ReadFault(&fault);
        if (status == MAX31856_OK)
        {
            if (fault == 0)
            {
                OLED_ShowString(2, 1, "Fault: OK      ");
            }
            else if (fault == 0xFF)
            {
                OLED_ShowString(2, 1, "Fault: 0xFF    ");
            }
            else
            {
                snprintf(buf, sizeof(buf), "Fault: 0x%02X    ", fault);
                OLED_ShowString(2, 1, buf);
            }
        }
        else
        {
            /* 故障读取失败，显示错误码用于调试 */
            snprintf(buf, sizeof(buf), "Fault Err: %d", status);
            OLED_ShowString(2, 1, buf);
        }
        
        /* 读取热电偶温度 - 使用高级函数 */
        status = MAX31856_ReadThermocoupleTemperature(&tc_temp);
        if (status == MAX31856_OK)
        {
            snprintf(buf, sizeof(buf), "TC: %.2fC", tc_temp);
            OLED_ShowString(3, 1, buf);
        }
        else
        {
            snprintf(buf, sizeof(buf), "TC: Err %d", status);
            OLED_ShowString(3, 1, buf);
        }
        
        /* 读取冷端温度 */
        status = MAX31856_ReadColdJunctionTemperature(&cj_temp);
        if (status == MAX31856_OK)
        {
            snprintf(buf, sizeof(buf), "CJ: %.2fC", cj_temp);
            OLED_ShowString(4, 1, buf);
        }
        else if (status == MAX31856_ERROR_FAULT)
        {
            /* 数据异常，可能是读取了错误的寄存器 */
            snprintf(buf, sizeof(buf), "CJ: Data Err");
            OLED_ShowString(4, 1, buf);
        }
        else
        {
            snprintf(buf, sizeof(buf), "CJ: Err %d", status);
            OLED_ShowString(4, 1, buf);
        }
        
        /* LED闪烁 */
        LED_Toggle(LED_1);
        
        /* 延时1秒（给转换足够时间） */
        Delay_ms(1000);
    }
}

