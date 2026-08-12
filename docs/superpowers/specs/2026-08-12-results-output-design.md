# Stage 9 结果导出与 Debug 设计

## 目标

在不改变 Stage 1–8 核心计算接口的前提下，增加固定容量的结果导出模块。模块能够把已完成的二维桁架计算结果写成可读 TXT 和可脚本处理 CSV，并提供受 `DEBUG` 编译开关控制的矩阵、向量打印函数。

## 范围与约束

- 继续使用 C11、固定容量数组和现有 `FemStatus` 错误码。
- 不使用动态内存，不引入第三方库，不改变 `FemModel`、求解器、后处理和反力计算的既有接口。
- Stage9 不把 `main.c` 改造成命令行程序；现有 Stage1 Demo 输出和 `DEBUG` 行为保持兼容。
- 输出函数只负责校验并序列化已经计算好的结果，不重新计算有限元结果。
- 所有写文件失败、空指针、非法计数和非有限数值都必须返回错误，不生成伪成功结果。

## 模块与数据接口

新增 `include/output.h` 和 `src/output.c`。

`output.h` 定义结果容器：

```c
typedef struct {
    double displacement[MAX_DOF];
    double reactions[MAX_DOF];
    ElementResult element_results[MAX_ELEMENTS];
    int constrained_dofs[MAX_DOF];
    int constrained_count;
    double residual_fx;
    double residual_fy;
} FemResults;

FemStatus write_results_txt(const char *path,
                            const FemModel *model,
                            const FemResults *results);

FemStatus write_results_csv(const char *path,
                            const FemModel *model,
                            const FemResults *results);

FemStatus write_results_markdown(const char *path,
                                 const FemModel *model,
                                 const FemResults *results);

void print_debug_matrix(const char *name,
                        const double matrix[MAX_DOF][MAX_DOF],
                        int size);

void print_debug_vector(const char *name,
                        const double vector[MAX_DOF],
                        int size);
```

输出模块通过 `io.h` 使用 `FemModel`，通过 `postprocess.h` 使用 `ElementResult`。`constrained_dofs` 使用已有的全局自由度编号，反力数组使用相同编号；节点编号仍通过 `model->nodes[i].id` 输出，不能把内部索引当作用户编号。

## TXT 格式

TXT 是面向人的分段结果报告，采用稳定标题和固定精度：

```text
2D Truss FEM Results

Nodal Displacements
node_id ux uy
...

Element Results
element_id elongation strain stress axial_force state
...

Support Reactions
node_id rx ry
...

Equilibrium
residual_fx residual_fy
...
```

节点段按 `model->node_count` 输出；单元段按 `model->element_count` 输出；反力段只输出同时具有约束自由度的节点，并分别把 `2*i` 与 `2*i+1` 的反力映射为 `rx`、`ry`，无约束方向输出零。状态输出为 `NEUTRAL`、`TENSION` 或 `COMPRESSION`。所有数值使用统一的 `%.12g` 格式。

## Markdown 格式

Markdown 是适合在项目报告、代码托管平台或教学记录中直接阅读的结果报告。它使用固定标题和表头，包含节点位移、单元结果、支座反力和整体平衡摘要：

```markdown
# 2D Truss FEM Results

## Nodal Displacements
| Node ID | ux | uy |
|---:|---:|---:|
| ... | ... | ... |

## Element Results
| Element ID | Elongation | Strain | Stress | Axial Force | State |
|---:|---:|---:|---:|---:|---|
| ... | ... | ... | ... | ... | ... |

## Support Reactions
| Node ID | rx | ry |
|---:|---:|---:|
| ... | ... | ... |

## Equilibrium
| Residual Fx | Residual Fy |
|---:|---:|
| ... | ... |
```

Markdown 数值同样使用 `%.12g`，状态输出为 `NEUTRAL`、`TENSION` 或 `COMPRESSION`，用户节点/单元 ID 直接来自模型。

## CSV 格式

CSV 使用单一稳定宽表，首列 `record_type` 区分记录类型，避免多个文件或不规则表头：

```text
record_type,id,ux,uy,elongation,strain,stress,axial_force,state,rx,ry,residual_fx,residual_fy
NODE,<node_id>,<ux>,<uy>,,,,,,,,
ELEMENT,<element_id>,,,<elongation>,<strain>,<stress>,<axial_force>,<state>,,,,
REACTION,<node_id>,,,,,,,,<rx>,<ry>,,
SUMMARY,,,,,,,,,,,<residual_fx>,<residual_fy>
```

字段不足的记录以空字段补齐；字段顺序和大小写固定；用户节点/单元 ID 直接输出，不输出内部数组索引。CSV 字段不包含逗号、换行或引号，因此不需要额外转义逻辑。

## Debug 输出

`print_debug_matrix` 和 `print_debug_vector` 只接受 `0 < size <= MAX_DOF` 的方阵/向量。非法参数不输出内容并直接返回；有效参数输出名称、维度和固定精度数值。调用方通过现有 `#if DEBUG` 控制是否调用，正式 TXT/CSV 文件永远不包含 Debug 内容。

## 错误处理与校验

导出前验证：

- `path`、`model`、`results` 非空；
- `0 < node_count <= MAX_NODES`、`0 <= element_count <= MAX_ELEMENTS`；
- `0 <= constrained_count <= MAX_DOF`，每个约束自由度在范围内且不重复；
- 所有要写出的位移、反力、单元结果和残差为有限数；
- `element_results` 与模型单元数量一致，由调用方按同一内部顺序填充。

发生任何校验或文件错误时返回 `FEM_INVALID_ARGUMENT`、`FEM_INPUT_ERROR` 或现有最贴切状态，并关闭已打开的文件。TXT/CSV 写入成功后返回 `FEM_OK`。

## 测试策略

新增 `tests/test_stage9.c`，使用临时文件并读取回内容，覆盖：

1. TXT 包含节点、单元、反力、平衡残差和状态文本；
2. Markdown 包含固定标题、表头、节点/单元/反力表和平衡摘要；
3. CSV 首行严格匹配稳定表头，NODE/ELEMENT/REACTION/SUMMARY 记录可解析；
4. 非连续用户 ID 正确输出，未泄漏内部索引；
5. 空指针、非法计数、重复约束自由度和非有限结果被拒绝；
6. 不可创建的输出路径返回错误；
7. Debug 矩阵和向量输出包含名称、维度和值；
8. Stage1–8 回归测试、Demo 和 Docker 构建继续通过。

测试使用真实输出文件和真实模块，不通过 mock 验证实现细节。输出文件写入临时目录，测试结束后删除。

## 验收标准

- 正式模式只产生简洁、稳定的 TXT/CSV 结果；
- Debug 接口能够打印关键矩阵和向量，且受 `DEBUG` 调用开关控制；
- Stage1–8 既有测试全部通过；
- Stage9 测试在严格 C11 `-Wall -Wextra -pedantic` 下通过；
- 无动态内存调用，工作树干净，Docker 构建和运行通过。
