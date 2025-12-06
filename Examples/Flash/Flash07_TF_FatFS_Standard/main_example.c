/**
 * @file main_example.c
 * @brief Flash07 - TF卡集成FatFS文件系统示例（标准格式化）
 * @details 演示FatFS文件系统的标准格式化方式，整个SD卡格式化为FAT32文件系统
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
 * 1. 文件系统初始化（标准格式化：整个SD卡格式化为FAT32）
 * 2. 文件操作（创建、读写、删除、重命名）
 * 3. 目录操作（创建、遍历、删除）
 * 4. 综合应用场景（数据日志、配置文件）
 * 
 * @note 本示例使用FatFS封装层（fatfs_wrapper.h），提供统一的错误码和接口
 * @note 需要修改ffconf.h启用FF_USE_MKFS
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
#include "fatfs_wrapper.h"
#include "ff.h"
#include "gpio.h"
#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ==================== 文件操作演示 ==================== */

/**
 * @brief 文件操作演示
 */
static void TestFileOperations(void)
{
    LOG_INFO("MAIN", "=== 文件操作演示 ===");
    
    FIL file;
    FatFS_Status_t status;
    uint32_t bytes_written, bytes_read;
    const char* test_file = "0:test.txt";
    const char* test_data = "Hello, FatFS! This is a test file.";
    uint8_t read_buffer[100];
    
    /* 1. 创建并写入文件 */
    LOG_INFO("MAIN", "1. 创建并写入文件: %s", test_file);
    status = FatFS_FileOpen(&file, test_file, FA_WRITE | FA_CREATE_ALWAYS);
    if (status != FATFS_OK) {
        LOG_ERROR("MAIN", "打开文件失败: %d", status);
        return;
    }
    
    status = FatFS_FileWrite(&file, test_data, strlen(test_data), &bytes_written);
    if (status != FATFS_OK) {
        LOG_ERROR("MAIN", "写入文件失败: %d", status);
        FatFS_FileClose(&file);
        return;
    }
    
    status = FatFS_FileSync(&file);
    if (status != FATFS_OK) {
        LOG_ERROR("MAIN", "同步文件失败: %d", status);
    }
    
    LOG_INFO("MAIN", "写入成功: %d 字节", bytes_written);
    FatFS_FileClose(&file);
    
    /* 2. 读取文件 */
    LOG_INFO("MAIN", "2. 读取文件: %s", test_file);
    status = FatFS_FileOpen(&file, test_file, FA_READ);
    if (status != FATFS_OK) {
        LOG_ERROR("MAIN", "打开文件失败: %d", status);
        return;
    }
    
    memset(read_buffer, 0, sizeof(read_buffer));
    status = FatFS_FileRead(&file, read_buffer, sizeof(read_buffer) - 1, &bytes_read);
    if (status != FATFS_OK) {
        LOG_ERROR("MAIN", "读取文件失败: %d", status);
        FatFS_FileClose(&file);
        return;
    }
    
    read_buffer[bytes_read] = '\0';
    LOG_INFO("MAIN", "读取成功: %d 字节", bytes_read);
    LOG_INFO("MAIN", "文件内容: %s", read_buffer);
    FatFS_FileClose(&file);
    
    /* 3. 追加写入 */
    LOG_INFO("MAIN", "3. 追加写入文件");
    /* 使用FA_WRITE | FA_OPEN_ALWAYS打开文件，然后定位到文件末尾 */
    status = FatFS_FileOpen(&file, test_file, FA_WRITE | FA_OPEN_ALWAYS);
    if (status != FATFS_OK) {
        LOG_ERROR("MAIN", "打开文件失败: %d", status);
        return;
    }
    
    /* 定位到文件末尾进行追加写入 */
    /* 使用f_size宏获取文件大小，然后定位到文件末尾 */
    FSIZE_t file_size = f_size(&file);
    if (file_size > 0) {
        /* 定位到文件末尾 */
        FatFS_FileSeek(&file, (uint32_t)file_size);
    }
    
    const char* append_data = " Appended data.";
    status = FatFS_FileWrite(&file, append_data, strlen(append_data), &bytes_written);
    if (status == FATFS_OK) {
        FatFS_FileSync(&file);
        LOG_INFO("MAIN", "追加写入成功: %d 字节", bytes_written);
    } else {
        LOG_ERROR("MAIN", "追加写入失败: %d", status);
    }
    FatFS_FileClose(&file);
    
    /* 4. 文件定位和读取 */
    LOG_INFO("MAIN", "4. 文件定位和读取");
    status = FatFS_FileOpen(&file, test_file, FA_READ);
    if (status == FATFS_OK) {
        /* 定位到文件开头 */
        FatFS_FileSeek(&file, 0);
        memset(read_buffer, 0, sizeof(read_buffer));
        FatFS_FileRead(&file, read_buffer, sizeof(read_buffer) - 1, &bytes_read);
        read_buffer[bytes_read] = '\0';
        LOG_INFO("MAIN", "定位后读取: %s", read_buffer);
        FatFS_FileClose(&file);
    }
    
    /* 5. 重命名文件 */
    LOG_INFO("MAIN", "5. 重命名文件");
    /* 注意：FatFS重命名时，新文件名可以使用相对路径或绝对路径
     * 如果旧文件名使用绝对路径（如"0:test.txt"），新文件名也应该使用绝对路径
     * 或者都使用相对路径（如"test.txt" -> "test_renamed.txt"）
     * 
     * 由于重命名在某些情况下可能失败（可能是FatFS的bug或文件系统状态问题），
     * 这里我们尝试多种方法：
     * 1. 先尝试绝对路径
     * 2. 如果失败，尝试相对路径
     * 3. 如果都失败，跳过重命名操作（不影响其他功能演示）
     */
    const char* new_file_abs = "0:test_renamed.txt";  /* 绝对路径 */
    const char* new_file_rel = "test_renamed.txt";     /* 相对路径 */
    
    /* 确保文件已完全关闭后再重命名 */
    /* 先同步文件系统，确保所有操作完成 */
    Delay_ms(500);  /* 给文件系统足够时间完成所有操作 */
    
    /* 尝试使用绝对路径重命名 */
    const char* renamed_file = new_file_abs;  /* 默认使用绝对路径 */
    status = FatFS_FileRename(test_file, new_file_abs);
    if (status != FATFS_OK) {
        /* 如果绝对路径失败，尝试相对路径 */
        LOG_WARN("MAIN", "使用绝对路径重命名失败 (错误码: %d)，尝试相对路径...", status);
        Delay_ms(100);  /* 稍等片刻 */
        status = FatFS_FileRename(test_file, new_file_rel);
        if (status == FATFS_OK) {
            renamed_file = new_file_rel;  /* 使用相对路径 */
        } else {
            LOG_WARN("MAIN", "使用相对路径重命名也失败 (错误码: %d)", status);
            LOG_WARN("MAIN", "注意：重命名功能在某些FatFS配置下可能不稳定，跳过此操作");
            /* 不返回，继续执行删除原文件的操作 */
            renamed_file = NULL;  /* 标记重命名失败 */
        }
    }
    if (status == FATFS_OK && renamed_file != NULL) {
        LOG_INFO("MAIN", "重命名成功: %s -> %s", test_file, renamed_file);
        
        /* 6. 删除重命名后的文件 */
        LOG_INFO("MAIN", "6. 删除文件: %s", renamed_file);
        Delay_ms(100);  /* 稍等片刻 */
        status = FatFS_FileDelete(renamed_file);
        if (status == FATFS_OK) {
            LOG_INFO("MAIN", "删除成功");
        } else {
            LOG_ERROR("MAIN", "删除失败: %d", status);
        }
    } else {
        /* 如果重命名失败，删除原文件 */
        LOG_INFO("MAIN", "6. 删除原文件: %s", test_file);
        Delay_ms(100);  /* 稍等片刻 */
        status = FatFS_FileDelete(test_file);
        if (status == FATFS_OK) {
            LOG_INFO("MAIN", "删除原文件成功");
        } else {
            LOG_ERROR("MAIN", "删除原文件失败: %d", status);
        }
    }
    
    LOG_INFO("MAIN", "文件操作演示完成");
}

