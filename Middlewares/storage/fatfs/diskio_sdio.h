/**
 * @file diskio_sdio.h
 * @brief FatFS SDIO接口磁盘I/O头文件
 * @version 1.0.0
 * @date 2024-01-01
 * @details 实现FatFS的diskio接口，使用SDIO接口访问SD卡
 * @note 当前SDIO模块为占位空函数，需要先实现SDIO模块的高级接口
 */

#ifndef DISKIO_SDIO_H
#define DISKIO_SDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"

#ifdef CONFIG_MODULE_FATFS_ENABLED
#if CONFIG_MODULE_FATFS_ENABLED

#ifdef CONFIG_MODULE_FATFS_SDIO_ENABLED
#if CONFIG_MODULE_FATFS_SDIO_ENABLED

#include "diskio.h"

/**
 * @brief SDIO磁盘物理驱动器号
 * @note FatFS中卷1对应SDIO接口
 */
#define DISKIO_SDIO_DRIVE_NUM    1

/**
 * @brief 初始化SDIO磁盘
 * @param[in] pdrv 物理驱动器号（应为DISKIO_SDIO_DRIVE_NUM）
 * @return DSTATUS 磁盘状态
 */
DSTATUS disk_initialize_sdio(BYTE pdrv);

/**
 * @brief 获取SDIO磁盘状态
 * @param[in] pdrv 物理驱动器号（应为DISKIO_SDIO_DRIVE_NUM）
 * @return DSTATUS 磁盘状态
 */
DSTATUS disk_status_sdio(BYTE pdrv);

/**
 * @brief 读取SDIO磁盘扇区
 * @param[in] pdrv 物理驱动器号（应为DISKIO_SDIO_DRIVE_NUM）
 * @param[out] buff 数据缓冲区
 * @param[in] sector 起始扇区号
 * @param[in] count 扇区数量
 * @return DRESULT 操作结果
 */
DRESULT disk_read_sdio(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count);

/**
 * @brief 写入SDIO磁盘扇区
 * @param[in] pdrv 物理驱动器号（应为DISKIO_SDIO_DRIVE_NUM）
 * @param[in] buff 数据缓冲区
 * @param[in] sector 起始扇区号
 * @param[in] count 扇区数量
 * @return DRESULT 操作结果
 */
DRESULT disk_write_sdio(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count);

/**
 * @brief SDIO磁盘控制命令
 * @param[in] pdrv 物理驱动器号（应为DISKIO_SDIO_DRIVE_NUM）
 * @param[in] cmd 控制命令
 * @param[in,out] buff 命令参数/结果缓冲区
 * @return DRESULT 操作结果
 */
DRESULT disk_ioctl_sdio(BYTE pdrv, BYTE cmd, void* buff);

#endif /* CONFIG_MODULE_FATFS_SDIO_ENABLED */
#endif /* CONFIG_MODULE_FATFS_SDIO_ENABLED */

#endif /* CONFIG_MODULE_FATFS_ENABLED */
#endif /* CONFIG_MODULE_FATFS_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* DISKIO_SDIO_H */



