# Flash驱动模块

## W25Q SPI Flash驱动模块 (w25q_spi)

### 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始化 | `W25Q_Init()` | 自动识别Flash型号并配置，≥128Mb芯片自动进入4字节模式 |
| 反初始化 | `W25Q_Deinit()` | 反初始化W25Q模块 |
| 获取信息 | `W25Q_GetInfo()` | 获取设备信息只读指针（容量、地址字节数等） |
| 状态检查 | `W25Q_IsInitialized()` | 检查是否已初始化 |
| 读取数据 | `W25Q_Read()` | 读取数据，内部自动处理跨页 |
| 写入数据 | `W25Q_Write()` | 写入数据，内部自动处理跨页和页边界（256字节） |
| 擦除扇区 | `W25Q_EraseSector()` | 擦除4KB扇区，自动检查地址对齐 |
| 整片擦除 | `W25Q_EraseChip()` | 整片擦除（谨慎使用） |
| 等待就绪 | `W25Q_WaitReady()` | 动态超时补偿等待Flash就绪 |

### 重要提醒

1. **状态集中管理**：所有容量差异信息保存在全局结构体 `g_w25q_device` 中，禁止分散存储
2. **接口纯净性**：API参数仅限 `uint32_t addr`，禁止传入 `sector_idx` 或 `page_num`
3. **失败熔断机制**：初始化失败后所有API返回 `W25Q_ERROR_NOT_INIT`，禁止带病运行
4. **64位地址运算**：所有 `addr + len` 计算必须强制转换为 `uint64_t`，防止溢出
5. **4字节模式非易失性**：≥128Mb芯片每次初始化必须重新发送0xB7指令，不能假设模式会保持
6. **编译时优化**：通过 `#define W25Q_FIXED_MODEL W25Q_MODEL_W25Q64` 锁定型号，可减少初始化时间50%以上
7. **动态超时补偿**：Gb级芯片（≥16MB）使用更长超时时间，擦除操作根据扇区索引动态调整
8. **地址越界检查**：所有读写擦除操作都会检查地址越界，越界时返回 `W25Q_ERROR_OUT_OF_BOUND`
9. **SPI依赖**：需要先初始化SPI模块（`SPI_HW_Init()`），确保SPI驱动已启用
10. **NSS控制**：使用软件NSS控制，确保SPI驱动的NSS控制函数已正确实现

### 支持的型号

| 型号 | 容量 | 地址字节 | 4字节模式 | 设备ID |
|------|------|----------|-----------|--------|
| W25Q16 | 2MB | 3 | 否 | 0xEF4015 |
| W25Q32 | 4MB | 3 | 否 | 0xEF4016 |
| W25Q64 | 8MB | 3 | 否 | 0xEF4017 |
| GD25Q64 | 8MB | 3 | 否 | 0xC84017（兼容W25Q64） |
| W25Q128 | 16MB | 4 | 是 | 0xEF4018 |
| W25Q256 | 32MB | 4 | 是 | 0xEF4019 |

### 配置说明

#### 1. 模块开关配置

在 `System/config.h` 中配置：

```c
#define CONFIG_MODULE_W25Q_ENABLED   1   /**< W25Q模块开关 */
```

#### 2. SPI实例配置

在 `BSP/board.h` 中配置：

```c
#ifndef W25Q_SPI_INSTANCE
#define W25Q_SPI_INSTANCE SPI_INSTANCE_1  /**< W25Q使用的SPI实例，默认SPI1 */
#endif
```

#### 3. 编译时优化（可选）

在编译选项中定义固定型号，可减少初始化时间：

```c
#define W25Q_FIXED_MODEL W25Q_MODEL_W25Q64   // 锁定为W25Q64
// 或
#define W25Q_FIXED_MODEL W25Q_MODEL_W25Q256  // 锁定为W25Q256
```

**支持的固定型号宏**：
- `W25Q_MODEL_W25Q16`
- `W25Q_MODEL_W25Q32`
- `W25Q_MODEL_W25Q64`
- `W25Q_MODEL_W25Q128`
- `W25Q_MODEL_W25Q256`
- `W25Q_MODEL_GD25Q64`

### 使用示例

#### 基本使用

```c
#include "w25q_spi.h"

/* 初始化W25Q */
W25Q_Status_t status = W25Q_Init();
if (status != W25Q_OK)
{
    /* 初始化失败处理 */
    ErrorHandler_Handle(status, "W25Q");
    return;
}

/* 获取设备信息 */
const w25q_dev_t *info = W25Q_GetInfo();
if (info != NULL)
{
    printf("Capacity: %d MB\n", info->capacity_mb);
    printf("Address bytes: %d\n", info->addr_bytes);
    printf("4-byte mode: %s\n", info->is_4byte_mode ? "Yes" : "No");
}

/* 读取数据 */
uint8_t read_buf[256];
status = W25Q_Read(0x0000, read_buf, sizeof(read_buf));
if (status != W25Q_OK)
{
    /* 读取失败处理 */
}

/* 写入数据 */
uint8_t write_buf[256] = {0x01, 0x02, 0x03, ...};
status = W25Q_Write(0x0000, write_buf, sizeof(write_buf));
if (status != W25Q_OK)
{
    /* 写入失败处理 */
}

/* 擦除扇区（4KB） */
status = W25Q_EraseSector(0x0000);  // 地址必须是4KB对齐
if (status != W25Q_OK)
{
    /* 擦除失败处理 */
}
```

