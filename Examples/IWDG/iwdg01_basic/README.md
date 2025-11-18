# 案例 - 独立看门狗基础使用

✅ **已创建**

## 📋 功能说明

本案例演示如何使用**独立看门狗（IWDG）**模块，展示IWDG的初始化、启用和喂狗操作。

### 核心功能

1. **IWDG初始化**
   - 演示IWDG模块的初始化步骤
   - 使用默认配置（1000ms超时）

2. **IWDG启用**
   - 演示如何启用看门狗
   - 启用后必须定期喂狗，否则系统会复位

3. **IWDG喂狗**
   - 在主循环中定期调用`IWDG_Refresh()`喂狗
   - 显示喂狗计数和状态

4. **OLED显示**
   - 显示看门狗超时时间
   - 显示看门狗状态
   - 显示喂狗计数

## 🔧 硬件要求

- **LED1**：连接到 `PA1`（系统状态指示）
- **OLED显示屏**（SSD1306，I2C接口）：
  - SCL：`PB8`（软件I2C）
  - SDA：`PB9`（软件I2C）
  - VCC：3.3V
  - GND：GND

## 📦 模块依赖

### 必需模块

- `iwdg`：独立看门狗模块（核心）
- `oled`：OLED显示模块（默认显示器）
- `soft_i2c`：软件I2C驱动模块（OLED使用）
- `gpio`：GPIO驱动模块（LED、I2C依赖）
- `led`：LED驱动模块（状态指示）
- `delay`：延时模块
- `system_init`：系统初始化模块

## 🚀 使用步骤

### 步骤1：硬件连接

1. 将OLED显示屏连接到STM32：
   - OLED SCL → STM32 PB8
   - OLED SDA → STM32 PB9
   - OLED VCC → 3.3V
   - OLED GND → GND

2. 将LED1连接到STM32：
   - LED1 → STM32 PA1

### 步骤2：配置模块开关

在 `config.h` 中启用必要模块（已配置）：

```c
#define CONFIG_MODULE_IWDG_ENABLED         1   /* 启用IWDG模块 */
#define CONFIG_MODULE_OLED_ENABLED         1   /* 启用OLED模块 */
```

### 步骤3：配置硬件

在 `board.h` 中配置OLED和LED（已配置）：

```c
/* OLED I2C配置 */
#define OLED_I2C_CONFIG {      \
    GPIOB,                     \
    GPIO_Pin_8, /* SCL: PB8 */ \
    GPIOB,                     \
    GPIO_Pin_9, /* SDA: PB9 */ \
}

/* LED配置 */
#define LED_CONFIGS {                                                    \
    {GPIOA, GPIO_Pin_1, Bit_RESET, 1}, /* LED1：PA1，低电平点亮，启用 */ \
}
```

### 步骤4：初始化流程

```c
/* 1. 系统初始化 */
System_Init();

/* 2. OLED初始化 */
OLED_Init();

/* 3. IWDG初始化（使用默认配置：1000ms超时） */
IWDG_Init(NULL);

/* 4. 启用看门狗 */
IWDG_Start();
```

## 📝 代码说明

### 主循环

```c
while (1)
{
    /* 喂狗（刷新看门狗计数器） */
    IWDG_Refresh();
    
    /* LED闪烁指示系统运行 */
    LED1_On();
    Delay_ms(100);
    LED1_Off();
    
    /* 延时900ms（小于看门狗超时时间1000ms） */
    Delay_ms(900);
}
```

### 测试看门狗复位

要测试看门狗复位功能，可以注释掉`IWDG_Refresh()`调用：

```c
while (1)
{
    /* IWDG_Refresh();  // 注释掉喂狗操作 */
    
    LED1_On();
    Delay_ms(100);
    LED1_Off();
    Delay_ms(900);
    
    /* 系统会在1秒后复位（看门狗超时） */
}
```

## ⚠️ 注意事项

1. **看门狗启用后无法禁用**
   - 一旦调用`IWDG_Start()`启用看门狗，只能通过系统复位禁用
   - 必须在看门狗超时前定期调用`IWDG_Refresh()`喂狗

2. **喂狗间隔**
   - 喂狗间隔必须小于看门狗超时时间
   - 建议喂狗间隔为超时时间的50%-80%

3. **超时时间配置**
   - 超时时间范围：1ms ~ 32768ms
   - 默认超时时间：1000ms（在`config.h`中配置）

4. **测试复位功能**
   - 测试时，可以注释掉`IWDG_Refresh()`调用
   - 系统会在超时后自动复位

## 🔍 预期效果

### OLED显示内容

- **第1行**：`IWDG Demo`
- **第2行**：`Timeout: 1000ms`
- **第3行**：`IWDG Started`
- **第4行**：`Refresh: XXXX`（喂狗计数，每秒递增）

### LED行为

- LED1每1秒闪烁一次（100ms亮，900ms灭）
- 如果看门狗复位，LED会重新开始闪烁

## 📚 相关模块

- **独立看门狗**：`system/iwdg.c/h`
- **OLED驱动**：`Drivers/display/oled_ssd1306.c/h`
- **LED驱动**：`Drivers/basic/led.c/h`
- **系统初始化**：`system/system_init.c/h`

---

**最后更新**：2024-01-01

