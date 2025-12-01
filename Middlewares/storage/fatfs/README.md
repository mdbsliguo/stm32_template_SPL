# FatFS封装层（fatfs_wrapper）

## 📋 概述

`fatfs_wrapper` 是FatFS文件系统的封装层，提供统一的API接口和错误码转换，简化FatFS的使用，提高代码的可维护性和可移植性。

### 核心特性

1. **统一错误码**：将FatFS的`FRESULT`错误码转换为项目统一的`FatFS_Status_t`错误码
2. **参数校验**：所有接口都进行参数校验，提高代码健壮性
3. **多卷支持**：支持SPI和SDIO两种接口的存储设备
4. **分区格式化**：提供便捷的分区格式化接口，支持MBR分区表创建和FAT32格式化
5. **SD卡状态监控**：自动检测SD卡状态，提供状态查询接口
6. **简化接口**：封装FatFS底层API，提供更易用的接口

### 设计目标

- **易用性**：简化FatFS的使用，减少学习成本
- **统一性**：统一错误码和接口风格
- **健壮性**：参数校验和错误处理
- **可维护性**：清晰的接口设计和文档

## 🔧 依赖关系

### 必需依赖

- **FatFS核心**：`ff.h`, `ff.c` - FatFS文件系统核心
- **磁盘I/O层**：`diskio.h`, `diskio.c` - 磁盘I/O接口
- **SPI磁盘驱动**：`diskio_spi.h`, `diskio_spi.c` - SPI接口磁盘驱动（可选）
- **SDIO磁盘驱动**：`diskio_sdio.h`, `diskio_sdio.c` - SDIO接口磁盘驱动（可选）

### 可选依赖

- **TF_SPI模块**：用于直接访问MBR和SD卡状态检查（分区格式化功能需要）
- **Delay模块**：用于格式化过程中的延时（分区格式化功能需要）

## 📦 模块配置

### 启用模块

在`config.h`中启用FatFS模块：

```c
#define CONFIG_MODULE_FATFS_ENABLED  1
```

### FatFS配置

在`ffconf.h`中配置FatFS功能：

```c
/* 启用多分区支持（分区格式化功能需要） */
#define FF_MULTI_PARTITION  1

/* 启用格式化功能（分区格式化功能需要） */
#define FF_USE_MKFS         1

/* 其他配置... */
```

### 状态检查间隔配置

在`config.h`中配置SD卡状态检查的最小间隔：

```c
/* SD卡状态检查最小间隔（毫秒） */
#define FATFS_STATUS_CHECK_INTERVAL_MS  100
```

## 📚 API接口说明

### 卷类型

```c
typedef enum {
    FATFS_VOLUME_SPI = 0,   /**< SPI接口卷（卷0） */
    FATFS_VOLUME_SDIO = 1   /**< SDIO接口卷（卷1） */
} FatFS_Volume_t;
```

### 错误码

```c
typedef enum {
    FATFS_OK = 0,                           /**< 操作成功 */
    FATFS_ERROR_NOT_IMPLEMENTED = -99,      /**< 功能未实现 */
    FATFS_ERROR_NULL_PTR = -1,              /**< 空指针错误 */
    FATFS_ERROR_INVALID_PARAM = -2,         /**< 无效参数 */
    FATFS_ERROR_INVALID_VOLUME = -3,        /**< 无效卷号 */
    FATFS_ERROR_NOT_MOUNTED = -4,           /**< 卷未挂载 */
    FATFS_ERROR_DISK_ERROR = -5,            /**< 磁盘错误 */
    FATFS_ERROR_NOT_READY = -6,             /**< 磁盘未就绪 */
    FATFS_ERROR_NO_FILE = -7,               /**< 文件不存在 */
    FATFS_ERROR_NO_PATH = -8,               /**< 路径不存在 */
    FATFS_ERROR_INVALID_NAME = -9,          /**< 无效文件名 */
    FATFS_ERROR_DENIED = -10,               /**< 访问被拒绝 */
    FATFS_ERROR_EXIST = -11,                /**< 文件已存在 */
    FATFS_ERROR_INVALID_OBJECT = -12,       /**< 无效对象 */
    FATFS_ERROR_WRITE_PROTECTED = -13,      /**< 写保护 */
    FATFS_ERROR_INVALID_DRIVE = -14,        /**< 无效驱动器 */
    FATFS_ERROR_NOT_ENABLED = -15,          /**< 未启用 */
    FATFS_ERROR_NO_FILESYSTEM = -16,        /**< 无文件系统 */
    FATFS_ERROR_TIMEOUT = -17,              /**< 超时 */
    FATFS_ERROR_LOCKED = -18,               /**< 锁定 */
    FATFS_ERROR_NOT_ENOUGH_CORE = -19,      /**< 内存不足 */
    FATFS_ERROR_TOO_MANY_OPEN_FILES = -20,  /**< 打开文件过多 */
    FATFS_ERROR_INVALID_PARAMETER = -21     /**< 无效参数 */
} FatFS_Status_t;
```

