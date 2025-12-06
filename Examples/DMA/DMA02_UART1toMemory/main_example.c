/**
 * @file main_example.c
 * @brief DMA02案例 - UART1 DMA接收示例
 * @details 实现UART1（PA9、PA10）的DMA接收功能，接收数据回传并在OLED上显示
 * 
 * 功能说明：
 * - UART1输入通过DMA自动搬运到内存缓冲区
 * - 接收到的数据通过串口回传（Echo功能）
 * - 接收到的数据实时显示在OLED上（十六进制格式）
 * 
 * 硬件要求：
 * - UART1：PA9(TX), PA10(RX), 115200
 * - OLED：I2C接口（PB8/PB9）
 * 
 * 注意：当前使用单字节DMA接收，如果DMA不响应，会fallback到中断方式
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

/* UART1 DMA接收缓冲区 */
#define UART1_RX_BUFFER_SIZE 1024  /* DMA接收缓冲区：1KB，DMA负责搬运到内存 */
static uint8_t uart1_rx_buffer[UART1_RX_BUFFER_SIZE] = {0};
static uint16_t uart1_rx_processed = 0;  /* 已处理的数据索引（轮询内存缓冲区用） */

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化UART1 DMA接收
 * @return error_code_t 错误码
 */
static error_code_t UART1_DMA_Init(void);

/**
 * @brief 重新启动DMA接收
 */
static void UART1_DMA_Restart(void);


/* ==================== 主函数 ==================== */

