/**
 * @file main_example.c
 * @brief Flash10 - LittleFS文件系统W25Q读写测试案例
 * @details 演示LittleFS文件系统在W25Q SPI Flash上的基本读写操作
 * 
 * 硬件连接：
 * - W25Q SPI Flash模块连接到SPI2
 *   - CS：PA11
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
 * - LED1：PA1（系统状态指示）
 * 
 * 功能演示：
 * 1. 系统初始化
 * 2. UART、Debug、Log初始化
 * 3. LED初始化
 * 4. 软件I2C初始化
 * 5. OLED初始化
 * 6. SPI初始化
 * 7. W25Q初始化与设备识别
 * 8. LittleFS文件系统初始化、挂载
 * 9. 文件操作测试（创建、写入、读取、验证）
 * 10. 目录操作测试（创建、列表）
 * 11. 主循环（LED闪烁，OLED显示状态）
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
#include "littlefs_wrapper.h"
#include "gpio.h"
#include "config.h"
#include "board.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
    uint32_t loop_count = 0;
    
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
    LOG_INFO("MAIN", "=== Flash10 - LittleFS文件系统W25Q读写测试案例 ===");
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
        OLED_ShowString(1, 1, "Flash10");
        OLED_ShowString(2, 1, "初始化中...");
        LOG_INFO("MAIN", "OLED 已初始化");
    }
    
    Delay_ms(500);
    
    /* ========== 步骤9：SPI初始化 ========== */
    OLED_ShowString(3, 1, "初始化SPI2...");
    
    /* 配置SPI2 NSS引脚为GPIO输出（软件NSS模式，SPI驱动不会自动配置） */
    /* 使用board.h中定义的宏，避免硬编码 */
    GPIO_EnableClock(SPI2_NSS_PORT);
    GPIO_Config(SPI2_NSS_PORT, SPI2_NSS_PIN, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHz);
    GPIO_WritePin(SPI2_NSS_PORT, SPI2_NSS_PIN, Bit_SET);  /* NSS默认拉高（不选中） */
    
    spi_status = SPI_HW_Init(SPI_INSTANCE_2);
    if (spi_status != SPI_OK)
    {
        char err_buf[17];
        snprintf(err_buf, sizeof(err_buf), "SPI失败:%d", spi_status);
        OLED_ShowString(4, 1, err_buf);
        LOG_ERROR("MAIN", "SPI 初始化失败: %d", spi_status);
        ErrorHandler_Handle(spi_status, "SPI");
        Delay_ms(3000);
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "SPI2 已初始化: PB13(SCK), PB14(MISO), PB15(MOSI), PA11(CS)");
    
    /* ========== 步骤10：W25Q初始化 ========== */
    OLED_ShowString(3, 1, "初始化W25Q...");
    w25q_status = W25Q_Init();
    if (w25q_status != W25Q_OK)
    {
        OLED_ShowString(4, 1, "W25Q初始化失败!");
        LOG_ERROR("MAIN", "W25Q 初始化失败: %d", w25q_status);
        ErrorHandler_Handle(w25q_status, "W25Q");
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "W25Q 初始化成功");
    
    /* 显示W25Q信息 */
    const w25q_dev_t* dev_info = W25Q_GetInfo();
    if (dev_info != NULL) {
        char buf[17];
        snprintf(buf, sizeof(buf), "容量:%d MB", dev_info->capacity_mb);
        OLED_ShowString(4, 1, buf);
        
        LOG_INFO("MAIN", "W25Q信息:");
        LOG_INFO("MAIN", "  容量: %d MB", dev_info->capacity_mb);
        LOG_INFO("MAIN", "  地址字节数: %d", dev_info->addr_bytes);
        LOG_INFO("MAIN", "  4字节模式: %s", dev_info->is_4byte_mode ? "是" : "否");
    }
    
    Delay_ms(1000);
    
    /* ========== 步骤11：LittleFS初始化 ========== */
    OLED_ShowString(3, 1, "初始化LittleFS...");
    LittleFS_Status_t littlefs_status = LittleFS_Init();
    if (littlefs_status != LITTLEFS_OK)
    {
        OLED_ShowString(4, 1, "LittleFS初始化失败!");
        LOG_ERROR("MAIN", "LittleFS 初始化失败: %d", littlefs_status);
        ErrorHandler_Handle(littlefs_status, "LittleFS");
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "LittleFS 初始化成功");
    OLED_ShowString(4, 1, "LittleFS已就绪");
    Delay_ms(500);
    
    /* ========== 步骤12：挂载前确保CS引脚配置正确 ========== */
    /* 确保SPI2 NSS引脚（CS）配置为推挽输出并拉高 */
    /* 使用GPIO模块API，避免直接操作寄存器（符合规范） */
    LOG_INFO("MAIN", "挂载前确保CS引脚配置正确...");
    
    /* 确保GPIO时钟已使能 */
    GPIO_EnableClock(SPI2_NSS_PORT);
    
    /* 确保CS引脚配置为推挽输出 */
    GPIO_Config(SPI2_NSS_PORT, SPI2_NSS_PIN, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHz);
    
    /* 确保CS引脚为高（释放状态） */
    GPIO_WritePin(SPI2_NSS_PORT, SPI2_NSS_PIN, Bit_SET);
    
    LOG_INFO("MAIN", "CS引脚已配置为推挽输出并拉高");
    Delay_ms(500);
    
    /* ========== 步骤13：挂载文件系统 ========== */
    LOG_INFO("MAIN", "开始挂载文件系统...");
    OLED_ShowString(3, 1, "Mounting...");
    LED_Toggle(LED_1);
    littlefs_status = LittleFS_Mount();
    LED_Toggle(LED_1);
    
