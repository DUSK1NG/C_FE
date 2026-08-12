# Stage 6：单元后处理设计

- 设计日期：2026-08-10
- 设计状态：已批准，待实施
- 工作分支：`stage6-element-postprocess`
- 基线：Stage 5 `e0eafaf`

## 1. 目标

Stage 6 在 Stage 1 的单元几何和材料参数、Stage 5 的完整节点位移向量之上，计算单个二维桁架杆件的轴向后处理结果：轴向伸长、应变、应力、轴力和拉压状态。

本阶段只增加单元结果计算能力，不承担支座反力、整体平衡、文件输入、结果导出或主程序流程改造。后续 Stage 7 将使用原始总体刚度、荷载和完整位移计算支座反力。

## 2. 公共接口

新增 `include/postprocess.h`，提供：

```c
typedef enum {
    ELEMENT_NEUTRAL = 0,
    ELEMENT_TENSION,
    ELEMENT_COMPRESSION
} ElementState;

typedef struct {
    double elongation;
    double strain;
    double stress;
    double axial_force;
    ElementState state;
} ElementResult;

FemStatus calculate_element_result(
    const Element *element,
    const double displacement[MAX_DOF],
    ElementResult *result);
```

`element->node1` 和 `element->node2` 是节点的内部数组索引。每个节点占用两个全局自由度：`2 * node_index` 为 x 向，`2 * node_index + 1` 为 y 向。该接口不增加 `Node` 位移字段，也不修改 Stage 1–5 的公共接口。

## 3. 计算模型

设杆件方向余弦为 `c`、`s`，端点全局位移为 `(Uix, Uiy)` 和 `(Ujx, Ujy)`：

```text
elongation = c * (Ujx - Uix) + s * (Ujy - Uiy)
strain     = elongation / element->length
stress     = element->E * strain
axial_force = stress * element->A
```

轴力符号约定：

- `axial_force > 0`：`ELEMENT_TENSION`
- `axial_force < 0`：`ELEMENT_COMPRESSION`
- `axial_force == 0`：`ELEMENT_NEUTRAL`

结果结构使用固定容量栈上数据，不引入动态内存。计算过程只读 `element` 和 `displacement`。

## 4. 输入校验与失败语义

函数在参数检查前清零非空 `result`。任意失败路径都保持整个 `ElementResult` 为零值和 `ELEMENT_NEUTRAL`，并保证 `element` 与 `displacement` 不变。

校验及状态码：

1. `element`、`displacement` 或 `result` 为空时返回 `FEM_INVALID_ARGUMENT`。
2. `element->node1` 或 `element->node2` 小于 0、达到 `MAX_NODES`，或两者相等时返回 `FEM_INVALID_INDEX`。
3. `element->length < GEOMETRY_TOL` 时返回 `FEM_ZERO_LENGTH`。
4. `element->E` 或 `element->A` 非有限或不大于 0 时返回 `FEM_INVALID_PROPERTY`。
5. `element->length`、`element->c`、`element->s` 或对应四个位移分量非有限时返回 `FEM_INVALID_ARGUMENT`。
6. 计算中任一结果非有限时返回 `FEM_INVALID_ARGUMENT`，结果保持清零。

## 5. 文件边界

| 文件 | 责任 |
|---|---|
| `include/postprocess.h` | 声明 `ElementState`、`ElementResult` 和单元结果 API |
| `src/postprocess.c` | 校验输入并计算单元伸长、应变、应力、轴力和状态 |
| `tests/test_stage6.c` | 覆盖正常结果、符号状态、非法输入和失败输出语义 |
| `Dockerfile` | 增加 Stage 6 测试的编译与运行步骤；不改变 Demo 运行语义 |
| `docs/superpowers/plans/2026-08-10-element-postprocess.md` | 记录实施步骤、验证命令和实际结果 |

本阶段不创建 `io.c`、`reactions.c` 或结果导出模块，不修改 `src/main.c`，不引入动态内存。

## 6. 验收标准

### 6.1 单杆理论用例

使用 `length = 1000 mm`、`c = 1`、`s = 0`、`E = 200000 MPa`、`A = 100 mm²`，节点 0 的位移为零、节点 1 的 x 向位移为 `1 mm`：

```text
elongation = 1.0 mm
strain = 0.001
stress = 200.0 MPa
axial_force = 20000.0 N
state = ELEMENT_TENSION
```

将节点 1 的 x 向位移改为 `-1 mm`，应得到相同绝对值的应变、应力和轴力，并标记为 `ELEMENT_COMPRESSION`。零位移应返回零值和 `ELEMENT_NEUTRAL`。

### 6.2 斜杆投影用例

使用 `c = sqrt(0.5)`、`s = sqrt(0.5)`，只施加 y 向相对位移，验证结果使用方向余弦投影而不是直接使用全局位移分量。

### 6.3 三角桁架回归

复用设计文档中的三角桁架完整位移，三个单元的参考轴力为：

```text
Element 1:  3125.000 N, tension
Element 2: -5896.238 N, compression
Element 3: -5896.238 N, compression
```

测试允许浮点误差，但必须验证符号、状态和数值在明确容差内一致。

### 6.4 回归与构建

- Stage 6 测试使用 `-std=c11 -Wall -Wextra -pedantic` 编译并运行成功。
- Stage 1–5 测试和现有 Demo 保持通过。
- Dockerfile 中新增的 Stage 6 等价 GCC 命令通过；如果 Docker 引擎不可用，必须明确记录未执行真实镜像验收。
- `git diff --check` 通过，且不得出现动态内存或 Stage 7 反力接口。