int main(void)
{
    UART_Status_t uart_status;
    Log_Status_t log_status;
    OLED_Status_t oled_status;
    int debug_status;
    log_config_t log_config;
    error_code_t error_code;
    
    /* ========== 步骤1：系统初始化 ========== */
    System_Init();
    
    /* ========== 步骤2：UART1初始化 ========== */
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
    
    LOG_INFO("MAIN", "=== DMA02案例：UART1 DMA接收 ===");
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
    
    /* 显示初始界面（参考提供的代码风格） */
    OLED_Clear();
    OLED_ShowString(1, 1, "RxData:");  /* 第一行第1列显示标签 */
    
    /* ========== 步骤6：初始化UART1 DMA接收 ========== */
    LOG_INFO("MAIN", "--- 初始化UART1 DMA接收 ---");
    error_code = UART1_DMA_Init();
    if (error_code != ERROR_OK)
    {
        ErrorHandler_Handle(error_code, "DMA");
        LOG_ERROR("MAIN", "DMA初始化失败: %d", error_code);
        OLED_ShowString(2, 1, "Init FAIL");
        while (1);
    }
    LOG_INFO("MAIN", "UART1 DMA接收已初始化，缓冲区大小: %d字节", UART1_RX_BUFFER_SIZE);
    LOG_INFO("MAIN", "UART1 DMA通道: DMA1_CH5");
    LOG_INFO("MAIN", "等待接收数据...");
    LOG_INFO("MAIN", "提示：请通过串口助手发送数据（115200波特率）");
    
    /* 调试：检查DMA和UART状态 */
    {
        DMA_Channel_TypeDef *dma_ch = DMA_GetChannel(DMA_CHANNEL_1_5);
        USART_TypeDef *uart_periph = UART_GetPeriph(UART_INSTANCE_1);
        
        if (dma_ch != NULL)
        {
            LOG_INFO("MAIN", "DMA状态: CCR=0x%04X, CNDTR=%d", dma_ch->CCR, dma_ch->CNDTR);
        }
        if (uart_periph != NULL)
        {
            LOG_INFO("MAIN", "UART状态: SR=0x%04X, CR3=0x%04X", uart_periph->SR, uart_periph->CR3);
        }
    }
    
    /* ========== 步骤7：主循环 ========== */
    /* 核心思路：DMA负责将UART数据搬运到内存缓冲区，CPU轮询内存缓冲区处理数据 */
    uint32_t loop_count = 0;
    
    while (1)
    {
        loop_count++;
        
        /* LED闪烁，证明程序在运行 */
        if (loop_count % 50 == 0)
        {
            LED1_On();
        }
        else if (loop_count % 50 == 25)
        {
            LED1_Off();
        }
        
        /* ========== 轮询内存缓冲区，检查是否有新数据 ========== */
        /* DMA负责搬运数据到内存，CPU轮询内存缓冲区处理数据 */
        uint16_t remaining = DMA_GetRemainingDataSize(DMA_CHANNEL_1_5);
        uint16_t received_count = UART1_RX_BUFFER_SIZE - remaining;  /* 已接收的字节数 */
        
        /* 检查是否有新数据到达（已接收的数据数 > 已处理的数据数） */
        if (received_count > uart1_rx_processed)
        {
            /* 处理新接收的数据（从已处理位置到当前接收位置） */
            while (uart1_rx_processed < received_count)
            {
                uint8_t rx_data = uart1_rx_buffer[uart1_rx_processed];
                
                /* 回传功能：将接收到的数据原样发送回串口（Echo测试） */
                UART_Transmit(UART_INSTANCE_1, &rx_data, 1, 100);
                
                /* 更新OLED显示（显示最新接收的字节） */
                OLED_ShowHexNum(1, 8, rx_data, 2);
                
                /* 输出日志 */
                LOG_INFO("MAIN", "收到数据[%d]: 0x%02X", uart1_rx_processed, rx_data);
                
                uart1_rx_processed++;
            }
            
            /* 输出完整接收缓冲区内容（十六进制格式） */
            {
                char hex_str[512] = {0};  /* 预留512字节用于十六进制字符串 */
                char *p = hex_str;
                uint16_t len = received_count;
                uint16_t max_display = 128;  /* 最多显示128字节，避免日志过长 */
                if (len > max_display) len = max_display;
                
                for (uint16_t i = 0; i < len; i++)
                {
                    p += sprintf(p, "%02X ", uart1_rx_buffer[i]);
                    if (p - hex_str > 500) break;  /* 防止溢出 */
                }
                
                if (received_count > max_display)
                {
                    LOG_INFO("MAIN", "完整内容(HEX)[%d字节，显示前%d字节]: %s...", received_count, max_display, hex_str);
                }
                else
                {
                    LOG_INFO("MAIN", "完整内容(HEX)[%d字节]: %s", received_count, hex_str);
                }
            }
            
            /* 输出完整接收缓冲区内容（ASCII原文格式） */
            {
                char ascii_str[256] = {0};  /* 预留256字节用于ASCII字符串 */
                char *p = ascii_str;
                uint16_t len = received_count;
                uint16_t max_display = 128;  /* 最多显示128字节，避免日志过长 */
                if (len > max_display) len = max_display;
                
                for (uint16_t i = 0; i < len; i++)
                {
                    uint8_t ch = uart1_rx_buffer[i];
                    if (ch >= 32 && ch < 127)  /* 可打印ASCII字符 */
                    {
                        *p++ = (char)ch;
                    }
                    else  /* 不可打印字符显示为点 */
                    {
                        *p++ = '.';
                    }
                    if (p - ascii_str > 250) break;  /* 防止溢出 */
                }
                *p = '\0';
                
                if (received_count > max_display)
                {
                    LOG_INFO("MAIN", "完整内容(ASCII)[%d字节，显示前%d字节]: %s...", received_count, max_display, ascii_str);
                }
                else
                {
                    LOG_INFO("MAIN", "完整内容(ASCII)[%d字节]: %s", received_count, ascii_str);
                }
            }
        }
        
        /* 防止卡死：当接收数据达到700字节时，处理并清空缓冲区 */
        if (received_count >= 700)
        {
            /* 处理所有已接收的数据（只处理非零数据，避免处理清空后的0x00） */
            uint16_t actual_processed = 0;
            while (uart1_rx_processed < received_count)
            {
                uint8_t rx_data = uart1_rx_buffer[uart1_rx_processed];
                
                /* 只处理非零数据 */
                if (rx_data != 0)
                {
                    /* 回传功能：将接收到的数据原样发送回串口（Echo测试） */
                    UART_Transmit(UART_INSTANCE_1, &rx_data, 1, 100);
                    
                    /* 更新OLED显示（显示最新接收的字节） */
                    OLED_ShowHexNum(1, 8, rx_data, 2);
                    
                    /* 输出日志 */
                    LOG_INFO("MAIN", "收到数据[%d]: 0x%02X", uart1_rx_processed, rx_data);
                    
                    actual_processed++;
                }
                
                uart1_rx_processed++;
            }
            
            /* 输出完整接收缓冲区内容（十六进制格式） */
            {
                char hex_str[512] = {0};
                char *p = hex_str;
                uint16_t max_display = 128;  /* 最多显示128字节 */
                uint16_t display_len = (received_count > max_display) ? max_display : received_count;
                
                for (uint16_t i = 0; i < display_len; i++)
                {
                    p += sprintf(p, "%02X ", uart1_rx_buffer[i]);
                    if (p - hex_str > 500) break;
                }
                
                if (received_count > max_display)
                {
                    LOG_INFO("MAIN", "完整内容(HEX)[%d字节，显示前%d字节]: %s...", received_count, max_display, hex_str);
                }
                else
                {
                    LOG_INFO("MAIN", "完整内容(HEX)[%d字节]: %s", received_count, hex_str);
                }
            }
            
            /* 输出完整接收缓冲区内容（ASCII原文格式） */
            {
                char ascii_str[256] = {0};
                char *p = ascii_str;
                uint16_t max_display = 128;  /* 最多显示128字节 */
                uint16_t display_len = (received_count > max_display) ? max_display : received_count;
                
                for (uint16_t i = 0; i < display_len; i++)
                {
                    uint8_t ch = uart1_rx_buffer[i];
                    if (ch >= 32 && ch < 127)  /* 可打印ASCII字符 */
                    {
                        *p++ = (char)ch;
                    }
                    else  /* 不可打印字符显示为点 */
                    {
                        *p++ = '.';
                    }
                    if (p - ascii_str > 250) break;
                }
                *p = '\0';
                
                if (received_count > max_display)
                {
                    LOG_INFO("MAIN", "完整内容(ASCII)[%d字节，显示前%d字节]: %s...", received_count, max_display, ascii_str);
                }
                else
                {
                    LOG_INFO("MAIN", "完整内容(ASCII)[%d字节]: %s", received_count, ascii_str);
                }
            }
            
            /* 重新启动DMA，清空缓冲区并重置状态 */
            UART1_DMA_Restart();
            
            LOG_INFO("MAIN", "已接收%d字节，处理并重新启动DMA", received_count);
            
            /* 关键：重新启动DMA后，跳过本次循环的后续处理，等待下次循环重新计算received_count */
            continue;
        }
        
        /* 循环模式下，DMA会自动回绕，不需要重新启动 */
        /* 当缓冲区满时（remaining == 0），DMA会自动回绕到缓冲区开头继续接收 */
        
        Delay_ms(10);
    }
}

