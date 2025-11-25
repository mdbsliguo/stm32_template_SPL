# ADC01 - 单通道电压采集示例

## 📋 案例目的

- **核心目标**：演示如何使用ADC驱动模块进行单通道电压采集，包括ADC初始化、单次转换读取、电压值计算和显示
- **学习重点**：
  - 理解ADC驱动模块的基本使用方法
  - 掌握ADC初始化和单次转换读取流程
  - 学习ADC原始值到电压值的转换计算
  - 了解OLED显示与ADC数据采集的结合使用
  - 学习系统初始化和错误处理的使用
- **应用场景**：适用于需要采集模拟电压信号的应用，如传感器数据采集、电压监测、信号调理等

---

## 🔧 硬件要求

### 必需硬件
- STM32F103C8T6开发板
- ADC输入信号源（可调电位器、传感器输出等）
- OLED显示屏（SSD1306，I2C接口，用于显示ADC值和电压值）
  - SCL连接到PB8
  - SDA连接到PB9

### 硬件连接

**ADC输入**：
- ADC输入连接到PA0（ADC_Channel_0）
- 输入电压范围：0V - 3.3V（不得超过3.3V，否则可能损坏芯片）

**方法1：使用可调电位器（推荐）**
```
3.3V ──[电位器上端]
GND  ──[电位器下端]
PA0  ──[电位器中间抽头]
```
旋转电位器可调节电压在0V-3.3V之间变化。

**方法2：使用电池（简单测试）**
```
电池正极 ─── PA0
电池负极 ─── GND
```
- **1.5V电池**（干电池/纽扣电池）：可直接连接，预期读数约1.5V
- **3V电池**（两节1.5V串联）：可直接连接，预期读数约3.0V
- **⚠️ 注意**：9V或更高电压电池需要使用分压电路，否则会损坏芯片

**方法3：使用分压电路（固定电压测试）**
```
GND ──[1kΩ]──┬── PA0
              │
           [1kΩ]
              │
            3.3V
```
- 两个相同阻值电阻串联，PA0接中间点
- 预期读数：约1.65V（3.3V的一半）
- 优点：电压稳定，不会短路，安全可靠
- 注意：两个电阻阻值相同时分压为1/2，不同阻值可得到不同电压

**方法4：直接短接测试**
```
PA0 ─── GND   (预期读数：0.000V)
PA0 ─── 3.3V  (预期读数：3.300V)
```

**⚠️ 重要说明**：
- **电压范围限制**：ADC输入电压必须在0V-3.3V范围内，超过3.3V可能损坏芯片
- **输入阻抗**：ADC输入阻抗较高，适合连接高阻抗信号源
- **采样时间**：采样时间越长，转换精度越高，但转换速度越慢

**OLED显示屏（用于显示ADC值和电压值）**：
- SCL连接到PB8
- SDA连接到PB9

### 硬件配置

**⚠️ 重要说明**：案例是独立工程，硬件配置在案例目录下的 `board.h` 中。
如果硬件引脚不同，直接修改 `Examples/ADC/ADC01_SingleChannelVoltage/board.h` 中的配置即可。

**ADC配置**（单通道）：
```c
/* ADC统一配置表 */
#define ADC_CONFIGS { \
    {ADC1, {ADC_Channel_0}, 1, ADC_SampleTime_55Cycles5, 1},  /* ADC1：PA0，单通道，55.5周期采样，启用 */ \
}
```

**配置说明**：
- `adc_periph`: `ADC1`（ADC外设）
- `channels`: `{ADC_Channel_0}`（通道数组，单通道）
- `channel_count`: `1`（通道数量）
- `sample_time`: `ADC_SampleTime_55Cycles5`（采样时间，55.5周期，平衡精度和速度）
- `enabled`: `1`（启用标志）

**OLED配置**（已包含在board.h中）：
```c
/* OLED I2C配置 */
#define OLED_I2C_CONFIG {      \
    GPIOB,                     \
    GPIO_Pin_8, /* SCL: PB8 */ \
    GPIOB,                     \
    GPIO_Pin_9, /* SDA: PB9 */ \
}
```

**UART配置**（已包含在board.h中，新项目必须）：
```c
/* UART统一配置表 - 标准配置：USART1，PA9/PA10，115200，8N1 */
#define UART_CONFIGS { \
    {USART1, GPIOA, GPIO_Pin_9, GPIOA, GPIO_Pin_10, 115200, USART_WordLength_8b, USART_StopBits_1, USART_Parity_No, 1}, \
}
```

**注意**：
- 根据实际硬件修改ADC通道（`ADC_Channel_0`等）
- 根据实际硬件修改OLED引脚（`GPIO_Pin_8`、`GPIO_Pin_9`等）
- 根据实际需求调整采样时间（`ADC_SampleTime_55Cycles5`等）
- **新项目必须配置UART**：所有新项目（除案例1外）必须包含UART_CONFIGS配置（用于串口调试和日志输出）

