# FatFS文件系统模块

**FatFS R0.15文件系统，支持SPI和SDIO两种底层磁盘接口。**

---

## 📋 目录说明

```
Middlewares/storage/fatfs/
├── README.md                    # 本文件
├── ffconf.h                     # FatFS配置文件（需从官方源码复制并修改）
├── ff.h                         # FatFS核心头文件（需从官方源码复制）
├── ff.c                         # FatFS核心实现（需从官方源码复制）
├── ffsystem.c                   # FatFS系统接口（需从官方源码复制，R0.15新增）
├── ffstring.c                   # FatFS字符串处理（需从官方源码复制，R0.15新增）
├── diskio.h                     # 磁盘I/O接口头文件（需从官方源码复制）
├── diskio_template.c            # 磁盘I/O模板实现（需从官方源码复制，参考用）
├── diskio_spi.c                 # SPI接口磁盘I/O实现
├── diskio_spi.h                 # SPI接口磁盘I/O头文件
├── diskio_sdio.c                # SDIO接口磁盘I/O实现
├── diskio_sdio.h                # SDIO接口磁盘I/O头文件
└── fatfs_wrapper.c/h            # FatFS封装层，提供统一接口
```

---

## 🎯 功能特性

- ✅ **完整功能支持**：文件读写、目录操作、格式化等
- ✅ **长文件名支持**：支持长文件名（LFN）
- ✅ **多卷支持**：同时支持SPI和SDIO两个卷
- ✅ **统一接口**：封装层提供统一的错误码和接口
- ✅ **项目规范**：遵循项目编码规范和错误处理规范

---

## 📦 依赖关系

### 必需依赖
- **SPI驱动**：`Drivers/spi/spi_hw.c/h`（用于SPI接口）
- **SDIO驱动**：`Drivers/peripheral/sdio.c/h`（用于SDIO接口）
- **GPIO驱动**：`Drivers/basic/gpio.c/h`（SPI/SDIO需要）
- **延时模块**：`System/delay.c/h`（超时处理）

### 可选依赖
- **日志模块**：`Debug/log.c/h`（日志输出）
- **错误处理**：`Common/error_handler.c/h`（错误处理）

---

## 🚀 快速开始

### 1. 获取FatFS官方源码

**⚠️ 重要**：需要从FatFS官网下载R0.15源码：

1. 访问：http://elm-chan.org/fsw/ff/00index_e.html
2. 下载：`ff15.zip`（R0.15版本）
3. 解压后，将以下文件复制到`Middlewares/storage/fatfs/`目录：
   - `ff.h` - FatFS核心头文件
   - `ff.c` - FatFS核心实现
   - `ffconf.h` - FatFS配置文件
   - `ffsystem.c` - 系统接口（R0.15新增）
   - `ffstring.c` - 字符串处理（R0.15新增）
   - `diskio.h` - 磁盘I/O接口头文件
   - `diskio_template.c` - 磁盘I/O模板实现（参考用）

### 2. 配置FatFS

编辑`ffconf.h`，确保以下配置：

```c
#define FF_USE_LFN        2       /* 启用长文件名支持 */
#define FF_FS_READONLY   0       /* 启用写操作 */
#define FF_LFN_UNICODE   0       /* 使用GB2312编码 */
#define FF_VOLUMES       2       /* 支持2个卷（SPI和SDIO） */
```

### 3. 初始化FatFS

```c
#include "fatfs_wrapper.h"

/* 初始化SPI接口的FatFS */
FatFS_Status_t status = FatFS_Mount(FATFS_VOLUME_SPI, "0:");
if (status != FATFS_OK) {
    /* 处理错误 */
}

/* 初始化SDIO接口的FatFS */
status = FatFS_Mount(FATFS_VOLUME_SDIO, "1:");
if (status != FATFS_OK) {
    /* 处理错误 */
}
```

### 4. 使用文件操作

```c
/* 打开文件 */
FIL file;
status = FatFS_FileOpen(&file, "0:test.txt", FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
if (status != FATFS_OK) {
    /* 处理错误 */
}

/* 写入数据 */
uint8_t data[] = "Hello, FatFS!";
uint32_t bytes_written;
status = FatFS_FileWrite(&file, data, sizeof(data) - 1, &bytes_written);

/* 读取数据 */
uint8_t buffer[100];
uint32_t bytes_read;
status = FatFS_FileRead(&file, buffer, sizeof(buffer), &bytes_read);

/* 关闭文件 */
FatFS_FileClose(&file);
```

---

## 📚 API文档

### 文件系统操作

- **`FatFS_Mount()`**：挂载文件系统
- **`FatFS_Unmount()`**：卸载文件系统
- **`FatFS_Format()`**：格式化文件系统

### 文件操作

- **`FatFS_FileOpen()`**：打开文件
- **`FatFS_FileClose()`**：关闭文件
- **`FatFS_FileRead()`**：读取文件
- **`FatFS_FileWrite()`**：写入文件
- **`FatFS_FileSeek()`**：定位文件指针
- **`FatFS_FileTruncate()`**：截断文件
- **`FatFS_FileSync()`**：同步文件
- **`FatFS_FileDelete()`**：删除文件
- **`FatFS_FileRename()`**：重命名文件

### 目录操作

- **`FatFS_DirOpen()`**：打开目录
- **`FatFS_DirRead()`**：读取目录项
- **`FatFS_DirClose()`**：关闭目录
- **`FatFS_DirCreate()`**：创建目录
- **`FatFS_DirDelete()`**：删除目录

### 文件系统信息

- **`FatFS_GetFreeSpace()`**：获取空闲空间
- **`FatFS_GetTotalSpace()`**：获取总空间

详细API文档请参考`fatfs_wrapper.h`。

---

## ⚙️ 配置说明

### 模块开关

在`System/config.h`中配置：

```c
#define CONFIG_MODULE_FATFS_ENABLED       1   /**< FatFS模块开关 */
#define CONFIG_MODULE_FATFS_SPI_ENABLED   1   /**< SPI接口开关 */
#define CONFIG_MODULE_FATFS_SDIO_ENABLED  1   /**< SDIO接口开关 */
```

### 硬件配置

在`BSP/board.h`中配置SPI和SDIO接口（参考项目其他模块的配置方式）。

---

## ⚠️ 注意事项

1. **源码获取**：必须从官网下载FatFS R0.15源码并复制到本目录
2. **编码问题**：FatFS配置为GB2312编码，与项目规范一致
3. **内存占用**：完整功能可能占用较多RAM，需要评估
4. **线程安全**：FatFS本身不支持多线程，需要用户保证单线程访问
5. **磁盘接口**：需要根据实际硬件实现底层接口（diskio_spi.c和diskio_sdio.c）

---

## 🔗 相关文档

- **FatFS官网**：http://elm-chan.org/fsw/ff/00index_e.html
- **项目规范**：`PROJECT_KEYWORDS.md`
- **SPI驱动**：`Drivers/spi/README.md`
- **SDIO驱动**：`Drivers/peripheral/sdio.h`

---

**最后更新**：2024-01-01

