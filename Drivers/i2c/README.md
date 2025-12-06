# I2C通信模块

## 硬件I2C模块 (i2c_hw)

### 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始�?| `I2C_HW_Init()` | 初始化I2C外设和GPIO（I2C1/I2C2�?|
| 反初始化 | `I2C_Deinit()` | 反初始化I2C外设 |
| 主模式发�?| `I2C_MasterTransmit()` | 主模式发送数据（7位地址�?|
| 主模式接�?| `I2C_MasterReceive()` | 主模式接收数据（7位地址�?|
| 写寄存器 | `I2C_MasterWriteReg()` | 写单个寄存器（常用封装） |
| 读寄存器 | `I2C_MasterReadReg()` | 读单个寄存器（常用封装） |
| 写多寄存�?| `I2C_MasterWriteRegs()` | 写多个连续寄存器 |
| 读多寄存�?| `I2C_MasterReadRegs()` | 读多个连续寄存器 |
| 10位地址发�?| `I2C_MasterTransmit10bit()` | 主模式发送（10位地址�?|
| 10位地址接收 | `I2C_MasterReceive10bit()` | 主模式接收（10位地址�?|
| 总线扫描 | `I2C_ScanBus()` | 扫描I2C总线上的设备 |
| 软件复位 | `I2C_SoftwareReset()` | I2C总线软件复位 |
| 状态检�?| `I2C_IsInitialized()` | 检查是否已初始�?|
| 状态检�?| `I2C_IsBusBusy()` | 检查总线是否�?|
| 获取外设 | `I2C_GetPeriph()` | 获取I2C外设指针 |

### 重要提醒

1. **实例选择**：支持I2C1和I2C2两个实例，使用`I2C_INSTANCE_1`或`I2C_INSTANCE_2`
2. **配置驱动**：GPIO和I2C参数在`BSP/board.h`中配置，自动初始�?3. **超时处理**：所有传输函数支持超时参数，0表示使用默认超时
4. **轮询模式**：当前实现为轮询模式，阻塞等待传输完�?5. **函数命名**：使用`I2C_HW_Init()`而非`I2C_Init()`，避免与STM32标准库冲�?
---

## 软件I2C模块 (i2c_sw)

### 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始�?| `I2C_SW_Init()` | 初始化软件I2C（任意GPIO引脚�?|
| 反初始化 | `SoftI2C_Deinit()` | 反初始化软件I2C |
| 主模式发�?| `SoftI2C_MasterTransmit()` | 主模式发送数据（7位地址�?|
| 主模式接�?| `SoftI2C_MasterReceive()` | 主模式接收数据（7位地址�?|
| 写寄存器 | `SoftI2C_MasterWriteReg()` | 写单个寄存器（常用封装） |
| 读寄存器 | `SoftI2C_MasterReadReg()` | 读单个寄存器（常用封装） |
| 写多寄存�?| `SoftI2C_MasterWriteRegs()` | 写多个连续寄存器 |
| 读多寄存�?| `SoftI2C_MasterReadRegs()` | 读多个连续寄存器 |
| 10位地址发�?| `SoftI2C_MasterTransmit10bit()` | 主模式发送（10位地址�?|
| 10位地址接收 | `SoftI2C_MasterReceive10bit()` | 主模式接收（10位地址�?|
| 总线扫描 | `SoftI2C_ScanBus()` | 扫描I2C总线上的设备 |
| 软件复位 | `SoftI2C_SoftwareReset()` | I2C总线软件复位（发�?个时钟脉冲） |
| 状态检�?| `SoftI2C_IsInitialized()` | 检查是否已初始�?|
| 状态检�?| `SoftI2C_IsBusBusy()` | 检查总线是否忙（检测SDA线） |

### 重要提醒

1. **引脚灵活**：支持任意GPIO引脚配置，不依赖硬件I2C外设
2. **多实例支�?*：支�?个软件I2C实例（`SOFT_I2C_INSTANCE_1~4`�?3. **配置驱动**：GPIO引脚在`BSP/board.h`中配置，自动初始�?4. **超时处理**：所有传输函数支持超时参数，0表示不超�?5. **时序控制**：通过GPIO模拟I2C时序，速度较硬件I2C�?6. **依赖GPIO**：需要先初始化GPIO模块
