/**
 * @file fatfs_wrapper.c
 * @brief FatFS封装层实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 提供统一的FatFS接口，封装FatFS底层API，统一错误码转换
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#ifdef CONFIG_MODULE_FATFS_ENABLED
#if CONFIG_MODULE_FATFS_ENABLED

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include "fatfs_wrapper.h"
#include "diskio.h"
#include "diskio_spi.h"
#include "diskio_sdio.h"
#include "ff.h"

/* 包含TF_SPI模块（用于直接访问MBR和状态检查） */
#ifdef CONFIG_MODULE_TF_SPI_ENABLED
#if CONFIG_MODULE_TF_SPI_ENABLED
#include "tf_spi.h"
#endif
#endif

/* 包含delay模块（用于格式化过程中的延时） */
#ifdef CONFIG_MODULE_DELAY_ENABLED
#if CONFIG_MODULE_DELAY_ENABLED
#include "delay.h"
#endif
#endif

/* 挂载状态标志 */
static uint8_t g_fatfs_mounted[FF_VOLUMES] = {0};

/* 文件系统对象 */
static FATFS g_fatfs[FF_VOLUMES];

/* SD卡状态监控 */
#if CONFIG_MODULE_TF_SPI_ENABLED
static FatFS_SDCardStatus_t g_sd_status[FF_VOLUMES] = {FATFS_SD_STATUS_UNKNOWN};
static FatFS_SDCardStatus_t g_sd_last_status[FF_VOLUMES] = {FATFS_SD_STATUS_UNKNOWN};
static uint8_t g_sd_status_changed[FF_VOLUMES] = {0};
static uint32_t g_last_status_check_time[FF_VOLUMES] = {0};
#define STATUS_CHECK_INTERVAL_MS  100  /* 最小检查间隔100ms */
#endif

#if FF_MULTI_PARTITION
/* 逻辑驱动器到物理驱动器和分区的映射表 */
/* pd: 物理驱动器号（0=SPI, 1=SDIO等） */
/* pt: 分区号（0=自动检测, 1-4=强制指定分区） */
/* 注意：当使用分区方案时，路径中的分区号会覆盖此配置 */
/* 例如：VolToPart[0] = {0, 1} 表示逻辑驱动器0 -> 物理驱动器0，分区1 */
/* 使用路径 "0:1:" 时会使用分区1，使用路径 "0:" 时会使用VolToPart中的配置 */
PARTITION VolToPart[FF_VOLUMES] = {
    {FATFS_VOLUME_SPI, 1},  /* 逻辑驱动器0 -> 物理驱动器0（SPI），分区1（FAT32分区） */
};
#endif

/**
 * @brief 将FatFS错误码转换为项目错误码
 */
static FatFS_Status_t FatFS_ConvertError(FRESULT fr)
{
    switch (fr) {
        case FR_OK:
            return FATFS_OK;
        case FR_DISK_ERR:
            return FATFS_ERROR_DISK_ERROR;
        case FR_INT_ERR:
            return FATFS_ERROR_INVALID_PARAM;
        case FR_NOT_READY:
            return FATFS_ERROR_NOT_READY;
        case FR_NO_FILE:
            return FATFS_ERROR_NO_FILE;
        case FR_NO_PATH:
            return FATFS_ERROR_NO_PATH;
        case FR_INVALID_NAME:
            return FATFS_ERROR_INVALID_NAME;
        case FR_DENIED:
            return FATFS_ERROR_DENIED;
        case FR_EXIST:
            return FATFS_ERROR_EXIST;
        case FR_INVALID_OBJECT:
            return FATFS_ERROR_INVALID_OBJECT;
        case FR_WRITE_PROTECTED:
            return FATFS_ERROR_WRITE_PROTECTED;
        case FR_INVALID_DRIVE:
            return FATFS_ERROR_INVALID_DRIVE;
        case FR_NOT_ENABLED:
            return FATFS_ERROR_NOT_ENABLED;
        case FR_NO_FILESYSTEM:
            return FATFS_ERROR_NO_FILESYSTEM;
        case FR_TIMEOUT:
            return FATFS_ERROR_TIMEOUT;
        case FR_LOCKED:
            return FATFS_ERROR_LOCKED;
        case FR_NOT_ENOUGH_CORE:
            return FATFS_ERROR_NOT_ENOUGH_CORE;
        case FR_TOO_MANY_OPEN_FILES:
            return FATFS_ERROR_TOO_MANY_OPEN_FILES;
        case FR_INVALID_PARAMETER:
            return FATFS_ERROR_INVALID_PARAMETER;
        default:
            return FATFS_ERROR_INVALID_PARAM;
    }
}

