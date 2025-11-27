/**
 * @file main_example.c
 * @brief Flash01 - W25Q SPI Flash模块演示
 * @details 演示W25Q Flash的完整读写测试，包括设备识别、扇区擦除、页编程、数据读取和断电重启测试
 * 
 * 硬件连接：
 * - W25Q SPI Flash模块连接到SPI2
 *   - CS：PA15
 *   - SCK：PB13（SPI2_SCK）
 *   - MISO：PB14（SPI2_MISO）
 *   - MOSI：PB15（SPI2_MOSI）
 *   - VCC：3.3V
 *   - GND：GND
 * - OLED显示屏（用于显示关键信息）
 *   - SCL：PB8
 *   - SDA：PB9
 * - UART1（用于详细日志输出）
 *   - TX：PA9
 *   - RX：PA10
 * 
 * 功能演示：
 * 1. W25Q初始化与设备识别
 * 2. 扇区擦除（4KB）
 * 3. 单页写入（256字节）
 * 4. 跨页写入（512字节）
 * 5. 数据读取与验证
 * 6. 断电重启测试（数据持久性验证）
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "led.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include "i2c_sw.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "spi_hw.h"
#include "w25q_spi.h"
#include "gpio.h"
#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 持久化测试数据地址（第二个扇区，避免与测试数据冲突） */
#define PERSISTENT_TEST_ADDR    0x1000
#define PERSISTENT_TEST_SIZE    256

/* 测试数据地址 */
#define TEST_SECTOR_ADDR        0x0000
#define TEST_PAGE_ADDR          0x0000
#define TEST_CROSS_PAGE_ADDR    0x0100  /* 修改为0x0100，避免与TEST_PAGE_ADDR重叠 */
#define TEST_PAGE_SIZE          256
#define TEST_CROSS_PAGE_SIZE    512

/**
 * @brief 在OLED上显示设备信息
 */
static void DisplayDeviceInfo(const w25q_dev_t *info)
{
    char buffer[17];
    uint8_t i;
    
    if (info == NULL)
    {
        OLED_ShowString(1, 1, "Device Info NULL");
        return;
    }
    
    /* 第1行：设备ID */
    snprintf(buffer, sizeof(buffer), "ID: 0x%04X%04X", info->manufacturer_id, info->device_id);
    OLED_ShowString(1, 1, buffer);
    /* 清除行尾残留字符 */
    for (i = strlen(buffer) + 1; i <= 16; i++)
    {
        OLED_ShowChar(1, i, ' ');
    }
    
    /* 第2行：容量和地址字节数 */
    snprintf(buffer, sizeof(buffer), "Cap: %dMB %dByte", info->capacity_mb, info->addr_bytes);
    OLED_ShowString(2, 1, buffer);
    /* 清除行尾残留字符 */
    for (i = strlen(buffer) + 1; i <= 16; i++)
    {
        OLED_ShowChar(2, i, ' ');
    }
    
    /* 第3行：4字节模式状态 */
    snprintf(buffer, sizeof(buffer), "4Byte: %s", info->is_4byte_mode ? "Yes" : "No");
    OLED_ShowString(3, 1, buffer);
    /* 清除行尾残留字符 */
    for (i = strlen(buffer) + 1; i <= 16; i++)
    {
        OLED_ShowChar(3, i, ' ');
    }
    
    /* 第4行：状态 */
    snprintf(buffer, sizeof(buffer), "State: %s", (info->state == W25Q_STATE_INITIALIZED) ? "OK" : "Fail");
    OLED_ShowString(4, 1, buffer);
    /* 清除行尾残留字符 */
    for (i = strlen(buffer) + 1; i <= 16; i++)
    {
        OLED_ShowChar(4, i, ' ');
    }
}

/**
 * @brief 数据对比并显示结果
 */
static uint8_t CompareData(const uint8_t *data1, const uint8_t *data2, uint32_t len, const char *test_name)
{
    uint32_t i;
    uint32_t error_count = 0;
    uint32_t first_error_pos = 0;
    
    for (i = 0; i < len; i++)
    {
        if (data1[i] != data2[i])
        {
            if (error_count == 0)
            {
                first_error_pos = i;
            }
            error_count++;
        }
    }
    
    if (error_count == 0)
    {
        /* 数据匹配 */
        OLED_ShowString(3, 1, "Verify OK");
        LOG_INFO("MAIN", "%s: 数据验证成功 (%d 字节)", test_name, len);
        return 1;
    }
    else
    {
        /* 数据不匹配 */
        char buffer[17];
        snprintf(buffer, sizeof(buffer), "Verify Fail:%d", error_count);
        OLED_ShowString(3, 1, buffer);
        LOG_ERROR("MAIN", "%s: 数据验证失败！错误: %d/%d，首个错误位置: %d (0x%02X != 0x%02X)",
                   test_name, error_count, len, first_error_pos, data1[first_error_pos], data2[first_error_pos]);
        return 0;
    }
}

