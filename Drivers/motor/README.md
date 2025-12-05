# 电机驱动模块

本目录包含多种电机驱动模块，支持不同功率和应用场景的电机控制。

---

## TB6612双路直流电机驱动模块

## 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始化 | `TB6612_Init()` | 初始化TB6612驱动模块（GPIO和PWM） |
| 反初始化 | `TB6612_Deinit()` | 反初始化TB6612驱动模块 |
| 设置方向 | `TB6612_SetDirection()` | 设置电机方向（正转/反转/停止/制动） |
| 设置速度 | `TB6612_SetSpeed()` | 设置电机速度（PWM占空比，0.0~100.0%） |
| 使能 | `TB6612_Enable()` | 使能TB6612（退出待机模式） |
| 禁用 | `TB6612_Disable()` | 禁用TB6612（进入待机模式） |
| 状态检查 | `TB6612_IsInitialized()` | 检查是否已初始化 |

## 重要提醒

1. **配置表驱动**：TB6612配置在`BSP/board.h`的`TB6612_CONFIGS`中定义，支持多实例管理
2. **PWM依赖**：TB6612依赖PWM模块，确保PWM模块已启用（`CONFIG_MODULE_TIMER_ENABLED = 1`）
3. **初始化顺序**：
   - 先初始化PWM模块（如果未初始化）
   - 调用`TB6612_Init()`初始化TB6612
   - **必须调用`TB6612_Enable()`使能TB6612**（否则电机不会工作）
   - 设置方向和速度
4. **STBY引脚**：STBY引脚**必须**连接到STM32的GPIO引脚，**不能**直接接VCC。如果直接接VCC，将无法通过软件控制待机/使能状态
5. **方向控制**：
   - `TB6612_DIR_STOP`：停止（AIN1=0, AIN2=0），速度自动设为0
   - `TB6612_DIR_FORWARD`：正转（AIN1=1, AIN2=0）
   - `TB6612_DIR_BACKWARD`：反转（AIN1=0, AIN2=1）
   - `TB6612_DIR_BRAKE`：制动（AIN1=1, AIN2=1），速度自动设为0
6. **速度控制**：速度范围0.0% ~ 100.0%，速度=0时自动禁用PWM通道（节省功耗），速度>0时自动使能PWM通道
7. **PB3/PB4特殊处理**：如果使用PB3或PB4作为GPIO（AIN1/AIN2/STBY），模块会自动禁用JTAG功能（保留SWD调试）
8. **电源要求**：
   - **VCC**：逻辑电源，3.3V或5V（与STM32共电源）
   - **VM**：马达驱动电源，5V-12V（根据马达规格选择，独立供电）
   - **必须共地**：外部电源的GND必须与STM32的GND连接在一起
9. **错误处理**：所有函数返回`TB6612_Status_t`错误码，需检查返回值

## 配置说明

在`BSP/board.h`中配置TB6612：

```c
#define TB6612_CONFIGS { \
    {GPIOB, GPIO_Pin_3, GPIOB, GPIO_Pin_4, GPIOB, GPIO_Pin_5, 1, 1, 1}, \
    /* TB6612实例1：PB3(AIN1), PB4(AIN2), PB5(STBY), TIM3 CH2(PWMA)，启用 */ \
    {NULL, 0, NULL, 0, NULL, 0, 0, 0, 0}, /* TB6612实例2：未使用，占位 */ \
}
```

**配置参数说明**：
- `ain1_port` / `ain1_pin`：AIN1引脚端口和引脚号（方向控制）
- `ain2_port` / `ain2_pin`：AIN2引脚端口和引脚号（方向控制）
- `stby_port` / `stby_pin`：STBY引脚端口和引脚号（待机控制）
- `pwm_instance`：PWM实例（0=TIM1, 1=TIM3, 2=TIM4）
- `pwm_channel`：PWM通道（0=CH1, 1=CH2, 2=CH3, 3=CH4）
- `enabled`：使能标志（1=启用，0=禁用）

