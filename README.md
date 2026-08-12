# 二维桁架有限元

本仓库提供一个固定容量的 C11 二维桁架有限元实现，用于演示单元几何与刚度、整体组装、荷载与约束、约束求解、支座反力、文本输入、结果导出以及项目级验证。

## 项目状态

Stage 1–10 已完成，并已整合到 `main` 分支。实现仍遵循下文所述的固定容量 C11 范围；Stage 10 契约覆盖模型解析、求解、支座反力、单元后处理以及 TXT/Markdown/CSV 结果导出。

提交或发布变更前，可以运行以下命令检查工作区和空白字符：

```text
git status --short --branch
git diff --check
```

## 范围与假设

- 结构为平面铰接桁架，每个节点包含两个平移自由度。
- 单元为直线轴向杆件，采用常量杨氏模量 `E` 和截面积 `A`。
- 节点和单元使用正整数用户 ID；模型输入会将这些 ID 映射到内部固定数组索引。
- 荷载和支座约束施加在节点自由度上。
- 实现仅使用 C11 和标准 C 库。

## 阶段状态

当前项目已实现 Stage 1–10：

- Stage 1：单元几何、方向余弦和刚度矩阵演示。
- Stage 2：整体刚度矩阵组装和力向量行为。
- Stage 3：自由度识别与输入校验。
- Stage 4：约束系统准备和数值边界情况覆盖。
- Stage 5：固定容量约束线性求解。
- Stage 6：单元后处理和状态分类。
- Stage 7：支座反力和整体平衡检查。
- Stage 8：固定容量模型文件解析。
- Stage 9：TXT、Markdown 和 CSV 结果导出，以及 Debug 矩阵/向量打印。
- Stage 10：中型/大型模型夹具、端到端结果导出和项目文档。

## 本地 C11 命令

在仓库根目录编译并运行 Stage 10 契约测试：

```text
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage10.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c -Iinclude -o test_stage10 -lm
./test_stage10
```

在 Windows PowerShell 中，也可以使用 `.\test_stage10.exe` 运行生成的程序。

编译并运行 Stage 1 Demo：

```text
gcc -std=c11 -Wall -Wextra -pedantic src/main.c src/fem.c src/solver.c -Iinclude -o fem -lm
./fem
```

## Stage 8 输入格式

模型文件必须按以下顺序包含这些区段：

```text
NODES <count>
<id> <x> <y>

ELEMENTS <count>
<id> <node1_id> <node2_id> <E> <A>

LOADS <count>
<node_id> <fx> <fy>

CONSTRAINTS <count>
<node_id> <fix_x> <fix_y>
```

支持空行和整行 `#` 注释。节点容量固定为 10，单元容量固定为 20；解析器不会动态分配内存。

## Stage 9 结果输出

输出模块提供以下接口：

- `write_results_txt`：输出便于阅读的分区报告。
- `write_results_markdown`：输出包含固定标题和表格的报告。
- `write_results_csv`：输出稳定的宽表记录，包含 `NODE`、`ELEMENT`、`REACTION` 和 `SUMMARY` 行。
- `print_debug_matrix` 和 `print_debug_vector`：输出明确标记为 Debug 的诊断信息。

## Stage 10 模型夹具

- `tests/data/medium.model` 包含 6 个节点和 8 个单元，其批准的稳定拓扑在节点 1 和节点 3 设置 X/Y 约束。
- `tests/data/large.model` 包含 10 个节点和 20 个单元，其中第 19–20 个单元作为内部网格斜杆处理。

Stage 10 契约会读取两个模型，完成刚度组装、荷载构建、自由度识别、位移求解、支座反力计算和单元结果计算，检查整体平衡，并为每个模型写出 TXT、Markdown 和 CSV 文件，最后删除生成的文件。

## 固定容量与非目标

项目使用 `MAX_NODES=10`、`MAX_ELEMENTS=20` 和 `MAX_DOF=2*MAX_NODES` 固定数组。动态内存分配和第三方数值库不在项目范围内。Stage 10 不增加命令行界面、持久化结果数据库、网格生成器、非线性材料模型，也不提供超出固定容量二维桁架工作流的一般用途求解器。