### 文件系统操作

#### 挂载文件系统

```c
FatFS_Status_t FatFS_Mount(FatFS_Volume_t volume, const char* path);
```

**参数**：
- `volume`：卷类型（`FATFS_VOLUME_SPI`或`FATFS_VOLUME_SDIO`）
- `path`：卷路径（如`"0:"`或`"1:"`）

**返回值**：
- `FATFS_OK`：挂载成功
- `FATFS_ERROR_NO_FILESYSTEM`：无文件系统（需要格式化）
- 其他错误码：挂载失败

**示例**：
```c
FatFS_Status_t status = FatFS_Mount(FATFS_VOLUME_SPI, "0:");
if (status == FATFS_OK) {
    // 挂载成功
} else if (status == FATFS_ERROR_NO_FILESYSTEM) {
    // 需要格式化
}
```

#### 卸载文件系统

```c
FatFS_Status_t FatFS_Unmount(FatFS_Volume_t volume);
```

**参数**：
- `volume`：卷类型

**返回值**：
- `FATFS_OK`：卸载成功
- 其他错误码：卸载失败

#### 格式化文件系统

```c
FatFS_Status_t FatFS_Format(FatFS_Volume_t volume, const char* path);
```

**参数**：
- `volume`：卷类型
- `path`：卷路径

**返回值**：
- `FATFS_OK`：格式化成功
- 其他错误码：格式化失败

**注意**：需要启用`FF_USE_MKFS`功能

### 文件操作

#### 打开文件

```c
FatFS_Status_t FatFS_FileOpen(FIL* file, const char* path, uint8_t mode);
```

**参数**：
- `file`：文件对象指针（输出）
- `path`：文件路径
- `mode`：打开模式（`FA_READ`, `FA_WRITE`, `FA_OPEN_ALWAYS`, `FA_CREATE_ALWAYS`等）

**打开模式**：
- `FA_READ`：只读
- `FA_WRITE`：只写
- `FA_OPEN_EXISTING`：打开已存在的文件（不存在则失败）
- `FA_CREATE_NEW`：创建新文件（已存在则失败）
- `FA_CREATE_ALWAYS`：创建新文件（已存在则覆盖）
- `FA_OPEN_ALWAYS`：打开文件（不存在则创建）
- `FA_OPEN_APPEND`：打开文件并定位到末尾（不存在则创建）

**示例**：
```c
FIL file;
FatFS_Status_t status = FatFS_FileOpen(&file, "0:test.txt", FA_WRITE | FA_CREATE_ALWAYS);
if (status == FATFS_OK) {
    // 文件打开成功
}
```

#### 关闭文件

```c
FatFS_Status_t FatFS_FileClose(FIL* file);
```

#### 读取文件

```c
FatFS_Status_t FatFS_FileRead(FIL* file, void* buff, uint32_t btr, uint32_t* br);
```

**参数**：
- `file`：文件对象指针
- `buff`：数据缓冲区
- `btr`：要读取的字节数
- `br`：实际读取的字节数（可为NULL）

**示例**：
```c
uint8_t buffer[100];
uint32_t bytes_read;
FatFS_Status_t status = FatFS_FileRead(&file, buffer, sizeof(buffer), &bytes_read);
if (status == FATFS_OK) {
    // 读取成功，bytes_read为实际读取的字节数
}
```

#### 写入文件

```c
FatFS_Status_t FatFS_FileWrite(FIL* file, const void* buff, uint32_t btw, uint32_t* bw);
```

**参数**：
- `file`：文件对象指针
- `buff`：数据缓冲区
- `btw`：要写入的字节数
- `bw`：实际写入的字节数（可为NULL）

**注意**：写入后需要调用`FatFS_FileSync()`确保数据写入磁盘

