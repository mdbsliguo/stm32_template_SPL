/**
 * @file diskio_spi.h
 * @brief FatFS SPI接口磁盘I/O头文件
 * @version 1.0.0
 * @date 2024-01-01
 * @details 实现FatFS的diskio接口，使用SPI接口（TF_SPI模块）访问SD卡
 */

#ifndef DISKIO_SPI_H
#define DISKIO_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"

#ifdef CONFIG_MODULE_FATFS_ENABLED
#if CONFIG_MODULE_FATFS_ENABLED

#ifdef CONFIG_MODULE_FATFS_SPI_ENABLED
#if CONFIG_MODULE_FATFS_SPI_ENABLED

#include "ff.h"
#include "diskio.h"

/**
 * @brief SPI磁盘物理驱动器号
 * @note FatFS中卷0对应SPI接口
 */
#define DISKIO_SPI_DRIVE_NUM    0

/**
 * @brief 初始化SPI磁盘
 * @param[in] pdrv 物理驱动器号（应为DISKIO_SPI_DRIVE_NUM）
 * @return DSTATUS 磁盘状态
 */
DSTATUS disk_initialize_spi(BYTE pdrv);

/**
 * @brief 获取SPI磁盘状态
 * @param[in] pdrv 物理驱动器号（应为DISKIO_SPI_DRIVE_NUM）
 * @return DSTATUS 磁盘状态
 */
DSTATUS disk_status_spi(BYTE pdrv);

/**
 * @brief 读取SPI磁盘扇区
 * @param[in] pdrv 物理驱动器号（应为DISKIO_SPI_DRIVE_NUM）
 * @param[out] buff 数据缓冲区
 * @param[in] sector 起始扇区号
 * @param[in] count 扇区数量
 * @return DRESULT 操作结果
 */
DRESULT disk_read_spi(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count);

/**
 * @brief 写入SPI磁盘扇区
 * @param[in] pdrv 物理驱动器号（应为DISKIO_SPI_DRIVE_NUM）
 * @param[in] buff 数据缓冲区
 * @param[in] sector 起始扇区号
 * @param[in] count 扇区数量
 * @return DRESULT 操作结果
 */
DRESULT disk_write_spi(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count);

/**
 * @brief SPI磁盘控制命令
 * @param[in] pdrv 物理驱动器号（应为DISKIO_SPI_DRIVE_NUM）
 * @param[in] cmd 控制命令
 * @param[in,out] buff 命令参数/结果缓冲区
 * @return DRESULT 操作结果
 */
DRESULT disk_ioctl_spi(BYTE pdrv, BYTE cmd, void* buff);

#endif /* CONFIG_MODULE_FATFS_SPI_ENABLED */
#endif /* CONFIG_MODULE_FATFS_SPI_ENABLED */

#endif /* CONFIG_MODULE_FATFS_ENABLED */
#endif /* CONFIG_MODULE_FATFS_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* DISKIO_SPI_H */

