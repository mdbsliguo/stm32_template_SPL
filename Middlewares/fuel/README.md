# fuel — 定量加油应用数据契约

**模块路径**：`Middlewares/fuel/`  
**适用案例**：`Examples/NPN/Preset_STM32F103ZE`（小精灵 F103ZE 量产定量加油）  
**状态**：数据契约已定稿；状态机 / 持久化读写 / 泵阀业务 **待阶段 6 实现**

---

## 1. 模块定位

本目录定义加油控制器的 **运行态 RAM 结构** 与 **LittleFS 持久化布局**，供 `fuel_app`（应用状态机）及后续 `config_store`、`fuel_checkpoint` 模块使用。

设计原则（与 Preset 架构一致）：

- **停泵判据**只依赖 ISR 脉冲 + RAM，不读 Flash
- **Flash 写入**经 `storage_guard`，降级时不停泵（新开笔策略另定）
- **升数对外保留 2 位小数**，内部定点 `liter_x100`（1 单位 = 0.01 L）

---

## 2. 源文件

| 文件 | 说明 |
|------|------|
| `fuel_types.h` | 状态枚举、故障位、按键事件、升数换算 inline |
| `fuel_persist.h` | `user.dat` / `machine.dat` / `checkpoint.dat` 结构体 |
| `fuel_runtime.h` | `fuel_runtime_t` 运行态与按键合法性 API |
| `fuel_runtime.c` | `Fuel_CanStartNewSession` 等（`CONFIG_MODULE_FUEL_APP_ENABLED=1` 时编译） |

错误码基值：`ERROR_BASE_FUEL`（-4500），见 `Common/error_code.h`。

---

## 3. 主状态与按键

### 3.1 状态机

```text
                    绿键长按 3s（仅准备就绪）
         ┌──────────────────────────────────────┐
         ▼                                      │
    ┌─────────┐   红长按 3s(停止)   ┌──────────┴──────────┐
    │ 准备就绪 │◄──────────────────│       加油中         │
    └────┬────┘                    │  ┌────────┐ ┌──────┐ │
         │                         └──│ 加注中 │ │ 暂停 │─┘
         │                            └───┬────┘ └──▲───┘
    ┌────▼────┐                           │红短按  │绿短按
    │  故障   │◄── 故障条件触发            └───────┘
    └─────────┘
```

| 主状态 `fuel_state_t` | 含义 |
|----------------------|------|
| `FUEL_STATE_READY` | 准备就绪：上一笔结束或红键长按停止后 |
| `FUEL_STATE_FUELING` | 加油中：一笔定量会话进行中 |
| `FUEL_STATE_FAULT` | 故障：有故障码，禁止自动开新笔 |

| 子状态 `fuel_run_substate_t` | 含义 |
|-----------------------------|------|
| `FUEL_RUN_ACTIVE` | 加注中：累计脉冲、可判停泵 |
| `FUEL_RUN_PAUSED` | 暂停：关泵（阀策略在业务层实现） |

### 3.2 按键（绿 / 红各一）

| 事件 | 条件 | 动作 |
|------|------|------|
| 绿短按 | 加油中 **且** 暂停 | 恢复加注 |
| 绿长按 3s | **准备就绪** | 用 `target_next` 开新一笔 |
| 红短按 | 加油中 **且** 加注中 | 暂停 |
| 红长按 3s | **加油中** | 停止本笔 → 准备就绪 |
| 其他 | — | 忽略 |

长按阈值：`FUEL_KEY_LONG_PRESS_MS`（默认 3000 ms），定义于 `board.h`。

### 3.3 上电策略

- 若 `checkpoint` 显示 **本笔未结束** 且掉电前为 **加注中** → 强制 **暂停** + 关泵，保留 `target_session`、`dispensed`、`pulse`
- 用户 **绿短按** 后从已加量继续加至本笔目标

---

## 4. 三个「升数」变量（核心）

| 变量 | 类型 | 含义 | 持久化 |
|------|------|------|--------|
| `target_next_liter_x100` | `uint32_t` | **新一轮加注量** = 用户最后设置；绿键长按开笔时用 | `config/user.dat` |
| `target_session_liter_x100` | `uint32_t` | **本笔目标加注量**；开笔时从 `target_next` 拷贝 | `session/checkpoint.dat` |
| `dispensed_liter_x100` | `uint32_t` | **本笔已加升数**（含 2 位小数） | `session/checkpoint.dat`（每秒） |

关系：

```text
准备就绪 + 绿长按:
  target_session ← target_next
  dispensed / pulse 清零 → 进入加油中/加注中

加油中:
  dispensed 随 OGM 脉冲增长
  target_session 本笔内不变

本笔进行中可改 target_next（只影响下一笔），不改 target_session
```

显示示例：`dispensed_liter_x100 = 1234` → **12.34 L**。

换算（`fuel_types.h`）：

```c
dispensed = Fuel_LiterX100FromPulse(pulse_count, pulses_per_liter);
target_pulse = Fuel_PulseTargetFromLiterX100(target_session, pulses_per_liter, hose_offset);
```

---

## 5. 持久化布局（LittleFS / W25Q）

路径宏定义于 `Examples/NPN/Preset_STM32F103ZE/board.h`：

| 路径 | 结构体 | 更新频率 | 内容 |
|------|--------|----------|------|
| `config/user.dat` | `fuel_user_settings_t` | 用户改定量时 | `target_next_liter_x100` |
| `config/machine.dat` | `fuel_machine_config_t` | 标定 / 运维 | 脉冲系数、补偿、时序、故障阈值等 |
| `session/checkpoint.dat` | `fuel_checkpoint_t` | **加注中每秒** + 事件 | 本笔目标、已加量、脉冲、状态 |

