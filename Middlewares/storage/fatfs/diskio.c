/*-----------------------------------------------------------------------/
/  Low level disk I/O module for FatFs     (C)ChaN, 2019                /
/-----------------------------------------------------------------------/
/  This file implements the disk I/O interface for FatFS.               /
/  It routes calls to the appropriate disk driver (SPI or SDIO).        /
/-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include "config.h"

#ifdef CONFIG_MODULE_FATFS_ENABLED
#if CONFIG_MODULE_FATFS_ENABLED

/* 包含磁盘接口实现 */
#ifdef CONFIG_MODULE_FATFS_SPI_ENABLED
#if CONFIG_MODULE_FATFS_SPI_ENABLED
#include "diskio_spi.h"
#endif
#endif

#ifdef CONFIG_MODULE_FATFS_SDIO_ENABLED
#if CONFIG_MODULE_FATFS_SDIO_ENABLED
#include "diskio_sdio.h"
#endif
#endif

/* 物理驱动器号定义 */
#define DEV_SPI     0   /* SPI接口驱动器（卷0） */
#define DEV_SDIO    1   /* SDIO接口驱动器（卷1） */

/*-----------------------------------------------------------------------/
/  Get Drive Status                                                      /
/-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive number to identify the drive */
)
{
	switch (pdrv) {
	case DEV_SPI:
		#ifdef CONFIG_MODULE_FATFS_SPI_ENABLED
		#if CONFIG_MODULE_FATFS_SPI_ENABLED
		return disk_status_spi(pdrv);
		#else
		return STA_NOINIT;
		#endif
		#else
		return STA_NOINIT;
		#endif

	case DEV_SDIO:
		#ifdef CONFIG_MODULE_FATFS_SDIO_ENABLED
		#if CONFIG_MODULE_FATFS_SDIO_ENABLED
		return disk_status_sdio(pdrv);
		#else
		return STA_NOINIT;
		#endif
		#else
		return STA_NOINIT;
		#endif

	default:
		return STA_NOINIT;
	}
}

/*-----------------------------------------------------------------------/
/  Initialize a Drive                                                    /
/-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv		/* Physical drive number to identify the drive */
)
{
	switch (pdrv) {
	case DEV_SPI:
		#ifdef CONFIG_MODULE_FATFS_SPI_ENABLED
		#if CONFIG_MODULE_FATFS_SPI_ENABLED
		return disk_initialize_spi(pdrv);
		#else
		return STA_NOINIT;
		#endif
		#else
		return STA_NOINIT;
		#endif

	case DEV_SDIO:
		#ifdef CONFIG_MODULE_FATFS_SDIO_ENABLED
		#if CONFIG_MODULE_FATFS_SDIO_ENABLED
		return disk_initialize_sdio(pdrv);
		#else
		return STA_NOINIT;
		#endif
		#else
		return STA_NOINIT;
		#endif

	default:
		return STA_NOINIT;
	}
}

/*-----------------------------------------------------------------------/
/  Read Sector(s)                                                        /
/-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive number to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	switch (pdrv) {
	case DEV_SPI:
		#ifdef CONFIG_MODULE_FATFS_SPI_ENABLED
		#if CONFIG_MODULE_FATFS_SPI_ENABLED
		return disk_read_spi(pdrv, buff, sector, count);
		#else
		return RES_ERROR;
		#endif
		#else
		return RES_ERROR;
		#endif

	case DEV_SDIO:
		#ifdef CONFIG_MODULE_FATFS_SDIO_ENABLED
		#if CONFIG_MODULE_FATFS_SDIO_ENABLED
		return disk_read_sdio(pdrv, buff, sector, count);
		#else
		return RES_ERROR;
		#endif
		#else
		return RES_ERROR;
		#endif

	default:
		return RES_PARERR;
	}
}

/*-----------------------------------------------------------------------/
/  Write Sector(s)                                                       /
/-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,		/* Physical drive number to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count		/* Number of sectors to write */
)
{
	switch (pdrv) {
	case DEV_SPI:
		#ifdef CONFIG_MODULE_FATFS_SPI_ENABLED
		#if CONFIG_MODULE_FATFS_SPI_ENABLED
		return disk_write_spi(pdrv, buff, sector, count);
		#else
		return RES_ERROR;
		#endif
		#else
		return RES_ERROR;
		#endif

	case DEV_SDIO:
		#ifdef CONFIG_MODULE_FATFS_SDIO_ENABLED
		#if CONFIG_MODULE_FATFS_SDIO_ENABLED
		return disk_write_sdio(pdrv, buff, sector, count);
		#else
		return RES_ERROR;
		#endif
		#else
		return RES_ERROR;
		#endif

	default:
		return RES_PARERR;
	}
}

#endif

/*-----------------------------------------------------------------------/
/  Miscellaneous Functions                                               /
/-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive number (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	switch (pdrv) {
	case DEV_SPI:
		#ifdef CONFIG_MODULE_FATFS_SPI_ENABLED
		#if CONFIG_MODULE_FATFS_SPI_ENABLED
		return disk_ioctl_spi(pdrv, cmd, buff);
		#else
		return RES_ERROR;
		#endif
		#else
		return RES_ERROR;
		#endif

	case DEV_SDIO:
		#ifdef CONFIG_MODULE_FATFS_SDIO_ENABLED
		#if CONFIG_MODULE_FATFS_SDIO_ENABLED
		return disk_ioctl_sdio(pdrv, cmd, buff);
		#else
		return RES_ERROR;
		#endif
		#else
		return RES_ERROR;
		#endif

	default:
		return RES_PARERR;
	}
}

#endif /* CONFIG_MODULE_FATFS_ENABLED */
#endif /* CONFIG_MODULE_FATFS_ENABLED */