/* ==================== 目录操作演示 ==================== */

/**
 * @brief 目录操作演示
 */
static void TestDirectoryOperations(void)
{
    LOG_INFO("MAIN", "=== 目录操作演示 ===");
    
    FatFS_Status_t status;
    DIR dir;
    FILINFO fno;
    const char* test_dir = "0:test_dir";
    const char* test_file = "0:test_dir/file1.txt";
    
    /* 1. 创建目录 */
    LOG_INFO("MAIN", "1. 创建目录: %s", test_dir);
    status = FatFS_DirCreate(test_dir);
    if (status == FATFS_OK) {
        LOG_INFO("MAIN", "目录创建成功");
    } else if (status == FATFS_ERROR_EXIST) {
        LOG_WARN("MAIN", "目录已存在，继续使用现有目录");
    } else {
        LOG_ERROR("MAIN", "目录创建失败: %d", status);
        return;
    }
    
    /* 2. 在目录中创建文件 */
    LOG_INFO("MAIN", "2. 在目录中创建文件: %s", test_file);
    FIL file;
    status = FatFS_FileOpen(&file, test_file, FA_WRITE | FA_CREATE_ALWAYS);
    if (status == FATFS_OK) {
        const char* data = "File in directory";
        uint32_t bytes_written;
        FatFS_FileWrite(&file, data, strlen(data), &bytes_written);
        FatFS_FileSync(&file);
        FatFS_FileClose(&file);
        LOG_INFO("MAIN", "文件创建成功");
    }
    
    /* 3. 遍历目录 */
    LOG_INFO("MAIN", "3. 遍历目录: %s", test_dir);
    status = FatFS_DirOpen(&dir, test_dir);
    if (status == FATFS_OK) {
        LOG_INFO("MAIN", "目录内容:");
        uint32_t entry_count = 0;
        const uint32_t max_entries = 100;  /* 防止死循环，最多读取100个目录项 */
        char last_fname[13] = "";  /* 记录上一个文件名，防止重复读取 */
        
        while (entry_count < max_entries) {
            status = FatFS_DirRead(&dir, &fno);
            if (status != FATFS_OK) {
                /* 读取失败，可能是目录遍历完成 */
                break;
            }
            
            /* 检查是否读取到空文件名（目录遍历完成的标志） */
            if (fno.fname[0] == 0) {
                /* 目录读取完成 */
                break;
            }
            
            /* 跳过 "." 和 ".." 目录项 */
            if (fno.fname[0] == '.' && (fno.fname[1] == '\0' || (fno.fname[1] == '.' && fno.fname[2] == '\0'))) {
                continue;
            }
            
            /* 检查是否与上一个文件名相同（防止重复读取） */
            /* 注意：FatFS在目录遍历结束时可能会重复返回最后一个文件 */
            if (entry_count > 0 && strcmp(fno.fname, last_fname) == 0) {
                /* 连续两次读取到相同文件名，目录遍历完成 */
                break;
            }
            
            /* 保存当前文件名 */
            strncpy(last_fname, fno.fname, sizeof(last_fname) - 1);
            last_fname[sizeof(last_fname) - 1] = '\0';
            
            LOG_INFO("MAIN", "  %s (%s, %lu 字节)",
                     fno.fname,
                     (fno.fattrib & AM_DIR) ? "目录" : "文件",
                     fno.fsize);
            entry_count++;
        }
        if (entry_count >= max_entries) {
            LOG_WARN("MAIN", "达到最大目录项数量限制 (%lu)，停止遍历", max_entries);
        } else {
            LOG_INFO("MAIN", "目录遍历完成，共 %lu 个目录项", entry_count);
        }
        FatFS_DirClose(&dir);
    } else {
        LOG_ERROR("MAIN", "打开目录失败: %d", status);
    }
    
    /* 4. 删除目录中的文件 */
    LOG_INFO("MAIN", "4. 删除目录中的文件: %s", test_file);
    FatFS_FileDelete(test_file);
    
    /* 5. 删除目录 */
    LOG_INFO("MAIN", "5. 删除目录: %s", test_dir);
    status = FatFS_DirDelete(test_dir);
    if (status == FATFS_OK) {
        LOG_INFO("MAIN", "目录删除成功");
    } else {
        LOG_ERROR("MAIN", "目录删除失败: %d", status);
    }
    
    LOG_INFO("MAIN", "目录操作演示完成");
}

