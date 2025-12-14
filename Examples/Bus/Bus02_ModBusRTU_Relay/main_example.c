/**
 * @file main_example.c
 * @brief Bus02 - ModBusRTU继电器控制示例
 * @details 通过UART2（RS485）使用ModBusRTU协议控制继电器模块
 * @version 1.0.0
 * @date 2024-01-01
 *
 * 功能说明：
 * - 自动检测继电器波特率（9600、38400、115200）
 * - 测试继电器1-8的开启和关闭（间隔500ms）
 * - 批量控制测试：使用批量控制寄存器一次性开启所有通道，然后按顺序关闭
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
 * - UART2：PA2(TX), PA3(RX)，动态波特率（用于ModBusRTU通信，连接RS485）
 * - RS485模块：连接UART2，自动方向控制
 * - OLED：PB8(SCL), PB9(SDA)，软件I2C
 * - 继电器模块：地址1，支持ModBusRTU协议
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "error_code.h"
#include "delay.h"
#include "modbus_rtu.h"
#include "oled_ssd1306.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ==================== 继电器配置 ==================== */

#define RELAY_SLAVE_ADDRESS     1       /**< 继电器模块默认地址 */
#define RELAY_CHANNEL_COUNT     8       /**< 继电器通道数量（1-8） */

/* 继电器寄存器地址定义 */
#define RELAY_REG_CHANNEL_BASE      0x0000  /**< 通道输出控制寄存器基地址（通道1=0x0000，通道2=0x0001...） */
#define RELAY_REG_COMM_CHECK        0x0030  /**< 通讯检测时间设置寄存器 */
#define RELAY_REG_SLAVE_ADDR        0x0032  /**< RS485地址寄存器 */
#define RELAY_REG_BAUDRATE          0x0033  /**< 波特率寄存器 */
#define RELAY_REG_BATCH_ALL         0x0034  /**< 批量控制寄存器（0=全关，1=全开） */
#define RELAY_REG_BATCH_CH1_16      0x0035  /**< 按位控制通道1~16寄存器（最低位=通道1，最高位=通道16） */
#define RELAY_REG_BATCH_CH17_32     0x0036  /**< 按位控制通道17~32寄存器 */
#define RELAY_REG_BATCH_CH33_48     0x0037  /**< 按位控制通道33~48寄存器 */
#define RELAY_REG_MODE_CH1          0x0096  /**< 通道1控制模式设置寄存器 */

/* 波特率定义（继电器模块支持） */
#define BAUDRATE_4800           0       /**< 4800bps */
#define BAUDRATE_9600           1       /**< 9600bps */
#define BAUDRATE_14400          2       /**< 14400bps */
#define BAUDRATE_19200          3       /**< 19200bps */
#define BAUDRATE_38400          4       /**< 38400bps（默认） */
#define BAUDRATE_56000          5       /**< 56000bps */
#define BAUDRATE_57600          6       /**< 57600bps */
#define BAUDRATE_115200         7       /**< 115200bps */

/* 波特率测试列表 */
static const uint32_t g_baudrate_list[] = {9600, 38400, 115200};
static const char* g_baudrate_names[] = {"9600", "38400", "115200"};
#define BAUDRATE_TEST_COUNT     (sizeof(g_baudrate_list) / sizeof(g_baudrate_list[0]))

/* ==================== 全局变量 ==================== */

static uint8_t g_relay_address = RELAY_SLAVE_ADDRESS;  /**< 继电器模块地址 */

/* 批量控制测试用的静态变量 */
static uint16_t g_batch_value = 0x00FF;  /**< 批量控制值 */

/* ==================== 前向声明 ==================== */

static void DisplayStatusOnOLED(uint32_t baudrate, uint8_t channel, uint8_t state);
static void TestModBusRTUCommunicationTime(void);
static uint8_t GetBaudrateRegisterValue(uint32_t baudrate);
static uint32_t GetNextBaudrate(uint32_t current_baudrate);
static void SwitchRelayBaudrate(uint32_t current_baudrate);

/* ==================== 私有函数 ==================== */

/**
 * @brief 设置UART2波特率
 * @param[in] baudrate 目标波特率
 * @return UART_Status_t 错误码
 */
