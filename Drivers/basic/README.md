# 基础外设模块

## GPIO模块

### 功能列表

| 功能 | 函数/宏 | 说明 |
|------|---------|------|
| GPIO配置 | `GPIO_Config()` | 配置GPIO模式、速度，自动使能时钟 |
| 时钟使能 | `GPIO_EnableClock()` | 手动使能GPIO端口时钟 |
| 写入电平 | `GPIO_WritePin()` | 设置引脚电平（高/低） |
| 读取电平 | `GPIO_ReadPin()` | 读取引脚当前电平 |
| 翻转电平 | `GPIO_Toggle()` | 翻转引脚电平状态 |
| 快捷宏 | `GPIO_Init_Output()` | 快速配置为推挽输出 |
| 快捷宏 | `GPIO_SetPin()` / `GPIO_ResetPin()` | 快速设置/复位引脚 |

### 重要提醒

1. **自动时钟管理**：`GPIO_Config()`会自动使能对应GPIO端口的时钟，无需手动调用`GPIO_EnableClock()`
2. **模式支持**：支持输入（浮空/上拉/下拉/模拟）、输出（推挽/开漏）、复用（推挽/开漏）等全模式
3. **错误处理**：所有函数返回`GPIO_Status_t`错误码，需检查返回值
4. **向后兼容**：保留了`GPIO_Init_Output()`、`GPIO_SetPin()`等宏定义，兼容旧代码

---

## LED模块

### 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始化 | `LED_Init()` | 根据配置表初始化所有启用的LED |
| 反初始化 | `LED_Deinit()` | 关闭所有LED并释放资源 |
| 设置状态 | `LED_SetState()` | 设置LED状态（ON/OFF） |
| 获取状态 | `LED_GetState()` | 获取LED当前状态 |
| 点亮 | `LED_On()` | 点亮指定LED |
| 熄灭 | `LED_Off()` | 熄灭指定LED |
| 翻转 | `LED_Toggle()` | 翻转LED状态 |
| 闪烁 | `LED_Blink()` | LED闪烁一次（阻塞延时） |
| 快捷宏 | `LED1_On()` / `LED2_On()` | LED1/LED2快捷操作 |

### 重要提醒

1. **配置表驱动**：LED配置在`BSP/board.h`的`LED_CONFIGS`中定义，支持多LED管理
2. **有效电平处理**：模块自动处理有效电平（高电平点亮/低电平点亮），无需关心硬件连接
3. **运行时控制**：支持运行时使能/禁用LED，只初始化`enabled=1`的LED
4. **阻塞式闪烁**：`LED_Blink()`使用阻塞延时，执行期间CPU不处理其他任务
5. **错误处理**：所有函数返回`LED_Status_t`错误码，需检查返回值

---

## Buzzer模块

### 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始化 | `Buzzer_Init()` | 根据配置表初始化所有启用的Buzzer |
| 反初始化 | `Buzzer_Deinit()` | 关闭所有Buzzer并释放资源 |
| 设置状态 | `Buzzer_SetState()` | 设置Buzzer状态（ON/OFF） |
| 获取状态 | `Buzzer_GetState()` | 获取Buzzer当前状态 |
| 开启 | `Buzzer_On()` | 开启指定Buzzer |
| 关闭 | `Buzzer_Off()` | 关闭指定Buzzer |
| 停止 | `Buzzer_Stop()` | 停止Buzzer（等同于Off） |
| 鸣响 | `Buzzer_Beep()` | Buzzer鸣响一次（阻塞延时） |
| 设置频率 | `Buzzer_SetFrequency()` | 设置Buzzer频率（仅PWM模式） |
| 播放音调 | `Buzzer_PlayTone()` | 播放指定音调（仅PWM模式） |
| 快捷宏 | `BUZZER1_On()` / `BUZZER2_On()` | Buzzer1/Buzzer2快捷操作 |

### 重要提醒

