# NPN07 - 预设加油泵 4L（小精灵 STM32F103ZE）

在 [NPN06](../NPN06_Preset_Pump_HwAlgo_STM32F103ZE/README.md) 同款 **F103ZE 小精灵板** 上，将应用从 **d/1s 分档标定测试** 改为 **量产向预设加油**：按 **PA4** 启动，固定 **5Hz** 运行，累计 **580 cnt**（145 脉冲/升 × 4 升）后自动停泵。

**与 NPN06 的关系**：板级接线、OGM 驱动、ModBus 泵控、OLED 显示框架 **完全相同**；仅 `main_example.c` 业务逻辑与 `board.h` 中 `OGM_PULSE_FACTOR` 不同。

---

## 📋 案例目的

### 功能说明

- **OGM 计量**：TIM3 CH1/CH2（**PA6 / PA7**）硬件双边沿输入捕获 + ISR **四边沿互锁**（`CONFIG_OGM_FLOW_IC_ALGO_FOUR_EDGE=1`，语义同 NPN05/NPN06）
- **预设加油**：**PA4** 启动键；每次加油目标 **4L**；停泵判据 `Cnt >= 580`
- **标定系数**：**145 脉冲/升**（`PULSES_PER_LITER`）；`board.h` 中 `OGM_PULSE_FACTOR = 1/145` 供瞬时流量 L/min 换算
- **固定泵频**：**5Hz**（`PUMP_FREQ_DEFAULT_HZ`）；**无** NPN06 的 5→50Hz 分档与尾段降频
- **ModBus 控泵**：GD200A，`0x2001H` 设频、`0x2000H` 启停（启动默认反转 `0x0002`）
- **上电流程**：485 预检 → 读 `2100H` 若在运行则停机 → 写 5Hz 待机（**不启泵**）

### 与 NPN05 / NPN06 对照

| 项目 | NPN05（C8） | NPN06（ZE 标定） | **NPN07（ZE 4L）** |
|------|-------------|------------------|-------------------|
| 芯片 | F103C8，`STM32F10X_MD` | F103ZE，`STM32F10X_HD` | **同 NPN06** |
| OGM 引脚 | TIM4 PB6/PB7 | TIM3 PA6/PA7 | **同 NPN06** |
| 启动键 | PA6 | PA4 | **PA4** |
| OLED | 软 I2C PB8/PB9 | 硬 I2C2 PB10/PB11 | **同 NPN06** |
| 固件模式 | 5→50Hz 分档测试 | 5→50Hz 分档测试 | **PA4 加 4L** |
| 停泵目标 | 2973 cnt/档 | 2973 cnt/档 | **580 cnt（4L）** |
| 脉冲系数 | 标定占位 | 标定占位 | **145 /L（已写入）** |
| 尾段降频 | 有（≥10Hz） | 有 | **无** |
| 默认泵频 | 5Hz 起档 | 5Hz 起档 | **固定 5Hz** |

### 定位说明

| 案例 | 用途 |
|------|------|
| **NPN06** | 多泵频 d/1s 标定、尾段策略验证 |
| **NPN07（本案例）** | 现场 **单次 4L 预设加油**；系数 145/L 已固化，可直接联机试加 |
| NPN05 | C8 板参考；计量算法与驱动同源 |

---

## 🔢 计数与停泵规则

```
PRESET_CNT_TARGET = PULSES_PER_LITER × PRESET_LITERS
                  = 145 × 4
                  = 580
```

| 参数 | 宏 / 值 | 说明 |
|------|---------|------|
| 每升脉冲 | `PULSES_PER_LITER = 145` | 四边沿互锁下的体积标定系数 |
| 预设升数 | `PRESET_LITERS = 4` | 单次加油量 |
| 停泵计数 | `PRESET_CNT_TARGET = 580` | `Cnt` 达到即停机 |
| 运行泵频 | `PUMP_FREQ_DEFAULT_HZ = 5` | 全程 5Hz，不变档 |

**注意：**

- 停泵依据 **ISR 累计 Cnt**，不是读 TIM `CNT` 寄存器
- `145/L` 须以本管路、本 OGM、本算法（四边沿互锁）**现场标定**；改系数时须同时改 `PULSES_PER_LITER` 与 `board.h` 的 `OGM_PULSE_FACTOR`
- 5Hz 稳态下 d/1s 参考约 **28~29**（与 NPN05/NPN06 同条件），整次 4L 耗时约 **20~25s**（视管路阻力而定）

---

## 🔧 硬件接线

与 NPN06 完全一致：

