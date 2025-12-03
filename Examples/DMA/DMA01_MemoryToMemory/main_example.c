/**
 * @file main_example.c
 * @brief DMA01案例 - 内存到内存数据搬运示例
 * @details 实现DMA内存到内存数据搬运功能，展示DMA的基本数据搬运能力
 * 
 * 功能说明：
 * - 使用DMA1_CH7通道实现内存到内存数据搬运
 * - 传输完成后验证数据正确性
 * - 在OLED上显示传输状态和结果
 * - 通过串口输出详细的传输信息
 * 
 * 硬件要求：
 * - DMA1_CH7：内存到内存传输通道
 * - OLED：I2C接口，用于结果显示
 * - UART1：串口调试输出
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "uart.h"
#include "dma.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "error_code.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include "led.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* ==================== 全局变量 ==================== */

/* DMA传输缓冲区 */
#define DMA_BUFFER_SIZE 64
static uint8_t src_buffer[DMA_BUFFER_SIZE];
static uint8_t dst_buffer[DMA_BUFFER_SIZE];

/* 传输计数 */
static uint32_t transfer_count = 0;

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化DMA内存到内存传输
 * @return error_code_t 错误码
 */
static error_code_t DMA_MemoryToMemory_Init(void);

/**
 * @brief 验证传输结果
 * @return uint8_t 1-验证成功，0-验证失败
 */
static uint8_t VerifyTransfer(void);

/**
 * @brief 更新OLED显示
 */
static void UpdateOLEDDisplay(uint8_t transfer_ok, uint8_t verify_ok);

/**
 * @brief 准备测试数据（动态模式）
 * @param[in] pattern 数据模式（0=递增，1=递减，2=波形，3=随机）
 */
static void PrepareTestData(uint8_t pattern);

/* ==================== 主函数 ==================== */