/**
 * @brief 打印数据内容（前32字节）
 */
static void PrintDataHex(const uint8_t *data, uint32_t len, const char *name)
{
    uint32_t i;
    uint32_t print_len = (len > 32) ? 32 : len;
    char hex_line[64];
    uint32_t pos = 0;
    
    LOG_INFO("MAIN", "%s (前 %d 字节):", name, print_len);
    
    for (i = 0; i < print_len; i++)
    {
        if (i % 16 == 0)
        {
            if (i > 0)
            {
                hex_line[pos] = '\0';
                LOG_INFO("MAIN", "  %s", hex_line);
                pos = 0;
            }
            pos += snprintf(hex_line + pos, sizeof(hex_line) - pos, "%04X: ", i);
        }
        pos += snprintf(hex_line + pos, sizeof(hex_line) - pos, "%02X ", data[i]);
        
        if ((i + 1) % 16 == 0)
        {
            hex_line[pos] = '\0';
            LOG_INFO("MAIN", "  %s", hex_line);
            pos = 0;
        }
    }
    if (print_len % 16 != 0)
    {
        hex_line[pos] = '\0';
        LOG_INFO("MAIN", "  %s", hex_line);
    }
}

/**
 * @brief 设备识别测试
 */
static void TestDeviceIdentification(void)
{
    const w25q_dev_t *info;
    uint32_t full_device_id;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Device ID Test");
    Delay_ms(500);
    
    /* 获取设备信息 */
    info = W25Q_GetInfo();
    if (info == NULL)
    {
        OLED_ShowString(2, 1, "GetInfo NULL!");
        LOG_ERROR("MAIN", "W25Q_GetInfo() 返回 NULL");
        Delay_ms(2000);
        return;
    }
    
    /* 显示设备信息 */
    DisplayDeviceInfo(info);
    
    /* UART输出详细信息 */
    full_device_id = ((uint32_t)info->manufacturer_id << 16) | info->device_id;
    LOG_INFO("MAIN", "=== W25Q 设备信息 ===");
    LOG_INFO("MAIN", "制造商ID: 0x%04X", info->manufacturer_id);
    LOG_INFO("MAIN", "设备ID: 0x%04X", info->device_id);
    LOG_INFO("MAIN", "完整设备ID: 0x%08X", full_device_id);
    LOG_INFO("MAIN", "容量: %d MB", info->capacity_mb);
    LOG_INFO("MAIN", "地址字节数: %d", info->addr_bytes);
    LOG_INFO("MAIN", "4字节模式: %s", info->is_4byte_mode ? "是" : "否");
    LOG_INFO("MAIN", "状态: %s", (info->state == W25Q_STATE_INITIALIZED) ? "已初始化" : "未初始化");
    
    Delay_ms(2000);
}

/**
 * @brief 扇区擦除测试
 */
static void TestSectorErase(void)
{
    W25Q_Status_t status;
    uint32_t start_tick, elapsed;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Sector Erase");
    OLED_ShowString(2, 1, "Addr: 0x0000");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 扇区擦除测试 ===");
    LOG_INFO("MAIN", "擦除地址 0x%04X 的扇区 (4KB)", TEST_SECTOR_ADDR);
    
    start_tick = Delay_GetTick();
    status = W25Q_EraseSector(TEST_SECTOR_ADDR);
    if (status != W25Q_OK)
    {
        OLED_ShowString(3, 1, "Erase Failed!");
        LOG_ERROR("MAIN", "W25Q_EraseSector 失败: %d", status);
        ErrorHandler_Handle(status, "W25Q");
        Delay_ms(2000);
        return;
    }
    
    OLED_ShowString(3, 1, "Waiting...");
    status = W25Q_WaitReady(0);
    if (status != W25Q_OK)
    {
        OLED_ShowString(3, 1, "Wait Timeout!");
        LOG_ERROR("MAIN", "W25Q_WaitReady 超时: %d", status);
        Delay_ms(2000);
        return;
    }
    
    elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
    OLED_ShowString(3, 1, "Erase Done");
    LOG_INFO("MAIN", "扇区擦除完成，耗时 %d ms", elapsed);
    
    Delay_ms(1500);
}