**示例**：
```c
const char* data = "Hello, FatFS!";
uint32_t bytes_written;
FatFS_Status_t status = FatFS_FileWrite(&file, data, strlen(data), &bytes_written);
if (status == FATFS_OK) {
    FatFS_FileSync(&file);  // 同步到磁盘
}
```

#### 定位文件指针

```c
FatFS_Status_t FatFS_FileSeek(FIL* file, uint32_t ofs);
```

**参数**：
- `file`：文件对象指针
- `ofs`：偏移量（从文件开头）

**示例**（定位到文件末尾）：
```c
FSIZE_t file_size = f_size(&file);
FatFS_FileSeek(&file, (uint32_t)file_size);  // 定位到文件末尾
```

#### 同步文件

```c
FatFS_Status_t FatFS_FileSync(FIL* file);
```

**注意**：写入文件后必须调用此函数确保数据写入磁盘

#### 删除文件

```c
FatFS_Status_t FatFS_FileDelete(const char* path);
```

#### 重命名文件

```c
FatFS_Status_t FatFS_FileRename(const char* path_old, const char* path_new);
```

### 目录操作

#### 打开目录

```c
FatFS_Status_t FatFS_DirOpen(DIR* dir, const char* path);
```

#### 关闭目录

```c
FatFS_Status_t FatFS_DirClose(DIR* dir);
```

#### 读取目录项

```c
FatFS_Status_t FatFS_DirRead(DIR* dir, FILINFO* fno);
```

**示例**（遍历目录）：
```c
DIR dir;
FILINFO fno;
FatFS_Status_t status = FatFS_DirOpen(&dir, "0:");
if (status == FATFS_OK) {
    while (1) {
        status = FatFS_DirRead(&dir, &fno);
        if (status != FATFS_OK || fno.fname[0] == 0) {
            break;  // 读取完毕
        }
        // 处理文件信息 fno
    }
    FatFS_DirClose(&dir);
}
```

#### 创建目录

```c
FatFS_Status_t FatFS_DirCreate(const char* path);
```

**返回值**：
- `FATFS_OK`：创建成功
- `FATFS_ERROR_EXIST`：目录已存在（可以忽略）

#### 删除目录

```c
FatFS_Status_t FatFS_DirDelete(const char* path);
```

**注意**：目录必须为空才能删除

### 文件系统信息

#### 获取空闲空间

```c
FatFS_Status_t FatFS_GetFreeSpace(FatFS_Volume_t volume, const char* path, 
                                   uint32_t* free_clusters, uint32_t* total_clusters);
```

**参数**：
- `volume`：卷类型
- `path`：卷路径
- `free_clusters`：空闲簇数（可为NULL）
- `total_clusters`：总簇数（可为NULL）

**示例**：
```c
uint32_t free_clusters, total_clusters;
FatFS_Status_t status = FatFS_GetFreeSpace(FATFS_VOLUME_SPI, "0:", 
                                            &free_clusters, &total_clusters);
if (status == FATFS_OK) {
    // 计算空闲空间（需要知道簇大小）
    FATFS* fs;
    DWORD cluster_size = fs->csize * 512;  // 簇大小（字节）
    uint64_t free_bytes = (uint64_t)free_clusters * cluster_size;
}
```

#### 获取总空间

```c
FatFS_Status_t FatFS_GetTotalSpace(FatFS_Volume_t volume, const char* path, uint64_t* total_bytes);
```

### 分区格式化（需要FF_MULTI_PARTITION和FF_USE_MKFS）

#### 分区配置结构体

```c
typedef struct {
    uint32_t reserved_area_sectors;    /**< 保留区扇区数，默认2047 */
    uint32_t mcu_direct_area_mb;      /**< MCU直接访问区大小MB，0=不使用，默认100 */
    uint32_t partition_start_sector;  /**< FAT32分区起始扇区，0=自动计算 */
    uint8_t  partition_number;       /**< 分区号（1-4），默认1 */
    uint8_t  format_type;             /**< 格式化类型：FM_FAT32/FM_FAT/FM_EXFAT，默认FM_FAT32 */
} FatFS_PartitionConfig_t;
```

#### 预设配置宏

