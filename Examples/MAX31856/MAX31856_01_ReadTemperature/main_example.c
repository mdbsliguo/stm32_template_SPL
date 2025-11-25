/**
 * @file main_example.c
 * @brief 案例 - MAX31856热电偶温度传感器读取示例
 * @details 演示MAX31856的初始化、配置和温度读取功能
 * 
 * 硬件连接：
 * - MAX31856模块连接到SPI2
 *   - CS：PB12
 *   - SCK：PB13（SPI2_SCK）
 *   - SDO：PB14（SPI2_MISO）
 *   - SDI：PB15（SPI2_MOSI）
 *   - VCC：3.3V
 *   - GND：GND
 * 
 * 功能演示：
 * 1. MAX31856初始化（硬件SPI接口）
 * 2. 配置热电偶类型（K型）
 * 3. 配置采样模式（16次平均）
 * 4. 配置转换模式（连续转换）
 * 5. 读取热电偶温度
 * 6. 读取冷端温度
 * 7. 故障检测和处理
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "error_code.h"
#include "delay.h"
#include "led.h"
#include "i2c_sw.h"
#include "oled_ssd1306.h"
#include "spi_hw.h"
#include "max31856.h"
#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief 在OLED上显示热电偶温度信息
 */
static void DisplayThermocoupleTemperature(float temperature)
{
    char buffer[17];
    uint8_t i;
    uint8_t temp_str_len;
    
    /* 温度显示（格式：TC: 123.45C°） */
    if (temperature < 0)
    {
        snprintf(buffer, sizeof(buffer), "TC:-%.2f", -temperature);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "TC: %.2f", temperature);
    }
    temp_str_len = strlen(buffer);
    OLED_ShowString(2, 1, buffer);
    
    /* 显示C */
    OLED_ShowChar(2, temp_str_len + 1, 'C');
    
    /* 显示度符号 */
    OLED_ShowChar(2, temp_str_len + 2, 0xB0);  /* 度符号 */
    
    /* 清除行尾残留字符 */
    for (i = temp_str_len + 3; i <= 16; i++)
    {
        OLED_ShowChar(2, i, ' ');
    }
}

/**
 * @brief 在OLED上显示冷端温度信息
 */
static void DisplayColdJunctionTemperature(float temperature)
{
    char buffer[17];
    uint8_t i;
    uint8_t temp_str_len;
    
    /* 温度显示（格式：CJ: 25.00C°） */
    if (temperature < 0)
    {
        snprintf(buffer, sizeof(buffer), "CJ:-%.2f", -temperature);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "CJ: %.2f", temperature);
    }
    temp_str_len = strlen(buffer);
    OLED_ShowString(3, 1, buffer);
    
    /* 显示C */
    OLED_ShowChar(3, temp_str_len + 1, 'C');
    
    /* 显示度符号 */
    OLED_ShowChar(3, temp_str_len + 2, 0xB0);  /* 度符号 */
    
    /* 清除行尾残留字符 */
    for (i = temp_str_len + 3; i <= 16; i++)
    {
        OLED_ShowChar(3, i, ' ');
    }
}

/**
 * @brief 在OLED上显示故障信息
 */
static void DisplayFaultStatus(uint8_t fault_flags)
{
    char buffer[17];
    uint8_t i;
    
    if (fault_flags == 0)
    {
        snprintf(buffer, sizeof(buffer), "Fault: OK");
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "Fault: 0x%02X", fault_flags);
    }
    OLED_ShowString(4, 1, buffer);
    /* 清除行尾残留字符 */
    for (i = strlen(buffer) + 1; i <= 16; i++)
    {
        OLED_ShowChar(4, i, ' ');
    }
}

/**
 * @brief MAX31856初始化流程（极简版本）
 * @return MAX31856_Status_t 初始化状态，MAX31856_OK表示成功，其他值表示失败
 */
