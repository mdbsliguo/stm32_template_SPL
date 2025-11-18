# 案例2 - Delay延时功能测试（带OLED显示�?
测试Delay模块的各种延时功能，包括毫秒、微秒、秒级延时。使用OLED显示当前测试状态，让延时效果更直观�?
---

## 📋 功能说明

- 测试 `Delay_ms()` - 毫秒级延�?- 测试 `Delay_us()` - 微秒级延�?- 测试 `Delay_s()` - 秒级延时
- 演示不同延时时间的效�?- 使用LED可视化延时效�?- **使用OLED显示当前测试状态和延时时间**

---

## 🔧 硬件要求

### 必需硬件
- STM32F103C8T6开发板
- LED1连接到PA1（用于可视化延时效果�?- OLED显示屏（SSD1306，I2C接口�?  - SCL连接到PB8
  - SDA连接到PB9

### 可选硬�?- LED2连接到PA2（用于交替闪烁演示）

### 硬件配置

**案例2提供了专用的硬件配置文件：`board.h`**

**⚠️ 重要说明**�?- `board.h` **只包含本案例需要的配置**（LED配置和OLED配置�?- 由于项目中包�?*所有模块的源文�?*，`案例目录下的 board.h` 必须包含**所有模块的配置**
- 因此，不能直接替�?`案例目录下的 board.h`，需�?*合并配置**

**正确使用方法**�?
**方法A：手动合并配置（推荐，适用于所有情况）**
1. 打开 `案例目录下的 board.h`
2. 找到 `LED_CONFIGS` �?`OLED_I2C_CONFIG` 部分
3. 参�?`Examples/example2_delay_test/board.h` 中的配置
4. 修改 `案例目录下的 board.h` 中的 `LED_CONFIGS` �?`OLED_I2C_CONFIG` 为案�?的配�?5. **保留其他配置不变**（clock_manager等配置必须保留）

**方法B：临时替换（仅用于演示，需确保项目不包含其他模块）**
```powershell
# 备份当前配置（可选）
Copy-Item 案例目录下的 board.h 案例目录下的 board.h.backup

# 使用案例配置（⚠�?注意：如果项目包含其他模块，会编译失败）
Copy-Item Examples\example2_delay_test\board.h 案例目录下的 board.h -Force

# 编译运行后，恢复原配置（如果备份了）
Copy-Item 案例目录下的 board.h.backup 案例目录下的 board.h -Force
```

**配置内容**�?- LED配置（PA1和PA2�?- OLED I2C配置（PB8和PB9�?
**注意**�?- 如果项目包含clock_manager等模块，必须使用方法A（手动合并）
- 如果硬件引脚不同，可以手动修�?`案例目录下的 board.h` 中的引脚配置
- 如果只有一个LED，LED2配置可以禁用（enabled=0�?- 如果OLED初始化失败，LED1会快速闪烁提示错�?
---

## 🚀 使用方法

### 快速开�?
**方法A：手动合并配置（推荐�?*
```powershell
# 1. 复制示例代码
Copy-Item Examples\example2_delay_test\main_example.c Core\main.c -Force

# 2. 手动合并配置（打开案例目录下的 board.h，确认LED和OLED配置正确�?
# 3. 编译运行（在Keil中按F7编译，F8下载�?```

**方法B：临时替换（仅用于演示）**
```powershell
# 1. 备份当前配置（可选）
Copy-Item 案例目录下的 board.h 案例目录下的 board.h.backup

# 2. 使用案例配置（⚠�?注意：如果项目包含其他模块，会编译失败）
Copy-Item Examples\example2_delay_test\board.h 案例目录下的 board.h -Force

# 3. 复制示例代码
Copy-Item Examples\example2_delay_test\main_example.c Core\main.c -Force

# 4. 编译运行（在Keil中按F7编译，F8下载�?
# 5. 恢复原配置（如果备份了）
Copy-Item 案例目录下的 board.h.backup 案例目录下的 board.h -Force
```

**说明**�?- `board.h` 只包含本案例需要的配置，无关配置已移除
- 如果项目包含多个模块，必须使用方法A（手动合并）
- 如果硬件引脚不同，可以手动修�?`案例目录下的 board.h` 中的引脚配置

### 详细操作流程

