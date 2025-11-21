# Basic02_pwm_Buzzer - PWM模式Buzzer控制示例

PWM模式Buzzer控制示例，演示如何使用Buzzer驱动模块进行PWM模式的频率控制、音调播放等功能。

---

## 📋 功能说明

- **PWM模式**：演示频率控制、音调播放、旋律播放、频率扫描等功能
- **OLED显示**：使用OLED显示当前操作状态和提示信息
- 演示Buzzer驱动的PWM模式使用
- 演示系统初始化和延时功能
- 演示Buzzer频率设置和音调播放功能

---

## 🔧 硬件要求

### 必需硬件
- STM32F103C8T6开发板
- Buzzer（**无源蜂鸣器**，PWM模式必须使用无源蜂鸣器）
- OLED显示屏（SSD1306，I2C接口，用于显示提示信息）
  - SCL连接到PB8
  - SDA连接到PB9

### 硬件连接

**Buzzer（无源蜂鸣器）**：
- Buzzer正极连接到PA6（TIM3 CH1）
- Buzzer负极连接到GND

**OLED显示屏（用于显示提示信息）**：
- SCL连接到PB8
- SDA连接到PB9

### 硬件配置

**⚠️ 重要说明**：案例是独立工程，硬件配置在案例目录下的 `board.h` 中。
如果硬件引脚不同，直接修改 `Examples/Basic/Basic02_pwm_Buzzer/board.h` 中的配置即可。

**Buzzer配置**（PWM模式）：
```c
/* Buzzer统一配置表 */
#define BUZZER_CONFIGS { \
    {BUZZER_MODE_PWM, NULL, 0, 1, 0, Bit_RESET, 1},  /* PWM模式，TIM3(实例1)通道1，启用 */ \
}
```

**配置说明**：
- `mode`: `BUZZER_MODE_PWM`（PWM模式）
- `port`/`pin`: PWM模式下可为NULL和0（忽略）
- `pwm_instance`: 1表示TIM3（0=TIM1, 1=TIM3, 2=TIM4）
- `pwm_channel`: 0表示通道1（0=CH1, 1=CH2, 2=CH3, 3=CH4）
- `active_level`: `Bit_RESET`为低电平有效，`Bit_SET`为高电平有效
- `enabled`: 1表示启用该Buzzer，0表示禁用

**PWM配置**（已包含在board.h中）：
```c
/* PWM统一配置表 */
#define PWM_CONFIGS { \
    {TIM3, {{GPIOA, GPIO_Pin_6, 1}, {GPIOA, GPIO_Pin_0, 0}, {GPIOA, GPIO_Pin_0, 0}, {GPIOA, GPIO_Pin_0, 0}}, 1}, /* TIM3：PA6(CH1)，启用 */ \
}
```

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
- **必须使用无源蜂鸣器**，有源蜂鸣器无法通过PWM控制频率
- 根据实际硬件修改PWM引脚（`GPIO_Pin_6`等）
- 根据实际硬件修改OLED引脚（`GPIO_Pin_8`、`GPIO_Pin_9`等）
- 根据实际硬件修改PWM实例和通道（`pwm_instance`、`pwm_channel`）

---

## 🚀 使用方法

### 快速开始
1. **打开案例工程**：双击 `Examples/Basic/Basic02_pwm_Buzzer/Examples.uvprojx` 打开Keil工程
2. **检查硬件配置**：确认案例目录下的 `board.h` 中Buzzer配置正确（TIM3 CH1，PA6）
3. **编译下载**：在Keil中编译（F7）并下载到开发板（F8）
4. **观察效果**：
   - OLED显示操作状态
   - 不同频率的鸣响（500Hz、1kHz、2kHz）
   - 播放C4-C5音阶
   - 播放简单旋律（小星星前奏）
   - 频率扫描效果（200Hz-2000Hz）
   - 持续播放音调

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

// PWM模式：设置频率
Buzzer_SetFrequency(BUZZER_1, 1000);  // 设置1kHz
BUZZER1_On();
Delay_ms(500);
BUZZER1_Off();

// PWM模式：播放音调
Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_A4, 500);  // 播放A4音调500ms