int main(void)
{
    UART_Status_t uart_status;
    Log_Status_t log_status;
    OLED_Status_t oled_status;
    DMA_Status_t dma_status;
    int debug_status;
    log_config_t log_config;
    error_code_t error_code;
    uint8_t transfer_ok = 0;
    uint8_t verify_ok = 0;
    
    /* ========== 步骤1：系统初始化 ========== */
    System_Init();
    
    /* ========== 步骤2：UART初始化 ========== */
    uart_status = UART_Init(UART_INSTANCE_1);
    if (uart_status != UART_OK)
    {
        while (1);
    }
    Delay_ms(100);
    
    /* ========== 步骤3：Debug模块初始化（UART模式） ========== */
    debug_status = Debug_Init(DEBUG_MODE_UART, 115200);
    if (debug_status != 0)
    {
        while (1);
    }
    Delay_ms(100);
    
    /* ========== 步骤4：Log模块初始化 ========== */
    memset(&log_config, 0, sizeof(log_config));
    log_config.level = LOG_LEVEL_INFO;
    log_config.enable_timestamp = 0;
    log_config.enable_module = 1;
    log_config.enable_color = 0;
    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK)
    {
        ErrorHandler_Handle(log_status, "LOG");
    }
    
    LOG_INFO("MAIN", "=== DMA01案例：内存到内存数据搬运 ===");
    LOG_INFO("MAIN", "UART1已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "Debug模块已初始化: UART模式");
    LOG_INFO("MAIN", "Log模块已初始化");
    
    /* ========== 步骤5：OLED初始化 ========== */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        ErrorHandler_Handle(oled_status, "OLED");
        while (1);
    }
    LOG_INFO("MAIN", "OLED已初始化并显示");
    
    /* 显示初始界面 */
    OLED_Clear();
    OLED_ShowString(1, 1, "DMA01 M2M");
    OLED_ShowString(2, 1, "Status: Init");
    OLED_ShowString(3, 1, "Count: 0");
    OLED_ShowString(4, 1, "Data: 0x00");
    
    /* ========== 步骤6：初始化传输计数 ========== */
    transfer_count = 0;
    
    /* ========== 步骤7：初始化DMA内存到内存传输 ========== */
    LOG_INFO("MAIN", "--- 初始化DMA内存到内存传输 ---");
    error_code = DMA_MemoryToMemory_Init();
    if (error_code != ERROR_OK)
    {
        ErrorHandler_Handle(error_code, "DMA");
        LOG_ERROR("MAIN", "DMA初始化失败: %d", error_code);
        OLED_ShowString(2, 1, "Status: FAIL");
        while (1);
    }
    LOG_INFO("MAIN", "DMA内存到内存传输已初始化");
    LOG_INFO("MAIN", "DMA通道: DMA1_CH7");
    LOG_INFO("MAIN", "缓冲区大小: %d字节", DMA_BUFFER_SIZE);
    
    /* ========== 步骤8：主循环 - 反复进行DMA传输 ========== */
    LOG_INFO("MAIN", "--- 进入主循环，开始反复DMA传输 ---");
    
    uint8_t pattern = 0;  /* 数据模式：0=递增，1=递减，2=波形，3=随机 */
    
    while (1)
    {
        transfer_count++;
        
        /* 准备测试数据（每次使用不同的模式） */
        pattern = (transfer_count - 1) % 4;  /* 循环使用4种模式 */
        PrepareTestData(pattern);
        
        /* 清零目标缓冲区 */
        memset(dst_buffer, 0, DMA_BUFFER_SIZE);
        
        /* LED闪烁表示开始传输 */
        LED1_On();
        
        /* 配置内存到内存传输 */
        dma_status = DMA_ConfigMemoryToMemory(DMA_CHANNEL_1_7,
                                              (uint32_t)src_buffer,
                                              (uint32_t)dst_buffer,
                                              DMA_BUFFER_SIZE,
                                              1);  /* 字节传输 */
        if (dma_status != DMA_OK)
        {
            ErrorHandler_Handle(dma_status, "DMA");
            LOG_ERROR("MAIN", "[%lu] DMA配置失败: %d", transfer_count, dma_status);
            OLED_ShowString(2, 1, "Status: FAIL");
            LED1_Off();
            Delay_ms(1000);
            continue;
        }
        
        /* 启动DMA传输 */
        dma_status = DMA_Start(DMA_CHANNEL_1_7);
        if (dma_status != DMA_OK)
        {
            ErrorHandler_Handle(dma_status, "DMA");
            LOG_ERROR("MAIN", "[%lu] DMA启动失败: %d", transfer_count, dma_status);
            OLED_ShowString(2, 1, "Status: FAIL");
            LED1_Off();
            Delay_ms(1000);
            continue;
        }
        
        /* ========== 关键验证：DMA传输期间CPU可以执行其他任务 ========== */
        /* 在DMA传输过程中，CPU可以执行其他操作（如LED闪烁、计数等） */
        /* 这证明数据传输是由DMA硬件完成的，而不是CPU复制 */
        uint32_t dma_start_tick = Delay_GetTick();
        uint8_t led_blink_count = 0;
        
        /* 等待传输完成，期间LED快速闪烁（证明CPU空闲） */
        do {
            /* 在等待期间，LED快速闪烁，证明CPU可以执行其他任务 */
            if (led_blink_count % 2 == 0)
            {
                LED1_On();
            }
            else
            {
                LED1_Off();
            }
            led_blink_count++;
            Delay_ms(1);  /* 短暂延时，让LED闪烁可见 */
            
            /* 检查DMA是否完成 */
            dma_status = DMA_IsComplete(DMA_CHANNEL_1_7) ? DMA_OK : DMA_ERROR_BUSY;
            
            /* 超时检查 */
            if (Delay_GetElapsed(Delay_GetTick(), dma_start_tick) > 1000)
            {
                dma_status = DMA_ERROR_TIMEOUT;
                break;
            }
        } while (dma_status == DMA_ERROR_BUSY);
        
        /* LED熄灭 */
        LED1_Off();
        
        if (dma_status != DMA_OK)
        {
            ErrorHandler_Handle(dma_status, "DMA");
            LOG_ERROR("MAIN", "[%lu] DMA传输超时或失败: %d", transfer_count, dma_status);
            transfer_ok = 0;
        }
        else
        {
            transfer_ok = 1;
            /* 计算传输耗时（微秒级，DMA传输很快） */
            uint32_t elapsed_ms = Delay_GetElapsed(Delay_GetTick(), dma_start_tick);
            if (transfer_count % 10 == 0)
            {
                LOG_INFO("MAIN", "[%lu] DMA传输耗时: %lu ms (LED闪烁 %d 次)", 
                        transfer_count, elapsed_ms, led_blink_count);
            }
        }
        
        /* 验证传输结果 */
        verify_ok = VerifyTransfer();
        
        /* 更新OLED显示 */
        UpdateOLEDDisplay(transfer_ok, verify_ok);
        
        /* 每10次传输输出一次日志 */
        if (transfer_count % 10 == 0)
        {
            LOG_INFO("MAIN", "[%lu] 传输完成，验证: %s", 
                    transfer_count, verify_ok ? "OK" : "FAIL");
        }
        
        /* 延时500ms，让效果更明显 */
        Delay_ms(500);
    }
}