```c
/* 标准混合存储方案：MBR + 保留区(1MB) + MCU区 + FAT32分区 */
#define FATFS_CONFIG_STANDARD(mcu_mb) {2047, mcu_mb, 0, 1, FM_FAT32}

/* 仅FAT32分区：MBR + FAT32分区（无保留区和MCU区） */
#define FATFS_CONFIG_FAT32_ONLY {0, 0, 0, 1, FM_FAT32}
```

#### 格式化分区（完整功能）

```c
FatFS_Status_t FatFS_FormatPartition(FatFS_Volume_t volume, const FatFS_PartitionConfig_t* config);
```

**参数**：
- `volume`：卷类型
- `config`：分区配置（可为NULL，使用默认配置）

**示例**（自定义配置）：
```c
FatFS_PartitionConfig_t config = {
    .reserved_area_sectors = 2047,      // 保留区2047扇区（约1MB）
    .mcu_direct_area_mb = 100,          // MCU直接访问区100MB
    .partition_start_sector = 0,        // 自动计算
    .partition_number = 1,              // 分区1
    .format_type = FM_FAT32             // FAT32格式
};
FatFS_Status_t status = FatFS_FormatPartition(FATFS_VOLUME_SPI, &config);
```

#### 格式化标准混合存储方案（便捷接口）

```c
FatFS_Status_t FatFS_FormatStandard(FatFS_Volume_t volume, uint32_t mcu_area_mb);
```

**参数**：
- `volume`：卷类型
- `mcu_area_mb`：MCU直接访问区大小（MB）

**示例**：
```c
// 创建标准布局：MBR + 保留区(1MB) + MCU区(100MB) + FAT32分区
FatFS_Status_t status = FatFS_FormatStandard(FATFS_VOLUME_SPI, 100);
```

#### 格式化仅FAT32分区（便捷接口）

```c
FatFS_Status_t FatFS_FormatFAT32Only(FatFS_Volume_t volume);
```

**示例**：
```c
// 创建仅FAT32分区：MBR + FAT32分区
FatFS_Status_t status = FatFS_FormatFAT32Only(FATFS_VOLUME_SPI);
```

### SD卡状态监控

#### SD卡状态枚举

```c
typedef enum {
    FATFS_SD_STATUS_UNKNOWN = 0,          /**< 未知状态（未初始化） */
    FATFS_SD_STATUS_NOT_PRESENT = 1,     /**< SD卡不存在 */
    FATFS_SD_STATUS_PRESENT = 2,         /**< SD卡存在但未初始化 */
    FATFS_SD_STATUS_INITIALIZED = 3,     /**< SD卡已初始化 */
    FATFS_SD_STATUS_READY = 4,           /**< SD卡就绪（已初始化且可用） */
    FATFS_SD_STATUS_ERROR = 5,           /**< SD卡错误 */
    FATFS_SD_STATUS_WRITE_PROTECTED = 6  /**< SD卡写保护 */
} FatFS_SDCardStatus_t;
```

#### 获取SD卡状态

```c
FatFS_SDCardStatus_t FatFS_GetSDCardStatus(FatFS_Volume_t volume);
```

**说明**：
- 每次调用时会自动检查SD卡状态并更新
- 状态检查有最小间隔控制（`FATFS_STATUS_CHECK_INTERVAL_MS`），避免过于频繁检查
- 返回当前状态码

**示例**：
```c
FatFS_SDCardStatus_t status = FatFS_GetSDCardStatus(FATFS_VOLUME_SPI);
if (status == FATFS_SD_STATUS_READY) {
    // SD卡就绪，可以进行文件操作
} else if (status == FATFS_SD_STATUS_NOT_PRESENT) {
    // SD卡不存在
}
```

#### 获取SD卡状态信息（包含变化标志）

```c
FatFS_SDCardStatusInfo_t FatFS_GetSDCardStatusInfo(FatFS_Volume_t volume);
```

**返回结构体**：
```c
typedef struct {
    FatFS_SDCardStatus_t current_status;  /**< 当前状态 */
    FatFS_SDCardStatus_t last_status;     /**< 上次状态 */
    uint8_t status_changed;               /**< 状态是否变化（1=变化，0=未变化） */
} FatFS_SDCardStatusInfo_t;
```

**示例**（检测状态变化）：
```c
FatFS_SDCardStatusInfo_t info = FatFS_GetSDCardStatusInfo(FATFS_VOLUME_SPI);
if (info.status_changed) {
    // 状态发生变化
    if (info.current_status == FATFS_SD_STATUS_NOT_PRESENT) {
        // SD卡已拔出
    } else if (info.current_status == FATFS_SD_STATUS_READY) {
        // SD卡已插入并就绪
    }
    FatFS_ClearSDCardStatusChanged(FATFS_VOLUME_SPI);  // 清除变化标志
}
```