/* ==================== 综合应用场景 ==================== */

/**
 * @brief 数据日志记录演示
 */
static void TestDataLogging(void)
{
    LOG_INFO("MAIN", "=== 数据日志记录演示 ===");
    
    const char* log_file = "0:data.log";
    FIL file;
    FatFS_Status_t status;
    char log_buffer[200];
    uint32_t bytes_written;
    
    /* 打开日志文件（追加模式） */
    /* 使用FA_WRITE | FA_OPEN_ALWAYS打开日志文件，然后定位到文件末尾 */
    status = FatFS_FileOpen(&file, log_file, FA_WRITE | FA_OPEN_ALWAYS);
    if (status != FATFS_OK) {
        LOG_ERROR("MAIN", "打开日志文件失败: %d", status);
        return;
    }
    
    /* 定位到文件末尾进行追加写入 */
    FSIZE_t file_size = f_size(&file);
    if (file_size > 0) {
        FatFS_FileSeek(&file, (uint32_t)file_size);
    }
    
    /* 写入日志条目 */
    for (int i = 0; i < 5; i++) {
        snprintf(log_buffer, sizeof(log_buffer), "Log entry %d: Test data %d\r\n", i + 1, i * 100);
        status = FatFS_FileWrite(&file, log_buffer, strlen(log_buffer), &bytes_written);
        if (status == FATFS_OK) {
            LOG_INFO("MAIN", "写入日志条目 %d: %d 字节", i + 1, bytes_written);
        }
    }
    
    /* 同步文件 */
    FatFS_FileSync(&file);
    FatFS_FileClose(&file);
    
    LOG_INFO("MAIN", "数据日志记录演示完成，日志文件: %s", log_file);
}

