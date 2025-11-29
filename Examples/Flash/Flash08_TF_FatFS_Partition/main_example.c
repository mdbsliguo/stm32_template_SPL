/**
 * @file main_example.c
 * @brief Flash08 - TF卡集成FatFS文件系统示例（单分区方案）
 * @details 演示FatFS文件系统的单分区方案，MBR + 保留区 + STM32直接访问区 + FAT32单分区
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
 * 1. 单分区格式化（MBR + 保留区 + STM32直接访问区 + FAT32单分区）
 * 2. MCU直接访问演示（访问STM32直接访问区：扇区2048-206847，100MB）
 * 3. 文件操作演示（创建、读取、写入、追加、重命名、删除）
 * 4. 目录操作演示（创建、遍历、删除）
 * 5. 数据记录演示（日志记录、配置存储）
 * 
 * @note FatFS相关API请参考fatfs_wrapper.h
 * @note 需要启用ffconf.h中的FF_USE_MKFS和FF_MULTI_PARTITION
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
#include "diskio.h"
#include "gpio.h"
#include "config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* ==================== 全局变量 ==================== */

/* MCU保留区域信息（STM32直接访问区） */
static struct {
    uint32_t start_sector;  /**< 起始扇区 */
    uint32_t sector_count;  /**< 扇区数量 */
    uint8_t initialized;    /**< 是否已初始化 */
} g_mcu_reserved_area = {0, 0, 0};

/* ==================== 辅助函数 ==================== */

/**
 * @brief 转换FatFS错误码
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

/* ==================== 分区格式化 ==================== */

/**
 * @brief 创建MBR分区表（单分区方案）
 * @details 创建MBR分区表，包含一个FAT32分区
 * 
 * 布局：
 * - 扇区0：MBR
 * - 扇区1-2047：保留区（约1MB）
 * - 扇区2048-206847：STM32直接访问区（100MB）
 * - 扇区206848-结束：FAT32分区
 */
static FatFS_Status_t CreateMBRPartition(void)
{
    LOG_INFO("MAIN", "=== 创建MBR分区表 ===");
    LOG_INFO("MAIN", "布局：MBR(1) + 保留区(2047) + STM32直接访问区(100MB) + FAT32分区");
    
    #if FF_MULTI_PARTITION && FF_USE_MKFS
    /* 获取SD卡信息 */
    const tf_spi_dev_t* dev_info = TF_SPI_GetInfo();
    if (dev_info == NULL) {
        LOG_ERROR("MAIN", "无法获取SD卡信息");
        return FATFS_ERROR_NOT_READY;
    }
    
    uint32_t total_sectors = dev_info->block_count;
    uint32_t partition_start = FATFS_PARTITION_START_SECTOR;
    uint32_t partition_sectors = total_sectors - partition_start;
    
    /* 检查分区大小是否足够 */
    if (partition_sectors < 2048) {  /* 至少1MB */
        LOG_ERROR("MAIN", "SD卡容量不足，无法创建FAT32分区");
        LOG_ERROR("MAIN", "  SD卡容量: %d MB", dev_info->capacity_mb);
        LOG_ERROR("MAIN", "  FAT32分区需要至少: 2048 扇区");
        return FATFS_ERROR_NOT_ENOUGH_CORE;
    }
    
    /* 计算MB大小（整数运算，避免精度问题） */
    uint64_t partition_bytes = (uint64_t)partition_sectors * 512;
    uint32_t partition_mb = (uint32_t)(partition_bytes / (1024 * 1024));
    
    LOG_INFO("MAIN", "SD卡信息:");
    LOG_INFO("MAIN", "  总容量: %d MB (%d 扇区)", dev_info->capacity_mb, total_sectors);
    LOG_INFO("MAIN", "  保留区: 扇区 1 - %d (约1MB)", FATFS_RESERVED_AREA_SECTORS);
    LOG_INFO("MAIN", "  STM32直接访问区: 扇区 %d - %d (%d MB)",
             2048, 2048 + (FATFS_MCU_DIRECT_AREA_MB * 1024 * 1024 / 512) - 1,
             FATFS_MCU_DIRECT_AREA_MB);
    LOG_INFO("MAIN", "  FAT32分区: 扇区 %d - %d (%d MB)",
             partition_start, partition_start + partition_sectors - 1, partition_mb);
    
    /* 初始化磁盘 */
    DSTATUS stat = disk_initialize(0);
    if (stat & STA_NOINIT) {
        LOG_ERROR("MAIN", "磁盘初始化失败: 0x%02X", stat);
        return FATFS_ERROR_NOT_READY;
    }
    
    /* 清除缓存，确保disk_read和disk_write能够正确访问物理扇区0（MBR） */
    #if defined(FATFS_PARTITION_START_SECTOR) && (FATFS_PARTITION_START_SECTOR > 0)
    extern void disk_ioctl_spi_clear_partition_cache(void);
    disk_ioctl_spi_clear_partition_cache();
    #endif
    
    /* 读取MBR（直接读取物理扇区0，不使用disk_read避免扇区映射） */
    BYTE mbr_buf[512];
    TF_SPI_Status_t tf_status = TF_SPI_ReadBlock(0, mbr_buf);
    DRESULT dr = (tf_status == TF_SPI_OK) ? RES_OK : RES_ERROR;
    if (dr != RES_OK) {
        LOG_ERROR("MAIN", "读取MBR失败: %d", dr);
        return FATFS_ERROR_DISK_ERROR;
    }
    
    /* 清空分区表（保留MBR引导代码） */
    memset(mbr_buf + 446, 0, 64);  /* 清空4个分区表项 */
    
    /* MBR分区表定义 */
    #define MBR_PARTITION_TABLE_OFFSET  446
    #define PARTITION_ENTRY_SIZE        16
    #define PTE_BOOT_FLAG              0
    #define PTE_START_CHS              1
    #define PTE_SYSTEM_ID              4
    #define PTE_END_CHS                5
    #define PTE_START_LBA              8
    #define PTE_SIZE_LBA               12
    
    /* CHS参数（用于兼容性，实际使用LBA） */
    #define N_SEC_TRACK  63
    uint32_t n_hd = 255;
    uint32_t n_sc = N_SEC_TRACK;
    
    /* 创建分区1：FAT32分区（从FATFS_PARTITION_START_SECTOR开始） */
    BYTE* pte1 = mbr_buf + MBR_PARTITION_TABLE_OFFSET;
    pte1[PTE_BOOT_FLAG] = 0x00;  /* 非活动分区 */
    pte1[PTE_SYSTEM_ID] = 0x0C;  /* FAT32 LBA */
    
    /* 起始LBA（小端格式） */
    uint32_t start_lba = partition_start;
    pte1[PTE_START_LBA + 0] = (BYTE)(start_lba);
    pte1[PTE_START_LBA + 1] = (BYTE)(start_lba >> 8);
    pte1[PTE_START_LBA + 2] = (BYTE)(start_lba >> 16);
    pte1[PTE_START_LBA + 3] = (BYTE)(start_lba >> 24);
    
    /* 分区大小（小端格式） */
    uint32_t size_lba = partition_sectors;
    pte1[PTE_SIZE_LBA + 0] = (BYTE)(size_lba);
    pte1[PTE_SIZE_LBA + 1] = (BYTE)(size_lba >> 8);
    pte1[PTE_SIZE_LBA + 2] = (BYTE)(size_lba >> 16);
    pte1[PTE_SIZE_LBA + 3] = (BYTE)(size_lba >> 24);
    
    /* CHS参数（兼容性，实际使用LBA） */
    uint32_t cy = (start_lba / n_sc / n_hd);
    BYTE hd = (BYTE)(start_lba / n_sc % n_hd);
    BYTE sc = (BYTE)(start_lba % n_sc + 1);
    pte1[PTE_START_CHS + 0] = hd;
    pte1[PTE_START_CHS + 1] = sc | ((cy >> 2) & 0xC0);
    pte1[PTE_START_CHS + 2] = (BYTE)cy;
    
    uint32_t end_lba = start_lba + size_lba - 1;
    cy = (end_lba / n_sc / n_hd);
    hd = (BYTE)(end_lba / n_sc % n_hd);
    sc = (BYTE)(end_lba % n_sc + 1);
    pte1[PTE_END_CHS + 0] = hd;
    pte1[PTE_END_CHS + 1] = sc | ((cy >> 2) & 0xC0);
    pte1[PTE_END_CHS + 2] = (BYTE)cy;
    
    /* MBR签名（必需） */
    mbr_buf[510] = 0x55;
    mbr_buf[511] = 0xAA;
    
    /* 写入MBR（直接写入物理扇区0，不使用disk_write避免扇区映射） */
    tf_status = TF_SPI_WriteBlock(0, mbr_buf);
    if (tf_status != TF_SPI_OK) {
        LOG_ERROR("MAIN", "写入MBR失败: TF_SPI错误码=%d", tf_status);
        return FATFS_ERROR_DISK_ERROR;
    }
    
    /* 同步磁盘 */
    Delay_ms(100);
    
    /* 验证MBR（直接读取物理扇区0） */
    {
        BYTE verify_mbr[512];
        tf_status = TF_SPI_ReadBlock(0, verify_mbr);
        dr = (tf_status == TF_SPI_OK) ? RES_OK : RES_ERROR;
        if (dr == RES_OK) {
            BYTE* verify_pte = verify_mbr + MBR_PARTITION_TABLE_OFFSET;
            uint32_t verify_start = verify_pte[PTE_START_LBA + 0] | 
                                   (verify_pte[PTE_START_LBA + 1] << 8) |
                                   (verify_pte[PTE_START_LBA + 2] << 16) |
                                   (verify_pte[PTE_START_LBA + 3] << 24);
            uint32_t verify_size = verify_pte[PTE_SIZE_LBA + 0] | 
                                  (verify_pte[PTE_SIZE_LBA + 1] << 8) |
                                  (verify_pte[PTE_SIZE_LBA + 2] << 16) |
                                  (verify_pte[PTE_SIZE_LBA + 3] << 24);
            if (verify_start == partition_start && verify_size == partition_sectors) {
                LOG_INFO("MAIN", "MBR验证成功：分区1起始=%d，大小=%d", verify_start, verify_size);
            } else {
                LOG_WARN("MAIN", "MBR验证失败：期望起始=%d实际=%d，期望大小=%d实际=%d",
                        partition_start, verify_start, partition_sectors, verify_size);
            }
        }
    }
    
    /* 保存MCU保留区域信息 */
    g_mcu_reserved_area.start_sector = 2048;
    g_mcu_reserved_area.sector_count = FATFS_MCU_DIRECT_AREA_MB * 1024 * 1024 / 512;
    g_mcu_reserved_area.initialized = 1;
    
    LOG_INFO("MAIN", "MBR分区表创建成功");
    return FATFS_OK;
    
    #else
    LOG_ERROR("MAIN", "需要启用FF_MULTI_PARTITION和FF_USE_MKFS");
    LOG_ERROR("MAIN", "请在Middlewares/storage/fatfs/ffconf.h中设置：");
    LOG_ERROR("MAIN", "  #define FF_MULTI_PARTITION  1");
    LOG_ERROR("MAIN", "  #define FF_USE_MKFS         1");
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
}