/**
 * @brief 挂载文件系统
 */
FatFS_Status_t FatFS_Mount(FatFS_Volume_t volume, const char* path)
{
    /* ========== 参数校验 ========== */
    if (path == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    if (volume >= FF_VOLUMES) {
        return FATFS_ERROR_INVALID_VOLUME;
    }
    
    /* 检查是否已经挂载 */
    if (g_fatfs_mounted[volume] != 0) {
        /* 已经挂载，直接返回成功 */
        return FATFS_OK;
    }
    
    /* 挂载文件系统 */
    FRESULT fr = f_mount(&g_fatfs[volume], path, 1);
    if (fr == FR_OK) {
        g_fatfs_mounted[volume] = 1;
    }
    
    return FatFS_ConvertError(fr);
}

/**
 * @brief 卸载文件系统
 */
FatFS_Status_t FatFS_Unmount(FatFS_Volume_t volume)
{
    /* ========== 参数校验 ========== */
    if (volume >= FF_VOLUMES) {
        return FATFS_ERROR_INVALID_VOLUME;
    }
    
    /* 检查是否已经卸载 */
    if (g_fatfs_mounted[volume] == 0) {
        /* 已经卸载，直接返回成功 */
        return FATFS_OK;
    }
    
    /* 卸载文件系统 */
    char path[4];
    path[0] = '0' + volume;
    path[1] = ':';
    path[2] = '\0';
    
    FRESULT fr = f_mount(NULL, path, 0);
    if (fr == FR_OK) {
        g_fatfs_mounted[volume] = 0;
    }
    
    return FatFS_ConvertError(fr);
}

/**
 * @brief 格式化文件系统
 */
FatFS_Status_t FatFS_Format(FatFS_Volume_t volume, const char* path)
{
    /* ========== 参数校验 ========== */
    if (path == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    if (volume >= FF_VOLUMES) {
        return FATFS_ERROR_INVALID_VOLUME;
    }
    
    #if FF_USE_MKFS
    /* 格式化参数 */
    MKFS_PARM opt;
    opt.fmt = FM_FAT32;  /* 使用FAT32格式 */
    opt.n_fat = 1;       /* 1个FAT表 */
    opt.align = 0;       /* 自动对齐 */
    opt.n_root = 0;      /* 根目录项数（FAT32自动） */
    opt.au_size = 0;     /* 自动簇大小 */
    
    /* 工作缓冲区（需要至少FF_MAX_SS字节） */
    static BYTE work[FF_MAX_SS];
    
    FRESULT fr = f_mkfs(path, &opt, work, sizeof(work));
    return FatFS_ConvertError(fr);
    #else
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
}

/**
 * @brief 打开文件
 */
FatFS_Status_t FatFS_FileOpen(FIL* file, const char* path, uint8_t mode)
{
    /* ========== 参数校验 ========== */
    if (file == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    if (path == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    FRESULT fr = f_open(file, path, mode);
    return FatFS_ConvertError(fr);
}

/**
 * @brief 关闭文件
 */
FatFS_Status_t FatFS_FileClose(FIL* file)
{
    /* ========== 参数校验 ========== */
    if (file == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    FRESULT fr = f_close(file);
    return FatFS_ConvertError(fr);
}

/**
 * @brief 读取文件
 */
FatFS_Status_t FatFS_FileRead(FIL* file, void* buff, uint32_t btr, uint32_t* br)
{
    /* ========== 参数校验 ========== */
    if (file == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    if (buff == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    UINT bytes_read;
    FRESULT fr = f_read(file, buff, btr, &bytes_read);
    
    if (br != NULL) {
        *br = bytes_read;
    }
    
    return FatFS_ConvertError(fr);
}

/**
 * @brief 写入文件
 */
FatFS_Status_t FatFS_FileWrite(FIL* file, const void* buff, uint32_t btw, uint32_t* bw)
{
    /* ========== 参数校验 ========== */
    if (file == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    if (buff == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    UINT bytes_written;
    FRESULT fr = f_write(file, buff, btw, &bytes_written);
    
    if (bw != NULL) {
        *bw = bytes_written;
    }
    
    return FatFS_ConvertError(fr);
}

/**
 * @brief 定位文件指针
 */
FatFS_Status_t FatFS_FileSeek(FIL* file, uint32_t ofs)
{
    /* ========== 参数校验 ========== */
    if (file == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    FRESULT fr = f_lseek(file, ofs);
    return FatFS_ConvertError(fr);
}

/**
 * @brief 截断文件
 */
FatFS_Status_t FatFS_FileTruncate(FIL* file)
{
    /* ========== 参数校验 ========== */
    if (file == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    #if !FF_FS_READONLY
    FRESULT fr = f_truncate(file);
    return FatFS_ConvertError(fr);
    #else
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
}

/**
 * @brief 同步文件
 */
FatFS_Status_t FatFS_FileSync(FIL* file)
{
    /* ========== 参数校验 ========== */
    if (file == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    #if !FF_FS_READONLY
    FRESULT fr = f_sync(file);
    return FatFS_ConvertError(fr);
    #else
    return FATFS_OK;
    #endif
}

/**
 * @brief 删除文件
 */
FatFS_Status_t FatFS_FileDelete(const char* path)
{
    /* ========== 参数校验 ========== */
    if (path == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    #if !FF_FS_READONLY
    FRESULT fr = f_unlink(path);
    return FatFS_ConvertError(fr);
    #else
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
}

/**
 * @brief 重命名文件
 */
FatFS_Status_t FatFS_FileRename(const char* path_old, const char* path_new)
{
    /* ========== 参数校验 ========== */
    if (path_old == NULL || path_new == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    #if !FF_FS_READONLY
    FRESULT fr = f_rename(path_old, path_new);
    return FatFS_ConvertError(fr);
    #else
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
}

/**
 * @brief 打开目录
 */
FatFS_Status_t FatFS_DirOpen(DIR* dir, const char* path)
{
    /* ========== 参数校验 ========== */
    if (dir == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    if (path == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    FRESULT fr = f_opendir(dir, path);
    return FatFS_ConvertError(fr);
}

/**
 * @brief 关闭目录
 */
FatFS_Status_t FatFS_DirClose(DIR* dir)
{
    /* ========== 参数校验 ========== */
    if (dir == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    FRESULT fr = f_closedir(dir);
    return FatFS_ConvertError(fr);
}

/**
 * @brief 读取目录项
 */
FatFS_Status_t FatFS_DirRead(DIR* dir, FILINFO* fno)
{
    /* ========== 参数校验 ========== */
    if (dir == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    if (fno == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    FRESULT fr = f_readdir(dir, fno);
    return FatFS_ConvertError(fr);
}

/**
 * @brief 创建目录
 */
FatFS_Status_t FatFS_DirCreate(const char* path)
{
    /* ========== 参数校验 ========== */
    if (path == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    #if !FF_FS_READONLY
    FRESULT fr = f_mkdir(path);
    return FatFS_ConvertError(fr);
    #else
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
}

/**
 * @brief 删除目录
 */
FatFS_Status_t FatFS_DirDelete(const char* path)
{
    /* ========== 参数校验 ========== */
    if (path == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    #if !FF_FS_READONLY
    FRESULT fr = f_unlink(path);
    return FatFS_ConvertError(fr);
    #else
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
}

/**
 * @brief 获取空闲空间
 */
FatFS_Status_t FatFS_GetFreeSpace(FatFS_Volume_t volume, const char* path, 
                                   uint32_t* free_clusters, uint32_t* total_clusters)
{
    /* ========== 参数校验 ========== */
    if (path == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    if (volume >= FF_VOLUMES) {
        return FATFS_ERROR_INVALID_VOLUME;
    }
    
    DWORD nclst;
    FATFS* fatfs;
    FRESULT fr = f_getfree(path, &nclst, &fatfs);
    
    if (fr == FR_OK) {
        if (free_clusters != NULL) {
            *free_clusters = nclst;
        }
        if (total_clusters != NULL && fatfs != NULL) {
            *total_clusters = fatfs->n_fatent - 2;  /* 总簇数 = FAT项数 - 2 */
        }
    }
    
    return FatFS_ConvertError(fr);
}

/**
 * @brief 获取总空间
 */
FatFS_Status_t FatFS_GetTotalSpace(FatFS_Volume_t volume, const char* path, uint64_t* total_bytes)
{
    /* ========== 参数校验 ========== */
    if (path == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    if (volume >= FF_VOLUMES) {
        return FATFS_ERROR_INVALID_VOLUME;
    }
    
    DWORD nclst;
    FATFS* fatfs;
    FRESULT fr = f_getfree(path, &nclst, &fatfs);
    
    if (fr == FR_OK && fatfs != NULL && total_bytes != NULL) {
        DWORD total_clusters = fatfs->n_fatent - 2;
        DWORD cluster_size = fatfs->csize * FF_MIN_SS;  /* 簇大小（字节） */
        /* 使用64位计算，支持最大16EB的文件系统 */
        *total_bytes = (uint64_t)total_clusters * (uint64_t)cluster_size;
    }
    
    return FatFS_ConvertError(fr);
}

/**
 * @brief 获取当前时间（FatFS时间戳函数）
 * @return DWORD 时间戳值
 * @details FatFS需要此函数来设置文件/目录的时间戳
 *          格式：bit31:25=年份(0-127, 1980+), bit24:21=月份(1-12), 
 *                bit20:16=日期(1-31), bit15:11=小时(0-23),
 *                bit10:5=分钟(0-59), bit4:0=秒/2(0-29)
 * @note 如果系统没有RTC，可以返回固定时间或使用系统时间
 */
#if !FF_FS_READONLY && !FF_FS_NORTC
DWORD get_fattime(void)
{
    /* 返回固定时间：2025年1月1日 12:00:00 */
    /* 年份：2025 - 1980 = 45 */
    /* 月份：1 */
    /* 日期：1 */
    /* 小时：12 */
    /* 分钟：0 */
    /* 秒：0 (秒/2 = 0) */
    
    DWORD time = 0;
    time |= ((DWORD)(2025 - 1980) << 25);  /* 年份：2025 */
    time |= ((DWORD)1 << 21);               /* 月份：1月 */
    time |= ((DWORD)1 << 16);               /* 日期：1日 */
    time |= ((DWORD)12 << 11);             /* 小时：12 */
    time |= ((DWORD)0 << 5);                /* 分钟：0 */
    time |= ((DWORD)(0 / 2));               /* 秒：0 (秒/2 = 0) */
    
    return time;
}
#endif /* !FF_FS_READONLY && !FF_FS_NORTC */

/* ==================== 分区格式化功能 ==================== */

#if FF_MULTI_PARTITION && FF_USE_MKFS

/**
 * @brief 创建MBR分区表
 */
static FatFS_Status_t FatFS_CreateMBRPartition(FatFS_Volume_t volume, 
                                                uint32_t partition_start, 
                                                uint32_t partition_sectors)
{
    #ifdef CONFIG_MODULE_TF_SPI_ENABLED
    #if CONFIG_MODULE_TF_SPI_ENABLED
    
    /* 获取SD卡信息 */
    const tf_spi_dev_t* dev_info = TF_SPI_GetInfo();
    if (dev_info == NULL) {
        return FATFS_ERROR_NOT_READY;
    }
    
    /* 检查分区大小是否足够 */
    if (partition_sectors < 2048) {  /* 至少1MB */
        return FATFS_ERROR_NOT_ENOUGH_CORE;
    }
    
    /* 初始化磁盘 */
    DSTATUS stat = disk_initialize(0);
    if (stat & STA_NOINIT) {
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
    if (tf_status != TF_SPI_OK) {
        return FATFS_ERROR_DISK_ERROR;
    }
    
    /* 清空分区表（保留MBR引导代码） */
    memset(mbr_buf + 446, 0, 64);  /* 清空4个分区表项 */
    
    /* MBR分区表定义 */
    #define MBR_PARTITION_TABLE_OFFSET  446
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
    
    /* 创建分区1：FAT32分区 */
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
        return FATFS_ERROR_DISK_ERROR;
    }
    
    /* 同步磁盘 */
    #ifdef CONFIG_MODULE_DELAY_ENABLED
    #if CONFIG_MODULE_DELAY_ENABLED
    Delay_ms(100);
    #endif
    #endif
    
    return FATFS_OK;
    
    #else
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
    #else
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
}

/**
 * @brief 格式化FAT32分区
 */
static FatFS_Status_t FatFS_FormatFAT32Partition(FatFS_Volume_t volume,
                                                  uint32_t partition_start,
                                                  uint32_t partition_sectors,
                                                  uint8_t partition_number)
{
    #ifdef CONFIG_MODULE_TF_SPI_ENABLED
    #if CONFIG_MODULE_TF_SPI_ENABLED
    
    /* 格式化参数 */
    MKFS_PARM opt;
    opt.fmt = FM_FAT32;  /* FAT32格式 */
    opt.n_fat = 1;       /* 1个FAT表 */
    opt.align = 0;       /* 自动对齐 */
    opt.n_root = 0;      /* FAT32不需要根目录项数 */
    opt.au_size = 0;     /* 自动簇大小 */
    
    /* 格式化工作缓冲区 - 使用静态变量避免栈溢出 */
    static BYTE work[FF_MAX_SS];
    
    /* 构建分区路径 */
    char path[8];
    path[0] = '0' + volume;
    path[1] = ':';
    path[2] = '0' + partition_number;
    path[3] = ':';
    path[4] = '\0';
    
    /* 在格式化前，先清除缓存，让f_mkfs能够正确读取MBR */
    #if defined(FATFS_PARTITION_START_SECTOR) && (FATFS_PARTITION_START_SECTOR > 0)
    extern void disk_ioctl_spi_clear_partition_cache(void);
    extern void disk_ioctl_spi_set_partition_sectors(uint32_t sectors);
    disk_ioctl_spi_clear_partition_cache();
    #endif
    
    FRESULT fr = f_mkfs(path, &opt, work, sizeof(work));
    if (fr != FR_OK) {
        return FatFS_ConvertError(fr);
    }
    
    /* 同步磁盘，确保所有写入完成 */
    disk_ioctl(0, CTRL_SYNC, NULL);
    #ifdef CONFIG_MODULE_DELAY_ENABLED
    #if CONFIG_MODULE_DELAY_ENABLED
    Delay_ms(100);
    #endif
    #endif
    
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
                    #ifdef CONFIG_MODULE_DELAY_ENABLED
                    #if CONFIG_MODULE_DELAY_ENABLED
                    Delay_ms(100);
                    #endif
                    #endif
                }
            }
        }
    }
    
    /* 设置缓存为正确的分区扇区数 */
    #if defined(FATFS_PARTITION_START_SECTOR) && (FATFS_PARTITION_START_SECTOR > 0)
    extern void disk_ioctl_spi_set_partition_sectors(uint32_t sectors);
    disk_ioctl_spi_set_partition_sectors(partition_sectors);
    #endif
    
    return FATFS_OK;
    
    #else
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
    #else
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
}

/**
 * @brief 格式化分区（完整功能）
 */
FatFS_Status_t FatFS_FormatPartition(FatFS_Volume_t volume, const FatFS_PartitionConfig_t* config)
{
    /* ========== 参数校验 ========== */
    if (volume >= FF_VOLUMES) {
        return FATFS_ERROR_INVALID_VOLUME;
    }
    
    #ifdef CONFIG_MODULE_TF_SPI_ENABLED
    #if CONFIG_MODULE_TF_SPI_ENABLED
    
    /* 使用默认配置或用户配置 */
    FatFS_PartitionConfig_t default_config = {2047, 100, 0, 1, FM_FAT32};
    const FatFS_PartitionConfig_t* cfg = (config != NULL) ? config : &default_config;
    
    /* 获取SD卡信息 */
    const tf_spi_dev_t* dev_info = TF_SPI_GetInfo();
    if (dev_info == NULL) {
        return FATFS_ERROR_NOT_READY;
    }
    
    uint32_t total_sectors = dev_info->block_count;
    
    /* 计算分区起始扇区 */
    uint32_t partition_start;
    if (cfg->partition_start_sector > 0) {
        partition_start = cfg->partition_start_sector;
    } else {
        /* 自动计算：MBR(1) + 保留区 + MCU区 */
        partition_start = 1 + cfg->reserved_area_sectors;
        if (cfg->mcu_direct_area_mb > 0) {
            partition_start += (cfg->mcu_direct_area_mb * 1024 * 1024 / 512);
        }
    }
    
    uint32_t partition_sectors = total_sectors - partition_start;
    
    /* 检查分区大小是否足够 */
    if (partition_sectors < 2048) {  /* 至少1MB */
        return FATFS_ERROR_NOT_ENOUGH_CORE;
    }
    
    /* 1. 创建MBR分区表 */
    FatFS_Status_t status = FatFS_CreateMBRPartition(volume, partition_start, partition_sectors);
    if (status != FATFS_OK) {
        return status;
    }
    
    #ifdef CONFIG_MODULE_DELAY_ENABLED
    #if CONFIG_MODULE_DELAY_ENABLED
    Delay_ms(500);
    #endif
    #endif
    
    /* 2. 格式化FAT32分区 */
    status = FatFS_FormatFAT32Partition(volume, partition_start, partition_sectors, cfg->partition_number);
    if (status != FATFS_OK) {
        return status;
    }
    
    return FATFS_OK;
    
    #else
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
    #else
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
}

/**
 * @brief 格式化标准混合存储方案（便捷接口）
 */
FatFS_Status_t FatFS_FormatStandard(FatFS_Volume_t volume, uint32_t mcu_area_mb)
{
    FatFS_PartitionConfig_t config = FATFS_CONFIG_STANDARD(mcu_area_mb);
    return FatFS_FormatPartition(volume, &config);
}

/**
 * @brief 格式化仅FAT32分区（便捷接口）
 */
FatFS_Status_t FatFS_FormatFAT32Only(FatFS_Volume_t volume)
{
    FatFS_PartitionConfig_t config = FATFS_CONFIG_FAT32_ONLY;
    return FatFS_FormatPartition(volume, &config);
}

#endif /* FF_MULTI_PARTITION && FF_USE_MKFS */

/* ==================== SD卡状态监控 ==================== */

#ifdef CONFIG_MODULE_TF_SPI_ENABLED
#if CONFIG_MODULE_TF_SPI_ENABLED

/**
 * @brief 获取SD卡状态（自动检查并更新）
 */
FatFS_SDCardStatus_t FatFS_GetSDCardStatus(FatFS_Volume_t volume)
{
    /* ========== 参数校验 ========== */
    if (volume >= FF_VOLUMES) {
        return FATFS_SD_STATUS_UNKNOWN;
    }
    
    /* 检查时间间隔，避免过于频繁检查 */
    #ifdef CONFIG_MODULE_DELAY_ENABLED
    #if CONFIG_MODULE_DELAY_ENABLED
    extern uint32_t Delay_GetTick(void);
    uint32_t current_time = Delay_GetTick();
    if (g_last_status_check_time[volume] > 0) {
        uint32_t elapsed;
        if (current_time >= g_last_status_check_time[volume]) {
            elapsed = current_time - g_last_status_check_time[volume];
        } else {
            /* 处理溢出情况 */
            elapsed = (0xFFFFFFFFUL - g_last_status_check_time[volume]) + current_time + 1;
        }
        if (elapsed < STATUS_CHECK_INTERVAL_MS) {
            /* 距离上次检查时间太短，直接返回缓存的状态 */
            return g_sd_status[volume];
        }
    }
    g_last_status_check_time[volume] = current_time;
    #endif
    #endif
    
    /* 保存上次状态 */
    g_sd_last_status[volume] = g_sd_status[volume];
    
    /* 检查TF_SPI模块是否初始化 */
    if (!TF_SPI_IsInitialized()) {
        /* 尝试初始化 */
        TF_SPI_Status_t tf_status = TF_SPI_Init();
        if (tf_status == TF_SPI_OK) {
            const tf_spi_dev_t* dev_info = TF_SPI_GetInfo();
            if (dev_info != NULL && dev_info->state == TF_SPI_STATE_INITIALIZED) {
                g_sd_status[volume] = FATFS_SD_STATUS_INITIALIZED;
            } else {
                g_sd_status[volume] = FATFS_SD_STATUS_PRESENT;
            }
        } else {
            g_sd_status[volume] = FATFS_SD_STATUS_NOT_PRESENT;
        }
    } else {
        /* 已初始化，检查SD卡状态 */
        uint8_t card_status;
        TF_SPI_Status_t tf_status = TF_SPI_SendStatus(&card_status);
        if (tf_status == TF_SPI_OK) {
            const tf_spi_dev_t* dev_info = TF_SPI_GetInfo();
            if (dev_info != NULL && dev_info->state == TF_SPI_STATE_INITIALIZED) {
                /* 检查容量是否足够（至少200MB） */
                if (dev_info->capacity_mb >= 200) {
                    g_sd_status[volume] = FATFS_SD_STATUS_READY;
                } else {
                    g_sd_status[volume] = FATFS_SD_STATUS_INITIALIZED;
                }
            } else {
                g_sd_status[volume] = FATFS_SD_STATUS_PRESENT;
            }
        } else {
            /* 状态检查失败，可能是SD卡已拔出 */
            g_sd_status[volume] = FATFS_SD_STATUS_NOT_PRESENT;
        }
    }
    
    /* 检查状态是否变化 */
    if (g_sd_status[volume] != g_sd_last_status[volume]) {
        g_sd_status_changed[volume] = 1;
    }
    
    return g_sd_status[volume];
}

/**
 * @brief 获取SD卡状态信息（包含变化标志）
 */
FatFS_SDCardStatusInfo_t FatFS_GetSDCardStatusInfo(FatFS_Volume_t volume)
{
    FatFS_SDCardStatusInfo_t info;
    
    /* ========== 参数校验 ========== */
    if (volume >= FF_VOLUMES) {
        info.current_status = FATFS_SD_STATUS_UNKNOWN;
        info.last_status = FATFS_SD_STATUS_UNKNOWN;
        info.status_changed = 0;
        return info;
    }
    
    /* 先更新状态 */
    FatFS_GetSDCardStatus(volume);
    
    info.current_status = g_sd_status[volume];
    info.last_status = g_sd_last_status[volume];
    info.status_changed = g_sd_status_changed[volume];
    
    return info;
}

/**
 * @brief 清除SD卡状态变化标志
 */
void FatFS_ClearSDCardStatusChanged(FatFS_Volume_t volume)
{
    if (volume < FF_VOLUMES) {
        g_sd_status_changed[volume] = 0;
    }
}

/**
 * @brief 获取SD卡状态字符串（用于调试）
 */
const char* FatFS_GetSDCardStatusString(FatFS_SDCardStatus_t status)
{
    switch (status) {
        case FATFS_SD_STATUS_UNKNOWN:
            return "UNKNOWN";
        case FATFS_SD_STATUS_NOT_PRESENT:
            return "NOT_PRESENT";
        case FATFS_SD_STATUS_PRESENT:
            return "PRESENT";
        case FATFS_SD_STATUS_INITIALIZED:
            return "INITIALIZED";
        case FATFS_SD_STATUS_READY:
            return "READY";
        case FATFS_SD_STATUS_ERROR:
            return "ERROR";
        case FATFS_SD_STATUS_WRITE_PROTECTED:
            return "WRITE_PROTECTED";
        default:
            return "INVALID";
    }
}

#endif /* CONFIG_MODULE_TF_SPI_ENABLED */
#endif /* CONFIG_MODULE_TF_SPI_ENABLED */

#endif /* CONFIG_MODULE_FATFS_ENABLED */
#endif /* CONFIG_MODULE_FATFS_ENABLED */
