/**
 * @file main_example.c
 * @brief Flash12 - LittleFS文件系统综合测试案例
 * @details 演示LittleFS文件系统在W25Q SPI Flash上的完整功能测试
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
 * 9. 11个综合功能测试
 * 10. 主循环（LED闪烁，OLED显示状态）
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

/* ==================== 辅助函数 ==================== */

/**
 * @brief 将LittleFS错误码转换为可读字符串
 */
static const char* littlefs_errstr(LittleFS_Status_t err)
{
    if (err == LITTLEFS_OK) return "OK";
    if (err == LITTLEFS_ERROR_NOENT) return "NOENT";
    if (err == LITTLEFS_ERROR_NOSPC) return "NOSPC";
    if (err == LITTLEFS_ERROR_CORRUPT) return "CORRUPT";
    if (err == LITTLEFS_ERROR_IO) return "IO";
    if (err == LITTLEFS_ERROR_EXIST) return "EXIST";
    if (err == LITTLEFS_ERROR_NOTDIR) return "NOTDIR";
    if (err == LITTLEFS_ERROR_ISDIR) return "ISDIR";
    if (err == LITTLEFS_ERROR_NOTEMPTY) return "NOTEMPTY";
    if (err == LITTLEFS_ERROR_BADF) return "BADF";
    if (err == LITTLEFS_ERROR_FBIG) return "FBIG";
    if (err == LITTLEFS_ERROR_INVALID_PARAM) return "INVAL";
    if (err == LITTLEFS_ERROR_NOMEM) return "NOMEM";
    if (err == LITTLEFS_ERROR_NOATTR) return "NOATTR";
    if (err == LITTLEFS_ERROR_NAMETOOLONG) return "NAMETOOLONG";
    return "UNKNOWN";
}

/**
 * @brief 检查文件系统状态并显示诊断信息（简化版）
 */
static void check_filesystem_status(void)
{
    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;
    LittleFS_Status_t status = LittleFS_GetInfo(&total_bytes, &free_bytes);
    
    if (status == LITTLEFS_OK)
    {
        LOG_INFO("TEST", "FS: %lluKB free/%lluKB total", 
                 (unsigned long long)free_bytes / 1024, 
                 (unsigned long long)total_bytes / 1024);
        
        if (free_bytes < 1024)
        {
            LOG_ERROR("TEST", "FS space < 1KB!");
        }
    }
}

/* ==================== 测试函数 ==================== */

/**
 * @brief 测试1：文件系统初始化测试
 */
static void test1_filesystem_init(void)
{
    LittleFS_Status_t littlefs_status;
    
    LOG_INFO("TEST", "=== 测试1：文件系统初始化测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test1: FS Init");
    
    /* 检查W25Q是否已初始化 */
    const w25q_dev_t* dev_info = W25Q_GetInfo();
    if (dev_info == NULL)
    {
        LOG_ERROR("TEST", "W25Q未初始化");
        OLED_ShowString(2, 1, "W25Q Not Init");
        return;
    }
    LOG_INFO("TEST", "W25Q已初始化: %d MB", dev_info->capacity_mb);
    OLED_ShowString(2, 1, "W25Q OK");
    
    /* 检查是否需要强制格式化 */
    #ifdef CONFIG_LITTLEFS_FORCE_FORMAT
    #if CONFIG_LITTLEFS_FORCE_FORMAT
    LOG_INFO("TEST", "强制格式化模式：先格式化再挂载");
    OLED_ShowString(3, 1, "Force Format...");
    littlefs_status = LittleFS_Format();
    if (littlefs_status != LITTLEFS_OK)
    {
        LOG_ERROR("TEST", "强制格式化失败: %d", littlefs_status);
        OLED_ShowString(3, 1, "Format Failed");
        return;
    }
    Delay_ms(500);
    #endif
    #endif
    
    /* 尝试挂载文件系统 */
    LOG_INFO("TEST", "尝试挂载文件系统...");
    OLED_ShowString(3, 1, "Mounting...");
    littlefs_status = LittleFS_Mount();
    
    if (littlefs_status != LITTLEFS_OK)
    {
        /* 挂载失败，自动格式化 */
        LOG_INFO("TEST", "挂载失败: %d (%s)，开始格式化...", littlefs_status, littlefs_errstr(littlefs_status));
        OLED_ShowString(3, 1, "Formatting...");
        littlefs_status = LittleFS_Format();
        
        if (littlefs_status == LITTLEFS_OK)
        {
            LOG_INFO("TEST", "格式化成功，重新挂载...");
            Delay_ms(500);
            littlefs_status = LittleFS_Mount();
        }
        else
        {
            LOG_ERROR("TEST", "格式化失败: %d", littlefs_status);
            OLED_ShowString(3, 1, "Format Failed");
            return;
        }
    }
    
    /* 挂载成功后，检查文件系统状态 */
    if (littlefs_status == LITTLEFS_OK)
    {
        uint64_t total_bytes = 0;
        uint64_t free_bytes = 0;
        LittleFS_Status_t info_status = LittleFS_GetInfo(&total_bytes, &free_bytes);
        if (info_status == LITTLEFS_OK)
        {
            /* 如果空闲空间异常（小于1KB），可能是文件系统损坏，建议格式化 */
            if (free_bytes < 1024)
            {
                LOG_WARN("TEST", "文件系统空间异常（小于1KB），建议格式化");
                LOG_INFO("TEST", "开始格式化...");
                OLED_ShowString(3, 1, "Re-formatting...");
                littlefs_status = LittleFS_Format();
                if (littlefs_status == LITTLEFS_OK)
                {
                    littlefs_status = LittleFS_Mount();
                }
            }
        }
    }
    
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("TEST", "文件系统挂载成功");
        OLED_ShowString(3, 1, "Mount OK");
        
        /* 显示文件系统信息 */
        uint64_t total_bytes = 0;
        uint64_t free_bytes = 0;
        littlefs_status = LittleFS_GetInfo(&total_bytes, &free_bytes);
        if (littlefs_status == LITTLEFS_OK)
        {
        LOG_INFO("TEST", "FS: %lluKB free/%lluKB total", 
                 (unsigned long long)free_bytes / 1024, 
                 (unsigned long long)total_bytes / 1024);
            
            char buf[17];
            snprintf(buf, sizeof(buf), "Total:%lluKB", (unsigned long long)total_bytes / 1024);
            OLED_ShowString(4, 1, buf);
        }
    }
    else
    {
        LOG_ERROR("TEST", "文件系统挂载失败: %d", littlefs_status);
        OLED_ShowString(3, 1, "Mount Failed");
    }
    
    Delay_ms(2000);
}

/**
 * @brief 测试2：基础文件操作测试
 */
