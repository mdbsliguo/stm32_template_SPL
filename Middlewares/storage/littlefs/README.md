# LITTLEFS - LittleFS文件系统封装模块

## 📋 模块简介

`littlefs_wrapper` 是基于 LittleFS 开源文件系统的封装模块，提供标准化的文件系统接口，支持在外部 Flash（如 W25Q）上实现完整的文件系统功能。

### 核心特性

- ✅ **完整的文件系统功能**：支持文件创建、读写、删除、重命名、截断等操作
- ✅ **目录管理**：支持目录创建、删除、遍历等操作
- ✅ **文件属性**：支持自定义文件属性（0x00-0xFF），可用于存储元数据
- ✅ **断电保护**：采用日志结构，确保数据在断电时的一致性
- ✅ **磨损均衡**：自动进行磨损均衡，延长 Flash 寿命
- ✅ **多实例支持**：支持同时挂载多个文件系统实例
- ✅ **RTOS 支持**：支持 RTOS 环境下的线程安全（可选）
- ✅ **日志回调**：支持自定义日志回调函数，便于调试
- ✅ **原始 API 访问**：提供底层 LittleFS API 访问接口，支持高级用法

### 设计原则

1. **标准化接口**：提供统一的 Init、Mount、Format 等标准接口
2. **错误处理**：使用统一的错误码系统（`ERROR_BASE_LITTLEFS`）
3. **资源管理**：自动管理文件系统资源，简化使用流程
4. **性能优化**：针对 W25Q Flash 优化配置参数

## 🎯 使用场景

适用于需要在外部 Flash 上实现文件系统功能的应用，特别是：

- **数据日志存储**：记录系统运行日志、事件日志等
- **配置文件管理**：存储系统配置、用户设置等
- **固件更新**：存储固件镜像、更新包等
- **参数保存**：保存校准参数、运行参数等
- **UI 资源管理**：存储字库、图片、音频等资源文件
- **数据采集**：存储传感器数据、历史记录等

## 📚 API说明

### 初始化与配置

#### LittleFS_Init

```c
LittleFS_Status_t LittleFS_Init(void);
```

初始化 LittleFS 文件系统（使用默认配置）。

**返回值**：
- `LITTLEFS_OK`：初始化成功
- `LITTLEFS_ERROR_NOT_INIT`：W25Q 未初始化
- 其他错误码：初始化失败

**使用示例**：
```c
LittleFS_Status_t status = LittleFS_Init();
if (status != LITTLEFS_OK) {
    // 处理错误
}
```

#### LittleFS_InitWithConfig

```c
LittleFS_Status_t LittleFS_InitWithConfig(LittleFS_Instance_t instance, 
                                          const LittleFS_Config_t *config);
```

使用自定义配置初始化指定实例的 LittleFS 文件系统。

**参数**：
- `instance`：实例号（`LITTLEFS_INSTANCE_0` 或 `LITTLEFS_INSTANCE_1`）
- `config`：配置结构体指针（NULL 表示使用默认配置）

**返回值**：
- `LITTLEFS_OK`：初始化成功
- 其他错误码：初始化失败

**配置结构体说明**：
```c
typedef struct {
    /* 块设备参数 */
    uint32_t read_size;          /**< 最小读取大小（字节），默认256 */
    uint32_t prog_size;          /**< 最小编程大小（字节），默认256 */
    uint32_t block_size;         /**< 块大小（字节），默认4096 */
    uint32_t block_count;        /**< 块数量（自动计算） */
    int32_t block_cycles;        /**< 磨损均衡周期，默认500（100-1000） */
    uint32_t cache_size;         /**< 缓存大小（字节），默认256 */
    uint32_t lookahead_size;     /**< 前瞻缓冲区大小（字节），自动计算，最小8 */
    
    /* 限制参数 */
    uint32_t name_max;           /**< 文件名最大长度，0=使用默认值 */
    uint32_t file_max;           /**< 文件最大大小，0=使用默认值 */
    uint32_t attr_max;           /**< 属性最大大小，0=使用默认值 */
    
    /* 回调函数 */
    LittleFS_LogCallback_t log_callback;    /**< 日志回调函数，NULL=不输出日志 */
    LittleFS_LockCallback_t lock_callback;   /**< RTOS 加锁回调函数 */
    LittleFS_UnlockCallback_t unlock_callback; /**< RTOS 解锁回调函数 */
    void *lock_context;                     /**< 锁回调函数的用户上下文 */
    
    /* 调试选项 */
    uint8_t debug_enabled;       /**< 调试模式开关，1=启用，0=禁用 */
} LittleFS_Config_t;
```

