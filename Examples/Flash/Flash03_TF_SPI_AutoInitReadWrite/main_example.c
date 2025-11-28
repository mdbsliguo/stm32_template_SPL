/**
 * @file main_example.c
 * @brief Flash03 - TF卡（MicroSD卡）SPI自动初始化读写示例
 * @details 演示TF卡自动初始化（TF_SPI_Init）和基本的读写操作，以及断电重启后验证写入数据是否生效
 * 
 * 硬件连接：
 * - TF卡（MicroSD卡）连接到SPI2
 *   - CS：PA11（软件NSS模式）
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
 * 1. 使用TF_SPI_Init()自动初始化SD卡
 * 2. 容量验证测试（检测SD卡是否虚标容量，测试5个关键地址）
 * 3. 断电重启验证（读取并检测当前数据模式A/B/其他）
 * 4. 智能写入测试（根据读取到的模式决定写入A或B，实现交替验证）
 * 5. 数据验证（验证写入的数据是否正确）
 * 
 * A/B交替验证逻辑：
 * - 模式A：0xAA（512字节全部为0xAA）
 * - 模式B：0x55（512字节全部为0x55）
 * - 如果读取到模式A，则写入模式B
 * - 如果读取到模式B，则写入模式A
 * - 如果读取到其他数据，则写入模式A
 * 
 * 容量验证逻辑：
 * - 测试5个关键地址：0%（起始）、50%、75%、90%、100%（最后一个块）
 * - 对每个地址执行：写入测试数据（0x5A）→ 读取 → 验证数据一致性
 * - 如果任何地址的读写失败，说明容量虚标，实际可用容量小于声明容量
 * - 可以检测不良厂商虚标容量的SD卡
 * 
 * @note 本示例使用TF_SPI模块的高级API（TF_SPI_Init()、TF_SPI_ReadBlock()、TF_SPI_WriteBlock()）
 * @note 通过A/B交替验证，可以确认数据是否持久化，即使SD卡被其他设备写过也能正常工作
 * @note 通过容量验证，可以检测SD卡是否虚标容量，避免使用超出实际容量的地址
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
#include "tf_spi.h"
#include "gpio.h"
#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 测试块地址 */
#define TEST_BLOCK_ADDR    0x0000  /* 测试块地址（块0） */

/* SD卡块大小 */
#define SD_BLOCK_SIZE      512     /* SD卡块大小（字节） */

/* 测试数据模式（A/B交替验证） */
#define TEST_DATA_PATTERN_A    0xAA    /* 测试数据模式A */
#define TEST_DATA_PATTERN_B    0x55    /* 测试数据模式B */

/* 数据模式枚举 */
typedef enum {
    DATA_PATTERN_NONE = 0,  /* 无匹配模式（其他数据） */
    DATA_PATTERN_A = 1,     /* 模式A（0xAA） */
    DATA_PATTERN_B = 2      /* 模式B（0x55） */
} data_pattern_t;

/* ==================== 测试函数 ==================== */

/**
 * @brief 读取并检测当前数据模式
 * @return data_pattern_t 检测到的数据模式（NONE/A/B）
 */
