# Basic01_gpio_Buzzer - GPIO模式Buzzer基础控制示例

基础的Buzzer控制示例，演示如何使用Buzzer驱动模块进行GPIO模式的控制。

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
- Buzzer（有源蜂鸣器）
- OLED显示屏（SSD1306，I2C接口，用于显示提示信息）
  - SCL连接到PB8
  - SDA连接到PB9

### 硬件连接

**Buzzer（有源蜂鸣器）**：
- Buzzer正极连接到PA3
- Buzzer负极连接到GND

**OLED显示屏（用于显示提示信息）**：
- SCL连接到PB8
- SDA连接到PB9

### 硬件配置

**⚠️ 重要说明**：案例是独立工程，硬件配置在案例目录下的 `board.h` 中。
如果硬件引脚不同，直接修改 `Examples/Basic/Basic01_gpio_Buzzer/board.h` 中的配置即可。

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

**注意**：
- 根据实际硬件修改Buzzer引脚（`GPIO_Pin_3`等）
- 根据实际硬件修改OLED引脚（`GPIO_Pin_8`、`GPIO_Pin_9`等）
- 根据实际硬件修改有效电平（`Bit_RESET`/`Bit_SET`）

---

## 🚀 使用方法

### 快速开始
1. **打开案例工程**：双击 `Examples/Basic/Basic01_gpio_Buzzer/Examples.uvprojx` 打开Keil工程
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

## 📝 代码说明

### 关键代码

```c
// 系统初始化
System_Init();

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
2. **Buzzer初始化**：`Buzzer_Init()` 初始化Buzzer驱动
3. **OLED初始化**：`OLED_Init()` 初始化OLED显示
4. **Buzzer控制**：使用快捷宏 `BUZZER1_On()`, `BUZZER1_Off()`, `BUZZER1_Beep()` 控制Buzzer
5. **延时**：使用 `Delay_ms()` 实现时间间隔

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

## ⚠️ 常见问题

### Buzzer不响
- 检查案例目录下的 `board.h` 中的Buzzer配置是否正确
- 检查Buzzer引脚是否与硬件连接一致
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

---

## 📚 相关模块

- **Buzzer驱动**：`Drivers/basic/buzzer.c/h`
- **GPIO驱动**：`Drivers/basic/gpio.c/h`
- **OLED驱动**：`Drivers/display/oled_ssd1306.c/h`
- **OLED字库**：`Drivers/display/oled_font_ascii8x16.c/h`
- **软件I2C驱动**：`Drivers/i2c/i2c_sw.c/h`
- **延时功能**：`system/delay.c/h`
- **系统初始化**：`system/system_init.c/h`
- **硬件配置**：案例目录下的 `board.h`

---

**最后更新**：2024-01-01