**使用示例**：
```c
LittleFS_Config_t config = {0};
LittleFS_GetDefaultConfig(&config);  // 获取默认配置
config.cache_size = 128;              // 修改缓存大小
config.block_cycles = 1000;           // 修改磨损均衡周期

LittleFS_Status_t status = LittleFS_InitWithConfig(LITTLEFS_INSTANCE_0, &config);
```

### 文件系统管理

#### LittleFS_Mount

```c
LittleFS_Status_t LittleFS_Mount(void);
```

挂载文件系统（默认实例）。

**返回值**：
- `LITTLEFS_OK`：挂载成功
- `LITTLEFS_ERROR_CORRUPT`：文件系统损坏，需要格式化
- `LITTLEFS_ERROR_NOT_INIT`：模块未初始化
- 其他错误码：挂载失败

**注意事项**：
- 如果文件系统损坏，返回 `LITTLEFS_ERROR_CORRUPT`，需要先格式化
- 首次使用或文件系统损坏时需要格式化

#### LittleFS_Format

```c
LittleFS_Status_t LittleFS_Format(void);
```

格式化文件系统（默认实例）。

**⚠️ 警告**：此操作会清除所有数据，请谨慎使用！

**返回值**：
- `LITTLEFS_OK`：格式化成功
- 其他错误码：格式化失败

**使用示例**：
```c
// 尝试挂载
LittleFS_Status_t status = LittleFS_Mount();
if (status == LITTLEFS_ERROR_CORRUPT) {
    // 文件系统损坏，格式化
    status = LittleFS_Format();
    if (status == LITTLEFS_OK) {
        // 格式化成功，重新挂载
        status = LittleFS_Mount();
    }
}
```

#### LittleFS_GetInfo

```c
LittleFS_Status_t LittleFS_GetInfo(uint64_t *total_bytes, uint64_t *free_bytes);
```

获取文件系统信息（默认实例）。

**参数**：
- `total_bytes`：总空间（字节），输出参数
- `free_bytes`：空闲空间（字节），输出参数

**返回值**：
- `LITTLEFS_OK`：查询成功
- 其他错误码：查询失败

### 文件操作

#### LittleFS_FileOpen

```c
LittleFS_Status_t LittleFS_FileOpen(lfs_file_t *file, const char *path, int flags);
```

打开文件。

**参数**：
- `file`：文件句柄指针（**必须在使用前清零**：`memset(&file, 0, sizeof(file))`）
- `path`：文件路径（**使用相对路径**，如 `"test.txt"`，不要使用 `"/test.txt"`）
- `flags`：打开标志
  - `LFS_O_RDONLY`：只读
  - `LFS_O_WRONLY`：只写
  - `LFS_O_RDWR`：读写
  - `LFS_O_CREAT`：文件不存在时创建
  - `LFS_O_EXCL`：与 `LFS_O_CREAT` 一起使用，文件存在时返回错误
  - `LFS_O_TRUNC`：打开时清空文件
  - `LFS_O_APPEND`：追加模式

**返回值**：
- `LITTLEFS_OK`：打开成功
- `LITTLEFS_ERROR_NOENT`：文件不存在（未使用 `LFS_O_CREAT`）
- 其他错误码：打开失败

**⚠️ 重要提示**：
- 文件句柄必须在使用前清零：`memset(&file, 0, sizeof(file))`
- 使用相对路径，不要使用绝对路径

#### LittleFS_FileWrite

```c
LittleFS_Status_t LittleFS_FileWrite(lfs_file_t *file, const void *buffer, 
                                     uint32_t size, uint32_t *bytes_written);
```

写入文件。

**参数**：
- `file`：文件句柄指针
- `buffer`：数据缓冲区
- `size`：写入大小（字节）
- `bytes_written`：实际写入字节数（输出参数，可为 NULL）

**返回值**：
- `LITTLEFS_OK`：写入成功
- 其他错误码：写入失败

**注意事项**：
- 写入后必须调用 `LittleFS_FileSync()` 确保数据落盘

