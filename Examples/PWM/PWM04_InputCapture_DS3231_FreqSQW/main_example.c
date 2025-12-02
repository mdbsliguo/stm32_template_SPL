/**
 * @file main_example.c
 * @brief PWM04 - DS3231 SQW输入捕获测量示例
 * @details 演示使用输入捕获功能测量DS3231的SQW不同频率输出（1Hz、1.024kHz、4.096kHz、8.192kHz）
 * @version 1.0.0
 * @date 2024-01-01
 *
 * 功能说明：
 * - 使用DS3231的SQW引脚输出不同频率的方波（1Hz、1.024kHz、4.096kHz、8.192kHz）
 * - 使用TIM3_CH1（PA6）输入捕获测量SQW频率
 * - 循环切换不同的SQW频率并测量
 * - OLED显示当前配置频率和测量结果
 * - UART输出详细日志（支持中文，GB2312编码）
 *
 * 硬件要求：
 * - DS3231实时时钟模块（I2C接口）
 *   - SCL连接到PB10（软件I2C2）
 *   - SDA连接到PB11（软件I2C2）
 *   - SQW/INT引脚连接到PA6（TIM3_CH1，输入捕获）
 *   - VCC连接到3.3V
 *   - GND连接到GND
 * - USART1：PA9(TX), PA10(RX)，115200波特率
 * - OLED显示屏：SSD1306（I2C接口），PB8(SCL), PB9(SDA)
 *
 * 技术要点：
 * - DS3231配置方波输出为不同频率（DS3231_SetSquareWave）
 * - TIM3_CH1配置为输入捕获模式（上升沿）
 * - 使用IC_MeasureFrequency测量SQW频率
 * - 循环测试不同频率并对比测量结果
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
#include "i2c_sw.h"
#include "ds3231.h"
#include "timer_input_capture.h"
#include <stdio.h>
#include <string.h>

/* ========== 全局变量 ========== */

static uint32_t last_measure_time = 0;
static uint32_t last_oled_update_time = 0;
static uint32_t last_freq_change_time = 0;

/* SQW频率配置表 */
typedef struct {
    DS3231_SquareWaveFreq_t freq_enum;
    const char *freq_name;
    uint32_t expected_freq;  /* 期望频率（Hz） */
} SQW_FreqConfig_t;

static const SQW_FreqConfig_t sqw_freq_configs[] = {
    {DS3231_SQW_1HZ,     "1Hz",     1},
    {DS3231_SQW_1024HZ, "1.024kHz", 1024},
    {DS3231_SQW_4096HZ, "4.096kHz", 4096},
    {DS3231_SQW_8192HZ, "8.192kHz", 8192},
};

#define SQW_FREQ_COUNT (sizeof(sqw_freq_configs) / sizeof(sqw_freq_configs[0]))

static uint8_t current_freq_index = 0;  /* 当前频率索引 */

/* ========== 函数声明 ========== */

static void UpdateOLEDDisplay(const SQW_FreqConfig_t *config, uint32_t measured_freq);
static void ChangeSQWFrequency(uint8_t freq_index);

/* ========== 主函数 ========== */