static MAX31856_Status_t MAX31856_InitRoutine(void)
{
    MAX31856_Status_t status;
    MAX31856_Config_t config;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "MAX31856 Init");
    Delay_ms(500);
    
    /* 1. 配置MAX31856（硬件SPI接口） */
    config.interface_type = MAX31856_INTERFACE_HARDWARE;
    config.config.hardware.spi_instance = SPI_INSTANCE_2;
    config.config.hardware.cs_port = GPIOB;
    config.config.hardware.cs_pin = GPIO_Pin_12;
    
    status = MAX31856_Init(&config);
    if (status != MAX31856_OK)
    {
        OLED_ShowString(2, 1, "Init Fail!");
        OLED_ShowString(3, 1, "Error:");
        char err_buf[17];
        snprintf(err_buf, sizeof(err_buf), "%d", status);
        OLED_ShowString(4, 1, err_buf);
        Delay_ms(3000);
        return status;
    }
    OLED_ShowString(2, 1, "Init OK");
    Delay_ms(500);
    
    /* 2. 设置K型热电偶 */
    status = MAX31856_SetThermocoupleType(MAX31856_TC_TYPE_K);
    if (status != MAX31856_OK)
    {
        OLED_ShowString(3, 1, "Set Type Fail!");
        Delay_ms(2000);
        return status;
    }
    OLED_ShowString(3, 1, "Type: K");
    Delay_ms(300);
    
    /* 3. 设置4次平均（快速转换） */
    status = MAX31856_SetAvgMode(MAX31856_AVG_SEL_4);
    if (status != MAX31856_OK)
    {
        OLED_ShowString(3, 1, "Set Avg Fail!");
        Delay_ms(2000);
        return status;
    }
    OLED_ShowString(3, 1, "Avg: 4");
    Delay_ms(300);
    
    /* 4. 设置连续转换模式（关键：启用CMODE位） */
    status = MAX31856_SetConvMode(MAX31856_CONV_MODE_CONTINUOUS);
    if (status != MAX31856_OK)
    {
        OLED_ShowString(3, 1, "Set Mode Fail!");
        Delay_ms(2000);
        return status;
    }
    OLED_ShowString(4, 1, "Mode: Cont");
    Delay_ms(300);
    
    /* 5. 清除故障 */
    MAX31856_ClearFault();
    Delay_ms(200);
    
    /* 6. 等待转换完成（4次平均约250ms，等待3秒确保稳定） */
    OLED_ShowString(4, 1, "Wait 3s...");
    Delay_ms(3000);
    
    /* 7. 验证配置：读取CR0和CR1寄存器 */
    {
        uint8_t cr0_value = 0;
        uint8_t cr1_value = 0;
        
        status = MAX31856_ReadCR0(&cr0_value);
        if (status == MAX31856_OK)
        {
            /* CR0应该为0x80（CMODE=1，连续转换模式） */
            char buf[17];
            snprintf(buf, sizeof(buf), "CR0: 0x%02X", cr0_value);
            OLED_ShowString(1, 1, buf);
        }
        
        Delay_ms(200);
        
        status = MAX31856_ReadCR1(&cr1_value);
        if (status == MAX31856_OK)
        {
            /* CR1应该包含K型热电偶类型（0x03）和4次平均（0x02） */
            char buf[17];
            snprintf(buf, sizeof(buf), "CR1: 0x%02X", cr1_value);
            OLED_ShowString(2, 1, buf);
        }
        
        Delay_ms(1000);
    }
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Ready!");
    Delay_ms(500);
    
    return MAX31856_OK;
}

/**
 * @brief 主函数
 */