static UART_Status_t SetUART2BaudRate(uint32_t baudrate)
{
    USART_TypeDef *uart2_periph;
    UART_Status_t status;
    
    uart2_periph = UART_GetPeriph(UART_INSTANCE_2);
    if (uart2_periph == NULL) {
        return UART_ERROR_INVALID_INSTANCE;
    }
    
    status = UART_SetBaudRate(uart2_periph, baudrate);
    if (status != UART_OK) {
        LOG_ERROR("RELAY", "设置UART2波特率失败: %d", status);
        return status;
    }
    
    /* 等待波特率切换稳定 */
    Delay_ms(50);
    
    return UART_OK;
}

/**
 * @brief 检测继电器波特率
 * @return uint32_t 检测到的波特率，0表示未检测到
 */
static uint32_t DetectRelayBaudRate(void)
{
    uint32_t baudrate;
    uint16_t register_value;
    ModBusRTU_Status_t status;
    UART_Status_t uart_status;
    uint8_t i;
    const char* error_msg;
    USART_TypeDef *uart2_periph;
    
    LOG_INFO("RELAY", "开始检测继电器波特率...");
    LOG_INFO("RELAY", "继电器地址: %d", g_relay_address);
    LOG_INFO("RELAY", "寄存器地址: 0x%04X (RS485地址寄存器)", RELAY_REG_SLAVE_ADDR);
    LOG_INFO("RELAY", "请确保：1) RS485模块已连接 2) 继电器模块已上电 3) 总线终端电阻已配置");
    
    /* 获取UART2外设指针 */
    uart2_periph = UART_GetPeriph(UART_INSTANCE_2);
    
    /* 依次尝试每个波特率 */
    for (i = 0; i < BAUDRATE_TEST_COUNT; i++) {
        baudrate = g_baudrate_list[i];
        
        LOG_INFO("RELAY", "========== 尝试波特率: %s ==========", g_baudrate_names[i]);
        
        /* 设置UART2波特率 */
        uart_status = SetUART2BaudRate(baudrate);
        if (uart_status != UART_OK) {
            LOG_WARN("RELAY", "设置UART2波特率失败: %d", uart_status);
            continue;
        }
        
        /* 等待波特率切换稳定 */
        Delay_ms(200);
        
        /* 清空UART接收缓冲区 */
        if (uart2_periph != NULL) {
            /* 读取并丢弃所有接收到的数据 */
            while (USART_GetFlagStatus(uart2_periph, USART_FLAG_RXNE) == SET) {
                volatile uint8_t dummy = USART_ReceiveData(uart2_periph);
                (void)dummy;
            }
        }
        
        /* 使用ModBusRTU函数读取测试 */
        status = ModBusRTU_ReadHoldingRegisters(UART_INSTANCE_2, g_relay_address,
                                                RELAY_REG_SLAVE_ADDR, 1, &register_value, 1000);
        
        if (status == ModBusRTU_OK) {
            LOG_INFO("RELAY", "检测成功！波特率: %s, 继电器地址: %d", 
                    g_baudrate_names[i], register_value);
            return baudrate;
        } else {
            /* 输出详细的错误信息 */
            switch (status) {
                case ModBusRTU_ERROR_TIMEOUT:
                    error_msg = "超时（无响应）";
                    break;
                case ModBusRTU_ERROR_CRC:
                    error_msg = "CRC校验错误";
                    break;
                case ModBusRTU_ERROR_INVALID_RESPONSE:
                    error_msg = "无效响应";
                    break;
                case ModBusRTU_ERROR_INVALID_ADDRESS:
                    error_msg = "无效地址";
                    break;
                case ModBusRTU_ERROR_EXCEPTION:
                    error_msg = "异常响应";
                    break;
                default:
                    error_msg = "未知错误";
                    break;
            }
            LOG_WARN("RELAY", "波特率 %s 测试失败: %d (%s)", 
                    g_baudrate_names[i], status, error_msg);
        }
        
        /* 等待一段时间再尝试下一个波特率 */
        Delay_ms(200);
    }
    
    LOG_ERROR("RELAY", "未检测到有效的波特率！");
    LOG_ERROR("RELAY", "请检查：");
    LOG_ERROR("RELAY", "  1. RS485模块是否正确连接到UART2（PA2/PA3）");
    LOG_ERROR("RELAY", "  2. 继电器模块是否已上电");
    LOG_ERROR("RELAY", "  3. RS485总线A+/B-是否正确连接");
    LOG_ERROR("RELAY", "  4. 总线终端电阻是否配置（建议120Ω）");
    LOG_ERROR("RELAY", "  5. 继电器模块地址是否为 %d（如果不是，请修改代码中的RELAY_SLAVE_ADDRESS）", g_relay_address);
    return 0;
}