int main(void)
{
    UART_Status_t uart_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    SoftI2C_Status_t soft_i2c_status;
    OLED_Status_t oled_status;
    DS3231_Status_t ds3231_status;
    IC_Status_t ic_status;
    DS3231_Config_t ds3231_config;
    uint8_t osf_flag;
    uint32_t sqw_frequency = 0;
    
    /* ========== 步骤1：系统初始化 ========== */
    System_Init();
    
    /* ========== 步骤2：初始化UART1 ========== */
    uart_status = UART_Init(UART_INSTANCE_1);
    if (uart_status != UART_OK)
    {
        /* UART初始化失败，无法继续，进入死循环 */
        while(1) { Delay_ms(1000); }
    }
    Delay_ms(100);
    
    /* ========== 步骤3：初始化Debug模块（UART模式） ========== */
    debug_status = Debug_Init(DEBUG_MODE_UART, 115200);
    if (debug_status != 0)
    {
        /* Debug初始化失败，无法继续，进入死循环 */
        while(1) { Delay_ms(1000); }
    }
    Delay_ms(100);
    
    /* ========== 步骤4：初始化Log模块 ========== */
    memset(&log_config, 0, sizeof(log_config));
    log_config.level = LOG_LEVEL_DEBUG;        /* 日志级别：DEBUG（显示所有日志） */
    log_config.enable_timestamp = 0;          /* 禁用时间戳（简化输出） */
    log_config.enable_module = 1;              /* 启用模块名显示 */
    log_config.enable_color = 0;               /* 禁用颜色输出（串口助手可能不支持） */
    
    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK)
    {
        ErrorHandler_Handle(log_status, "LOG");
        /* Log初始化失败，但可以继续运行（使用UART直接输出） */
    }
    
    /* ========== 步骤5：错误处理模块 ========== */
    /* 注意：ErrorHandler模块在编译时自动初始化，无需显式调用 */
    
    /* ========== 步骤6：输出初始化信息 ========== */
    LOG_INFO("MAIN", "=== PWM04 输入捕获测DS3231 SQW频率示例 ===");
    LOG_INFO("MAIN", "系统初始化完成");
    LOG_INFO("MAIN", "UART1已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "Debug模块已初始化: UART模式");
    LOG_INFO("MAIN", "Log模块已初始化");
    LOG_INFO("MAIN", "ErrorHandler模块已就绪");
    
    /* ========== 步骤7：初始化OLED ========== */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        LOG_ERROR("MAIN", "OLED初始化失败: %d", oled_status);
        ErrorHandler_Handle(oled_status, "OLED");
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "OLED已初始化");
    
    /* ========== 步骤8：初始化软件I2C2（用于DS3231） ========== */
    LOG_INFO("MAIN", "初始化软件I2C2（PB10/11，用于DS3231）");
    soft_i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_2);
    if (soft_i2c_status != SOFT_I2C_OK)
    {
        LOG_ERROR("MAIN", "软件I2C2初始化失败: %d", soft_i2c_status);
        ErrorHandler_Handle(soft_i2c_status, "SoftI2C");
        if (oled_status == OLED_OK)
        {
            OLED_ShowString(3, 1, "I2C Init Fail!");
        }
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "软件I2C2已初始化: PB10(SCL), PB11(SDA)");
    
    /* ========== 步骤9：初始化DS3231 ========== */
    LOG_INFO("MAIN", "初始化DS3231模块");
    ds3231_config.interface_type = DS3231_INTERFACE_SOFTWARE;
    ds3231_config.config.software.soft_i2c_instance = SOFT_I2C_INSTANCE_2;
    
    ds3231_status = DS3231_Init(&ds3231_config);
    if (ds3231_status != DS3231_OK)
    {
        LOG_ERROR("MAIN", "DS3231初始化失败: %d", ds3231_status);
        ErrorHandler_Handle(ds3231_status, "DS3231");
        if (oled_status == OLED_OK)
        {
            OLED_ShowString(3, 1, "DS3231 Init Fail!");
        }
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "DS3231已初始化");
    
    /* ========== 步骤10：检查并清除OSF标志 ========== */
    ds3231_status = DS3231_CheckOSF(&osf_flag);
    if (ds3231_status == DS3231_OK)
    {
        if (osf_flag)
        {
            LOG_WARN("MAIN", "DS3231 OSF标志已设置，正在清除");
            DS3231_ClearOSF();
            Delay_ms(100);
        }
        LOG_INFO("MAIN", "DS3231 OSF标志正常");
    }
    
    /* ========== 步骤11：启动DS3231振荡器 ========== */
    ds3231_status = DS3231_Start();
    if (ds3231_status != DS3231_OK)
    {
        LOG_ERROR("MAIN", "DS3231启动失败: %d", ds3231_status);
        ErrorHandler_Handle(ds3231_status, "DS3231");
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "DS3231振荡器已启动");
    
    /* ========== 步骤12：配置DS3231中断模式为方波输出 ========== */
    ds3231_status = DS3231_SetInterruptMode(DS3231_INT_MODE_SQUARE_WAVE);
    if (ds3231_status != DS3231_OK)
    {
        LOG_ERROR("MAIN", "DS3231中断模式配置失败: %d", ds3231_status);
        ErrorHandler_Handle(ds3231_status, "DS3231");
    }
    else
    {
        LOG_INFO("MAIN", "DS3231中断模式已设置为方波输出模式（INTCN=0）");
    }
    
    /* ========== 步骤13：初始化输入捕获（TIM3_CH1，用于SQW） ========== */
    LOG_INFO("MAIN", "初始化输入捕获: TIM3_CH1（PA6，SQW）");
    ic_status = IC_Init(IC_INSTANCE_TIM3, IC_CHANNEL_1, IC_POLARITY_RISING);
    if (ic_status != IC_OK)
    {
        LOG_ERROR("MAIN", "输入捕获初始化失败: %d", ic_status);
        ErrorHandler_Handle(ic_status, "IC");
        if (oled_status == OLED_OK)
        {
            OLED_ShowString(3, 1, "IC Init Fail!");
        }
        while(1) { Delay_ms(1000); }
    }
    
    /* ========== 步骤14：启动输入捕获 ========== */
    LOG_INFO("MAIN", "输入捕获已启动: TIM3_CH1（SQW）");
    ic_status = IC_Start(IC_INSTANCE_TIM3, IC_CHANNEL_1);
    if (ic_status != IC_OK)
    {
        LOG_ERROR("MAIN", "输入捕获启动失败: %d", ic_status);
        ErrorHandler_Handle(ic_status, "IC");
    }
    
    /* ========== 步骤15：配置初始SQW频率 ========== */
    LOG_INFO("MAIN", "配置初始SQW频率");
    ChangeSQWFrequency(0);  /* 从1Hz开始 */
    
    /* 等待DS3231输出稳定 */
    Delay_ms(500);
    LOG_INFO("MAIN", "DS3231输出已稳定，可以开始测量");
    
    /* ========== 步骤16：主循环 ========== */
    LOG_INFO("MAIN", "初始化完成，开始测量频率");
    
    while (1)
    {
        uint32_t current_time = Delay_GetTick();
        
        /* 每5秒切换一次SQW频率 */
        if (Delay_GetElapsed(last_freq_change_time, current_time) >= 5000)
        {
            last_freq_change_time = current_time;
            current_freq_index = (current_freq_index + 1) % SQW_FREQ_COUNT;
            ChangeSQWFrequency(current_freq_index);
            Delay_ms(500);  /* 等待频率切换稳定 */
        }
        
        /* 每500ms测量一次频率 */
        if (Delay_GetElapsed(last_measure_time, current_time) >= 500)
        {
            last_measure_time = current_time;
            
            /* 测量SQW频率 */
            /* 低频用测周法，高频用测频法 */
            /* 测周法（< 2kHz）：测量一个完整周期的时间，精度高但测量时间长 */
            /* 测频法（>= 2kHz）：在固定时间内计数脉冲数量，测量时间短但精度相对较低 */
            const SQW_FreqConfig_t *config = &sqw_freq_configs[current_freq_index];
            
            if (config->expected_freq < 2000) {
                /* 低频信号使用测周法（< 2kHz） */
                uint32_t timeout_ms;
                if (config->expected_freq <= 1) {
                    timeout_ms = 2500;  /* 1Hz：需要等待2个上升沿，每个最多1秒，总共最多2秒，加上余量 */
                } else {
                    timeout_ms = 100;   /* 1.024kHz：周期约1ms，等待2个上升沿，加上余量 */
                }
                ic_status = IC_MeasureFrequency(IC_INSTANCE_TIM3, IC_CHANNEL_1, &sqw_frequency, timeout_ms);
            } else {
                /* 高频信号也使用测周法（>= 2kHz） */
                /* 注意：测频法需要快速循环检查，可能错过上升沿，所以高频信号也使用测周法 */
                /* 测周法对于高频信号也是可行的，只要定时器时钟足够快 */
                uint32_t timeout_ms = 50;  /* 高频信号周期短，50ms足够等待2个上升沿 */
                ic_status = IC_MeasureFrequency(IC_INSTANCE_TIM3, IC_CHANNEL_1, &sqw_frequency, timeout_ms);
            }
            if (ic_status != IC_OK)
            {
                LOG_WARN("IC", "SQW频率测量失败: %d", ic_status);
                sqw_frequency = 0;
            }
            else
            {
                LOG_INFO("IC", "SQW频率: 配置=%s, 期望=%lu Hz, 测量=%lu Hz, 误差=%ld Hz",
                         config->freq_name, config->expected_freq, sqw_frequency,
                         (int32_t)sqw_frequency - (int32_t)config->expected_freq);
            }
        }
        
        /* 每200ms更新一次OLED显示 */
        if (Delay_GetElapsed(last_oled_update_time, current_time) >= 200)
        {
            last_oled_update_time = current_time;
            UpdateOLEDDisplay(&sqw_freq_configs[current_freq_index], sqw_frequency);
        }
        
        Delay_ms(50);
    }
}