/**
 * @brief 格式化FAT32分区
 */
static FatFS_Status_t FormatFAT32Partition(void)
{
    LOG_INFO("MAIN", "=== 格式化FAT32分区 ===");
    
    #if FF_MULTI_PARTITION && FF_USE_MKFS
    /* 获取SD卡信息 */
    const tf_spi_dev_t* dev_info = TF_SPI_GetInfo();
    if (dev_info == NULL) {
        LOG_ERROR("MAIN", "无法获取SD卡信息");
        return FATFS_ERROR_NOT_READY;
    }
    
    uint32_t partition_start = FATFS_PARTITION_START_SECTOR;
    uint32_t partition_sectors = dev_info->block_count - partition_start;
    uint64_t partition_bytes = (uint64_t)partition_sectors * 512;
    uint32_t partition_mb = (uint32_t)(partition_bytes / (1024 * 1024));
    
    LOG_INFO("MAIN", "格式化分区1（FAT32）...");
    LOG_INFO("MAIN", "  分区起始扇区: %d", partition_start);
    LOG_INFO("MAIN", "  分区大小: %d 扇区 (%d MB)", partition_sectors, partition_mb);
    LOG_INFO("MAIN", "  格式化可能需要几分钟，请耐心等待...");
    LOG_INFO("MAIN", "  格式化过程中LED会闪烁，表示程序正在运行");
    
    OLED_ShowString(2, 1, "Format P1...");
    OLED_ShowString(3, 1, "Please wait...");
    
    /* 格式化参数 */
    MKFS_PARM opt;
    opt.fmt = FM_FAT32;  /* FAT32格式 */
    opt.n_fat = 1;       /* 1个FAT表 */
    opt.align = 0;       /* 自动对齐 */
    opt.n_root = 0;     /* FAT32不需要根目录项数 */
    opt.au_size = 0;     /* 自动簇大小 */
    
    /* 格式化工作缓冲区 - 使用静态变量避免栈溢出 */
    static BYTE work[FF_MAX_SS];
    
    /* 在格式化前，先清除缓存，让f_mkfs能够正确读取MBR */
    #if defined(FATFS_PARTITION_START_SECTOR) && (FATFS_PARTITION_START_SECTOR > 0)
    extern void disk_ioctl_spi_clear_partition_cache(void);
    extern void disk_ioctl_spi_set_partition_sectors(uint32_t sectors);
    disk_ioctl_spi_clear_partition_cache();
    #endif
    
    FRESULT fr = f_mkfs("0:1:", &opt, work, sizeof(work));
    
    if (fr != FR_OK) {
        LOG_ERROR("MAIN", "格式化失败: %d", fr);
        return ConvertFatFSError(fr);
    }
    
    /* 同步磁盘，确保所有写入完成 */
    disk_ioctl(0, CTRL_SYNC, NULL);
    Delay_ms(100);
    
    /* 恢复MBR中的正确分区扇区数（f_mkfs可能会修改MBR） */
    {
        /* 直接读取物理MBR（扇区0），不使用disk_read避免扇区映射 */
        BYTE restore_mbr[512];
        TF_SPI_Status_t tf_status = TF_SPI_ReadBlock(0, restore_mbr);
        if (tf_status == TF_SPI_OK) {
            #define MBR_PARTITION_TABLE_OFFSET  446
            #define PTE_SIZE_LBA               12
            BYTE* restore_pte = restore_mbr + MBR_PARTITION_TABLE_OFFSET;
            uint32_t current_size = restore_pte[PTE_SIZE_LBA + 0] | 
                                   (restore_pte[PTE_SIZE_LBA + 1] << 8) |
                                   (restore_pte[PTE_SIZE_LBA + 2] << 16) |
                                   (restore_pte[PTE_SIZE_LBA + 3] << 24);
            
            if (current_size != partition_sectors) {
                LOG_WARN("MAIN", "检测到f_mkfs修改了MBR中的分区扇区数");
                LOG_WARN("MAIN", "  当前值: %d 扇区", current_size);
                LOG_WARN("MAIN", "  正确值: %d 扇区 (%d MB)", partition_sectors, partition_mb);
                LOG_INFO("MAIN", "恢复MBR中的正确分区扇区数...");
                
                /* 恢复正确的分区扇区数 */
                restore_pte[PTE_SIZE_LBA + 0] = (BYTE)(partition_sectors);
                restore_pte[PTE_SIZE_LBA + 1] = (BYTE)(partition_sectors >> 8);
                restore_pte[PTE_SIZE_LBA + 2] = (BYTE)(partition_sectors >> 16);
                restore_pte[PTE_SIZE_LBA + 3] = (BYTE)(partition_sectors >> 24);
                
                /* 重新计算并更新CHS参数 */
                uint32_t n_hd = 255;
                uint32_t n_sc = 63;
                uint32_t end_lba = partition_start + partition_sectors - 1;
                uint32_t cy = (end_lba / n_sc / n_hd);
                BYTE hd = (BYTE)(end_lba / n_sc % n_hd);
                BYTE sc = (BYTE)(end_lba % n_sc + 1);
                restore_pte[PTE_END_CHS + 0] = hd;
                restore_pte[PTE_END_CHS + 1] = sc | ((cy >> 2) & 0xC0);
                restore_pte[PTE_END_CHS + 2] = (BYTE)cy;
                
                /* 确保MBR签名正确 */
                restore_mbr[510] = 0x55;
                restore_mbr[511] = 0xAA;
                
                /* 直接写入物理MBR（扇区0），不使用disk_write避免扇区映射 */
                tf_status = TF_SPI_WriteBlock(0, restore_mbr);
                if (tf_status == TF_SPI_OK) {
                    Delay_ms(100);
                    /* 验证MBR是否写入成功 */
                    BYTE verify_mbr[512];
                    TF_SPI_Status_t verify_status = TF_SPI_ReadBlock(0, verify_mbr);
                    if (verify_status == TF_SPI_OK) {
                        uint32_t verify_size = verify_mbr[MBR_PARTITION_TABLE_OFFSET + PTE_SIZE_LBA + 0] | 
                                              (verify_mbr[MBR_PARTITION_TABLE_OFFSET + PTE_SIZE_LBA + 1] << 8) |
                                              (verify_mbr[MBR_PARTITION_TABLE_OFFSET + PTE_SIZE_LBA + 2] << 16) |
                                              (verify_mbr[MBR_PARTITION_TABLE_OFFSET + PTE_SIZE_LBA + 3] << 24);
                        if (verify_size != partition_sectors) {
                            LOG_WARN("MAIN", "MBR恢复后验证失败：期望=%d，实际=%d", partition_sectors, verify_size);
                        }
                    }
                } else {
                    LOG_ERROR("MAIN", "MBR恢复失败: TF_SPI错误码=%d", tf_status);
                }
            }
        }
    }
    
    /* 设置缓存为正确的分区扇区数 */
    #if defined(FATFS_PARTITION_START_SECTOR) && (FATFS_PARTITION_START_SECTOR > 0)
    extern void disk_ioctl_spi_set_partition_sectors(uint32_t sectors);
    disk_ioctl_spi_set_partition_sectors(partition_sectors);
    #endif
    
    LOG_INFO("MAIN", "FAT32分区格式化完成");
    return FATFS_OK;
    
    #else
    LOG_ERROR("MAIN", "需要启用FF_MULTI_PARTITION和FF_USE_MKFS");
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
}

