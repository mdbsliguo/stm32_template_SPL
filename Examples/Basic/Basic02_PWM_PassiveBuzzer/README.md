# Basic02 - 无源蜂鸣器播放完整音乐

## 📋 案例目的

- **核心目标**：演示如何使用无源蜂鸣器播放一首完整的音乐
- **学习重点**：
  - 理解无源蜂鸣器PWM模式的工作原理
  - 掌握音符频率与PWM频率的对应关系
  - 学习如何将乐谱转换为代码实现
  - 学习节拍控制和音乐播放的实现方法
- **应用场景**：适用于需要播放音乐、音效提示、报警音等应用

---

## 📋 功能说明

- **完整音乐播放**：播放《小星星》完整版（6段歌词）
- **音符频率控制**：支持C3-C6音域的音符播放
- **节拍控制**：支持全音符、二分音符、四分音符、八分音符、十六分音符
- **循环播放**：播放完成后自动重复

---

## 🔧 硬件要求

### 必需硬件
- STM32F103C8T6开发板
- Buzzer（**无源蜂鸣器**，必须使用无源蜂鸣器）

### 硬件连接

**Buzzer（无源蜂鸣器）**：
- Buzzer正极连接到PA6（TIM3 CH1）
- Buzzer负极连接到GND

**⚠️ 重要说明**：
- **必须使用无源蜂鸣器**：无源蜂鸣器需要外部驱动信号，通过改变PWM频率控制音调
- 有源蜂鸣器无法通过PWM控制频率，无法播放音乐
- 如果使用有源蜂鸣器，请使用Basic01案例（GPIO模式）

### 硬件配置

**⚠️ 重要说明**：案例是独立工程，硬件配置在案例目录下的 `board.h` 中。
如果硬件引脚不同，直接修改 `Examples/Basic/Basic02_PWM_PassiveBuzzer/board.h` 中的配置即可。

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

**注意**：
- **必须使用无源蜂鸣器**，有源蜂鸣器无法通过PWM控制频率
- 根据实际硬件修改PWM引脚（`GPIO_Pin_6`等）
- 根据实际硬件修改PWM实例和通道（`pwm_instance`、`pwm_channel`）

---

## 🚀 使用方法

### 快速开始
1. **打开案例工程**：双击 `Examples/Basic/Basic02_PWM_PassiveBuzzer/Examples.uvprojx` 打开Keil工程
2. **检查硬件配置**：确认案例目录下的 `board.h` 中Buzzer配置正确（TIM3 CH1，PA6）
3. **编译下载**：在Keil中编译（F7）并下载到开发板（F8）
4. **观察效果**：
   - 无源蜂鸣器播放《小星星》完整版音乐
   - 播放完成后等待3秒自动重复

### 详细操作流程