#### 完整示例：读写操作

```c
/* 初始化 */
W25Q_Init();

/* 擦除扇区（写入前必须先擦除） */
W25Q_EraseSector(0x0000);

/* 等待擦除完成 */
W25Q_WaitReady(0);

/* 写入数据 */
uint8_t data[256];
memset(data, 0xAA, sizeof(data));
W25Q_Write(0x0000, data, sizeof(data));

/* 等待写入完成 */
W25Q_WaitReady(0);

/* 读取数据验证 */
uint8_t read_data[256];
W25Q_Read(0x0000, read_data, sizeof(read_data));

/* 验证数据 */
if (memcmp(data, read_data, sizeof(data)) == 0)
{
    printf("Data verification OK\n");
}
```

#### 跨页写入示例

```c
/* 写入跨页数据（自动处理） */
uint8_t large_data[512];  // 超过256字节，跨页
memset(large_data, 0x55, sizeof(large_data));

/* 擦除相关扇区 */
W25Q_EraseSector(0x0000);
W25Q_EraseSector(0x1000);  // 如果数据跨扇区

/* 写入数据（自动处理跨页） */
W25Q_Write(0x00FF, large_data, sizeof(large_data));
// 从0x00FF开始写入，会自动处理：
// - 第一页：写入0x00FF~0x00FF（1字节）
// - 第二页：写入0x0100~0x01FF（256字节）
// - 第三页：写入0x0200~0x02FE（255字节）
```

### 错误码说明

| 错误码 | 值 | 说明 |
|--------|-----|------|
| `W25Q_OK` | 0 | 操作成功 |
| `W25Q_ERROR_INIT_FAILED` | -3601 | 初始化失败 |
| `W25Q_ERROR_ID_MISMATCH` | -3602 | ID不匹配（未识别的型号） |
| `W25Q_ERROR_OUT_OF_BOUND` | -3603 | 地址越界 |
| `W25Q_ERROR_4BYTE_MODE_FAIL` | -3604 | 4字节模式切换失败 |
| `W25Q_ERROR_NOT_INIT` | -3605 | 模块未初始化 |
| `W25Q_ERROR_TIMEOUT` | -3606 | 操作超时 |

### 硬件连接

**SPI信号连接**：
- **SCK（时钟）**：连接到STM32的SPI SCK引脚（如PA5）
- **MISO（主入从出）**：连接到STM32的SPI MISO引脚（如PA6）
- **MOSI（主出从入）**：连接到STM32的SPI MOSI引脚（如PA7）
- **CS/NSS（片选）**：连接到STM32的SPI NSS引脚（如PA4，软件NSS模式）

**电源连接**：
- **VCC**：3.3V电源
- **GND**：公共地
- **HOLD**：保持引脚（可选，通常接VCC或悬空）
- **WP**：写保护引脚（可选，通常接VCC或悬空）

**注意**：
- W25Q系列Flash使用3.3V供电，不能使用5V
- 确保SPI时钟频率不超过芯片规格（通常≤50MHz）
- 建议在CS和VCC之间添加上拉电阻（10kΩ）

### 设计约束说明

#### 1. 状态集中管理

所有容量差异信息必须保存在全局结构体 `g_w25q_device` 中：

```c
typedef struct {
    uint32_t capacity_mb;        // 容量（MB）
    uint8_t addr_bytes;          // 地址字节数（3或4）
    uint8_t is_4byte_mode;       // 4字节模式标志
    W25Q_State_t state;          // 模块状态
    uint16_t manufacturer_id;    // 制造商ID
    uint16_t device_id;          // 设备ID
} w25q_dev_t;
```

**禁止**：
- ? 使用局部static变量存储容量信息
- ? 使用#define宏分支处理容量差异
- ? 在API参数中传入容量相关参数

#### 2. 接口纯净性

所有API使用统一的 `uint32_t addr` 地址参数：

```c
// ? 正确
W25Q_Read(0x0000, buf, len);
W25Q_Write(0x0000, buf, len);
W25Q_EraseSector(0x0000);

// ? 错误（禁止）
W25Q_ReadSector(sector_idx, buf, len);  // 禁止传入sector_idx
W25Q_WritePage(page_num, buf, len);     // 禁止传入page_num
```

#### 3. 64位地址运算

所有地址计算必须使用 `uint64_t`：

```c
// ? 正确
uint64_t capacity_bytes = (uint64_t)g_w25q_device.capacity_mb * 1024ULL * 1024ULL;
if ((uint64_t)addr + (uint64_t)len > capacity_bytes)
{
    return W25Q_ERROR_OUT_OF_BOUND;
}

// ? 错误（可能溢出）
if (addr + len > g_w25q_device.capacity_mb * 1024 * 1024)  // 32位运算可能溢出
```