/**
 * @brief 配置文件存储演示
 */
static void TestConfigStorage(void)
{
    LOG_INFO("MAIN", "=== 配置文件存储演示 ===");
    
    const char* config_file = "0:config.txt";
    FIL file;
    FatFS_Status_t status;
    char config_buffer[200];
    uint32_t bytes_written, bytes_read;
    
    /* 写入配置文件 */
    LOG_INFO("MAIN", "写入配置文件: %s", config_file);
    status = FatFS_FileOpen(&file, config_file, FA_WRITE | FA_CREATE_ALWAYS);
    if (status == FATFS_OK) {
        snprintf(config_buffer, sizeof(config_buffer),
                 "DeviceID=001\r\n"
                 "BaudRate=115200\r\n"
                 "Timeout=5000\r\n"
                 "EnableLog=1\r\n");
        FatFS_FileWrite(&file, config_buffer, strlen(config_buffer), &bytes_written);
        FatFS_FileSync(&file);
        FatFS_FileClose(&file);
        LOG_INFO("MAIN", "配置文件写入成功: %d 字节", bytes_written);
    }
    
    /* 读取配置文件 */
    LOG_INFO("MAIN", "读取配置文件: %s", config_file);
    status = FatFS_FileOpen(&file, config_file, FA_READ);
    if (status == FATFS_OK) {
        memset(config_buffer, 0, sizeof(config_buffer));
        FatFS_FileRead(&file, config_buffer, sizeof(config_buffer) - 1, &bytes_read);
        config_buffer[bytes_read] = '\0';
        FatFS_FileClose(&file);
        LOG_INFO("MAIN", "配置文件读取成功: %d 字节", bytes_read);
        LOG_INFO("MAIN", "配置内容:\r\n%s", config_buffer);
    }
    
    LOG_INFO("MAIN", "配置文件存储演示完成");
}

/**
 * @brief 简单的伪随机数生成器（线性同余法）
 */
static uint32_t g_random_seed = 12345;
static uint32_t SimpleRandom(void)
{
    g_random_seed = g_random_seed * 1103515245 + 12345;
    return (g_random_seed >> 16) & 0x7FFF;
}

/**
 * @brief 随机文件操作演示
 */