/**
 * @brief 单页写入测试
 */
static void TestPageWrite(void)
{
    W25Q_Status_t status;
    uint8_t write_buf[TEST_PAGE_SIZE];
    uint32_t i;
    uint32_t start_tick, elapsed;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Page Write Test");
    OLED_ShowString(2, 1, "256 bytes");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 页写入测试 ===");
    
    /* 准备测试数据（递增序列） */
    for (i = 0; i < TEST_PAGE_SIZE; i++)
    {
        write_buf[i] = (uint8_t)(i & 0xFF);
    }
    
    LOG_INFO("MAIN", "写入 %d 字节到地址 0x%04X", TEST_PAGE_SIZE, TEST_PAGE_ADDR);
    PrintDataHex(write_buf, TEST_PAGE_SIZE, "写入数据");
    
    OLED_ShowString(3, 1, "Writing...");
    start_tick = Delay_GetTick();
    status = W25Q_Write(TEST_PAGE_ADDR, write_buf, TEST_PAGE_SIZE);
    if (status != W25Q_OK)
    {
        OLED_ShowString(3, 1, "Write Failed!");
        LOG_ERROR("MAIN", "W25Q_Write 失败: %d", status);
        ErrorHandler_Handle(status, "W25Q");
        Delay_ms(2000);
        return;
    }
    
    status = W25Q_WaitReady(0);
    if (status != W25Q_OK)
    {
        OLED_ShowString(3, 1, "Wait Timeout!");
        LOG_ERROR("MAIN", "W25Q_WaitReady 超时: %d", status);
        Delay_ms(2000);
        return;
    }
    
    elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
    OLED_ShowString(3, 1, "Write Done");
    LOG_INFO("MAIN", "页写入完成，耗时 %d ms", elapsed);
    
    Delay_ms(1500);
}

/**
 * @brief 跨页写入测试
 */
static void TestCrossPageWrite(void)
{
    W25Q_Status_t status;
    uint8_t write_buf[TEST_CROSS_PAGE_SIZE];
    uint32_t i;
    uint32_t start_tick, elapsed;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Cross-Page Write");
    OLED_ShowString(2, 1, "512 bytes");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 跨页写入测试 ===");
    
    /* 准备测试数据（固定模式：0xAA, 0x55交替） */
    for (i = 0; i < TEST_CROSS_PAGE_SIZE; i++)
    {
        write_buf[i] = (i % 2 == 0) ? 0xAA : 0x55;
    }
    
    LOG_INFO("MAIN", "写入 %d 字节到地址 0x%04X (跨页边界)", TEST_CROSS_PAGE_SIZE, TEST_CROSS_PAGE_ADDR);
    PrintDataHex(write_buf, TEST_CROSS_PAGE_SIZE, "写入数据");
    
    OLED_ShowString(3, 1, "Writing...");
    start_tick = Delay_GetTick();
    status = W25Q_Write(TEST_CROSS_PAGE_ADDR, write_buf, TEST_CROSS_PAGE_SIZE);
    if (status != W25Q_OK)
    {
        OLED_ShowString(3, 1, "Write Failed!");
        LOG_ERROR("MAIN", "W25Q_Write 失败: %d", status);
        ErrorHandler_Handle(status, "W25Q");
        Delay_ms(2000);
        return;
    }
    
    status = W25Q_WaitReady(0);
    if (status != W25Q_OK)
    {
        OLED_ShowString(3, 1, "Wait Timeout!");
        LOG_ERROR("MAIN", "W25Q_WaitReady 超时: %d", status);
        Delay_ms(2000);
        return;
    }
    
    elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
    OLED_ShowString(3, 1, "Write Done");
    LOG_INFO("MAIN", "跨页写入完成，耗时 %d ms", elapsed);
    
    Delay_ms(1500);
}

/**
 * @brief 数据读取与验证测试
 */