#### LittleFS_FileRead

```c
LittleFS_Status_t LittleFS_FileRead(lfs_file_t *file, void *buffer, 
                                    uint32_t size, uint32_t *bytes_read);
```

读取文件。

**参数**：
- `file`：文件句柄指针
- `buffer`：数据缓冲区
- `size`：读取大小（字节）
- `bytes_read`：实际读取字节数（输出参数，可为 NULL）

**返回值**：
- `LITTLEFS_OK`：读取成功
- 其他错误码：读取失败

#### LittleFS_FileSync

```c
LittleFS_Status_t LittleFS_FileSync(lfs_file_t *file);
```

同步文件，确保数据写入 Flash。

**⚠️ 重要**：所有写入操作后必须调用此函数，否则数据可能在断电后丢失！

**返回值**：
- `LITTLEFS_OK`：同步成功
- 其他错误码：同步失败

#### LittleFS_FileClose

```c
LittleFS_Status_t LittleFS_FileClose(lfs_file_t *file);
```

关闭文件。

**返回值**：
- `LITTLEFS_OK`：关闭成功
- 其他错误码：关闭失败

#### LittleFS_FileSeek

```c
LittleFS_Status_t LittleFS_FileSeek(lfs_file_t *file, int32_t offset, int whence);
```

文件定位。

**参数**：
- `file`：文件句柄指针
- `offset`：偏移量（字节）
- `whence`：定位方式
  - `LFS_SEEK_SET`：文件开头
  - `LFS_SEEK_CUR`：当前位置
  - `LFS_SEEK_END`：文件末尾

**返回值**：
- `LITTLEFS_OK`：定位成功
- 其他错误码：定位失败

#### LittleFS_FileDelete

```c
LittleFS_Status_t LittleFS_FileDelete(const char *path);
```

删除文件。

**参数**：
- `path`：文件路径（相对路径）

**返回值**：
- `LITTLEFS_OK`：删除成功
- `LITTLEFS_ERROR_NOENT`：文件不存在
- 其他错误码：删除失败

#### LittleFS_FileRename

```c
LittleFS_Status_t LittleFS_FileRename(const char *old_path, const char *new_path);
```

重命名文件。

**参数**：
- `old_path`：旧路径（相对路径）
- `new_path`：新路径（相对路径）

**返回值**：
- `LITTLEFS_OK`：重命名成功
- 其他错误码：重命名失败

### 目录操作

#### LittleFS_DirCreate

```c
LittleFS_Status_t LittleFS_DirCreate(const char *path);
```

创建目录。

**参数**：
- `path`：目录路径（相对路径）

**返回值**：
- `LITTLEFS_OK`：创建成功
- `LITTLEFS_ERROR_EXIST`：目录已存在
- 其他错误码：创建失败

#### LittleFS_DirOpen

```c
LittleFS_Status_t LittleFS_DirOpen(lfs_dir_t *dir, const char *path);
```

打开目录。

**参数**：
- `dir`：目录句柄指针（**必须在使用前清零**）
- `path`：目录路径（相对路径，`"."` 表示当前目录）

**返回值**：
- `LITTLEFS_OK`：打开成功
- 其他错误码：打开失败

#### LittleFS_DirRead

```c
LittleFS_Status_t LittleFS_DirRead(lfs_dir_t *dir, struct lfs_info *info);
```

读取目录项。

**参数**：
- `dir`：目录句柄指针
- `info`：目录项信息结构体指针

**返回值**：
- `LITTLEFS_OK`：读取成功
- `LITTLEFS_ERROR_NOENT`：目录读取完成（无更多项）
- 其他错误码：读取失败

**使用示例**：
```c
lfs_dir_t dir;
memset(&dir, 0, sizeof(dir));
LittleFS_DirOpen(&dir, ".");  // 打开根目录

struct lfs_info info;
while (LittleFS_DirRead(&dir, &info) == LITTLEFS_OK) {
    // 处理目录项
    printf("Name: %s, Size: %lu, Type: %d\n", 
           info.name, (unsigned long)info.size, info.type);
}

LittleFS_DirClose(&dir);
```

#### LittleFS_DirDelete

```c
LittleFS_Status_t LittleFS_DirDelete(const char *path);
```

删除目录（目录必须为空）。