#### 清除SD卡状态变化标志

```c
void FatFS_ClearSDCardStatusChanged(FatFS_Volume_t volume);
```

#### 获取SD卡状态字符串（用于调试）

```c
const char* FatFS_GetSDCardStatusString(FatFS_SDCardStatus_t status);
```

**示例**：
```c
FatFS_SDCardStatus_t status = FatFS_GetSDCardStatus(FATFS_VOLUME_SPI);
printf("SD卡状态: %s\n", FatFS_GetSDCardStatusString(status));
```

## 💡 使用示例

### 基本文件操作

```c
#include "fatfs_wrapper.h"

/* 挂载文件系统 */
FatFS_Status_t status = FatFS_Mount(FATFS_VOLUME_SPI, "0:");
if (status != FATFS_OK) {
    // 处理错误
    return;
}

/* 创建并写入文件 */
FIL file;
status = FatFS_FileOpen(&file, "0:test.txt", FA_WRITE | FA_CREATE_ALWAYS);
if (status == FATFS_OK) {
    const char* data = "Hello, FatFS!";
    uint32_t bytes_written;
    FatFS_FileWrite(&file, data, strlen(data), &bytes_written);
    FatFS_FileSync(&file);  // 同步到磁盘
    FatFS_FileClose(&file);
}

/* 读取文件 */
status = FatFS_FileOpen(&file, "0:test.txt", FA_READ);
if (status == FATFS_OK) {
    char buffer[100];
    uint32_t bytes_read;
    FatFS_FileRead(&file, buffer, sizeof(buffer) - 1, &bytes_read);
    buffer[bytes_read] = '\0';  // 添加字符串结束符
    printf("文件内容: %s\n", buffer);
    FatFS_FileClose(&file);
}
```

### 分区格式化示例

```c
#include "fatfs_wrapper.h"

/* 方式1：使用便捷接口（标准混合存储方案） */
FatFS_Status_t status = FatFS_FormatStandard(FATFS_VOLUME_SPI, 100);
if (status == FATFS_OK) {
    // 格式化成功
    // 布局：MBR + 保留区(1MB) + MCU区(100MB) + FAT32分区
}

/* 方式2：使用便捷接口（仅FAT32分区） */
status = FatFS_FormatFAT32Only(FATFS_VOLUME_SPI);
if (status == FATFS_OK) {
    // 格式化成功
    // 布局：MBR + FAT32分区
}

/* 方式3：使用完整接口（自定义配置） */
FatFS_PartitionConfig_t config = FATFS_CONFIG_STANDARD(100);
status = FatFS_FormatPartition(FATFS_VOLUME_SPI, &config);
```

### SD卡状态监控示例

```c
#include "fatfs_wrapper.h"

/* 在主循环中监控SD卡状态 */
void MainLoop(void)
{
    while (1) {
        /* 获取状态信息 */
        FatFS_SDCardStatusInfo_t info = FatFS_GetSDCardStatusInfo(FATFS_VOLUME_SPI);
        
        /* 检测状态变化 */
        if (info.status_changed) {
            if (info.current_status == FATFS_SD_STATUS_NOT_PRESENT) {
                // SD卡已拔出，卸载文件系统
                FatFS_Unmount(FATFS_VOLUME_SPI);
            } else if (info.current_status == FATFS_SD_STATUS_READY) {
                // SD卡已插入并就绪，重新挂载
                FatFS_Mount(FATFS_VOLUME_SPI, "0:");
            }
            FatFS_ClearSDCardStatusChanged(FATFS_VOLUME_SPI);
        }
        
        Delay_ms(100);
    }
}
```

### 完整的文件系统初始化流程