static void test2_basic_file_ops(void)
{
    LittleFS_Status_t littlefs_status;
    lfs_file_t file;
    const char* test_file = "test1.txt";
    const char* test_data = "Hello, LittleFS!";
    
    LOG_INFO("TEST", "=== 测试2：基础文件操作测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test2: Basic Ops");
    
    /* 清零文件句柄 */
    memset(&file, 0, sizeof(file));
    
    /* 创建文件并写入数据（使用TRUNC标志，即使文件已存在也会截断） */
    LOG_INFO("TEST", "创建文件: %s", test_file);
    OLED_ShowString(2, 1, "Creating...");
    littlefs_status = LittleFS_FileOpen(&file, test_file, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (littlefs_status == LITTLEFS_OK)
    {
        uint32_t bytes_written = 0;
        littlefs_status = LittleFS_FileWrite(&file, test_data, strlen(test_data), &bytes_written);
        if (littlefs_status == LITTLEFS_OK)
        {
            LOG_INFO("TEST", "Write: %lu bytes", (unsigned long)bytes_written);
            
            /* 同步文件 */
            littlefs_status = LittleFS_FileSync(&file);
            if (littlefs_status != LITTLEFS_OK)
            {
                LOG_ERROR("TEST", "Sync failed: %d (%s)", littlefs_status, littlefs_errstr(littlefs_status));
                /* 即使同步失败，也尝试关闭文件 */
                LittleFS_FileClose(&file);
                Delay_ms(200);
                /* 如果同步失败，可能是文件系统损坏，建议重新格式化 */
                LOG_ERROR("TEST", "文件同步失败，可能是文件系统损坏");
                LOG_ERROR("TEST", "建议：重新格式化文件系统（在测试1中会自动格式化）");
                OLED_ShowString(2, 1, "Sync Failed");
                OLED_ShowString(3, 1, "FS Corrupted?");
                Delay_ms(2000);
                return;
            }
            
            LittleFS_FileClose(&file);
            Delay_ms(500);  /* 关闭后延时，确保Flash操作完成 */
            OLED_ShowString(2, 1, "Write OK");
        }
        else
        {
            LOG_ERROR("TEST", "写入失败: %d", littlefs_status);
            OLED_ShowString(2, 1, "Write Failed");
            LittleFS_FileClose(&file);
            return;
        }
    }
    else
    {
        LOG_ERROR("TEST", "创建文件失败: %d (%s)", littlefs_status, littlefs_errstr(littlefs_status));
        if (littlefs_status == LITTLEFS_ERROR_NOSPC)
        {
            LOG_ERROR("TEST", "错误原因：文件系统空间不足");
            check_filesystem_status();
        }
        OLED_ShowString(2, 1, "Create Failed");
        return;
    }
    
    Delay_ms(500);  /* 增加延时，确保Flash写入完成 */
    
    /* 检查文件是否存在 */
    lfs_t* lfs = LittleFS_GetLFS(LITTLEFS_INSTANCE_0);
    if (lfs != NULL) {
        struct lfs_info info;
        int stat_err = lfs_stat(lfs, test_file, &info);
        if (stat_err == 0) {
            LOG_INFO("TEST", "文件存在: name='%s' size=%lu", info.name, (unsigned long)info.size);
        } else {
            LOG_ERROR("TEST", "文件不存在或stat失败: %d", stat_err);
            OLED_ShowString(3, 1, "File Not Found");
            Delay_ms(1000);
            return;
        }
    }
    else
    {
        LOG_ERROR("TEST", "获取lfs_t指针失败");
        OLED_ShowString(3, 1, "LFS NULL");
        return;
    }
    
    /* 读取并验证数据 */
    LOG_INFO("TEST", "读取文件并验证...");
    OLED_ShowString(3, 1, "Reading...");
    
    /* 再次清零文件句柄，确保干净 */
    memset(&file, 0, sizeof(file));
    char read_buffer[64] = {0};
    uint32_t bytes_read = 0;
    
    /* 增加延时，确保文件系统状态稳定 */
    Delay_ms(200);
    
    littlefs_status = LittleFS_FileOpen(&file, test_file, LFS_O_RDONLY);
    if (littlefs_status == LITTLEFS_OK)
    {
        littlefs_status = LittleFS_FileRead(&file, read_buffer, sizeof(read_buffer) - 1, &bytes_read);
        if (littlefs_status == LITTLEFS_OK)
        {
            read_buffer[bytes_read] = '\0';
            LOG_INFO("TEST", "Read: %lu bytes", (unsigned long)bytes_read);
            
            if (strcmp(read_buffer, test_data) == 0)
            {
                LOG_INFO("TEST", "Verify OK");
                OLED_ShowString(3, 1, "Verify OK");
            }
            else
            {
                LOG_ERROR("TEST", "Verify Failed");
                OLED_ShowString(3, 1, "Verify Failed");
            }
        }
        else
        {
            LOG_ERROR("TEST", "读取失败: %d", littlefs_status);
            OLED_ShowString(3, 1, "Read Failed");
        }
        LittleFS_FileClose(&file);
    }
    else
    {
        LOG_ERROR("TEST", "打开文件失败: %d", littlefs_status);
        OLED_ShowString(3, 1, "Open Failed");
    }
    
    Delay_ms(2000);
}

/**
 * @brief 测试3：文件定位测试
 */
static void test3_file_seek(void)
{
    LittleFS_Status_t littlefs_status;
    lfs_file_t file;
    const char* test_file = "test1.txt";
    
    LOG_INFO("TEST", "=== 测试3：文件定位测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test3: File Seek");
    
    /* 先检查文件是否存在 */
    lfs_t* lfs = LittleFS_GetLFS(LITTLEFS_INSTANCE_0);
    if (lfs != NULL) {
        struct lfs_info info;
        int stat_err = lfs_stat(lfs, test_file, &info);
        if (stat_err != 0) {
            LOG_ERROR("TEST", "文件不存在: %s (stat_err=%d)", test_file, stat_err);
            OLED_ShowString(2, 1, "File Not Found");
            return;
        }
        LOG_INFO("TEST", "文件存在: name='%s' size=%lu", info.name, (unsigned long)info.size);
    }
    else
    {
        LOG_ERROR("TEST", "获取lfs_t指针失败");
        OLED_ShowString(2, 1, "LFS NULL");
        return;
    }
    
    /* 清零文件句柄 */
    memset(&file, 0, sizeof(file));
    
    /* 增加延时，确保文件系统状态稳定 */
    Delay_ms(200);
    
    /* 打开文件 */
    littlefs_status = LittleFS_FileOpen(&file, test_file, LFS_O_RDONLY);
    if (littlefs_status != LITTLEFS_OK)
    {
        LOG_ERROR("TEST", "打开文件失败: %d (%s)", littlefs_status, littlefs_errstr(littlefs_status));
        OLED_ShowString(2, 1, "Open Failed");
        return;
    }
    
    /* 获取文件大小 */
    uint32_t file_size = 0;
    littlefs_status = LittleFS_FileSize(&file, &file_size);
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("TEST", "Size: %lu bytes", (unsigned long)file_size);
    }
    
    /* SEEK_SET：定位到文件开头 */
    OLED_ShowString(2, 1, "Seek SET...");
    littlefs_status = LittleFS_FileSeek(&file, 0, LFS_SEEK_SET);
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("TEST", "Seek SET OK");
    }
    
    /* SEEK_CUR：定位到中间位置 */
    if (file_size > 5)
    {
        OLED_ShowString(3, 1, "Seek CUR...");
        int32_t offset = (int32_t)(file_size / 2);
        littlefs_status = LittleFS_FileSeek(&file, offset, LFS_SEEK_CUR);
        if (littlefs_status == LITTLEFS_OK)
        {
            LOG_INFO("TEST", "Seek CUR OK");
        }
    }
    
    /* SEEK_END：定位到文件末尾 */
    OLED_ShowString(4, 1, "Seek END...");
    littlefs_status = LittleFS_FileSeek(&file, 0, LFS_SEEK_END);
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("TEST", "Seek END OK");
        OLED_ShowString(4, 1, "Seek OK");
    }
    
    LittleFS_FileClose(&file);
    Delay_ms(2000);
}