#if CONFIG_LITTLEFS_FORCE_FORMAT
    /* 强制格式化模式：无论挂载是否成功，都先卸载然后格式化（解决NOSPC问题） */
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("MAIN", "挂载成功，但配置为强制格式化模式，准备卸载以便格式化...");
        OLED_ShowString(4, 1, "Unmounting...");
        Delay_ms(500);
        
        /* 先卸载 */
        LittleFS_Status_t unmount_status = LittleFS_Unmount();
        if (unmount_status != LITTLEFS_OK)
        {
            LOG_ERROR("MAIN", "卸载失败: %d（继续执行格式化）", unmount_status);
        }
    }
    
    /* 开始格式化文件系统（强制格式化模式） */
    LOG_INFO("MAIN", "开始格式化文件系统（强制格式化模式）...");
    OLED_ShowString(3, 1, "Formatting...");
    OLED_ShowString(4, 1, "Force Format");
    Delay_ms(500);
    
    LED_Toggle(LED_1);
    littlefs_status = LittleFS_Format();
    LED_Toggle(LED_1);
    
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("MAIN", "格式化成功！");
        OLED_ShowString(4, 1, "Format OK");
        Delay_ms(500);
        
        /* 格式化后重新挂载 */
        LOG_INFO("MAIN", "格式化后重新挂载...");
        OLED_ShowString(3, 1, "Remounting...");
        LED_Toggle(LED_1);
        littlefs_status = LittleFS_Mount();
        LED_Toggle(LED_1);
        
        if (littlefs_status == LITTLEFS_OK)
        {
            LOG_INFO("MAIN", "重新挂载成功！");
            OLED_ShowString(4, 1, "Mount OK");
        }
        else
        {
            LOG_ERROR("MAIN", "重新挂载失败: %d", littlefs_status);
            OLED_ShowString(4, 1, "Mount Failed!");
            while(1) { Delay_ms(1000); }
        }
    }
    else
    {
        LOG_ERROR("MAIN", "格式化失败: %d", littlefs_status);
        OLED_ShowString(4, 1, "Format Failed!");
        while(1) { Delay_ms(1000); }
    }