static data_pattern_t TestReadAndDetectPattern(void)
{
    TF_SPI_Status_t status;
    uint8_t read_buf[SD_BLOCK_SIZE];
    uint32_t i;
    uint8_t match_a = 1;
    uint8_t match_b = 1;
    
    LOG_INFO("MAIN", "=== 读取并检测数据模式 ===");
    LOG_INFO("MAIN", "读取块地址: 0x%04X", TEST_BLOCK_ADDR);
    
    /* 读取数据 */
    status = TF_SPI_ReadBlock(TEST_BLOCK_ADDR, read_buf);
    if (status != TF_SPI_OK)
    {
        LOG_ERROR("MAIN", "读取失败，状态: %d", status);
        LOG_INFO("MAIN", "可能是首次运行或SD卡未初始化");
        return DATA_PATTERN_NONE;
    }
    
    /* 检测是否为模式A（0xAA） */
    for (i = 0; i < SD_BLOCK_SIZE; i++)
    {
        if (read_buf[i] != TEST_DATA_PATTERN_A)
        {
            match_a = 0;
            break;
        }
    }
    
    if (match_a)
    {
        LOG_INFO("MAIN", "检测到数据模式A（0x%02X），512字节全部匹配", TEST_DATA_PATTERN_A);
        LOG_INFO("MAIN", "前16字节: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                 read_buf[0], read_buf[1], read_buf[2], read_buf[3],
                 read_buf[4], read_buf[5], read_buf[6], read_buf[7],
                 read_buf[8], read_buf[9], read_buf[10], read_buf[11],
                 read_buf[12], read_buf[13], read_buf[14], read_buf[15]);
        return DATA_PATTERN_A;
    }
    
    /* 检测是否为模式B（0x55） */
    for (i = 0; i < SD_BLOCK_SIZE; i++)
    {
        if (read_buf[i] != TEST_DATA_PATTERN_B)
        {
            match_b = 0;
            break;
        }
    }
    
    if (match_b)
    {
        LOG_INFO("MAIN", "检测到数据模式B（0x%02X），512字节全部匹配", TEST_DATA_PATTERN_B);
        LOG_INFO("MAIN", "前16字节: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                 read_buf[0], read_buf[1], read_buf[2], read_buf[3],
                 read_buf[4], read_buf[5], read_buf[6], read_buf[7],
                 read_buf[8], read_buf[9], read_buf[10], read_buf[11],
                 read_buf[12], read_buf[13], read_buf[14], read_buf[15]);
        return DATA_PATTERN_B;
    }
    
    /* 既不是A也不是B，显示前16字节 */
    LOG_INFO("MAIN", "检测到其他数据（既不是模式A也不是模式B）");
    LOG_INFO("MAIN", "前16字节: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
             read_buf[0], read_buf[1], read_buf[2], read_buf[3],
             read_buf[4], read_buf[5], read_buf[6], read_buf[7],
             read_buf[8], read_buf[9], read_buf[10], read_buf[11],
             read_buf[12], read_buf[13], read_buf[14], read_buf[15]);
    return DATA_PATTERN_NONE;
}

/**
 * @brief 单块写入测试（根据读取到的内容决定写入A或B）
 * @param[in] current_pattern 当前读取到的数据模式
 * @return uint8_t 写入的数据模式（TEST_DATA_PATTERN_A 或 TEST_DATA_PATTERN_B）
 */
static uint8_t TestSingleBlockWrite(data_pattern_t current_pattern)
{
    TF_SPI_Status_t status;
    uint8_t write_buf[SD_BLOCK_SIZE];
    uint32_t i;
    uint8_t write_pattern;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Write Test");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 单块写入测试 ===");
    LOG_INFO("MAIN", "写入块地址: 0x%04X", TEST_BLOCK_ADDR);
    
    /* 根据读取到的内容决定写入A还是B */
    if (current_pattern == DATA_PATTERN_A)
    {
        /* 如果读取到A，则写入B */
        write_pattern = TEST_DATA_PATTERN_B;
        LOG_INFO("MAIN", "检测到模式A，将写入模式B（0x%02X）", write_pattern);
    }
    else if (current_pattern == DATA_PATTERN_B)
    {
        /* 如果读取到B，则写入A */
        write_pattern = TEST_DATA_PATTERN_A;
        LOG_INFO("MAIN", "检测到模式B，将写入模式A（0x%02X）", write_pattern);
    }
    else
    {
        /* 如果读取到其他数据，则写入A */
        write_pattern = TEST_DATA_PATTERN_A;
        LOG_INFO("MAIN", "检测到其他数据，将写入模式A（0x%02X）", write_pattern);
    }
    
    /* 准备测试数据 */
    for (i = 0; i < SD_BLOCK_SIZE; i++)
    {
        write_buf[i] = write_pattern;
    }
    
    /* 使用TF_SPI模块的高级API进行写入 */
    status = TF_SPI_WriteBlock(TEST_BLOCK_ADDR, write_buf);
    
    if (status == TF_SPI_OK)
    {
        OLED_ShowString(2, 1, "Write: OK");
        char buf[17];
        snprintf(buf, sizeof(buf), "Pattern: 0x%02X", write_pattern);
        OLED_ShowString(3, 1, buf);
        LOG_INFO("MAIN", "单块写入成功");
        LOG_INFO("MAIN", "写入数据模式: 0x%02X (512字节)", write_pattern);
        Delay_ms(2000);
        return write_pattern;
    }
    else
    {
        OLED_ShowString(2, 1, "Write: Failed");
        LOG_ERROR("MAIN", "单块写入失败，状态: %d", status);
        Delay_ms(2000);
        return 0;
    }
}

