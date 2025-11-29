/**
 * @file flash09_app.c
 * @brief Flash09最小化版本业务逻辑层实现
 * @details 封装Flash09案例的业务逻辑，保持main函数简洁
 * @note 最小化版本：保留OLED显示，禁用UART、Log等模块，只保留核心功能
 */

#include "flash09_app.h"
#include "stm32f10x.h"
#include "system_init.h"
#include "led.h"
#include "delay.h"
#include "error_handler.h"
#include "spi_hw.h"
#include "tf_spi.h"
#include "fatfs_wrapper.h"
#include "ff.h"
#include "diskio.h"
#include "gpio.h"
#include "config.h"
#include "oled_ssd1306.h"
#include "i2c_sw.h"
#include <string.h>
#include <stddef.h>
#include <stdio.h>

/* ==================== 全局变量 ==================== */

/* MCU保留区域信息（STM32直接访问区）- 当前未使用，保留用于未来扩展 */
#if 0
static struct {
    uint32_t start_sector;  /**< 起始扇区 */
    uint32_t sector_count;  /**< 扇区数量 */
    uint8_t initialized;    /**< 是否已初始化 */
} g_mcu_reserved_area = {0, 0, 0};
#endif

/* ==================== 内部辅助函数声明 ==================== */

/* 清除分区表 */
static FatFS_Status_t ClearMBRPartitionTable(void);

/* SD卡检测 */
static bool CheckSDCardPresent(void);
static bool CheckSDCardUsable(void);
static bool CheckSDCardRemoved(void);

/* 文件系统操作 */
static FatFS_Status_t MountFileSystem(const char* mount_path);
static bool HandleSDCardRemoval(const char* mount_path);

/* ==================== 公共接口实现 ==================== */

/**
 * @brief 初始化Flash09应用（系统初始化）
 */
Flash09_AppStatus_t Flash09_AppInit(void)
{
    OLED_Status_t oled_status;
    
    /* ========== 步骤1：系统初始化 ========== */
    System_Init();
    
    /* ========== 步骤2：LED初始化 ========== */
    if (LED_Init() != LED_OK) {
        return FLASH09_APP_ERROR_INIT;
    }
    
    Delay_ms(500);
    
    /* ========== 步骤3：软件I2C初始化（OLED需要） ========== */
    SoftI2C_Status_t i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_1);
    if (i2c_status != SOFT_I2C_OK) {
        ErrorHandler_Handle(i2c_status, "SOFT_I2C");
        return FLASH09_APP_ERROR_INIT;
    }
    
    /* ========== 步骤4：OLED初始化 ========== */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK) {
        ErrorHandler_Handle(oled_status, "OLED");
        return FLASH09_APP_ERROR_INIT;
    } else {
        OLED_Clear();
        OLED_ShowString(1, 1, "Flash09 Demo");
        OLED_ShowString(2, 1, "Initializing...");
    }
    
    Delay_ms(500);
    
    /* ========== 步骤5：SPI初始化 ========== */
    /* 手动配置PA11为GPIO输出（软件NSS模式） */
    GPIO_EnableClock(GPIOA);
    GPIO_Config(GPIOA, GPIO_Pin_11, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHz);
    GPIO_WritePin(GPIOA, GPIO_Pin_11, Bit_SET);  /* NSS默认拉高（不选中） */
    
    SPI_Status_t spi_status = SPI_HW_Init(SPI_INSTANCE_2);
    if (spi_status != SPI_OK) {
        ErrorHandler_Handle(spi_status, "SPI");
        OLED_Clear();
        OLED_ShowString(1, 1, "SPI Init Fail");
        return FLASH09_APP_ERROR_INIT;
    }
    
    OLED_ShowString(3, 1, "SPI OK");
    Delay_ms(500);
    
    return FLASH09_APP_OK;
}

/**
 * @brief 初始化SD卡（检测并初始化）
 */
