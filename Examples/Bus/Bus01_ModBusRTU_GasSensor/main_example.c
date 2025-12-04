/**
 * @file main_example.c
 * @brief Bus01 - ModBusRTU气体传感器读取示例
 * @details 通过UART2（RS485）使用ModBusRTU协议读取3个气体传感器数据并在OLED上显示
 * @version 1.0.0
 * @date 2024-01-01
 *
 * 功能说明：
 * - 使用ModBusRTU协议读取3个传感器数据（地址1：温湿度，地址2：氧气，地址3：一氧化碳）
 * - 解析传感器数据（气体浓度、温度、湿度、状态等）
 * - 在OLED上显示传感器数据
 * - 通过串口输出日志信息
 *
 * 模块依赖：
 * - system_init：系统初始化模块
 * - uart：UART驱动模块
 * - debug：Debug模块（UART输出功能）
 * - log：日志模块
 * - error_handler：错误处理模块
 * - modbus_rtu：ModBusRTU协议栈模块
 * - oled_ssd1306：OLED显示模块
 * - delay：延时模块
 *
 * 硬件要求：
 * - UART1：PA9(TX), PA10(RX)，115200波特率（用于Debug输出）
 * - UART2：PA2(TX), PA3(RX)，9600波特率（用于ModBusRTU通信，连接RS485）
 * - OLED：PB8(SCL), PB9(SDA)，软件I2C
 * - RS485模块：连接UART2，自动方向控制
 * - 3个气体传感器：地址1（温湿度）、地址2（氧气）、地址3（一氧化碳）
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "error_code.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include "modbus_rtu.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ==================== 传感器数据结构 ==================== */

/**
 * @brief 传感器数据结构
 */
typedef struct {
    uint16_t unit_decimal;      /**< 寄存器0：单位和小数点 */
    uint16_t concentration;     /**< 寄存器1：当前气体浓度 */
    uint16_t low_alarm;         /**< 寄存器2：低报警值 */
    uint16_t high_alarm;        /**< 寄存器3：高报警值 */
    uint16_t full_range;        /**< 寄存器4：全量程值 */
    uint8_t status;             /**< 寄存器5：传感器状态 */
    uint16_t adc_value;         /**< 寄存器6：实时AD值 */
    uint16_t temperature;       /**< 寄存器7：环境温度（原始值） */
    uint8_t gas_type;           /**< 寄存器8高8位：气体类型 */
    uint16_t humidity;          /**< 寄存器9：环境湿度（原始值） */
} SensorData_t;

/* ==================== 传感器配置 ==================== */

#define SENSOR_COUNT 3  /**< 传感器数量 */
// #define SENSORS_PER_PAGE 3  /**< 每页显示的传感器数量 */
// #define PAGE_COUNT ((SENSOR_COUNT + SENSORS_PER_PAGE - 1) / SENSORS_PER_PAGE)  /**< 总页数 */

/**
 * @brief 传感器配置信息
 */
typedef struct {
    uint8_t address;            /**< 传感器地址 */
    const char *name;           /**< 传感器名称 */
    const char *unit;           /**< 单位（用于显示） */
} SensorConfig_t;

static const SensorConfig_t g_sensor_configs[SENSOR_COUNT] = {
    {1, "TempHum", "C/%"},      /**< 地址1：温湿度传感器 */
    {2, "O2", "ppm"},           /**< 地址2：氧气传感器 */
    {3, "CO", "ppm"},           /**< 地址3：一氧化碳传感器 */
    // {4, "CO2", "ppm"},          /**< 地址4：二氧化碳传感器 */
    // {5, "CH4", "ppm"},          /**< 地址5：甲烷传感器 */
    // {6, "H2", "ppm"},           /**< 地址6：氢气传感器 */
};

/* ==================== 全局变量 ==================== */

static SensorData_t g_sensor_data[SENSOR_COUNT] = {0};  /**< 传感器数据缓冲区 */
static uint8_t g_sensor_read_success[SENSOR_COUNT] = {0};  /**< 传感器读取成功标志 */
// static uint8_t g_current_page = 0;  /**< 当前显示页（0-1） */

/* ==================== 私有函数 ==================== */