/**
 * @brief 数据验证测试（验证写入的数据是否正确）
 * @param[in] expected_pattern 期望的数据模式（TEST_DATA_PATTERN_A 或 TEST_DATA_PATTERN_B）
 */
static void TestDataVerification(uint8_t expected_pattern)
{
    TF_SPI_Status_t status;
    uint8_t read_buf[SD_BLOCK_SIZE];
    uint32_t i;
    uint32_t error_count = 0;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Verify Test");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 数据验证测试 ===");
    LOG_INFO("MAIN", "验证块地址: 0x%04X", TEST_BLOCK_ADDR);
    LOG_INFO("MAIN", "期望数据模式: 0x%02X", expected_pattern);
    
    /* 读取数据 */
    status = TF_SPI_ReadBlock(TEST_BLOCK_ADDR, read_buf);
    if (status != TF_SPI_OK)
    {
        OLED_ShowString(2, 1, "Read Failed");
        LOG_ERROR("MAIN", "读取失败，状态: %d", status);
        Delay_ms(2000);
        return;
    }
    
    /* 对比数据 */
    for (i = 0; i < SD_BLOCK_SIZE; i++)
    {
        if (read_buf[i] != expected_pattern)
        {
            error_count++;
            if (error_count <= 5)  /* 只记录前5个错误 */
            {
                LOG_ERROR("MAIN", "数据不匹配，位置 %d: 期望=0x%02X, 读取=0x%02X", 
                         i, expected_pattern, read_buf[i]);
            }
        }
    }
    
    if (error_count == 0)
    {
        OLED_ShowString(2, 1, "Verify: OK");
        char buf[17];
        snprintf(buf, sizeof(buf), "Pattern: 0x%02X", expected_pattern);
        OLED_ShowString(3, 1, buf);
        LOG_INFO("MAIN", "数据验证成功，512字节全部匹配模式 0x%02X", expected_pattern);
    }
    else
    {
        char buf[17];
        snprintf(buf, sizeof(buf), "Error: %d", error_count);
        OLED_ShowString(2, 1, buf);
        LOG_ERROR("MAIN", "数据验证失败，错误字节数: %d/%d", error_count, SD_BLOCK_SIZE);
        if (error_count > 5)
        {
            LOG_ERROR("MAIN", "（仅显示前5个错误，实际错误数: %d）", error_count);
        }
    }
    
    Delay_ms(2000);
}

/**
 * @brief 容量验证测试（检测SD卡是否虚标容量）
 * @param[in] dev_info 设备信息指针
 * @return uint8_t 1-容量验证通过，0-容量验证失败（虚标）
 */