**通用操作步骤请参考**：[Examples/README.md](../README.md#-通用操作流程)

**注意**：本案例是独立工程，无需复制文件，直接打开工程文件即可编译运行。

---

## 🔄 实现流程

### 整体逻辑

本案例通过以下步骤实现完整音乐播放：

1. **系统初始化**：初始化系统时钟、延时模块
2. **UART初始化**：初始化串口用于日志输出
3. **Buzzer初始化**：初始化PWM模式的Buzzer
4. **音乐播放**：按照乐谱顺序播放每个音符
5. **循环播放**：播放完成后等待3秒，自动重复

### 关键方法

- **音符频率映射**：将音符（C3-C6）映射到对应的频率（Hz）
- **节拍控制**：通过延时控制每个音符的持续时间
- **音符播放函数**：`PlayNote()` 函数实现单个音符的播放
- **音乐播放函数**：`PlaySong_TwinkleTwinkleLittleStar()` 实现完整音乐播放

### 工作流程示意

```
系统初始化（System_Init）
    ↓
UART初始化（UART_Init）
    ↓
Debug模块初始化（Debug_Init）
    ↓
Log模块初始化（Log_Init）
    ↓
Buzzer初始化（Buzzer_Init）
    ↓
播放音乐（PlaySong_TwinkleTwinkleLittleStar）
    ↓
等待3秒
    ↓
循环播放
```

---

## 📝 代码说明

### 关键代码

```c
/* 音符频率定义 */
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
/* ... 更多音符 ... */

/* 节拍时长定义 */
#define TEMPO 120  /* 每分钟120拍 */
#define QUARTER_NOTE   (60000 / TEMPO)        /* 四分音符 */
#define HALF_NOTE      (60000 / TEMPO * 2)    /* 二分音符 */

/* 播放单个音符 */
static void PlayNote(uint32_t frequency, uint32_t duration)
{
    if (frequency == 0) {
        /* 休止符 */
        BUZZER1_Off();
        Delay_ms(duration);
    } else {
        /* 播放音符 */
        Buzzer_SetFrequency(BUZZER_1, frequency);
        BUZZER1_On();
        Delay_ms(duration);
        BUZZER1_Off();
        Delay_ms(20);  /* 音符间隔 */
    }
}

/* 播放音乐 */
PlaySong_TwinkleTwinkleLittleStar();
```

### 完整流程

1. **系统初始化**：`System_Init()` 初始化SysTick和延时模块
2. **Buzzer初始化**：`Buzzer_Init()` 初始化Buzzer驱动（PWM模式）
3. **音符播放**：使用 `PlayNote()` 播放每个音符，通过频率和时长控制
4. **音乐播放**：按照乐谱顺序调用 `PlayNote()` 播放所有音符
5. **循环播放**：播放完成后等待3秒，自动重复

---

## 📚 关键函数说明

### Buzzer相关函数

- **`Buzzer_Init()`**：初始化Buzzer驱动模块
  - 在本案例中用于初始化PWM模式的Buzzer
  - 根据配置表自动初始化所有enabled=1的Buzzer
  - 返回Buzzer_Status_t错误码，需要检查返回值

- **`Buzzer_SetFrequency()`**：设置Buzzer频率（PWM模式）
  - 在本案例中用于设置每个音符的频率
  - 参数：Buzzer编号、频率值（Hz）
  - 返回Buzzer_Status_t错误码，需要检查返回值

- **`BUZZER1_On()`** / **`BUZZER1_Off()`**：开启/关闭Buzzer（快捷宏）
  - 在本案例中用于控制音符的开始和结束

### 自定义函数

- **`PlayNote()`**：播放单个音符
  - 参数：频率（Hz）、持续时间（毫秒）
  - 频率为0时表示休止符
  - 自动处理音符间隔

- **`PlaySong_TwinkleTwinkleLittleStar()`**：播放《小星星》完整版
  - 按照乐谱顺序调用 `PlayNote()` 播放所有音符
  - 包含6段歌词的完整旋律

---

## 🎯 预期效果

- **音乐播放**：无源蜂鸣器播放《小星星》完整版（6段歌词）
- **循环播放**：播放完成后等待3秒，自动重复播放
- **串口日志**：输出播放开始和完成的日志信息

---

## 🔍 扩展练习

1. **修改音乐**：将 `PlaySong_TwinkleTwinkleLittleStar()` 函数中的音符序列替换为其他歌曲
2. **调整节拍**：修改 `TEMPO` 宏定义，改变播放速度
3. **添加更多音符**：扩展音符频率表，支持更多音域（如低音、高音）
4. **实现和声**：配置多个Buzzer，实现多声部效果
5. **音量控制**：通过调整PWM占空比实现音量控制
6. **节拍变化**：实现渐快、渐慢等节拍变化效果

---

## ⚠️ 注意事项与重点

### ⚠️ 重要提示

1. **必须使用无源蜂鸣器**：PWM模式必须使用无源蜂鸣器，有源蜂鸣器无法通过PWM控制频率
2. **PWM配置正确性**：确保PWM实例和通道配置正确（TIM3 CH1对应PA6）
3. **初始化顺序**：必须严格按照 System_Init → UART → Debug → Log → Buzzer 的顺序初始化
4. **音符频率准确性**：确保音符频率定义准确，以获得正确的音调

### 🔑 关键点

1. **音符频率映射**：
   - C4 = 262 Hz（中央C）
   - 每个半音频率比约为 2^(1/12)
   - 标准音A4 = 440 Hz

2. **节拍控制**：
   - TEMPO定义每分钟的拍数
   - 四分音符时长 = 60000 / TEMPO（毫秒）
   - 其他音符时长按比例计算

3. **音符间隔**：
   - 每个音符播放后需要短暂间隔（20ms）
   - 避免音符粘连，保证音质

4. **音乐播放**：
   - 按照乐谱顺序播放每个音符
   - 支持休止符（频率为0）
   - 支持不同时长的音符

### 💡 调试技巧

1. **没有声音**：
   - 检查蜂鸣器类型（必须使用无源蜂鸣器）
   - 检查PWM配置是否正确（TIM3 CH1，PA6）
   - 检查Buzzer的enabled标志是否为1
   - 检查定时器模块是否已启用

2. **音调不正确**：
   - 检查音符频率定义是否正确
   - 检查系统时钟配置是否正确
   - 检查PWM频率设置是否在有效范围内

3. **节拍不准确**：
   - 检查TEMPO定义是否正确
   - 检查延时函数是否正常工作
   - 检查音符间隔是否合适

---

## ⚠️ 常见问题

### 蜂鸣器不响
- **检查蜂鸣器类型**：必须使用**无源蜂鸣器**，有源蜂鸣器无法通过PWM控制频率
- 检查案例目录下的 `board.h` 中的Buzzer配置是否正确
- 检查PWM实例和通道配置（`pwm_instance`、`pwm_channel`）
- 检查PWM配置（`PWM_CONFIGS`）中的引脚是否正确（PA6）
- 检查Buzzer的 `enabled` 标志是否为1
- 检查定时器模块是否已启用（`CONFIG_MODULE_TIMER_ENABLED = 1`）

### 音调不正确
- 检查音符频率定义是否正确
- 检查系统时钟配置是否正确
- 检查PWM频率设置是否在有效范围内（1Hz ~ 72MHz）

### 节拍不准确
- 检查TEMPO定义是否正确
- 检查延时函数是否正常工作
- 检查音符间隔是否合适

### 编译错误
- 确保已包含必要的头文件
- 确保 `System_Init()` 和 `Buzzer_Init()` 已正确调用
- 确保Buzzer模块已启用（`CONFIG_MODULE_BUZZER_ENABLED = 1`）
- 确保定时器模块已启用（`CONFIG_MODULE_TIMER_ENABLED = 1`）

---

## 🔗 相关文档

- **主程序代码**：`Examples/Basic/Basic02_PWM_PassiveBuzzer/main_example.c`
- **硬件配置**：`Examples/Basic/Basic02_PWM_PassiveBuzzer/board.h`
- **模块配置**：`Examples/Basic/Basic02_PWM_PassiveBuzzer/config.h`
- **Buzzer驱动模块文档**：`Drivers/basic/buzzer.c/h`
- **PWM驱动模块文档**：`Drivers/timer/timer_pwm.c/h`
- **UART驱动模块文档**：`Drivers/uart/uart.c/h`
- **Log模块文档**：`Debug/log.c/h`
- **ErrorHandler模块文档**：`Common/error_handler.c/h`
- **项目规范文档**：`PROJECT_KEYWORDS.md`
- **案例参考**：`Examples/README.md`

---

## 📚 相关模块

- **Buzzer驱动**：`Drivers/basic/buzzer.c/h`
- **PWM驱动**：`Drivers/timer/timer_pwm.c/h`
- **GPIO驱动**：`Drivers/basic/gpio.c/h`
- **UART驱动**：`Drivers/uart/uart.c/h`
- **Debug模块**：`Debug/debug.c/h`
- **Log模块**：`Debug/log.c/h`
- **ErrorHandler模块**：`Common/error_handler.c/h`
- **延时功能**：`System/delay.c/h`
- **系统初始化**：`System/system_init.c/h`
- **硬件配置**：案例目录下的 `board.h`

---

## 🔄 与Basic01的区别

| 特性 | Basic01_ActiveBuzzer | Basic02_PWM_PassiveBuzzer |
|------|---------------------|---------------------------|
| 驱动模式 | GPIO模式 | PWM模式 |
| 蜂鸣器类型 | 有源蜂鸣器 | 无源蜂鸣器 |
| 频率控制 | ❌ 不支持 | ✅ 支持 |
| 音调播放 | ❌ 不支持 | ✅ 支持 |
| 音乐播放 | ❌ 不支持 | ✅ 支持（完整音乐） |
| 引脚配置 | 直接GPIO引脚（PA3） | PWM引脚（PA6，TIM3 CH1） |
| 功能 | 简单开关、鸣响 | 频率控制、音调播放、完整音乐 |

---

**最后更新**：2024-01-01