**注意**：配置表中的顺序对应`TB6612_Instance_t`枚举值（0=TB6612_INSTANCE_1, 1=TB6612_INSTANCE_2）。

## 使用示例

```c
/* 初始化TB6612 */
TB6612_Init(TB6612_INSTANCE_1);

/* 使能TB6612（退出待机模式） */
TB6612_Enable(TB6612_INSTANCE_1);

/* 设置方向为正转 */
TB6612_SetDirection(TB6612_INSTANCE_1, TB6612_DIR_FORWARD);

/* 设置速度为50% */
TB6612_SetSpeed(TB6612_INSTANCE_1, 50.0f);

/* 运行一段时间后停止 */
Delay_ms(5000);
TB6612_SetDirection(TB6612_INSTANCE_1, TB6612_DIR_STOP);

/* 禁用TB6612（进入待机模式） */
TB6612_Disable(TB6612_INSTANCE_1);
```

## 硬件连接

**控制信号连接**：
- **PWMA/PWMB**：连接到STM32的PWM输出引脚（如TIM3 CH2）
- **AIN1/AIN2**：连接到STM32的GPIO引脚（方向控制）
- **STBY**：连接到STM32的GPIO引脚（待机控制，**不能直接接VCC**）

**电源连接**：
- **VCC**：逻辑电源，连接到3.3V或5V（与STM32共电源）
- **VM**：马达驱动电源，连接到5V-12V（根据马达规格选择，独立供电）
- **GND**：公共地，**必须与STM32共地**

**马达连接**：
- **AO1/AO2**：连接到马达的两根线（不区分正负极，方向由AIN1/AIN2控制）

## 常见问题

### 马达不转

**排查步骤**：

1. **检查STBY引脚**：用万用表测量STBY引脚，应该为3.3V（高电平）。如果为0V，检查是否调用了`TB6612_Enable()`
2. **检查方向控制引脚**：用万用表测量AIN1和AIN2引脚。正转时：AIN1=3.3V，AIN2=0V；反转时：AIN1=0V，AIN2=3.3V
3. **检查PWM输出**：用示波器测量PWM引脚，应该有PWM波形输出。用万用表测量平均电压，应该与占空比一致
4. **检查电源连接**：检查VCC和VM是否有电压，确保所有GND连接在一起（共地）
5. **检查马达连接**：确认马达的两根线正确连接到AO1和AO2

### 初始化失败

**可能原因**：
1. 配置未启用：检查`board.h`中的`enabled`标志是否为1
2. PWM配置错误：检查PWM实例和通道配置是否正确
3. GPIO配置错误：检查GPIO引脚配置是否正确
4. PWM模块未启用：检查`config.h`中`CONFIG_MODULE_TIMER_ENABLED`是否为1

---

## BTS7960大电流H桥电机驱动模块

### 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始化 | `BTS7960_Init()` | 初始化BTS7960驱动模块（GPIO和PWM，默认PWM频率20kHz） |
| 反初始化 | `BTS7960_Deinit()` | 反初始化BTS7960驱动模块 |
| 设置方向 | `BTS7960_SetDirection()` | 设置电机方向（正转/反转/停止/制动） |
| 设置速度 | `BTS7960_SetSpeed()` | 设置电机速度（PWM占空比，0.0~100.0%） |
| 使能 | `BTS7960_Enable()` | 使能BTS7960（设置R_EN和L_EN同时为高电平） |
| 禁用 | `BTS7960_Disable()` | 禁用BTS7960（设置R_EN和L_EN同时为低电平） |
| 状态检查 | `BTS7960_IsInitialized()` | 检查是否已初始化 |
| 获取电流报警 | `BTS7960_GetCurrentAlarmStatus()` | 获取电流报警状态（读取R_IS/L_IS引脚） |
| 设置报警回调 | `BTS7960_SetCurrentAlarmCallback()` | 设置电流报警回调函数（EXTI中断方式） |
| 使能报警中断 | `BTS7960_EnableCurrentAlarmInterrupt()` | 使能电流报警中断 |
| 禁用报警中断 | `BTS7960_DisableCurrentAlarmInterrupt()` | 禁用电流报警中断 |
| 设置PWM频率 | `BTS7960_SetPWMFrequency()` | 设置PWM频率（1Hz~25kHz，建议20kHz） |
| 获取PWM频率 | `BTS7960_GetPWMFrequency()` | 获取当前PWM频率 |