static uint8_t TestCapacityVerification(const tf_spi_dev_t *dev_info)
{
    TF_SPI_Status_t status;
    uint8_t write_buf[SD_BLOCK_SIZE];
    uint8_t read_buf[SD_BLOCK_SIZE];
    uint32_t i;
    uint32_t test_addresses[5];
    uint32_t test_count = 5;
    uint32_t test_idx;
    uint8_t test_pattern = 0x5A;  /* 测试数据模式（与A/B不同，避免冲突） */
    uint8_t all_passed = 1;
    
    /* 计算测试地址：起始区域、50%、75%、90%、100% */
    /* 注意：不使用块地址0，因为块地址0用于A/B模式断电重启验证 */
    test_addresses[0] = 1;                              /* 块1（起始区域，避免覆盖块0的A/B模式数据） */
    test_addresses[1] = dev_info->block_count / 2;     /* 50%位置 */
    test_addresses[2] = dev_info->block_count * 3 / 4;  /* 75%位置 */
    test_addresses[3] = dev_info->block_count * 9 / 10; /* 90%位置 */
    test_addresses[4] = dev_info->block_count - 1;      /* 最后一个块 */
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Cap Verify");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 容量验证测试 ===");
    LOG_INFO("MAIN", "检测SD卡是否虚标容量");
    LOG_INFO("MAIN", "声明容量: %d MB (%d 块)", dev_info->capacity_mb, dev_info->block_count);
    LOG_INFO("MAIN", "将测试 %d 个关键地址", test_count);
    
    /* 准备测试数据 */
    for (i = 0; i < SD_BLOCK_SIZE; i++)
    {
        write_buf[i] = test_pattern;
    }
    
    /* 测试每个关键地址 */
    for (test_idx = 0; test_idx < test_count; test_idx++)
    {
        uint32_t test_addr = test_addresses[test_idx];
        uint32_t percentage = (test_addr * 100) / dev_info->block_count;
        
        LOG_INFO("MAIN", "--- 测试地址 %lu/%lu (块地址: %lu, 约 %lu%%) ---", 
                 test_idx + 1, test_count, test_addr, percentage);
        
        /* 写入测试 */
        status = TF_SPI_WriteBlock(test_addr, write_buf);
        if (status != TF_SPI_OK)
        {
            LOG_ERROR("MAIN", "地址 %lu 写入失败，状态: %d", test_addr, status);
            LOG_ERROR("MAIN", "容量可能虚标！实际可用容量小于声明容量");
            all_passed = 0;
            break;
        }
        
        /* 读取测试 */
        status = TF_SPI_ReadBlock(test_addr, read_buf);
        if (status != TF_SPI_OK)
        {
            LOG_ERROR("MAIN", "地址 %lu 读取失败，状态: %d", test_addr, status);
            LOG_ERROR("MAIN", "容量可能虚标！实际可用容量小于声明容量");
            all_passed = 0;
            break;
        }
        
        /* 数据验证 */
        uint8_t match = 1;
        for (i = 0; i < SD_BLOCK_SIZE; i++)
        {
            if (read_buf[i] != test_pattern)
            {
                match = 0;
                break;
            }
        }
        
        if (match)
        {
            LOG_INFO("MAIN", "地址 %lu 验证成功", test_addr);
        }
        else
        {
            LOG_ERROR("MAIN", "地址 %lu 数据验证失败", test_addr);
            LOG_ERROR("MAIN", "容量可能虚标！数据无法正确写入/读取");
            all_passed = 0;
            break;
        }
        
        Delay_ms(100);  /* 短暂延时 */
    }
    
    if (all_passed)
    {
        OLED_ShowString(2, 1, "Cap: OK");
        char buf[17];
        snprintf(buf, sizeof(buf), "%d MB OK", dev_info->capacity_mb);
        OLED_ShowString(3, 1, buf);
        LOG_INFO("MAIN", "=== 容量验证通过 ===");
        LOG_INFO("MAIN", "所有测试地址读写正常，容量未虚标");
        return 1;
    }
    else
    {
        OLED_ShowString(2, 1, "Cap: Failed");
        OLED_ShowString(3, 1, "May Be Fake");
        LOG_ERROR("MAIN", "=== 容量验证失败 ===");
        LOG_ERROR("MAIN", "检测到容量虚标！实际可用容量小于声明容量");
        LOG_ERROR("MAIN", "建议：使用实际可用容量，避免写入超出范围的地址");
        return 0;
    }
}

/**
 * @brief 主函数
 */