**参数**：
- `path`：目录路径（相对路径）

**返回值**：
- `LITTLEFS_OK`：删除成功
- `LITTLEFS_ERROR_NOTEMPTY`：目录不为空
- 其他错误码：删除失败

### 文件属性操作

#### LittleFS_FileSetAttr

```c
LittleFS_Status_t LittleFS_FileSetAttr(const char *path, uint8_t type, 
                                       const void *buffer, uint32_t size);
```

设置文件属性。

**参数**：
- `path`：文件路径（相对路径）
- `type`：属性类型（0x00-0xFF，uint8_t 类型）
- `buffer`：属性数据缓冲区
- `size`：属性数据大小（字节）

**返回值**：
- `LITTLEFS_OK`：设置成功
- 其他错误码：设置失败

**注意事项**：
- 属性类型范围为 0x00-0xFF
- 建议使用 0x10-0xFF 避开系统保留值（0x00-0x0F）

#### LittleFS_FileGetAttr

```c
LittleFS_Status_t LittleFS_FileGetAttr(const char *path, uint8_t type, 
                                       void *buffer, uint32_t size, 
                                       uint32_t *actual_size);
```

获取文件属性。

**参数**：
- `path`：文件路径（相对路径）
- `type`：属性类型
- `buffer`：属性数据缓冲区
- `size`：缓冲区大小（字节）
- `actual_size`：实际属性数据大小（输出参数，可为 NULL）

**返回值**：
- `LITTLEFS_OK`：获取成功
- `LITTLEFS_ERROR_NOATTR`：属性不存在
- 其他错误码：获取失败

### 高级功能

#### LittleFS_Traverse

```c
LittleFS_Status_t LittleFS_Traverse(const char *root_path, 
                                    LittleFS_TraverseCallback_t callback, 
                                    void *user_data);
```

遍历文件系统（递归遍历所有文件和目录）。

**参数**：
- `root_path`：根路径（如 `"/"`）
- `callback`：回调函数指针
- `user_data`：用户数据指针

**回调函数类型**：
```c
typedef int (*LittleFS_TraverseCallback_t)(const char *path, 
                                            const struct lfs_info *info, 
                                            void *user_data);
```

**返回值**：
- `LITTLEFS_OK`：遍历成功
- 其他错误码：遍历失败

#### LittleFS_HealthCheck

```c
LittleFS_Status_t LittleFS_HealthCheck(LittleFS_HealthStatus_t *health_status);
```

文件系统健康检查。

**参数**：
- `health_status`：健康状态指针（输出参数）

**返回值**：
- `LITTLEFS_OK`：检查成功
- 其他错误码：检查失败

**健康状态**：
- `LITTLEFS_HEALTH_OK`：正常
- `LITTLEFS_HEALTH_CORRUPT`：文件系统损坏
- `LITTLEFS_HEALTH_BLOCK_DEVICE_ERROR`：块设备错误
- `LITTLEFS_HEALTH_UNKNOWN`：未知状态

## 🔧 配置说明

### 模块开关

在 `System/config.h` 中配置：

```c
#define CONFIG_MODULE_LITTLEFS_ENABLED  1   /**< LittleFS模块开关 */
```

### 依赖模块

- `CONFIG_MODULE_W25Q_ENABLED` - W25Q Flash 驱动（必须）
- `CONFIG_MODULE_SPI_ENABLED` - SPI 驱动（必须）
- `CONFIG_MODULE_ERROR_HANDLER_ENABLED` - 错误处理模块（推荐）

### 默认配置参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `read_size` | 256 | 最小读取大小（字节），必须与 W25Q 页大小匹配 |
| `prog_size` | 256 | 最小编程大小（字节），必须与 W25Q 页大小匹配 |
| `block_size` | 4096 | 块大小（字节），W25Q 扇区大小 |
| `block_cycles` | 1000 | 磨损均衡周期（100-1000），值越大磨损越慢 |
| `cache_size` | 256 | 缓存大小（字节），影响读写性能 |
| `lookahead_size` | 自动计算 | 前瞻缓冲区大小（字节），最小 8 |

### 性能优化配置

根据 RAM 大小选择合适的配置：

#### 平衡配置（推荐）- STM32F103C8T6

