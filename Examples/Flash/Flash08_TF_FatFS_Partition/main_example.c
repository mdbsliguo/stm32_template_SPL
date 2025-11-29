/**
 * @file main_example.c
 * @brief Flash08 - TF卡集成FatFS文件系统示例（两个分区）
 * @details 演示FatFS文件系统的分区方案，创建MBR分区表，分区1（10%）给MCU直接访问，分区2（90%）给FAT32文件系统
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
 * 1. 文件系统初始化（分区方案：创建两个分区）
 * 2. MCU直接访问演示（访问分区1）
 * 3. 文件操作（创建、读写、删除、重命名）
 * 4. 目录操作（创建、遍历、删除）
 * 5. 综合应用场景（数据日志、配置文件）
 * 
 * @note 本示例使用FatFS封装层（fatfs_wrapper.h），提供统一的错误码和接口
 * @note 需要修改ffconf.h启用FF_USE_MKFS和FF_MULTI_PARTITION
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

/* ==================== 全局变量 ==================== */

/* MCU预留区域信息（分区1） */
static struct {
    uint32_t start_sector;  /**< 起始扇区 */
    uint32_t sector_count;  /**< 扇区数量 */
    uint8_t initialized;    /**< 是否已初始化 */
} g_mcu_reserved_area = {0, 0, 0};

/* ==================== 格式化函数 ==================== */

/**
 * @brief 将FatFS错误码转换为项目错误码（本地辅助函数）
 */
static FatFS_Status_t ConvertFatFSError(FRESULT fr)
{
    switch (fr) {
        case FR_OK: return FATFS_OK;
        case FR_DISK_ERR: return FATFS_ERROR_DISK_ERROR;
        case FR_INT_ERR: return FATFS_ERROR_INVALID_PARAM;
        case FR_NOT_READY: return FATFS_ERROR_NOT_READY;
        case FR_NO_FILE: return FATFS_ERROR_NO_FILE;
        case FR_NO_PATH: return FATFS_ERROR_NO_PATH;
        case FR_INVALID_NAME: return FATFS_ERROR_INVALID_NAME;
        case FR_DENIED: return FATFS_ERROR_DENIED;
        case FR_EXIST: return FATFS_ERROR_EXIST;
        case FR_INVALID_OBJECT: return FATFS_ERROR_INVALID_OBJECT;
        case FR_WRITE_PROTECTED: return FATFS_ERROR_WRITE_PROTECTED;
        case FR_INVALID_DRIVE: return FATFS_ERROR_INVALID_DRIVE;
        case FR_NOT_ENABLED: return FATFS_ERROR_NOT_ENABLED;
        case FR_NO_FILESYSTEM: return FATFS_ERROR_NO_FILESYSTEM;
        case FR_TIMEOUT: return FATFS_ERROR_TIMEOUT;
        case FR_LOCKED: return FATFS_ERROR_LOCKED;
        case FR_NOT_ENOUGH_CORE: return FATFS_ERROR_NOT_ENOUGH_CORE;
        case FR_TOO_MANY_OPEN_FILES: return FATFS_ERROR_TOO_MANY_OPEN_FILES;
        case FR_INVALID_PARAMETER: return FATFS_ERROR_INVALID_PARAMETER;
        default: return FATFS_ERROR_INVALID_PARAM;
    }
}

/**
 * @brief 分区方案格式化（创建两个分区）
 */
