# 案例1 - 点亮PA1灯、PA2�?
基础的LED控制示例，演示如何使用LED驱动模块控制两个LED交替闪烁�?
---

## 📋 功能说明

- LED1（PA1）和LED2（PA2）交替点�?- 演示LED驱动的基本使�?- 演示系统初始化和延时功能
- 演示LED快捷宏的使用

---

## 🔧 硬件要求

### 必需硬件
- STM32F103C8T6开发板
- LED1连接到PA1
- LED2连接到PA2

### 硬件配置

**⚠️ 重要说明**：案�?是独立工程，硬件配置在案例目录下�?`board.h` 中�?
如果硬件引脚不同，直接修�?`Examples/example1_led_blink/board.h` 中的配置即可�?
�?`board.h` 中配置LED引脚�?
```c
/* LED统一配置�?*/
#define LED_CONFIGS { \
    {GPIOA, GPIO_Pin_1, Bit_RESET, 1},  /* LED1：PA1，低电平点亮，启�?*/ \
    {GPIOA, GPIO_Pin_2, Bit_RESET, 1},  /* LED2：PA2，低电平点亮，启�?*/ \
}
```

**注意**�?- 根据实际硬件修改引脚（`GPIO_Pin_1`、`GPIO_Pin_2`�?- 根据实际硬件修改有效电平（`Bit_RESET`为低电平有效，`Bit_SET`为高电平有效�?- `enabled` �?表示启用该LED，为0表示禁用

---

## 🚀 使用方法

### 快速开�?
1. **检查硬件配�?*：确认案例目录下�?`board.h` 中LED配置正确
2. **复制示例代码**：`cp Examples/example1_led_blink/main_example.c Core/main.c`
3. **编译下载**：在Keil中编译并下载到开发板
4. **观察效果**：LED1和LED2交替闪烁

### 详细操作流程

**通用操作步骤请参�?*：[Examples/README.md](../README.md#-通用操作流程)


---

## 📝 代码说明

### 关键代码

```c
// 系统初始化（自动初始化LED和延时模块）
System_Init();

// LED控制（使用快捷宏�?LED1_On();   // LED1点亮
LED2_Off();  // LED2熄灭
Delay_ms(500);

LED1_Off();  // LED1熄灭
LED2_On();   // LED2点亮
Delay_ms(500);
```

### 完整流程

1. **系统初始�?*：`System_Init()` 自动初始化SysTick和LED驱动
2. **LED控制**：使用快捷宏 `LED1_On()`, `LED1_Off()` 等控制LED
3. **延时**：使�?`Delay_ms()` 实现闪烁间隔

---

## 🎯 预期效果

- LED1和LED2�?00ms间隔交替点亮
- 程序正常运行，无错误

---

## 🔍 扩展练习

1. **修改闪烁频率**：改�?`Delay_ms()` 的参�?2. **同时闪烁**：让两个LED同时点亮/熄灭
3. **流水灯效�?*：添加更多LED，实现流水灯效果
4. **使用函数接口**：不使用快捷宏，改用 `LED_On()`, `LED_Off()` 函数

---

## ⚠️ 常见问题

### LED不亮
- 检查案例目录下�?`board.h` 中的LED配置是否正确
- 检查LED引脚是否与硬件连接一�?- 检查LED有效电平配置（高电平有效/低电平有效）
- 检查LED�?`enabled` 标志是否�?

### 闪烁频率不对
- 检查延时时间是否正�?- 检查系统时钟配�?
### 编译错误
- 确保已包含必要的头文�?- 确保 `System_Init()` 已正确调�?
---

## 📚 相关模块

- **LED驱动**：`Drivers/basic/led.c/h`
- **延时功能**：`system/delay.c/h`
- **系统初始�?*：`system/system_init.c/h`
- **硬件配置**：案例目录下�?`board.h`

---

**最后更�?*�?024-01-01