/**
 * @brief 解析传感器数据
 * @param[in] registers 寄存器数据（10个寄存器）
 * @param[out] sensor_data 解析后的传感器数据
 */
static void ParseSensorData(const uint16_t *registers, SensorData_t *sensor_data)
{
    sensor_data->unit_decimal = registers[0];
    sensor_data->concentration = registers[1];
    sensor_data->low_alarm = registers[2];
    sensor_data->high_alarm = registers[3];
    sensor_data->full_range = registers[4];
    sensor_data->status = (uint8_t)(registers[5] & 0xFF);
    sensor_data->adc_value = registers[6];
    sensor_data->temperature = registers[7];
    sensor_data->gas_type = (uint8_t)((registers[8] >> 8) & 0xFF);
    sensor_data->humidity = registers[9];
}

/**
 * @brief 计算温度值（℃）
 * @param[in] raw_value 原始温度值
 * @return float 温度值（℃）
 */
static float CalculateTemperature(uint16_t raw_value)
{
    return ((float)raw_value - 500.0f) / 10.0f;
}

/**
 * @brief 计算湿度值（%）
 * @param[in] raw_value 原始湿度值
 * @return float 湿度值（%）
 */
static float CalculateHumidity(uint16_t raw_value)
{
    return (float)raw_value / 10.0f;
}

/**
 * @brief 读取单个传感器数据
 * @param[in] sensor_index 传感器索引（0-2）
 * @return ModBusRTU_Status_t 错误码
 */
static ModBusRTU_Status_t ReadSensorData(uint8_t sensor_index)
{
    uint16_t registers[10];
    ModBusRTU_Status_t status;
    const SensorConfig_t *config = &g_sensor_configs[sensor_index];
    
    /* 读取10个寄存器（0x0000-0x0009） */
    status = ModBusRTU_ReadHoldingRegisters(UART_INSTANCE_2, config->address,
                                           0x0000, 10, registers, 1000);
    
    if (status == ModBusRTU_OK) {
        /* 解析数据 */
        ParseSensorData(registers, &g_sensor_data[sensor_index]);
        g_sensor_read_success[sensor_index] = 1;
        
        /* 读取成功不输出日志 */
    } else {
        g_sensor_read_success[sensor_index] = 0;
        /* 读取失败才输出日志 */
        LOG_ERROR("SENSOR", "传感器%d (%s) 读取失败: %d", config->address, config->name, status);
        ErrorHandler_Handle(status, "ModBusRTU");
    }
    
    return status;
}

/**
 * @brief 获取单位字符串
 * @param[in] unit_type 单位类型（Bit15-12）
 * @return const char* 单位字符串
 */
static const char* GetUnitString(uint8_t unit_type)
{
    switch (unit_type) {
        case 0: return "ppm";
        case 2: return "%LEL";
        case 4: return "%VOL";
        case 6: return "mg/m3";
        case 8: return "ppb";
        case 10: return "C";
        default: return "?";
    }
}

/**
 * @brief 计算气体浓度值（根据小数位数）
 * @param[in] raw_value 原始浓度值
 * @param[in] decimal_places 小数位数（Bit11-8）
 * @return float 计算后的浓度值
 */
static float CalculateConcentration(uint16_t raw_value, uint8_t decimal_places)
{
    float divisor;
    
    switch (decimal_places) {
        case 0: divisor = 1.0f; break;      /* 无小数位 */
        case 4: divisor = 10.0f; break;      /* 一位小数 */
        case 8: divisor = 100.0f; break;     /* 两位小数 */
        case 12: divisor = 1000.0f; break;  /* 三位小数 */
        default: divisor = 1.0f; break;
    }
    
    return (float)raw_value / divisor;
}

/**
 * @brief 在OLED上显示传感器数据
 */
