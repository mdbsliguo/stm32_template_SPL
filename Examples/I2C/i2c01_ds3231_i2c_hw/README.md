# 案例11 - DS3231实时时钟模块演示（硬件I2C接口）

✅ **已调试完成**

## 📋 功能说明

本案例演示如何使用**DS3231实时时钟模块**，展示DS3231的常规配置流程和常用功能。

### 核心功能

1. **DS3231初始化（硬件I2C接口）**
   - 演示硬件I2C接口的初始化步骤
   - 展示I2C2的配置和使用

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
  - SCL：`PB10`（I2C2）
  - SDA：`PB11`（I2C2）
  - VCC：3.3V
  - GND：GND
  - **注意**：DS3231模块需要外部上拉电阻（通常4.7kΩ-10kΩ）连接到SCL和SDA

## 📦 模块依赖

### 必需模块

- `i2c`：硬件I2C驱动模块（DS3231使用I2C2）
- `ds3231`：DS3231实时时钟驱动模块（核心）
- `gpio`：GPIO驱动模块（I2C依赖）
- `led`：LED驱动模块（状态指示）
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

### 步骤2：配置模块开关

在 `config.h` 中启用必要模块：

```c
#define CONFIG_MODULE_I2C_ENABLED           1   /* 启用I2C模块 */
#define CONFIG_MODULE_DS3231_ENABLED        1   /* 启用DS3231模块 */
```

### 步骤3：配置硬件

在 `board.h` 中配置I2C2：

```c
#define I2C_CONFIGS {                                                                    \
    {I2C2, GPIOB, GPIO_Pin_10, GPIOB, GPIO_Pin_11, 100000, 0x00, 1}, /* I2C2：PB10(SCL), PB11(SDA)，100kHz */ \
}
```

### 步骤4：初始化流程

```c
/* 1. 系统初始化 */
System_Init();

/* 2. I2C初始化（I2C2） */
I2C_HW_Init(I2C_INSTANCE_2);

/* 3. DS3231初始化（硬件I2C接口） */
DS3231_Config_t ds3231_config;
ds3231_config.interface_type = DS3231_INTERFACE_HARDWARE;
ds3231_config.config.hardware.i2c_instance = I2C_INSTANCE_2;
DS3231_Init(&ds3231_config);
```

## 📊 DS3231常规配置流程

### 1. 检查振荡器停止标志（OSF）

```c
uint8_t osf_flag;
DS3231_CheckOSF(&osf_flag);
if (osf_flag)
{
    /* OSF标志被设置，时间可能不准确 */
    DS3231_ClearOSF();  /* 清除OSF标志 */
}
```

### 2. 启动振荡器

```c
DS3231_Start();  /* 确保RTC运行 */
```

### 3. 配置方波输出

```c
/* 设置1Hz方波输出并使能 */
DS3231_SetSquareWave(DS3231_SQW_1HZ, 1);

/* 或禁用方波输出 */
DS3231_DisableSquareWave();
```

### 4. 配置32kHz输出

```c
/* 禁用32kHz输出（节省功耗） */
DS3231_Disable32kHz();

/* 或使能32kHz输出 */
DS3231_Enable32kHz();
```

### 5. 配置中断模式

```c
/* 配置为方波输出模式 */
DS3231_SetInterruptMode(DS3231_INT_MODE_SQUARE_WAVE);

/* 或配置为闹钟中断模式 */
DS3231_SetInterruptMode(DS3231_INT_MODE_ALARM);
```

## 📝 DS3231常用功能示例

### 示例1：读取和设置时间

```c
DS3231_Time_t time;

/* 读取当前时间 */
DS3231_ReadTime(&time);
printf("Time: %04d-%02d-%02d %02d:%02d:%02d\r\n",
       time.year, time.month, time.day,
       time.hour, time.minute, time.second);

/* 设置时间 */
time.year = 2024;
time.month = 1;
time.day = 1;
time.weekday = 1;  /* Monday */
time.hour = 12;
time.minute = 0;
time.second = 0;
DS3231_SetTime(&time);
```

### 示例2：读取温度

```c
int16_t temperature;

/* 读取温度（单位：0.01°C，例如2500表示25.00°C） */
DS3231_ReadTemperature(&temperature);

/* 转换为浮点数 */
float temp_float;
DS3231_ReadTemperatureFloat(&temp_float);
```

### 示例3：配置闹钟1

```c
DS3231_Alarm_t alarm1;

/* 配置为每分钟的第30秒触发 */
alarm1.mode = DS3231_ALARM_MODE_SECOND_MATCH;
alarm1.second = 30;
alarm1.minute = 0x80;  /* 匹配模式 */
alarm1.hour = 0x80;    /* 匹配模式 */
alarm1.day_or_weekday = 0x80;  /* 匹配模式 */

DS3231_SetAlarm1(&alarm1);
DS3231_EnableAlarm1();  /* 使能闹钟中断 */
```

### 示例4：检查闹钟标志

```c
uint8_t alarm_flag;

/* 检查闹钟1标志 */
DS3231_CheckAlarm1Flag(&alarm_flag);
if (alarm_flag)
{
    printf("Alarm 1 triggered!\r\n");
    DS3231_ClearAlarm1Flag();  /* 清除标志 */
}
```