/**
 * @brief 格式化分区（单分区方案）
 */
static FatFS_Status_t FormatPartition(void)
{
    LOG_INFO("MAIN", "=== 单分区格式化 ===");
    LOG_INFO("MAIN", "方案：MBR + 保留区(1MB) + STM32直接访问区(100MB) + FAT32分区");
    
    /* 1. 创建MBR分区表 */
    FatFS_Status_t status = CreateMBRPartition();
    if (status != FATFS_OK) {
        return status;
    }
    
    Delay_ms(500);
    
    /* 2. 格式化FAT32分区 */
    status = FormatFAT32Partition();
    if (status != FATFS_OK) {
        return status;
    }
    
    LOG_INFO("MAIN", "单分区格式化完成");
    return FATFS_OK;
}

/* ==================== 清除分区表 ==================== */

/**
 * @brief 清除MBR分区表（清空分区表数据，保留MBR签名）
 * @return FatFS_Status_t 错误码
 */
static FatFS_Status_t ClearMBRPartitionTable(void)
{
    LOG_INFO("MAIN", "=== 清除MBR分区表 ===");
    
    /* 检查SD卡状态 */
    const tf_spi_dev_t* dev_info = TF_SPI_GetInfo();
    if (dev_info == NULL || dev_info->state != TF_SPI_STATE_INITIALIZED) {
        LOG_WARN("MAIN", "SD卡未初始化，尝试重新初始化...");
        TF_SPI_Status_t init_status = TF_SPI_Init();
        if (init_status != TF_SPI_OK) {
            LOG_ERROR("MAIN", "SD卡初始化失败: %d，无法清除MBR分区表", init_status);
            return FATFS_ERROR_NOT_READY;
        }
    }
    
    /* 检查SD卡是否可用 */
    uint8_t status;
    TF_SPI_Status_t status_check = TF_SPI_SendStatus(&status);
    if (status_check != TF_SPI_OK) {
        LOG_WARN("MAIN", "SD卡状态检查失败: %d，可能已拔出", status_check);
        LOG_WARN("MAIN", "跳过清除MBR分区表操作");
        return FATFS_ERROR_NOT_READY;
    }
    
    /* 读取MBR（直接读取物理扇区0，不使用disk_read避免扇区映射） */
    BYTE mbr_buf[512];
    TF_SPI_Status_t tf_status = TF_SPI_ReadBlock(0, mbr_buf);
    if (tf_status != TF_SPI_OK) {
        LOG_ERROR("MAIN", "读取MBR失败: TF_SPI错误码=%d", tf_status);
        LOG_WARN("MAIN", "可能SD卡已拔出或状态异常，跳过清除操作");
        return FATFS_ERROR_DISK_ERROR;
    }
    
    /* 清空分区表（保留MBR引导代码和签名） */
    #define MBR_PARTITION_TABLE_OFFSET  446
    memset(mbr_buf + MBR_PARTITION_TABLE_OFFSET, 0, 64);  /* 清空4个分区表项 */
    
    /* 确保MBR签名正确 */
    mbr_buf[510] = 0x55;
    mbr_buf[511] = 0xAA;
    
    /* 写入MBR（直接写入物理扇区0，不使用disk_write避免扇区映射） */
    tf_status = TF_SPI_WriteBlock(0, mbr_buf);
    if (tf_status != TF_SPI_OK) {
        LOG_ERROR("MAIN", "写入MBR失败: TF_SPI错误码=%d", tf_status);
        return FATFS_ERROR_DISK_ERROR;
    }
    
    /* 同步磁盘 */
    Delay_ms(100);
    
    /* 验证MBR（直接读取物理扇区0） */
    {
        BYTE verify_mbr[512];
        tf_status = TF_SPI_ReadBlock(0, verify_mbr);
        if (tf_status == TF_SPI_OK) {
            /* 检查分区表是否已清空 */
            bool is_cleared = true;
            for (int i = 0; i < 64; i++) {
                if (verify_mbr[MBR_PARTITION_TABLE_OFFSET + i] != 0) {
                    is_cleared = false;
                    break;
                }
            }
            
            /* 检查MBR签名 */
            bool signature_ok = (verify_mbr[510] == 0x55 && verify_mbr[511] == 0xAA);
            
            if (is_cleared && signature_ok) {
                LOG_INFO("MAIN", "MBR分区表清除成功并验证通过");
            } else {
                LOG_WARN("MAIN", "MBR分区表清除后验证失败: 分区表已清空=%d, 签名正确=%d", 
                        is_cleared, signature_ok);
            }
        } else {
            LOG_WARN("MAIN", "MBR清除后无法验证");
        }
    }
    
    LOG_INFO("MAIN", "MBR分区表清除完成");
    return FATFS_OK;
}

/* ==================== 插拔卡检测 ==================== */

/**
 * @brief 检测SD卡是否存在（初始化时调用）
 * @return true: SD卡存在，false: SD卡不存在
 */
static bool CheckSDCardPresent(void)
{
    /* 尝试初始化SD卡，如果失败则说明SD卡不存在 */
    TF_SPI_Status_t tf_status = TF_SPI_Init();
    if (tf_status == TF_SPI_OK) {
        const tf_spi_dev_t* dev_info = TF_SPI_GetInfo();
        if (dev_info != NULL && dev_info->state == TF_SPI_STATE_INITIALIZED) {
            LOG_INFO("MAIN", "SD卡检测成功: %d MB", dev_info->capacity_mb);
            return true;
        }
    }
    
    LOG_WARN("MAIN", "SD卡检测失败: 错误码=%d", tf_status);
    LOG_WARN("MAIN", "请检查SD卡是否已插入");
    return false;
}

/**
 * @brief 检查SD卡是否满足使用要求
 * @return true: SD卡满足使用要求，false: SD卡不满足使用要求
 * @note 检查SD卡容量、状态等是否满足使用要求
 */
static bool CheckSDCardUsable(void)
{
    const tf_spi_dev_t* dev_info = TF_SPI_GetInfo();
    if (dev_info == NULL || dev_info->state != TF_SPI_STATE_INITIALIZED) {
        LOG_WARN("MAIN", "SD卡未初始化");
        return false;
    }
    
    /* 检查SD卡容量是否足够（至少需要100MB用于STM32直接访问区） */
    if (dev_info->capacity_mb < 200) {
        LOG_WARN("MAIN", "SD卡容量不足: %d MB，至少需要200MB", dev_info->capacity_mb);
        return false;
    }
    
    /* 检查SD卡状态 */
    uint8_t status;
    TF_SPI_Status_t tf_status = TF_SPI_SendStatus(&status);
    if (tf_status != TF_SPI_OK) {
        LOG_WARN("MAIN", "SD卡状态检查失败: %d", tf_status);
        return false;
    }
    
    LOG_INFO("MAIN", "SD卡满足使用要求: %d MB", dev_info->capacity_mb);
    return true;
}