### 5.1 `fuel_user_settings_t`

- Magic `'FUSR'`，version + CRC16
- `target_next_liter_x100`：最后设置的新一轮加注量

### 5.2 `fuel_machine_config_t`

主要字段：

| 字段 | 说明 | 默认参考 |
|------|------|----------|
| `pulses_per_liter` | 脉冲/升 | 145（NPN07） |
| `hose_pulse_offset` | 软管补偿脉冲 | 现场标定 |
| `max_target_liter_x100` | 单笔上限 | 20000（200.00 L） |
| `checkpoint_interval_ms` | 检查点周期 | 1000 ms |
| `pause_on_powerup_active` | 上电若加注中则先暂停 | 1 |
| `vfd_run_freq_x100` | 运行频率 | 500（5.00 Hz） |

### 5.3 `fuel_checkpoint_t`

断电续加依赖字段：

- `session_active`：1 = 本笔未结束
- `target_session_liter_x100`：本笔目标（**断电不丢**）
- `dispensed_liter_x100`：已加量（**每秒刷新**）
- `pulse_count` / `target_pulse`：计量与停泵
- `saved_state` / `saved_sub` / `pause_reason`：恢复状态机
- `rtc_counter`：最后检查点时刻（RTC 秒计数）
- `txn_id`：交易序号

所有持久化块均带 **magic + version + CRC16**；CRC 失败应进 `FUEL_FAULT_CHECKPOINT`。

---

## 6. 运行态 RAM — `fuel_runtime_t`

`fuel_runtime.h` 中定义，为状态机 **唯一权威** 数据源（实现阶段由 `fuel_app` 维护）。

关键字段：

- 三态 + 子状态、`txn_id`
- 三个升数 + `target_pulse`、`pulse_isr` / `pulse_snap`
- `fault_code`、`valve_open`、`pump_running`、`pause_reason`
- `session_active`、`checkpoint_dirty`、`last_checkpoint_ms`
- `key_event`：待处理按键事件
- `user_cache` / `machine_cache` / `checkpoint_cache`：持久化镜像

辅助 API：

- `Fuel_CanStartNewSession` / `Fuel_CanPause` / `Fuel_CanResume` / `Fuel_CanStopSession`

---

## 7. 故障与自动恢复

故障位 `fuel_fault_t`（可组合）：

| 位 | 含义 |
|----|------|
| `FUEL_FAULT_VFD_COMM` | 变频器通信失败 |
| `FUEL_FAULT_VFD_TRIP` | 变频器故障 |
| `FUEL_FAULT_OGM_STALL` | 加注中长时间无脉冲 |
| `FUEL_FAULT_OVER_TARGET` | 超量软件保护 |
| `FUEL_FAULT_STORAGE` | 存储降级 |
| `FUEL_FAULT_CHECKPOINT` | 检查点损坏 |
| … | 见 `fuel_types.h` |

**自动恢复**：周期检测故障条件消失 + 防抖 → 清故障 → 进入 **准备就绪**（不自动回到加注中；断电续加由 checkpoint 单独处理）。

---

## 8. 与 `liter_ring` 分工

| 模块 | 用途 | 频率 |
|------|------|------|
| `checkpoint.dat` | 断电续加、实时升数 | 每秒 + 事件 |
| `liter_ring` | 历史每升明细、审计 | 每满 1.00 L |
| 每笔汇总（待实现） | 红长按停止 / 到量完成 | 每笔一次 |

**不要**为完成标记单独建小文件并频繁 `sync`（Stage4 已验证会触发 MCU 复位）。

---

## 9. 编译开关

在案例 `config.h` 中：

```c
#define CONFIG_MODULE_FUEL_APP_ENABLED  0   /* 阶段 6 前为 0 */
```

启用后需在 Keil 工程中加入 `fuel_runtime.c` 及后续 `fuel_checkpoint.c`、`fuel_config.c` 等。

Include 路径：`..\..\..\Middlewares\fuel`

---

## 10. 待实现（阶段 6+）

### 10.1 代码基线

| 模块 | 复用案例 | 要点 |
|------|----------|------|
| 流量计量 | **NPN06** | `Drivers/.../ogm_flow_ic`；`CONFIG_OGM_FLOW_IC_ALGO_FOUR_EDGE=1`；`OGM_FlowIC_GetCount`；`OGM_Tick1s` / **d/1s**；瞬时 L/min |
| 定量加油 | **NPN07** | USART2 ModBus → GD200A；`0x2001H` 设频、`0x2000H` 启停；上电读 `2100H`；**ISR 累计 Cnt ≥ target_pulse 停泵** |

不采用 NPN06 的 **5→50Hz 分档** 与 **尾段降频**（标定台架专用）。

Preset 相对 NPN07：`PRESET_LITERS=4` 改为 `target_session_liter_x100`；双键与 checkpoint 见上文。

### 10.2 待办清单

- [ ] `fuel_config`：`user.dat` / `machine.dat` 加载、保存、CRC
- [ ] `fuel_checkpoint`：每秒落盘、上电恢复、与 `storage_guard` 联动
- [ ] `fuel_app`：NPN06 计量层 + NPN07 泵控 + 状态机/双键
- [ ] `liter_ring` 记录增加 `rtc_counter` 或时间戳字段
- [ ] 串口 / 网口修改 `target_next` 命令

---

**最后更新**：2026-06-25