static void TestReadAndVerify(void)
{
    W25Q_Status_t status;
    uint8_t read_buf[TEST_CROSS_PAGE_SIZE + TEST_CROSS_PAGE_ADDR - TEST_PAGE_ADDR];
    uint8_t write_buf[TEST_PAGE_SIZE];
    uint32_t i;
    uint8_t verify_ok;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Read & Verify");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 读取与验证测试 ===");
    
    /* 测试1：读取单页数据并验证 */
    OLED_ShowString(2, 1, "Test 1: Page");
    LOG_INFO("MAIN", "从地址 0x%04X 读取 %d 字节", TEST_PAGE_ADDR, TEST_PAGE_SIZE);
    
    status = W25Q_Read(TEST_PAGE_ADDR, read_buf, TEST_PAGE_SIZE);
    if (status != W25Q_OK)
    {
        OLED_ShowString(3, 1, "Read Failed!");
        LOG_ERROR("MAIN", "W25Q_Read 失败: %d", status);
        ErrorHandler_Handle(status, "W25Q");
        Delay_ms(2000);
        return;
    }
    
    /* 准备期望数据（递增序列） */
    for (i = 0; i < TEST_PAGE_SIZE; i++)
    {
        write_buf[i] = (uint8_t)(i & 0xFF);
    }
    
    /* 对比数据 */
    verify_ok = CompareData(write_buf, read_buf, TEST_PAGE_SIZE, "页读取测试");
    PrintDataHex(read_buf, TEST_PAGE_SIZE, "读取数据");
    
    Delay_ms(2000);
    
    /* 测试2：读取跨页数据并验证 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Read & Verify");
    OLED_ShowString(2, 1, "Test 2: Cross");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "从地址 0x%04X 读取 %d 字节 (跨页)", TEST_CROSS_PAGE_ADDR, TEST_CROSS_PAGE_SIZE);
    
    status = W25Q_Read(TEST_CROSS_PAGE_ADDR, read_buf, TEST_CROSS_PAGE_SIZE);
    if (status != W25Q_OK)
    {
        OLED_ShowString(3, 1, "Read Failed!");
        LOG_ERROR("MAIN", "W25Q_Read 失败: %d", status);
        ErrorHandler_Handle(status, "W25Q");
        Delay_ms(2000);
        return;
    }
    
    /* 准备期望数据（固定模式：0xAA, 0x55交替） */
    for (i = 0; i < TEST_CROSS_PAGE_SIZE; i++)
    {
        write_buf[i] = (i % 2 == 0) ? 0xAA : 0x55;
    }
    
    /* 对比数据 */
    verify_ok = CompareData(write_buf, read_buf, TEST_CROSS_PAGE_SIZE, "跨页读取测试");
    PrintDataHex(read_buf, TEST_CROSS_PAGE_SIZE, "读取数据");
    
    if (verify_ok)
    {
        OLED_ShowString(4, 1, "All Tests OK");
    }
    else
    {
        OLED_ShowString(4, 1, "Test Failed!");
    }
    
    Delay_ms(2000);
}

/**
 * @brief 断电重启测试（数据持久性验证）
 */