---

## 📦 模块依赖

本案例使用以下模块：

- `adc`：ADC驱动模块（核心功能，单通道电压采集）
- `gpio`：GPIO驱动模块（ADC、UART、OLED依赖）
- `uart`：UART驱动模块（串口调试，新项目必须）
- `debug`：Debug模块（printf重定向，新项目必须）
- `log`：日志模块（分级日志系统，新项目必须）
- `error_handler`：错误处理模块（统一错误处理，新项目必须）
- `oled_ssd1306`：OLED显示驱动模块（状态显示）
- `i2c_sw`：软件I2C驱动模块（OLED使用）
- `delay`：延时模块（用于实现时间间隔）
- `system_init`：系统初始化模块

---

## 🚀 使用方法

### 快速开始
1. **打开案例工程**：双击 `Examples/ADC/ADC01_SingleChannelVoltage/Examples.uvprojx` 打开Keil工程
2. **检查硬件配置**：确认案例目录下的 `board.h` 中ADC配置正确（PA0）
3. **连接硬件**：将ADC输入信号连接到PA0（0V-3.3V范围）
4. **编译下载**：在Keil中编译（F7）并下载到开发板（F8）
5. **观察效果**：
   - OLED显示ADC原始值和电压值
   - 串口输出详细日志
   - 每500ms采集一次ADC值

### 详细操作流程

