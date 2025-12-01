# FS_WRAPPER - 文件系统应用层封装模块

## 📋 模块简介

`fs_wrapper` 是在 `littlefs_wrapper` 基础上提供的应用层封装模块，旨在解决路径管理、错误处理内聚和代码简化问题。

### 核心特性

- ✅ **路径集中管理**：使用目录枚举避免硬编码路径
- ✅ **错误处理内聚**：业务代码无需逐操作检查返回值
- ✅ **自动文件句柄管理**：内部自动处理文件打开/关闭
- ✅ **自动同步**：写入操作后自动同步，确保数据落盘
- ✅ **轻量级设计**：约100-200行代码，保持简洁

### 设计原则

1. **轻量级封装**：不重复实现底层功能，仅提供便捷接口
2. **路径管理**：集中管理文件路径，避免硬编码
3. **错误处理内聚**：将错误处理逻辑封装在内部
4. **自动资源管理**：自动管理文件句柄等资源

## 🎯 使用场景

适用于需要在外部Flash上实现文件系统功能的应用，特别是：
- 字库文件存储和读取
- 配置文件管理
- 日志文件存储
- UI资源文件管理

## 📚 API说明

### 初始化

```c
error_code_t FS_Init(void);
```

初始化文件系统封装模块，内部自动处理：
- LittleFS初始化（如果未初始化）
- 文件系统挂载
- **智能格式化**：只在文件系统损坏（`LITTLEFS_ERROR_CORRUPT`）时自动格式化并重新挂载
  - ✅ 首次使用：自动格式化
  - ✅ 文件系统损坏：自动格式化恢复
  - ❌ IO错误：不格式化，直接返回错误（避免数据丢失）
  - ❌ 配置错误：不格式化，直接返回错误
- 创建必要的目录结构

### 读取文件

```c
error_code_t FS_ReadFile(fs_dir_t dir, const char* name, uint32_t offset, 
                         void* buf, uint32_t size);
```

从指定目录读取文件数据：
- `dir`: 目录枚举（FS_DIR_FONT、FS_DIR_CONFIG等）
- `name`: 文件名（不含路径）
- `offset`: 读取偏移量（字节）
- `buf`: 数据缓冲区
- `size`: 读取大小（字节）

**特点**：
- 内部自动管理文件句柄
- 自动定位到指定偏移量
- 错误处理内聚

### 写入文件

```c
error_code_t FS_WriteFile(fs_dir_t dir, const char* name, 
                          const void* buf, uint32_t size);
```

向指定目录写入文件数据：
- `dir`: 目录枚举
- `name`: 文件名（不含路径）
- `buf`: 数据缓冲区
- `size`: 写入大小（字节）

**特点**：
- 内部自动管理文件句柄
- 文件不存在时自动创建
- 文件存在时自动覆盖
- 写入后自动同步，确保数据落盘

### 获取文件路径

```c
const char* FS_GetPath(fs_dir_t dir, const char* name);
```

获取文件的完整路径：
- `dir`: 目录枚举
- `name`: 文件名（不含路径）
- 返回：完整路径字符串（格式：`/dir_name/file_name`）

**注意**：返回的路径字符串存储在静态缓冲区中，不要修改返回值。

### 检查初始化状态

```c
uint8_t FS_IsInitialized(void);
```

检查模块是否已初始化。

**返回值**：
- `1`：已初始化
- `0`：未初始化

### 错误码说明

| 错误码 | 值 | 说明 |
|--------|-----|------|
| `FS_WRAPPER_OK` | `ERROR_OK` | 操作成功 |
| `FS_WRAPPER_ERROR_NOT_INIT` | `ERROR_BASE_FS_WRAPPER - 1` | 模块未初始化 |
| `FS_WRAPPER_ERROR_INVALID_PARAM` | `ERROR_BASE_FS_WRAPPER - 2` | 无效参数 |
| `FS_WRAPPER_ERROR_INVALID_DIR` | `ERROR_BASE_FS_WRAPPER - 3` | 无效目录 |
| `FS_WRAPPER_ERROR_NULL_PTR` | `ERROR_BASE_FS_WRAPPER - 4` | 空指针 |
| `FS_WRAPPER_ERROR_READ_FAILED` | `ERROR_BASE_FS_WRAPPER - 5` | 读取失败 |
| `FS_WRAPPER_ERROR_WRITE_FAILED` | `ERROR_BASE_FS_WRAPPER - 6` | 写入失败 |
| `FS_WRAPPER_ERROR_SYNC_FAILED` | `ERROR_BASE_FS_WRAPPER - 7` | 同步失败 |
| `FS_WRAPPER_ERROR_LITTLEFS` | `ERROR_BASE_FS_WRAPPER - 8` | LittleFS 错误 |

## 📖 使用示例

### 基本使用