/**
 * @brief 控制单个继电器通道
 * @param[in] channel 通道号（1-8）
 * @param[in] state 状态（1=开启，0=关闭）
 * @param[in] detected_baudrate 当前波特率（用于OLED显示）
 * @return ModBusRTU_Status_t 错误码
 */
static ModBusRTU_Status_t ControlRelayChannel(uint8_t channel, uint8_t state, uint32_t detected_baudrate)
{
    uint16_t register_address;
    uint16_t register_value;
    ModBusRTU_Status_t status;
    
    /* 参数校验 */
    if (channel < 1 || channel > RELAY_CHANNEL_COUNT) {
        return ModBusRTU_ERROR_INVALID_PARAM;
    }
    
    /* 计算寄存器地址（通道1=0x0000，通道2=0x0001...） */
    register_address = RELAY_REG_CHANNEL_BASE + (channel - 1);
    
    /* 设置寄存器值（1=开启，0=关闭） */
    register_value = state ? 1 : 0;
    
    /* 更新OLED显示 */
    DisplayStatusOnOLED(detected_baudrate, channel, state);
    
    /* 使用功能码06写单个寄存器 */
    status = ModBusRTU_WriteSingleRegister(UART_INSTANCE_2, g_relay_address,
                                          register_address, register_value, 500);
    
    if (status == ModBusRTU_OK) {
        LOG_INFO("RELAY", "通道%d %s成功", channel, state ? "开启" : "关闭");
    } else {
        LOG_ERROR("RELAY", "通道%d %s失败: %d", channel, state ? "开启" : "关闭", status);
        ErrorHandler_Handle(status, "ModBusRTU");
    }
    
    return status;
}

/**
 * @brief 测试继电器开关
 * @details 依次开启继电器1-8，然后依次关闭，每个操作间隔500ms
 * @param[in] detected_baudrate 当前波特率（用于OLED显示）
 */
static void TestRelaySwitching(uint32_t detected_baudrate)
{
    uint8_t i;
    ModBusRTU_Status_t status;
    
    LOG_INFO("RELAY", "=== 开始测试继电器开关 ===");
    
    /* 依次开启继电器1-8 */
    LOG_INFO("RELAY", "开启继电器1-8...");
    for (i = 1; i <= RELAY_CHANNEL_COUNT; i++) {
        status = ControlRelayChannel(i, 1, detected_baudrate);
        if (status != ModBusRTU_OK) {
            LOG_WARN("RELAY", "通道%d开启失败，继续测试", i);
        }
        
        /* 间隔500ms */
        if (i < RELAY_CHANNEL_COUNT) {
            Delay_ms(500);
        }
    }
    
    /* 等待500ms */
    Delay_ms(500);
    
    /* 依次关闭继电器1-8 */
    LOG_INFO("RELAY", "关闭继电器1-8...");
    for (i = 1; i <= RELAY_CHANNEL_COUNT; i++) {
        status = ControlRelayChannel(i, 0, detected_baudrate);
        if (status != ModBusRTU_OK) {
            LOG_WARN("RELAY", "通道%d关闭失败，继续测试", i);
        }
        
        /* 间隔500ms */
        if (i < RELAY_CHANNEL_COUNT) {
            Delay_ms(500);
        }
    }
    
    LOG_INFO("RELAY", "=== 继电器开关测试完成 ===");
    /* 显示就绪状态 */
    DisplayStatusOnOLED(detected_baudrate, 0, 0);
}

/**
 * @brief 测试ModBusRTU通信时间
 * @details 测量9600波特率下，发送一个WriteSingleRegister命令到接收返回数据的完整时间
 */