#else
    /* 正常模式：只在挂载失败时格式化 */
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("MAIN", "挂载成功！");
        OLED_ShowString(4, 1, "Mount OK");
    }
    else
    {
        /* 挂载失败，可能是文件系统不存在或损坏（第一次使用是正常的） */
        LOG_INFO("MAIN", "挂载失败: %d (可能是文件系统不存在，需要格式化)", littlefs_status);
        OLED_ShowString(3, 1, "Mount Failed");
        OLED_ShowString(4, 1, "Formatting...");
        Delay_ms(1000);
        
        /* 尝试格式化 */
        LOG_INFO("MAIN", "开始格式化文件系统...");
        LED_Toggle(LED_1);
        littlefs_status = LittleFS_Format();
        LED_Toggle(LED_1);
        
        if (littlefs_status == LITTLEFS_OK)
        {
            LOG_INFO("MAIN", "格式化成功！");
            OLED_ShowString(4, 1, "Format OK");
            Delay_ms(500);
            
            /* 格式化后重新挂载 */
            LOG_INFO("MAIN", "格式化后重新挂载...");
            OLED_ShowString(3, 1, "Remounting...");
            LED_Toggle(LED_1);
            littlefs_status = LittleFS_Mount();
            LED_Toggle(LED_1);
            
            if (littlefs_status == LITTLEFS_OK)
            {
                LOG_INFO("MAIN", "重新挂载成功！");
                OLED_ShowString(4, 1, "Mount OK");
            }
            else
            {
                LOG_ERROR("MAIN", "重新挂载失败: %d", littlefs_status);
                OLED_ShowString(4, 1, "Mount Failed!");
                while(1) { Delay_ms(1000); }
            }
        }
        else
        {
            LOG_ERROR("MAIN", "格式化失败: %d", littlefs_status);
            OLED_ShowString(4, 1, "Format Failed!");
            while(1) { Delay_ms(1000); }
        }
    }