/* ==================== 函数实现 ==================== */

/**
 * @brief 初始化UART1 DMA接收
 * @note 按照硬件时序要求：先启动DMA，再启用UART DMA请求，确保DMA能响应
 */
static error_code_t UART1_DMA_Init(void)
{
    DMA_Status_t dma_status;
    USART_TypeDef *uart_periph;
    DMA_Channel_TypeDef *dma_ch;
    
    /* 清空接收缓冲区 */
    memset(uart1_rx_buffer, 0, sizeof(uart1_rx_buffer));
    uart1_rx_processed = 0;  /* 重置已处理索引 */
    
    uart_periph = UART_GetPeriph(UART_INSTANCE_1);
    if (uart_periph == NULL)
    {
        return ERROR_BASE_UART - 1;
    }
    
    /* ========== 关键步骤1：禁用UART的RXNE中断，确保DMA能正常工作 ========== */
    USART_ITConfig(uart_periph, USART_IT_RXNE, DISABLE);
    LOG_INFO("MAIN", "已禁用UART RXNE中断，确保DMA工作");
    
    /* ========== 关键步骤2：临时禁用UART DMA请求，准备重新配置 ========== */
    USART_DMACmd(uart_periph, USART_DMAReq_Rx, DISABLE);
    
    /* ========== 关键步骤3：清除UART DR中的任何残留数据 ========== */
    /* 必须在DMA启动前清除，否则DMA会错过请求信号 */
    while (uart_periph->SR & USART_FLAG_RXNE)
    {
        volatile uint16_t dummy = uart_periph->DR;
        (void)dummy;
    }
    LOG_INFO("MAIN", "已清除UART DR中的残留数据");
    
    /* ========== 关键步骤4：初始化DMA通道（DMA1_CH5用于UART1 RX，STM32F103固定映射） ========== */
    dma_status = DMA_HW_Init(DMA_CHANNEL_1_5);
    if (dma_status != DMA_OK)
    {
        return ERROR_BASE_DMA - 1;
    }
    
    /* 获取DMA通道指针（在配置之前获取） */
    dma_ch = DMA_GetChannel(DMA_CHANNEL_1_5);
    if (dma_ch == NULL)
    {
        LOG_ERROR("MAIN", "获取DMA通道指针失败");
        return ERROR_BASE_DMA - 1;
    }
    
    /* ========== 关键步骤5：配置DMA传输（外设到内存，循环模式） ========== */
    dma_status = DMA_ConfigTransfer(DMA_CHANNEL_1_5, (uint32_t)&uart_periph->DR,
                                    (uint32_t)uart1_rx_buffer, UART1_RX_BUFFER_SIZE,
                                    DMA_DIR_PERIPHERAL_TO_MEMORY, 1);
    if (dma_status != DMA_OK)
    {
        LOG_ERROR("MAIN", "DMA配置失败: %d", dma_status);
        return ERROR_BASE_DMA - 2;
    }
    
    /* ========== 关键步骤6：清除DMA错误标志（IFCR寄存器） ========== */
    /* 清除可能的错误标志，确保DMA处于干净状态 */
    DMA1->IFCR = DMA_IFCR_CGIF5 | DMA_IFCR_CTCIF5 | DMA_IFCR_CHTIF5 | DMA_IFCR_CTEIF5;
    
    /* ========== 关键步骤7：先启动DMA（EN=1），再启用UART DMA请求 ========== */
    /* 按照硬件时序要求：先启动DMA，确保DMA准备好响应请求 */
    dma_status = DMA_Start(DMA_CHANNEL_1_5);
    if (dma_status != DMA_OK)
    {
        LOG_ERROR("MAIN", "DMA启动失败: %d", dma_status);
        return ERROR_BASE_DMA - 3;
    }
    
    /* 等待DMA完全启动（确保EN位稳定） */
    {
        uint32_t wait_start = Delay_GetTick();
        while ((dma_ch->CCR & 0x0001) == 0)
        {
            if (Delay_GetElapsed(Delay_GetTick(), wait_start) > 10)
            {
                LOG_WARN("MAIN", "等待DMA启动超时，CCR=0x%04X", dma_ch->CCR);
                break;
            }
        }
        
        /* 验证DMA是否真的启动了 */
        if ((dma_ch->CCR & 0x0001) == 0)
        {
            LOG_ERROR("MAIN", "DMA启动失败！CCR=0x%04X", dma_ch->CCR);
            return ERROR_BASE_DMA - 4;
        }
    }
    
    /* ========== 关键步骤8：再次清除UART DR，确保DMA启动时DR为空 ========== */
    while (uart_periph->SR & USART_FLAG_RXNE)
    {
        volatile uint16_t dummy = uart_periph->DR;
        (void)dummy;
    }
    
    /* ========== 关键步骤9：启用UART DMA接收请求 ========== */
    /* 此时DMA已启动，UART DR为空，DMA能正确响应后续的RXNE信号 */
    /* 注意：必须在DMA启动后，再启用UART DMA请求，确保时序正确 */
    
    /* 关键：在启用UART DMA请求之前，确保UART DR绝对为空 */
    while (uart_periph->SR & USART_FLAG_RXNE)
    {
        volatile uint16_t dummy = uart_periph->DR;
        (void)dummy;
    }
    
    /* 启用UART DMA接收请求 */
    USART_DMACmd(uart_periph, USART_DMAReq_Rx, ENABLE);
    
    /* 关键：立即检查UART CR3寄存器，确认DMA请求已启用 */
    if ((uart_periph->CR3 & USART_DMAReq_Rx) == 0)
    {
        LOG_ERROR("MAIN", "UART DMA请求启用失败！CR3=0x%04X", uart_periph->CR3);
        return ERROR_BASE_UART - 2;
    }
    
    /* 关键：立即检查DMA CCR寄存器，确认DMA已启用 */
    if ((dma_ch->CCR & 0x0001) == 0)
    {
        LOG_ERROR("MAIN", "DMA启用失败！CCR=0x%04X", dma_ch->CCR);
        return ERROR_BASE_DMA - 6;
    }
    
    /* 等待一小段时间，确保DMA请求信号稳定 */
    /* 注意：不要延时太久，否则可能错过第一个数据 */
    Delay_ms(1);
    
    /* 再次清除UART DR（防止在启用DMA请求期间有数据到达） */
    while (uart_periph->SR & USART_FLAG_RXNE)
    {
        volatile uint16_t dummy = uart_periph->DR;
        (void)dummy;
    }
    
    LOG_INFO("MAIN", "已启用UART DMA接收请求，DMA已启动");
    
    /* ========== 关键步骤10：启用DMA传输完成中断（可选，主要用于调试） ========== */
    dma_status = DMA_EnableIT(DMA_CHANNEL_1_5, DMA_IT_TYPE_TC);
    if (dma_status != DMA_OK)
    {
        LOG_ERROR("MAIN", "启用DMA中断失败: %d", dma_status);
        return ERROR_BASE_DMA - 4;
    }
    
    /* 验证中断是否启用（如果未启用则手动启用） */
    dma_ch = DMA_GetChannel(DMA_CHANNEL_1_5);
    if (dma_ch != NULL && (dma_ch->CCR & 0x0002) == 0)
    {
        dma_ch->CCR |= 0x0002;  /* 直接设置TCIE位 */
    }
    
    /* 调试：验证DMA和UART配置 */
    {
        DMA_Channel_TypeDef *dma_ch = DMA_GetChannel(DMA_CHANNEL_1_5);
        USART_TypeDef *uart_periph = UART_GetPeriph(UART_INSTANCE_1);
        
        if (dma_ch != NULL)
        {
            LOG_INFO("MAIN", "DMA配置: CCR=0x%04X, CNDTR=%d", dma_ch->CCR, dma_ch->CNDTR);
            LOG_INFO("MAIN", "DMA地址: CPAR=0x%08X, CMAR=0x%08X", dma_ch->CPAR, dma_ch->CMAR);
            
            if ((dma_ch->CCR & 0x0001) == 0)
            {
                LOG_ERROR("MAIN", "DMA未启动！CCR.EN=0");
                return ERROR_BASE_DMA - 4;
            }
        }
        
        if (uart_periph != NULL)
        {
            LOG_INFO("MAIN", "UART配置: CR3=0x%04X (DMA接收请求: %s), CR1=0x%04X (RXNE中断: %s)", 
                    uart_periph->CR3, 
                    (uart_periph->CR3 & USART_CR3_DMAR) ? "启用" : "禁用",
                    uart_periph->CR1,
                    (uart_periph->CR1 & USART_CR1_RXNEIE) ? "启用" : "禁用");
        }
    }
    
    LOG_INFO("MAIN", "DMA已启动，等待数据...");
    
    return ERROR_OK;
}