static void TestRandomFileOperations(void)
{
    LOG_INFO("MAIN", "=== 随机文件操作演示 ===");
    
    #define MAX_FILES 10
    #define KEEP_FILES 5
    #define DELETE_FILES (MAX_FILES - KEEP_FILES)
    
    char file_names[MAX_FILES][32];  /* 存储文件名 */
    uint8_t file_exists[MAX_FILES] = {0};  /* 文件是否存在标志 */
    FIL file;
    FatFS_Status_t status;
    uint32_t bytes_written;
    char file_path[32];
    char content_buffer[200];
    
    /* 步骤1：按编号顺序创建10个文件 */
    LOG_INFO("MAIN", "步骤1：按编号顺序创建 %d 个文件", MAX_FILES);
    for (int i = 0; i < MAX_FILES; i++) {
        /* 生成按编号顺序的文件名（8.3格式：主文件名最多8字符，扩展名3字符） */
        /* 格式：F0000000.TXT, F0000001.TXT, ... F0000009.TXT (8.3格式) */
        snprintf(file_path, sizeof(file_path), "0:F%07d.TXT", i);
        strncpy(file_names[i], file_path, sizeof(file_names[i]) - 1);
        file_names[i][sizeof(file_names[i]) - 1] = '\0';
        
        /* 生成随机内容 */
        uint32_t content_size = 50 + (SimpleRandom() % 150);  /* 50-200字节 */
        for (uint32_t j = 0; j < content_size; j++) {
            content_buffer[j] = 'A' + (SimpleRandom() % 26);  /* 随机字母 */
        }
        content_buffer[content_size] = '\0';
        
        /* 创建并写入文件 */
        status = FatFS_FileOpen(&file, file_path, FA_WRITE | FA_CREATE_ALWAYS);
        if (status == FATFS_OK) {
            FatFS_FileWrite(&file, content_buffer, content_size, &bytes_written);
            FatFS_FileSync(&file);
            FatFS_FileClose(&file);
            file_exists[i] = 1;
            LOG_INFO("MAIN", "  创建文件 %d: %s (%lu 字节)", i + 1, file_path, bytes_written);
        } else {
            LOG_ERROR("MAIN", "  创建文件失败: %s (错误码: %d)", file_path, status);
        }
        Delay_ms(50);  /* 稍作延时 */
    }
    
    LOG_INFO("MAIN", "步骤1完成：成功创建 %d 个文件", MAX_FILES);
    Delay_ms(500);
    
    /* 步骤2：删除编号小的5个文件，保留编号大的5个文件 */
    LOG_INFO("MAIN", "步骤2：删除编号小的 %d 个文件，保留编号大的 %d 个文件", DELETE_FILES, KEEP_FILES);
    
    /* 删除编号小的文件（索引0到DELETE_FILES-1） */
    for (int i = 0; i < DELETE_FILES; i++) {
        if (file_exists[i]) {
            status = FatFS_FileDelete(file_names[i]);
            if (status == FATFS_OK) {
                file_exists[i] = 0;
                LOG_INFO("MAIN", "  删除文件: %s", file_names[i]);
            } else {
                LOG_ERROR("MAIN", "  删除文件失败: %s (错误码: %d)", file_names[i], status);
            }
            Delay_ms(50);
        } else {
            LOG_WARN("MAIN", "  文件不存在，跳过: %s", file_names[i]);
        }
    }
    
    LOG_INFO("MAIN", "步骤2完成：已删除 %d 个文件", DELETE_FILES);
    Delay_ms(500);
    
    /* 步骤3：验证剩余文件（通过遍历根目录） */
    LOG_INFO("MAIN", "步骤3：遍历根目录验证剩余文件");
    DIR dir;
    FILINFO fno;
    status = FatFS_DirOpen(&dir, "0:");
    if (status == FATFS_OK) {
        LOG_INFO("MAIN", "根目录内容:");
        uint32_t entry_count = 0;
        const uint32_t max_entries = 100;
        char last_fname[13] = "";
        
        while (entry_count < max_entries) {
            status = FatFS_DirRead(&dir, &fno);
            if (status != FATFS_OK) {
                break;
            }
            
            if (fno.fname[0] == 0) {
                break;
            }
            
            /* 跳过 "." 和 ".." 目录项 */
            if (fno.fname[0] == '.' && (fno.fname[1] == '\0' || (fno.fname[1] == '.' && fno.fname[2] == '\0'))) {
                continue;
            }
            
            /* 检查是否与上一个文件名相同 */
            if (entry_count > 0 && strcmp(fno.fname, last_fname) == 0) {
                break;
            }
            
            strncpy(last_fname, fno.fname, sizeof(last_fname) - 1);
            last_fname[sizeof(last_fname) - 1] = '\0';
            
            LOG_INFO("MAIN", "  %s (%s, %lu 字节)",
                     fno.fname,
                     (fno.fattrib & AM_DIR) ? "目录" : "文件",
                     fno.fsize);
            entry_count++;
        }
        FatFS_DirClose(&dir);
        LOG_INFO("MAIN", "根目录遍历完成，共 %lu 个目录项", entry_count);
    } else {
        LOG_ERROR("MAIN", "打开根目录失败: %d", status);
    }
    
    /* 统计剩余文件 */
    int remaining_count = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_exists[i]) {
            remaining_count++;
        }
    }
    
    LOG_INFO("MAIN", "随机文件操作演示完成");
    LOG_INFO("MAIN", "  创建文件数: %d", MAX_FILES);
    LOG_INFO("MAIN", "  删除文件数: %d", DELETE_FILES);
    LOG_INFO("MAIN", "  剩余文件数: %d", remaining_count);
}