int main(void)
{
    UART_Status_t uart_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    LED_Status_t led_status;
    SPI_Status_t spi_status;
    MAX31856_Status_t max31856_status;
    OLED_Status_t oled_status;
    SoftI2C_Status_t i2c_status;
    float tc_temperature = 0.0f;
    float cj_temperature = 0.0f;
    uint8_t fault_flags = 0;
    uint32_t last_read_tick = 0;
    
    /* ========== 步骤1：系统初始化 ========== */
    System_Init();
    
    /* ========== 步骤2：UART初始化 ========== */
    uart_status = UART_Init(UART_INSTANCE_1);
    if (uart_status != UART_OK)
    {
        /* UART初始化失败，无法继续，进入死循环 */
        while(1) { Delay_ms(1000); }
    }
    
    /* 等待UART稳定 */
    Delay_ms(100);
    
    /* ========== 步骤3：Debug模块初始化（UART模式） ========== */
    debug_status = Debug_Init(DEBUG_MODE_UART, 115200);
    if (debug_status != 0)
    {
        /* Debug初始化失败，无法继续，进入死循环 */
        while(1) { Delay_ms(1000); }
    }
    
    /* 等待Debug模块稳定 */
    Delay_ms(100);
    
    /* ========== 步骤4：Log模块初始化 ========== */
    memset(&log_config, 0, sizeof(log_config));
    log_config.level = LOG_LEVEL_INFO;         /* 日志级别：INFO（简化输出） */
    log_config.enable_timestamp = 0;          /* 禁用时间戳 */
    log_config.enable_module = 1;              /* 启用模块名显示 */
    log_config.enable_color = 0;              /* 禁用颜色输出 */
    
    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK)
    {
        ErrorHandler_Handle(log_status, "LOG");
    }
    
    /* ========== 步骤5：错误处理模块 ========== */
    /* 注意：ErrorHandler模块在编译时自动初始化，无需显式调用 */
    
    /* ========== 步骤6：输出初始化信息 ========== */
    LOG_INFO("MAIN", "=== 系统初始化完成 ===");
    LOG_INFO("MAIN", "UART1已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "Debug模块已初始化: UART模式");
    LOG_INFO("MAIN", "Log模块已初始化");
    LOG_INFO("MAIN", "ErrorHandler模块已就绪");
    LOG_INFO("MAIN", "=== MAX31856热电偶温度传感器读取示例 ===");
    
    /* ========== 步骤7：初始化其他模块（按依赖顺序） ========== */
    
    /* LED初始化 */
    LOG_INFO("MAIN", "正在初始化LED...");
    led_status = LED_Init();
    if (led_status != LED_OK)
    {
        LOG_ERROR("MAIN", "LED初始化失败: %d", led_status);
        ErrorHandler_Handle(led_status, "LED");
        /* LED初始化失败，但可以继续运行 */
    }
    else
    {
        LOG_INFO("MAIN", "LED已初始化: PA1");
    }
    
    /* 软件I2C初始化（OLED需要） */
    LOG_INFO("MAIN", "正在初始化软件I2C...");
    i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_1);
    if (i2c_status != SOFT_I2C_OK)
    {
        LOG_ERROR("MAIN", "软件I2C初始化失败: %d", i2c_status);
        ErrorHandler_Handle(i2c_status, "SOFT_I2C");
        /* I2C初始化失败，OLED无法使用，但可以继续运行 */
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
    }
    else
    {
        LOG_INFO("MAIN", "软件I2C已初始化: PB8(SCL), PB9(SDA)");
    }
    
    /* OLED初始化（默认显示器） */
    LOG_INFO("MAIN", "正在初始化OLED...");
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        LOG_ERROR("MAIN", "OLED初始化失败: %d", oled_status);
        ErrorHandler_Handle(oled_status, "OLED");
        /* OLED初始化失败，但可以继续运行（串口仍可输出） */
    }
    else
    {
        OLED_Clear();
        OLED_ShowString(1, 1, "System Init OK");
        OLED_ShowString(2, 1, "UART Ready");
        OLED_ShowString(3, 1, "Log Ready");
        LOG_INFO("MAIN", "OLED已初始化并显示");
    }
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Example");
    OLED_ShowString(2, 1, "MAX31856 Demo");
    OLED_ShowString(3, 1, "SPI2 PB12-15");
    LOG_INFO("MAIN", "OLED显示初始化信息");
    Delay_ms(1500);
    
    /* 初始化SPI2 */
    LOG_INFO("MAIN", "正在初始化SPI2...");
    OLED_Clear();
    OLED_ShowString(1, 1, "Init SPI2...");
    Delay_ms(300);
    spi_status = SPI_HW_Init(SPI_INSTANCE_2);
    if (spi_status != SPI_OK)
    {
        LOG_ERROR("MAIN", "SPI2初始化失败: %d", spi_status);
        ErrorHandler_Handle(spi_status, "SPI");
        OLED_ShowString(2, 1, "SPI2 Init Fail!");
        char err_buf[17];
        snprintf(err_buf, sizeof(err_buf), "Error: %d", spi_status);
        OLED_ShowString(3, 1, err_buf);
        Delay_ms(3000);
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
    }
    else
    {
        OLED_ShowString(2, 1, "SPI2 OK");
        LOG_INFO("MAIN", "SPI2已初始化: PB13(SCK), PB14(MISO), PB15(MOSI), PB12(CS)");
    }
    Delay_ms(500);
    
    /* 执行MAX31856初始化流程 */
    LOG_INFO("MAIN", "开始MAX31856初始化流程...");
    max31856_status = MAX31856_InitRoutine();
    if (max31856_status != MAX31856_OK)
    {
        LOG_ERROR("MAIN", "MAX31856初始化失败: %d", max31856_status);
        ErrorHandler_Handle(max31856_status, "MAX31856");
        /* MAX31856初始化失败，显示错误信息并进入错误处理循环 */
        OLED_Clear();
        OLED_ShowString(1, 1, "MAX31856 Init");
        OLED_ShowString(2, 1, "FAILED!");
        char err_buf[17];
        snprintf(err_buf, sizeof(err_buf), "Error: %d", max31856_status);
        OLED_ShowString(3, 1, err_buf);
        OLED_ShowString(4, 1, "Check Hardware");
        Delay_ms(3000);
        
        /* 进入错误处理循环：LED闪烁，OLED显示错误信息 */
        while (1) 
        { 
            LED_Toggle(LED_1); 
            Delay_ms(200); 
        }
    }
    else
    {
        LOG_INFO("MAIN", "MAX31856初始化成功: K型热电偶，4次平均，连续转换模式");
    }
    
    /* ========== 步骤8：主循环 ========== */
    /* 主循环：读取并显示温度 */
    LOG_INFO("MAIN", "=== MAX31856温度读取演示开始 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "MAX31856 Reading");
    uint32_t current_tick = 0;
    
    while (1)
    {
        current_tick = Delay_GetTick();
        
        /* 每500ms读取一次温度 */
        if (Delay_GetElapsed(current_tick, last_read_tick) >= 500)
        {
            last_read_tick = current_tick;
            
            /* 读取故障状态 */
            max31856_status = MAX31856_ReadFault(&fault_flags);
            if (max31856_status == MAX31856_OK)
            {
                DisplayFaultStatus(fault_flags);
                
                /* 如果有故障，清除故障标志 */
                /* 注意：某些故障（如CJ_LOW）可能是芯片未完全初始化导致的，
                 * 清除后需要等待一段时间让芯片稳定
                 */
                if (fault_flags != 0 && fault_flags != 0xFF)
                {
                    LOG_WARN("MAX31856", "检测到故障: 0x%02X", fault_flags);
                    MAX31856_ClearFault();
                    /* 如果检测到CJ_LOW故障，说明冷端温度传感器未就绪，需要等待 */
                    if (fault_flags & 0x40)  /* CJ_LOW */
                    {
                        LOG_DEBUG("MAX31856", "CJ_LOW故障，等待冷端温度传感器稳定");
                        Delay_ms(100);  /* 等待冷端温度传感器稳定 */
                    }
                }
            }
            else if (max31856_status == MAX31856_ERROR_SPI_FAILED)
            {
                /* SPI通信错误，显示错误信息 */
                OLED_ShowString(4, 1, "Fault: SPI ERR");
            }
            else
            {
                /* 其他错误，显示错误码 */
                char err_buf[17];
                snprintf(err_buf, sizeof(err_buf), "Fault Err: %d", max31856_status);
                OLED_ShowString(4, 1, err_buf);
            }
            
            /* 读取热电偶温度 */
            max31856_status = MAX31856_ReadThermocoupleTemperature(&tc_temperature);
            if (max31856_status == MAX31856_OK)
            {
                DisplayThermocoupleTemperature(tc_temperature);
                LOG_DEBUG("MAX31856", "热电偶温度: %.2f°C", tc_temperature);
            }
            else
            {
                /* 根据错误码显示更友好的错误信息 */
                if (max31856_status == MAX31856_ERROR_FAULT)
                {
                    /* 检查具体故障类型 */
                    uint8_t fault_check;
                    if (MAX31856_CheckFault(MAX31856_FAULT_OPEN, &fault_check) == MAX31856_OK && fault_check)
                    {
                        OLED_ShowString(2, 1, "TC: OPEN FAULT");
                    }
                    else
                    {
                        OLED_ShowString(2, 1, "TC: FAULT");
                    }
                }
                else if (max31856_status == MAX31856_ERROR_SPI_FAILED)
                {
                    OLED_ShowString(2, 1, "TC: SPI ERR");
                }
                else
                {
                    char err_buf[17];
                    snprintf(err_buf, sizeof(err_buf), "TC Err: %d", max31856_status);
                    OLED_ShowString(2, 1, err_buf);
                }
            }
            
            /* 读取冷端温度 */
            max31856_status = MAX31856_ReadColdJunctionTemperature(&cj_temperature);
            if (max31856_status == MAX31856_OK)
            {
                DisplayColdJunctionTemperature(cj_temperature);
                LOG_DEBUG("MAX31856", "冷端温度: %.2f°C", cj_temperature);
            }
            else
            {
                /* 根据错误码显示更友好的错误信息 */
                if (max31856_status == MAX31856_ERROR_SPI_FAILED)
                {
                    OLED_ShowString(3, 1, "CJ: SPI ERR");
                }
                else if (max31856_status == MAX31856_ERROR_FAULT)
                {
                    OLED_ShowString(3, 1, "CJ: FAULT");
                }
                else
                {
                    char err_buf[17];
                    snprintf(err_buf, sizeof(err_buf), "CJ Err: %d", max31856_status);
                    OLED_ShowString(3, 1, err_buf);
                }
            }
            
            /* LED闪烁指示系统运行 */
            LED_Toggle(LED_1);
        }
        
        /* Delay hook */
        Delay_ms(10);
    }
}