static void TestModBusRTUCommunicationTime(void)
{
    uint8_t i;
    ModBusRTU_Status_t status;
    uint32_t start_tick;
    uint32_t end_tick;
    uint32_t elapsed_ms;
    uint32_t total_time = 0;
    uint32_t min_time = 0xFFFFFFFF;
    uint32_t max_time = 0;
    uint8_t success_count = 0;  /* 成功次数 */
    const uint8_t test_count = 10;  /* 测试10次取平均值 */
    
    LOG_INFO("RELAY", "=== 开始测试ModBusRTU通信时间 ===");
    LOG_INFO("RELAY", "测试条件：波特率9600，WriteSingleRegister命令");
    LOG_INFO("RELAY", "测试次数：%d次", test_count);
    
    /* 确保所有通道都关闭 */
    ModBusRTU_WriteSingleRegister(UART_INSTANCE_2, g_relay_address,
                                  RELAY_REG_BATCH_CH1_16, 0x0000, 500);
    Delay_ms(100);
    
    for (i = 0; i < test_count; i++) {
        /* 交替测试开启和关闭，更真实地模拟批量控制场景 */
        uint16_t test_value = (i % 2 == 0) ? 0x0001 : 0x0000;  /* 偶数次开启，奇数次关闭 */
        const char* operation = (i % 2 == 0) ? "开启" : "关闭";
        
        /* 记录开始时间 */
        start_tick = Delay_GetTick();
        
        /* 发送WriteSingleRegister命令（交替开启/关闭通道1） */
        status = ModBusRTU_WriteSingleRegister(UART_INSTANCE_2, g_relay_address,
                                              RELAY_REG_BATCH_CH1_16, test_value, 500);
        
        /* 记录结束时间 */
        end_tick = Delay_GetTick();
        
        /* 计算耗时 */
        elapsed_ms = Delay_GetElapsed(end_tick, start_tick);
        
        if (status == ModBusRTU_OK) {
            total_time += elapsed_ms;
            success_count++;
            if (elapsed_ms < min_time) {
                min_time = elapsed_ms;
            }
            if (elapsed_ms > max_time) {
                max_time = elapsed_ms;
            }
            LOG_INFO("RELAY", "第%d次测试 (%s): %lu ms (状态: OK)", i + 1, operation, elapsed_ms);
        } else {
            LOG_WARN("RELAY", "第%d次测试 (%s): %lu ms (状态: 失败 %d)", i + 1, operation, elapsed_ms, status);
        }
        
        /* 等待一小段时间，避免连续发送造成干扰 */
        Delay_ms(50);
    }
    
    /* 计算平均值（只计算成功的测试） */
    uint32_t avg_time = 0;
    if (success_count > 0) {
        avg_time = total_time / success_count;
    }
    
    LOG_INFO("RELAY", "=== 测试结果 ===");
    LOG_INFO("RELAY", "成功次数: %d/%d", success_count, test_count);
    if (success_count > 0) {
        LOG_INFO("RELAY", "平均时间: %lu ms", avg_time);
        LOG_INFO("RELAY", "最短时间: %lu ms", min_time);
        LOG_INFO("RELAY", "最长时间: %lu ms", max_time);
        LOG_INFO("RELAY", "建议定时器间隔: %lu ms (平均时间的1.5倍)", avg_time * 3 / 2);
        LOG_INFO("RELAY", "建议最小间隔: %lu ms (最长时间的1.2倍)", max_time * 6 / 5);
    } else {
        LOG_ERROR("RELAY", "所有测试均失败，无法计算通信时间");
    }
    LOG_INFO("RELAY", "=== 测试完成 ===");
}

/**
 * @brief 获取波特率对应的寄存器值
 * @param[in] baudrate 波特率值（如9600、38400、115200）
 * @return uint8_t 寄存器值（0-7），如果波特率不支持则返回0xFF
 */
static uint8_t GetBaudrateRegisterValue(uint32_t baudrate)
{
    switch (baudrate) {
        case 4800:   return BAUDRATE_4800;
        case 9600:   return BAUDRATE_9600;
        case 14400:  return BAUDRATE_14400;
        case 19200:  return BAUDRATE_19200;
        case 38400:  return BAUDRATE_38400;
        case 56000:  return BAUDRATE_56000;
        case 57600:  return BAUDRATE_57600;
        case 115200: return BAUDRATE_115200;
        default:     return 0xFF;  /* 不支持的波特率 */
    }
}

/**
 * @brief 获取下一个波特率（循环切换：9600->38400->115200->9600）
 * @param[in] current_baudrate 当前波特率
 * @return uint32_t 下一个波特率，如果当前波特率不支持则返回0
 */
static uint32_t GetNextBaudrate(uint32_t current_baudrate)
{
    switch (current_baudrate) {
        case 9600:   return 38400;
        case 38400:  return 115200;
        case 115200: return 9600;
        default:     return 0;  /* 不支持的波特率 */
    }
}

/**
 * @brief 切换继电器波特率
 * @param[in] current_baudrate 当前波特率
 */
