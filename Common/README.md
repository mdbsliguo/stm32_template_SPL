# common - 公共模块

公共模块，提供跨模块使用的通用功能和工具，包括错误码定义、错误处理等。

---

## 📋 目录说明

```
common/
├── error_code.h         # 全局错误码定义
└── error_handler.c/h   # 统一错误处理
```

---

## 🎯 职责

1. **错误码系统**：统一的错误码类型定义，模块错误码基值定义，错误码规范
2. **错误处理**：错误码到字符串转换，错误回调机制，错误日志记录，错误统计功能（可选）

---

## 📝 功能列表

| 模块 | 主要功能 | 状态 |
|------|----------|------|
| error_code | 全局错误码定义，模块错误码基值定义，错误码规范 | ✅ 已实现 |
| error_handler | 错误码到字符串转换，错误回调机制，错误处理统一接口，日志系统集成，错误统计功能（可选），配置管理 | ✅ 已实现 |

---

## 📊 已支持的模块错误码

错误处理模块已支持以下模块的错误码转换：

| 模块 | 基值 | 状态 |
|------|------|------|
| OLED | ERROR_BASE_OLED (-100) | ✅ 已支持 |
| SYSTICK | ERROR_BASE_SYSTICK (-200) | ✅ 已支持 |
| GPIO | ERROR_BASE_GPIO (-300) | ✅ 已支持 |
| LED | ERROR_BASE_LED (-400) | ✅ 已支持 |
| System | ERROR_BASE_SYSTEM (-500) | ✅ 已支持 |
| Clock Manager | ERROR_BASE_CLOCK_MANAGER (-600) | ✅ 已支持 |
| Delay | ERROR_BASE_DELAY (-700) | ✅ 已支持 |
| TIM2_TimeBase | ERROR_BASE_BASE_TIMER (-800) | ✅ 已支持 |

**注意**：clock_manager模块目前使用 `ERROR_BASE_SYSTEM` 作为错误码基值（历史原因），错误处理模块同时支持两种错误码范围。

---

## 📚 错误码规范

### 错误码范围

- **0**：成功（ERROR_OK）
- **负数**：错误码
  - 每个模块占用100个错误码空间
  - 从基值开始递减定义

### 错误码定义规范

1. **在 `error_code.h` 中添加模块基值**：`#define ERROR_BASE_MYMODULE -900`
2. **在模块头文件中定义错误码枚举**：基于模块基值定义错误码
3. **在模块函数中使用**：返回错误码类型（`error_code_t`）

---

## ⚠️ 重要说明

1. **错误码必须基于模块基值**
   - ❌ 错误：`ERROR_INVALID = -1`（没有基值）
   - ✅ 正确：`MODULE_ERROR_INVALID = ERROR_BASE_MODULE - 1`

2. **错误码范围不要重叠**
   - 每个模块使用独立的100个错误码空间
   - 新增模块时选择合适的基值

3. **错误码类型统一**
   - 所有模块使用 `error_code_t` 类型
   - 或使用基于 `error_code_t` 的枚举类型

4. **错误处理要及时**
   - 不要忽略错误码
   - 至少记录错误日志

---

## 📚 详细文档

- **配置教程**：[Docs/CONFIG_TUTORIAL.md](../Docs/CONFIG_TUTORIAL.md) - 所有模块的使用配置说明
- **使用示例**：[Examples/](../Examples/) - 演示案例

---

## 🔗 相关模块

- **日志系统**：`Debug/log.c/h` - 错误日志记录
- **系统初始化**：`system/system_init.c/h` - 初始化错误处理

---

**最后更新**：2024-01-01
