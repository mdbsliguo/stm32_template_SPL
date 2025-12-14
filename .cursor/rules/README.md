# Cursor AI规则体系说明

**注意：本目录为 Cursor IDE 的规则加载入口。实际规则文件存储在 `AI/` 目录。**

本项目的AI行为规范由分层规则体系控制：
- **主存储位置**：`AI/` 目录（使用 `.md` 和 `.mdc` 格式，UTF-8编码）
- **Cursor 加载位置**：`.cursor/rules/` 目录（使用 `.mdc` 格式，UTF-8编码）

## 文件同步说明

本目录下的 `.mdc` 文件是从 `AI/` 目录同步的副本，用于 Cursor IDE 加载。

**重要**：
- 所有规则文件的实际存储位置在 `AI/` 目录
- 如需修改规则，请编辑 `AI/` 目录下的对应文件
- 修改后需要同步到本目录（`.cursor/rules/`）

## 文件对应关系

| .cursor/rules/ | AI/ |
|----------------|-----|
| 00-keywords.mdc | AI/KEYWORDS.mdc |
| 01-writing.mdc | AI/rules/0_core/writing.mdc |
| 02-structure.mdc | AI/rules/0_core/structure.mdc |
| 03-priority.mdc | AI/rules/0_core/priority.mdc |
| 10-stm32.mdc | AI/rules/1_topics/stm32.mdc |
| 11-tech.mdc | AI/rules/1_topics/tech.mdc |
| 12-style.mdc | AI/rules/1_topics/style.mdc |
| 13-error.mdc | AI/rules/1_topics/error.mdc |
| 14-examples.mdc | AI/rules/1_topics/examples.mdc |
| 20-markdown.mdc | AI/rules/2_reference/markdown.mdc |
| 21-examples.mdc | AI/rules/2_reference/examples.mdc |

---

## ? 规则加载优先级（由高到低）

1. **00-keywords.mdc**（最高优先级，alwaysApply: true）
   - STM32项目最高优先级规则
   - 所有AI行为的唯一最高优先级规则源，不可被覆盖

2. **核心规则（0_core）**
   - `01-writing.mdc` - 写作核心规则
   - `02-structure.mdc` - 文档结构规则
   - `03-priority.mdc` - 优先级体系规则

3. **功能性规则（1_topics）**
   - `10-stm32.mdc` - STM32项目特定规则（自动附加到.c/.h文件）
   - `11-tech.mdc` - 技术写作规则（自动附加到.md文件）
   - `12-style.mdc` - 文案风格规则（自动附加到.md文件）
   - `13-error.mdc` - 错误处理规则（自动附加到Common/目录）
   - `14-examples.mdc` - 案例创建规范（自动附加到Examples/目录）

4. **参考内容（2_reference）**
   - `20-markdown.mdc` - Markdown格式参考（无约束力）
   - `21-examples.mdc` - 示例文档参考（无约束力）

---

## ? 规则应用方式

### Always规则
- `00-keywords.mdc`：始终包含在AI上下文中

### Auto Attached规则
- 通过`globs`匹配文件类型，自动附加到相关文件
- `10-stm32.mdc`：匹配所有`.c/.h`文件
- `11-tech.mdc`：匹配所有`.md`文件
- `12-style.mdc`：匹配所有`.md`文件
- `13-error.mdc`：匹配`Common/`目录
- `14-examples.mdc`：匹配`Examples/`目录

### Agent Requested规则
- AI根据任务相关性选择包含
- 通过`description`字段描述规则用途

### Manual规则
- 通过`@ruleName`显式调用
- 适用于参考内容

---

## ? 快速访问

### 核心规则
- [KEYWORDS规则](00-keywords.mdc) - 最高优先级规则
- [写作规则](01-writing.mdc) - 写作核心原则
- [结构规则](02-structure.mdc) - 文档结构规范
- [优先级规则](03-priority.mdc) - 优先级体系说明

### STM32项目规则
- [STM32特定规则](10-stm32.mdc) - 函数返回类型、命名规范、硬件配置等

### 功能性规则
- [技术写作规则](11-tech.mdc) - 技术文档写作规范
- [文案风格规则](12-style.mdc) - 风格调整规范
- [错误处理规则](13-error.mdc) - 错误处理规范
- [案例创建规范](14-examples.mdc) - 案例创建规范

### 参考内容
- [Markdown参考](20-markdown.mdc) - Markdown格式示例
- [示例文档参考](21-examples.mdc) - 文档格式和代码示例

---

## ? 使用指南

### 对于AI
1. 首先加载`00-keywords.mdc`（自动加载）
2. 根据文件类型自动加载相关规则（通过globs匹配）
3. 根据任务相关性选择加载其他规则
4. 冲突时，以优先级最高的规则为准

### 对于开发者
1. 修改规则时，注意保持优先级体系
2. 新增规则时，遵循文件命名规范（数字前缀）
3. 更新规则时，注意更新本README
4. 测试规则时，验证规则是否正确应用

---

## ? 规则文件命名规范

- **00-keywords.mdc**：最高优先级规则
- **01-XX.mdc**：核心规则（0_core）
- **10-XX.mdc**：功能性规则（1_topics）
- **14-examples.mdc**：案例创建规范（1_topics，Examples目录专用）
- **20-XX.mdc**：参考内容（2_reference）

数字前缀用于：
- 明确优先级顺序
- 便于文件排序
- 便于快速识别规则类型

---

## ? 冲突解决

若规则冲突，按以下优先级解决：
1. KEYWORDS规则（00-keywords.mdc）
2. 核心规则（01-XX.mdc）
3. 功能性规则（10-XX.mdc）
4. 参考内容（20-XX.mdc）

---

## ? 更新日志

### v1.0.0 (2024-12-14)
- 初始版本
- 创建完整的规则体系
- 包含KEYWORDS、核心规则、功能性规则和参考内容

---

**最后更新**：2024-12-14