/* ========== 辅助函数 ========== */

/**
 * @brief 切换SQW频率
 */
static void ChangeSQWFrequency(uint8_t freq_index)
{
    DS3231_Status_t ds3231_status;
    const SQW_FreqConfig_t *config;
    
    if (freq_index >= SQW_FREQ_COUNT)
    {
        return;
    }
    
    config = &sqw_freq_configs[freq_index];
    
    LOG_INFO("MAIN", "切换SQW频率: %s (期望: %lu Hz)", config->freq_name, config->expected_freq);
    
    ds3231_status = DS3231_SetSquareWave(config->freq_enum, 1);
    if (ds3231_status != DS3231_OK)
    {
        LOG_ERROR("MAIN", "DS3231 SQW配置失败: %d", ds3231_status);
        ErrorHandler_Handle(ds3231_status, "DS3231");
    }
    else
    {
        LOG_INFO("MAIN", "DS3231 SQW输出已切换: %s", config->freq_name);
    }
}

/**
 * @brief 更新OLED显示
 */
static void UpdateOLEDDisplay(const SQW_FreqConfig_t *config, uint32_t measured_freq)
{
    char buf[17];
    int32_t error;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "PWM04 IC Demo");
    
    /* 显示配置频率 */
    snprintf(buf, sizeof(buf), "Set: %s", config->freq_name);
    OLED_ShowString(2, 1, buf);
    
    /* 显示测量频率 */
    if (measured_freq > 0)
    {
        if (measured_freq < 1000)
        {
            snprintf(buf, sizeof(buf), "Meas: %lu Hz", measured_freq);
        }
        else if (measured_freq < 10000)
        {
            snprintf(buf, sizeof(buf), "Meas: %luHz", measured_freq);
        }
        else
        {
            snprintf(buf, sizeof(buf), "Meas: %lukHz", measured_freq / 1000);
        }
        OLED_ShowString(3, 1, buf);
        
        /* 显示误差 */
        error = (int32_t)measured_freq - (int32_t)config->expected_freq;
        if (error >= 0)
        {
            snprintf(buf, sizeof(buf), "Err: +%ld Hz", error);
        }
        else
        {
            snprintf(buf, sizeof(buf), "Err: %ld Hz", error);
        }
        OLED_ShowString(4, 1, buf);
    }
    else
    {
        OLED_ShowString(3, 1, "Meas: -- Hz");
        OLED_ShowString(4, 1, "Err: --");
    }
}
