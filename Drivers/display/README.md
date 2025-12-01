# 显示驱动模块

## OLED显示模块 (oled_ssd1306)

### 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始化 | `OLED_Init()` | 初始化OLED和I2C接口 |
| 反初始化 | `OLED_Deinit()` | 反初始化OLED |
| 清屏 | `OLED_Clear()` | 清空显示缓冲区 |
| 设置光标 | `OLED_SetCursor()` | 设置显示位置（页、列） |
| 显示字符 | `OLED_ShowChar()` | 显示单个ASCII字符或度符号（°） |
| 显示字符串 | `OLED_ShowString()` | 显示字符串（仅ASCII） |
| 显示无符号数 | `OLED_ShowNum()` | 显示无符号十进制数 |
| 显示有符号数 | `OLED_ShowSignedNum()` | 显示有符号十进制数 |
| 显示十六进制 | `OLED_ShowHexNum()` | 显示十六进制数 |
| 显示二进制 | `OLED_ShowBinNum()` | 显示二进制数 |
| 显示中文字符 | `OLED_ShowChineseChar()` | 显示单个中文字符（16x16点阵，需要fs_wrapper） |
| 显示GB2312字符串 | `OLED_ShowStringGB2312()` | 显示GB2312编码字符串（支持中英文混显） |
| 显示UTF-8字符串 | `OLED_ShowStringUTF8()` | 显示UTF-8编码字符串（支持中英文混显，转换功能有限） |

### 重要提醒

1. **OLED输出规范** `[案例]` `[P1]`
   - OLED输出关键状态和实时数据，全英文（ASCII字符）
   - 禁止使用中文显示
   - 详见[PROJECT_KEYWORDS.md](../../PROJECT_KEYWORDS.md#串口与oled输出分工规范-案例-p1)
   - **注意**：虽然模块支持中文显示功能，但项目规范要求OLED仅显示英文

2. **I2C接口解耦**：OLED模块通过抽象接口使用I2C，支持硬件I2C和软件I2C
3. **接口选择**：根据`BSP/board.h`中的`OLED_I2C_TYPE`配置选择I2C接口类型
4. **字库支持**：
   - **ASCII字库**：使用`oled_font_ascii8x16`字库，支持ASCII字符和度符号（°）显示
   - **中文字库**：使用`oled_font_chinese16x16`字库，支持GB2312字符集（16x16点阵），存储在文件系统中
5. **显示区域**：128x64像素，分为4行
   - ASCII字符：每行16个字符（8x16像素，每字符8像素宽）
   - 中文字符：每行8个字符（16x16像素，每字符16像素宽）
   - 中英文混显：根据字符类型动态计算位置
6. **初始化顺序**：
   - 使用ASCII显示：必须先初始化对应的I2C模块（硬件I2C或软件I2C）
   - 使用中文显示：必须先初始化fs_wrapper模块（`FS_Init()`），然后初始化中文字库模块（`OLED_ChineseFont_Init()`）
7. **字符坐标**：
   - ASCII字符：行号1~4，列号1~16
   - 中文字符：行号1~4，列号1~8（中文字符占用2个ASCII字符宽度）

---

## OLED字库模块 (oled_font_ascii8x16)

### 功能列表

| 功能 | 说明 |
|------|------|
| ASCII字库 | 8x16像素ASCII字符字库数据（含度符号） |

### 重要提醒

1. **字库数据**：提供ASCII字符（0x20~0x7E）和度符号（°）的8x16像素点阵数据，共96个字符
2. **内部使用**：由`oled_ssd1306`模块内部使用，通常不需要直接调用
3. **字符范围**：支持可打印ASCII字符（空格到波浪号）和度符号（°）
4. **度符号使用**：度符号可通过字符码0xB0或176访问，例如：`OLED_ShowChar(1, 1, 0xB0)`

---

## OLED中文字库模块 (oled_font_chinese16x16)

### 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始化 | `OLED_ChineseFont_Init()` | 初始化中文字库模块 |
| 获取字模数据 | `OLED_ChineseFont_GetData()` | 从文件系统读取字模数据 |
| 获取字库索引 | `OLED_ChineseFont_GetIndex()` | 计算GB2312编码对应的字库索引 |
| 检查GB2312编码 | `OLED_ChineseFont_IsValidGB2312()` | 检查GB2312编码是否有效 |

### 重要提醒

1. **字库文件**：中文字库存储在W25Q文件系统的`/font/chinese16x16.bin`文件中
2. **字库格式**：16x16点阵，每个字符32字节，按GB2312索引顺序存储
3. **字库大小**：完整GB2312约250KB，可根据需要选择字符集
4. **内存优化**：默认不使用缓存，字模数据临时存储在栈上（32字节），节省RAM
5. **可选缓存**：通过`#define OLED_CHINESE_FONT_CACHE_ENABLED 1`启用小缓存（1-2个字符，32-64字节）
6. **依赖模块**：需要启用`CONFIG_MODULE_FS_WRAPPER_ENABLED`和`CONFIG_MODULE_LITTLEFS_ENABLED`
7. **初始化顺序**：使用前必须先初始化fs_wrapper模块（`FS_Init()`）

### 使用示例

```c
#include "oled_ssd1306.h"
#include "oled_font_chinese16x16.h"
#include "fs_wrapper.h"

/* 初始化文件系统 */
FS_Init();

/* 初始化中文字库 */
OLED_ChineseFont_Init();

/* 显示中文字符（GB2312编码：0xB6C8 = "温"） */
OLED_ShowChineseChar(1, 1, 0xB6C8);

/* 显示GB2312编码字符串（支持中英文混显） */
OLED_ShowStringGB2312(2, 1, "温度:25°C");  /* 需要GB2312编码字符串 */
```

---

## OLED UTF-8解析模块 (oled_utf8_parser)

### 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 获取字符信息 | `OLED_UTF8_GetNextCharInfo()` | 获取下一个字符的类型和长度 |
| UTF-8转GB2312 | `OLED_UTF8_ToGB2312()` | UTF-8字符转换为GB2312编码（功能有限） |
| 检查ASCII | `OLED_UTF8_IsASCII()` | 检查字符是否为ASCII |
| 检查UTF-8中文 | `OLED_UTF8_IsChineseFirstByte()` | 检查是否为UTF-8中文字符首字节 |
| 检查GB2312 | `OLED_UTF8_IsGB2312()` | 检查两个字节是否为GB2312编码 |

### 重要提醒

1. **字符识别**：自动识别ASCII、UTF-8中文、GB2312中文字符
2. **UTF-8转换**：UTF-8到GB2312转换功能有限（简化实现），建议直接使用GB2312编码字符串
3. **内存占用**：仅使用栈变量，无静态RAM占用
4. **内部使用**：由`oled_ssd1306`模块内部使用，通常不需要直接调用

### 使用示例

```c
#include "oled_ssd1306.h"
#include "oled_utf8_parser.h"

/* 显示UTF-8编码字符串（支持中英文混显） */
/* 注意：UTF-8到GB2312转换功能有限，可能不支持所有字符 */
OLED_ShowStringUTF8(1, 1, "温度:25°C");  /* UTF-8编码字符串 */
```