/* ==================== 函数实现 ==================== */

/**
 * @brief 初始化DMA内存到内存传输
 */
static error_code_t DMA_MemoryToMemory_Init(void)
{
    DMA_Status_t dma_status;
    
    /* 初始化DMA通道（DMA1_CH7用于内存到内存传输） */
    dma_status = DMA_HW_Init(DMA_CHANNEL_1_7);
    if (dma_status != DMA_OK)
    {
        return ERROR_BASE_DMA - 1;
    }
    
    return ERROR_OK;
}

/**
 * @brief 验证传输结果
 */
static uint8_t VerifyTransfer(void)
{
    uint8_t match = 1;
    uint16_t first_error_index = 0;
    
    /* 逐字节比较源数据和目标数据 */
    for (uint16_t i = 0; i < DMA_BUFFER_SIZE; i++)
    {
        if (src_buffer[i] != dst_buffer[i])
        {
            if (match)
            {
                first_error_index = i;
                match = 0;
            }
            LOG_ERROR("MAIN", "数据不匹配 [%d]: 源=0x%02X, 目标=0x%02X", 
                     i, src_buffer[i], dst_buffer[i]);
        }
    }
    
    if (match)
    {
        LOG_INFO("MAIN", "数据验证：所有%d字节数据完全匹配", DMA_BUFFER_SIZE);
        /* 输出前几个字节作为示例 */
        LOG_INFO("MAIN", "前8字节示例:");
        for (uint8_t i = 0; i < 8 && i < DMA_BUFFER_SIZE; i++)
        {
            LOG_INFO("MAIN", "  [%d]: 源=0x%02X, 目标=0x%02X", 
                    i, src_buffer[i], dst_buffer[i]);
        }
    }
    else
    {
        LOG_ERROR("MAIN", "数据验证失败：第一个错误位置 [%d]", first_error_index);
    }
    
    return match;
}

/**
 * @brief 准备测试数据（动态模式）
 */
static void PrepareTestData(uint8_t pattern)
{
    switch (pattern)
    {
        case 0:  /* 递增模式 */
            for (uint16_t i = 0; i < DMA_BUFFER_SIZE; i++)
            {
                src_buffer[i] = (uint8_t)((i + transfer_count) & 0xFF);
            }
            break;
            
        case 1:  /* 递减模式 */
            for (uint16_t i = 0; i < DMA_BUFFER_SIZE; i++)
            {
                src_buffer[i] = (uint8_t)((0xFF - (i + transfer_count)) & 0xFF);
            }
            break;
            
        case 2:  /* 波形模式（锯齿波） */
            for (uint16_t i = 0; i < DMA_BUFFER_SIZE; i++)
            {
                /* 简单的锯齿波：0-255循环 */
                src_buffer[i] = (uint8_t)((i + transfer_count) & 0xFF);
            }
            break;
            
        case 3:  /* 随机模式（伪随机） */
            for (uint16_t i = 0; i < DMA_BUFFER_SIZE; i++)
            {
                /* 简单的伪随机数生成 */
                uint32_t seed = transfer_count * 1103515245 + 12345;
                src_buffer[i] = (uint8_t)((seed >> (i % 16)) & 0xFF);
            }
            break;
            
        default:
            /* 默认递增模式 */
            for (uint16_t i = 0; i < DMA_BUFFER_SIZE; i++)
            {
                src_buffer[i] = (uint8_t)(i & 0xFF);
            }
            break;
    }
}

/**
 * @brief 更新OLED显示
 */
static void UpdateOLEDDisplay(uint8_t transfer_ok, uint8_t verify_ok)
{
    char display_str[17];
    
    /* 显示传输状态 */
    if (transfer_ok && verify_ok)
    {
        OLED_ShowString(2, 1, "Status: OK ");
    }
    else
    {
        OLED_ShowString(2, 1, "Status: FAIL");
    }
    
    /* 显示传输次数 */
    snprintf(display_str, sizeof(display_str), "Count: %lu", transfer_count);
    OLED_ShowString(3, 1, display_str);
    
    /* 显示第一个字节的数据（十六进制） */
    snprintf(display_str, sizeof(display_str), "Data: 0x%02X", dst_buffer[0]);
    OLED_ShowString(4, 1, display_str);
}

