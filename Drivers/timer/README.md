# 定时器模块

本模块包含定时器的各种功能：TIM2时间基准、PWM输出、输入捕获、编码器模式、输出比较。

## TIM2时间基准模块 (TIM2_TimeBase)

### 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始化 | `TIM2_TimeBase_Init()` | 初始化TIM2时间基准（1ms中断） |
| 重新配置 | `TIM2_TimeBase_Reconfig()` | 频率切换时重新配置，保持1ms中断间隔不变 |
| 获取tick | `TIM2_TimeBase_GetTick()` | 获取当前tick值（代表真实时间，毫秒） |
| 检查状态 | `TIM2_TimeBase_IsInitialized()` | 检查是否已初始化 |

### 重要提醒

1. **TIM2被占用**：TIM2被TIM2_TimeBase模块占用，用于系统时间基准（1ms中断）
2. **时间基准**：提供系统时间基准，确保频率变化时1秒永远是1秒
3. **动态适配**：频率切换时自动重新配置，保持1ms中断间隔不变
4. **全局变量**：`g_task_tick` 在TIM2中断中递增，代表真实时间（毫秒）

### 使用说明

TIM2_TimeBase模块通常由系统初始化框架自动初始化，无需手动调用。如果需要手动使用：

```c
/* 初始化TIM2时间基准 */
TIM2_TimeBase_Init();

/* 获取当前tick（毫秒） */
uint32_t tick = TIM2_TimeBase_GetTick();

/* 频率切换时重新配置 */
TIM2_TimeBase_Reconfig(72000000);  /* 切换到72MHz */
```

---

## PWM模块 (timer_pwm)

### 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始化 | `PWM_Init()` | 初始化定时器外设和GPIO（TIM1/TIM3/TIM4） |
| 反初始化 | `PWM_Deinit()` | 反初始化定时器外设 |
| 设置频率 | `PWM_SetFrequency()` | 设置PWM频率（所有通道共享） |
| 设置占空比 | `PWM_SetDutyCycle()` | 设置指定通道的占空比（0.0~100.0%） |
| 使能通道 | `PWM_EnableChannel()` | 使能PWM通道输出 |
| 禁用通道 | `PWM_DisableChannel()` | 禁用PWM通道输出 |
| 状态检查 | `PWM_IsInitialized()` | 检查是否已初始化 |
| 获取外设 | `PWM_GetPeriph()` | 获取定时器外设指针 |

### 重要提醒

1. **实例选择**：支持TIM1、TIM3、TIM4三个实例（TIM2已被TIM2_TimeBase占用），使用`PWM_INSTANCE_TIM1`、`PWM_INSTANCE_TIM3`或`PWM_INSTANCE_TIM4`
2. **配置驱动**：GPIO和定时器参数在`BSP/board.h`中配置，自动初始化
3. **频率设置**：所有通道共享同一个频率，设置频率会影响所有通道
4. **占空比设置**：每个通道可以独立设置占空比（0.0~100.0%）
5. **通道配置**：每个定时器最多支持4个PWM通道，在`BSP/board.h`中配置哪些通道启用
6. **TIM1特殊处理**：TIM1需要调用`TIM_CtrlPWMOutputs()`使能主输出

## 配置说明

在`BSP/board.h`中配置PWM：

```c
#define PWM_CONFIGS {                                                                                    \
    {TIM3, {{GPIOA, GPIO_Pin_6, 1}, {GPIOA, GPIO_Pin_7, 1}, {GPIOA, GPIO_Pin_0, 0}, {GPIOA, GPIO_Pin_1, 0}}, 1}, /* TIM3：PA6(CH1), PA7(CH2)，启用 */ \
}
```

## 使用示例

```c
/* 初始化PWM（TIM3） */
PWM_Init(PWM_INSTANCE_TIM3);

/* 设置频率为1kHz */
PWM_SetFrequency(PWM_INSTANCE_TIM3, 1000);

/* 设置通道1占空比为50% */
PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_1, 50.0f);

/* 设置通道2占空比为25% */
PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_2, 25.0f);

/* 使能通道1和通道2 */
PWM_EnableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_1);
PWM_EnableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_2);
```

