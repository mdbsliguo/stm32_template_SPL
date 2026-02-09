# NPN01 - 流量计 NPN 脉冲计数示例

演示使用 EXTI 对 NPN 开漏流量计的脉冲输出进行计数，可选 OLED 显示累计值。

---

## 📋 案例目的
- 使用 EXTI 计数 NPN 流量计脉冲
- 展示上拉输入、下降沿触发、回调计数的用法
- 可选 OLED 显示累计脉冲数

## 🔧 硬件要求
- **流量计（NPN 开漏输出）**：信号线 → `PA0`（EXTI0，下降沿），VCC → 3.3V（或按规格），GND → GND
- **LED1**：`PA1`（状态指示，计数变化时闪烁）
- **OLED（可选）**：SSD1306，软件 I2C，`PB8`(SCL)/`PB9`(SDA)

### 连接示意
| STM32F103C8T6 | 流量计 | 说明 |
|---------------|--------|------|
| PA0 | OUT | NPN 开漏脉冲输出，下降沿触发 |
| 3.3V | VCC | 供电（按传感器规格可为 3.3V/5V） |
| GND | GND | 共地 |

**⚠️ 提示**：PA0 需配置为上拉输入；NPN 导通时拉低，故默认使用“下降沿触发”。若传感器输出为双沿波形，可在 `board.h` 将 `EXTI_TRIGGER_FALLING` 改为 `EXTI_TRIGGER_RISING_FALLING`。

## 🧩 配置说明（位于本目录 `board.h` / `config.h`）
- `LED_CONFIGS`：PA1，低电平点亮，启用。
- `EXTI_CONFIGS`：PA0，下降沿触发，中断模式，启用。
- `OLED_I2C_CONFIG` / `SOFT_I2C_CONFIGS`：PB8/PB9 软件 I2C（可选）。
- `config.h` 模块开关：开启 System_Init、GPIO、Delay、TIM2_TimeBase、LED、EXTI、OLED、SoftI2C、Error_Handler。

## 🚀 使用步骤
1. **工程文件**：规范禁止直接创建 `.uvprojx`。请从 `Examples/EXTI/EXTI01_Infrared_Counter/` 复制 `Examples.uvprojx` 与 `Examples.uvoptx` 到本目录，并在 Keil 中将目标/源路径改为 `NPN01_Flowmeter`。
2. 根据实际硬件修改本目录 `board.h`（引脚/触发方式）与 `config.h`（模块开关）。
3. 打开 Keil 工程，编译（F7）并下载（F8）。

## 🔄 实现流程
1. `System_Init()`：初始化基础模块与 LED。
2. `OLED_Init()`（可选）：清屏并显示标题/计数。
3. `EXTI_HW_Init()`：配置 EXTI0 下降沿中断；随后将 PA0 设为上拉输入。
4. `EXTI_SetCallback()`：回调中仅执行 `g_counter++`。
5. `EXTI_Enable()`：使能中断。
6. 主循环：检测计数变化 → OLED 更新 → LED 闪烁提示；`Delay_ms(10)` 降低占用。注意 OLED 数字显示需满足“列号 + 长度 <= 16”，本例使用 `OLED_ShowNum(2, 6, counter, 10)`。

## 🛠️ 关键点
- 中断回调只做计数，避免耗时操作。
- 需要上拉输入以适配 NPN 开漏；若传感器内置上拉，可改为浮空输入。
- 计数变量使用 `volatile uint32_t`，主循环简单读取；若需一致性快照可在读取时短暂关中断（本示例未开启）。
- 流量换算：在 `main_example.c` 顶部有 `PULSES_PER_LITER_DEFAULT`（本例默认 72 脉冲/L，可按传感器规格修改），主循环用整数计算体积并显示：
  - 第3行：P/L 值（脉冲/升）
  - 第4行：体积 `Vol:LLLLL.mmm`（升.毫升，整数运算）

## ❓ 常见问题
- **无计数/触发不稳**：确认 PA0 上拉，触发沿与传感器输出逻辑匹配；检查共地。
- **高频脉冲丢数**：可改为定时器外部时钟计数（参考 `Examples/Timer/Timer04_DS3231ExternalClock32kHz` 的 ETR 模式）或输入捕获测频。
- **OLED 不显示**：检查 I2C 引脚与上拉电阻，确认已启用 OLED/SoftI2C 模块。

## 🔗 相关参考
- `Examples/EXTI/EXTI01_Infrared_Counter/`（脉冲计数示例）
- `Examples/Timer/Timer04_DS3231ExternalClock32kHz/`（外部时钟计数参考）
- `Drivers/peripheral/exti.h`（EXTI 接口）

---

**最后更新**：2026-02-09