### 重要提醒

1. **配置表驱动**：BTS7960配置在`BSP/board.h`的`BTS7960_CONFIGS`中定义，支持多实例管理
2. **PWM依赖**：BTS7960依赖PWM模块，确保PWM模块已启用（`CONFIG_MODULE_TIMER_ENABLED = 1`）
3. **初始化顺序**：
   - 先初始化PWM模块（如果未初始化）
   - 调用`BTS7960_Init()`初始化BTS7960（自动设置PWM频率为20kHz）
   - **必须调用`BTS7960_Enable()`使能BTS7960**（设置R_EN和L_EN同时为高电平）
   - 设置方向和速度
4. **使能引脚要求**：**R_EN和L_EN必须同时接高电平才能工作**，即使只单向驱动也必须同时使能
5. **方向控制**：
   - `BTS7960_DIR_STOP`：停止（RPWM=0, LPWM=0）
   - `BTS7960_DIR_FORWARD`：正转（RPWM=速度, LPWM=0）
   - `BTS7960_DIR_BACKWARD`：反转（RPWM=0, LPWM=速度）
   - `BTS7960_DIR_BRAKE`：制动（RPWM=100%, LPWM=100%）
6. **速度控制**：速度范围0.0% ~ 100.0%，速度=0时自动禁用PWM通道（节省功耗），速度>0时自动使能PWM通道
7. **PWM频率限制**：BTS7960要求PWM频率≤25kHz，默认设置为20kHz，可通过`BTS7960_SetPWMFrequency()`修改
8. **电流报警检测**：R_IS/L_IS为输出引脚，过流时输出报警信号，可通过`BTS7960_GetCurrentAlarmStatus()`读取或配置EXTI中断
9. **电源要求**：
   - **VCC**：逻辑电源，5V（与STM32的3.3V逻辑电平不兼容，需要电平转换或使用5V容忍引脚）
   - **工作电压**：6V~27V（典型12V/24V），需要外部电源
   - **必须共地**：外部电源的GND必须与STM32的GND连接在一起
10. **最大电流**：43A（持续工作需加散热片），注意电源和线材选择
11. **应用场景**：智能车、机器人、半导体制冷片驱动等大功率应用
12. **错误处理**：所有函数返回`BTS7960_Status_t`错误码，需检查返回值

### 配置说明

在`BSP/board.h`中配置BTS7960：

```c
#define BTS7960_CONFIGS { \
    {GPIOB, GPIO_Pin_0, GPIOB, GPIO_Pin_1, 1, 1, 1, 2, NULL, 0, NULL, 0, 1}, \
    /* BTS7960实例1：PB0(R_EN), PB1(L_EN), TIM3 CH2(RPWM), TIM3 CH3(LPWM)，启用 */ \
    {NULL, 0, NULL, 0, 0, 0, 0, 0, NULL, 0, NULL, 0, 0}, /* BTS7960实例2：未使用，占位 */ \
}
```

**配置参数说明**：
- `r_en_port` / `r_en_pin`：R_EN引脚端口和引脚号（正转使能）
- `l_en_port` / `l_en_pin`：L_EN引脚端口和引脚号（反转使能）
- `rpwm_instance`：RPWM实例（0=TIM1, 1=TIM3, 2=TIM4）
- `rpwm_channel`：RPWM通道（0=CH1, 1=CH2, 2=CH3, 3=CH4）
- `lpwm_instance`：LPWM实例（0=TIM1, 1=TIM3, 2=TIM4）
- `lpwm_channel`：LPWM通道（0=CH1, 1=CH2, 2=CH3, 3=CH4）
- `r_is_port` / `r_is_pin`：R_IS引脚端口和引脚号（正转电流报警，可选）
- `l_is_port` / `l_is_pin`：L_IS引脚端口和引脚号（反转电流报警，可选）
- `enabled`：使能标志（1=启用，0=禁用）

