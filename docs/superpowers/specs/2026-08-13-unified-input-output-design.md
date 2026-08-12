# 统一输入输出设计

## 目标

为 Stage 1–10 增加一个统一的命令行运行入口：读取一个固定格式的 `.model` 文件，依次执行几何计算、刚度组装、荷载与约束处理、约束求解、单元后处理和支座反力检查，并根据用户选择输出 TXT、Markdown、CSV 结果。

设计目标是整合现有能力，不重写各 Stage 的数值算法，也不删除现有 Stage 测试和公共 C API。

## 当前问题

当前项目已经具备完整的分析链路：

1. Stage 8 的 `read_model_file()` 读取模型。
2. Stage 1–7 的 API 分别完成几何、刚度、荷载、自由度、求解、单元结果、支座反力和整体平衡计算。
3. Stage 9 的三个写入函数输出 TXT、Markdown 和 CSV。
4. Stage 10 通过测试程序手动串联上述步骤。

但是，用户目前需要自行编写或复制测试代码才能完成一次完整分析，缺少统一输入、统一执行和自定义输出的程序入口。

## 方案与范围

采用 C11 命令行入口方案，不引入 JSON、YAML 或其他第三方依赖。

统一命令示例：

```text
fem --input tests/data/medium.model
```

自定义输出示例：

```text
fem --input tests/data/medium.model `
     --output-dir results `
     --prefix medium `
     --format txt,markdown `
     --include nodes,reactions,summary
```

上述命令生成：

```text
results/medium.txt
results/medium.md
```

输出目录必须已经存在，程序不负责创建目录，以保持标准 C11 和跨平台行为的一致性。

## 架构设计

### 统一分析管线

新增 `include/pipeline.h` 和 `src/pipeline.c`，提供统一分析函数：

```c
FemStatus run_fem_analysis(const FemModel *model,
                           FemResults *results);
```

该函数按以下顺序复用现有 API：

1. 使用 `assemble_global_stiffness()` 完成单元几何、单元刚度和整体刚度组装。
2. 使用 `build_force_vector()` 构建整体荷载向量。
3. 使用 `identify_dofs()` 划分自由和约束自由度。
4. 使用 `solve_constrained_system()` 求解位移。
5. 对每个单元调用 `calculate_element_result()` 计算伸长、应变、应力、轴力和状态。
6. 使用 `calculate_support_reactions()` 计算支座反力。
7. 使用 `check_global_equilibrium()` 检查整体平衡，并保存残差。

`FemResults` 继续作为统一结果载体。管线负责清空输出结构、检查输入指针、传递第一个失败的 `FemStatus`，不复制现有数值算法。

### 命令行入口

`src/main.c` 改为命令行入口，并保留原 Stage 1 Demo：

```text
fem --input PATH
fem --demo
fem --help
```

使用 `--input` 时，入口负责解析参数、读取模型、调用统一管线、构建输出选择并调用输出模块。使用 `--demo` 时保留当前 Stage 1 单元演示行为。

## 参数设计

| 参数 | 必需 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `--input PATH` | 是（`--demo` 除外） | 无 | 输入 `.model` 文件 |
| `--output-dir DIR` | 否 | `.` | 输出目录，必须已存在 |
| `--prefix NAME` | 否 | `fem_results` | 输出文件前缀 |
| `--format LIST` | 否 | `txt,markdown,csv` | 逗号分隔的输出格式 |
| `--include LIST` | 否 | `nodes,elements,reactions,summary` | 逗号分隔的输出区段 |
| `--demo` | 否 | 关闭 | 运行 Stage 1 Demo |
| `--help` | 否 | 关闭 | 显示参数说明 |

允许的格式为 `txt`、`markdown` 和 `csv`。允许的输出区段为：

- `nodes`：节点坐标、荷载、约束和位移。
- `elements`：单元几何和单元后处理结果。
- `reactions`：约束自由度和支座反力。
- `summary`：节点/单元数量、整体平衡残差和分析摘要。

格式和区段列表不得为空；未知名称、重复名称和非法逗号列表都视为参数错误。

## 输出接口设计

在 `include/output.h` 中增加输出选择类型，例如：

```c
typedef struct {
    unsigned sections;
} FemOutputOptions;
```

增加带选项的写入接口，或让现有写入函数接收该选项。已有的 `write_results_txt()`、`write_results_markdown()` 和 `write_results_csv()` 保留为“输出全部区段”的兼容包装函数。

统一入口根据 `--output-dir`、`--prefix` 和格式后缀构造目标路径：

```text
<output-dir>/<prefix>.txt
<output-dir>/<prefix>.md
<output-dir>/<prefix>.csv
```

Markdown 使用 `.md` 后缀；命令参数使用 `markdown` 名称。未选择的格式不会创建文件，未选择的区段不会写入结果内容。

## 错误处理

统一入口使用以下进程返回码：

| 返回码 | 类型 | 行为 |
| --- | --- | --- |
| `0` | 成功 | 分析和所有选定输出完成 |
| `2` | 参数错误 | 打印错误和帮助提示 |
| `3` | 输入错误 | 报告文件无法读取或模型格式错误 |
| `4` | 分析错误 | 报告容量、约束、奇异矩阵或平衡检查错误 |
| `5` | 输出错误 | 报告具体失败的输出路径 |

错误信息应包含失败阶段和 `FemStatus` 文本。分析必须全部成功后才开始生成结果文件；如果多个输出格式中途失败，程序返回 `5` 并报告具体文件。

## 测试设计

保留现有 `tests/test_stage1.c` 至 `tests/test_stage10.c`，并新增统一管线测试，至少覆盖：

1. medium 模型的完整分析链路。
2. large 模型的完整分析链路。
3. TXT、Markdown、CSV 三种格式的完整输出。
4. 仅选择部分格式时不生成未选文件。
5. `nodes`、`elements`、`reactions`、`summary` 区段选择正确生效。
6. 缺少输入文件、未知格式、未知区段和无效参数返回 `2` 或对应错误码。
7. 原有 Stage 1 Demo 仍然能够运行。

验证命令包括：

- Stage 1–10 原有测试全部通过。
- 新增统一管线测试通过。
- 编译使用 `-std=c11 -Wall -Wextra -pedantic`。
- `git diff --check` 通过。
- 手动运行一次完整输入和一次自定义输出命令，确认输出文件集合和内容区段。

## 非目标

本次不增加动态内存分配、第三方解析库、GUI、服务端接口、自动创建输出目录或超出 `MAX_NODES=10`、`MAX_ELEMENTS=20`、`MAX_DOF=20` 的模型容量。

本次不删除现有 Stage API、Stage 测试、模型格式或现有结果格式。

## 验收标准

完成后，用户可以仅通过一个命令读取 `.model` 文件并完成 Stage 1–10 分析，按需选择输出格式和结果区段；错误输入具有稳定返回码和可读错误信息；原有测试和 Stage 1 Demo 保持可用。