static void DisplaySensorData(void)
{
    uint8_t i;
    char buffer[32];
    float temp, hum;
    float concentration;
    uint8_t unit_type;
    uint8_t decimal_places;
    const char *unit_str;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Bus01 ModBusRTU");
    
    for (i = 0; i < SENSOR_COUNT; i++) {
        const SensorConfig_t *config = &g_sensor_configs[i];
        const SensorData_t *data = &g_sensor_data[i];
        
        if (g_sensor_read_success[i]) {
            if (i == 0) {
                /* 地址1：温湿度传感器，显示温度和湿度 */
                temp = CalculateTemperature(data->temperature);
                hum = CalculateHumidity(data->humidity);
                snprintf(buffer, sizeof(buffer), "%d:%s T:%.1f H:%.1f", 
                        config->address, config->name, temp, hum);
            } else {
                /* 地址2和3：气体传感器，显示浓度 */
                /* 解析寄存器0的单位和小数点配置 */
                unit_type = (uint8_t)((data->unit_decimal >> 12) & 0x0F);
                decimal_places = (uint8_t)((data->unit_decimal >> 8) & 0x0F);
                
                /* 计算浓度值（根据小数位数） */
                concentration = CalculateConcentration(data->concentration, decimal_places);
                
                /* 获取单位字符串 */
                unit_str = GetUnitString(unit_type);
                
                /* 根据小数位数格式化显示 */
                if (decimal_places == 0) {
                    /* 无小数位，显示整数 */
                    snprintf(buffer, sizeof(buffer), "%d:%s %.0f%s", 
                            config->address, config->name, concentration, unit_str);
                } else if (decimal_places == 4) {
                    /* 一位小数 */
                    snprintf(buffer, sizeof(buffer), "%d:%s %.1f%s", 
                            config->address, config->name, concentration, unit_str);
                } else if (decimal_places == 8) {
                    /* 两位小数 */
                    snprintf(buffer, sizeof(buffer), "%d:%s %.2f%s", 
                            config->address, config->name, concentration, unit_str);
                } else {
                    /* 三位小数 */
                    snprintf(buffer, sizeof(buffer), "%d:%s %.3f%s", 
                            config->address, config->name, concentration, unit_str);
                }
            }
        } else {
            /* 读取失败，显示错误信息 */
            snprintf(buffer, sizeof(buffer), "%d:%s Error", config->address, config->name);
        }
        
        OLED_ShowString(i + 2, 1, buffer);
    }
}

// /**
//  * @brief 切换到下一页
//  */
// static void NextPage(void)
// {
//     g_current_page++;
//     if (g_current_page >= PAGE_COUNT) {
//         g_current_page = 0;  /* 循环到第一页 */
//     }
// }

/**
 * @brief 输出传感器数据到串口
 */
static void PrintSensorData(void)
{
    uint8_t i;
    float temp, hum;
    float concentration;
    uint8_t unit_type;
    uint8_t decimal_places;
    const char *unit_str;
    
    LOG_INFO("SENSOR", "=== 传感器数据 ===");
    
    for (i = 0; i < SENSOR_COUNT; i++) {
        const SensorConfig_t *config = &g_sensor_configs[i];
        const SensorData_t *data = &g_sensor_data[i];
        
        if (g_sensor_read_success[i]) {
            if (i == 0) {
                /* 地址1：温湿度传感器 */
                temp = CalculateTemperature(data->temperature);
                hum = CalculateHumidity(data->humidity);
                LOG_INFO("SENSOR", "传感器%d (%s): 温度=%.1f℃, 湿度=%.1f%%, 状态=0x%02X",
                        config->address, config->name, temp, hum, data->status);
            } else {
                /* 地址2-6：气体传感器 */
                /* 解析寄存器0的单位和小数点配置 */
                unit_type = (uint8_t)((data->unit_decimal >> 12) & 0x0F);
                decimal_places = (uint8_t)((data->unit_decimal >> 8) & 0x0F);
                
                /* 计算浓度值（根据小数位数） */
                concentration = CalculateConcentration(data->concentration, decimal_places);
                
                /* 获取单位字符串 */
                unit_str = GetUnitString(unit_type);
                
                /* 根据小数位数格式化显示 */
                if (decimal_places == 0) {
                    LOG_INFO("SENSOR", "传感器%d (%s): 浓度=%.0f%s, 状态=0x%02X, 类型=%d",
                            config->address, config->name, concentration, unit_str,
                            data->status, data->gas_type);
                } else if (decimal_places == 4) {
                    LOG_INFO("SENSOR", "传感器%d (%s): 浓度=%.1f%s, 状态=0x%02X, 类型=%d",
                            config->address, config->name, concentration, unit_str,
                            data->status, data->gas_type);
                } else if (decimal_places == 8) {
                    LOG_INFO("SENSOR", "传感器%d (%s): 浓度=%.2f%s, 状态=0x%02X, 类型=%d",
                            config->address, config->name, concentration, unit_str,
                            data->status, data->gas_type);
                } else {
                    LOG_INFO("SENSOR", "传感器%d (%s): 浓度=%.3f%s, 状态=0x%02X, 类型=%d",
                            config->address, config->name, concentration, unit_str,
                            data->status, data->gas_type);
                }
            }
        } else {
            LOG_WARN("SENSOR", "传感器%d (%s): 读取失败", config->address, config->name);
        }
    }
}