| 功能 | 引脚 | 说明 |
|------|------|------|
| OGM A / B | **PA6 / PA7** | TIM3 CH1/CH2，双边沿捕获 + 四边沿互锁 |
| 485② 变频器 | PA2 / PA3 | USART2，19200 8E1，ModBus 地址 1 |
| TTL 调试 | PA9 / PA10 | USART1，115200 8N1 |
| OLED | PB10 / PB11 | 硬件 I2C2（SSD1306） |
| 启动键 | **PA4** | 上拉，按下接 GND |
| LED | PC4 | 低电平点亮；485 异常快闪，有流量慢闪 |
| GND | — | OGM、RS485、变频器信号地 **必须共地** |

**OGM 接线注意：**

- 双 NPN 开漏；MCU 内上拉，线长时建议外接 4.7k~10kΩ 到 3.3V
- 小精灵板 OGM 插座为 **PA6/PA7**（编码器②），与 NPN05 的 PB6/PB7 **不同物理口**

---

## 🎮 操作（4L 预设加油）

1. 上电 → OLED 待机 `PA4:4L 145/L idle`，485 预检通过则 `485:OK`
2. 按 **PA4** → 写 5Hz 并启泵反转，`Cnt` **清零** 开始计量
3. 运行中 OLED 第 2 行：`4L F:05 T:0580`
4. `Cnt` 达到 **580** → 自动发停机命令
5. 回到待机；可 **再次按 PA4** 重复下一次 4L

**按键说明：**

| 按键 | 本固件 |
|------|--------|
| **PA4** | 启动一次 4L 加油（运行中忽略） |
| PA5 | 未引出，无效 |

**与 NPN06 的区别**：无频率档位切换、无尾段 5Hz 切换；每次按键都是相同的 4L / 580 cnt 任务。

---

## 📺 OLED 显示

| 行 | 待机 | 运行中 |
|----|------|--------|
| 1 | `Cnt:00000000` | 实时累计计数 |
| 2 | `PA4:4L 145/L idle` | `4L F:05 T:0580` |
| 3 | `d/1s:000000` | 最近约 1s 计数增量 |
| 4 | `485:OK    0s` | `485:OK  NNNs`（本次运行秒数） |

### 读数说明

| 显示项 | 含义 |
|--------|------|
| **Cnt** | 四边沿互锁累计；**≥580 即停泵** |
| **d/1s** | 瞬时速率；5Hz 稳态参考约 28~29 |
| **T:0580** | 本次目标计数（固定） |
| **秒数** | 本次 PA4 启动到停机的耗时 |

---

## 📦 模块与工程

### config.h 要点

| 宏 | 值 | 说明 |
|----|-----|------|
| `CONFIG_MODULE_OGM_FLOW_IC_ENABLED` | 1 | OGM 输入捕获 |
| `CONFIG_MODULE_I2C_ENABLED` | 1 | 硬件 I2C（OLED） |
| `CONFIG_MODULE_SOFT_I2C_ENABLED` | 0 | 不用软 I2C |
| `CONFIG_OGM_FLOW_IC_ALGO_FOUR_EDGE` | 1 | 四边沿互锁 |

### board.h 要点（NPN07 特有）

| 宏 | 值 | 说明 |
|----|-----|------|
| `OGM_FLOW_IC_INSTANCE` | 1（TIM3） | 同 NPN06 |
| `OGM_CH_A/B` | PA6 / PA7 | 同 NPN06 |
| `OGM_PULSE_FACTOR` | **`1.0f / 145.0f`** | 与 `PULSES_PER_LITER` 一致 |
| `OLED_I2C_TYPE` | 硬件 I2C2 | 同 NPN06 |

### 目录结构

```text
NPN07_Preset_Pump_HwAlgo_STM32F103ZE_4L/
├── main_example.c      # 4L 预设业务逻辑
├── board.h             # 硬件表 + OGM_PULSE_FACTOR
├── board_early_init.c  # JTAG 释放
├── config.h            # 模块开关
├── Examples.uvprojx    # Keil 工程（须改 Output 名）
└── README.md
```

### 编译烧录

1. 打开 `Examples/NPN/NPN07_Preset_Pump_HwAlgo_STM32F103ZE_4L/Examples.uvprojx`
2. 在 Keil 中将 **TargetName / OutputName** 改为 `NPN07_Preset_Pump_HwAlgo_STM32F103ZE_4L`（若仍为 NPN06 名称）
3. 确认 Device：`STM32F103ZE`，宏 `STM32F10X_HD`
4. **Rebuild all** → 烧录 `Build/Keil/Objects/NPN07_Preset_Pump_HwAlgo_STM32F103ZE_4L.hex`

### 上电串口预期（USART1，115200）