#### 4. 4字节模式非易失性

≥128Mb芯片每次初始化必须重新发送0xB7指令：

```c
// 每次W25Q_Init()都会：
// 1. 发送0xB7指令进入4字节模式
// 2. 读取状态寄存器3验证S23位
// 3. 验证失败返回W25Q_ERROR_4BYTE_MODE_FAIL
```

**不能假设**：4字节模式会在断电后保持，必须每次初始化重新配置。

### 性能优化

#### 编译时优化

通过定义 `W25Q_FIXED_MODEL` 可以：
- 跳过ID识别表查找
- 直接填充容量信息
- **代码体积增加≤5%**
- **初始化时间减少≥50%**

```c
// 在编译选项中定义
#define W25Q_FIXED_MODEL W25Q_MODEL_W25Q64
```

#### 动态超时补偿

- **普通芯片**：基础超时时间
- **Gb级芯片（≥16MB）**：
  - `W25Q_WaitReady()`: 超时时间×2
  - `W25Q_EraseSector()`: `timeout = sector_idx * 2ms + 100ms`（最大200ms）
  - `W25Q_EraseChip()`: 超时时间×2（60秒）

### 常见问题

#### 1. 初始化失败

**可能原因**：
- SPI模块未初始化：确保先调用 `SPI_HW_Init(W25Q_SPI_INSTANCE)`
- SPI模块未启用：检查 `config.h` 中 `CONFIG_MODULE_SPI_ENABLED` 是否为1
- 硬件连接错误：检查SPI信号线连接
- 芯片型号不匹配：检查设备ID是否在支持列表中

**排查步骤**：
1. 检查SPI初始化状态：`SPI_IsInitialized(W25Q_SPI_INSTANCE)`
2. 检查SPI配置：确认 `board.h` 中SPI配置正确
3. 检查硬件连接：用示波器检查SPI信号
4. 检查设备ID：读取ID寄存器确认芯片型号

#### 2. 写入失败或数据错误

**可能原因**：
- 写入前未擦除：Flash必须先擦除才能写入
- 地址未对齐：扇区擦除地址必须是4KB对齐
- 跨页写入：虽然自动处理，但建议按页对齐写入以提高性能
- 写入保护：检查WP引脚是否被拉低

**排查步骤**：
1. 确保写入前已擦除扇区
2. 检查地址对齐（扇区擦除：4KB对齐）
3. 验证写入数据：写入后读取验证
4. 检查WP引脚状态

#### 3. 4字节模式切换失败

**可能原因**：
- 芯片不支持4字节模式：检查芯片型号（只有≥128Mb支持）
- 状态寄存器读取失败：检查SPI通信
- 芯片损坏：尝试重新上电

**排查步骤**：
1. 确认芯片型号：检查 `W25Q_GetInfo()` 返回的容量
2. 检查状态寄存器3：读取S23位确认模式
3. 重新初始化：调用 `W25Q_Deinit()` 后重新 `W25Q_Init()`

#### 4. 地址越界错误

**可能原因**：
- 地址超出芯片容量：检查地址范围
- 地址+长度溢出：使用64位运算检查

**示例**：
```c
// W25Q64（8MB）的地址范围：0x000000 ~ 0x7FFFFF
W25Q_Read(0x7FFFFF, buf, 1);   // ? 正确（最后一个字节）
W25Q_Read(0x800000, buf, 1);   // ? 错误（越界）
```

#### 5. 擦除超时

**可能原因**：
- 芯片容量大：Gb级芯片擦除时间更长
- 扇区索引大：后面的扇区擦除时间更长（动态补偿）
- 芯片损坏：尝试重新上电

**解决方案**：
- 使用动态超时补偿（已自动实现）
- 增加超时时间：`W25Q_WaitReady(timeout_ms)` 传入更大的超时值
- 检查芯片状态：读取状态寄存器确认芯片是否正常

### 注意事项

1. **写入前必须擦除**：Flash必须先擦除才能写入，擦除单位是扇区（4KB）
2. **页边界处理**：写入会自动处理跨页，但建议按页对齐以提高性能
3. **扇区对齐**：扇区擦除地址必须是4KB对齐（0x0000, 0x1000, 0x2000...）
4. **整片擦除谨慎**：`W25Q_EraseChip()` 会擦除整个Flash，请谨慎使用
5. **初始化顺序**：必须先初始化SPI模块，再初始化W25Q模块
6. **错误处理**：所有API返回 `W25Q_Status_t`，必须检查返回值
7. **线程安全**：当前实现不支持多线程，多线程环境需要添加互斥锁

### 相关模块

- **SPI驱动**：`Drivers/spi/spi_hw.c/h` - 硬件SPI驱动
- **延时模块**：`System/delay.c/h` - 延时功能（用于超时处理）
- **错误处理**：`Common/error_handler.c/h` - 错误处理模块