static void SwitchRelayBaudrate(uint32_t current_baudrate)
{
    uint32_t next_baudrate;
    uint8_t next_register_value;
    ModBusRTU_Status_t status;
    const char* current_name = "未知";
    const char* next_name = "未知";
    
    /* 获取当前波特率名称 */
    if (current_baudrate == 9600) {
        current_name = "9600";
    } else if (current_baudrate == 38400) {
        current_name = "38400";
    } else if (current_baudrate == 115200) {
        current_name = "115200";
    }
    
    /* 获取下一个波特率 */
    next_baudrate = GetNextBaudrate(current_baudrate);
    if (next_baudrate == 0) {
        LOG_ERROR("RELAY", "当前波特率 %lu 不支持切换", current_baudrate);
        return;
    }
    
    /* 获取下一个波特率的寄存器值 */
    next_register_value = GetBaudrateRegisterValue(next_baudrate);
    if (next_register_value == 0xFF) {
        LOG_ERROR("RELAY", "目标波特率 %lu 不支持", next_baudrate);
        return;
    }
    
    /* 获取目标波特率名称 */
    if (next_baudrate == 9600) {
        next_name = "9600";
    } else if (next_baudrate == 38400) {
        next_name = "38400";
    } else if (next_baudrate == 115200) {
        next_name = "115200";
    }
    
    LOG_INFO("RELAY", "=== 切换继电器波特率 ===");
    LOG_INFO("RELAY", "当前波特率: %s (%lu bps)", current_name, current_baudrate);
    LOG_INFO("RELAY", "目标波特率: %s (%lu bps)", next_name, next_baudrate);
    LOG_INFO("RELAY", "寄存器值: %d", next_register_value);
    
    /* 写入新的波特率值到寄存器0x0033H */
    status = ModBusRTU_WriteSingleRegister(UART_INSTANCE_2, g_relay_address,
                                           RELAY_REG_BAUDRATE, next_register_value, 1000);
    
    if (status == ModBusRTU_OK) {
        LOG_INFO("RELAY", "波特率设置成功！");
        LOG_INFO("RELAY", "");
        LOG_INFO("RELAY", "========================================");
        LOG_INFO("RELAY", "已切换波特率，请重启继电器与单片机");
        LOG_INFO("RELAY", "当前: %s -> 目标: %s", current_name, next_name);
        LOG_INFO("RELAY", "========================================");
        LOG_INFO("RELAY", "");
    } else {
        LOG_ERROR("RELAY", "波特率设置失败: %d", status);
        ErrorHandler_Handle(status, "ModBusRTU");
    }
}

/**
 * @brief 测试继电器批量控制
 * @details 使用批量控制寄存器（0x0035H）按位控制一次性开启所有通道1-8，然后按顺序关闭（通道1:100ms后，通道2:200ms后...通道8:800ms后）
 * @param[in] detected_baudrate 当前波特率（用于OLED显示）
 */
static void TestRelayBatchControl(uint32_t detected_baudrate)
{
    uint8_t i;
    ModBusRTU_Status_t status;
    
    LOG_INFO("RELAY", "=== 开始批量控制测试 ===");
    
    /* 使用0x0035H寄存器按位控制通道1~16，对于8路模块，设置低8位为1（开启通道1-8） */
    /* 位0=通道1，位1=通道2，...，位7=通道8 */
    g_batch_value = 0x00FF;  /* 低8位全为1，表示开启通道1-8 */
    
    /* 使用功能码06写单个寄存器，一次性开启所有通道1-8 */
    status = ModBusRTU_WriteSingleRegister(UART_INSTANCE_2, g_relay_address,
                                          RELAY_REG_BATCH_CH1_16, g_batch_value, 500);
    
    if (status != ModBusRTU_OK) {
        LOG_ERROR("RELAY", "批量开启失败: %d", status);
        ErrorHandler_Handle(status, "ModBusRTU");
        return;
    }
    
    /* 更新OLED显示（显示所有通道开启） */
    DisplayStatusOnOLED(detected_baudrate, 99, 1);  /* 使用99表示所有通道 */
    
    /* 全开2秒后开始关闭 */
    Delay_ms(2000);
    
    /* 记录关闭阶段的开始时间 */
    uint32_t start_tick = Delay_GetTick();
    
    /* 关闭通道（仅等待+通信，不更新OLED，避免OLED刷新延迟影响时间精度） */
    for (i = 0; i < RELAY_CHANNEL_COUNT; i++) {
        /* 精确等待到指定时间点（通道1=100ms，通道2=200ms，...，通道8=800ms） */
        while (Delay_GetElapsed(Delay_GetTick(), start_tick) < (i + 1) * 100) {
            /* 等待到目标时间 */
        }
        
        /* 关闭当前通道：清除对应的位 */
        g_batch_value &= ~(1 << i);
        
        /* 使用0x0035H寄存器按位控制，更新当前通道状态 */
        /* 关闭阶段不检查返回值，避免超时影响时间精度 */
        /* 超时时间缩短到20ms，减少阻塞时间 */
        ModBusRTU_WriteSingleRegister(UART_INSTANCE_2, g_relay_address,
                                      RELAY_REG_BATCH_CH1_16, g_batch_value, 20);
    }
    
    /* 统一更新OLED（所有通道关闭后一次性刷新，避免刷新延迟影响时间精度） */
    DisplayStatusOnOLED(detected_baudrate, 0, 0);
    
    /* 全部关闭后，检查所有通道的状态（验证所有通道是否已关闭） */
    Delay_ms(100);  /* 等待100ms，确保所有命令执行完成 */
    
    /* 读取0x0035H寄存器，检查所有通道状态 */
    uint16_t read_value = 0;
    status = ModBusRTU_ReadHoldingRegisters(UART_INSTANCE_2, g_relay_address,
                                           RELAY_REG_BATCH_CH1_16, 1, &read_value, 500);
    
    if (status == ModBusRTU_OK) {
        if (read_value == 0) {
            LOG_INFO("RELAY", "所有通道已成功关闭（寄存器值: 0x%04X）", read_value);
        } else {
            LOG_WARN("RELAY", "部分通道未关闭（寄存器值: 0x%04X）", read_value);
        }
    } else {
        LOG_WARN("RELAY", "读取通道状态失败: %d", status);
    }
    
    LOG_INFO("RELAY", "=== 批量控制测试完成 ===");
    /* 显示就绪状态 */
    DisplayStatusOnOLED(detected_baudrate, 0, 0);
}

