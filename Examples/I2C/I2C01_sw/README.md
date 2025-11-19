# 案例12 - DS3231实时时钟模块演示（软件I2C接口）

✅ **已创建**

## 📋 功能说明

本案例演示如何使用**DS3231实时时钟模块**，展示DS3231的常规配置流程和常用功能，使用**软件I2C接口**。

### 核心功能

1. **DS3231初始化（软件I2C接口）**
   - 演示软件I2C接口的初始化步骤
   - 展示软件I2C的配置和使用（PB10/11）

2. **DS3231常规配置流程**
   - 检查并清除振荡器停止标志（OSF）
   - 启动振荡器
   - 配置方波输出
   - 禁用32kHz输出
   - 配置中断模式

3. **DS3231常用功能示例**
   - 时间读取和设置
   - 温度读取
   - 闹钟配置（Alarm 1）
   - 闹钟标志检查
   - 方波输出控制

4. **主循环实时显示**
   - 每秒读取并显示当前时间
   - LED闪烁指示系统运行

## 🔧 硬件要求

- **LED1**：连接到 `PA1`（系统状态指示）
- **DS3231实时时钟模块**（I2C接口）：
  - SCL：`PB10`（软件I2C）
  - SDA：`PB11`（软件I2C）
  - VCC：3.3V
  - GND：GND
  - **注意**：DS3231模块需要外部上拉电阻（通常4.7kΩ-10kΩ）连接到SCL和SDA
- **OLED显示屏**（软件I2C接口）：
  - SCL：`PB8`（软件I2C）
  - SDA：`PB9`（软件I2C）
  - VCC：3.3V
  - GND：GND

## 📦 模块依赖

### 必需模块

- `soft_i2c`：软件I2C驱动模块（DS3231使用软件I2C）
- `ds3231`：DS3231实时时钟驱动模块（核心）
- `gpio`：GPIO驱动模块（软件I2C依赖）
- `led`：LED驱动模块（状态指示）
- `oled`：OLED显示模块（默认显示器）
- `delay`：延时模块
- `error_handler`：错误处理模块
- `log`：日志模块（调试输出）

## 🚀 使用步骤

### 步骤1：硬件连接

1. 将DS3231模块连接到STM32：
   - DS3231 SCL → STM32 PB10
   - DS3231 SDA → STM32 PB11
   - DS3231 VCC → 3.3V
   - DS3231 GND → GND

2. **重要**：在SCL和SDA线上添加上拉电阻（4.7kΩ-10kΩ）到3.3V

3. 将OLED显示屏连接到STM32：
   - OLED SCL → STM32 PB8
   - OLED SDA → STM32 PB9
   - OLED VCC → 3.3V
   - OLED GND → GND

### 步骤2：配置模块开关

在 `config.h` 中启用必要模块：

```c
#define CONFIG_MODULE_SOFT_I2C_ENABLED     1   /* 启用软件I2C模块 */
#define CONFIG_MODULE_DS3231_ENABLED        1   /* 启用DS3231模块 */
#define CONFIG_MODULE_OLED_ENABLED         1   /* 启用OLED模块 */
```

### 步骤3：配置硬件

在 `board.h` 中配置软件I2C：

```c
#define SOFT_I2C_CONFIGS {                                                                    \
    {GPIOB, GPIO_Pin_10, GPIOB, GPIO_Pin_11, 5, 1}, /* SoftI2C1：PB10(SCL), PB11(SDA)，5us延时，启用（DS3231） */ \
}
```

### 步骤4：初始化流程

```c
/* 1. 系统初始化 */
System_Init();

/* 2. 软件I2C初始化（SoftI2C2: PB10/11） */
I2C_SW_Init(SOFT_I2C_INSTANCE_2);

/* 3. DS3231初始化（软件I2C接口） */
DS3231_Config_t ds3231_config;
ds3231_config.interface_type = DS3231_INTERFACE_SOFTWARE;
ds3231_config.config.software.soft_i2c_instance = SOFT_I2C_INSTANCE_2;  /* 使用SoftI2C2实例（PB10/11） */
DS3231_Init(&ds3231_config);
```

## 📝 代码说明

### 主要区别（与案例11对比）

1. **I2C接口类型**：
   - 案例11：硬件I2C（I2C2外设）
   - 案例12：软件I2C（GPIO模拟）

2. **初始化方式**：
   - 案例11：`I2C_HW_Init(I2C_INSTANCE_2)`
   - 案例12：`I2C_SW_Init(SOFT_I2C_INSTANCE_2)`

3. **DS3231配置**：
   - 案例11：`DS3231_INTERFACE_HARDWARE` + `i2c_instance`
   - 案例12：`DS3231_INTERFACE_SOFTWARE` + GPIO引脚配置

### 功能演示

案例12演示了以下功能（与案例11相同）：

1. **时间读取和设置**：读取当前时间并设置新时间
2. **温度读取**：读取DS3231内部温度传感器
3. **闹钟配置**：配置Alarm 1，在每分钟的第30秒触发
4. **方波输出**：配置1Hz方波输出
5. **32kHz输出控制**：禁用32kHz输出以节省功耗

## ⚠️ 注意事项

1. **上拉电阻**：DS3231的SCL和SDA线必须连接上拉电阻（4.7kΩ-10kΩ）到3.3V
2. **引脚冲突**：确保DS3231（PB10/11）和OLED（PB8/9）使用不同的引脚
3. **时序延时**：软件I2C的延时参数（`delay_us`）需要根据系统时钟调整，标准模式建议5-10us
4. **软件I2C限制**：软件I2C速度较慢，不适合高速通信，但对于DS3231（标准模式100kHz）完全足够

## 🔍 与案例11的对比

| 特性 | 案例11（硬件I2C） | 案例12（软件I2C） |
|------|------------------|------------------|
| I2C接口 | 硬件I2C2外设 | 软件I2C（GPIO模拟） |
| 引脚 | PB10/11（固定） | PB10/11（可配置） |
| 速度 | 硬件控制，可达400kHz | 软件控制，通常<100kHz |
| 资源占用 | 占用I2C外设 | 仅占用GPIO引脚 |
| 适用场景 | 高速通信、多I2C设备 | 引脚灵活、无硬件I2C外设 |

## 📚 参考

- 案例11：DS3231使用示例（硬件I2C接口）
- DS3231模块驱动文档：`Drivers/sensors/ds3231.h`
- 软件I2C模块驱动文档：`Drivers/i2c/i2c_sw.h`

