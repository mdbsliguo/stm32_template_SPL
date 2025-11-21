# 案例4 - delay模块手动调频测试

测试delay模块在不同频率下的延时准确性，验证频率变化�?秒永远是1秒�?
**模块说明**�?- **TIM2_TimeBase**：TIM2外设定时器，提供1ms时间基准，频率切换时自动适配
- **delay**：阻塞式延时（基于SysTick�? 非阻塞式延时（基于TIM2_TimeBase），频率切换时自动适配
- **clock_manager**：时钟管理模块，支持手动调频（本案例使用�?
---

## 📋 功能说明

- 测试 `Delay_ms()` 在不同频率下的准确�?- 测试 `Delay_ms_nonblock()` 在不同频率下的准确�?- 验证频率切换时，延时时间是否保持准确�?秒永远是1秒）
- 使用LED可视化延时效果和频率状�?
---

## 🔧 硬件要求

### 必需硬件
- STM32F103C8T6开发板
- LED1连接到PA1（用于可视化延时效果�?- LED2连接到PA2（用于显示当前频率状态）
- OLED显示屏（SSD1306，I2C接口�?  - SCL连接到PB8
  - SDA连接到PB9

### 硬件配置

**案例4提供了专用的硬件配置文件：`board.h`**

**快速使�?*�?```powershell
# 备份当前配置（可选）
Copy-Item 案例目录下的 board.h 案例目录下的 board.h.backup

# 使用案例配置
Copy-Item Examples\example4_delay_freq_test\board.h 案例目录下的 board.h -Force

# 编译运行后，恢复原配置（如果备份了）
Copy-Item 案例目录下的 board.h.backup 案例目录下的 board.h -Force
```

**配置内容**�?- LED配置（PA1和PA2�?- OLED I2C配置（PB8和PB9�?
**注意**�?- `board.h` 只包含本案例需要的配置，复制后即可编译
- 如果硬件引脚不同，可以手动修�?`案例目录下的 board.h` 中的引脚配置

---

## 🚀 使用方法

### 方法1：使用案例专用配置（推荐用于演示�?
```bash
# 备份当前配置
Copy-Item 案例目录下的 board.h 案例目录下的 board.h.backup

# 使用案例配置
Copy-Item Examples\example4_delay_freq_test\board.h 案例目录下的 board.h -Force

# 复制示例代码�?Core/main.c
Copy-Item Examples\example4_delay_freq_test\main_example.c Core\main.c -Force

# 在Keil中编译并下载
```

### 方法2：手动修改配�?
1. 打开 `案例目录下的 board.h`
2. 修改 `LED_CONFIGS` 为案例配�?3. 复制 `Examples/example4_delay_freq_test/main_example.c` �?`Core/main.c`
4. 编译并下�?
---

## 📊 测试内容

### 测试1：不同频率下的阻塞式延时

- 依次切换到：72MHz �?48MHz �?24MHz �?8MHz
- 在每个频率下测试500ms延时
- **OLED显示**�?  - �?行：`Test1:Block Delay`
  - �?行：`Freq: XXMHz`（实时显示当前频率）
  - �?行：`Status:Testing` / `LED1 ON` / `LED1 OFF`
- **预期效果**：LED1在每个频率下都闪�?00ms，频率变化不影响延时时间

### 测试2：频率切换时的非阻塞式延�?
- LED1�?000ms�?秒）闪烁一�?- LED2�?00ms闪烁一�?- �?秒自动切换一次频率（72MHz �?48MHz �?24MHz �?8MHz �?循环�?- **OLED显示**（每500ms更新一次）�?  - �?行：`Test2:NonBlock`
  - �?行：`Freq: XXMHz`（实时显示当前频率）
  - �?行：`LED1:1s LED2:200ms` + LED状态（ON/OFF�?  - �?行：`Switch: Xs`（显示距离下次频率切换的倒计时）
- **预期效果**�?  - LED1始终1秒闪烁一次，不受频率变化影响
  - LED2始终200ms闪烁一次，不受频率变化影响
  - 频率切换时，LED闪烁节奏保持不变
  - OLED上显示的频率应该与实际频率一�?
---

## 🔍 测试要点

### 1. 阻塞式延时准确�?
**验证方法**�?- 观察LED1在每个频率下的闪烁间�?- 所有频率下，LED1的闪烁间隔应该相同（500ms�?
**判断标准**�?- �?正常：所有频率下LED1闪烁间隔相同
- �?异常：不同频率下LED1闪烁间隔不同

### 2. 非阻塞式延时准确�?
**验证方法**�?- 观察LED1和LED2的闪烁频�?- 频率切换时，LED闪烁节奏应该保持不变