/**
 * @brief 检测SD卡是否已拔出（循环中调用）
 * @return true: SD卡已拔出，false: SD卡仍然存在
 */
static bool CheckSDCardRemoved(void)
{
    const tf_spi_dev_t* dev_info = TF_SPI_GetInfo();
    if (dev_info == NULL || dev_info->state != TF_SPI_STATE_INITIALIZED) {
        return true;  /* SD卡已拔出或未初始化 */
    }
    
    /* 尝试发送CMD13检查SD卡状态（使用较短的超时时间，避免长时间阻塞） */
    uint8_t status;
    TF_SPI_Status_t tf_status = TF_SPI_SendStatus(&status);
    if (tf_status != TF_SPI_OK) {
        /* 状态检查失败，可能是SD卡已拔出 */
        return true;
    }
    
    return false;  /* SD卡仍然存在 */
}

/**
 * @brief 挂载文件系统并处理错误
 * @param mount_path 挂载路径
 * @return FatFS_Status_t 错误码
 */
static FatFS_Status_t MountFileSystem(const char* mount_path)
{
    FatFS_Status_t fatfs_status = FatFS_Mount(FATFS_VOLUME_SPI, mount_path);
    
    if (fatfs_status == FATFS_ERROR_NO_FILESYSTEM) {
        /* 无分区：格式化处理 */
        LOG_INFO("MAIN", "检测到SD卡未格式化，开始格式化...");
        LOG_WARN("MAIN", "警告：格式化将清空SD卡所有数据！");
        
        /* 格式化分区 */
        FatFS_Status_t format_status = FormatPartition();
        if (format_status != FATFS_OK) {
            LOG_ERROR("MAIN", "格式化失败: %d", format_status);
            return format_status;
        }
        
        LOG_INFO("MAIN", "格式化完成");
        
        /* 重新挂载 */
        fatfs_status = FatFS_Mount(FATFS_VOLUME_SPI, mount_path);
    } else if (fatfs_status != FATFS_OK) {
        /* 其他原因：报挂载失败 */
        LOG_ERROR("MAIN", "文件系统挂载失败: %d", fatfs_status);
    }
    
    return fatfs_status;
}

/**
 * @brief 处理SD卡拔卡情况（等待插回并重新挂载）
 * @param mount_path 挂载路径
 * @return true: 处理成功，false: 处理失败
 */
static bool HandleSDCardRemoval(const char* mount_path)
{
    LOG_WARN("MAIN", "检测到SD卡已拔出");
    OLED_Clear();
    OLED_ShowString(1, 1, "SD Card");
    OLED_ShowString(2, 1, "Removed!");
    
    /* 卸载文件系统 */
    FatFS_Unmount(FATFS_VOLUME_SPI);
    Delay_ms(100);
    
    /* 反初始化TF_SPI（清除状态，避免状态检查失败） */
    TF_SPI_Deinit();
    Delay_ms(100);
    
    /* 等待SD卡重新插入（检测TF_SPI初始化状态） */
    LOG_INFO("MAIN", "等待SD卡重新插入...");
    uint32_t wait_count = 0;
    while (1) {
        /* 尝试初始化SD卡，如果成功则说明SD卡已插入 */
        TF_SPI_Status_t tf_status = TF_SPI_Init();
        if (tf_status == TF_SPI_OK) {
            const tf_spi_dev_t* dev_info = TF_SPI_GetInfo();
            if (dev_info != NULL && dev_info->state == TF_SPI_STATE_INITIALIZED) {
                LOG_INFO("MAIN", "检测到SD卡重新插入: %d MB", dev_info->capacity_mb);
                break;  /* SD卡已插入并初始化成功 */
            }
        }
        
        /* 每2秒显示一次等待信息 */
        if (wait_count % 4 == 0) {
            LOG_INFO("MAIN", "等待SD卡插入... (已等待 %d 秒)", wait_count / 2);
        }
        
        LED1_Toggle();
        Delay_ms(500);
        wait_count++;
    }
    
    LOG_INFO("MAIN", "检查SD卡是否满足使用要求...");
    
    /* 每次插卡回去会检查SD卡是否满足使用 */
    while (1) {
        /* 检查SD卡是否满足使用要求 */
        if (!CheckSDCardUsable()) {
            LOG_WARN("MAIN", "SD卡不满足使用要求，等待更换...");
            OLED_ShowString(3, 1, "Not Usable!");
            LED1_Toggle();
            Delay_ms(500);
            
            /* 重新检测SD卡是否拔出 */
            if (CheckSDCardRemoved()) {
                LOG_WARN("MAIN", "SD卡已拔出，重新等待插入...");
                TF_SPI_Deinit();
                Delay_ms(100);
                return HandleSDCardRemoval(mount_path);  /* 递归处理 */
            }
            continue;
        }
        
        /* SD卡满足使用要求，退出检查循环 */
        LOG_INFO("MAIN", "SD卡满足使用要求");
        break;
    }
    
    /* 如果SD卡满足使用要求，重新挂载文件系统 */
    if (CheckSDCardUsable()) {
        LOG_INFO("MAIN", "重新挂载文件系统...");
        FatFS_Status_t fatfs_status = MountFileSystem(mount_path);
        
        if (fatfs_status != FATFS_OK) {
            LOG_ERROR("MAIN", "文件系统重新挂载失败: %d", fatfs_status);
            OLED_Clear();
            OLED_ShowString(1, 1, "Mount Fail");
            OLED_ShowString(2, 1, "Error!");
            return false;
        } else {
            LOG_INFO("MAIN", "文件系统重新挂载成功");
            OLED_Clear();
            OLED_ShowString(1, 1, "Mount OK");
            return true;
        }
    }
    
    return false;
}


/**
 * @brief STM32直接操作区边界测试（开始位置写1MB，刚好到结束地址写1MB，不越界）
 */