1. **配置表驱动**：Buzzer配置在`BSP/board.h`的`BUZZER_CONFIGS`中定义，支持多Buzzer管理
2. **双模式支持**：
   - **GPIO模式**：简单开关控制，无频率控制，适用于**有源蜂鸣器**
     - 有源蜂鸣器内部有振荡电路，通电即响，频率固定（通常2-4kHz）
     - 只能控制开关，无法改变频率和音调
     - 使用场景：报警提示、按键反馈、状态提示等简单应用
   - **PWM模式**：通过PWM控制频率，支持音调播放，适用于**无源蜂鸣器**
     - 无源蜂鸣器需要外部驱动信号，通过改变PWM频率控制音调
     - 支持频率控制和音调播放，可以播放音乐
     - 使用场景：音乐播放、多音调报警、按键音效等需要音调控制的应用
3. **有效电平处理**：模块自动处理有效电平（高电平有效/低电平有效），无需关心硬件连接
4. **运行时控制**：支持运行时使能/禁用Buzzer，只初始化`enabled=1`的Buzzer
5. **阻塞式鸣响**：`Buzzer_Beep()`使用阻塞延时，执行期间CPU不处理其他任务
6. **PWM功能限制**：`Buzzer_SetFrequency()`和`Buzzer_PlayTone()`仅在PWM模式下有效，GPIO模式会返回错误
7. **音调支持**：支持C4-C5一个八度的音调（Do、Re、Mi、Fa、Sol、La、Si、Do）
8. **错误处理**：所有函数返回`Buzzer_Status_t`错误码，需检查返回值

### 配置说明

在`BSP/board.h`中配置Buzzer：

**GPIO模式示例（有源蜂鸣器）**：
```c
#define BUZZER_CONFIGS {                                                                                    \
    {BUZZER_MODE_GPIO, GPIOA, GPIO_Pin_2, 0, 0, Bit_RESET, 1}, /* Buzzer1：GPIO模式（有源蜂鸣器），PA2，低电平有效，启用（PWM实例和通道在GPIO模式下忽略） */ \
}
```

**PWM模式示例（无源蜂鸣器）**：
```c
#define BUZZER_CONFIGS {                                                                                    \
    {BUZZER_MODE_PWM, NULL, 0, 1, 0, Bit_RESET, 1}, /* Buzzer1：PWM模式（无源蜂鸣器），TIM3(实例1)通道1，启用（GPIO端口和引脚在PWM模式下忽略） */ \
}
```

**⚠️ 重要提示**：
- **有源蜂鸣器**：必须使用GPIO模式，不能使用PWM模式（有源蜂鸣器内部有振荡电路，无法通过PWM改变频率）
- **无源蜂鸣器**：必须使用PWM模式，不能使用GPIO模式（无源蜂鸣器需要外部驱动信号，GPIO模式无法产生可变频率）
- 选择错误的模式会导致蜂鸣器无法正常工作或损坏

**配置说明**：
- `pwm_instance`: 0=TIM1, 1=TIM3, 2=TIM4
- `pwm_channel`: 0=通道1, 1=通道2, 2=通道3, 3=通道4

### 使用示例

```c
/* 初始化Buzzer */
Buzzer_Init();

/* GPIO模式：简单开关控制 */
Buzzer_On(BUZZER_1);
Delay_ms(1000);
Buzzer_Off(BUZZER_1);

/* GPIO模式：鸣响 */
Buzzer_Beep(BUZZER_1, 500);  /* 鸣响500ms */

/* PWM模式：设置频率 */
Buzzer_SetFrequency(BUZZER_1, 1000);  /* 设置1kHz */
Buzzer_On(BUZZER_1);
Delay_ms(1000);
Buzzer_Off(BUZZER_1);

/* PWM模式：播放音调 */
Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_A4, 500);  /* 播放A4音调500ms */
Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_C5, 0);   /* 持续播放C5，需要手动调用Buzzer_Stop()停止 */
Delay_ms(1000);
Buzzer_Stop(BUZZER_1);
```