Flash09_AppStatus_t Flash09_InitSDCard(void)
{
    OLED_Clear();
    OLED_ShowString(1, 1, "TF Card Init");
    Delay_ms(500);
    
    /* 检测SD卡是否存在（初始化时完成） */
    while (!CheckSDCardPresent()) {
        OLED_ShowString(2, 1, "No SD Card!");
        OLED_ShowString(3, 1, "Waiting...");
        LED1_Toggle();
        Delay_ms(500);
    }
    
    /* 检查SD卡是否满足使用要求 */
    while (!CheckSDCardUsable()) {
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
    }
    
    Delay_ms(1000);
    
    return FLASH09_APP_OK;
}

/**
 * @brief 挂载文件系统
 */
Flash09_AppStatus_t Flash09_MountFileSystem(char* mount_path, uint32_t path_size)
{
    if (mount_path == NULL || path_size < 4) {
        return FLASH09_APP_ERROR_INIT;
    }
    
    mount_path[0] = '0';
    mount_path[1] = ':';
    mount_path[2] = '\0';
    
    /* ========== 步骤1：强制格式化检查 ========== */
    #if FATFS_FORCE_FORMAT
    /* 强制格式化开关：1不检查直接格式化，0跳过 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Formatting...");
    OLED_ShowString(2, 1, "Please wait...");
    
    /* 先尝试卸载文件系统（如果已挂载） */
    FatFS_Unmount(FATFS_VOLUME_SPI);
    Delay_ms(100);
    
    /* 格式化分区（使用封装接口） */
    FatFS_Status_t fatfs_status = FatFS_FormatStandard(FATFS_VOLUME_SPI, FATFS_MCU_DIRECT_AREA_MB);
    if (fatfs_status != FATFS_OK) {
        ErrorHandler_Handle(fatfs_status, "FatFS");
        OLED_Clear();
        OLED_ShowString(1, 1, "Format Fail");
        OLED_ShowString(2, 1, "Error!");
        return FLASH09_APP_ERROR_MOUNT;
    }
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Format OK!");
    OLED_ShowString(2, 1, "Mounting...");
    
    /* 闪烁LED提示格式化完成 */
    for (int i = 0; i < 3; i++) {
        LED1_On();
        Delay_ms(200);
        LED1_Off();
        Delay_ms(200);
    }
    
    Delay_ms(1000);
    #endif
    
    /* ========== 步骤2：挂载SD分区 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "Mounting...");
    
    FatFS_Status_t fatfs_status = MountFileSystem(mount_path);
    
    if (fatfs_status != FATFS_OK) {
        ErrorHandler_Handle(fatfs_status, "FatFS");
        OLED_Clear();
        OLED_ShowString(1, 1, "Mount Fail");
        OLED_ShowString(2, 1, "Error!");
        return FLASH09_APP_ERROR_MOUNT;
    }
    
    OLED_ShowString(3, 1, "Mount OK");
    Delay_ms(1000);
    
    return FLASH09_APP_OK;
}

/**
 * @brief 显示文件系统信息
 */
void Flash09_ShowFileSystemInfo(const char* mount_path)
{
    uint32_t free_clusters, total_clusters;
    FatFS_Status_t fatfs_status = FatFS_GetFreeSpace(FATFS_VOLUME_SPI, mount_path, &free_clusters, &total_clusters);
    
    OLED_Clear();
    OLED_ShowString(1, 1, "File System");
    
    if (fatfs_status == FATFS_OK) {
        uint64_t total_bytes = 0;
        FatFS_GetTotalSpace(FATFS_VOLUME_SPI, mount_path, &total_bytes);
        
        /* 计算总空间和空闲空间（MB） */
        FATFS* fs;
        DWORD free_clusters_calc = 0;
        FRESULT fr = f_getfree(mount_path, &free_clusters_calc, &fs);
        uint64_t free_bytes = 0;
        if (fr == FR_OK && fs != NULL) {
            DWORD cluster_size_bytes = fs->csize * 512;
            free_bytes = (uint64_t)free_clusters_calc * (uint64_t)cluster_size_bytes;
        }
        
        uint32_t total_mb = (uint32_t)(total_bytes / (1024 * 1024));
        uint32_t free_mb = (uint32_t)(free_bytes / (1024 * 1024));
        
        char buf[17];
        snprintf(buf, sizeof(buf), "Total: %d MB", total_mb);
        OLED_ShowString(2, 1, buf);
        snprintf(buf, sizeof(buf), "Free: %d MB", free_mb);
        OLED_ShowString(3, 1, buf);
    } else {
        OLED_ShowString(2, 1, "Info Error");
    }
    
    Delay_ms(2000);
}

/**
 * @brief 运行主循环（测试插拔卡检测与挂载）
 */
Flash09_AppStatus_t Flash09_RunMainLoop(const char* mount_path, uint32_t loop_duration_ms)
{
    uint32_t loop_count = 0;
    const uint32_t loop_interval_ms = 100;
    const uint32_t max_loop_count = loop_duration_ms / loop_interval_ms;
    
    while (loop_count < max_loop_count) {
        loop_count++;
        
        /* 检查SD卡是否拔出 */
        if (CheckSDCardRemoved()) {
            if (!HandleSDCardRemoval(mount_path)) {
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
                FatFS_Unmount(FATFS_VOLUME_SPI);
                Delay_ms(100);
                
                FatFS_Status_t fatfs_status = FatFS_FormatStandard(FATFS_VOLUME_SPI, FATFS_MCU_DIRECT_AREA_MB);
                if (fatfs_status != FATFS_OK) {
                    Delay_ms(1000);
                    continue;
                }
                
                /* 格式化后重新挂载 */
                mount_status = MountFileSystem(mount_path);
                if (mount_status != FATFS_OK) {
                    Delay_ms(1000);
                    continue;
                }
            } else {
                Delay_ms(1000);
                continue;
            }
        }
        
        /* 循环运行中，定期闪烁LED */
        if (loop_count % 50 == 0) {
            LED1_Toggle();
        }
        
        Delay_ms(loop_interval_ms);
    }
    
    return FLASH09_APP_OK;
}

/**
 * @brief 程序结束流程（倒计时、清除分区表）
 */
Flash09_AppStatus_t Flash09_Shutdown(void)
{
    OLED_Clear();
    OLED_ShowString(1, 1, "Countdown 5s");
    
    /* 倒计时5秒 */
    for (int countdown = 5; countdown >= 0; countdown--) {
        char countdown_str[17];
        snprintf(countdown_str, sizeof(countdown_str), "Time: %d s", countdown);
        OLED_ShowString(2, 1, countdown_str);
        
        if (countdown > 0) {
            LED1_On();
            Delay_ms(500);
            LED1_Off();
            Delay_ms(500);
        }
    }
    
    /* 清除MBR分区表 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Clearing MBR");
    OLED_ShowString(2, 1, "Please wait...");
    
    FatFS_Status_t fatfs_status = ClearMBRPartitionTable();
    if (fatfs_status != FATFS_OK) {
        /* 可能SD卡已拔出或状态异常，忽略错误 */
        OLED_Clear();
        OLED_ShowString(1, 1, "Clear MBR");
        OLED_ShowString(2, 1, "Skip/Fail");
        Delay_ms(2000);
    } else {
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
    
    return FLASH09_APP_OK;
}

/* ==================== 内部辅助函数实现 ==================== */

/**
 * @brief 清除MBR分区表（清空分区表数据，保留MBR签名）
 * @return FatFS_Status_t 错误码
 */
static FatFS_Status_t ClearMBRPartitionTable(void)
{
    /* 检查SD卡状态 */
    const tf_spi_dev_t* dev_info = TF_SPI_GetInfo();
    if (dev_info == NULL || dev_info->state != TF_SPI_STATE_INITIALIZED) {
        TF_SPI_Status_t init_status = TF_SPI_Init();
        if (init_status != TF_SPI_OK) {
            return FATFS_ERROR_NOT_READY;
        }
    }
    
    /* 检查SD卡是否可用 */
    uint8_t status;
    TF_SPI_Status_t status_check = TF_SPI_SendStatus(&status);
    if (status_check != TF_SPI_OK) {
        return FATFS_ERROR_NOT_READY;
    }
    
    /* 读取MBR（直接读取物理扇区0，不使用disk_read避免扇区映射） */
    BYTE mbr_buf[512];
    TF_SPI_Status_t tf_status = TF_SPI_ReadBlock(0, mbr_buf);
    if (tf_status != TF_SPI_OK) {
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
        return FATFS_ERROR_DISK_ERROR;
    }
    
    /* 同步磁盘 */
    Delay_ms(100);
    
    return FATFS_OK;
}

/**
 * @brief 检测SD卡是否存在（初始化时调用）
 * @return true: SD卡存在，false: SD卡不存在
 */
static bool CheckSDCardPresent(void)
{
    /* 使用封装的状态检测接口 */
    FatFS_SDCardStatus_t status = FatFS_GetSDCardStatus(FATFS_VOLUME_SPI);
    if (status == FATFS_SD_STATUS_READY || status == FATFS_SD_STATUS_INITIALIZED) {
        return true;
    }
    return false;
}

/**
 * @brief 检查SD卡是否满足使用要求
 * @return true: SD卡满足使用要求，false: SD卡不满足使用要求
 * @note 检查SD卡容量、状态等是否满足使用要求
 */
static bool CheckSDCardUsable(void)
{
    FatFS_SDCardStatus_t status = FatFS_GetSDCardStatus(FATFS_VOLUME_SPI);
    if (status == FATFS_SD_STATUS_READY) {
        return true;
    }
    
    if (status == FATFS_SD_STATUS_INITIALIZED) {
        const tf_spi_dev_t* dev_info = TF_SPI_GetInfo();
        if (dev_info != NULL && dev_info->capacity_mb < 200) {
            return false;
        }
    }
    
    return false;
}

/**
 * @brief 检测SD卡是否已拔出（循环中调用）
 * @return true: SD卡已拔出，false: SD卡仍然存在
 */
static bool CheckSDCardRemoved(void)
{
    FatFS_SDCardStatus_t status = FatFS_GetSDCardStatus(FATFS_VOLUME_SPI);
    return (status == FATFS_SD_STATUS_NOT_PRESENT || status == FATFS_SD_STATUS_UNKNOWN);
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
        /* 格式化分区（使用封装接口） */
        FatFS_Status_t format_status = FatFS_FormatStandard(FATFS_VOLUME_SPI, FATFS_MCU_DIRECT_AREA_MB);
        if (format_status != FATFS_OK) {
            return format_status;
        }
        
        /* 重新挂载 */
        fatfs_status = FatFS_Mount(FATFS_VOLUME_SPI, mount_path);
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
    OLED_ShowString(3, 1, "Waiting...");
    uint32_t wait_count = 0;
    while (1) {
        /* 尝试初始化SD卡，如果成功则说明SD卡已插入 */
        TF_SPI_Status_t tf_status = TF_SPI_Init();
        if (tf_status == TF_SPI_OK) {
            const tf_spi_dev_t* dev_info = TF_SPI_GetInfo();
            if (dev_info != NULL && dev_info->state == TF_SPI_STATE_INITIALIZED) {
                break;  /* SD卡已插入并初始化成功 */
            }
        }
        
        LED1_Toggle();
        Delay_ms(500);
        wait_count++;
    }
    
    /* 每次插卡回去会检查SD卡是否满足使用 */
    while (1) {
        /* 检查SD卡是否满足使用要求 */
        if (!CheckSDCardUsable()) {
            OLED_ShowString(3, 1, "Not Usable!");
            LED1_Toggle();
            Delay_ms(500);
            
            /* 重新检测SD卡是否拔出 */
            if (CheckSDCardRemoved()) {
                TF_SPI_Deinit();
                Delay_ms(100);
                return HandleSDCardRemoval(mount_path);  /* 递归处理 */
            }
            continue;
        }
        
        /* SD卡满足使用要求，退出检查循环 */
        break;
    }
    
    /* 如果SD卡满足使用要求，重新挂载文件系统 */
    if (CheckSDCardUsable()) {
        OLED_ShowString(3, 1, "Remounting...");
        FatFS_Status_t fatfs_status = MountFileSystem(mount_path);
        
        if (fatfs_status != FATFS_OK) {
            OLED_Clear();
            OLED_ShowString(1, 1, "Mount Fail");
            OLED_ShowString(2, 1, "Error!");
            return false;
        } else {
            OLED_Clear();
            OLED_ShowString(1, 1, "Mount OK");
            return true;
        }
    }
    
    return false;
}