static void TestPowerCyclePersistence(void)
{
    W25Q_Status_t status;
    uint8_t write_buf[PERSISTENT_TEST_SIZE];
    uint8_t read_buf[PERSISTENT_TEST_SIZE];
    uint32_t i;
    uint8_t verify_ok;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Power Cycle Test");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 断电重启测试 ===");
    
    /* 检查是否已有持久化数据 */
    OLED_ShowString(2, 1, "Reading...");
    status = W25Q_Read(PERSISTENT_TEST_ADDR, read_buf, PERSISTENT_TEST_SIZE);
    if (status != W25Q_OK)
    {
        OLED_ShowString(3, 1, "Read Failed!");
        LOG_ERROR("MAIN", "W25Q_Read 失败: %d", status);
        Delay_ms(2000);
        return;
    }
    
    /* 检查数据是否为有效标记（前4字节为0xDEADBEEF的字节序列） */
    if (read_buf[0] == 0xDE && read_buf[1] == 0xAD && 
        read_buf[2] == 0xBE && read_buf[3] == 0xEF)
    {
        /* 发现已有持久化数据，进行验证 */
        OLED_ShowString(2, 1, "Found Data!");
        LOG_INFO("MAIN", "发现地址 0x%04X 处已有持久化数据", PERSISTENT_TEST_ADDR);
        
        /* 准备期望数据 */
        write_buf[0] = 0xDE;
        write_buf[1] = 0xAD;
        write_buf[2] = 0xBE;
        write_buf[3] = 0xEF;
        for (i = 4; i < PERSISTENT_TEST_SIZE; i++)
        {
            write_buf[i] = (uint8_t)(i & 0xFF);
        }
        
        /* 对比数据 */
        verify_ok = CompareData(write_buf, read_buf, PERSISTENT_TEST_SIZE, "断电重启测试");
        PrintDataHex(read_buf, PERSISTENT_TEST_SIZE, "持久化数据");
        
        if (verify_ok)
        {
            OLED_ShowString(3, 1, "Persist OK!");
            OLED_ShowString(4, 1, "Data Survived");
            LOG_INFO("MAIN", "断电重启测试通过：数据在断电重启后正确保留");
        }
        else
        {
            OLED_ShowString(3, 1, "Persist Fail!");
            OLED_ShowString(4, 1, "Data Corrupted");
            LOG_ERROR("MAIN", "断电重启测试失败：数据在断电重启后损坏");
        }
    }
    else
    {
        /* 没有持久化数据，写入新数据 */
        OLED_ShowString(2, 1, "Writing...");
        LOG_INFO("MAIN", "未发现已有持久化数据，写入新数据");
        
        /* 准备测试数据（标记 + 递增序列） */
        write_buf[0] = 0xDE;
        write_buf[1] = 0xAD;
        write_buf[2] = 0xBE;
        write_buf[3] = 0xEF;
        for (i = 4; i < PERSISTENT_TEST_SIZE; i++)
        {
            write_buf[i] = (uint8_t)(i & 0xFF);
        }
        
        /* 擦除扇区 */
        LOG_INFO("MAIN", "擦除地址 0x%04X 的扇区", PERSISTENT_TEST_ADDR);
        status = W25Q_EraseSector(PERSISTENT_TEST_ADDR);
        if (status != W25Q_OK)
        {
            OLED_ShowString(3, 1, "Erase Failed!");
            LOG_ERROR("MAIN", "W25Q_EraseSector 失败: %d", status);
            Delay_ms(2000);
            return;
        }
        status = W25Q_WaitReady(0);
        if (status != W25Q_OK)
        {
            OLED_ShowString(3, 1, "Wait Timeout!");
            LOG_ERROR("MAIN", "W25Q_WaitReady 超时: %d", status);
            Delay_ms(2000);
            return;
        }
        
        /* 写入数据 */
        LOG_INFO("MAIN", "写入持久化数据到地址 0x%04X", PERSISTENT_TEST_ADDR);
        status = W25Q_Write(PERSISTENT_TEST_ADDR, write_buf, PERSISTENT_TEST_SIZE);
        if (status != W25Q_OK)
        {
            OLED_ShowString(3, 1, "Write Failed!");
            LOG_ERROR("MAIN", "W25Q_Write 失败: %d", status);
            Delay_ms(2000);
            return;
        }
        status = W25Q_WaitReady(0);
        if (status != W25Q_OK)
        {
            OLED_ShowString(3, 1, "Wait Timeout!");
            LOG_ERROR("MAIN", "W25Q_WaitReady 超时: %d", status);
            Delay_ms(2000);
            return;
        }
        
        OLED_ShowString(3, 1, "Write Done");
        LOG_INFO("MAIN", "持久化数据写入成功");
        PrintDataHex(write_buf, PERSISTENT_TEST_SIZE, "写入数据");
        
        /* 提示用户断电重启 */
        OLED_Clear();
        OLED_ShowString(1, 1, "Power Cycle");
        OLED_ShowString(2, 1, "Test Ready");
        OLED_ShowString(3, 1, "Power Off &");
        OLED_ShowString(4, 1, "Restart Now");
        LOG_INFO("MAIN", "=== 断电重启测试数据已写入 ===");
        LOG_INFO("MAIN", "请断电重启系统以验证数据持久性");
        
        Delay_ms(5000);
    }
    
    Delay_ms(2000);
}

/**
 * @brief 主函数
 */