### 示例5：方波输出频率选择

```c
/* 1Hz方波 */
DS3231_SetSquareWave(DS3231_SQW_1HZ, 1);

/* 1.024kHz方波 */
DS3231_SetSquareWave(DS3231_SQW_1024HZ, 1);

/* 4.096kHz方波 */
DS3231_SetSquareWave(DS3231_SQW_4096HZ, 1);

/* 8.192kHz方波 */
DS3231_SetSquareWave(DS3231_SQW_8192HZ, 1);
```

## ⚠️ 注意事项

### 1. I2C上拉电阻

**必须**在SCL和SDA线上添加上拉电阻（4.7kΩ-10kΩ）到3.3V，否则I2C通信会失败。

### 2. 初始化顺序

1. 先初始化I2C模块：`I2C_HW_Init(I2C_INSTANCE_2)`
2. 再初始化DS3231：`DS3231_Init(&config)`

### 3. OSF标志

- 如果检测到OSF标志，说明振荡器曾经停止过，时间可能不准确
- 首次使用或电池耗尽后，需要清除OSF标志并重新设置时间

### 4. 时间格式

- DS3231使用24小时制
- 星期：1=Sunday, 2=Monday, ..., 7=Saturday
- 年份范围：2000-2099

### 5. 闹钟匹配模式

- `DS3231_ALARM_MODE_ONCE_PER_SECOND`：每秒触发
- `DS3231_ALARM_MODE_SECOND_MATCH`：秒匹配
- `DS3231_ALARM_MODE_MIN_SEC_MATCH`：分秒匹配
- `DS3231_ALARM_MODE_HOUR_MIN_SEC_MATCH`：时分秒匹配
- `DS3231_ALARM_MODE_DAY_HOUR_MIN_SEC_MATCH`：日期时分秒匹配
- `DS3231_ALARM_MODE_WEEKDAY_HOUR_MIN_SEC_MATCH`：星期时分秒匹配

### 6. 方波输出

- 方波从INT/SQW引脚输出
- 需要配置中断模式为`DS3231_INT_MODE_SQUARE_WAVE`才能输出方波
- 如果配置为`DS3231_INT_MODE_ALARM`，方波输出会被禁用

## 🔍 测试方法

### 测试1：基本通信测试

1. 编译并下载程序
2. 观察串口输出，应该能看到DS3231初始化成功的消息
3. 如果初始化失败，检查：
   - I2C上拉电阻是否正确连接
   - 硬件连接是否正确
   - I2C地址是否正确（DS3231默认地址：0x68）

### 测试2：时间读取测试

1. 程序会自动读取并显示当前时间
2. 观察串口输出的时间格式是否正确
3. 验证时间是否在合理范围内

### 测试3：时间设置测试

1. 程序会设置时间为2024-01-01 12:00:00
2. 观察串口输出，验证设置是否成功
3. 等待几秒后再次读取，验证时间是否在递增

### 测试4：温度读取测试

1. 程序会自动读取温度
2. 观察串口输出的温度值
3. 温度应该在合理范围内（通常20-40°C）

### 测试5：闹钟测试

1. 程序会配置闹钟1为每分钟的第30秒触发
2. 等待到第30秒，观察是否触发
3. 验证闹钟标志是否正确设置和清除

### 测试6：方波输出测试

1. 使用示波器或逻辑分析仪连接到INT/SQW引脚
2. 配置1Hz方波输出
3. 观察是否输出1Hz方波信号

## 📝 代码结构

```
example11_ds3231_demo/
├── main_example.c      # 主程序文件
├── board.h             # 硬件配置（I2C2配置）
├── config.h            # 模块开关配置
└── README.md           # 本文档
```

## 🔗 相关文档

- [DS3231驱动模块文档](../../Drivers/sensors/ds3231.h)
- [I2C驱动模块文档](../../Drivers/i2c/i2c_hw.h)
- [配置管理文档](../../system/config.h)

## 📌 总结

DS3231实时时钟模块提供了完整的RTC功能，包括：

1. ✅ **时间管理**：精确的时间读取和设置
2. ✅ **温度监控**：内置温度传感器
3. ✅ **闹钟功能**：两个可编程闹钟
4. ✅ **方波输出**：可配置频率的方波信号
5. ✅ **低功耗**：支持电池备份

通过本案例，您可以快速掌握DS3231的使用方法，并将其集成到您的项目中。

**串口输出示例**：
```
========================================
  案例11：DS3231实时时钟模块演示
  硬件I2C接口（I2C2，PB10/11）
========================================

初始化I2C2（PB10/11）...
I2C2初始化成功

初始化DS3231（硬件I2C接口）...
DS3231初始化成功

=== DS3231常规配置流程 ===
1. 检查振荡器停止标志（OSF）...
  OSF标志正常，时间准确
2. 启动振荡器...
  振荡器已启动
3. 配置方波输出（1Hz）...
  方波输出已配置为1Hz
...

Time: 2024-01-01 12:00:00 Mon
Temperature: 25.00 C
...
```

