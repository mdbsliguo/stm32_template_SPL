# 传感器/外设芯片模块

## DS3231实时时钟模块 (ds3231)

### 功能列表

| 功能分类 | 函数 | 说明 |
|---------|------|------|
| **初始化** | `DS3231_Init()` | 初始化DS3231（支持硬件/软件I2C） |
| **初始化** | `DS3231_Deinit()` | 反初始化DS3231 |
| **初始化** | `DS3231_IsInitialized()` | 检查是否已初始化 |
| **时间管理** | `DS3231_ReadTime()` | 读取当前时间 |
| **时间管理** | `DS3231_SetTime()` | 设置时间 |
| **温度读取** | `DS3231_ReadTemperature()` | 读取温度（整数，0.25°C精度） |
| **温度读取** | `DS3231_ReadTemperatureFloat()` | 读取温度（浮点数） |
| **振荡器控制** | `DS3231_CheckOSF()` | 检查振荡器停止标志 |
| **振荡器控制** | `DS3231_ClearOSF()` | 清除振荡器停止标志 |
| **振荡器控制** | `DS3231_IsRunning()` | 检查是否正在运行 |
| **振荡器控制** | `DS3231_Start()` / `DS3231_Stop()` | 启动/停止RTC |
| **闹钟功能** | `DS3231_SetAlarm1()` / `DS3231_SetAlarm2()` | 设置闹钟1/2 |
| **闹钟功能** | `DS3231_ReadAlarm1()` / `DS3231_ReadAlarm2()` | 读取闹钟1/2 |
| **闹钟功能** | `DS3231_CheckAlarm1Flag()` / `DS3231_CheckAlarm2Flag()` | 检查闹钟标志 |
| **闹钟功能** | `DS3231_ClearAlarm1Flag()` / `DS3231_ClearAlarm2Flag()` | 清除闹钟标志 |
| **闹钟功能** | `DS3231_EnableAlarm1()` / `DS3231_DisableAlarm1()` | 使能/禁用闹钟1 |
| **闹钟功能** | `DS3231_EnableAlarm2()` / `DS3231_DisableAlarm2()` | 使能/禁用闹钟2 |
| **方波输出** | `DS3231_SetSquareWave()` | 设置方波输出频率和使能 |
| **方波输出** | `DS3231_DisableSquareWave()` | 禁用方波输出 |
| **方波输出** | `DS3231_EnableBatteryBackupSQW()` | 使能电池备份方波输出 |
| **32kHz输出** | `DS3231_Enable32kHz()` / `DS3231_Disable32kHz()` | 使能/禁用32kHz输出 |
| **32kHz输出** | `DS3231_Is32kHzEnabled()` | 检查32kHz输出状态 |
| **温度转换** | `DS3231_TriggerTemperatureConversion()` | 触发温度转换 |
| **温度转换** | `DS3231_IsTemperatureBusy()` | 检查温度转换忙标志 |
| **中断控制** | `DS3231_SetInterruptMode()` / `DS3231_GetInterruptMode()` | 设置/获取中断模式 |
| **老化偏移** | `DS3231_ReadAgingOffset()` / `DS3231_SetAgingOffset()` | 读取/设置老化偏移 |
| **寄存器访问** | `DS3231_ReadControlRegister()` / `DS3231_WriteControlRegister()` | 读取/写入控制寄存器 |
| **寄存器访问** | `DS3231_ReadStatusRegister()` / `DS3231_WriteStatusRegister()` | 读取/写入状态寄存器 |

### 重要提醒

1. **I2C接口选择**：支持硬件I2C和软件I2C两种接口，通过配置结构体选择
2. **接口依赖**：使用硬件I2C前必须先包含`i2c_hw.h`，使用软件I2C前必须先包含`i2c_sw.h`
3. **初始化顺序**：必须先初始化对应的I2C模块（硬件I2C或软件I2C），再初始化DS3231
4. **I2C地址**：DS3231的I2C地址为0x68（7位地址）
5. **时间格式**：24小时制，年份范围2000-2099
6. **温度精度**：温度读取精度0.25°C，转换需要约1秒
7. **振荡器停止**：OSF标志为1表示RTC可能丢失时间，需要重新设置时间
8. **闹钟模式**：支持多种匹配模式（每秒、秒匹配、分秒匹配等）
9. **方波频率**：支持1Hz、1.024kHz、4.096kHz、8.192kHz四种频率
10. **中断模式**：支持方波输出模式和闹钟中断模式