int main(void)
{
    SPI_Status_t spi_status;
    W25Q_Status_t w25q_status;
    SoftI2C_Status_t i2c_status;
    UART_Status_t uart_status;
    OLED_Status_t oled_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    
    /* ========== 步骤1：系统初始化 ========== */
    System_Init();
    
    /* ========== 步骤2：UART初始化 ========== */
    uart_status = UART_Init(UART_INSTANCE_1);
    if (uart_status != UART_OK)
    {
        while(1) { Delay_ms(1000); }
    }
    Delay_ms(100);
    
    /* ========== 步骤3：Debug模块初始化 ========== */
    debug_status = Debug_Init(DEBUG_MODE_UART, 115200);
    if (debug_status != 0)
    {
        while(1) { Delay_ms(1000); }
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
    
    /* ========== 步骤5：输出初始化信息 ========== */
    LOG_INFO("MAIN", "=== W25Q Flash 演示初始化 ===");
    LOG_INFO("MAIN", "UART1 已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "Debug 模块已初始化: UART 模式");
    LOG_INFO("MAIN", "Log 模块已初始化");
    
    /* ========== 步骤6：LED初始化 ========== */
    if (LED_Init() != LED_OK)
    {
        LOG_ERROR("MAIN", "LED 初始化失败");
        while(1) { Delay_ms(1000); }
    }
    
    /* ========== 步骤7：软件I2C初始化（OLED需要） ========== */
    i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_1);
    if (i2c_status != SOFT_I2C_OK)
    {
        LOG_ERROR("MAIN", "软件I2C 初始化失败: %d", i2c_status);
        ErrorHandler_Handle(i2c_status, "SOFT_I2C");
    }
    else
    {
        LOG_INFO("MAIN", "软件I2C 已初始化: PB8(SCL), PB9(SDA)");
    }
    
    /* ========== 步骤8：OLED初始化 ========== */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        LOG_ERROR("MAIN", "OLED 初始化失败: %d", oled_status);
        ErrorHandler_Handle(oled_status, "OLED");
    }
    else
    {
        OLED_Clear();
        OLED_ShowString(1, 1, "W25Q Flash Demo");
        OLED_ShowString(2, 1, "Initializing...");
        LOG_INFO("MAIN", "OLED 已初始化");
    }
    
    Delay_ms(500);
    
    /* ========== 步骤9：SPI初始化 ========== */
    OLED_ShowString(3, 1, "Init SPI2...");
    
    /* 手动配置PA11为GPIO输出（软件NSS模式，SPI驱动不会自动配置） */
    GPIO_EnableClock(GPIOA);
    GPIO_Config(GPIOA, GPIO_Pin_11, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHz);
    GPIO_WritePin(GPIOA, GPIO_Pin_11, Bit_SET);  /* NSS默认拉高（不选中） */
    
    spi_status = SPI_HW_Init(SPI_INSTANCE_2);
    if (spi_status != SPI_OK)
    {
        char err_buf[17];
        snprintf(err_buf, sizeof(err_buf), "SPI Fail:%d", spi_status);
        OLED_ShowString(4, 1, err_buf);
        LOG_ERROR("MAIN", "SPI 初始化失败: %d", spi_status);
        ErrorHandler_Handle(spi_status, "SPI");
        Delay_ms(3000);
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "SPI2 已初始化: PB13(SCK), PB14(MISO), PB15(MOSI), PA11(CS)");
    
    /* ========== 步骤10：W25Q初始化 ========== */
    OLED_ShowString(3, 1, "Init W25Q...");
    w25q_status = W25Q_Init();
    if (w25q_status != W25Q_OK)
    {
        OLED_ShowString(4, 1, "W25Q Init Fail!");
        LOG_ERROR("MAIN", "W25Q 初始化失败: %d", w25q_status);
        ErrorHandler_Handle(w25q_status, "W25Q");
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "W25Q 初始化成功");
    
    /* 诊断代码已移除，功能已验证正常 */
    Delay_ms(1000);
    
    /* ========== 功能演示 ========== */
    
    /* 1. 设备识别测试 */
    TestDeviceIdentification();
    
    /* 2. 扇区擦除测试（擦除地址0x0000的扇区） */
    TestSectorErase();
    
    /* 3. 单页写入测试（地址0x0000） */
    TestPageWrite();
    
    /* 4. 擦除地址0x0100所在的扇区（为跨页写入测试准备） */
    {
        W25Q_Status_t status;
        LOG_INFO("MAIN", "擦除地址 0x0100 的扇区 (为跨页写入测试准备)");
        status = W25Q_EraseSector(0x0100);
        if (status == W25Q_OK)
        {
            W25Q_WaitReady(0);
            LOG_INFO("MAIN", "扇区擦除完成");
        }
        Delay_ms(500);
    }
    
    /* 5. 跨页写入测试（地址0x0100） */
    TestCrossPageWrite();
    
    /* 5. 数据读取与验证测试 */
    TestReadAndVerify();
    
    /* 6. 断电重启测试（数据持久性验证） */
    TestPowerCyclePersistence();
    
    /* ========== 主循环 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "Demo Complete!");
    OLED_ShowString(2, 1, "All Tests Done");
    LOG_INFO("MAIN", "=== 所有测试完成 ===");
    
    while (1)
    {
        LED_Toggle(LED_1);
        Delay_ms(500);
    }
}