```c
#include "fatfs_wrapper.h"

FatFS_Status_t InitFileSystem(void)
{
    /* 1. 检查SD卡状态 */
    FatFS_SDCardStatus_t sd_status = FatFS_GetSDCardStatus(FATFS_VOLUME_SPI);
    if (sd_status != FATFS_SD_STATUS_READY && sd_status != FATFS_SD_STATUS_INITIALIZED) {
        return FATFS_ERROR_NOT_READY;
    }
    
    /* 2. 尝试挂载文件系统 */
    FatFS_Status_t status = FatFS_Mount(FATFS_VOLUME_SPI, "0:");
    
    /* 3. 如果无文件系统，进行格式化 */
    if (status == FATFS_ERROR_NO_FILESYSTEM) {
        // 格式化标准混合存储方案
        status = FatFS_FormatStandard(FATFS_VOLUME_SPI, 100);
        if (status != FATFS_OK) {
            return status;
        }
        
        // 格式化后重新挂载
        status = FatFS_Mount(FATFS_VOLUME_SPI, "0:");
    }
    
    return status;
}
```

## ⚠️ 注意事项

### 1. 文件同步

写入文件后必须调用`FatFS_FileSync()`确保数据写入磁盘：

```c
FatFS_FileWrite(&file, data, len, &bytes_written);
FatFS_FileSync(&file);  // 必须调用
FatFS_FileClose(&file);
```

### 2. 路径格式

- 挂载路径：`"0:"`（逻辑驱动器0）
- 文件路径：`"0:filename.txt"`（绝对路径）或`"filename.txt"`（相对路径，已挂载到`"0:"`）
- 目录路径：`"0:dirname"`（绝对路径）或`"dirname"`（相对路径）

### 3. 分区格式化

- 需要启用`FF_MULTI_PARTITION`和`FF_USE_MKFS`
- 格式化会清空SD卡所有数据
- 格式化过程可能需要较长时间（取决于SD卡容量）
- 格式化前需要确保SD卡已初始化

### 4. SD卡状态检查

- 状态检查有最小间隔控制，避免过于频繁检查影响性能
- 状态检查需要TF_SPI模块支持（用于发送CMD13命令）
- 状态变化标志需要手动清除

### 5. 错误处理

所有接口都返回`FatFS_Status_t`错误码，应该检查返回值：

```c
FatFS_Status_t status = FatFS_FileOpen(&file, "0:test.txt", FA_READ);
if (status != FATFS_OK) {
    // 处理错误
    if (status == FATFS_ERROR_NO_FILE) {
        // 文件不存在
    } else if (status == FATFS_ERROR_DISK_ERROR) {
        // 磁盘错误
    }
}
```

### 6. 内存管理

- 文件对象（`FIL`）和目录对象（`DIR`）需要在使用前初始化
- 文件读写缓冲区需要足够大
- 格式化工作缓冲区使用静态变量避免栈溢出

### 7. 多线程安全

- FatFS本身不是线程安全的
- 如果需要在多线程环境中使用，需要添加互斥锁保护

## 🔍 常见问题

### Q1: 挂载失败，返回`FATFS_ERROR_NO_FILESYSTEM`

**原因**：SD卡未格式化或分区表损坏

**解决方法**：
1. 检查SD卡是否已初始化
2. 调用`FatFS_FormatStandard()`或`FatFS_FormatFAT32Only()`进行格式化
3. 格式化后重新挂载

### Q2: 写入文件后数据丢失

**原因**：未调用`FatFS_FileSync()`

**解决方法**：写入后必须调用`FatFS_FileSync()`确保数据写入磁盘

### Q3: 格式化失败

**原因**：
- 未启用`FF_MULTI_PARTITION`或`FF_USE_MKFS`
- SD卡未初始化
- SD卡写保护
- 内存不足

**解决方法**：
1. 检查`ffconf.h`配置
2. 确保SD卡已初始化
3. 检查SD卡写保护开关
4. 增加工作缓冲区大小

### Q4: SD卡状态检查不准确

**原因**：
- TF_SPI模块未初始化
- 状态检查间隔设置过小
- SD卡硬件问题

**解决方法**：
1. 确保TF_SPI模块已初始化
2. 调整`FATFS_STATUS_CHECK_INTERVAL_MS`配置
3. 检查SD卡硬件连接

## 📖 相关文档

- **FatFS官方文档**：`00readme.txt`
- **FatFS历史记录**：`00history.txt`
- **磁盘I/O接口**：`diskio.h`
- **SPI磁盘驱动**：`diskio_spi.h`
- **SDIO磁盘驱动**：`diskio_sdio.h`

## 📝 更新日志

### v1.0.0 (2024-01-01)
- 初始版本
- 实现基本文件系统操作接口
- 实现分区格式化功能
- 实现SD卡状态监控功能