static FatFS_Status_t FormatPartition(void)
{
    LOG_INFO("MAIN", "=== 分区方案格式化 ===");
    LOG_INFO("MAIN", "创建两个分区：分区1（10% MCU直接访问），分区2（90% FAT32文件系统）");
    
    #if FF_MULTI_PARTITION && FF_USE_MKFS
    /* 获取SD卡总容量 */
    const tf_spi_dev_t* dev_info = TF_SPI_GetInfo();
    if (dev_info == NULL) {
        LOG_ERROR("MAIN", "无法获取SD卡信息");
        return FATFS_ERROR_NOT_READY;
    }
    
    uint32_t total_sectors = dev_info->block_count;
    uint32_t reserved_sectors = total_sectors / 10;  /* 预留10% */
    uint32_t fs_sectors = total_sectors - reserved_sectors;  /* 文件系统使用90% */
    
    /* ?? 重要：分区1从扇区1开始，跳过扇区0（MBR分区表） */
    /* 扇区0：MBR分区表（绝对不能覆盖） */
    /* 分区1：从扇区1开始，用于MCU直接访问 */
    /* 分区2：从分区1结束位置开始，用于FAT32文件系统 */
    uint32_t partition1_start = 1;  /* 分区1起始扇区（跳过MBR） */
    uint32_t partition1_sectors = reserved_sectors - 1;  /* 分区1大小（减去MBR扇区） */
    uint32_t partition2_start = reserved_sectors;  /* 分区2起始扇区 */
    
    LOG_INFO("MAIN", "SD卡总容量: %d MB (%d 扇区)", dev_info->capacity_mb, total_sectors);
    LOG_INFO("MAIN", "扇区0：MBR分区表（保护区域，不可访问）");
    LOG_INFO("MAIN", "预留区域（分区1）: 扇区 %d - %d（%d 扇区，约 %d MB，MCU直接访问）",
             partition1_start, partition1_start + partition1_sectors - 1,
             partition1_sectors, (partition1_sectors * 512) / (1024 * 1024));
    LOG_INFO("MAIN", "文件系统区域（分区2）: 扇区 %d - %d（%d 扇区，约 %d MB，FAT32文件系统）",
             partition2_start, total_sectors - 1,
             fs_sectors, (fs_sectors * 512) / (1024 * 1024));
    
    /* 保存MCU预留区域信息（分区1，从扇区1开始） */
    g_mcu_reserved_area.start_sector = partition1_start;  /* 从扇区1开始，跳过MBR */
    g_mcu_reserved_area.sector_count = partition1_sectors;
    g_mcu_reserved_area.initialized = 1;
    
    /* 创建分区表：分区1（MCU直接访问），分区2（FAT32文件系统） */
    /* 注意：f_fdisk()的分区大小参数包括MBR扇区，所以分区1大小 = reserved_sectors */
    LBA_t partition_table[4];
    partition_table[0] = reserved_sectors;  /* 分区1大小（10%，包括MBR扇区） */
    partition_table[1] = fs_sectors;       /* 分区2大小（90%） */
    partition_table[2] = 0;                /* 分区3：未使用 */
    partition_table[3] = 0;                /* 分区4：未使用 */
    
    /* 工作缓冲区（至少512字节） */
    static BYTE work_buf[512];
    
    /* 创建分区表 */
    LOG_INFO("MAIN", "正在创建分区表...");
    FRESULT fr = f_fdisk(0, partition_table, work_buf);
    if (fr != FR_OK) {
        LOG_ERROR("MAIN", "创建分区表失败: %d", fr);
        return ConvertFatFSError(fr);
    }
    
    LOG_INFO("MAIN", "分区表创建成功");
    LOG_INFO("MAIN", "[重要] 扇区0包含MBR分区表，绝对不能覆盖！");
    LOG_INFO("MAIN", "分区1：扇区 %d - %d（MCU直接访问，10%%，跳过MBR）", partition1_start, partition1_start + partition1_sectors - 1);
    LOG_INFO("MAIN", "分区2：扇区 %d - %d（FAT32文件系统，90%%）", partition2_start, total_sectors - 1);
    
    /* 在分区2上创建文件系统 */
    LOG_INFO("MAIN", "正在格式化分区2（FAT32文件系统）...");
    LOG_INFO("MAIN", "[注意] 格式化 %d MB 的分区需要较长时间（可能需要几分钟），请耐心等待...", (fs_sectors * 512) / (1024 * 1024));
    LOG_INFO("MAIN", "[提示] 格式化过程中会有大量扇区读写操作，这是正常的");
    OLED_ShowString(2, 1, "Formatting...");
    OLED_ShowString(3, 1, "Please wait...");
    
    MKFS_PARM opt;
    opt.fmt = FM_FAT32;  /* FAT32格式 */
    opt.n_fat = 1;       /* 1个FAT表 */
    opt.align = 0;       /* 自动对齐 */
    opt.n_root = 0;     /* 根目录项数（FAT32自动） */
    opt.au_size = 0;    /* 自动簇大小 */
    
    /* 工作缓冲区（至少FF_MAX_SS字节） */
    static BYTE mkfs_work[FF_MAX_SS];
    
    /* 在分区2上创建文件系统（使用"0:2:"指定分区2） */
    fr = f_mkfs("0:2:", &opt, mkfs_work, sizeof(mkfs_work));
    if (fr != FR_OK) {
        LOG_ERROR("MAIN", "在分区2上创建文件系统失败: %d", fr);
        return ConvertFatFSError(fr);
    }
    
    LOG_INFO("MAIN", "分区方案格式化完成");
    return FATFS_OK;
    
    #else
    LOG_ERROR("MAIN", "分区方案需要启用FF_MULTI_PARTITION和FF_USE_MKFS");
    LOG_ERROR("MAIN", "请修改Middlewares/storage/fatfs/ffconf.h：");
    LOG_ERROR("MAIN", "  #define FF_MULTI_PARTITION  1");
    LOG_ERROR("MAIN", "  #define FF_USE_MKFS         1");
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
}

