# 案例3 - OLED显示测试

测试OLED模块的各种显示功能，包括字符、字符串、数字等�?
---

## 📋 功能说明

- 测试 `OLED_ShowChar()` - 显示单个字符
- 测试 `OLED_ShowString()` - 显示字符�?- 测试 `OLED_ShowNum()` - 显示无符号十进制�?- 测试 `OLED_ShowSignedNum()` - 显示有符号十进制�?- 测试 `OLED_ShowHexNum()` - 显示十六进制�?- 测试 `OLED_ShowBinNum()` - 显示二进制数

---

## 🔧 硬件要求

### 必需硬件
- STM32F103C8T6开发板
- OLED显示屏（SSD1306，I2C接口�?- SCL连接到PB8
- SDA连接到PB9

### 硬件连接

| OLED引脚 | STM32引脚 | 说明 |
|---------|-----------|------|
| VCC     | 3.3V      | 电源正极 |
| GND     | GND       | 电源负极 |
| SCL     | PB8       | I2C时钟�?|
| SDA     | PB9       | I2C数据�?|

---

## ⚙️ 硬件配置

**案例3提供了专用的硬件配置文件：`board.h`**

**快速使�?*�?```powershell
# 备份当前配置（可选）
Copy-Item 案例目录下的 board.h 案例目录下的 board.h.backup

# 使用案例配置
Copy-Item Examples\example3_oled_test\board.h 案例目录下的 board.h -Force

# 编译运行后，恢复原配置（如果备份了）
Copy-Item 案例目录下的 board.h.backup 案例目录下的 board.h -Force
```

**配置内容**�?- OLED I2C配置（PB8和PB9�?
**注意**�?- `board.h` 只包含本案例需要的配置，复制后即可编译
- 如果硬件引脚不同，可以手动修�?`案例目录下的 board.h` 中的引脚配置
- 确保引脚未被其他功能占用

---

## 🚀 使用方法

### 方法1：使用案例专用配置（推荐用于演示�?
```bash
# 备份当前配置
Copy-Item 案例目录下的 board.h 案例目录下的 board.h.backup

# 使用案例配置（如果有�?# Copy-Item Examples\example3_oled_test\board.h 案例目录下的 board.h -Force

# 复制示例代码
Copy-Item Examples\example3_oled_test\main_example.c Core\main.c -Force

# 编译运行
```

### 方法2：手动修改主配置

1. 根据实际硬件修改 `案例目录下的 board.h` 中的OLED配置
2. 复制示例代码�?`Core/main.c`
3. 编译运行

---

## 📝 代码说明

### 测试内容

#### 1. 显示字符

```c
// 在第1行第1列显示字�?A'
OLED_ShowChar(1, 1, 'A');
```

#### 2. 显示字符�?
```c
// 在第1行第3列显示字符串"HelloWorld!"
OLED_ShowString(1, 3, "HelloWorld!");
```

#### 3. 显示无符号十进制�?
```c
// 在第2行第1列显示十进制�?2345，显示长�?
OLED_ShowNum(2, 1, 12345, 5);
```

#### 4. 显示有符号十进制�?
```c
// 在第2行第7列显示有符号十进制数-66，显示长�?
OLED_ShowSignedNum(2, 7, -66, 2);
```

#### 5. 显示十六进制�?
```c
// 在第3行第1列显示十六进制数0xAA55，显示长�?
OLED_ShowHexNum(3, 1, 0xAA55, 4);
```

#### 6. 显示二进制数

```c
// 在第4行第1列显示二进制�?xAA55，显示长�?6
OLED_ShowBinNum(4, 1, 0xAA55, 16);
```

---

## 🎯 预期效果

OLED显示屏应显示�?
```
Line 1: A HelloWorld!
Line 2: 12345 -66
Line 3: AA55
Line 4: 1010101001010101
```

---

## 🔍 测试要点

### 1. 显示位置

- **行号**�?~4（对应OLED�?行显示区域）
- **列号**�?~16（每行可显示16个字符）

### 2. 字符范围

- 支持ASCII字符：空�?0x20)到波浪号(0x7E)
- 超出范围的字符可能导致显示异�?
### 3. 数字显示

- **无符号数**�?~4294967295
- **有符号数**�?2147483648~2147483647
- **十六进制**�?~0xFFFFFFFF
- **二进�?*�?~0xFFFF（length<=16�?
---

## ⚠️ 注意事项

### 1. I2C引脚配置

- 必须配置�?*开漏输�?*模式
- 需要上拉电阻（通常OLED模块已内置）

### 2. 初始化顺�?
- 必须先调�?`OLED_Init()` 才能使用其他函数
- 初始化失败会返回错误�?
### 3. 显示刷新

- 每次调用显示函数都会立即更新OLED
- 无需手动刷新

### 4. 清屏

- 使用 `OLED_Clear()` 清空整个屏幕
- 清屏后所有内容变为空�?
---

## 🔧 扩展测试

### 1. 动态显�?
```c
uint32_t counter = 0;
while (1)
{
    OLED_ShowNum(1, 1, counter, 5);
    counter++;
    Delay_ms(1000);
}
```

### 2. 多行显示

```c
OLED_ShowString(1, 1, "Line 1");
OLED_ShowString(2, 1, "Line 2");
OLED_ShowString(3, 1, "Line 3");
OLED_ShowString(4, 1, "Line 4");
```

### 3. 格式化显�?
```c
// 显示温度�?OLED_ShowString(1, 1, "Temp:");
OLED_ShowNum(1, 7, 25, 2);
OLED_ShowChar(1, 9, 'C');
```

---

## 📚 相关模块

- **OLED驱动**：`Drivers/display/oled_ssd1306.c/h`
- **OLED字库**：`Drivers/display/oled_font_ascii8x16.c/h`
- **GPIO驱动**：`Drivers/basic/gpio.c/h`
- **系统初始�?*：`system/system_init.c/h`
- **硬件配置**：`案例目录下的 board.h`

---

## 🎓 学习要点

1. **I2C通信**
   - 软件I2C实现
   - 起始/停止信号
   - 数据发送时�?
2. **OLED驱动**
   - SSD1306初始化序�?   - 页地址和列地址设置
   - 字模数据发�?
3. **显示控制**
   - 光标位置设置
   - 字符/字符串显�?   - 数字格式化显�?
---

**最后更�?*�?024-01-01

