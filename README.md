# 二维桁架有限元（C11，固定容量）

本仓库提供一个固定容量、无动态内存分配的二维桁架有限元示例实现。当前 Stage 1–10 已完成，既保留了 Stage 1 的单元演示模式，也提供了统一的本地命令行工作流，可直接读取 `.model` 文件并输出 TXT、Markdown、CSV 结果。

## 项目范围

- 平面桁架，每个节点 2 个平移自由度。
- 直线轴向杆单元，材料参数为常量杨氏模量 `E` 和截面面积 `A`。
- 固定容量：`MAX_NODES=10`、`MAX_ELEMENTS=20`、`MAX_DOF=20`。
- 仅使用 C11 和标准 C 库，不依赖 Docker、第三方数值库或动态分配。

## Stage 进展

1. Stage 1：单元几何、方向余弦、刚度矩阵与 Demo。
2. Stage 2：整体刚度矩阵组装与荷载向量。
3. Stage 3：自由/受约束自由度识别与输入校验。
4. Stage 4：受约束系统准备与数值边界情形。
5. Stage 5：固定容量线性方程组求解。
6. Stage 6：单元后处理（伸长、应变、应力、轴力、状态）。
7. Stage 7：支座反力与整体平衡检查。
8. Stage 8：固定容量 `.model` 文本输入解析。
9. Stage 9：TXT / Markdown / CSV 结果导出与 Debug 打印。
10. Stage 10：中/大模型端到端夹具验证与项目级回归。

Stage 10 仍然是回归测试的一部分，但日常使用不再需要单独编排测试程序：统一入口 `fem` 已直接覆盖“读取输入 → 求解 → 选定输出”的完整流程。

## 无 Docker 的本地命令行工作流

在仓库根目录编译统一入口：

```text
gcc -std=c11 -Wall -Wextra -pedantic src/main.c src/cli.c src/pipeline.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c -Iinclude -o fem -lm
```

编译后可直接运行以下命令。

默认统一命令（输出全部格式、全部区段）：

```text
./fem --input tests/data/medium.model
```

默认会在当前目录生成：

- `fem_results.txt`
- `fem_results.md`
- `fem_results.csv`

自定义输出命令（只写 TXT 和 Markdown，且只包含节点 / 反力 / 汇总区段）：

```text
./fem --input tests/data/medium.model --output-dir . --prefix unified_medium --format txt,markdown --include nodes,reactions,summary
```

该命令只会生成：

- `unified_medium.txt`
- `unified_medium.md`

不会生成 `unified_medium.csv`，且输出中不会包含 element 区段。

保留 Stage 1 演示模式：

```text
./fem --demo
```

帮助信息：

```text
./fem --help
```

## 命令行参数

| 参数 | 是否必需 | 默认值 | 说明 |
|---|---|---|---|
| `--input PATH` | 是（`--demo` / `--help` 除外） | 无 | 输入 `.model` 文件 |
| `--output-dir DIR` | 否 | `.` | 输出目录；目录必须已存在，程序不会自动创建 |
| `--prefix NAME` | 否 | `fem_results` | 输出文件名前缀 |
| `--format LIST` | 否 | `txt,markdown,csv` | 逗号分隔格式列表 |
| `--include LIST` | 否 | `nodes,elements,reactions,summary` | 逗号分隔输出区段列表 |
| `--demo` | 否 | 关闭 | 运行 Stage 1 演示模式 |
| `--help` | 否 | 关闭 | 显示帮助并退出 |

参数约束与行为：

- `--input` 与 `--demo` 不能同时使用。
- 未提供 `--input` 且未使用 `--demo` / `--help` 时，命令行解析失败。
- `--format` 只接受 `txt`、`markdown`、`csv`。
- `--include` 只接受 `nodes`、`elements`、`reactions`、`summary`。
- `--format` / `--include` 会拒绝空条目、重复条目和未知条目。
- 每个命令行选项最多出现一次；重复的标量或列表选项都会返回 `2`。
- 输出文件后缀固定为 `.txt`、`.md`、`.csv`，并按该顺序尝试写出。
- 任一所选输出目标已存在时，命令返回 `5`，且不会覆盖或删除已有文件。

## 输出格式与区段含义

`--format` 可选值：

- `txt`
- `markdown`
- `csv`

`--include` 可选值：

- `nodes`：节点位移区段
  - TXT 标题：`Nodal Displacements`
  - Markdown 标题：`## Nodal Displacements`
  - CSV 记录：`NODE,...`
- `elements`：单元结果区段（伸长、应变、应力、轴力、状态）
  - TXT 标题：`Element Results`
  - Markdown 标题：`## Element Results`
  - CSV 记录：`ELEMENT,...`
- `reactions`：支座反力区段
  - TXT 标题：`Support Reactions`
  - Markdown 标题：`## Support Reactions`
  - CSV 记录：`REACTION,...`
- `summary`：整体平衡汇总区段
  - TXT 标题：`Equilibrium`
  - Markdown 标题：`## Equilibrium`
  - CSV 记录：`SUMMARY,...`

当未显式指定 `--format` 或 `--include` 时，程序默认输出全部三种格式与全部四个区段。

## 返回码

| 返回码 | 含义 |
|---|---|
| `0` | 成功（含 `--help` 和 `--demo` 成功执行） |
| `2` | 命令行参数错误 |
| `3` | 输入错误（如模型文件不存在或内容非法） |
| `4` | 求解/分析错误 |
| `5` | 输出错误（如 `--output-dir` 不存在、路径非法或写文件失败） |

常见失败示例：

- `./fem` → 返回 `2`
- `./fem --input missing.model` → 返回 `3`
- `./fem --input <奇异或无效模型>` → 返回 `4`
- `./fem --input tests/data/medium.model --output-dir missing-dir` → 返回 `5`

## Stage 8 输入格式

`.model` 文件必须按固定顺序提供以下区段：

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

支持空行和整行 `#` 注释。

当前仓库内已提供：

- `tests/data/triangle.model`
- `tests/data/medium.model`
- `tests/data/large.model`

## 回归检查建议

提交前可在仓库根目录检查工作区状态：

```text
git status --short --branch
git diff --check
```

如需完整项目回归，可继续编译并运行 Stage 1–10、pipeline、output-selection、CLI 与统一入口黑盒检查；这些验证属于开发/验收流程，不是使用统一命令时的前置条件。