```c
LittleFS_Config_t config = {0};
LittleFS_GetDefaultConfig(&config);
config.cache_size = 128;        // 每个文件缓存 128 字节
config.lookahead_size = 128;    // 块分配缓存 128 字节
config.block_cycles = 500;      // 磨损均衡周期 500
```

**预期性能**：
- 写 1KB 文件：约 8ms
- 同时打开文件数：5-10 个（占用 RAM 约 1.5KB）

#### 极致配置（省 RAM）- STM32F103C6T6

```c
LittleFS_Config_t config = {0};
LittleFS_GetDefaultConfig(&config);
config.cache_size = 16;         // 每个文件缓存 16 字节
config.lookahead_size = 16;     // 块分配缓存 16 字节
config.block_cycles = 500;      // 磨损均衡周期 500
```

**⚠️ 警告**：
- 性能降低约 5 倍
- Flash 磨损快 16 倍

#### 最优配置（性能型）- STM32F103RCT6

```c
LittleFS_Config_t config = {0};
LittleFS_GetDefaultConfig(&config);
config.cache_size = 256;        // 每个文件缓存 256 字节
config.lookahead_size = 256;    // 块分配缓存 256 字节
config.block_cycles = 1000;     // 磨损均衡周期 1000
```

**预期性能**：
- 写 1KB 文件：约 4ms
- 同时打开文件数：3-5 个（占用 RAM 约 1.5KB）

详细配置说明请参考：[内联文件读取失败问题修复与内存配置建议.md](./内联文件读取失败问题修复与内存配置建议.md)

## ⚠️ 注意事项

### 重要提示

1. **文件句柄清零**：
   - 每次使用文件句柄前必须清零：`memset(&file, 0, sizeof(file))`
   - 目录句柄同样需要清零
   - 未清零的句柄可能包含脏数据，导致操作失败

2. **文件同步**：
   - 所有写入操作后必须调用 `LittleFS_FileSync()` 确保数据落盘
   - 未同步的数据可能在断电后丢失
   - 文件关闭前建议先同步

3. **路径格式**：
   - **使用相对路径**（如 `"test.txt"`），不要使用绝对路径（如 `"/test.txt"`）
   - 路径格式错误会导致文件操作失败（返回 `LITTLEFS_ERROR_NOENT`）
   - 与目录列表中的路径格式保持一致

4. **初始化顺序**：
   - 必须先初始化 W25Q 和 SPI 模块
   - 然后调用 `LittleFS_Init()` 初始化文件系统
   - 最后调用 `LittleFS_Mount()` 挂载文件系统

5. **格式化时机**：
   - 首次使用或文件系统损坏时需要格式化
   - 格式化会清除所有数据，仅在必要时使用
   - 挂载失败（返回 `LITTLEFS_ERROR_CORRUPT`）时先格式化再挂载

6. **Flash 物理层参数**：
   - `read_size` 和 `prog_size` 必须与 W25Q 页大小（256 字节）匹配
   - `block_size` 必须与 W25Q 扇区大小（4096 字节）匹配
   - 这些参数不可随意修改，否则会导致数据损坏

7. **文件大小限制**：
   - LittleFS 理论最大文件大小：2,147,483,647 字节（约 2GB）
   - 对于 W25Q 8MB Flash，实际可用空间约 7-8MB
   - 建议单个文件不超过 6-7MB，为文件系统预留空间
   - 文件至少占用一个块（4096 字节），小文件也会占用一个块

8. **多文件并发**：
   - 每个文件需要独立的缓存缓冲区
   - 缓存大小必须与 `cfg->cache_size` 匹配
   - 缓存缓冲区必须 4 字节对齐

9. **RTOS 支持**：
   - 如需在多线程环境下使用，需要设置锁回调函数
   - 使用 `LittleFS_SetLockCallback()` 设置锁回调

10. **错误处理**：
    - 所有文件操作必须检查返回值
    - 常见错误码：
      - `LITTLEFS_ERROR_NOENT`：文件/目录不存在
      - `LITTLEFS_ERROR_CORRUPT`：文件系统损坏
      - `LITTLEFS_ERROR_NOSPC`：空间不足
      - `LITTLEFS_ERROR_NOT_MOUNTED`：文件系统未挂载

## 📖 使用示例

### 基本使用流程