**判断标准**�?- �?正常：频率切换时，LED1始终1秒闪烁一次，LED2始终200ms闪烁一�?- �?异常：频率切换时，LED闪烁节奏发生变化

### 3. 频率切换稳定�?
**验证方法**�?- 观察频率切换时系统是否稳�?- 观察LED是否出现异常闪烁

**判断标准**�?- �?正常：频率切换平滑，LED闪烁正常
- �?异常：频率切换时系统卡顿或LED异常

---

## 📈 预期效果

### 初始化阶�?
- OLED显示：`Delay Freq Test` �?`Init OK`�?秒后进入测试�?
### 测试1阶段（阻塞式延时测试�?
1. **72MHz**�?   - OLED显示：`Freq: 72MHz` �?`LED1 ON` �?`LED1 OFF`
   - LED2点亮 �?LED1闪烁500ms �?LED2熄灭
2. **48MHz**�?   - OLED显示：`Freq: 48MHz` �?`LED1 ON` �?`LED1 OFF`
   - LED2点亮 �?LED1闪烁500ms �?LED2熄灭
3. **24MHz**�?   - OLED显示：`Freq: 24MHz` �?`LED1 ON` �?`LED1 OFF`
   - LED2点亮 �?LED1闪烁500ms �?LED2熄灭
4. **8MHz**�?   - OLED显示：`Freq:  8MHz` �?`LED1 ON` �?`LED1 OFF`
   - LED2点亮 �?LED1闪烁500ms �?LED2熄灭

**关键观察�?*�?- 所有频率下，LED1的闪烁间隔应该相同（500ms�?- OLED上显示的频率应该与实际频率一�?
### 测试2阶段（非阻塞式延�?+ 频率切换�?
- **OLED显示**（每500ms更新）：
  - �?行：`Freq: XXMHz`（实时显示当前频率，�?秒切换一次）
  - �?行：`LED1:1s LED2:200ms` + LED状态（ON/OFF�?  - �?行：`Switch: Xs`（倒计时，5秒→0秒循环）
- **LED行为**�?  - **LED1**：始�?秒闪烁一次（无论频率如何变化�?  - **LED2**：始�?00ms闪烁一次（无论频率如何变化�?- **频率切换**：每5秒切换一次（72MHz �?48MHz �?24MHz �?8MHz �?循环�?
**关键观察�?*�?- 频率切换时，LED1和LED2的闪烁节奏应该保持不�?- OLED上显示的频率应该与实际频率一�?- 倒计时应该准确（5秒→0秒循环）

---

## ⚠️ 注意事项

### 1. 频率切换间隔

- 频率切换需要一定时间（�?00ms�?- 切换后需要等待稳定，再开始测�?
### 2. 延时精度

- **阻塞式延�?*（Delay_us/Delay_ms）：基于SysTick寄存器，频率切换时自动适配系数，确保延时准�?- **非阻塞式延时**（Delay_ms_nonblock）：基于TIM2_TimeBase�?ms中断，频率切换时自动适配，确�?秒永远是1�?
### 3. 观察方法

- 建议使用秒表或手机计时器辅助观察
- 重点观察频率切换时，LED闪烁节奏是否保持不变

---

## 🔧 扩展测试

### 测试更多频率档位

修改测试频率列表，测试所�?个档位：

```c
CLKM_FreqLevel_t test_freqs[] = {
    CLKM_LVL_72MHZ,  /* 72MHz */
    CLKM_LVL_64MHZ,  /* 64MHz */
    CLKM_LVL_56MHZ,  /* 56MHz */
    CLKM_LVL_48MHZ,  /* 48MHz */
    CLKM_LVL_40MHZ,  /* 40MHz */
    CLKM_LVL_32MHZ,  /* 32MHz */
    CLKM_LVL_24MHZ,  /* 24MHz */
    CLKM_LVL_16MHZ,  /* 16MHz */
    CLKM_LVL_8MHZ,   /* 8MHz */
};
```

### 测试微秒级延�?
添加微秒级延时测试：

```c
/* 在不同频率下测试微秒级延�?*/
LED1_On();
Delay_us(10000);  // 10ms
LED1_Off();
Delay_us(10000);
```

---

## 📝 测试报告模板

### 测试结果记录

| 频率 | 阻塞式延时（500ms�?| 非阻塞式延时�?000ms�?| 备注 |
|------|-------------------|---------------------|------|
| 72MHz | �?正常 | �?正常 | |
| 48MHz | �?正常 | �?正常 | |
| 24MHz | �?正常 | �?正常 | |
| 8MHz | �?正常 | �?正常 | |

### 测试结论

- �?阻塞式延时：所有频率下延时准确
- �?非阻塞式延时：频率切换时延时准确
- �?频率切换：系统稳定，延时不受影响

---

**最后更�?*�?024-01-01