/**
 * @brief 在OLED上显示状态信息
 * @param[in] baudrate 当前波特率（0表示未检测到）
 * @param[in] channel 当前操作的通道（0表示无操作，99表示所有通道）
 * @param[in] state 当前通道状态（0=关闭，1=开启）
 */
static void DisplayStatusOnOLED(uint32_t baudrate, uint8_t channel, uint8_t state)
{
    char buffer[32];
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Bus02 ModBusRTU");
    
    if (baudrate == 0) {
        OLED_ShowString(2, 1, "Detecting...");
        OLED_ShowString(3, 1, "Baudrate");
    } else {
        snprintf(buffer, sizeof(buffer), "Baud: %lu", baudrate);
        OLED_ShowString(2, 1, buffer);
        
        if (channel == 99) {
            /* 显示所有通道开启 */
            OLED_ShowString(3, 1, "All: ON ");
        } else if (channel > 0 && channel <= RELAY_CHANNEL_COUNT) {
            snprintf(buffer, sizeof(buffer), "Ch%d: %s", channel, state ? "ON " : "OFF");
            OLED_ShowString(3, 1, buffer);
        } else {
            OLED_ShowString(3, 1, "Ready");
        }
    }
    
    snprintf(buffer, sizeof(buffer), "Addr: %d", g_relay_address);
    OLED_ShowString(4, 1, buffer);
}

/**
 * @brief 测试设置继电器站点地址
 * @param[in] new_address 新地址（1-255）
 * @return ModBusRTU_Status_t 错误码
 * @note 此函数为预留功能，当前未在主循环中调用。如需使用，可在主循环中添加调用。
 */