/**
 * @brief 测试4：追加写入测试
 */
static void test4_append_write(void)
{
    LittleFS_Status_t littlefs_status;
    lfs_file_t file;
    const char* test_file = "test2.txt";
    const char* line1 = "Line 1\n";
    const char* line2 = "Line 2\n";
    
    LOG_INFO("TEST", "=== 测试4：追加写入测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test4: Append");
    
    memset(&file, 0, sizeof(file));
    
    /* 创建文件并写入第一行（使用TRUNC标志，确保是新文件） */
    OLED_ShowString(2, 1, "Write Line1...");
    littlefs_status = LittleFS_FileOpen(&file, test_file, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (littlefs_status == LITTLEFS_OK)
    {
        uint32_t bytes_written = 0;
        littlefs_status = LittleFS_FileWrite(&file, line1, strlen(line1), &bytes_written);
        if (littlefs_status == LITTLEFS_OK)
        {
            littlefs_status = LittleFS_FileSync(&file);
            if (littlefs_status != LITTLEFS_OK)
            {
                LOG_ERROR("TEST", "Sync failed: %d", littlefs_status);
                LittleFS_FileClose(&file);
                OLED_ShowString(2, 1, "Sync Failed");
                return;
            }
        }
        LittleFS_FileClose(&file);
        Delay_ms(200);
    }
    else
    {
        LOG_ERROR("TEST", "Create failed: %d", littlefs_status);
        OLED_ShowString(2, 1, "Create Failed");
        return;
    }
    
    /* 使用APPEND模式追加第二行 */
    OLED_ShowString(3, 1, "Append...");
    memset(&file, 0, sizeof(file));
    /* 使用APPEND | CREAT，确保文件存在时可以追加，不存在时创建 */
    littlefs_status = LittleFS_FileOpen(&file, test_file, LFS_O_WRONLY | LFS_O_APPEND | LFS_O_CREAT);
    if (littlefs_status == LITTLEFS_OK)
    {
        uint32_t bytes_written = 0;
        littlefs_status = LittleFS_FileWrite(&file, line2, strlen(line2), &bytes_written);
        if (littlefs_status == LITTLEFS_OK)
        {
            littlefs_status = LittleFS_FileSync(&file);
            if (littlefs_status != LITTLEFS_OK)
            {
                LOG_ERROR("TEST", "Append sync failed: %d", littlefs_status);
                LittleFS_FileClose(&file);
                OLED_ShowString(3, 1, "Sync Failed");
                return;
            }
        }
        LittleFS_FileClose(&file);
        Delay_ms(200);
        OLED_ShowString(3, 1, "Append OK");
    }
    else
    {
        LOG_ERROR("TEST", "Append failed: %d", littlefs_status);
        OLED_ShowString(3, 1, "Append Failed");
        return;
    }
    
    Delay_ms(200);
    
    /* 读取并验证完整内容 */
    memset(&file, 0, sizeof(file));
    char read_buffer[64] = {0};
    uint32_t bytes_read = 0;
    
    littlefs_status = LittleFS_FileOpen(&file, test_file, LFS_O_RDONLY);
    if (littlefs_status == LITTLEFS_OK)
    {
        LittleFS_FileRead(&file, read_buffer, sizeof(read_buffer) - 1, &bytes_read);
        read_buffer[bytes_read] = '\0';
        
        if (strstr(read_buffer, line1) != NULL && strstr(read_buffer, line2) != NULL)
        {
            LOG_INFO("TEST", "Verify OK");
            OLED_ShowString(4, 1, "Verify OK");
        }
        else
        {
            LOG_ERROR("TEST", "Verify Failed");
            OLED_ShowString(4, 1, "Verify Failed");
        }
        LittleFS_FileClose(&file);
    }
    
    Delay_ms(2000);
}

/**
 * @brief 测试5：文件截断测试
 */
static void test5_file_truncate(void)
{
    LittleFS_Status_t littlefs_status;
    lfs_file_t file;
    const char* test_file = "test3.txt";
    uint8_t large_data[512];
    
    LOG_INFO("TEST", "=== 测试5：文件截断测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test5: Truncate");
    
    /* 初始化大量数据 */
    for (int i = 0; i < 512; i++)
    {
        large_data[i] = (uint8_t)(i & 0xFF);
    }
    
    memset(&file, 0, sizeof(file));
    
    /* 创建文件并写入大量数据 */
    OLED_ShowString(2, 1, "Writing 512B...");
    littlefs_status = LittleFS_FileOpen(&file, test_file, LFS_O_WRONLY | LFS_O_CREAT);
    if (littlefs_status == LITTLEFS_OK)
    {
        uint32_t bytes_written = 0;
        LittleFS_FileWrite(&file, large_data, sizeof(large_data), &bytes_written);
        LittleFS_FileSync(&file);
        LittleFS_FileClose(&file);
    }
    else
    {
        LOG_ERROR("TEST", "Create failed: %d", littlefs_status);
        OLED_ShowString(2, 1, "Create Failed");
        return;
    }
    
    Delay_ms(200);
    
    /* 截断文件到128字节 */
    OLED_ShowString(3, 1, "Truncate...");
    memset(&file, 0, sizeof(file));
    littlefs_status = LittleFS_FileOpen(&file, test_file, LFS_O_RDWR);
    if (littlefs_status == LITTLEFS_OK)
    {
        LittleFS_FileSeek(&file, 0, LFS_SEEK_SET);
        littlefs_status = LittleFS_FileTruncate(&file, 128);
        if (littlefs_status == LITTLEFS_OK)
        {
            LittleFS_FileSync(&file);
            LittleFS_FileClose(&file);
            OLED_ShowString(3, 1, "Truncate OK");
        }
        else
        {
            LOG_ERROR("TEST", "Truncate failed: %d", littlefs_status);
            OLED_ShowString(3, 1, "Truncate Failed");
            LittleFS_FileClose(&file);
            return;
        }
    }
    
    Delay_ms(200);
    
    /* 验证文件大小 */
    memset(&file, 0, sizeof(file));
    littlefs_status = LittleFS_FileOpen(&file, test_file, LFS_O_RDONLY);
    if (littlefs_status == LITTLEFS_OK)
    {
        uint32_t file_size = 0;
        littlefs_status = LittleFS_FileSize(&file, &file_size);
        if (littlefs_status == LITTLEFS_OK)
        {
            if (file_size == 128)
            {
                LOG_INFO("TEST", "Size OK: %lu", (unsigned long)file_size);
                OLED_ShowString(4, 1, "Size OK");
            }
            else
            {
                LOG_ERROR("TEST", "Size failed: %lu", (unsigned long)file_size);
                OLED_ShowString(4, 1, "Size Failed");
            }
        }
        LittleFS_FileClose(&file);
    }
    
    Delay_ms(2000);
}

/**
 * @brief 测试6：文件重命名测试
 */
static void test6_file_rename(void)
{
    LittleFS_Status_t littlefs_status;
    lfs_file_t file;
    const char* old_file = "old.txt";
    const char* new_file = "new.txt";
    const char* test_data = "Rename Test";
    
    LOG_INFO("TEST", "=== 测试6：文件重命名测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test6: Rename");
    
    memset(&file, 0, sizeof(file));
    
    /* 先检查文件是否已存在，如果存在则删除 */
    lfs_t* lfs = LittleFS_GetLFS(LITTLEFS_INSTANCE_0);
    if (lfs != NULL) {
        struct lfs_info info;
        int stat_err = lfs_stat(lfs, old_file, &info);
        if (stat_err == 0) {
            LOG_INFO("TEST", "文件已存在，先删除: %s", old_file);
            LittleFS_FileDelete(old_file);
            Delay_ms(100);
        }
    }
    
    /* 创建文件 */
    OLED_ShowString(2, 1, "Creating...");
    littlefs_status = LittleFS_FileOpen(&file, old_file, LFS_O_WRONLY | LFS_O_CREAT);
    if (littlefs_status == LITTLEFS_OK)
    {
        uint32_t bytes_written = 0;
        littlefs_status = LittleFS_FileWrite(&file, test_data, strlen(test_data), &bytes_written);
        if (littlefs_status == LITTLEFS_OK)
        {
            LittleFS_FileSync(&file);
        }
        LittleFS_FileClose(&file);
    }
    else
    {
        LOG_ERROR("TEST", "Create failed: %d", littlefs_status);
        OLED_ShowString(2, 1, "Create Failed");
        return;
    }
    
    Delay_ms(200);
    
    /* 重命名文件 */
    OLED_ShowString(3, 1, "Renaming...");
    littlefs_status = LittleFS_FileRename(old_file, new_file);
    if (littlefs_status == LITTLEFS_OK)
    {
        OLED_ShowString(3, 1, "Rename OK");
    }
    else
    {
        LOG_ERROR("TEST", "Rename failed: %d", littlefs_status);
        OLED_ShowString(3, 1, "Rename Failed");
        return;
    }
    
    Delay_ms(200);
    
    /* 验证旧文件不存在，新文件存在 */
    if (lfs != NULL)
    {
        struct lfs_info info;
        
        if (lfs_stat(lfs, old_file, &info) != 0)
        {
            LOG_INFO("TEST", "Old removed OK");
        }
        
        if (lfs_stat(lfs, new_file, &info) == 0)
        {
            /* 读取新文件验证内容 */
            memset(&file, 0, sizeof(file));
            littlefs_status = LittleFS_FileOpen(&file, new_file, LFS_O_RDONLY);
            if (littlefs_status == LITTLEFS_OK)
            {
                char read_buffer[64] = {0};
                uint32_t bytes_read = 0;
                LittleFS_FileRead(&file, read_buffer, sizeof(read_buffer) - 1, &bytes_read);
                read_buffer[bytes_read] = '\0';
                
                if (strcmp(read_buffer, test_data) == 0)
                {
                    LOG_INFO("TEST", "Verify OK");
                    OLED_ShowString(4, 1, "Verify OK");
                }
                else
                {
                    LOG_ERROR("TEST", "Verify Failed");
                    OLED_ShowString(4, 1, "Verify Failed");
                }
                LittleFS_FileClose(&file);
            }
        }
        else
        {
            LOG_ERROR("TEST", "New not found");
            OLED_ShowString(4, 1, "Not Found");
        }
    }
    
    Delay_ms(2000);
}

/**
 * @brief 测试7：文件删除测试
 */
static void test7_file_delete(void)
{
    LittleFS_Status_t littlefs_status;
    lfs_file_t file;
    const char* test_file = "delete.txt";
    const char* test_data = "Delete Test";
    
    LOG_INFO("TEST", "=== 测试7：文件删除测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test7: Delete");
    
    memset(&file, 0, sizeof(file));
    
    /* 创建文件 */
    OLED_ShowString(2, 1, "Creating...");
    littlefs_status = LittleFS_FileOpen(&file, test_file, LFS_O_WRONLY | LFS_O_CREAT);
    if (littlefs_status == LITTLEFS_OK)
    {
        uint32_t bytes_written = 0;
        LittleFS_FileWrite(&file, test_data, strlen(test_data), &bytes_written);
        LittleFS_FileSync(&file);
        LittleFS_FileClose(&file);
        OLED_ShowString(2, 1, "Created OK");
    }
    else
    {
        LOG_ERROR("TEST", "Create failed: %d", littlefs_status);
        OLED_ShowString(2, 1, "Create Failed");
        return;
    }
    
    Delay_ms(200);
    
    /* 删除文件 */
    OLED_ShowString(3, 1, "Deleting...");
    littlefs_status = LittleFS_FileDelete(test_file);
    if (littlefs_status == LITTLEFS_OK)
    {
        OLED_ShowString(3, 1, "Delete OK");
    }
    else
    {
        LOG_ERROR("TEST", "Delete failed: %d", littlefs_status);
        OLED_ShowString(3, 1, "Delete Failed");
        return;
    }
    
    Delay_ms(200);
    
    /* 验证文件不存在 */
    lfs_t* lfs = LittleFS_GetLFS(LITTLEFS_INSTANCE_0);
    if (lfs != NULL)
    {
        struct lfs_info info;
        if (lfs_stat(lfs, test_file, &info) != 0)
        {
            LOG_INFO("TEST", "Verify OK");
            OLED_ShowString(4, 1, "Verify OK");
        }
        else
        {
            LOG_ERROR("TEST", "Verify Failed");
            OLED_ShowString(4, 1, "Verify Failed");
        }
        
        /* 尝试打开文件应该失败 */
        memset(&file, 0, sizeof(file));
        littlefs_status = LittleFS_FileOpen(&file, test_file, LFS_O_RDONLY);
        if (littlefs_status == LITTLEFS_ERROR_NOENT || littlefs_status == -3905)
        {
            LOG_INFO("TEST", "NOENT OK");
        }
    }
    
    Delay_ms(2000);
}

/**
 * @brief 测试8：目录操作测试
 */
static void test8_directory_ops(void)
{
    LittleFS_Status_t littlefs_status;
    lfs_file_t file;
    lfs_dir_t dir;
    const char* test_dir = "testdir";
    const char* test_file = "testdir/file.txt";
    const char* test_data = "Directory Test";
    
    LOG_INFO("TEST", "=== 测试8：目录操作测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test8: Directory");
    
    /* 创建目录 */
    OLED_ShowString(2, 1, "Creating Dir...");
    littlefs_status = LittleFS_DirCreate(test_dir);
    if (littlefs_status == LITTLEFS_OK)
    {
        OLED_ShowString(2, 1, "Dir Created");
    }
    else if (littlefs_status == LITTLEFS_ERROR_EXIST)
    {
        OLED_ShowString(2, 1, "Dir Exists");
    }
    else
    {
        LOG_ERROR("TEST", "Create dir failed: %d", littlefs_status);
        OLED_ShowString(2, 1, "Create Failed");
        return;
    }
    
    Delay_ms(200);
    
    /* 在目录中创建文件 */
    OLED_ShowString(3, 1, "Creating File...");
    memset(&file, 0, sizeof(file));
    littlefs_status = LittleFS_FileOpen(&file, test_file, LFS_O_WRONLY | LFS_O_CREAT);
    if (littlefs_status == LITTLEFS_OK)
    {
        uint32_t bytes_written = 0;
        LittleFS_FileWrite(&file, test_data, strlen(test_data), &bytes_written);
        LittleFS_FileSync(&file);
        LittleFS_FileClose(&file);
        OLED_ShowString(3, 1, "File Created");
    }
    else
    {
        LOG_ERROR("TEST", "Create file failed: %d", littlefs_status);
        OLED_ShowString(3, 1, "File Failed");
        return;
    }
    
    Delay_ms(200);
    
    /* 遍历目录，列出所有文件 */
    OLED_ShowString(4, 1, "Listing...");
    memset(&dir, 0, sizeof(dir));
    struct lfs_info info;
    
    littlefs_status = LittleFS_DirOpen(&dir, test_dir);
    if (littlefs_status == LITTLEFS_OK)
    {
        int count = 0;
        while (1)
        {
            memset(&info, 0, sizeof(info));
            littlefs_status = LittleFS_DirRead(&dir, &info);
            if (littlefs_status == LITTLEFS_OK)
            {
                count++;
            }
            else if (littlefs_status == LITTLEFS_ERROR_NOENT)
            {
                break;
            }
            else
            {
                break;
            }
        }
        LittleFS_DirClose(&dir);
        LOG_INFO("TEST", "Dir entries: %d", count);
        
        char buf[17];
        snprintf(buf, sizeof(buf), "Count:%d", count);
        OLED_ShowString(4, 1, buf);
    }
    else
    {
        LOG_ERROR("TEST", "Open dir failed: %d", littlefs_status);
        OLED_ShowString(4, 1, "Open Failed");
    }
    
    Delay_ms(500);
    
    /* 删除目录中的文件 */
    LittleFS_FileDelete(test_file);
    Delay_ms(100);
    
    /* 删除目录 */
    LittleFS_DirDelete(test_dir);
    
    Delay_ms(2000);
}

/**
 * @brief 测试9：文件属性测试
 */
static void test9_file_attributes(void)
{
    LittleFS_Status_t littlefs_status;
    lfs_file_t file;
    const char* test_file = "attr.txt";
    const char* test_data = "Attribute Test";
    uint32_t version = 0x01020304;
    uint32_t create_time = 1234567890;
    uint8_t attr_type_version = 0x10;  /* 用户自定义属性类型：版本号 */
    uint8_t attr_type_time = 0x11;     /* 用户自定义属性类型：创建时间 */
    
    LOG_INFO("TEST", "=== 测试9：文件属性测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test9: Attributes");
    
    memset(&file, 0, sizeof(file));
    
    /* 创建文件 */
    LOG_INFO("TEST", "创建文件: %s", test_file);
    OLED_ShowString(2, 1, "Creating...");
    littlefs_status = LittleFS_FileOpen(&file, test_file, LFS_O_WRONLY | LFS_O_CREAT);
    if (littlefs_status == LITTLEFS_OK)
    {
        uint32_t bytes_written = 0;
        LittleFS_FileWrite(&file, test_data, strlen(test_data), &bytes_written);
        LittleFS_FileSync(&file);
        LittleFS_FileClose(&file);
        LOG_INFO("TEST", "文件创建成功");
    }
    else
    {
        LOG_ERROR("TEST", "创建文件失败: %d (%s)", littlefs_status, littlefs_errstr(littlefs_status));
        if (littlefs_status == LITTLEFS_ERROR_NOSPC)
        {
            LOG_ERROR("TEST", "错误原因：文件系统空间不足");
            check_filesystem_status();
        }
        OLED_ShowString(2, 1, "Create Failed");
        return;
    }
    
    Delay_ms(200);  /* 增加延时，确保Flash写入完成 */
    
    /* 设置文件属性：版本号 */
    LOG_INFO("TEST", "设置文件属性：版本号 = 0x%08X", version);
    OLED_ShowString(3, 1, "Set Attr...");
    littlefs_status = LittleFS_FileSetAttr(test_file, attr_type_version, &version, sizeof(version));
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("TEST", "版本号属性设置成功");
    }
    else
    {
        LOG_ERROR("TEST", "版本号属性设置失败: %d", littlefs_status);
        OLED_ShowString(3, 1, "Set Failed");
    }
    
    /* 设置文件属性：创建时间 */
    LOG_INFO("TEST", "设置文件属性：创建时间 = %lu", (unsigned long)create_time);
    littlefs_status = LittleFS_FileSetAttr(test_file, attr_type_time, &create_time, sizeof(create_time));
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("TEST", "创建时间属性设置成功");
    }
    else
    {
        LOG_ERROR("TEST", "创建时间属性设置失败: %d", littlefs_status);
    }
    
    Delay_ms(200);  /* 增加延时，确保Flash写入完成 */
    
    /* 获取文件属性并验证 */
    LOG_INFO("TEST", "获取文件属性并验证...");
    uint32_t read_version = 0;
    uint32_t read_time = 0;
    uint32_t actual_size = 0;
    
    littlefs_status = LittleFS_FileGetAttr(test_file, attr_type_version, &read_version, sizeof(read_version), &actual_size);
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("TEST", "版本号属性: 0x%08X (期望: 0x%08X)", read_version, version);
        if (read_version == version)
        {
            LOG_INFO("TEST", "版本号验证成功");
        }
        else
        {
            LOG_ERROR("TEST", "版本号验证失败");
        }
    }
    else
    {
        LOG_ERROR("TEST", "获取版本号属性失败: %d", littlefs_status);
    }
    
    littlefs_status = LittleFS_FileGetAttr(test_file, attr_type_time, &read_time, sizeof(read_time), &actual_size);
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("TEST", "创建时间属性: %lu (期望: %lu)", (unsigned long)read_time, (unsigned long)create_time);
        if (read_time == create_time)
        {
            LOG_INFO("TEST", "创建时间验证成功");
            OLED_ShowString(4, 1, "Verify OK");
        }
        else
        {
            LOG_ERROR("TEST", "创建时间验证失败");
            OLED_ShowString(4, 1, "Verify Failed");
        }
    }
    else
    {
        LOG_ERROR("TEST", "获取创建时间属性失败: %d", littlefs_status);
    }
    
    Delay_ms(1000);
    
    /* 移除文件属性 */
    LOG_INFO("TEST", "移除文件属性...");
    littlefs_status = LittleFS_FileRemoveAttr(test_file, attr_type_version);
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("TEST", "版本号属性移除成功");
    }
    
    /* 验证属性已移除 */
    read_version = 0;
    littlefs_status = LittleFS_FileGetAttr(test_file, attr_type_version, &read_version, sizeof(read_version), &actual_size);
    if (littlefs_status != LITTLEFS_OK)
    {
        LOG_INFO("TEST", "版本号属性已移除（正确）");
    }
    else
    {
        LOG_ERROR("TEST", "版本号属性仍然存在（错误）");
    }
    
    Delay_ms(2000);
}