/**
 * @brief 重新启动DMA接收
 */
/**
 * @brief 重新启动DMA接收（按照硬件时序要求）
 * @note 循环模式下通常不需要重启，此函数用于异常恢复
 */
static void UART1_DMA_Restart(void)
{
    DMA_Status_t dma_status;
    USART_TypeDef *uart_periph;
    DMA_Channel_TypeDef *dma_ch;
    
    uart_periph = UART_GetPeriph(UART_INSTANCE_1);
    dma_ch = DMA_GetChannel(DMA_CHANNEL_1_5);
    
    if (uart_periph == NULL || dma_ch == NULL)
    {
        LOG_ERROR("MAIN", "UART或DMA外设指针为空");
        return;
    }
    
    /* ========== 关键步骤1：先禁用UART DMA接收请求 ========== */
    USART_DMACmd(uart_periph, USART_DMAReq_Rx, DISABLE);
    
    /* ========== 关键步骤2：停止DMA传输 ========== */
    DMA_Cmd(dma_ch, DISABLE);
    
    /* 等待DMA完全停止 */
    uint32_t wait_start = Delay_GetTick();
    while ((dma_ch->CCR & 0x0001) != 0)
    {
        if (Delay_GetElapsed(Delay_GetTick(), wait_start) > 10)
        {
            break;
        }
    }
    
    /* ========== 关键步骤3：清除UART DR中的任何残留数据 ========== */
    while (uart_periph->SR & USART_FLAG_RXNE)
    {
        volatile uint16_t dummy = uart_periph->DR;
        (void)dummy;
    }
    
    /* 清空缓冲区 */
    memset(uart1_rx_buffer, 0, sizeof(uart1_rx_buffer));
    uart1_rx_processed = 0;
    
    /* 短暂延时，确保状态稳定 */
    Delay_ms(1);
    
    /* ========== 关键步骤4：重新配置DMA传输 ========== */
    dma_status = DMA_ConfigTransfer(DMA_CHANNEL_1_5, (uint32_t)&uart_periph->DR,
                                    (uint32_t)uart1_rx_buffer, UART1_RX_BUFFER_SIZE,
                                    DMA_DIR_PERIPHERAL_TO_MEMORY, 1);
    if (dma_status != DMA_OK)
    {
        LOG_ERROR("MAIN", "重新配置DMA失败: %d", dma_status);
        return;
    }
    
    /* ========== 关键步骤5：先启动DMA，再启用UART DMA请求 ========== */
    dma_status = DMA_Start(DMA_CHANNEL_1_5);
    if (dma_status != DMA_OK)
    {
        LOG_ERROR("MAIN", "重新启动DMA失败: %d", dma_status);
        return;
    }
    
    /* 等待DMA完全启动 */
    Delay_ms(1);
    
    /* ========== 关键步骤6：再次清除UART DR ========== */
    while (uart_periph->SR & USART_FLAG_RXNE)
    {
        volatile uint16_t dummy = uart_periph->DR;
        (void)dummy;
    }
    
    /* ========== 关键步骤7：启用UART DMA接收请求 ========== */
    USART_DMACmd(uart_periph, USART_DMAReq_Rx, ENABLE);
    
    /* ========== 关键步骤8：重新启用DMA中断 ========== */
    dma_status = DMA_EnableIT(DMA_CHANNEL_1_5, DMA_IT_TYPE_TC);
    if (dma_status != DMA_OK)
    {
        LOG_ERROR("MAIN", "重新启用DMA中断失败: %d", dma_status);
    }
    
    /* 确保中断使能 */
    if ((dma_ch->CCR & 0x0002) == 0)
    {
        dma_ch->CCR |= 0x0002;  /* 直接设置TCIE位 */
    }
    
    LOG_INFO("MAIN", "DMA已重新启动");
}