#pragma diag_suppress 177
static ModBusRTU_Status_t TestSetSlaveAddress(uint8_t new_address)
{
    uint16_t read_value;
    ModBusRTU_Status_t status;
    
    LOG_INFO("RELAY", "=== 测试设置站点地址 ===");
    
    /* 参数校验 */
    if (new_address < 1 || new_address > 255) {
        LOG_ERROR("RELAY", "无效的站点地址: %d（范围1-255）", new_address);
        return ModBusRTU_ERROR_INVALID_PARAM;
    }
    
    /* 读取当前地址 */
    status = ModBusRTU_ReadHoldingRegisters(UART_INSTANCE_2, g_relay_address,
                                            RELAY_REG_SLAVE_ADDR, 1, &read_value, 500);
    if (status == ModBusRTU_OK) {
        LOG_INFO("RELAY", "当前站点地址: %d", read_value);
    } else {
        LOG_WARN("RELAY", "读取当前站点地址失败: %d", status);
    }
    
    /* 设置新地址 */
    LOG_INFO("RELAY", "设置站点地址为: %d", new_address);
    status = ModBusRTU_WriteSingleRegister(UART_INSTANCE_2, g_relay_address,
                                           RELAY_REG_SLAVE_ADDR, new_address, 500);
    if (status != ModBusRTU_OK) {
        LOG_ERROR("RELAY", "设置站点地址失败: %d", status);
        ErrorHandler_Handle(status, "ModBusRTU");
        return status;
    }
    
    /* 等待设置生效 */
    Delay_ms(100);
    
    /* 读取验证 */
    status = ModBusRTU_ReadHoldingRegisters(UART_INSTANCE_2, g_relay_address,
                                            RELAY_REG_SLAVE_ADDR, 1, &read_value, 500);
    if (status == ModBusRTU_OK) {
        if (read_value == new_address) {
            LOG_INFO("RELAY", "站点地址设置成功，当前地址: %d", read_value);
            LOG_WARN("RELAY", "注意：参数修改后需重启继电器模块才能生效！");
        } else {
            LOG_WARN("RELAY", "站点地址设置后读取值不匹配：期望%d，实际%d", new_address, read_value);
            LOG_WARN("RELAY", "可能需要重启继电器模块才能生效");
        }
    } else {
        LOG_WARN("RELAY", "读取站点地址验证失败: %d", status);
        LOG_WARN("RELAY", "可能需要使用新地址重新通信");
    }
    
    LOG_INFO("RELAY", "=== 站点地址设置测试完成 ===");
    return ModBusRTU_OK;
}

/**
 * @brief 测试设置继电器波特率
 * @param[in] baudrate_code 波特率代码（0=4800, 1=9600, 4=38400, 7=115200等）
 * @return ModBusRTU_Status_t 错误码
 * @note 此函数为预留功能，当前未在主循环中调用。如需使用，可在主循环中添加调用。
 */
#pragma diag_suppress 177
static ModBusRTU_Status_t TestSetBaudRate(uint8_t baudrate_code)
{
    uint16_t read_value;
    ModBusRTU_Status_t status;
    const char* baudrate_name = "未知";
    
    LOG_INFO("RELAY", "=== 测试设置波特率 ===");
    
    /* 参数校验 */
    if (baudrate_code > 7) {
        LOG_ERROR("RELAY", "无效的波特率代码: %d（范围0-7）", baudrate_code);
        return ModBusRTU_ERROR_INVALID_PARAM;
    }
    
    /* 获取波特率名称 */
    switch (baudrate_code) {
        case 0: baudrate_name = "4800"; break;
        case 1: baudrate_name = "9600"; break;
        case 2: baudrate_name = "14400"; break;
        case 3: baudrate_name = "19200"; break;
        case 4: baudrate_name = "38400"; break;
        case 5: baudrate_name = "56000"; break;
        case 6: baudrate_name = "57600"; break;
        case 7: baudrate_name = "115200"; break;
    }
    
    /* 读取当前波特率 */
    status = ModBusRTU_ReadHoldingRegisters(UART_INSTANCE_2, g_relay_address,
                                            RELAY_REG_BAUDRATE, 1, &read_value, 500);
    if (status == ModBusRTU_OK) {
        LOG_INFO("RELAY", "当前波特率代码: %d", read_value);
    } else {
        LOG_WARN("RELAY", "读取当前波特率失败: %d", status);
    }
    
    /* 设置新波特率 */
    LOG_INFO("RELAY", "设置波特率代码为: %d (%s)", baudrate_code, baudrate_name);
    status = ModBusRTU_WriteSingleRegister(UART_INSTANCE_2, g_relay_address,
                                           RELAY_REG_BAUDRATE, baudrate_code, 500);
    if (status != ModBusRTU_OK) {
        LOG_ERROR("RELAY", "设置波特率失败: %d", status);
        ErrorHandler_Handle(status, "ModBusRTU");
        return status;
    }
    
    /* 等待设置生效 */
    Delay_ms(100);
    
    /* 读取验证 */
    status = ModBusRTU_ReadHoldingRegisters(UART_INSTANCE_2, g_relay_address,
                                            RELAY_REG_BAUDRATE, 1, &read_value, 500);
    if (status == ModBusRTU_OK) {
        if (read_value == baudrate_code) {
            LOG_INFO("RELAY", "波特率设置成功，当前代码: %d", read_value);
            LOG_WARN("RELAY", "注意：参数修改后需重启继电器模块才能生效！");
        } else {
            LOG_WARN("RELAY", "波特率设置后读取值不匹配：期望%d，实际%d", baudrate_code, read_value);
            LOG_WARN("RELAY", "可能需要重启继电器模块才能生效");
        }
    } else {
        LOG_WARN("RELAY", "读取波特率验证失败: %d", status);
        LOG_WARN("RELAY", "可能需要使用新波特率重新通信");
    }
    
    LOG_INFO("RELAY", "=== 波特率设置测试完成 ===");
    return ModBusRTU_OK;
}