// PWM模式：持续播放（需要手动停止）
Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_A4, 0);  // 持续播放
Delay_ms(1000);
BUZZER1_Stop();  // 手动停止
```

### 完整流程

1. **系统初始化**：`System_Init()` 初始化SysTick和延时模块
2. **Buzzer初始化**：`Buzzer_Init()` 初始化Buzzer驱动（PWM模式）
3. **OLED初始化**：`OLED_Init()` 初始化OLED显示
4. **频率控制**：使用 `Buzzer_SetFrequency()` 设置频率，然后 `BUZZER1_On()` 开启
5. **音调播放**：使用 `Buzzer_PlayTone()` 播放指定音调
6. **延时**：使用 `Delay_ms()` 实现时间间隔

---

## 🎯 预期效果

- **OLED显示**：显示当前操作状态和提示信息
- **示例1**：不同频率的鸣响（500Hz、1kHz、2kHz，各500ms）
- **示例2**：播放C4-C5音阶（每个音调300ms）
- **示例3**：播放简单旋律（小星星前奏）
- **示例4**：频率扫描效果（从200Hz扫描到2000Hz）
- **示例5**：持续播放A4音调1秒后停止
- 程序循环执行上述操作

---

## 🔍 扩展练习

1. **修改频率范围**：修改频率扫描的起始和结束频率
2. **播放完整旋律**：实现完整的小星星或其他歌曲
3. **添加更多音调**：扩展音调范围（需要修改buzzer.c中的音调频率表）
4. **实现节拍控制**：添加节拍功能，实现更复杂的音乐播放
5. **音量控制**：通过调整PWM占空比实现音量控制（需要修改buzzer.c）
6. **多Buzzer和声**：配置多个Buzzer，实现和声效果

---

## ⚠️ 常见问题

### Buzzer不响
- **检查蜂鸣器类型**：必须使用**无源蜂鸣器**，有源蜂鸣器无法通过PWM控制频率
- 检查案例目录下的 `board.h` 中的Buzzer配置是否正确
- 检查PWM实例和通道配置（`pwm_instance`、`pwm_channel`）
- 检查PWM配置（`PWM_CONFIGS`）中的引脚是否正确（PA6）
- 检查Buzzer的 `enabled` 标志是否为1
- 检查定时器模块是否已启用（`CONFIG_MODULE_TIMER_ENABLED = 1`）

### 频率不正确
- 检查系统时钟配置是否正确
- 检查PWM频率设置是否在有效范围内（1Hz ~ 72MHz）
- 检查PWM配置是否正确

### OLED不显示
- 检查OLED连接是否正确（SCL: PB8, SDA: PB9）
- 检查软件I2C模块是否已启用（`CONFIG_MODULE_SOFT_I2C_ENABLED = 1`）
- 检查OLED模块是否已启用（`CONFIG_MODULE_OLED_ENABLED = 1`）

### 编译错误
- 确保已包含必要的头文件
- 确保 `System_Init()` 和 `Buzzer_Init()` 已正确调用
- 确保Buzzer模块已启用（`CONFIG_MODULE_BUZZER_ENABLED = 1`）
- 确保定时器模块已启用（`CONFIG_MODULE_TIMER_ENABLED = 1`）
- 确保OLED模块已启用（`CONFIG_MODULE_OLED_ENABLED = 1`）
- 确保软件I2C模块已启用（`CONFIG_MODULE_SOFT_I2C_ENABLED = 1`）

---

## 📚 相关模块

- **Buzzer驱动**：`Drivers/basic/buzzer.c/h`
- **PWM驱动**：`Drivers/timer/timer_pwm.c/h`
- **GPIO驱动**：`Drivers/basic/gpio.c/h`
- **OLED驱动**：`Drivers/display/oled_ssd1306.c/h`
- **OLED字库**：`Drivers/display/oled_font_ascii8x16.c/h`
- **软件I2C驱动**：`Drivers/i2c/i2c_sw.c/h`
- **延时功能**：`system/delay.c/h`
- **系统初始化**：`system/system_init.c/h`
- **硬件配置**：案例目录下的 `board.h`

---

## 🔄 与Basic01的区别

| 特性 | Basic01_gpio_Buzzer | Basic02_pwm_Buzzer |
|------|---------------------|-------------------|
| 驱动模式 | GPIO模式 | PWM模式 |
| 蜂鸣器类型 | 有源蜂鸣器 | 无源蜂鸣器 |
| 频率控制 | ❌ 不支持 | ✅ 支持 |
| 音调播放 | ❌ 不支持 | ✅ 支持 |
| 引脚配置 | 直接GPIO引脚（PA3） | PWM引脚（PA6，TIM3 CH1） |
| 功能 | 简单开关、鸣响 | 频率控制、音调播放、旋律 |

---

**最后更新**：2024-01-01