```c
#include "fs_wrapper.h"

int main(void)
{
    /* 初始化系统 */
    System_Init();
    
    /* 初始化文件系统封装模块 */
    error_code_t status = FS_Init();
    if (status != FS_WRAPPER_OK) {
        /* 初始化失败处理 */
        return -1;
    }
    
    /* 写入配置文件 */
    const char* config_data = "key1=value1\nkey2=value2\n";
    status = FS_WriteFile(FS_DIR_CONFIG, "app.cfg", config_data, strlen(config_data));
    if (status != FS_WRAPPER_OK) {
        /* 写入失败处理 */
    }
    
    /* 读取配置文件 */
    char read_buf[256];
    status = FS_ReadFile(FS_DIR_CONFIG, "app.cfg", 0, read_buf, sizeof(read_buf));
    if (status != FS_WRAPPER_OK) {
        /* 读取失败处理 */
    }
    
    return 0;
}
```

### 完整示例：字库文件管理

```c
#include "fs_wrapper.h"

/* 写入字库文件 */
error_code_t WriteFontFile(const char* font_name, const uint8_t* font_data, uint32_t size)
{
    return FS_WriteFile(FS_DIR_FONT, font_name, font_data, size);
}

/* 读取字库文件（从指定偏移量） */
error_code_t ReadFontFile(const char* font_name, uint32_t offset, 
                          uint8_t* buffer, uint32_t size)
{
    return FS_ReadFile(FS_DIR_FONT, font_name, offset, buffer, size);
}

/* 获取字库文件路径 */
const char* GetFontPath(const char* font_name)
{
    return FS_GetPath(FS_DIR_FONT, font_name);
}
```

### 完整示例：日志文件管理

```c
#include "fs_wrapper.h"
#include <stdio.h>
#include <string.h>

/* 追加日志到文件 */
error_code_t AppendLog(const char* log_file, const char* message)
{
    /* 读取现有日志 */
    char existing_log[1024] = {0};
    error_code_t status = FS_ReadFile(FS_DIR_LOG, log_file, 0, 
                                      existing_log, sizeof(existing_log) - 1);
    
    /* 如果文件不存在，status 会返回错误，但可以继续写入新文件 */
    
    /* 追加新日志 */
    char new_log[2048];
    snprintf(new_log, sizeof(new_log), "%s%s\n", 
             (status == FS_WRAPPER_OK) ? existing_log : "", message);
    
    /* 写入文件（自动覆盖） */
    return FS_WriteFile(FS_DIR_LOG, log_file, new_log, strlen(new_log));
}
```

### 完整示例：配置文件管理

```c
#include "fs_wrapper.h"
#include <string.h>

/* 读取配置项 */
error_code_t ReadConfig(const char* config_file, const char* key, 
                        char* value, uint32_t value_size)
{
    char config_data[512];
    error_code_t status = FS_ReadFile(FS_DIR_CONFIG, config_file, 0, 
                                      config_data, sizeof(config_data) - 1);
    if (status != FS_WRAPPER_OK) {
        return status;
    }
    
    config_data[sizeof(config_data) - 1] = '\0';
    
    /* 解析配置（简单示例） */
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "%s=", key);
    char* key_pos = strstr(config_data, search_key);
    if (key_pos == NULL) {
        return FS_WRAPPER_ERROR_INVALID_PARAM;
    }
    
    key_pos += strlen(search_key);
    char* end_pos = strchr(key_pos, '\n');
    if (end_pos == NULL) {
        end_pos = key_pos + strlen(key_pos);
    }
    
    uint32_t value_len = end_pos - key_pos;
    if (value_len >= value_size) {
        value_len = value_size - 1;
    }
    
    memcpy(value, key_pos, value_len);
    value[value_len] = '\0';
    
    return FS_WRAPPER_OK;
}
```

### 对比：使用前 vs 使用后

**使用前（直接使用littlefs_wrapper）：**
```c
/* 需要手动管理文件句柄、错误处理等 */
lfs_file_t file;
memset(&file, 0, sizeof(file));

LittleFS_Status_t status = LittleFS_FileOpen(&file, "/config/app.cfg", 
                                              LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
if (status != LITTLEFS_OK) {
    /* 错误处理 */
    return;
}

uint32_t bytes_written = 0;
status = LittleFS_FileWrite(&file, config_data, strlen(config_data), &bytes_written);
if (status != LITTLEFS_OK) {
    LittleFS_FileClose(&file);
    /* 错误处理 */
    return;
}

status = LittleFS_FileSync(&file);
if (status != LITTLEFS_OK) {
    LittleFS_FileClose(&file);
    /* 错误处理 */
    return;
}

LittleFS_FileClose(&file);
```