/* ==================== MCU直接访问演示 ==================== */

/**
 * @brief MCU直接访问演示（访问分区1）
 */
static void TestMCUDirectAccess(void)
{
    if (!g_mcu_reserved_area.initialized) {
        LOG_WARN("MAIN", "MCU预留区域未初始化，跳过直接访问演示");
        return;
    }
    
    LOG_INFO("MAIN", "=== MCU直接访问演示（分区1） ===");
    LOG_INFO("MAIN", "预留区域：扇区 %d - %d（%d 扇区，约 %d MB）",
             g_mcu_reserved_area.start_sector,
             g_mcu_reserved_area.start_sector + g_mcu_reserved_area.sector_count - 1,
             g_mcu_reserved_area.sector_count,
             (g_mcu_reserved_area.sector_count * 512) / (1024 * 1024));
    
    /* 测试数据 */
    uint8_t test_data[512];
    uint8_t read_data[512];
    uint32_t i;
    
    /* 填充测试数据 */
    for (i = 0; i < 512; i++) {
        test_data[i] = (uint8_t)(i & 0xFF);
    }
    
    /* ?? 保护检查：确保不会覆盖MBR分区表（扇区0） */
    if (g_mcu_reserved_area.start_sector == 0) {
        LOG_ERROR("MAIN", "错误：MCU预留区域起始扇区为0，这会覆盖MBR分区表！");
        LOG_ERROR("MAIN", "请修改代码，确保分区1从扇区1开始");
        return;
    }
    
    /* 写入预留区域的第一个扇区 */
    LOG_INFO("MAIN", "写入预留区域扇区 %d（跳过MBR扇区0）", g_mcu_reserved_area.start_sector);
    TF_SPI_Status_t status = TF_SPI_WriteBlock(g_mcu_reserved_area.start_sector, test_data);
    if (status != TF_SPI_OK) {
        LOG_ERROR("MAIN", "写入失败: %d", status);
        return;
    }
    
    /* 读取验证 */
    LOG_INFO("MAIN", "读取预留区域扇区 %d 进行验证", g_mcu_reserved_area.start_sector);
    status = TF_SPI_ReadBlock(g_mcu_reserved_area.start_sector, read_data);
    if (status != TF_SPI_OK) {
        LOG_ERROR("MAIN", "读取失败: %d", status);
        return;
    }
    
    /* 验证数据 */
    uint8_t match = 1;
    for (i = 0; i < 512; i++) {
        if (read_data[i] != test_data[i]) {
            match = 0;
            break;
        }
    }
    
    if (match) {
        LOG_INFO("MAIN", "MCU直接访问测试成功：数据验证通过");
        LOG_INFO("MAIN", "前16字节: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                 read_data[0], read_data[1], read_data[2], read_data[3],
                 read_data[4], read_data[5], read_data[6], read_data[7],
                 read_data[8], read_data[9], read_data[10], read_data[11],
                 read_data[12], read_data[13], read_data[14], read_data[15]);
    } else {
        LOG_ERROR("MAIN", "MCU直接访问测试失败：数据验证不匹配");
    }
    
    LOG_INFO("MAIN", "MCU直接访问演示完成");
}

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
    const char* test_file = "test.txt";  /* 使用相对路径（已挂载到0:2:） */
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
    const char* new_file = "test_renamed.txt";
    /* 确保文件已完全关闭后再重命名 */
    /* 先同步文件系统，确保所有操作完成 */
    Delay_ms(200);  /* 给文件系统一点时间完成所有操作 */
    status = FatFS_FileRename(test_file, new_file);
    if (status == FATFS_OK) {
        LOG_INFO("MAIN", "重命名成功: %s -> %s", test_file, new_file);
        
        /* 6. 删除重命名后的文件 */
        LOG_INFO("MAIN", "6. 删除文件: %s", new_file);
        status = FatFS_FileDelete(new_file);
        if (status == FATFS_OK) {
            LOG_INFO("MAIN", "删除成功");
        } else {
            LOG_ERROR("MAIN", "删除失败: %d", status);
        }
    } else {
        LOG_ERROR("MAIN", "重命名失败: %d", status);
        /* 如果重命名失败，尝试删除原文件 */
        LOG_INFO("MAIN", "6. 删除原文件: %s", test_file);
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
    const char* test_dir = "test_dir";
    const char* test_file = "test_dir/file1.txt";
    
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
                LOG_WARN("MAIN", "读取目录项失败: %d", status);
                break;
            }
            if (fno.fname[0] == 0) {
                /* 目录读取完成 */
                break;
            }
            
            /* 跳过 "." 和 ".." 目录项 */
            if (fno.fname[0] == '.' && (fno.fname[1] == '\0' || (fno.fname[1] == '.' && fno.fname[2] == '\0'))) {
                continue;
            }
            
            /* 检查是否与上一个文件名相同（防止重复读取） */
            /* 注意：只在读取多个条目后才检查，避免误判 */
            if (entry_count > 0 && strcmp(fno.fname, last_fname) == 0) {
                /* 连续两次读取到相同文件名，可能是目录遍历完成或出现循环 */
                LOG_WARN("MAIN", "检测到重复文件名，停止遍历");
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
    
    const char* log_file = "data.log";
    FIL file;
    FatFS_Status_t status;
    char log_buffer[200];
    uint32_t bytes_written;
    
    /* 打开日志文件（追加模式） */
    /* FA_OPEN_APPEND已经包含了FA_WRITE，FA_CREATE_ALWAYS用于文件不存在时创建 */
    /* 使用FA_WRITE | FA_OPEN_ALWAYS打开日志文件，然后定位到文件末尾 */
    status = FatFS_FileOpen(&file, log_file, FA_WRITE | FA_OPEN_ALWAYS);
    if (status == FATFS_OK) {
        /* 定位到文件末尾进行追加写入 */
        FSIZE_t file_size = f_size(&file);
        if (file_size > 0) {
            FatFS_FileSeek(&file, (uint32_t)file_size);
        }
    }
    if (status != FATFS_OK) {
        LOG_ERROR("MAIN", "打开日志文件失败: %d", status);
        return;
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
    
    const char* config_file = "config.txt";
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
    
    LOG_INFO("MAIN", "=== Flash08 - TF卡集成FatFS文件系统示例（两个分区） ===");
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
        OLED_ShowString(1, 1, "Flash08 Demo");
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
    
    /* ========== 步骤10：文件系统初始化（分区方案） ========== */
    LOG_INFO("MAIN", "=== 文件系统初始化（分区方案） ===");
    
    #if FATFS_FORCE_FORMAT
    /* 强制格式化模式：跳过挂载检查，直接格式化 */
    LOG_WARN("MAIN", "[强制格式化] 强制格式化模式已启用！");
    LOG_WARN("MAIN", "[警告] 格式化将清空SD卡所有数据！");
    
    /* 创建分区表并格式化分区2 */
    fatfs_status = FormatPartition();
    if (fatfs_status == FATFS_OK) {
        /* 挂载文件系统 */
        fatfs_status = FatFS_Mount(FATFS_VOLUME_SPI, "0:2:");
    }
    
    #else
    /* 自动检测模式：尝试挂载，如果失败则格式化 */
    /* 尝试挂载分区2 */
    fatfs_status = FatFS_Mount(FATFS_VOLUME_SPI, "0:2:");
    
    if (fatfs_status == FATFS_ERROR_NO_FILESYSTEM) {
        LOG_INFO("MAIN", "检测到分区2未格式化，开始创建分区并格式化...");
        LOG_WARN("MAIN", "?? 警告：格式化将清空SD卡所有数据！");
        
        /* 创建分区表并格式化分区2 */
        fatfs_status = FormatPartition();
        if (fatfs_status == FATFS_OK) {
            /* 重新挂载 */
            fatfs_status = FatFS_Mount(FATFS_VOLUME_SPI, "0:2:");
        }
    }
    #endif
    
    if (fatfs_status != FATFS_OK) {
        LOG_ERROR("MAIN", "文件系统挂载失败: %d", fatfs_status);
        ErrorHandler_Handle(fatfs_status, "FatFS");
        OLED_ShowString(3, 1, "Mount Fail");
        while(1) { Delay_ms(1000); }
    }
    
    LOG_INFO("MAIN", "文件系统挂载成功（分区2）");
    OLED_ShowString(3, 1, "Mount OK");
    
    /* 显示文件系统信息 */
    uint32_t free_clusters, total_clusters;
    fatfs_status = FatFS_GetFreeSpace(FATFS_VOLUME_SPI, "0:2:", &free_clusters, &total_clusters);
    if (fatfs_status == FATFS_OK) {
        uint32_t total_bytes = 0;
        FatFS_GetTotalSpace(FATFS_VOLUME_SPI, "0:2:", &total_bytes);
        uint32_t free_bytes = (free_clusters * 512 * 8);  /* 简化计算 */
        LOG_INFO("MAIN", "文件系统信息（分区2）:");
        LOG_INFO("MAIN", "  总空间: %lu KB", total_bytes / 1024);
        LOG_INFO("MAIN", "  空闲空间: %lu KB", free_bytes / 1024);
        LOG_INFO("MAIN", "  总簇数: %lu", total_clusters);
        LOG_INFO("MAIN", "  空闲簇数: %lu", free_clusters);
    }
    
    /* ========== 步骤11：MCU直接访问演示（访问分区1） ========== */
    Delay_ms(1000);
    TestMCUDirectAccess();
    Delay_ms(1000);
    
    /* ========== 步骤12：文件操作演示 ========== */
    Delay_ms(1000);
    TestFileOperations();
    Delay_ms(1000);
    
    /* ========== 步骤13：目录操作演示 ========== */
    Delay_ms(1000);
    TestDirectoryOperations();
    Delay_ms(1000);
    
    /* ========== 步骤14：综合应用场景 ========== */
    Delay_ms(1000);
    TestDataLogging();
    Delay_ms(1000);
    TestConfigStorage();
    
    LOG_INFO("MAIN", "=== 所有演示完成 ===");
    OLED_ShowString(4, 1, "All Tests OK");
    
    /* ========== 主循环 ========== */
    while (1) {
        LED1_Toggle();
        Delay_ms(500);
    }
}

