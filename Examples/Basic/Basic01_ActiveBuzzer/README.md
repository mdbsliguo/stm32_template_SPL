# Basic01 - 有源蜂鸣器控制示例

## 📋 案例目的

- **核心目标**：演示如何使用Buzzer驱动模块进行GPIO模式的控制，包括简单开关控制、鸣响功能、报警音效
- **学习重点**：
  - 理解Buzzer驱动模块的基本使用方法
  - 掌握GPIO模式Buzzer的控制方法
  - 学习Buzzer快捷宏的使用
  - 了解OLED显示与Buzzer控制的结合使用
  - 学习系统初始化和延时功能的使用
- **应用场景**：适用于需要声音提示的应用，如报警系统、按键反馈、状态提示等

---

## 📋 功能说明

- **GPIO模式**：演示简单开关控制、鸣响功能、报警音效
- **OLED显示**：使用OLED显示当前操作状态和提示信息
- 演示Buzzer驱动的基本使用
- 演示系统初始化和延时功能
- 演示Buzzer快捷宏的使用

---

## 🔧 硬件要求

### 必需硬件
- STM32F103C8T6开发板
- Buzzer（**有源蜂鸣器**，必须使用有源蜂鸣器）
- OLED显示屏（SSD1306，I2C接口，用于显示提示信息）
  - SCL连接到PB8
  - SDA连接到PB9

### 硬件连接

**Buzzer（有源蜂鸣器）**：
- Buzzer正极连接到PA3
- Buzzer负极连接到GND

**⚠️ 重要说明**：
- **必须使用有源蜂鸣器**：有源蜂鸣器内部有振荡电路，通电即响，频率固定（通常2-4kHz）
- GPIO模式只能控制开关，无法改变频率和音调
- 如果使用无源蜂鸣器，请使用Basic02案例（PWM模式）

**OLED显示屏（用于显示提示信息）**：
- SCL连接到PB8
- SDA连接到PB9

### 硬件配置

**⚠️ 重要说明**：案例是独立工程，硬件配置在案例目录下的 `board.h` 中。
如果硬件引脚不同，直接修改 `Examples/Basic/Basic01_ActiveBuzzer/board.h` 中的配置即可。

**Buzzer配置**（GPIO模式）：
```c
/* Buzzer统一配置表 */
#define BUZZER_CONFIGS { \
    {BUZZER_MODE_GPIO, GPIOA, GPIO_Pin_3, 1, 0, Bit_RESET, 1},  /* GPIO模式，PA3，低电平有效，启用 */ \
}
```

**配置说明**：
- `mode`: `BUZZER_MODE_GPIO`（GPIO模式）
- `port`/`pin`: 指定端口和引脚（GPIOA, GPIO_Pin_3）
- `active_level`: `Bit_RESET`为低电平有效，`Bit_SET`为高电平有效
- `enabled`: 1表示启用该Buzzer，0表示禁用

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
- 根据实际硬件修改Buzzer引脚（`GPIO_Pin_3`等）
- 根据实际硬件修改OLED引脚（`GPIO_Pin_8`、`GPIO_Pin_9`等）
- 根据实际硬件修改有效电平（`Bit_RESET`/`Bit_SET`）
- **新项目必须配置UART**：所有新项目（除案例1外）必须包含UART_CONFIGS配置（用于串口调试和日志输出）

---

## 📦 模块依赖

本案例使用以下模块：

- `buzzer`：Buzzer驱动模块（核心功能，GPIO模式）
- `gpio`：GPIO驱动模块（Buzzer、UART依赖）
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
1. **打开案例工程**：双击 `Examples/Basic/Basic01_ActiveBuzzer/Examples.uvprojx` 打开Keil工程
2. **检查硬件配置**：确认案例目录下的 `board.h` 中Buzzer配置正确（PA3）
3. **编译下载**：在Keil中编译（F7）并下载到开发板（F8）
4. **观察效果**：
   - OLED显示操作状态
   - Buzzer开启500ms，关闭500ms
   - 三声短鸣响（每声100ms）
   - 报警音效（三短一长）

### 详细操作流程

