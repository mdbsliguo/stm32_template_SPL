# SPI通信模块

## 硬件SPI模块 (spi_hw)

### 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始�?| `SPI_HW_Init()` | 初始化SPI外设和GPIO（SPI1/SPI2/SPI3�?|
| 反初始化 | `SPI_Deinit()` | 反初始化SPI外设 |
| 主模式发�?| `SPI_MasterTransmit()` | 主模式发送数据（8位） |
| 主模式接�?| `SPI_MasterReceive()` | 主模式接收数据（8位） |
| 全双工传�?| `SPI_MasterTransmitReceive()` | 主模式全双工传输�?位） |
| 发送单字节 | `SPI_MasterTransmitByte()` | 发送单个字�?|
| 接收单字�?| `SPI_MasterReceiveByte()` | 接收单个字节 |
| 发�?6�?| `SPI_MasterTransmit16()` | 主模式发送数据（16位） |
| 接收16�?| `SPI_MasterReceive16()` | 主模式接收数据（16位） |
| 全双�?6�?| `SPI_MasterTransmitReceive16()` | 主模式全双工传输�?6位） |
| NSS控制 | `SPI_NSS_Low()` / `SPI_NSS_High()` | 软件NSS控制（拉�?拉高�?|
| 状态检�?| `SPI_IsInitialized()` | 检查是否已初始�?|
| 获取外设 | `SPI_GetPeriph()` | 获取SPI外设指针 |

### 重要提醒

1. **实例选择**：支持SPI1、SPI2、SPI3三个实例
2. **配置驱动**：GPIO和SPI参数在`BSP/board.h`中配置，自动初始�?3. **超时处理**：所有传输函数支持超时参数，0表示使用默认超时
4. **轮询模式**：当前实现为轮询模式，阻塞等待传输完�?5. **函数命名**：使用`SPI_HW_Init()`而非`SPI_Init()`，避免与STM32标准库冲�?6. **NSS控制**：支持硬件NSS和软件NSS，软件NSS需手动控制

---

## 软件SPI模块 (spi_sw)

### 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始�?| `SPI_SW_Init()` | 初始化软件SPI（任意GPIO引脚�?|
| 反初始化 | `SPI_SW_Deinit()` | 反初始化软件SPI |
| 主模式发�?| `SPI_SW_MasterTransmit()` | 主模式发送数据（8位） |
| 主模式接�?| `SPI_SW_MasterReceive()` | 主模式接收数据（8位） |
| 全双工传�?| `SPI_SW_MasterTransmitReceive()` | 主模式全双工传输�?位） |
| 发送单字节 | `SPI_SW_MasterTransmitByte()` | 发送单个字�?|
| 接收单字�?| `SPI_SW_MasterReceiveByte()` | 接收单个字节 |
| 发�?6�?| `SPI_SW_MasterTransmit16()` | 主模式发送数据（16位） |
| 接收16�?| `SPI_SW_MasterReceive16()` | 主模式接收数据（16位） |
| 全双�?6�?| `SPI_SW_MasterTransmitReceive16()` | 主模式全双工传输�?6位） |
| NSS控制 | `SPI_SW_NSS_Low()` / `SPI_SW_NSS_High()` | 软件NSS控制（拉�?拉高�?|
| 状态检�?| `SPI_SW_IsInitialized()` | 检查是否已初始�?|

### 重要提醒

1. **引脚灵活**：支持任意GPIO引脚配置，不依赖硬件SPI外设
2. **多实例支�?*：支�?个软件SPI实例（`SPI_SW_INSTANCE_1~4`�?3. **SPI模式**：支�?种SPI模式（CPOL/CPHA组合），可配置MSB/LSB优先
4. **配置驱动**：GPIO引脚和SPI参数在`BSP/board.h`中配置，自动初始�?5. **时序控制**：通过GPIO模拟SPI时序，速度较硬件SPI�?6. **依赖GPIO**：需要先初始化GPIO模块
7. **NSS控制**：必须手动控制NSS信号（片选）