**通用操作步骤请参�?*：[Examples/README.md](../README.md#-通用操作流程)

---

## 📝 代码说明

### 测试内容

#### 1. 毫秒级延时测试（Delay_ms�?
```c
// 快速闪烁：100ms间隔
LED1_On();
Delay_ms(100);
LED1_Off();
Delay_ms(100);

// 中速闪烁：500ms间隔
LED1_On();
Delay_ms(500);
LED1_Off();
Delay_ms(500);

// 慢速闪烁：1000ms间隔
LED1_On();
Delay_ms(1000);
LED1_Off();
Delay_ms(1000);
```

#### 2. 微秒级延时测试（Delay_us�?
```c
// 快速闪烁：10ms间隔（使用微秒延时）
LED1_On();
Delay_us(10000);  // 10ms = 10000us
LED1_Off();
Delay_us(10000);
```

#### 3. 秒级延时测试（Delay_s�?
```c
// 慢速闪烁：1秒间�?LED1_On();
Delay_s(1);  // 延时1�?LED1_Off();
Delay_s(1);
```

#### 4. 组合测试

主循环中演示不同延时时间的组合效果�?
---

## 🎯 预期效果

### OLED显示内容

- **�?�?*：当前测试阶段（Test 1: ms / Test 2: us / Test 3: s / Test 4: Loop�?- **�?�?*：当前延时时间（Delay: XXXms / XXXus / XXXs�?- **�?�?*：LED状态（LED1: ON / LED1: OFF�?- **�?�?*：测试进度或计数（Count: X�?
### 启动阶段

1. **测试1：毫秒级延时**
   - OLED显示：Test 1: ms
   - 快速闪烁（100ms间隔），OLED显示延时时间
   - 中速闪烁（500ms间隔），OLED显示延时时间
   - 慢速闪烁（1000ms间隔），OLED显示延时时间

2. **测试2：微秒级延时**
   - OLED显示：Test 2: us
   - 快速闪烁（10ms间隔），OLED显示延时时间
   - 中速闪烁（50ms间隔），OLED显示延时时间

3. **测试3：秒级延�?*
   - OLED显示：Test 3: s
   - 慢速闪烁（1秒间隔），OLED显示延时时间

### 主循环（测试4�?
- OLED显示：Test 4: Loop
- 快速闪�?次（100ms），OLED显示计数 �?延时500ms
- 中速闪�?次（500ms），OLED显示计数 �?延时1�?- 如果有LED2：LED1和LED2交替闪烁�?00ms），OLED显示状�?
---

## 🔍 测试要点

### 1. 延时精度测试

- 观察LED闪烁间隔是否准确
- 使用示波器测量实际延时时�?- 验证不同延时时间的准确�?
### 2. 延时范围测试

- **微秒�?*：测�?us ~ 1000us
- **毫秒�?*：测�?ms ~ 10000ms
- **秒级**：测�?s ~ 10s

### 3. 延时稳定性测�?
- 长时间运行，观察延时是否稳定
- 检查是否有累积误差

---

## ⚠️ 注意事项

### 1. 延时精度

- **微秒级延�?*：精度取决于系统时钟频率
- **毫秒级延�?*：通过循环调用微秒延时实现
- **秒级延时**：通过循环调用毫秒延时实现

### 2. 阻塞特�?
- 所有延时函数都�?*阻塞�?*�?- 延时期间CPU不处理其他任�?- 如需非阻塞延时，请使用定时器中断或RTOS

### 3. 延时范围

- **Delay_us()**：范�?0~1864135us（@72MHz�?- **Delay_ms()**：范�?0~4294967295ms（自动分段处理）
- **Delay_s()**：范�?0~4294967295s

---

## 🔧 扩展测试

### 1. 修改延时时间

```c
// 测试不同的延时时�?Delay_ms(50);   // 50ms
Delay_ms(200);  // 200ms
Delay_ms(1000); // 1000ms
```

### 2. 测试延时精度

```c
// 使用示波器测量实际延�?LED1_On();
Delay_us(1000);  // 应该延时1ms
LED1_Off();
```

### 3. 测试长时间延�?
```c
// 测试秒级延时
Delay_s(5);  // 延时5�?```

---

## 📚 相关模块

- **延时功能**：`system/delay.c/h`
- **系统初始�?*：`system/system_init.c/h`
- **LED驱动**：`Drivers/basic/led.c/h`
- **硬件配置**：`案例目录下的 board.h`

---

## 🎓 学习要点

1. **延时函数的使�?*
   - `Delay_ms()` - 最常用
   - `Delay_us()` - 精确延时
   - `Delay_s()` - 长时间延�?
2. **延时精度**
   - 理解系统时钟对延时精度的影响
   - 了解阻塞式延时的特�?
3. **延时应用**
   - LED闪烁控制
   - 按键消抖
   - 时序控制

---

**最后更�?*�?024-01-01