#endif /* CONFIG_LITTLEFS_FORCE_FORMAT */
    Delay_ms(2000);
    
    /* ========== 步骤14：显示文件系统信息 ========== */
    LOG_INFO("MAIN", "获取文件系统信息...");
    OLED_Clear();
    OLED_ShowString(1, 1, "文件系统信息");
    
    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;
    littlefs_status = LittleFS_GetInfo(&total_bytes, &free_bytes);
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("MAIN", "文件系统信息:");
        LOG_INFO("MAIN", "  总空间: %llu 字节 (%.2f MB)", 
                 (unsigned long long)total_bytes, (double)total_bytes / (1024.0 * 1024.0));
        LOG_INFO("MAIN", "  空闲空间: %llu 字节 (%.2f MB)", 
                 (unsigned long long)free_bytes, (double)free_bytes / (1024.0 * 1024.0));
        
        char buf[17];
        snprintf(buf, sizeof(buf), "总:%lluKB", (unsigned long long)total_bytes / 1024);
        OLED_ShowString(2, 1, buf);
        snprintf(buf, sizeof(buf), "空闲:%lluKB", (unsigned long long)free_bytes / 1024);
        OLED_ShowString(3, 1, buf);
    }
    Delay_ms(2000);
    
    /* ========== 步骤14：基础文件操作测试 ========== */
    LOG_INFO("MAIN", "=== 开始文件操作测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "文件操作测试");
    
    /* 测试1：创建文件并写入数据 */
    LOG_INFO("MAIN", "测试1：创建文件并写入数据...");
    OLED_ShowString(2, 1, "创建文件...");
    
    lfs_file_t file;
    memset(&file, 0, sizeof(file));  /* ? 关键：清零文件句柄，避免脏数据 */
    const char* test_file = "test.txt";  /* ? 修复：使用相对路径，与目录列表一致 */
    const char* test_data = "Hello LittleFS!";
    
    /* 使用LittleFS标准API：LFS_O_WRONLY（只写）| LFS_O_CREAT（创建） */
    littlefs_status = LittleFS_FileOpen(&file, test_file, LFS_O_WRONLY | LFS_O_CREAT);
    if (littlefs_status == LITTLEFS_OK)
    {
        uint32_t bytes_written = 0;
        littlefs_status = LittleFS_FileWrite(&file, test_data, strlen(test_data), &bytes_written);
        if (littlefs_status == LITTLEFS_OK)
        {
            LOG_INFO("MAIN", "写入成功: %lu 字节", (unsigned long)bytes_written);
            
            /* ? 关键：同步文件，确保数据写入Flash */
            littlefs_status = LittleFS_FileSync(&file);
            if (littlefs_status == LITTLEFS_OK)
            {
                LOG_INFO("MAIN", "文件同步成功");
            }
            else
            {
                LOG_ERROR("MAIN", "文件同步失败: %d", littlefs_status);
            }
            
            LittleFS_FileClose(&file);
            OLED_ShowString(3, 1, "Write OK");
        }
        else
        {
            LOG_ERROR("MAIN", "写入失败: %d", littlefs_status);
            OLED_ShowString(3, 1, "写入失败");
            LittleFS_FileClose(&file);
        }
    }
    else
    {
        LOG_ERROR("MAIN", "创建文件失败: %d", littlefs_status);
        OLED_ShowString(3, 1, "创建失败");
    }
    Delay_ms(1000);
    
    /* 测试2：读取文件并验证 */
    LOG_INFO("MAIN", "测试2：读取文件并验证...");
    OLED_ShowString(2, 1, "Read File...");
    
    /* ? 关键：先检查文件是否存在 */
    lfs_t* lfs = LittleFS_GetLFS(LITTLEFS_INSTANCE_0);
    if (lfs != NULL) {
        struct lfs_info info;
        int stat_err = lfs_stat(lfs, test_file, &info);
        if (stat_err == 0) {
            LOG_INFO("MAIN", "文件存在: name='%s' size=%lu type=%d", 
                     info.name, (unsigned long)info.size, info.type);
        } else {
            LOG_ERROR("MAIN", "文件不存在或stat失败: %d", stat_err);
            OLED_ShowString(3, 1, "File Not Found");
            Delay_ms(1000);
        }
    }
    
    char read_buffer[64] = {0};
    uint32_t bytes_read = 0;
    
    /* ? 关键：清零文件句柄，避免脏数据 */
    memset(&file, 0, sizeof(file));
    
    /* ? 关键：添加延迟，确保Flash写入完成 */
    Delay_ms(100);
    
    LOG_INFO("MAIN", "尝试打开文件: %s (标志: 0x%X)", test_file, LFS_O_RDONLY);
    littlefs_status = LittleFS_FileOpen(&file, test_file, LFS_O_RDONLY);
    if (littlefs_status == LITTLEFS_OK)
    {
        littlefs_status = LittleFS_FileRead(&file, read_buffer, sizeof(read_buffer) - 1, &bytes_read);
        if (littlefs_status == LITTLEFS_OK)
        {
            read_buffer[bytes_read] = '\0';
            LOG_INFO("MAIN", "读取成功: %lu 字节", (unsigned long)bytes_read);
            LOG_INFO("MAIN", "读取内容: %s", read_buffer);
            
            if (strcmp(read_buffer, test_data) == 0)
            {
                LOG_INFO("MAIN", "数据验证成功！");
                OLED_ShowString(3, 1, "验证成功");
            }
            else
            {
                LOG_ERROR("MAIN", "数据验证失败！");
                OLED_ShowString(3, 1, "验证失败");
            }
            LittleFS_FileClose(&file);
        }
        else
        {
            LOG_ERROR("MAIN", "读取失败: %d", littlefs_status);
            OLED_ShowString(3, 1, "读取失败");
            LittleFS_FileClose(&file);
        }
    }
    else
    {
        LOG_ERROR("MAIN", "打开文件失败: %d", littlefs_status);
        /* -3905 是 LITTLEFS_ERROR_NOENT 的封装错误码 */
        if (littlefs_status == -3905 || littlefs_status == LITTLEFS_ERROR_NOENT) {
            LOG_ERROR("MAIN", "错误码 %d = LFS_ERR_NOENT (文件不存在或元数据损坏)", littlefs_status);
            LOG_ERROR("MAIN", "可能原因：1. 文件路径不正确 2. 文件元数据损坏 3. 缓存缓冲区冲突");
            LOG_ERROR("MAIN", "建议：检查文件路径格式，确保使用相对路径（如'test.txt'而非'/test.txt'）");
        } else if (littlefs_status == LITTLEFS_ERROR_CORRUPT) {
            LOG_ERROR("MAIN", "错误码 %d = LFS_ERR_CORRUPT (文件系统损坏)", littlefs_status);
        } else {
            LOG_ERROR("MAIN", "未知错误码: %d", littlefs_status);
        }
        OLED_ShowString(3, 1, "Open Failed");
    }
    Delay_ms(1000);
    
    /* 测试3：目录操作 */
    LOG_INFO("MAIN", "测试3：目录操作...");
    OLED_Clear();
    OLED_ShowString(1, 1, "目录操作测试");
    
    /* 创建目录 */
    const char* test_dir = "testdir";  /* ? 修复：使用相对路径 */
    LOG_INFO("MAIN", "创建目录: %s", test_dir);
    OLED_ShowString(2, 1, "创建目录...");
    littlefs_status = LittleFS_DirCreate(test_dir);
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("MAIN", "目录创建成功");
        OLED_ShowString(3, 1, "创建成功");
    }
    else if (littlefs_status == LITTLEFS_ERROR_EXIST)
    {
        LOG_INFO("MAIN", "目录已存在（正常）");
        OLED_ShowString(3, 1, "目录已存在");
    }
    else
    {
        LOG_ERROR("MAIN", "目录创建失败: %d", littlefs_status);
        OLED_ShowString(3, 1, "创建失败");
    }
    Delay_ms(1000);
    
    /* 列出根目录内容 */
    LOG_INFO("MAIN", "测试4：列出根目录内容...");
    OLED_Clear();
    OLED_ShowString(1, 1, "列出目录");
    
    lfs_dir_t dir;
    memset(&dir, 0, sizeof(dir));  /* ? 关键：清零目录句柄 */
    struct lfs_info info;  /* 注意：使用struct lfs_info，不是LittleFS_Info_t */
    memset(&info, 0, sizeof(info));  /* 初始化info结构体 */
    littlefs_status = LittleFS_DirOpen(&dir, ".");  /* ? 修复：使用"."表示当前目录（根目录） */
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("MAIN", "根目录内容:");
        int count = 0;
        while (1)
        {
            /* 每次读取前清空info结构体 */
            memset(&info, 0, sizeof(info));
            littlefs_status = LittleFS_DirRead(&dir, &info);
            if (littlefs_status == LITTLEFS_OK)
            {
                count++;
                const char* type_str = (info.type == LFS_TYPE_REG) ? "文件" : "目录";
                LOG_INFO("MAIN", "  [%d] %s: %s (大小: %lu 字节)", 
                         count, type_str, info.name, (unsigned long)info.size);
            }
            else if (littlefs_status == LITTLEFS_ERROR_NOENT)
            {
                /* 目录读取完毕，这是正常情况 */
                LOG_INFO("MAIN", "目录读取完毕（没有更多条目）");
                break;
            }
            else
            {
                /* 其他错误，记录并退出 */
                LOG_ERROR("MAIN", "读取目录项失败: %d (原始littlefs错误码可能未映射)", littlefs_status);
                break;
            }
        }
        LittleFS_DirClose(&dir);
        LOG_INFO("MAIN", "共 %d 个条目", count);
        
        char buf[17];
        snprintf(buf, sizeof(buf), "共%d个条目", count);
        OLED_ShowString(2, 1, buf);
    }
    else
    {
        LOG_ERROR("MAIN", "打开根目录失败: %d", littlefs_status);
        OLED_ShowString(2, 1, "打开失败");
    }
    Delay_ms(2000);
    
    /* ========== 步骤15：显示初始化完成 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "Flash10");
    OLED_ShowString(2, 1, "初始化完成");
    OLED_ShowString(3, 1, "LittleFS已就绪");
    LOG_INFO("MAIN", "=== 初始化完成，进入主循环 ===");
    Delay_ms(1000);
    
    /* ========== 步骤16：主循环 ========== */
    while (1)
    {
        loop_count++;
        LED_Toggle(LED_1);
        
        /* 每10次循环（约5秒）更新一次OLED显示 */
        if (loop_count % 10 == 0) {
            char buf[17];
            snprintf(buf, sizeof(buf), "运行中:%lu", (unsigned long)loop_count);
            OLED_ShowString(4, 1, buf);
            LOG_INFO("MAIN", "主循环运行中... (循环 %lu)", (unsigned long)loop_count);
        }
        
        Delay_ms(500);
    }
}