**使用后（使用fs_wrapper）：**
```c
/* 一行代码完成，错误处理内聚 */
error_code_t status = FS_WriteFile(FS_DIR_CONFIG, "app.cfg", config_data, strlen(config_data));
if (status != FS_WRAPPER_OK) {
    /* 错误处理 */
}
```

## 🔧 配置说明

### 模块开关

在 `System/config.h` 中配置：

```c
#define CONFIG_MODULE_FS_WRAPPER_ENABLED  1   /**< FS_WRAPPER模块开关 */
```

### 依赖模块

- `CONFIG_MODULE_LITTLEFS_ENABLED` - LittleFS文件系统模块（必须）
- `CONFIG_MODULE_W25Q_ENABLED` - W25Q Flash驱动（必须）
- `CONFIG_MODULE_ERROR_HANDLER_ENABLED` - 错误处理模块（推荐）

## 📊 目录结构

模块支持以下目录：

| 目录枚举 | 目录名称 | 用途 |
|---------|---------|------|
| FS_DIR_FONT | /font | 字库文件 |
| FS_DIR_CONFIG | /config | 配置文件 |
| FS_DIR_LOG | /log | 日志文件 |
| FS_DIR_UI | /ui | UI资源文件 |

## ⚠️ 注意事项

1. **初始化顺序**：
   - 必须先初始化LittleFS和W25Q模块
   - 然后调用 `FS_Init()` 初始化封装模块

2. **格式化策略**：
   - **自动格式化**：只在文件系统损坏（`LITTLEFS_ERROR_CORRUPT`）时自动格式化
   - **首次使用**：文件系统未格式化时，挂载会返回CORRUPT错误，自动格式化
   - **IO错误**：如果挂载返回IO错误，不会格式化，直接返回错误（避免数据丢失）
   - **手动格式化**：如需强制格式化，可先调用 `LittleFS_Format()`，再调用 `FS_Init()`

2. **路径管理**：
   - 使用目录枚举避免硬编码路径
   - 文件名不应包含路径分隔符

3. **错误处理**：
   - 所有函数返回 `error_code_t`
   - 错误信息通过error_handler记录
   - 业务代码只需检查返回值

4. **线程安全**：
   - 当前版本不支持多线程并发访问
   - 如需多线程支持，建议在应用层加锁

5. **文件路径获取**：
   - `FS_GetPath()` 返回的路径字符串存储在静态缓冲区中
   - 不要修改返回值，不要保存指针（下次调用会被覆盖）
   - 如需保存路径，应复制字符串内容

6. **文件大小限制**：
   - 受底层 LittleFS 文件系统限制
   - 建议单个文件不超过 6-7MB（W25Q 8MB Flash）
   - 小文件也会占用一个块（4096 字节）

7. **错误处理**：
   - 所有函数返回 `error_code_t`，使用统一的错误码系统
   - 错误信息通过 `error_handler` 模块记录（如果启用）
   - 业务代码只需检查返回值，无需逐操作检查

## 🔍 常见问题排查

### 问题1：初始化失败

**现象**：`FS_Init()` 返回错误

**可能原因**：
- W25Q 未初始化
- LittleFS 初始化失败
- 文件系统挂载失败且格式化失败

**解决方法**：
1. 确保 W25Q 已正确初始化
2. 检查 SPI 通信是否正常
3. 检查 Flash 是否损坏

### 问题2：文件读取失败

**现象**：`FS_ReadFile()` 返回 `FS_WRAPPER_ERROR_READ_FAILED`

**可能原因**：
- 文件不存在
- 文件路径错误
- 读取偏移量超出文件大小

**解决方法**：
1. 使用 `FS_GetPath()` 获取完整路径，检查文件是否存在
2. 确保文件名正确，不包含路径分隔符
3. 检查读取偏移量是否合理

### 问题3：文件写入失败

**现象**：`FS_WriteFile()` 返回 `FS_WRAPPER_ERROR_WRITE_FAILED`

**可能原因**：
- 文件系统空间不足
- Flash 写入失败
- 文件系统损坏

**解决方法**：
1. 检查文件系统空闲空间（使用 `LittleFS_GetInfo()`）
2. 检查 Flash 是否正常工作
3. 尝试重新格式化文件系统

### 问题4：路径获取返回 NULL

**现象**：`FS_GetPath()` 返回 NULL

**可能原因**：
- 目录枚举无效
- 文件名为 NULL
- 路径缓冲区溢出

**解决方法**：
1. 检查目录枚举值是否在有效范围内
2. 确保文件名不为 NULL
3. 检查文件名长度（路径缓冲区大小为 64 字节）

## 📖 相关文档

- [LittleFS模块文档](../littlefs/README.md)
- [W25Q驱动文档](../../Drivers/flash/README.md)
- [错误处理文档](../../Common/README.md)

## 📝 更新日志

### v1.0.0 (2024-01-01)
- 初始版本
- 实现路径管理、简化读写接口、初始化封装