**注意**：配置表中的顺序对应`BTS7960_Instance_t`枚举值（0=BTS7960_INSTANCE_1, 1=BTS7960_INSTANCE_2）。

### 使用示例

```c
/* 初始化BTS7960 */
BTS7960_Init(BTS7960_INSTANCE_1);

/* 使能BTS7960（设置R_EN和L_EN同时为高电平） */
BTS7960_Enable(BTS7960_INSTANCE_1);

/* 设置方向为正转 */
BTS7960_SetDirection(BTS7960_INSTANCE_1, BTS7960_DIR_FORWARD);

/* 设置速度为50% */
BTS7960_SetSpeed(BTS7960_INSTANCE_1, 50.0f);

/* 运行一段时间后停止 */
Delay_ms(5000);
BTS7960_SetDirection(BTS7960_INSTANCE_1, BTS7960_DIR_STOP);

/* 禁用BTS7960（设置R_EN和L_EN同时为低电平） */
BTS7960_Disable(BTS7960_INSTANCE_1);

/* 电流报警检测示例 */
BTS7960_CurrentAlarmStatus_t alarm_status;
BTS7960_GetCurrentAlarmStatus(BTS7960_INSTANCE_1, &alarm_status);
if (alarm_status.r_is_alarm || alarm_status.l_is_alarm) {
    /* 处理过流报警 */
}
```

### 硬件连接

**控制信号连接**：
- **RPWM/LPWM**：连接到STM32的PWM输出引脚（两个独立的PWM通道）
- **R_EN/L_EN**：连接到STM32的GPIO引脚（使能控制，必须同时接高电平才能工作）
- **R_IS/L_IS**：连接到STM32的GPIO引脚（电流报警输出，可选，用于过流检测）

**电源连接**：
- **VCC**：逻辑电源，5V（与STM32的3.3V逻辑电平不兼容，需要电平转换或使用5V容忍引脚）
- **工作电压**：6V~27V（典型12V/24V），需要外部电源
- **GND**：公共地，**必须与STM32共地**

**马达连接**：
- **输出端**：连接到马达的两根线（不区分正负极，方向由RPWM/LPWM控制）

### 常见问题

#### 马达不转

**排查步骤**：

1. **检查使能引脚**：用万用表测量R_EN和L_EN引脚，应该都为3.3V（高电平）。如果为0V，检查是否调用了`BTS7960_Enable()`
2. **检查PWM输出**：用示波器测量RPWM和LPWM引脚，应该有PWM波形输出。正转时：RPWM有波形，LPWM=0；反转时：RPWM=0，LPWM有波形
3. **检查PWM频率**：确保PWM频率≤25kHz（默认20kHz），可通过`BTS7960_GetPWMFrequency()`检查
4. **检查电源连接**：检查VCC和工作电压是否有电压，确保所有GND连接在一起（共地）
5. **检查马达连接**：确认马达的两根线正确连接到输出端

#### 初始化失败

**可能原因**：
1. 配置未启用：检查`board.h`中的`enabled`标志是否为1
2. PWM配置错误：检查RPWM和LPWM的实例和通道配置是否正确
3. GPIO配置错误：检查R_EN/L_EN引脚配置是否正确
4. PWM模块未启用：检查`config.h`中`CONFIG_MODULE_TIMER_ENABLED`是否为1

#### 电流报警

**排查步骤**：
1. **检查报警状态**：调用`BTS7960_GetCurrentAlarmStatus()`读取R_IS/L_IS引脚状态
2. **检查负载**：确认负载电流是否超过43A（需要加散热片）
3. **检查电源**：确认工作电压是否在6V~27V范围内
4. **配置中断**：如需及时响应过流，可配置EXTI中断检测R_IS/L_IS引脚