static void TestMCUAreaBoundary(void)
{
    LOG_INFO("MAIN", "=== STM32直接操作区边界测试 ===");
    
    /* 如果未初始化或值异常，重新初始化 */
    if (!g_mcu_reserved_area.initialized || 
        g_mcu_reserved_area.start_sector != 2048 ||
        g_mcu_reserved_area.sector_count != (FATFS_MCU_DIRECT_AREA_MB * 1024 * 1024 / 512)) {
        LOG_WARN("MAIN", "MCU保留区域信息异常，重新初始化");
        g_mcu_reserved_area.start_sector = 2048;
        g_mcu_reserved_area.sector_count = FATFS_MCU_DIRECT_AREA_MB * 1024 * 1024 / 512;
        g_mcu_reserved_area.initialized = 1;
    }
    
    uint32_t start_sector = g_mcu_reserved_area.start_sector;
    uint32_t sector_count = g_mcu_reserved_area.sector_count;
    uint32_t end_sector = start_sector + sector_count - 1;  /* 结束扇区（包含） */
    
    /* 1MB = 2048扇区 */
    const uint32_t test_size_sectors = 1 * 1024 * 1024 / 512;  /* 1MB的扇区数 */
    
    LOG_INFO("MAIN", "MCU直接操作区信息:");
    LOG_INFO("MAIN", "  起始扇区: %d", start_sector);
    LOG_INFO("MAIN", "  结束扇区: %d", end_sector);
    LOG_INFO("MAIN", "  扇区数量: %d", sector_count);
    LOG_INFO("MAIN", "  大小: %d MB", FATFS_MCU_DIRECT_AREA_MB);
    LOG_INFO("MAIN", "  测试大小: 1 MB (%d 扇区)", test_size_sectors);
    
    /* 准备测试数据缓冲区（512字节） */
    uint8_t write_buffer[512];
    uint8_t verify_buffer[512];
    
    /* ========== 测试1：开始位置写1MB ========== */
    LOG_INFO("MAIN", "测试1：开始位置写1MB（扇区 %d - %d）", 
             start_sector, start_sector + test_size_sectors - 1);
    
    for (uint32_t i = 0; i < test_size_sectors; i++) {
        uint32_t sector_addr = start_sector + i;
        
        /* 准备扇区数据（标记为开始位置测试） */
        for (int j = 0; j < 512; j++) {
            write_buffer[j] = (uint8_t)((sector_addr + j) & 0xFF);
        }
        write_buffer[0] = 0xAA;  /* 标记：开始位置 */
        write_buffer[1] = 0x55;
        
        /* 写入扇区 */
        TF_SPI_Status_t tf_status = TF_SPI_WriteBlock(sector_addr, write_buffer);
        if (tf_status != TF_SPI_OK) {
            LOG_ERROR("MAIN", "写入扇区 %d 失败: %d", sector_addr, tf_status);
            return;
        }
        
        /* 每10%显示一次进度 */
        if ((i + 1) % (test_size_sectors / 10) == 0 || (i + 1) == test_size_sectors) {
            uint32_t progress = ((i + 1) * 100) / test_size_sectors;
            LOG_INFO("MAIN", "  写入进度: %d%% (%d/%d 扇区)", progress, i + 1, test_size_sectors);
        }
    }
    
    LOG_INFO("MAIN", "开始位置1MB写入完成");
    
    /* 验证开始位置的几个扇区 */
    LOG_INFO("MAIN", "验证开始位置数据...");
    uint32_t verify_ok_start = 0;
    const uint32_t verify_count = 10;
    for (uint32_t i = 0; i < verify_count && i < test_size_sectors; i++) {
        uint32_t sector_addr = start_sector + i;
        
        TF_SPI_Status_t tf_status = TF_SPI_ReadBlock(sector_addr, verify_buffer);
        if (tf_status != TF_SPI_OK) {
            LOG_ERROR("MAIN", "读取扇区 %d 失败: %d", sector_addr, tf_status);
            continue;
        }
        
        /* 验证标记 */
        if (verify_buffer[0] == 0xAA && verify_buffer[1] == 0x55) {
            verify_ok_start++;
        } else {
            LOG_ERROR("MAIN", "扇区 %d 标记验证失败: 期望 0xAA55，实际 0x%02X%02X",
                     sector_addr, verify_buffer[0], verify_buffer[1]);
        }
    }
    LOG_INFO("MAIN", "开始位置验证: %d/%d 扇区通过", verify_ok_start, verify_count);
    
    /* ========== 测试2：刚好到结束地址写1MB（不越界） ========== */
    /* 计算结束位置的起始扇区：结束扇区 - 1MB + 1 = 结束扇区 - (1MB扇区数 - 1) */
    uint32_t end_test_start_sector = end_sector - test_size_sectors + 1;
    
    LOG_INFO("MAIN", "测试2：刚好到结束地址写1MB（不越界）");
    LOG_INFO("MAIN", "  写入范围: 扇区 %d - %d", end_test_start_sector, end_sector);
    LOG_INFO("MAIN", "  边界检查: 起始扇区 %d >= 开始扇区 %d", end_test_start_sector, start_sector);
    LOG_INFO("MAIN", "  边界检查: 结束扇区 %d <= 结束扇区 %d", end_sector, end_sector);
    
    /* 边界检查：确保不越界 */
    if (end_test_start_sector < start_sector) {
        LOG_ERROR("MAIN", "边界检查失败：结束测试起始扇区 %d < 开始扇区 %d", 
                 end_test_start_sector, start_sector);
        return;
    }
    
    if (end_sector >= start_sector + sector_count) {
        LOG_ERROR("MAIN", "边界检查失败：结束扇区 %d >= 区域结束 %d", 
                 end_sector, start_sector + sector_count);
        return;
    }
    
    for (uint32_t i = 0; i < test_size_sectors; i++) {
        uint32_t sector_addr = end_test_start_sector + i;
        
        /* 确保不越界 */
        if (sector_addr > end_sector) {
            LOG_ERROR("MAIN", "边界检查失败：扇区 %d > 结束扇区 %d", sector_addr, end_sector);
            return;
        }
        
        /* 准备扇区数据（标记为结束位置测试） */
        for (int j = 0; j < 512; j++) {
            write_buffer[j] = (uint8_t)((sector_addr + j) & 0xFF);
        }
        write_buffer[0] = 0xBB;  /* 标记：结束位置 */
        write_buffer[1] = 0x66;
        
        /* 写入扇区 */
        TF_SPI_Status_t tf_status = TF_SPI_WriteBlock(sector_addr, write_buffer);
        if (tf_status != TF_SPI_OK) {
            LOG_ERROR("MAIN", "写入扇区 %d 失败: %d", sector_addr, tf_status);
            return;
        }
        
        /* 每10%显示一次进度 */
        if ((i + 1) % (test_size_sectors / 10) == 0 || (i + 1) == test_size_sectors) {
            uint32_t progress = ((i + 1) * 100) / test_size_sectors;
            LOG_INFO("MAIN", "  写入进度: %d%% (%d/%d 扇区)", progress, i + 1, test_size_sectors);
        }
    }
    
    LOG_INFO("MAIN", "结束位置1MB写入完成");
    
    /* 验证结束位置的几个扇区 */
    LOG_INFO("MAIN", "验证结束位置数据...");
    uint32_t verify_ok_end = 0;
    for (uint32_t i = 0; i < verify_count && i < test_size_sectors; i++) {
        uint32_t sector_addr = end_sector - verify_count + 1 + i;
        
        /* 确保不越界 */
        if (sector_addr > end_sector) {
            break;
        }
        
        TF_SPI_Status_t tf_status = TF_SPI_ReadBlock(sector_addr, verify_buffer);
        if (tf_status != TF_SPI_OK) {
            LOG_ERROR("MAIN", "读取扇区 %d 失败: %d", sector_addr, tf_status);
            continue;
        }
        
        /* 验证标记 */
        if (verify_buffer[0] == 0xBB && verify_buffer[1] == 0x66) {
            verify_ok_end++;
        } else {
            LOG_ERROR("MAIN", "扇区 %d 标记验证失败: 期望 0xBB66，实际 0x%02X%02X",
                     sector_addr, verify_buffer[0], verify_buffer[1]);
        }
    }
    LOG_INFO("MAIN", "结束位置验证: %d/%d 扇区通过", verify_ok_end, verify_count);
    
    /* 测试结果 */
    if (verify_ok_start == verify_count && verify_ok_end == verify_count) {
        LOG_INFO("MAIN", "STM32直接操作区边界测试成功");
        LOG_INFO("MAIN", "  开始位置测试: 通过");
        LOG_INFO("MAIN", "  结束位置测试: 通过（未越界）");
    } else {
        LOG_WARN("MAIN", "STM32直接操作区边界测试部分失败");
        LOG_WARN("MAIN", "  开始位置验证: %d/%d", verify_ok_start, verify_count);
        LOG_WARN("MAIN", "  结束位置验证: %d/%d", verify_ok_end, verify_count);
    }
}

/* ==================== 文件夹测试 ==================== */

/**
 * @brief 文件夹测试（处理同名文件夹）
 */
static void TestDirectoryCreation(void)
{
    LOG_INFO("MAIN", "=== 文件夹测试 ===");
    
    const char* test_dir = "0:TESTDIR";
    FatFS_Status_t status;
    
    /* 1. 创建文件夹 */
    LOG_INFO("MAIN", "1. 创建文件夹: %s", test_dir);
    status = FatFS_DirCreate(test_dir);
    if (status == FATFS_OK) {
        LOG_INFO("MAIN", "文件夹创建成功");
    } else if (status == FATFS_ERROR_EXIST) {
        LOG_WARN("MAIN", "文件夹已存在，继续使用现有文件夹");
    } else {
        LOG_ERROR("MAIN", "文件夹创建失败: %d", status);
        return;
    }
    
    /* 2. 尝试再次创建同名文件夹 */
    LOG_INFO("MAIN", "2. 尝试再次创建同名文件夹: %s", test_dir);
    status = FatFS_DirCreate(test_dir);
    if (status == FATFS_ERROR_EXIST) {
        LOG_INFO("MAIN", "正确处理：同名文件夹已存在，返回EXIST错误");
    } else if (status == FATFS_OK) {
        LOG_WARN("MAIN", "警告：同名文件夹创建成功（可能覆盖了原有文件夹）");
    } else {
        LOG_ERROR("MAIN", "创建失败: %d", status);
    }
    
    LOG_INFO("MAIN", "文件夹测试完成");
}

/* ==================== 文件测试 ==================== */

/**
 * @brief 文件测试（处理同名文件、增量内容）
 */
