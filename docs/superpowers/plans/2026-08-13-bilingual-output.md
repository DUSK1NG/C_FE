# 双语结果输出计划

**目标：** 让 TXT、Markdown、CSV 三种结果文件都同时包含中文和英文信息，同时保持 CSV 原有英文机器字段的兼容顺序。

## 设计

- TXT：标题、章节、字段名和单元状态使用“中文 / English”并列。
- Markdown：标题、章节和表头使用“中文 / English”并列，状态使用双语值。
- CSV：保留现有英文表头和字段位置，在末尾追加 `record_label_zh` 与 `state_bilingual` 两列；记录增加中文类型标签，单元状态保留原英文 `state` 并增加双语状态列。
- README：说明三种格式均为双语，CSV 的原字段仍可按原顺序读取。

## 任务

1. [x] 扩展 Stage 9 和输出选择测试，验证 TXT/Markdown 双语标题、字段和状态，以及 CSV 双语列和记录标签；确认旧实现失败。
2. [x] 修改 `src/output.c` 的 TXT、Markdown、CSV writer，运行 Stage 9 和输出选择测试。
3. [x] 更新 README，重新编译 `fem.exe`，生成并检查真实 TXT/Markdown/CSV 示例。
4. [x] 运行完整 C11 回归和 CSV 列结构检查，准备提交并合并回 `main`。

## 验证结果

- Stage 1–10、统一管线、输出选择和 CLI 测试全部通过。
- TXT 和 Markdown 已验证包含中英文标题、章节、字段名及单元状态。
- CSV 已验证为 23 列，原英文字段顺序保留，中文记录标签和双语状态列均有值。
