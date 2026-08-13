# 项目报告：二维桁架有限元（Stage 1–10）

## 项目概览

本项目实现了一个固定容量的二维桁架有限元求解流程，覆盖单元几何、整体组装、自由度识别、受约束求解、单元后处理、支座反力、文本输入解析以及 TXT / Markdown / CSV 结果导出。实现坚持以下边界：

- 固定数组容量：`MAX_NODES=10`、`MAX_ELEMENTS=20`、`MAX_DOF=20`
- 无动态内存分配
- 仅依赖 C11 与标准 C 库
- 不引入 Docker 才能运行的主工作流

## 数据流与求解流程

统一分析路径如下：

1. 读取 Stage 8 `.model` 输入文件。
2. 构造 `FemModel`。
3. 调用统一 pipeline，按既有 Stage API 顺序执行：
   - 单元几何与整体刚度组装
   - 荷载向量构建
   - 自由/受约束自由度识别
   - 受约束线性系统求解
   - 单元结果后处理
   - 支座反力与整体平衡检查
4. 将结果填入 `FemResults`。
5. 按 CLI 选择的格式与区段写出结果文件。

这条路径复用了 Stage 1–10 的数值能力；统一入口只负责参数解析、读入模型、调度 pipeline、构造输出路径和返回相应退出码。

## 无 Docker 的统一命令

仓库根目录的本地编译命令为：

```text
gcc -std=c11 -Wall -Wextra -pedantic src/main.c src/cli.c src/pipeline.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c -Iinclude -o fem -lm
```

编译完成后，统一命令支持以下三种常用入口。

默认分析命令：

```text
./fem --input tests/data/medium.model
```

默认输出行为：

- 输出目录：当前目录 `.`
- 前缀：`fem_results`
- 格式：`txt,markdown,csv`
- 区段：`nodes,elements,reactions,summary`

因此默认会生成：

- `fem_results.txt`
- `fem_results.md`
- `fem_results.csv`

自定义输出命令：

```text
./fem --input tests/data/medium.model --output-dir . --prefix unified_medium --format txt,markdown --include nodes,reactions,summary
```

该命令只会生成：

- `unified_medium.txt`
- `unified_medium.md`

并且不会写出 element 区段，也不会生成 CSV。

Stage 1 演示命令：

```text
./fem --demo
```

帮助命令：

```text
./fem --help
```

## CLI 契约摘要

| 参数 | 默认值 | 说明 |
|---|---|---|
| `--input PATH` | 无 | 正常分析模式必须提供 |
| `--output-dir DIR` | `.` | 输出目录，必须预先存在 |
| `--prefix NAME` | `fem_results` | 输出文件名前缀 |
| `--format LIST` | `txt,markdown,csv` | 可选 `txt`、`markdown`、`csv` |
| `--include LIST` | `nodes,elements,reactions,summary` | 可选 `nodes`、`elements`、`reactions`、`summary` |
| `--demo` | 关闭 | 运行 Stage 1 Demo |
| `--help` | 关闭 | 输出帮助 |

命令行解析规则：

- `--help` 成功解析后直接退出，不再要求 `--input`。
- `--demo` 成功解析后运行旧的 Stage 1 演示逻辑。
- `--demo` 与 `--input` 不能同时出现。
- 缺少 `--input`、未知选项、未知格式、未知区段、重复条目、空条目、缺少参数值，都会返回 CLI 错误。
- 每个标量或列表选项最多出现一次；重复选项返回 CLI 错误。
- `--output-dir` 目录必须已经存在；程序不会自动创建目录。
- 输出扩展名固定为 `.txt`、`.md`、`.csv`。
- 所选输出目标必须尚不存在；预先存在的文件不会被覆盖或在失败清理中删除。

## 输出区段与文件内容

`--include` 控制四类稳定区段：

| 取值 | 含义 | 典型标记 |
|---|---|---|
| `nodes` | 节点位移与节点元数据 | `Nodal Displacements` / `## Nodal Displacements` / `NODE,` |
| `elements` | 单元伸长、应变、应力、轴力、状态 | `Element Results` / `## Element Results` / `ELEMENT,` |
| `reactions` | 受约束节点支座反力 | `Support Reactions` / `## Support Reactions` / `REACTION,` |
| `summary` | 整体平衡残差与汇总信息 | `Equilibrium` / `## Equilibrium` / `SUMMARY,` |

默认情况下，统一入口输出全部区段；自定义命令可以稳定地裁剪为部分区段。与此同时，Stage 9 的旧包装函数 `write_results_txt()`、`write_results_markdown()`、`write_results_csv()` 仍保持“输出全部区段”的兼容行为，用于保留既有测试契约。

## 返回码

| 返回码 | 含义 |
|---|---|
| `0` | 成功 |
| `2` | 命令行参数错误 |
| `3` | 输入错误 |
| `4` | 分析错误 |
| `5` | 输出错误 |

对应示例：

- `./fem --help` → `0`
- `./fem --demo` → `0`
- `./fem` → `2`
- `./fem --input missing.model` → `3`
- `./fem --input <奇异模型>` → `4`
- `./fem --input tests/data/medium.model --output-dir missing-dir` → `5`

## Stage 10 与当前工作流的关系

Stage 10 仍是项目级回归的一部分：它会读取 `tests/data/medium.model` 与 `tests/data/large.model`，完成端到端求解、结果导出与清理检查。但这不再意味着用户必须通过单独的测试程序来使用系统。当前推荐的日常路径就是统一入口 `fem`，直接在本地（无 Docker）完成输入、求解与输出。

## 固定容量限制与非目标

项目不覆盖以下能力：

- 超出固定容量的更大模型
- 动态内存扩容
- 非线性分析、动力分析或通用高维单元
- 自动网格生成
- 结果数据库持久化
- Docker 专属运行流程

因此，本报告记录的都是已经由统一命令与现有 Stage 回归共同验证过的行为，不额外宣称未经验证的能力。