static void TestFileOperations(void)
{
    LOG_INFO("MAIN", "=== 文件测试 ===");
    
    FIL file;
    FatFS_Status_t status;
    uint32_t bytes_written, bytes_read;
    const char* test_file = "0:TEST.TXT";
    const char* test_data1 = "First write: Hello, FatFS!";
    const char* test_data2 = "\r\nSecond write: This is appended content.";
    uint8_t read_buffer[200];
    
    /* 1. 创建并写入文件 */
    LOG_INFO("MAIN", "1. 创建并写入文件: %s", test_file);
    status = FatFS_FileOpen(&file, test_file, FA_WRITE | FA_CREATE_ALWAYS);
    if (status != FATFS_OK) {
        LOG_ERROR("MAIN", "打开文件失败: %d", status);
        return;
    }
    
    status = FatFS_FileWrite(&file, test_data1, strlen(test_data1), &bytes_written);
    if (status != FATFS_OK) {
        LOG_ERROR("MAIN", "写入文件失败: %d", status);
        FatFS_FileClose(&file);
        return;
    }
    FatFS_FileSync(&file);
    FatFS_FileClose(&file);
    LOG_INFO("MAIN", "首次写入成功: %d 字节", bytes_written);
    
    /* 2. 尝试再次创建同名文件（FA_CREATE_ALWAYS会覆盖） */
    LOG_INFO("MAIN", "2. 尝试再次创建同名文件（FA_CREATE_ALWAYS）");
    status = FatFS_FileOpen(&file, test_file, FA_WRITE | FA_CREATE_ALWAYS);
    if (status == FATFS_OK) {
        const char* overwrite_data = "Overwritten content";
        FatFS_FileWrite(&file, overwrite_data, strlen(overwrite_data), &bytes_written);
        FatFS_FileSync(&file);
        FatFS_FileClose(&file);
        LOG_INFO("MAIN", "同名文件已覆盖: %d 字节", bytes_written);
    } else {
        LOG_ERROR("MAIN", "打开文件失败: %d", status);
    }
    
    /* 3. 增量内容测试（追加模式） */
    LOG_INFO("MAIN", "3. 增量内容测试（追加模式）");
    status = FatFS_FileOpen(&file, test_file, FA_WRITE | FA_OPEN_ALWAYS);
    if (status == FATFS_OK) {
        /* 定位到文件末尾 */
        FSIZE_t file_size = f_size(&file);
        if (file_size > 0) {
            FatFS_FileSeek(&file, (uint32_t)file_size);
        }
        
        /* 追加写入 */
        status = FatFS_FileWrite(&file, test_data2, strlen(test_data2), &bytes_written);
        if (status == FATFS_OK) {
            FatFS_FileSync(&file);
            LOG_INFO("MAIN", "追加写入成功: %d 字节", bytes_written);
        }
        FatFS_FileClose(&file);
    } else {
        LOG_ERROR("MAIN", "打开文件失败: %d", status);
    }
    
    /* 4. 读取文件验证 */
    LOG_INFO("MAIN", "4. 读取文件验证: %s", test_file);
    status = FatFS_FileOpen(&file, test_file, FA_READ);
    if (status == FATFS_OK) {
        memset(read_buffer, 0, sizeof(read_buffer));
        status = FatFS_FileRead(&file, read_buffer, sizeof(read_buffer) - 1, &bytes_read);
        if (status == FATFS_OK) {
            read_buffer[bytes_read] = '\0';
            LOG_INFO("MAIN", "读取成功: %d 字节", bytes_read);
            LOG_INFO("MAIN", "文件内容: %s", read_buffer);
        }
        FatFS_FileClose(&file);
    } else {
        LOG_ERROR("MAIN", "打开文件失败: %d", status);
    }
    
    LOG_INFO("MAIN", "文件测试完成");
}

/* ==================== 重命名测试 ==================== */

/**
 * @brief 重命名测试
 */
static void TestRename(void)
{
    LOG_INFO("MAIN", "=== 重命名测试 ===");
    
    const char* old_name = "0:TEST.TXT";
    const char* new_name = "0:RENAME.TXT";
    FatFS_Status_t status;
    
    /* 1. 确保源文件存在 */
    FIL file;
    status = FatFS_FileOpen(&file, old_name, FA_WRITE | FA_CREATE_ALWAYS);
    if (status == FATFS_OK) {
        const char* data = "Test file for rename";
        uint32_t bytes_written;
        FatFS_FileWrite(&file, data, strlen(data), &bytes_written);
        FatFS_FileSync(&file);
        FatFS_FileClose(&file);
        LOG_INFO("MAIN", "创建源文件: %s", old_name);
    }
    
    /* 2. 重命名文件 */
    LOG_INFO("MAIN", "2. 重命名文件: %s -> %s", old_name, new_name);
    status = FatFS_FileRename(old_name, new_name);
    if (status == FATFS_OK) {
        LOG_INFO("MAIN", "重命名成功");
    } else {
        LOG_ERROR("MAIN", "重命名失败: %d", status);
    }
    
    /* 3. 验证新文件存在 */
    LOG_INFO("MAIN", "3. 验证新文件存在: %s", new_name);
    status = FatFS_FileOpen(&file, new_name, FA_READ);
    if (status == FATFS_OK) {
        LOG_INFO("MAIN", "新文件存在，重命名验证成功");
        FatFS_FileClose(&file);
    } else {
        LOG_ERROR("MAIN", "新文件不存在，重命名可能失败");
    }
    
    LOG_INFO("MAIN", "重命名测试完成");
}

/* ==================== 删除测试 ==================== */

/**
 * @brief 删除测试
 */