**通用操作步骤请参考**：[Examples/README.md](../README.md#-通用操作流程)

**注意**：本案例是独立工程，无需复制文件，直接打开工程文件即可编译运行。

---

## 🔄 实现流程

### 整体逻辑

本案例通过3个示例阶段，全面演示Buzzer GPIO模式的各种功能。每个示例阶段展示不同的功能点：

1. **示例1：简单开关控制**
   - 演示Buzzer的开启和关闭
   - 展示基本的GPIO控制功能
   - OLED显示当前状态

2. **示例2：鸣响功能**
   - 演示单次鸣响（300ms）
   - 演示连续短鸣响（三声100ms）
   - OLED显示操作状态

3. **示例3：报警音效**
   - 演示报警音模式（三短一长）
   - 展示复杂节奏控制
   - OLED显示播放状态

### 关键方法

- **标准初始化流程**：按照System_Init → UART → Debug → Log → 其他模块的顺序初始化
- **错误处理集成**：通过ErrorHandler模块统一处理错误，并输出错误日志
- **分级日志输出**：通过Log模块实现不同级别的日志输出，便于调试和监控
- **串口与OLED输出分工**：串口输出详细日志（中文），OLED输出简要状态（英文）

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
Buzzer初始化（Buzzer_Init）
    ↓
软件I2C初始化（I2C_SW_Init）
    ↓
OLED初始化（OLED_Init）
    ↓
示例1：简单开关控制演示
    ↓
示例2：鸣响功能演示
    ↓
示例3：报警音效演示
    ↓
循环执行
```

---

## 📝 代码说明

### 关键代码

```c
// 系统初始化
System_Init();

// UART初始化
UART_Init(UART_INSTANCE_1);

// Debug模块初始化
Debug_Init(DEBUG_MODE_UART, 115200);

// Log模块初始化
Log_Init(&log_config);

// Buzzer初始化
Buzzer_Init();

// OLED初始化
OLED_Init();

// Buzzer控制（使用快捷宏）
BUZZER1_On();      // 开启Buzzer
Delay_ms(500);     // 延时500ms
BUZZER1_Off();     // 关闭Buzzer

// 鸣响功能
BUZZER1_Beep(300); // 鸣响300ms

// 报警音效（三短一长）
BUZZER1_Beep(100);  // 短
Delay_ms(100);
BUZZER1_Beep(100);  // 短
Delay_ms(100);
BUZZER1_Beep(100);  // 短
Delay_ms(100);
BUZZER1_Beep(500);  // 长
```

### 完整流程

1. **系统初始化**：`System_Init()` 初始化SysTick和延时模块
2. **UART初始化**：`UART_Init()` 初始化串口通信
3. **Debug模块初始化**：`Debug_Init()` 初始化printf重定向
4. **Log模块初始化**：`Log_Init()` 初始化日志系统
5. **Buzzer初始化**：`Buzzer_Init()` 初始化Buzzer驱动
6. **OLED初始化**：`OLED_Init()` 初始化OLED显示
7. **Buzzer控制**：使用快捷宏 `BUZZER1_On()`, `BUZZER1_Off()`, `BUZZER1_Beep()` 控制Buzzer
8. **延时**：使用 `Delay_ms()` 实现时间间隔

---

## 📚 关键函数说明

### Buzzer相关函数

- **`Buzzer_Init()`**：初始化Buzzer驱动模块
  - 在本案例中用于初始化GPIO模式的Buzzer
  - 根据配置表自动初始化所有enabled=1的Buzzer
  - 返回Buzzer_Status_t错误码，需要检查返回值

- **`BUZZER1_On()`**：开启Buzzer（快捷宏）
  - 在本案例中用于开启Buzzer输出

- **`BUZZER1_Off()`**：关闭Buzzer（快捷宏）
  - 在本案例中用于关闭Buzzer输出

- **`BUZZER1_Beep()`**：鸣响Buzzer（快捷宏）
  - 在本案例中用于实现单次鸣响和报警音效
  - 参数：持续时间（毫秒）

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

### OLED相关函数

- **`OLED_Init()`**：初始化OLED显示模块
  - 在本案例中用于初始化OLED显示
  - 返回OLED_Status_t错误码，需要检查返回值

- **`OLED_Clear()`**：清屏
  - 在本案例中用于清除OLED显示内容

- **`OLED_ShowString()`**：显示字符串
  - 在本案例中用于显示操作状态和提示信息
  - 参数：行号、列号、字符串（全英文，ASCII字符）

**详细函数实现和调用示例请参考**：`main_example.c` 中的代码

---

## 🎯 预期效果

- **OLED显示**：显示当前操作状态和提示信息
- **示例1**：Buzzer开启500ms，然后关闭500ms（OLED显示"Buzzer: ON/OFF"）
- **示例2**：Buzzer鸣响300ms（OLED显示"Beeping..."），然后三声短鸣响（每声100ms）
- **示例3**：报警音效（三短一长，OLED显示"Alarm Sound"）
- 程序循环执行上述操作

---

## 🔍 扩展练习

1. **修改鸣响时长**：修改 `BUZZER1_Beep()` 的参数，改变鸣响时长
2. **修改报警音模式**：尝试不同的报警音组合（如两短一长、四短一长等）
3. **添加更多Buzzer**：配置多个Buzzer，实现多声部效果
4. **使用函数接口**：不使用快捷宏，改用 `Buzzer_On()`, `Buzzer_Off()` 函数
5. **自定义节奏**：创建自己的节奏模式

---

## ⚠️ 注意事项与重点

### ⚠️ 重要提示

1. **必须使用有源蜂鸣器**：GPIO模式必须使用有源蜂鸣器，无源蜂鸣器无法通过GPIO控制
2. **初始化顺序**：必须严格按照 System_Init → UART → Debug → Log → 其他模块 的顺序初始化
3. **UART配置**：新项目必须包含UART_CONFIGS配置（用于串口调试和日志输出）
4. **错误处理**：所有模块初始化函数必须检查返回值，使用ErrorHandler统一处理错误

### 🔑 关键点

1. **标准初始化流程**：
   - 系统初始化（System_Init）
   - UART初始化（UART_Init）
   - Debug模块初始化（Debug_Init，UART模式）
   - Log模块初始化（Log_Init）
   - ErrorHandler自动初始化（无需显式调用）
   - 其他模块初始化（按依赖顺序）

2. **错误处理策略**：
   - UART/Debug初始化失败：必须停止程序（进入死循环）
   - Log初始化失败：可以继续运行（使用UART直接输出）
   - 其他模块初始化失败：根据模块重要性决定是否继续运行

3. **输出分工规范**：
   - **串口（UART）**：详细日志、调试信息、错误详情（支持中文，GB2312编码）
   - **OLED**：关键状态、实时数据、简要提示（全英文，ASCII字符）
   - **双边输出**：系统启动信息、关键错误、重要状态变化

4. **GPIO模式特点**：
   - 只能控制开关，无法改变频率和音调
   - 适用于有源蜂鸣器（内部有振荡电路）
   - 控制简单，响应快速

5. **日志输出**：
   - 开发阶段使用LOG_LEVEL_DEBUG（显示所有日志）
   - 发布时改为LOG_LEVEL_INFO或LOG_LEVEL_WARN
   - 日志调用必须包含模块名字符串

### 💡 调试技巧

1. **没有日志输出**：
   - 检查UART是否正确初始化
   - 检查Debug模块是否正确初始化
   - 检查串口助手配置是否正确（115200, 8N1）
   - 检查硬件连接是否正确（PA9/PA10）

2. **Buzzer不响**：
   - 检查蜂鸣器类型（必须使用有源蜂鸣器）
   - 检查GPIO配置是否正确（PA3）
   - 检查Buzzer的enabled标志是否为1
   - 检查有效电平配置（高电平有效/低电平有效）

3. **OLED不显示**：
   - 检查OLED连接是否正确（SCL: PB8, SDA: PB9）
   - 检查软件I2C模块是否已启用
   - 检查OLED模块是否已启用

---

## ⚠️ 常见问题

### Buzzer不响
- **检查蜂鸣器类型**：必须使用**有源蜂鸣器**，无源蜂鸣器无法通过GPIO控制
- 检查案例目录下的 `board.h` 中的Buzzer配置是否正确
- 检查Buzzer引脚是否与硬件连接一致（PA3）
- 检查Buzzer有效电平配置（高电平有效/低电平有效）
- 检查Buzzer的 `enabled` 标志是否为1
- 检查Buzzer类型（有源/无源）是否与驱动模式匹配

### OLED不显示
- 检查OLED连接是否正确（SCL: PB8, SDA: PB9）
- 检查软件I2C模块是否已启用（`CONFIG_MODULE_SOFT_I2C_ENABLED = 1`）
- 检查OLED模块是否已启用（`CONFIG_MODULE_OLED_ENABLED = 1`）

### 编译错误
- 确保已包含必要的头文件
- 确保 `System_Init()` 和 `Buzzer_Init()` 已正确调用
- 确保Buzzer模块已启用（`CONFIG_MODULE_BUZZER_ENABLED = 1`）
- 确保OLED模块已启用（`CONFIG_MODULE_OLED_ENABLED = 1`）
- 确保软件I2C模块已启用（`CONFIG_MODULE_SOFT_I2C_ENABLED = 1`）
- 确保UART模块已启用（`CONFIG_MODULE_UART_ENABLED = 1`）
- 确保Log模块已启用（`CONFIG_MODULE_LOG_ENABLED = 1`）
- 确保ErrorHandler模块已启用（`CONFIG_MODULE_ERROR_HANDLER_ENABLED = 1`）

---

## 🔗 相关文档

- **主程序代码**：`Examples/Basic/Basic01_ActiveBuzzer/main_example.c`
- **硬件配置**：`Examples/Basic/Basic01_ActiveBuzzer/board.h`
- **模块配置**：`Examples/Basic/Basic01_ActiveBuzzer/config.h`
- **Buzzer驱动模块文档**：`Drivers/basic/buzzer.c/h`
- **GPIO驱动模块文档**：`Drivers/basic/gpio.c/h`
- **UART驱动模块文档**：`Drivers/uart/uart.c/h`
- **Log模块文档**：`Debug/log.c/h`
- **ErrorHandler模块文档**：`Common/error_handler.c/h`
- **OLED驱动模块文档**：`Drivers/display/oled_ssd1306.c/h`
- **项目规范文档**：`PROJECT_KEYWORDS.md`
- **案例参考**：`Examples/README.md`

---

## 📚 相关模块

- **Buzzer驱动**：`Drivers/basic/buzzer.c/h`
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