/**
 * @brief 测试10：原子操作测试
 */
static void test10_atomic_operations(void)
{
    LittleFS_Status_t littlefs_status;
    lfs_file_t file;
    const char* test_file = "atomic.txt";
    const char* test_data = "Atomic Write Test Data";
    uint8_t attr_type_checksum = 0x12;  /* 用户自定义属性类型：校验和 */
    
    LOG_INFO("TEST", "=== 测试10：原子操作测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test10: Atomic");
    
    /* 检查文件系统状态 */
    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;
    littlefs_status = LittleFS_GetInfo(&total_bytes, &free_bytes);
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("TEST", "FS状态: %lluKB free/%lluKB total", 
                 (unsigned long long)free_bytes / 1024, 
                 (unsigned long long)total_bytes / 1024);
    }
    
    /* 可选：在测试10前重新格式化（解决NOSPC问题） */
    #if CONFIG_LITTLEFS_REFORMAT_BEFORE_TEST10
    LOG_INFO("TEST", "测试10前重新格式化（解决NOSPC问题）...");
    OLED_ShowString(2, 1, "Re-formatting...");
    LittleFS_Unmount();
    Delay_ms(200);
    littlefs_status = LittleFS_Format();
    if (littlefs_status == LITTLEFS_OK)
    {
        Delay_ms(200);
        littlefs_status = LittleFS_Mount();
        if (littlefs_status == LITTLEFS_OK)
        {
            LOG_INFO("TEST", "重新格式化并挂载成功");
            OLED_ShowString(2, 1, "Re-format OK");
        }
        else
        {
            LOG_ERROR("TEST", "重新挂载失败: %d", littlefs_status);
            OLED_ShowString(2, 1, "Remount Failed");
            return;
        }
    }
    else
    {
        LOG_ERROR("TEST", "重新格式化失败: %d", littlefs_status);
        OLED_ShowString(2, 1, "Re-format Failed");
        return;
    }
    Delay_ms(500);
    #endif
    
    /* 方式1：原子写入演示 */
    LOG_INFO("TEST", "方式1：原子写入演示");
    OLED_ShowString(2, 1, "Atomic Write...");
    
    lfs_t* lfs = LittleFS_GetLFS(LITTLEFS_INSTANCE_0);
    
    memset(&file, 0, sizeof(file));
    /* 使用TRUNC标志，确保创建新文件 */
    littlefs_status = LittleFS_FileOpen(&file, test_file, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (littlefs_status == LITTLEFS_OK)
    {
        uint32_t bytes_written = 0;
        littlefs_status = LittleFS_FileWrite(&file, test_data, strlen(test_data), &bytes_written);
        if (littlefs_status == LITTLEFS_OK)
        {
            LOG_INFO("TEST", "写入成功: %lu 字节", (unsigned long)bytes_written);
            
            /* 立即同步，确保数据落盘 */
            littlefs_status = LittleFS_FileSync(&file);
            if (littlefs_status == LITTLEFS_OK)
            {
                LOG_INFO("TEST", "原子写入成功（已同步）");
                OLED_ShowString(2, 1, "Atomic OK");
                Delay_ms(1000);  /* 同步后延时，确保Flash操作完成 */
            }
            else
            {
                LOG_ERROR("TEST", "原子写入失败（同步失败）: %d (%s)", littlefs_status, littlefs_errstr(littlefs_status));
                OLED_ShowString(2, 1, "Sync Failed");
                LittleFS_FileClose(&file);
                /* 同步失败，跳过方式2测试 */
                Delay_ms(2000);
                return;
            }
        }
        else
        {
            LOG_ERROR("TEST", "写入失败: %d", littlefs_status);
            OLED_ShowString(2, 1, "Write Failed");
            LittleFS_FileClose(&file);
            Delay_ms(2000);
            return;
        }
        LittleFS_FileClose(&file);
    }
    else
    {
        LOG_ERROR("TEST", "打开文件失败: %d", littlefs_status);
        OLED_ShowString(2, 1, "Open Failed");
        Delay_ms(2000);
        return;
    }
    
    Delay_ms(500);  /* 增加延时，确保Flash写入完成 */
    
    /* 方式2：使用文件属性存储校验和 */
    LOG_INFO("TEST", "方式2：使用文件属性存储校验和");
    OLED_ShowString(3, 1, "Checksum Test...");
    
    /* 先检查文件是否存在 */
    if (lfs != NULL) {
        struct lfs_info info;
        if (lfs_stat(lfs, test_file, &info) != 0) {
            LOG_ERROR("TEST", "文件不存在，无法进行校验和测试");
            OLED_ShowString(3, 1, "File Not Found");
            Delay_ms(2000);
            return;
        }
    }
    
    /* 计算数据校验和 */
    uint32_t checksum = 0;
    for (size_t i = 0; i < strlen(test_data); i++)
    {
        checksum += (uint32_t)test_data[i];
    }
    
    /* 将校验和存储到文件属性 */
    littlefs_status = LittleFS_FileSetAttr(test_file, attr_type_checksum, &checksum, sizeof(checksum));
    if (littlefs_status != LITTLEFS_OK)
    {
        LOG_ERROR("TEST", "Set attr failed: %d", littlefs_status);
        OLED_ShowString(3, 1, "Set Failed");
        Delay_ms(2000);
        return;
    }
    
    Delay_ms(200);
    
    /* 读取文件并重新计算校验和 */
    memset(&file, 0, sizeof(file));
    char read_buffer[64] = {0};
    uint32_t bytes_read = 0;
    
    littlefs_status = LittleFS_FileOpen(&file, test_file, LFS_O_RDONLY);
    if (littlefs_status == LITTLEFS_OK)
    {
        littlefs_status = LittleFS_FileRead(&file, read_buffer, sizeof(read_buffer) - 1, &bytes_read);
        if (littlefs_status == LITTLEFS_OK && bytes_read > 0)
        {
            read_buffer[bytes_read] = '\0';
            
            /* 重新计算校验和 */
            uint32_t recalc_checksum = 0;
            for (size_t i = 0; i < bytes_read; i++)
            {
                recalc_checksum += (uint32_t)read_buffer[i];
            }
            
            /* 从文件属性读取存储的校验和 */
            uint32_t stored_checksum = 0;
            uint32_t actual_size = 0;
            littlefs_status = LittleFS_FileGetAttr(test_file, attr_type_checksum, &stored_checksum, sizeof(stored_checksum), &actual_size);
            if (littlefs_status == LITTLEFS_OK)
            {
                if (recalc_checksum == stored_checksum && recalc_checksum == checksum)
                {
                    LOG_INFO("TEST", "Checksum OK");
                    OLED_ShowString(4, 1, "Checksum OK");
                }
                else
                {
                    LOG_ERROR("TEST", "Checksum Failed");
                    OLED_ShowString(4, 1, "Checksum Fail");
                }
            }
            else
            {
                LOG_ERROR("TEST", "Get attr failed: %d", littlefs_status);
                OLED_ShowString(4, 1, "Get Failed");
            }
        }
        else
        {
            LOG_ERROR("TEST", "Read failed: %d", littlefs_status);
            OLED_ShowString(4, 1, "Read Failed");
        }
        LittleFS_FileClose(&file);
    }
    else
    {
        LOG_ERROR("TEST", "Open failed: %d", littlefs_status);
        OLED_ShowString(3, 1, "Open Failed");
    }
    
    Delay_ms(2000);
}