## 输入捕获模块 (timer_input_capture)

### 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始化 | `IC_Init()` | 初始化定时器输入捕获（TIM1/TIM2/TIM3/TIM4） |
| 反初始化 | `IC_Deinit()` | 反初始化输入捕获 |
| 启动/停止 | `IC_Start()` / `IC_Stop()` | 启动/停止输入捕获 |
| 读取捕获值 | `IC_ReadValue()` | 读取原始捕获值 |
| 测量频率 | `IC_MeasureFrequency()` | 测量信号频率 |
| 测量PWM | `IC_MeasurePWM()` | 测量PWM信号（频率、占空比） |
| 测量脉冲宽度 | `IC_MeasurePulseWidth()` | 测量脉冲宽度 |

### 使用示例

```c
/* 初始化输入捕获（TIM2，通道1，上升沿） */
IC_Init(IC_INSTANCE_TIM2, IC_CHANNEL_1, IC_POLARITY_RISING);

/* 启动输入捕获 */
IC_Start(IC_INSTANCE_TIM2, IC_CHANNEL_1);

/* 测量频率 */
uint32_t frequency;
IC_MeasureFrequency(IC_INSTANCE_TIM2, IC_CHANNEL_1, &frequency, 1000);

/* 测量PWM信号（需要配置为双边沿模式） */
IC_MeasureResult_t result;
IC_MeasurePWM(IC_INSTANCE_TIM2, IC_CHANNEL_1, &result, 1000);
```

## 编码器模块 (timer_encoder)

### 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始化 | `ENCODER_Init()` | 初始化定时器编码器接口（TIM1/TIM2/TIM3/TIM4） |
| 反初始化 | `ENCODER_Deinit()` | 反初始化编码器 |
| 读取计数值 | `ENCODER_ReadCount()` | 读取编码器计数值（有符号） |
| 读取计数值 | `ENCODER_ReadCountUnsigned()` | 读取编码器计数值（无符号） |
| 设置计数值 | `ENCODER_SetCount()` | 设置编码器计数值 |
| 清零计数值 | `ENCODER_ClearCount()` | 清零编码器计数值 |
| 获取方向 | `ENCODER_GetDirection()` | 获取编码器旋转方向 |
| 启动/停止 | `ENCODER_Start()` / `ENCODER_Stop()` | 启动/停止编码器 |

### 使用示例

```c
/* 初始化编码器（TIM3，4倍频模式） */
ENCODER_Init(ENCODER_INSTANCE_TIM3, ENCODER_MODE_TI12);

/* 启动编码器 */
ENCODER_Start(ENCODER_INSTANCE_TIM3);

/* 读取编码器位置 */
int32_t position;
ENCODER_ReadCount(ENCODER_INSTANCE_TIM3, &position);

/* 获取旋转方向 */
ENCODER_Direction_t direction;
ENCODER_GetDirection(ENCODER_INSTANCE_TIM3, &direction);
```

## 输出比较模块 (timer_output_compare)

### 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始化 | `OC_Init()` | 初始化定时器输出比较（TIM1/TIM2/TIM3/TIM4） |
| 反初始化 | `OC_Deinit()` | 反初始化输出比较 |
| 设置比较值 | `OC_SetCompareValue()` | 设置输出比较值 |
| 获取比较值 | `OC_GetCompareValue()` | 获取输出比较值 |
| 启动/停止 | `OC_Start()` / `OC_Stop()` | 启动/停止输出比较 |
| 生成单脉冲 | `OC_GenerateSinglePulse()` | 生成单脉冲输出 |

### 使用示例

```c
/* 初始化输出比较（TIM3，通道1，翻转模式） */
OC_Init(OC_INSTANCE_TIM3, OC_CHANNEL_1, OC_MODE_TOGGLE, 1000, 500);

/* 启动输出比较 */
OC_Start(OC_INSTANCE_TIM3, OC_CHANNEL_1);

/* 生成单脉冲 */
OC_GenerateSinglePulse(OC_INSTANCE_TIM3, OC_CHANNEL_1, 100);
```