/* ==================== 主函数 ==================== */

int main(void)
{
    UART_Status_t uart_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    OLED_Status_t oled_status;
    uint8_t i;
    
    /* ========== 步骤1：系统初始化 ========== */
    System_Init();
    
    /* ========== 步骤2：UART初始化 ========== */
    /* 初始化UART1（用于Debug输出） */
    uart_status = UART_Init(UART_INSTANCE_1);
    if (uart_status != UART_OK) {
        while(1) { Delay_ms(1000); }
    }
    
    /* 初始化UART2（用于ModBusRTU通信） */
    uart_status = UART_Init(UART_INSTANCE_2);
    if (uart_status != UART_OK) {
        while(1) { Delay_ms(1000); }
    }
    
    /* ========== 步骤3：Debug模块初始化（UART模式） ========== */
    debug_status = Debug_Init(DEBUG_MODE_UART, 115200);
    if (debug_status != 0) {
        while(1) { Delay_ms(1000); }
    }
    
    /* ========== 步骤4：Log模块初始化 ========== */
    memset(&log_config, 0, sizeof(log_config));
    log_config.level = LOG_LEVEL_DEBUG;
    log_config.enable_timestamp = 0;
    log_config.enable_module = 1;
    log_config.enable_color = 0;
    
    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK) {
        ErrorHandler_Handle(log_status, "LOG");
    }
    
    /* ========== 步骤5：错误处理模块 ========== */
    /* ErrorHandler模块在编译时自动初始化，无需显式调用 */
    
    /* ========== 步骤6：输出初始化信息 ========== */
    LOG_INFO("MAIN", "=== Bus01 ModBusRTU气体传感器读取示例 ===");
    LOG_INFO("MAIN", "系统初始化完成");
    LOG_INFO("MAIN", "UART1已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "UART2已初始化: PA2(TX), PA3(RX), 9600");
    LOG_INFO("MAIN", "Debug模块已初始化: UART模式");
    LOG_INFO("MAIN", "Log模块已初始化");
    LOG_INFO("MAIN", "ErrorHandler模块已就绪");
    
    /* ========== 步骤7：初始化OLED ========== */
    oled_status = OLED_Init();
    if (oled_status == OLED_OK) {
        OLED_Clear();
        OLED_ShowString(1, 1, "Bus01 ModBusRTU");
        OLED_ShowString(2, 1, "Initializing...");
        LOG_INFO("MAIN", "OLED已初始化并显示");
    } else {
        LOG_ERROR("MAIN", "OLED初始化失败: %d", oled_status);
        ErrorHandler_Handle(oled_status, "OLED");
    }
    
    Delay_ms(1000);
    
    /* ========== 步骤8：主循环 ========== */
    LOG_INFO("MAIN", "开始读取传感器数据...");
    
    while (1) {
        /* 轮询读取3个传感器 */
        for (i = 0; i < SENSOR_COUNT; i++) {
            (void)ReadSensorData(i);  /* 读取传感器数据，忽略返回值（错误已在函数内处理） */
            
            /* 每个传感器读取间隔500ms（符合协议要求） */
            if (i < SENSOR_COUNT - 1) {
                Delay_ms(500);
            }
        }
        
        /* 显示数据 */
        DisplaySensorData();
        
        /* 输出日志 */
        PrintSensorData();
        
        /* 轮询间隔500ms */
        Delay_ms(500);
    }
}