/**
 * @brief 测试11：断电保护测试
 */
static void test11_power_protection(void)
{
    LittleFS_Status_t littlefs_status;
    lfs_file_t file;
    const char* flag_file = "power_test.flag";
    const char* persist_file = "persist.dat";
    const char* test_data = "Power Protection Test Data";
    uint32_t magic_number = 0xDEADBEEF;
    
    LOG_INFO("TEST", "=== 测试11：断电保护测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test11: Power");
    
    lfs_t* lfs = LittleFS_GetLFS(LITTLEFS_INSTANCE_0);
    if (lfs == NULL)
    {
        LOG_ERROR("TEST", "获取lfs_t指针失败");
        OLED_ShowString(2, 1, "LFS NULL");
        return;
    }
    
    /* 方式1：提示用户手动重启 */
    LOG_INFO("TEST", "方式1：提示用户手动重启");
    OLED_ShowString(2, 1, "Method1: Flag");
    
    struct lfs_info info;
    int stat_err = lfs_stat(lfs, flag_file, &info);
    
    if (stat_err != 0)
    {
        /* 首次运行：写入关键数据 */
        OLED_ShowString(3, 1, "First Run");
        
        memset(&file, 0, sizeof(file));
        littlefs_status = LittleFS_FileOpen(&file, persist_file, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
        if (littlefs_status == LITTLEFS_OK)
        {
            uint32_t bytes_written = 0;
            littlefs_status = LittleFS_FileWrite(&file, test_data, strlen(test_data), &bytes_written);
            if (littlefs_status == LITTLEFS_OK)
            {
                littlefs_status = LittleFS_FileSync(&file);
                if (littlefs_status != LITTLEFS_OK)
                {
                    LOG_ERROR("TEST", "同步失败: %d (%s)", littlefs_status, littlefs_errstr(littlefs_status));
                }
                else
                {
                    Delay_ms(1000);  /* 同步成功后延时，确保Flash操作完成 */
                }
            }
            LittleFS_FileClose(&file);
            Delay_ms(200);
        }
        
        /* 创建标记文件 */
        memset(&file, 0, sizeof(file));
        littlefs_status = LittleFS_FileOpen(&file, flag_file, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
        if (littlefs_status == LITTLEFS_OK)
        {
            const char* flag_data = "FLAG";
            uint32_t bytes_written = 0;
            littlefs_status = LittleFS_FileWrite(&file, flag_data, strlen(flag_data), &bytes_written);
            if (littlefs_status == LITTLEFS_OK)
            {
                littlefs_status = LittleFS_FileSync(&file);
                if (littlefs_status != LITTLEFS_OK)
                {
                    LOG_ERROR("TEST", "标记文件同步失败: %d (%s)", littlefs_status, littlefs_errstr(littlefs_status));
                }
                else
                {
                    Delay_ms(1000);  /* 同步成功后延时，确保Flash操作完成 */
                }
            }
            LittleFS_FileClose(&file);
            Delay_ms(200);
        }
        
        LOG_INFO("TEST", "Please restart");
        OLED_ShowString(4, 1, "Restart Now!");
        Delay_ms(3000);
    }
    else
    {
        /* 第二次运行：验证数据 */
        OLED_ShowString(3, 1, "Second Run");
        
        memset(&file, 0, sizeof(file));
        littlefs_status = LittleFS_FileOpen(&file, persist_file, LFS_O_RDONLY);
        if (littlefs_status == LITTLEFS_OK)
        {
            char read_buffer[64] = {0};
            uint32_t bytes_read = 0;
            LittleFS_FileRead(&file, read_buffer, sizeof(read_buffer) - 1, &bytes_read);
            read_buffer[bytes_read] = '\0';
            LittleFS_FileClose(&file);
            
            if (strcmp(read_buffer, test_data) == 0)
            {
                LOG_INFO("TEST", "Protect OK");
                OLED_ShowString(4, 1, "Protect OK");
            }
            else
            {
                LOG_ERROR("TEST", "Protect Failed");
                OLED_ShowString(4, 1, "Protect Failed");
            }
        }
        
        /* 删除标记文件 */
        LittleFS_FileDelete(flag_file);
    }
    
    Delay_ms(200);  /* 增加延时，确保Flash写入完成 */
    
    /* 方式2：自动检测首次/二次运行 */
    LOG_INFO("TEST", "方式2：自动检测首次/二次运行");
    OLED_Clear();
    OLED_ShowString(1, 1, "Method2: Auto");
    
    /* 先检查并删除旧文件（如果存在），确保干净状态 */
    stat_err = lfs_stat(lfs, persist_file, &info);
    if (stat_err == 0)
    {
        LOG_INFO("TEST", "检测到旧文件，先删除: %s", persist_file);
        LittleFS_FileDelete(persist_file);
        Delay_ms(100);
    }
    
    stat_err = lfs_stat(lfs, persist_file, &info);
    if (stat_err != 0)
    {
        /* 首次运行：写入标记数据和测试数据 */
        LOG_INFO("TEST", "首次运行：写入标记数据...");
        OLED_ShowString(2, 1, "First Run");
        
        memset(&file, 0, sizeof(file));
        littlefs_status = LittleFS_FileOpen(&file, persist_file, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
        if (littlefs_status == LITTLEFS_OK)
        {
            uint32_t bytes_written = 0;
            /* 写入标记数据 */
            littlefs_status = LittleFS_FileWrite(&file, &magic_number, sizeof(magic_number), &bytes_written);
            if (littlefs_status == LITTLEFS_OK && bytes_written == sizeof(magic_number))
            {
                /* 写入测试数据 */
                bytes_written = 0;
                littlefs_status = LittleFS_FileWrite(&file, test_data, strlen(test_data), &bytes_written);
                if (littlefs_status == LITTLEFS_OK)
                {
                    littlefs_status = LittleFS_FileSync(&file);
                    if (littlefs_status == LITTLEFS_OK)
                    {
                        LOG_INFO("TEST", "标记数据和测试数据写入成功");
                        OLED_ShowString(3, 1, "Write OK");
                        Delay_ms(1000);  /* 同步成功后延时，确保Flash操作完成 */
                    }
                    else
                    {
                        LOG_ERROR("TEST", "同步失败: %d (%s)", littlefs_status, littlefs_errstr(littlefs_status));
                        OLED_ShowString(3, 1, "Sync Failed");
                    }
                }
                else
                {
                    LOG_ERROR("TEST", "写入测试数据失败: %d", littlefs_status);
                    OLED_ShowString(3, 1, "Write Failed");
                }
            }
            else
            {
                LOG_ERROR("TEST", "写入标记数据失败: status=%d, bytes=%lu", 
                         littlefs_status, (unsigned long)bytes_written);
                OLED_ShowString(3, 1, "Write Magic Failed");
            }
            LittleFS_FileClose(&file);
            Delay_ms(200);
        }
        else
        {
            LOG_ERROR("TEST", "创建文件失败: %d", littlefs_status);
            OLED_ShowString(3, 1, "Create Failed");
        }
    }
    else
    {
        /* 第二次运行：读取并验证 */
        LOG_INFO("TEST", "第二次运行：读取并验证...");
        OLED_ShowString(2, 1, "Second Run");
        
        memset(&file, 0, sizeof(file));
        littlefs_status = LittleFS_FileOpen(&file, persist_file, LFS_O_RDONLY);
        if (littlefs_status == LITTLEFS_OK)
        {
            uint32_t read_magic = 0;
            uint32_t bytes_read = 0;
            
            /* 读取标记数据 */
            bytes_read = 0;
            littlefs_status = LittleFS_FileRead(&file, &read_magic, sizeof(read_magic), &bytes_read);
            if (littlefs_status != LITTLEFS_OK)
            {
                LOG_ERROR("TEST", "读取标记数据失败: status=%d", littlefs_status);
                LittleFS_FileClose(&file);
                OLED_ShowString(3, 1, "Read Failed");
                return;
            }
            if (bytes_read != sizeof(read_magic))
            {
                LOG_ERROR("TEST", "读取标记数据不完整: bytes_read=%lu (期望=%lu)", 
                         (unsigned long)bytes_read, (unsigned long)sizeof(read_magic));
                LittleFS_FileClose(&file);
                OLED_ShowString(3, 1, "Read Incomplete");
                return;
            }
            
            if (read_magic == magic_number)
            {
                LOG_INFO("TEST", "标记数据匹配: 0x%08X", read_magic);
                
                /* 读取测试数据 */
                char read_buffer[64] = {0};
                littlefs_status = LittleFS_FileRead(&file, read_buffer, sizeof(read_buffer) - 1, &bytes_read);
                if (littlefs_status == LITTLEFS_OK && bytes_read > 0)
                {
                    read_buffer[bytes_read] = '\0';
                }
                else
                {
                    LOG_ERROR("TEST", "读取测试数据失败: status=%d, bytes_read=%lu", 
                             littlefs_status, (unsigned long)bytes_read);
                    LittleFS_FileClose(&file);
                    OLED_ShowString(3, 1, "Read Failed");
                    return;
                }
                
                if (strcmp(read_buffer, test_data) == 0)
                {
                    LOG_INFO("TEST", "测试数据验证成功，断电保护正常");
                    OLED_ShowString(3, 1, "Protect OK");
                }
                else
                {
                    LOG_ERROR("TEST", "测试数据验证失败");
                    OLED_ShowString(3, 1, "Data Failed");
                }
            }
            else
            {
                LOG_ERROR("TEST", "标记数据不匹配: 0x%08X (期望: 0x%08X)", read_magic, magic_number);
                OLED_ShowString(3, 1, "Magic Failed");
            }
            LittleFS_FileClose(&file);
        }
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
    LOG_INFO("MAIN", "=== Flash12 - LittleFS文件系统综合测试案例 ===");
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
        OLED_ShowString(1, 1, "Flash12");
        OLED_ShowString(2, 1, "Initializing...");
        LOG_INFO("MAIN", "OLED 已初始化");
    }
    
    Delay_ms(500);
    
    /* ========== 步骤9：SPI初始化 ========== */
    OLED_ShowString(3, 1, "Init SPI2...");
    
    /* 配置SPI2 NSS引脚为GPIO输出（软件NSS模式，SPI驱动不会自动配置） */
    /* 使用board.h中定义的宏，避免硬编码 */
    GPIO_EnableClock(SPI2_NSS_PORT);
    GPIO_Config(SPI2_NSS_PORT, SPI2_NSS_PIN, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHz);
    GPIO_WritePin(SPI2_NSS_PORT, SPI2_NSS_PIN, Bit_SET);  /* NSS默认拉高（不选中） */
    
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
    
    /* 显示W25Q信息 */
    const w25q_dev_t* dev_info = W25Q_GetInfo();
    if (dev_info != NULL) {
        char buf[17];
        snprintf(buf, sizeof(buf), "Size:%d MB", dev_info->capacity_mb);
        OLED_ShowString(4, 1, buf);
        
        LOG_INFO("MAIN", "W25Q信息:");
        LOG_INFO("MAIN", "  容量: %d MB", dev_info->capacity_mb);
        LOG_INFO("MAIN", "  地址字节数: %d", dev_info->addr_bytes);
        LOG_INFO("MAIN", "  4字节模式: %s", dev_info->is_4byte_mode ? "是" : "否");
    }
    
    Delay_ms(1000);
    
    /* ========== 步骤11：LittleFS初始化 ========== */
    OLED_ShowString(3, 1, "Init LittleFS...");
    LittleFS_Status_t littlefs_status = LittleFS_Init();
    if (littlefs_status != LITTLEFS_OK)
    {
        OLED_ShowString(4, 1, "LittleFS Init Fail!");
        LOG_ERROR("MAIN", "LittleFS 初始化失败: %d", littlefs_status);
        ErrorHandler_Handle(littlefs_status, "LittleFS");
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "LittleFS 初始化成功");
    OLED_ShowString(4, 1, "LittleFS Ready");
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
    
    /* ========== 步骤13：执行11个综合测试 ========== */
    LOG_INFO("MAIN", "=== 开始执行11个综合测试 ===");
    
    /* 先测试底层W25Q Flash是否正常 */
    LOG_INFO("MAIN", "=== 底层Flash硬件测试 ===");
    {
        W25Q_Status_t w25q_status;
        uint8_t test_write[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                  0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
        uint8_t test_read[16] = {0};
        
        /* 擦除扇区0（用于测试） */
        LOG_INFO("MAIN", "擦除扇区0（测试地址）...");
        w25q_status = W25Q_EraseSector(0x10000);  /* 使用16KB地址，避免与LittleFS冲突 */
        if (w25q_status == W25Q_OK)
        {
            W25Q_WaitReady(0);
            LOG_INFO("MAIN", "扇区擦除成功");
            
            /* 写入测试数据 */
            LOG_INFO("MAIN", "写入16字节测试数据...");
            w25q_status = W25Q_Write(0x10000, test_write, sizeof(test_write));
            if (w25q_status == W25Q_OK)
            {
                W25Q_WaitReady(0);
                LOG_INFO("MAIN", "数据写入成功");
                
                /* 读取并验证 */
                Delay_ms(10);
                w25q_status = W25Q_Read(0x10000, test_read, sizeof(test_read));
                if (w25q_status == W25Q_OK)
                {
                    if (memcmp(test_write, test_read, sizeof(test_write)) == 0)
                    {
                        LOG_INFO("MAIN", "底层Flash硬件测试通过");
                    }
                    else
                    {
                        LOG_ERROR("MAIN", "底层Flash数据验证失败！");
                        LOG_ERROR("MAIN", "可能是Flash硬件故障或SPI通信问题");
                    }
                }
                else
                {
                    LOG_ERROR("MAIN", "底层Flash读取失败: %d", w25q_status);
                }
            }
            else
            {
                LOG_ERROR("MAIN", "底层Flash写入失败: %d", w25q_status);
            }
        }
        else
        {
            LOG_ERROR("MAIN", "底层Flash擦除失败: %d", w25q_status);
        }
    }
    Delay_ms(1000);
    
    test1_filesystem_init();
    test2_basic_file_ops();
    test3_file_seek();
    test4_append_write();
    test5_file_truncate();
    test6_file_rename();
    test7_file_delete();
    test8_directory_ops();
    test9_file_attributes();
    test10_atomic_operations();
    test11_power_protection();
    
    /* ========== 步骤14：显示初始化完成 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "Flash12");
    OLED_ShowString(2, 1, "All Tests Done");
    OLED_ShowString(3, 1, "LittleFS Ready");
    LOG_INFO("MAIN", "=== 所有测试完成，进入主循环 ===");
    Delay_ms(1000);
    
    /* ========== 步骤15：主循环 ========== */
    while (1)
    {
        loop_count++;
        LED_Toggle(LED_1);
        
        /* 每10次循环（约5秒）更新一次OLED显示 */
        if (loop_count % 10 == 0) {
            char buf[17];
            snprintf(buf, sizeof(buf), "Running:%lu", (unsigned long)loop_count);
            OLED_ShowString(4, 1, buf);
            LOG_INFO("MAIN", "主循环运行中... (循环 %lu)", (unsigned long)loop_count);
        }
        
        Delay_ms(500);
    }
}