static void TestDelete(void)
{
    LOG_INFO("MAIN", "=== 删除测试 ===");
    
    const char* test_file = "0:RENAME.TXT";
    const char* test_dir = "0:TESTDIR";
    FatFS_Status_t status;
    
    /* 1. 删除文件 */
    LOG_INFO("MAIN", "1. 删除文件: %s", test_file);
    status = FatFS_FileDelete(test_file);
    if (status == FATFS_OK) {
        LOG_INFO("MAIN", "文件删除成功");
    } else if (status == FATFS_ERROR_NO_FILE || status == FATFS_ERROR_NO_PATH) {
        LOG_WARN("MAIN", "文件不存在，可能已被删除");
    } else {
        LOG_ERROR("MAIN", "文件删除失败: %d", status);
    }
    
    /* 2. 删除文件夹 */
    LOG_INFO("MAIN", "2. 删除文件夹: %s", test_dir);
    status = FatFS_DirDelete(test_dir);
    if (status == FATFS_OK) {
        LOG_INFO("MAIN", "文件夹删除成功");
    } else if (status == FATFS_ERROR_NO_FILE || status == FATFS_ERROR_NO_PATH) {
        LOG_WARN("MAIN", "文件夹不存在，可能已被删除");
    } else if (status == FATFS_ERROR_DENIED) {
        LOG_WARN("MAIN", "文件夹不为空或访问被拒绝，无法删除");
    } else {
        LOG_ERROR("MAIN", "文件夹删除失败: %d", status);
    }
    
    LOG_INFO("MAIN", "删除测试完成");
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
    log_config.level = LOG_LEVEL_INFO;  /* 使用INFO级别，关闭DEBUG输出 */
    log_config.enable_timestamp = 0;
    log_config.enable_module = 1;
    log_config.enable_color = 0;
    
    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK) {
        ErrorHandler_Handle(log_status, "LOG");
    }
    
    LOG_INFO("MAIN", "=== Flash08 - TF卡集成FatFS文件系统示例（单分区方案） ===");
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
    
    /* ========== 步骤9：SD初始化（含插拔卡检测） ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "TF Card Init");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== SD初始化（含插拔卡检测） ===");
    
    /* 检测SD卡是否存在（初始化时完成） */
    while (!CheckSDCardPresent()) {
        LOG_WARN("MAIN", "等待SD卡插入...");
        OLED_ShowString(2, 1, "No SD Card!");
        OLED_ShowString(3, 1, "Waiting...");
        LED1_Toggle();
        Delay_ms(500);
    }
    
    /* 检查SD卡是否满足使用要求 */
    while (!CheckSDCardUsable()) {
        LOG_WARN("MAIN", "SD卡不满足使用要求，等待更换...");
        OLED_ShowString(2, 1, "SD Card");
        OLED_ShowString(3, 1, "Not Usable!");
        LED1_Toggle();
        Delay_ms(500);
        
        /* 重新检测SD卡 */
        if (!CheckSDCardPresent()) {
            continue;  /* SD卡已拔出，重新等待插入 */
        }
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
    
    Delay_ms(1000);
    
    /* ========== 步骤10：强制格式化检查 ========== */
    char mount_path[8];
    snprintf(mount_path, sizeof(mount_path), "0:");
    
    #if FATFS_FORCE_FORMAT
    /* 强制格式化开关：1不检查直接格式化，0跳过 */
    LOG_WARN("MAIN", "[强制格式化] 强制格式化模式已启用！");
    LOG_WARN("MAIN", "[警告] 格式化将清空SD卡所有数据！");
    
    /* 先尝试卸载文件系统（如果已挂载） */
    FatFS_Unmount(FATFS_VOLUME_SPI);
    Delay_ms(100);
    
    /* 格式化分区 */
    fatfs_status = FormatPartition();
    if (fatfs_status != FATFS_OK) {
        LOG_ERROR("MAIN", "格式化失败: %d", fatfs_status);
        ErrorHandler_Handle(fatfs_status, "FatFS");
        OLED_Clear();
        OLED_ShowString(1, 1, "Format Fail");
        OLED_ShowString(2, 1, "Error!");
        while(1) { 
            LED1_Toggle();
            Delay_ms(500); 
        }
    }
    
    LOG_INFO("MAIN", "格式化完成");
    
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
    #endif
    
    /* ========== 步骤11：挂载SD分区 ========== */
    LOG_INFO("MAIN", "=== 挂载SD分区 ===");
    fatfs_status = MountFileSystem(mount_path);
    
    if (fatfs_status != FATFS_OK) {
        /* 其他原因：报挂载失败 */
        LOG_ERROR("MAIN", "文件系统挂载失败: %d", fatfs_status);
        ErrorHandler_Handle(fatfs_status, "FatFS");
        OLED_Clear();
        OLED_ShowString(1, 1, "Mount Fail");
        OLED_ShowString(2, 1, "Error!");
        while(1) { 
            LED1_Toggle();
            Delay_ms(500); 
        }
    }
    
    LOG_INFO("MAIN", "文件系统挂载成功");
    OLED_ShowString(3, 1, "Mount OK");
    
    /* 显示文件系统信息 */
    uint32_t free_clusters, total_clusters;
    fatfs_status = FatFS_GetFreeSpace(FATFS_VOLUME_SPI, mount_path, &free_clusters, &total_clusters);
    if (fatfs_status == FATFS_OK) {
        /* 调试：详细检查MBR和disk_ioctl */
        #if FATFS_DETAILED_DEBUG
        {
            /* 1. 直接读取物理MBR（扇区0），不使用disk_read避免扇区映射 */
            BYTE debug_mbr[512];
            TF_SPI_Status_t tf_status = TF_SPI_ReadBlock(0, debug_mbr);
            if (tf_status == TF_SPI_OK) {
                #define MBR_PARTITION_TABLE_OFFSET  446
                #define PTE_START_LBA               8
                #define PTE_SIZE_LBA               12
                BYTE* debug_pte = debug_mbr + MBR_PARTITION_TABLE_OFFSET;
                uint32_t mbr_start = debug_pte[PTE_START_LBA + 0] | 
                                    (debug_pte[PTE_START_LBA + 1] << 8) |
                                    (debug_pte[PTE_START_LBA + 2] << 16) |
                                    (debug_pte[PTE_START_LBA + 3] << 24);
                uint32_t mbr_sectors = debug_pte[PTE_SIZE_LBA + 0] | 
                                      (debug_pte[PTE_SIZE_LBA + 1] << 8) |
                                      (debug_pte[PTE_SIZE_LBA + 2] << 16) |
                                      (debug_pte[PTE_SIZE_LBA + 3] << 24);
                LOG_INFO("MAIN", "物理MBR（扇区0）直接读取:");
                LOG_INFO("MAIN", "  分区起始扇区: %d (原始字节: %02X %02X %02X %02X)",
                         mbr_start,
                         debug_pte[PTE_START_LBA + 0],
                         debug_pte[PTE_START_LBA + 1],
                         debug_pte[PTE_START_LBA + 2],
                         debug_pte[PTE_START_LBA + 3]);
                LOG_INFO("MAIN", "  分区扇区数: %d (原始字节: %02X %02X %02X %02X)",
                         mbr_sectors,
                         debug_pte[PTE_SIZE_LBA + 0],
                         debug_pte[PTE_SIZE_LBA + 1],
                         debug_pte[PTE_SIZE_LBA + 2],
                         debug_pte[PTE_SIZE_LBA + 3]);
                uint64_t mbr_bytes = (uint64_t)mbr_sectors * 512;
                uint32_t mbr_mb = (uint32_t)(mbr_bytes / (1024 * 1024));
                LOG_INFO("MAIN", "  MBR中的分区大小: %d MB", mbr_mb);
            }
            
            /* 1.5 检查缓存值（通过disk_ioctl间接检查） */
            /* 注意：不能直接访问static变量，需要通过函数或disk_ioctl检查 */
            
            /* 2. 检查disk_ioctl返回的值（多次调用，看是否有变化） */
            for (int i = 0; i < 3; i++) {
                LBA_t sector_count = 0;
                DRESULT dr_ioctl = disk_ioctl(0, GET_SECTOR_COUNT, &sector_count);
                if (dr_ioctl == RES_OK) {
                    uint64_t sector_bytes = (uint64_t)sector_count * 512;
                    uint32_t sector_mb = (uint32_t)(sector_bytes / (1024 * 1024));
                    LOG_INFO("MAIN", "disk_ioctl(GET_SECTOR_COUNT)调用%d: %lu 扇区 (%lu MB)", 
                             i + 1, (unsigned long)sector_count, sector_mb);
                    
                    /* 分析这个值的来源 */
                    if (i == 0) {
                        const tf_spi_dev_t* dev_info_debug = TF_SPI_GetInfo();
                        if (dev_info_debug != NULL) {
                            LOG_INFO("MAIN", "  分析: SD卡总扇区数=%d, 差值=%d", 
                                     dev_info_debug->block_count,
                                     dev_info_debug->block_count - (uint32_t)sector_count);
                        }
                    }
                }
                Delay_ms(10);
            }
            
            /* 3. 检查期望值 */
            const tf_spi_dev_t* dev_info = TF_SPI_GetInfo();
            if (dev_info != NULL) {
                uint32_t expected_sectors = dev_info->block_count - FATFS_PARTITION_START_SECTOR;
                uint64_t expected_bytes = (uint64_t)expected_sectors * 512;
                uint32_t expected_mb = (uint32_t)(expected_bytes / (1024 * 1024));
                LOG_INFO("MAIN", "期望的分区大小: %d 扇区 (%d MB)", expected_sectors, expected_mb);
                LOG_INFO("MAIN", "SD卡总扇区数: %d", dev_info->block_count);
                LOG_INFO("MAIN", "分区起始扇区: %d", FATFS_PARTITION_START_SECTOR);
            }
        }
        #endif /* FATFS_DETAILED_DEBUG */
        
        uint64_t total_bytes = 0;
        FatFS_GetTotalSpace(FATFS_VOLUME_SPI, mount_path, &total_bytes);
        
        /* 正确计算空闲空间：使用FatFS内部的簇大小 */
        FATFS* fs_calc;
        DWORD free_clusters_calc = 0;
        FRESULT fr_calc = f_getfree(mount_path, &free_clusters_calc, &fs_calc);
        uint64_t free_bytes = 0;
        if (fr_calc == FR_OK && fs_calc != NULL) {
            DWORD cluster_size_bytes = fs_calc->csize * 512;
            free_bytes = (uint64_t)free_clusters_calc * (uint64_t)cluster_size_bytes;
            #if FATFS_DETAILED_DEBUG
            LOG_INFO("MAIN", "空闲空间计算: free_clusters=%lu, cluster_size=%lu, free_bytes=%lu", 
                     (unsigned long)free_clusters_calc, (unsigned long)cluster_size_bytes, 
                     (unsigned long)free_bytes);
            #endif
        } else {
            #if FATFS_DETAILED_DEBUG
            LOG_WARN("MAIN", "f_getfree失败: fr=%d, fs_calc=%p", fr_calc, fs_calc);
            #endif
            /* 如果获取失败，使用之前获取的free_clusters */
            if (fs_calc != NULL) {
                DWORD cluster_size_bytes = fs_calc->csize * 512;
                free_bytes = (uint64_t)free_clusters * (uint64_t)cluster_size_bytes;
            } else {
                free_bytes = (uint64_t)free_clusters * 512 * 8;  /* 简化计算（可能不准确） */
            }
        }
        
        LOG_INFO("MAIN", "文件系统信息:");
        /* 使用64位计算避免溢出，直接从FatFS内部信息计算 */
        FATFS* fs_display;
        FRESULT fr_display = f_getfree(mount_path, &free_clusters, &fs_display);
        uint64_t total_bytes_64 = 0;
        if (fr_display == FR_OK && fs_display != NULL) {
            DWORD total_clusters_display = fs_display->n_fatent - 2;
            DWORD cluster_size_display = fs_display->csize * 512;
            total_bytes_64 = (uint64_t)total_clusters_display * (uint64_t)cluster_size_display;
        } else {
            /* 如果无法获取，使用FatFS_GetTotalSpace的返回值 */
            total_bytes_64 = total_bytes;
        }
        uint32_t total_mb = (uint32_t)(total_bytes_64 / (1024 * 1024));
        uint32_t free_mb = (uint32_t)(free_bytes / (1024 * 1024));
        LOG_INFO("MAIN", "  总空间: %lu MB (%lu 字节)", total_mb, (unsigned long)total_bytes_64);
        LOG_INFO("MAIN", "  空闲空间: %lu MB (%lu 字节)", free_mb, (unsigned long)free_bytes);
        LOG_INFO("MAIN", "  总簇数: %lu", total_clusters);
        LOG_INFO("MAIN", "  空闲簇数: %lu", free_clusters);
        
        /* 4. 检查FatFS内部信息 */
        {
            FATFS* fs;
            FRESULT fr = f_getfree(mount_path, &free_clusters, &fs);
            if (fr == FR_OK && fs != NULL) {
                DWORD total_clusters_fs = fs->n_fatent - 2;
                DWORD cluster_size = fs->csize * 512;
                uint64_t total_bytes_fs = (uint64_t)total_clusters_fs * cluster_size;
                uint32_t total_mb_fs = (uint32_t)(total_bytes_fs / (1024 * 1024));
                LOG_INFO("MAIN", "FatFS内部信息:");
                LOG_INFO("MAIN", "  n_fatent: %lu", (unsigned long)fs->n_fatent);
                LOG_INFO("MAIN", "  csize: %lu (簇大小: %lu 扇区)", 
                         (unsigned long)fs->csize, (unsigned long)fs->csize);
                LOG_INFO("MAIN", "  总簇数: %lu", (unsigned long)total_clusters_fs);
                LOG_INFO("MAIN", "  计算的总空间: %lu MB", total_mb_fs);
            }
        }
    }
    
    /* ========== 步骤12：进入测试环节 ========== */
    LOG_INFO("MAIN", "=== 进入测试环节 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Testing...");
    Delay_ms(500);
    
    /* 1. 写入文件夹测试 */
    LOG_INFO("MAIN", "1. 写入文件夹测试");
    TestDirectoryCreation();
    Delay_ms(500);
    
    /* 2. 写入文件测试 */
    LOG_INFO("MAIN", "2. 写入文件测试");
    TestFileOperations();
    Delay_ms(500);
    
    /* 3. 重命名测试 */
    LOG_INFO("MAIN", "3. 重命名测试");
    TestRename();
    Delay_ms(500);
    
    /* 4. 删除测试 */
    LOG_INFO("MAIN", "4. 删除测试");
    TestDelete();
    Delay_ms(500);
    
    /* 5. STM32直接操作区边界测试 */
    LOG_INFO("MAIN", "5. STM32直接操作区边界测试");
    TestMCUAreaBoundary();
    Delay_ms(1000);
    
    LOG_INFO("MAIN", "=== 所有测试完成 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "All Tests OK");
    OLED_ShowString(2, 1, "Loop Running");
    
    /* ========== 步骤13：进入循环（测试插拔卡检查与挂载） ========== */
    LOG_INFO("MAIN", "=== 进入循环（测试插拔卡检查与挂载） ===");
    
    uint32_t loop_count = 0;
    const uint32_t max_loop_count = 300;  /* 循环300次，约30秒（每次100ms） */
    
    while (loop_count < max_loop_count) {
        loop_count++;
        
        /* 检查SD卡是否拔出 */
        if (CheckSDCardRemoved()) {
            LOG_WARN("MAIN", "检测到SD卡已拔出（循环 %lu）", (unsigned long)loop_count);
            if (!HandleSDCardRemoval(mount_path)) {
                LOG_ERROR("MAIN", "处理SD卡拔出失败");
                Delay_ms(1000);
                continue;
            }
            /* 重新挂载后继续 */
            continue;
        }
        
        /* 检查文件系统是否已挂载（直接尝试挂载，如果已挂载会返回成功） */
        FatFS_Status_t mount_status = MountFileSystem(mount_path);
        
        if (mount_status != FATFS_OK) {
            /* 检查是否是因为没有文件系统（需要格式化） */
            if (mount_status == FATFS_ERROR_NO_FILESYSTEM) {
                LOG_WARN("MAIN", "检测到无文件系统，执行格式化...");
                FatFS_Unmount(FATFS_VOLUME_SPI);
                Delay_ms(100);
                
                fatfs_status = FormatPartition();
                if (fatfs_status != FATFS_OK) {
                    LOG_ERROR("MAIN", "格式化失败: %d", fatfs_status);
                    Delay_ms(1000);
                    continue;
                }
                
                /* 格式化后重新挂载 */
                mount_status = MountFileSystem(mount_path);
                if (mount_status != FATFS_OK) {
                    LOG_ERROR("MAIN", "格式化后挂载失败: %d", mount_status);
                    Delay_ms(1000);
                    continue;
                }
                LOG_INFO("MAIN", "格式化并挂载成功");
            } else {
                LOG_ERROR("MAIN", "挂载失败: %d", mount_status);
                Delay_ms(1000);
                continue;
            }
        }
        
        /* 循环运行中，定期输出日志 */
        if (loop_count % 100 == 0) {
            LOG_INFO("MAIN", "循环运行中... (循环 %lu/%lu)", (unsigned long)loop_count, (unsigned long)max_loop_count);
        }
        
        Delay_ms(100);
    }
    
    LOG_INFO("MAIN", "循环结束，准备倒计时");
    
    /* ========== 步骤14：倒计时5秒，清除分区表，结束程序 ========== */
    LOG_INFO("MAIN", "=== 倒计时5秒，准备清除分区表 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Countdown 5s");
    
    /* 倒计时5秒 */
    for (int countdown = 5; countdown >= 0; countdown--) {
        char countdown_str[17];
        snprintf(countdown_str, sizeof(countdown_str), "Time: %d s", countdown);
        OLED_ShowString(2, 1, countdown_str);
        LOG_INFO("MAIN", "倒计时: %d 秒", countdown);
        
        if (countdown > 0) {
            Delay_ms(1000);
        }
    }
    
    /* 清除MBR分区表 */
    LOG_INFO("MAIN", "开始清除MBR分区表...");
    OLED_Clear();
    OLED_ShowString(1, 1, "Clearing MBR");
    OLED_ShowString(2, 1, "Please wait...");
    
    fatfs_status = ClearMBRPartitionTable();
    if (fatfs_status != FATFS_OK) {
        LOG_WARN("MAIN", "清除MBR分区表失败或跳过: %d", fatfs_status);
        LOG_WARN("MAIN", "可能SD卡已拔出或状态异常");
        OLED_Clear();
        OLED_ShowString(1, 1, "Clear MBR");
        OLED_ShowString(2, 1, "Skip/Fail");
        Delay_ms(2000);
    } else {
        LOG_INFO("MAIN", "MBR分区表清除成功");
        OLED_Clear();
        OLED_ShowString(1, 1, "Clear MBR OK");
        OLED_ShowString(2, 1, "Program End");
        Delay_ms(2000);
    }
    
    /* 闪烁LED提示程序结束 */
    for (int i = 0; i < 5; i++) {
        LED1_On();
        Delay_ms(200);
        LED1_Off();
        Delay_ms(200);
    }
    
    LOG_INFO("MAIN", "=== 程序结束 ===");
    
    /* 结束程序 */
    while(1) {
        Delay_ms(1000);
    }
}
