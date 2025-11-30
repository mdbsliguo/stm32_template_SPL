# Flash10 - LittleFS文件系统W25Q读写测试案例

## 概述

Flash10_LittleFS_W25Q_ReadWrite 是LittleFS文件系统在W25Q SPI Flash上的读写测试案例，演示了LittleFS文件系统的基本功能，包括文件创建、读写、目录操作等。

## 硬件要求

- **LED1**：PA1（系统状态指示）
- **W25Q SPI Flash模块**（SPI2接口）
  - CS：PA11
  - SCK：PB13（SPI2_SCK）
  - MISO：PB14（SPI2_MISO）
  - MOSI：PB15（SPI2_MOSI）
  - VCC：3.3V
  - GND：GND
- **OLED显示屏**（软件I2C接口）
  - SCL：PB8（软件I2C）
  - SDA：PB9（软件I2C）
- **UART1**（用于详细日志输出）
  - TX：PA9
  - RX：PA10

## 功能演示

1. **系统初始化**：System_Init()
2. **UART初始化**：UART1，115200波特率
3. **Debug模块初始化**：UART模式
4. **Log模块初始化**：INFO级别
5. **LED初始化**：LED1（PA1）
6. **软件I2C初始化**：PB8(SCL), PB9(SDA)
7. **OLED初始化**：显示初始化状态
8. **SPI初始化**：SPI2，PB13/14/15，PA11(CS)
9. **W25Q初始化**：识别设备并显示信息
10. **LittleFS初始化**：初始化文件系统
11. **LittleFS挂载**：挂载文件系统到W25Q Flash
12. **文件操作测试**：创建文件、写入数据、读取验证
13. **目录操作测试**：创建目录、列出目录内容
14. **主循环**：LED闪烁，OLED显示运行状态

## 模块依赖

### 必需模块
- GPIO模块
- LED模块
- OLED模块
- SPI模块
- W25Q模块
- UART模块
- 软件I2C模块
- Delay模块
- TIM2_TimeBase模块
- System_Init模块
- ErrorHandler模块
- Log模块

### 已启用
- LittleFS模块（文件系统操作）

## 编译说明

1. 使用Keil uVision打开 `Examples.uvprojx`
2. 编译项目（应该0错误，0警告）
3. 下载到STM32F103C8T6
4. 观察LED闪烁和OLED显示

## 预期行为

1. **LED**：每秒闪烁一次
2. **OLED**：
   - 显示文件系统信息（总空间、空闲空间）
   - 显示文件操作测试结果
   - 显示目录列表
   - 显示运行状态
3. **串口**：输出详细的初始化日志、文件系统操作日志和运行状态

## 已实现功能

- ✅ LittleFS文件系统初始化、挂载、格式化
- ✅ 文件创建、写入、读取、验证
- ✅ 目录创建、列表读取
- ✅ 文件系统信息获取（总空间、空闲空间）

## 后续计划（可选）

- 文件定位（Seek）
- 文件截断（Truncate）
- 文件重命名（Rename）
- 文件删除（Delete）
- 文件属性操作（SetAttr/GetAttr）
- 目录删除

## 注意事项

1. 确保硬件连接正确
2. 确保W25Q Flash模块已正确连接
3. 串口波特率：115200
4. 如果OLED不显示，检查I2C连接（PB8/9）
5. 如果串口无输出，检查UART连接（PA9/10）