```text
=== NPN07 Preset Pump 4L F103ZE ===
OGM: TIM3 IC 4-edge lock PA6/PA7
PA4 start: 4L x 145/L = 580 cnt
上电设频 5 Hz（待机，按 PA4 加 4L）
PA4=启动 4L；580 cnt 自动停，可重复按键
```

按键启动后：

```text
4L 预设启动 5 Hz，目标 580 cnt (145/L x 4L)
已达 580 cnt（4L），停机；再按 PA4 可重复
```

---

## ⚙️ 变频器参数（最小必查）

与 NPN05/NPN06 相同，须与代码 **19200 8E1、地址 1** 一致。详细见 [NPN05 README 变频器章节](../NPN05_Preset_Pump_HwAlgo/README.md) 或 `Examples/Bus/Bus04_ModBusRTU_Invt_GD200A/`。

| 参数 | 建议值 |
|------|--------|
| 波特率 | 19200 |
| 数据位 / 校验 / 停止位 | 8E1 |
| 从站地址 | 1 |
| 通讯启停 | 启用（P14 等，按现场手册） |

---

## ⚙️ 可调宏（`main_example.c`）

| 宏 | 当前值 | 说明 |
|----|--------|------|
| `PULSES_PER_LITER` | **145** | 每升脉冲数；改后须同步 `board.h` 的 `OGM_PULSE_FACTOR` |
| `PRESET_LITERS` | **4** | 预设升数 |
| `PRESET_CNT_TARGET` | **580**（自动计算） | 停泵目标，一般勿手改 |
| `PUMP_FREQ_DEFAULT_HZ` | **5** | 运行泵频；可改为 10/15 等，但 145/L 须按新泵频复标 |

---

## ✅ 测试清单

- [ ] 上电 OLED：`PA4:4L 145/L idle`、`485:OK`
- [ ] 串口打印 `NPN07 Preset Pump 4L`、`580 cnt`
- [ ] 按 PA4：5Hz 启泵，`Cnt` 从 0 递增
- [ ] 运行中第 2 行：`4L F:05 T:0580`
- [ ] `Cnt` 达 **580** 自动停机，串口有「已达 580 cnt（4L）」
- [ ] 停机后可再按 PA4 重复，每次 `Cnt` 重新清零
- [ ] 5Hz 稳态 `d/1s` 约 28~29（与 NPN06 同管路 ±5%）
- [ ] 实加 4L 称重验证（地磅/量杯）；偏差大时调整 `PULSES_PER_LITER`
- [ ] `OGM_FlowIC_Init` 失败时 LED（PC4）快闪

---

## ❓ 常见问题

### Cnt 到不了 580 就停或一直不停

- 查 OGM 接线 PA6/PA7、共地、NPN 传感器供电
- 查变频器是否在运行（读 `2100H`）
- 串口是否有 `OGM FlowIC init fail`

### 实加油量与 4L 偏差大

- **145/L** 可能不适合当前管路；用 NPN06 分档标定后重算系数
- 勿混用 NPN04 下降沿算法系数（约为四边沿一半）
- 改 `PULSES_PER_LITER` 后必须改 `OGM_PULSE_FACTOR` 并重新编译

### 烧录后串口仍显示 NPN06

- 烧错了 hex；确认 Keil **OutputName** 与烧录文件路径
- 本目录 `Examples.uvprojx` 若未改名，输出可能仍为 `NPN06_...hex`

### 按 PA4 无反应

- 485 预检失败时日志有 `485 未就绪`；查 RS485 接线与变频器参数
- 运行中按 PA4 **不会**重复启动（须等本次 580 cnt 停机）

---

## 📝 修订记录

| 日期 | 说明 |
|------|------|
| 2026-06-27 | 标定系数改为 **145 脉冲/升**，停泵目标 **580 cnt** |
| 2026-06-27 | 完善文档：案例定位、580 cnt 规则、OLED/串口、测试清单、FAQ |
| 2026-06-27 | 由 NPN06 派生：4L 预设（145/L×4）、固定 5Hz、去除分档测试 |

---

## 🔗 相关案例

| 案例 | 说明 |
|------|------|
| [NPN06](../NPN06_Preset_Pump_HwAlgo_STM32F103ZE/README.md) | 同板 d/1s 分档标定（2973 cnt/档） |
| [NPN05](../NPN05_Preset_Pump_HwAlgo/README.md) | C8 主参考；计量原理与标定表 |
| `Drivers/timer/ogm_flow_ic.c` | 共用 OGM 硬件捕获驱动 |
| `Examples/Bus/Bus04_ModBusRTU_Invt_GD200A` | GD200A 变频器通讯 |

---

**最后更新**：2026-06-27