```c
#include "littlefs_wrapper.h"

int main(void)
{
    /* 1. 初始化系统 */
    System_Init();
    
    /* 2. 初始化 W25Q 和 SPI（必须先初始化） */
    W25Q_Init();
    
    /* 3. 初始化 LittleFS */
    LittleFS_Status_t status = LittleFS_Init();
    if (status != LITTLEFS_OK) {
        // 处理错误
        return -1;
    }
    
    /* 4. 挂载文件系统 */
    status = LittleFS_Mount();
    if (status == LITTLEFS_ERROR_CORRUPT) {
        // 文件系统损坏，格式化
        status = LittleFS_Format();
        if (status == LITTLEFS_OK) {
            // 格式化成功，重新挂载
            status = LittleFS_Mount();
        }
    }
    
    if (status != LITTLEFS_OK) {
        // 处理错误
        return -1;
    }
    
    /* 5. 文件操作 */
    lfs_file_t file;
    memset(&file, 0, sizeof(file));  // 必须清零
    
    // 创建并写入文件
    status = LittleFS_FileOpen(&file, "test.txt", 
                                LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (status == LITTLEFS_OK) {
        const char *data = "Hello LittleFS!";
        uint32_t bytes_written = 0;
        status = LittleFS_FileWrite(&file, data, strlen(data), &bytes_written);
        if (status == LITTLEFS_OK) {
            // 同步文件，确保数据落盘
            status = LittleFS_FileSync(&file);
        }
        LittleFS_FileClose(&file);
    }
    
    // 读取文件
    memset(&file, 0, sizeof(file));  // 必须清零
    status = LittleFS_FileOpen(&file, "test.txt", LFS_O_RDONLY);
    if (status == LITTLEFS_OK) {
        char buffer[64];
        uint32_t bytes_read = 0;
        status = LittleFS_FileRead(&file, buffer, sizeof(buffer) - 1, &bytes_read);
        if (status == LITTLEFS_OK) {
            buffer[bytes_read] = '\0';
            printf("Read: %s\n", buffer);
        }
        LittleFS_FileClose(&file);
    }
    
    return 0;
}
```

### 目录遍历示例

```c
/* 遍历根目录 */
lfs_dir_t dir;
memset(&dir, 0, sizeof(dir));  // 必须清零

LittleFS_Status_t status = LittleFS_DirOpen(&dir, ".");
if (status == LITTLEFS_OK) {
    struct lfs_info info;
    while (LittleFS_DirRead(&dir, &info) == LITTLEFS_OK) {
        printf("Name: %s, Size: %lu, Type: %d\n", 
               info.name, (unsigned long)info.size, info.type);
    }
    LittleFS_DirClose(&dir);
}
```

### 文件属性示例

```c
/* 设置文件属性（版本号） */
uint32_t version = 0x01020304;
LittleFS_FileSetAttr("config.bin", 0x10, &version, sizeof(version));

/* 获取文件属性 */
uint32_t read_version = 0;
uint32_t actual_size = 0;
LittleFS_FileGetAttr("config.bin", 0x10, &read_version, 
                     sizeof(read_version), &actual_size);
```

## 📖 相关文档

### 模块文档

- [FS_WRAPPER 模块文档](../fs_wrapper/README.md) - 应用层封装模块
- [W25Q 驱动文档](../../Drivers/flash/README.md) - Flash 驱动
- [SPI 驱动文档](../../Drivers/spi/README.md) - SPI 通信接口

### 案例文档

- [Flash10 案例文档](../../Examples/Flash/Flash10_LittleFS_W25Q_ReadWrite/README.md) - 基础文件操作
- [Flash11 案例文档](../../Examples/Flash/Flash11_LittleFS_InlineFileFix_And_MemoryConfig/README.md) - 内联文件修复和内存配置
- [Flash12 案例文档](../../Examples/Flash/Flash12_LittleFS_ComprehensiveTest/README.md) - 综合功能测试

### 技术文档

- [内联文件读取失败问题修复与内存配置建议.md](./内联文件读取失败问题修复与内存配置建议.md) - 内存配置优化指南

### 其他模块

- [错误处理文档](../../Common/README.md) - 错误处理模块
- [系统初始化文档](../../System/README.md) - 系统初始化

## 📝 更新日志

### v1.0.0 (2024-01-01)
- 初始版本
- 实现完整的文件系统功能
- 支持多实例、RTOS、日志回调等高级特性