/* ==================== 主函数 ==================== */

int main(void)
{
    UART_Status_t uart_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    OLED_Status_t oled_status;
    uint32_t detected_baudrate;
    
    /* ========== 步骤1：系统初始化 ========== */
    System_Init();
    
    /* ========== 步骤2：UART初始化 ========== */
    /* 初始化UART1（用于Debug输出） */
    uart_status = UART_Init(UART_INSTANCE_1);
    if (uart_status != UART_OK) {
        while(1) { Delay_ms(1000); }
    }
    
    /* 初始化UART2（用于ModBusRTU通信，初始波特率9600） */
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
    LOG_INFO("MAIN", "=== Bus02 ModBusRTU继电器控制示例 ===");
    LOG_INFO("MAIN", "系统初始化完成");
    LOG_INFO("MAIN", "UART1已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "UART2已初始化: PA2(TX), PA3(RX), 9600（初始值）");
    LOG_INFO("MAIN", "Debug模块已初始化: UART模式");
    LOG_INFO("MAIN", "Log模块已初始化");
    LOG_INFO("MAIN", "ErrorHandler模块已就绪");
    
    /* ========== 步骤7：初始化OLED ========== */
    oled_status = OLED_Init();
    if (oled_status == OLED_OK) {
        DisplayStatusOnOLED(0, 0, 0);  /* 显示初始状态 */
        LOG_INFO("MAIN", "OLED已初始化并显示");
    } else {
        LOG_ERROR("MAIN", "OLED初始化失败: %d", oled_status);
        ErrorHandler_Handle(oled_status, "OLED");
    }
    
    Delay_ms(1000);
    
    /* ========== 步骤8：波特率自动检测 ========== */
    DisplayStatusOnOLED(0, 0, 0);  /* 显示检测中状态 */
    detected_baudrate = DetectRelayBaudRate();
    
    /* 如果第一次检测失败，等待5秒后再次检测 */
    if (detected_baudrate == 0) {
        LOG_WARN("MAIN", "第一次检测失败，5秒后再次检测...");
        Delay_ms(5000);
        detected_baudrate = DetectRelayBaudRate();
        
        /* 如果第二次检测仍然失败，程序无法继续运行 */
        if (detected_baudrate == 0) {
            LOG_ERROR("MAIN", "波特率检测失败，程序无法继续运行");
            DisplayStatusOnOLED(0, 0, 0);  /* 显示检测失败状态 */
            while(1) {
                Delay_ms(1000);
            }
        }
    }
    
    /* 确保UART2使用检测到的波特率 */
    if (SetUART2BaudRate(detected_baudrate) != UART_OK) {
        LOG_ERROR("MAIN", "设置UART2波特率失败，程序无法继续运行");
        DisplayStatusOnOLED(0, 0, 0);  /* 显示设置失败状态 */
        while(1) {
            Delay_ms(1000);
        }
    }
    
    LOG_INFO("MAIN", "继电器通信已就绪，波特率: %lu", detected_baudrate);
    DisplayStatusOnOLED(detected_baudrate, 0, 0);  /* 显示检测成功状态 */
    Delay_ms(1000);
    
    /* ========== 步骤9：测试ModBusRTU通信时间 ========== */
    TestModBusRTUCommunicationTime();
    
    /* 等待3秒 */
    LOG_INFO("MAIN", "等待3秒后开始功能测试...");
    Delay_ms(3000);
    
    /* ========== 步骤10：测试继电器开关 ========== */
    TestRelaySwitching(detected_baudrate);
    
    /* 等待5秒后开始下一轮测试 */
    LOG_INFO("MAIN", "等待5秒后开始下一轮测试...");
    Delay_ms(5000);
    
    /* ========== 步骤11：批量控制测试 ========== */
    TestRelayBatchControl(detected_baudrate);
    
    /* 等待2秒 */
    LOG_INFO("MAIN", "所有测试完成，等待2秒后切换波特率...");
    Delay_ms(2000);
    
    /* ========== 步骤12：切换继电器波特率 ========== */
    SwitchRelayBaudrate(detected_baudrate);
    
    /* 程序执行完成，进入死循环等待 */
    LOG_INFO("MAIN", "程序执行完成，等待重启...");
    while (1) {
        Delay_ms(1000);
    }
}