**通用操作步骤请参考**：[Examples/README.md](../README.md#-通用操作流程)

**注意**：本案例是独立工程，无需复制文件，直接打开工程文件即可编译运行。

---

## 🔄 实现流程

### 整体逻辑

本案例通过标准初始化流程和主循环，全面演示ADC单通道电压采集的完整流程：

1. **系统初始化阶段**
   - 系统初始化（System_Init）
   - UART初始化（串口调试）
   - Debug模块初始化（printf重定向）
   - Log模块初始化（分级日志）
   - ErrorHandler自动初始化（错误处理）

2. **ADC初始化阶段**
   - ADC初始化（ADC_Init）
   - 配置ADC通道和采样时间
   - ADC校准

3. **显示初始化阶段**
   - 软件I2C初始化（OLED需要）
   - OLED初始化（状态显示）

4. **主循环阶段**
   - 每500ms采集一次ADC值
   - 计算电压值（12位ADC，0-4095对应0V-3.3V）
   - OLED显示ADC原始值和电压值
   - 串口输出详细日志

### 关键方法

- **标准初始化流程**：按照System_Init → UART → Debug → Log → ADC → OLED的顺序初始化
- **错误处理集成**：通过ErrorHandler模块统一处理错误，并输出错误日志
- **分级日志输出**：通过Log模块实现不同级别的日志输出，便于调试和监控
- **串口与OLED输出分工**：串口输出详细日志（中文），OLED输出关键数据（英文）
- **电压值计算**：使用公式 `电压(V) = (ADC值 / 4095) * 3.3V` 计算实际电压

### 工作流程示意

```
系统初始化（System_Init）
    ↓
UART初始化（UART_Init）
    ↓
Debug模块初始化（Debug_Init，UART模式）
    ↓
Log模块初始化（Log_Init）
    ↓
ErrorHandler模块自动初始化
    ↓
ADC初始化（ADC_Init）
    ↓
软件I2C初始化（I2C_SW_Init）
    ↓
OLED初始化（OLED_Init）
    ↓
主循环：每500ms采集一次ADC值
    ├─ ADC_ReadChannel（读取ADC原始值）
    ├─ 计算电压值
    ├─ OLED显示（ADC值和电压值）
    └─ 串口输出日志
```

---

## 📚 关键函数说明

### ADC相关函数

- **`ADC_Init()`**：初始化ADC驱动模块
  - 在本案例中用于初始化ADC1，配置PA0为ADC输入
  - 根据配置表自动初始化所有enabled=1的ADC实例
  - 返回ADC_Status_t错误码，需要检查返回值
  - 初始化失败必须停止程序（ADC是核心功能）

- **`ADC_ReadChannel()`**：单通道单次转换读取
  - 在本案例中用于读取PA0（ADC_Channel_0）的ADC值
  - 参数：ADC实例、通道号、输出缓冲区、超时时间
  - 返回ADC_Status_t错误码，需要检查返回值
  - 返回12位ADC值（0-4095）

### UART相关函数

- **`UART_Init()`**：初始化UART外设
  - 在本案例中用于初始化USART1，配置为115200波特率、8N1格式
  - 参数：UART实例索引（UART_INSTANCE_1）
  - 返回UART_Status_t错误码，初始化失败必须停止程序

### Debug模块相关函数

- **`Debug_Init()`**：初始化Debug模块（UART模式）
  - 在本案例中用于初始化Debug模块，配置为UART输出模式
  - 参数：输出模式（DEBUG_MODE_UART）、波特率
  - 返回int类型，0表示成功，非0表示失败，初始化失败必须停止程序

### Log模块相关函数

- **`Log_Init()`**：初始化日志系统
  - 在本案例中用于初始化日志系统，配置日志级别和功能开关
  - 参数：日志配置结构体指针
  - 返回Log_Status_t错误码，初始化失败可以继续运行

- **`LOG_DEBUG()`** / **`LOG_INFO()`** / **`LOG_WARN()`** / **`LOG_ERROR()`**：分级日志宏
  - 在本案例中用于输出不同级别的日志
  - 参数：模块名称、格式字符串、参数列表
  - 串口输出详细日志（支持中文）

### 错误处理相关函数

- **`ErrorHandler_Handle()`**：处理错误
  - 在本案例中用于处理各种错误，并输出错误日志
  - 参数：错误码、模块名称
  - ErrorHandler模块在编译时自动初始化，无需显式调用

### 软件I2C相关函数

- **`I2C_SW_Init()`**：初始化软件I2C驱动模块
  - 在本案例中用于初始化软件I2C，为OLED提供I2C通信接口
  - 参数：软件I2C实例索引（SOFT_I2C_INSTANCE_1）
  - 返回SoftI2C_Status_t错误码，需要检查返回值
  - 初始化失败时OLED无法使用，但ADC仍可工作

### OLED相关函数

- **`OLED_Init()`**：初始化OLED显示模块
  - 在本案例中用于初始化OLED显示
  - 返回OLED_Status_t错误码，需要检查返回值

- **`OLED_Clear()`**：清屏
  - 在本案例中用于清除OLED显示内容

- **`OLED_ShowString()`**：显示字符串
  - 在本案例中用于显示ADC值和电压值
  - 参数：行号、列号、字符串（全英文，ASCII字符）

### 延时相关函数

- **`Delay_ms()`**：毫秒级延时（阻塞式）
  - 在本案例中用于实现时间间隔（500ms采集一次）
  - 参数：延时时间（毫秒）
  - 阻塞式延时，会占用CPU时间

**详细函数实现和调用示例请参考**：`main_example.c` 中的代码

---

## 🎯 预期效果

- **OLED显示**：
  - 第1行：`ADC01 Demo`
  - 第2行：`Channel: PA0`
  - 第3行：`ADC: xxxx`（ADC原始值，0-4095）
  - 第4行：`Volt: x.xxxV`（电压值，0.000V-3.300V）
- **串口输出**：每500ms输出一次ADC值和电压值的详细日志
- **实时更新**：ADC值和电压值每500ms更新一次

---

## 🔍 扩展练习

1. **修改采样时间**：修改 `board.h` 中的 `sample_time`，观察精度和速度的变化
2. **多通道采集**：修改配置为多通道，实现多路电压采集
3. **连续转换模式**：使用 `ADC_StartContinuous()` 实现连续转换，提高采集频率
4. **电压范围扩展**：使用分压电路扩展输入电压范围（如0V-10V）
5. **数据滤波**：实现滑动平均滤波，提高ADC值的稳定性
6. **阈值报警**：设置电压阈值，超过阈值时触发报警

---

## ⚠️ 注意事项与重点

### ⚠️ 重要提示

1. **电压范围限制**：ADC输入电压必须在0V-3.3V范围内，超过3.3V可能损坏芯片
2. **初始化顺序**：必须严格按照 System_Init → UART → Debug → Log → ADC → OLED 的顺序初始化
3. **UART配置**：新项目必须包含UART_CONFIGS配置（用于串口调试和日志输出）
4. **错误处理**：所有模块初始化函数必须检查返回值，使用ErrorHandler统一处理错误

### 🔑 关键点

1. **标准初始化流程**：
   - 系统初始化（System_Init）
   - UART初始化（UART_Init）
   - Debug模块初始化（Debug_Init，UART模式）
   - Log模块初始化（Log_Init）
   - ErrorHandler自动初始化（无需显式调用）
   - ADC初始化（ADC_Init）
   - 其他模块初始化（按依赖顺序）

2. **错误处理策略**：
   - UART/Debug初始化失败：必须停止程序（进入死循环）
   - Log初始化失败：可以继续运行（使用UART直接输出）
   - ADC初始化失败：必须停止程序（ADC是核心功能）
   - OLED初始化失败：可以继续运行（ADC仍可工作）

3. **输出分工规范**：
   - **串口（UART）**：详细日志、调试信息、错误详情（支持中文，GB2312编码）
   - **OLED**：关键状态、实时数据、简要提示（全英文，ASCII字符）
   - **双边输出**：系统启动信息、关键错误、重要状态变化

4. **ADC使用要点**：
   - ADC值为12位（0-4095），对应0V-3.3V
   - 采样时间越长，精度越高，但转换速度越慢
   - 单次转换模式适合低频采集，连续转换模式适合高频采集
   - 内部通道（16、17）不需要GPIO配置

5. **电压计算公式**：
   ```
   电压(V) = (ADC值 / 4095) * 参考电压(3.3V)
   ```

### 💡 调试技巧

1. **没有日志输出**：
   - 检查UART是否正确初始化
   - 检查Debug模块是否正确初始化
   - 检查串口助手配置是否正确（115200, 8N1）
   - 检查硬件连接是否正确（PA9/PA10）

2. **ADC值始终为0或4095**：
   - 检查ADC输入连接是否正确（PA0）
   - 检查输入电压是否在0V-3.3V范围内
   - 检查ADC初始化是否成功
   - 检查ADC通道配置是否正确

3. **ADC值不稳定**：
   - 增加采样时间（提高精度）
   - 检查输入信号是否稳定
   - 实现软件滤波（滑动平均等）
   - 检查电源是否稳定

4. **OLED不显示**：
   - 检查OLED连接是否正确（SCL: PB8, SDA: PB9）
   - 检查软件I2C模块是否已启用
   - 检查OLED模块是否已启用

---

## ⚠️ 常见问题

### ADC值始终为0或4095
- **检查输入连接**：确认ADC输入连接到PA0
- **检查电压范围**：确认输入电压在0V-3.3V范围内
- **检查ADC初始化**：确认ADC初始化成功（检查返回值）
- **检查通道配置**：确认ADC通道配置正确（ADC_Channel_0）

### ADC值不稳定
- **增加采样时间**：修改 `board.h` 中的 `sample_time`，使用更长的采样时间
- **检查输入信号**：确认输入信号稳定，无干扰
- **实现软件滤波**：在主循环中实现滑动平均滤波
- **检查电源**：确认电源稳定，无纹波

### OLED不显示
- 检查OLED连接是否正确（SCL: PB8, SDA: PB9）
- 检查软件I2C模块是否已启用（`CONFIG_MODULE_SOFT_I2C_ENABLED = 1`）
- 检查OLED模块是否已启用（`CONFIG_MODULE_OLED_ENABLED = 1`）

### 编译错误
- 确保已包含必要的头文件
- 确保 `System_Init()` 和 `ADC_Init()` 已正确调用
- 确保ADC模块已启用（`CONFIG_MODULE_ADC_ENABLED = 1`）
- 确保OLED模块已启用（`CONFIG_MODULE_OLED_ENABLED = 1`）
- 确保软件I2C模块已启用（`CONFIG_MODULE_SOFT_I2C_ENABLED = 1`）
- 确保UART模块已启用（`CONFIG_MODULE_UART_ENABLED = 1`）
- 确保Log模块已启用（`CONFIG_MODULE_LOG_ENABLED = 1`）
- 确保ErrorHandler模块已启用（`CONFIG_MODULE_ERROR_HANDLER_ENABLED = 1`）

---

## 🔗 相关文档

- **主程序代码**：`Examples/ADC/ADC01_SingleChannelVoltage/main_example.c`
- **硬件配置**：`Examples/ADC/ADC01_SingleChannelVoltage/board.h`
- **模块配置**：`Examples/ADC/ADC01_SingleChannelVoltage/config.h`
- **ADC驱动模块文档**：`Drivers/analog/adc.c/h`
- **GPIO驱动模块文档**：`Drivers/basic/gpio.c/h`
- **UART驱动模块文档**：`Drivers/uart/uart.c/h`
- **Log模块文档**：`Debug/log.c/h`
- **ErrorHandler模块文档**：`Common/error_handler.c/h`
- **OLED驱动模块文档**：`Drivers/display/oled_ssd1306.c/h`
- **项目规范文档**：`PROJECT_KEYWORDS.md`
- **案例参考**：`Examples/README.md`

---

## 📚 相关模块

- **ADC驱动**：`Drivers/analog/adc.c/h`
- **GPIO驱动**：`Drivers/basic/gpio.c/h`
- **UART驱动**：`Drivers/uart/uart.c/h`
- **Debug模块**：`Debug/debug.c/h`
- **Log模块**：`Debug/log.c/h`
- **ErrorHandler模块**：`Common/error_handler.c/h`
- **OLED驱动**：`Drivers/display/oled_ssd1306.c/h`
- **OLED字库**：`Drivers/display/oled_font_ascii8x16.c/h`
- **软件I2C驱动**：`Drivers/i2c/i2c_sw.c/h`
- **延时功能**：`System/delay.c/h`
- **系统初始化**：`System/system_init.c/h`
- **硬件配置**：案例目录下的 `board.h`

---

**最后更新**：2024-01-01