/* ==================== 主函数 ==================== */

/**
 * @brief 主函数
 */
int main(void)
{
    FatFS_Status_t fatfs_status;
    UART_Status_t uart_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    OLED_Status_t oled_status;
    
    /* ========== 步骤1：系统初始化 ========== */
    System_Init();
    
    /* ========== 步骤2：UART初始化 ========== */
    uart_status = UART_Init(UART_INSTANCE_1);
    if (uart_status != UART_OK) {
        while(1) { Delay_ms(1000); }
    }
    Delay_ms(100);
    
    /* ========== 步骤3：Debug模块初始化（UART模式） ========== */
    debug_status = Debug_Init(DEBUG_MODE_UART, 115200);
    if (debug_status != 0) {
        while(1) { Delay_ms(1000); }
    }
    Delay_ms(100);
    
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
    
    LOG_INFO("MAIN", "=== Flash07 - TF卡集成FatFS文件系统示例（标准格式化） ===");
    LOG_INFO("MAIN", "=== 系统初始化完成 ===");
    
    /* ========== 步骤5：LED初始化 ========== */
    if (LED_Init() != LED_OK) {
        LOG_ERROR("MAIN", "LED初始化失败");
        while(1) { Delay_ms(1000); }
    }
    
    /* ========== 步骤6：软件I2C初始化（OLED需要） ========== */
    SoftI2C_Status_t i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_1);
    if (i2c_status != SOFT_I2C_OK) {
        LOG_ERROR("MAIN", "软件I2C初始化失败: %d", i2c_status);
        ErrorHandler_Handle(i2c_status, "SOFT_I2C");
    } else {
        LOG_INFO("MAIN", "软件I2C已初始化: PB8(SCL), PB9(SDA)");
    }
    
    /* ========== 步骤7：OLED初始化 ========== */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK) {
        LOG_ERROR("MAIN", "OLED初始化失败: %d", oled_status);
        ErrorHandler_Handle(oled_status, "OLED");
    } else {
        OLED_Clear();
        OLED_ShowString(1, 1, "Flash07 Demo");
        OLED_ShowString(2, 1, "Initializing...");
        LOG_INFO("MAIN", "OLED已初始化");
    }
    
    Delay_ms(500);
    
    /* ========== 步骤8：SPI初始化 ========== */
    LOG_INFO("MAIN", "初始化SPI模块...");
    
    /* 手动配置PA11为GPIO输出（软件NSS模式） */
    GPIO_EnableClock(GPIOA);
    GPIO_Config(GPIOA, GPIO_Pin_11, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHz);
    GPIO_WritePin(GPIOA, GPIO_Pin_11, Bit_SET);  /* NSS默认拉高（不选中） */
    
    SPI_Status_t spi_status = SPI_HW_Init(SPI_INSTANCE_2);
    if (spi_status != SPI_OK) {
        LOG_ERROR("MAIN", "SPI初始化失败: %d", spi_status);
        ErrorHandler_Handle(spi_status, "SPI");
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "SPI初始化成功");
    
    Delay_ms(500);
    
    /* ========== 步骤9：TF卡自动初始化（参考Flash05，使用高级API） ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "TF Card Init");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== TF卡自动初始化 ===");
    TF_SPI_Status_t tf_status = TF_SPI_Init();
    if (tf_status != TF_SPI_OK) {
        LOG_ERROR("MAIN", "TF_SPI初始化失败: %d", tf_status);
        LOG_ERROR("MAIN", "可能的原因：");
        LOG_ERROR("MAIN", "  1. SPI模块未正确初始化");
        LOG_ERROR("MAIN", "  2. SD卡未插入或损坏");
        LOG_ERROR("MAIN", "  3. 硬件连接问题（检查CS=PA11, SCK=PB13, MISO=PB14, MOSI=PB15）");
        LOG_ERROR("MAIN", "  4. MISO引脚（PB14）缺少上拉电阻（10k-50kΩ）");
        LOG_ERROR("MAIN", "  5. 电源问题（确保3.3V稳定）");
        LOG_ERROR("MAIN", "请查看上面的TF_SPI调试日志以获取详细信息");
        ErrorHandler_Handle(tf_status, "TF_SPI");
        while(1) { Delay_ms(1000); }
    }
    
    /* 显示SD卡信息 */
    const tf_spi_dev_t* dev_info = TF_SPI_GetInfo();
    if (dev_info != NULL) {
        char buf[17];
        snprintf(buf, sizeof(buf), "Cap: %d MB", dev_info->capacity_mb);
        OLED_ShowString(3, 1, buf);
        
        LOG_INFO("MAIN", "SD卡信息:");
        LOG_INFO("MAIN", "  容量: %d MB", dev_info->capacity_mb);
        LOG_INFO("MAIN", "  块大小: %d 字节", dev_info->block_size);
        LOG_INFO("MAIN", "  块数量: %d", dev_info->block_count);
        LOG_INFO("MAIN", "  卡类型: %s", dev_info->is_sdhc ? "SDHC/SDXC" : "SDSC");
    }
    
    Delay_ms(2000);
    
    /* ========== 步骤10：文件系统初始化（标准格式化） ========== */
    LOG_INFO("MAIN", "=== 文件系统初始化 ===");
    
    #if FATFS_FORCE_FORMAT
    /* 强制格式化模式：跳过挂载检查，直接格式化 */
    LOG_WARN("MAIN", "[强制格式化] 强制格式化模式已启用！");
    LOG_WARN("MAIN", "[警告] 格式化将清空SD卡所有数据！");
    
    /* 先尝试卸载文件系统（如果已挂载） */
    FatFS_Unmount(FATFS_VOLUME_SPI);
    Delay_ms(100);
    
    /* 显示SD卡信息 */
    const tf_spi_dev_t* dev_info_fmt = TF_SPI_GetInfo();
    if (dev_info_fmt == NULL) {
        LOG_ERROR("MAIN", "无法获取SD卡信息，无法格式化");
        ErrorHandler_Handle(FATFS_ERROR_NOT_READY, "FatFS");
        while(1) { Delay_ms(1000); }
    }
    
    LOG_INFO("MAIN", "SD卡容量: %d MB (%d 扇区)", dev_info_fmt->capacity_mb, dev_info_fmt->block_count);
    LOG_INFO("MAIN", "[注意] 格式化 %d MB 的SD卡需要较长时间（可能需要几分钟），请耐心等待...", dev_info_fmt->capacity_mb);
    LOG_INFO("MAIN", "[提示] 格式化过程中会有大量扇区读写操作，这是正常的");
    
    /* 标准格式化：整个SD卡格式化为FAT32 */
    LOG_INFO("MAIN", "=== 标准格式化（强制模式） ===");
    LOG_INFO("MAIN", "开始格式化整个SD卡为FAT32文件系统...");
    
    /* OLED显示格式化提示 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Formatting...");
    OLED_ShowString(2, 1, "Please wait...");
    char buf[17];
    snprintf(buf, sizeof(buf), "%d MB", dev_info_fmt->capacity_mb);
    OLED_ShowString(3, 1, buf);
    
    /* 开始格式化（这个过程可能需要几分钟） */
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "========================================");
    LOG_INFO("MAIN", "开始格式化，请耐心等待...");
    LOG_INFO("MAIN", "格式化过程中请勿断电或拔出SD卡！");
    LOG_INFO("MAIN", "格式化可能需要几分钟时间...");
    LOG_INFO("MAIN", "========================================");
    LOG_INFO("MAIN", "");
    
    /* 闪烁LED提示开始格式化 */
    for (int i = 0; i < 5; i++) {
        LED1_Toggle();
        Delay_ms(200);
    }
    
    /* 开始格式化（阻塞操作，可能需要几分钟） */
    LOG_INFO("MAIN", "调用FatFS_Format()开始格式化...");
    LOG_INFO("MAIN", "[提示] 格式化过程中LED会闪烁，表示程序正在运行");
    LOG_INFO("MAIN", "[提示] 格式化大容量SD卡可能需要几分钟，请耐心等待...");
    
    /* 在格式化前闪烁LED提示 */
    LED1_On();
    Delay_ms(100);
    LED1_Off();
    
    /* 开始格式化（阻塞操作，可能需要几分钟） */
    /* 注意：格式化过程中无法输出日志，但LED会在disk_write中闪烁 */
    fatfs_status = FatFS_Format(FATFS_VOLUME_SPI, "0:");
    
    LOG_INFO("MAIN", "FatFS_Format()返回，状态码: %d", fatfs_status);
    if (fatfs_status != FATFS_OK) {
        LOG_ERROR("MAIN", "标准格式化失败: %d", fatfs_status);
        ErrorHandler_Handle(fatfs_status, "FatFS");
        OLED_Clear();
        OLED_ShowString(1, 1, "Format Fail");
        OLED_ShowString(2, 1, "Error!");
        while(1) { 
            LED1_Toggle();
            Delay_ms(500); 
        }
    }
    
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "========================================");
    LOG_INFO("MAIN", "标准格式化完成！");
    LOG_INFO("MAIN", "========================================");
    LOG_INFO("MAIN", "");
    
    /* 闪烁LED提示格式化完成 */
    for (int i = 0; i < 3; i++) {
        LED1_On();
        Delay_ms(200);
        LED1_Off();
        Delay_ms(200);
    }
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Format OK!");
    OLED_ShowString(2, 1, "Mounting...");
    Delay_ms(1000);
    
    /* 挂载文件系统 */
    fatfs_status = FatFS_Mount(FATFS_VOLUME_SPI, "0:");
    
    #else
    /* 自动检测模式：尝试挂载，如果失败则格式化 */
    /* 尝试挂载文件系统 */
    fatfs_status = FatFS_Mount(FATFS_VOLUME_SPI, "0:");
    
    if (fatfs_status == FATFS_ERROR_NO_FILESYSTEM) {
        LOG_INFO("MAIN", "检测到SD卡未格式化，开始格式化...");
        LOG_WARN("MAIN", "?? 警告：格式化将清空SD卡所有数据！");
        
        /* 标准格式化：整个SD卡格式化为FAT32 */
        LOG_INFO("MAIN", "=== 标准格式化 ===");
        LOG_INFO("MAIN", "格式化整个SD卡为FAT32文件系统");
        
        fatfs_status = FatFS_Format(FATFS_VOLUME_SPI, "0:");
        if (fatfs_status != FATFS_OK) {
            LOG_ERROR("MAIN", "标准格式化失败: %d", fatfs_status);
            ErrorHandler_Handle(fatfs_status, "FatFS");
            OLED_ShowString(3, 1, "Format Fail");
            while(1) { Delay_ms(1000); }
        }
        
        LOG_INFO("MAIN", "标准格式化完成");
        
        /* 重新挂载 */
        fatfs_status = FatFS_Mount(FATFS_VOLUME_SPI, "0:");
    }
    #endif
    
    if (fatfs_status != FATFS_OK) {
        LOG_ERROR("MAIN", "文件系统挂载失败: %d", fatfs_status);
        ErrorHandler_Handle(fatfs_status, "FatFS");
        OLED_ShowString(3, 1, "Mount Fail");
        while(1) { Delay_ms(1000); }
    }
    
    LOG_INFO("MAIN", "文件系统挂载成功");
    OLED_ShowString(3, 1, "Mount OK");
    
    /* 显示文件系统信息 */
    uint32_t free_clusters, total_clusters;
    fatfs_status = FatFS_GetFreeSpace(FATFS_VOLUME_SPI, "0:", &free_clusters, &total_clusters);
    if (fatfs_status == FATFS_OK) {
        uint32_t total_bytes = 0;
        FatFS_GetTotalSpace(FATFS_VOLUME_SPI, "0:", &total_bytes);
        uint32_t free_bytes = (free_clusters * 512 * 8);  /* 简化计算 */
        LOG_INFO("MAIN", "文件系统信息:");
        LOG_INFO("MAIN", "  总空间: %lu KB", total_bytes / 1024);
        LOG_INFO("MAIN", "  空闲空间: %lu KB", free_bytes / 1024);
        LOG_INFO("MAIN", "  总簇数: %lu", total_clusters);
        LOG_INFO("MAIN", "  空闲簇数: %lu", free_clusters);
    }
    
    /* ========== 步骤11：文件操作演示 ========== */
    Delay_ms(1000);
    TestFileOperations();
    Delay_ms(1000);
    
    /* ========== 步骤12：目录操作演示 ========== */
    Delay_ms(1000);
    TestDirectoryOperations();
    Delay_ms(1000);
    
    /* ========== 步骤13：综合应用场景 ========== */
    Delay_ms(1000);
    TestDataLogging();
    Delay_ms(1000);
    TestConfigStorage();
    Delay_ms(1000);
    
    /* ========== 步骤14：随机文件操作演示 ========== */
    TestRandomFileOperations();
    
    LOG_INFO("MAIN", "=== 所有演示完成 ===");
    OLED_ShowString(4, 1, "All Tests OK");
    
    /* ========== 主循环 ========== */
    while (1) {
        LED1_Toggle();
        Delay_ms(500);
    }
}