int main(void)
{
    TF_SPI_Status_t tf_status;
    SPI_Status_t spi_status;
    SoftI2C_Status_t i2c_status;
    UART_Status_t uart_status;
    OLED_Status_t oled_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    const tf_spi_dev_t *dev_info;
    
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
    LOG_INFO("MAIN", "=== TF卡（MicroSD卡）SPI自动初始化读写示例 ===");
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
        OLED_ShowString(1, 1, "TF Card Demo");
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
        LOG_ERROR("MAIN", "SPI2 初始化失败: %d", spi_status);
        ErrorHandler_Handle(spi_status, "SPI");
        while(1) { Delay_ms(1000); }
    }
    else
    {
        OLED_ShowString(4, 1, "SPI2: OK");
        LOG_INFO("MAIN", "SPI2 已初始化: PB13(SCK), PB14(MISO), PB15(MOSI), PA11(CS)");
    }
    
    Delay_ms(500);
    
    /* ========== 步骤10：TF卡自动初始化 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "TF Card Init");
    OLED_ShowString(2, 1, "Using Auto Init");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== TF卡自动初始化 ===");
    LOG_INFO("MAIN", "使用TF_SPI_Init()自动初始化SD卡");
    
    tf_status = TF_SPI_Init();
    if (tf_status == TF_SPI_OK)
    {
        OLED_ShowString(2, 1, "Init: OK");
        LOG_INFO("MAIN", "TF_SPI_Init()成功！");
        
        /* 获取设备信息 */
        dev_info = TF_SPI_GetInfo();
        if (dev_info != NULL)
        {
            char buf[17];
            snprintf(buf, sizeof(buf), "Cap: %d MB", dev_info->capacity_mb);
            OLED_ShowString(3, 1, buf);
            
            LOG_INFO("MAIN", "SD卡信息：");
            LOG_INFO("MAIN", "  容量: %d MB", dev_info->capacity_mb);
            LOG_INFO("MAIN", "  块大小: %d 字节", dev_info->block_size);
            LOG_INFO("MAIN", "  块数量: %d", dev_info->block_count);
            LOG_INFO("MAIN", "  卡类型: %s", 
                     dev_info->card_type == TF_SPI_CARD_TYPE_SDSC ? "SDSC" :
                     dev_info->card_type == TF_SPI_CARD_TYPE_SDHC ? "SDHC" :
                     dev_info->card_type == TF_SPI_CARD_TYPE_SDXC ? "SDXC" : "Unknown");
            
            /* 容量验证测试（检测是否虚标容量） */
            Delay_ms(1000);
            TestCapacityVerification(dev_info);
            Delay_ms(2000);
        }
    }
    else
    {
        OLED_ShowString(2, 1, "Init: Failed");
        char err_buf[17];
        snprintf(err_buf, sizeof(err_buf), "Error: %d", tf_status);
        OLED_ShowString(3, 1, err_buf);
        LOG_ERROR("MAIN", "TF_SPI_Init()失败，状态: %d", tf_status);
        LOG_ERROR("MAIN", "可能原因：");
        LOG_ERROR("MAIN", "  1. SD卡未插入或未上电");
        LOG_ERROR("MAIN", "  2. SPI引脚连接问题");
        LOG_ERROR("MAIN", "  3. CS引脚（PA11）控制问题");
        while(1) { Delay_ms(1000); }
    }
    
    Delay_ms(2000);
    
    /* ========== 步骤11：断电重启验证（读取并检测数据模式） ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "Power Off Test");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 断电重启验证 ===");
    LOG_INFO("MAIN", "读取并检测当前数据模式，判断是否为上次写入的数据");
    
    data_pattern_t current_pattern = TestReadAndDetectPattern();
    uint8_t write_pattern;
    uint8_t verify_success = 0;
    
    /* 根据检测到的模式决定写入A还是B */
    if (current_pattern == DATA_PATTERN_A)
    {
        /* 检测到模式A，说明上次写入的是A，这次写入B */
        write_pattern = TEST_DATA_PATTERN_B;
        LOG_INFO("MAIN", "断电重启验证：检测到模式A（0x%02X），说明上次写入的数据已持久化", TEST_DATA_PATTERN_A);
        LOG_INFO("MAIN", "本次将写入模式B（0x%02X）进行交替验证", write_pattern);
        verify_success = 1;  /* 验证成功，数据已持久化 */
    }
    else if (current_pattern == DATA_PATTERN_B)
    {
        /* 检测到模式B，说明上次写入的是B，这次写入A */
        write_pattern = TEST_DATA_PATTERN_A;
        LOG_INFO("MAIN", "断电重启验证：检测到模式B（0x%02X），说明上次写入的数据已持久化", TEST_DATA_PATTERN_B);
        LOG_INFO("MAIN", "本次将写入模式A（0x%02X）进行交替验证", write_pattern);
        verify_success = 1;  /* 验证成功，数据已持久化 */
    }
    else
    {
        /* 检测到其他数据，说明是首次运行或数据被其他设备修改过 */
        write_pattern = TEST_DATA_PATTERN_A;
        LOG_INFO("MAIN", "断电重启验证：检测到其他数据，可能是首次运行或数据被其他设备修改过");
        LOG_INFO("MAIN", "本次将写入模式A（0x%02X）", write_pattern);
        verify_success = 0;  /* 验证失败，首次运行 */
    }
    
    Delay_ms(1000);
    
    /* ========== 步骤12：执行写入测试 ========== */
    uint8_t actual_write_pattern = TestSingleBlockWrite(current_pattern);
    if (actual_write_pattern == 0)
    {
        LOG_ERROR("MAIN", "写入失败，无法继续测试");
        while(1) { Delay_ms(1000); }
    }
    Delay_ms(1000);
    
    /* ========== 步骤13：执行数据验证 ========== */
    TestDataVerification(actual_write_pattern);
    Delay_ms(1000);
    
    /* ========== 步骤14：显示测试结果 ========== */
    LOG_INFO("MAIN", "=== 读写测试完成 ===");
    LOG_INFO("MAIN", "数据已写入到块地址: 0x%04X", TEST_BLOCK_ADDR);
    LOG_INFO("MAIN", "写入数据模式: 0x%02X (512字节)", actual_write_pattern);
    
    if (verify_success)
    {
        LOG_INFO("MAIN", "=== 断电重启验证成功 ===");
        LOG_INFO("MAIN", "数据已持久化保存，断电后数据保持有效");
        LOG_INFO("MAIN", "下次启动将检测到模式 0x%02X，并写入另一个模式进行交替验证", actual_write_pattern);
        
        OLED_Clear();
        OLED_ShowString(1, 1, "Power Off OK");
        OLED_ShowString(2, 1, "Data Persisted");
        char buf[17];
        snprintf(buf, sizeof(buf), "Pattern: 0x%02X", actual_write_pattern);
        OLED_ShowString(3, 1, buf);
    }
    else
    {
        LOG_INFO("MAIN", "=== 首次运行或数据被修改 ===");
        LOG_INFO("MAIN", "已写入模式A（0x%02X），下次启动将检测并写入模式B", TEST_DATA_PATTERN_A);
        
        OLED_Clear();
        OLED_ShowString(1, 1, "First Run");
        OLED_ShowString(2, 1, "Pattern: 0xAA");
    }
    
    Delay_ms(3000);
    
    /* ========== 步骤15：主循环 ========== */
    LOG_INFO("MAIN", "=== 进入主循环 ===");
    if (verify_success)
    {
        LOG_INFO("MAIN", "状态: 数据已验证，系统正常运行");
    }
    else
    {
        LOG_INFO("MAIN", "状态: 首次运行，等待下次断电重启验证");
    }
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Running...");
    if (verify_success)
    {
        OLED_ShowString(2, 1, "Data Verified");
    }
    else
    {
        OLED_ShowString(2, 1, "First Run");
    }
    
    while (1)
    {
        LED_Toggle(LED_1);
        Delay_ms(500);
    }
}
