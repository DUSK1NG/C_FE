# Stage 5：自由度缩减与位移回代设计

- 设计日期：2026-08-10
- 设计状态：已实现、已验证
- 工作分支：`stage5-dof-reduction`
- 基线：已合并 Stage 4 的 `stage3-loads-constraints`

## 1. 目标与边界

Stage 5 在 Stage 3 的原始总体刚度矩阵、原始荷载向量和自由度集合之上，构造自由自由度子系统，复用 Stage 4 的高斯消元求解器得到自由位移，并恢复固定容量的完整位移向量。

本阶段包含：

- 校验自由度集合的容量、范围、重复项和交集；
- 从 `global_k` 和 `force` 提取 `Kff` 与 `Ff`；
- 调用现有 `solve_linear_system()` 求解 `Kff * Uf = Ff`；
- 将自由位移回填到完整 `displacement[MAX_DOF]`；
- 将约束自由度以及未参与当前模型的尾部自由度保持为零；
- 覆盖成功、奇异矩阵、非法参数、容量超限、非法自由度、重复自由度和输入不变性测试。

本阶段不包含：

- 修改 Stage 4 线性求解器的接口或数值算法；
- 支持非零支座位移；当前 `fix_x` 和 `fix_y` 只表示零位移约束；
- 计算支座反力、应变、应力或单元后处理结果；
- 修改文件输入、动态内存策略或主程序流程；Docker 仅允许把
  `src/solver.c` 加入既有 demo 与 Stage 1 测试的编译源列表，以保持
  Stage 5 后的链接有效，不改变镜像阶段、测试执行或运行入口；
- 要求自由度集合覆盖 `0..MAX_DOF` 的全部槽位。Stage 3 只对实际节点自由度输出集合，固定容量尾部可以不参与求解。

## 2. 设计方案

采用单一高层公共接口，由 `fem.c` 负责有限元数据的缩减与回代，由 `solver.c` 继续负责独立的线性方程求解。

```c
FemStatus solve_constrained_system(
    const double global_k[MAX_DOF][MAX_DOF],
    const double force[MAX_DOF],
    const int free_dofs[MAX_DOF],
    int free_count,
    const int constrained_dofs[MAX_DOF],
    int constrained_count,
    double displacement[MAX_DOF]);
```

选择该方案的原因：调用方只需要提交 Stage 2/3 已有的原始数据和自由度集合，缩减、求解、回代共享同一个清零和失败处理边界；同时不扩大 Stage 4 求解器的职责，也不暴露临时 `Kff`/`Ff` 缓冲区。

相比拆分成两个公共接口，该方案减少了 API 面积和调用顺序错误的可能。矩阵缩减仍通过固定容量的内部数组完成，测试可以通过完整位移结果、输入不变性和求解器状态覆盖其行为。

## 3. 数据流与固定容量策略

```text
global_k[MAX_DOF][MAX_DOF] ──┐
                             ├─> Kff[MAX_DOF][MAX_DOF] ──┐
force[MAX_DOF] ──────────────┘                          │
free_dofs[] ────────────────────────────────────────────┼─> solve_linear_system()
                                                        │
                                                        └─> Uf[] ──> displacement[]
constrained_dofs[] ──> 合法性/交集校验 ───────────────────────────────┘
```

函数使用固定容量的局部数组：

- `reduced_matrix[MAX_DOF][MAX_DOF]`；
- `reduced_force[MAX_DOF]`；
- `free_solution[MAX_DOF]`；
- `seen[MAX_DOF]`，用于检查自由度重复项和自由/约束集合交集。

不使用动态内存。函数开始时清零 `displacement` 的全部 `MAX_DOF` 槽位；任何失败路径都保持完整输出为零。成功时只写入 `displacement[free_dofs[i]]`，约束自由度和未参与集合的槽位自然保持零。

当 `free_count == 0` 时，经过参数和自由度集合校验后直接返回 `FEM_OK`，完整位移向量保持为零；这表示当前集合全部为约束自由度，不需要调用 Stage 4 求解器。

## 4. 校验与错误处理

校验顺序和结果如下：

1. 如果 `displacement` 非空，先清零完整输出；
2. `global_k`、`force`、两个自由度数组和 `displacement` 任一为空，或计数为负，返回 `FEM_INVALID_ARGUMENT`；
3. `free_count`、`constrained_count` 或两者之和超过 `MAX_DOF`，返回 `FEM_CAPACITY_EXCEEDED`；
4. 每个自由度必须位于 `[0, MAX_DOF)`，否则返回 `FEM_INVALID_ARGUMENT`；
5. 自由度数组内部不得重复，自由度与约束自由度之间不得交集，否则返回 `FEM_INVALID_ARGUMENT`；
6. 按自由度数组顺序提取 `Kff[i][j] = global_k[free_dofs[i]][free_dofs[j]]` 和 `Ff[i] = force[free_dofs[i]]`；
7. 调用 `solve_linear_system(Kff, Ff, free_count, Uf)`；其 `FEM_INVALID_ARGUMENT`、`FEM_SINGULAR_MATRIX` 或其他状态原样返回，输出继续保持全零；
8. 求解成功后将 `Uf[i]` 回填到对应自由度，返回 `FEM_OK`。

原始 `global_k`、`force`、`free_dofs` 和 `constrained_dofs` 全程只读，不因缩减或求解而改变。`Kff` 或 `Ff` 中的 NaN/无穷值由 Stage 4 求解器拒绝，并通过其已有状态返回；Stage 5 不新增状态码。

## 5. 文件范围

| 文件 | 职责 |
|---|---|
| `include/fem.h` | 声明 `solve_constrained_system()` |
| `src/fem.c` | 增加自由度集合校验、`Kff`/`Ff` 提取、求解调用和位移回代 |
| `tests/test_stage5.c` | 覆盖 Stage 5 公共接口契约和回归路径 |
| `Dockerfile` | 在既有 demo 与 Stage 1 测试链接命令中加入 `src/solver.c` |
| `docs/superpowers/plans/2026-08-10-dof-reduction.md` | 记录实现步骤与验收结果 |

不修改 `include/model.h`、`include/solver.h`、`src/solver.c`、`src/main.c`
和既有 Stage 1～4 测试文件。Dockerfile 的变更严格限于上述两条命令的
编译源列表。

## 6. 验收模型与测试

测试使用一个固定容量的稀疏对角/耦合系统，明确指定自由度和约束自由度，避免把 Stage 5 的正确性绑定到几何刚度数值。至少覆盖：

- `free_dofs = {2, 4, 5}`、`constrained_dofs = {0, 1, 3}` 时，成功求得已知 `Uf`，并验证约束位移为零；
- 自由度集合顺序保持调用方给出的顺序，回填按自由度编号定位；
- `free_count == 0` 时返回成功且完整位移为零；
- 缺少指针、负计数、越界自由度和重复自由度返回 `FEM_INVALID_ARGUMENT`，并清空输出；
- 计数或总计数超过 `MAX_DOF` 返回 `FEM_CAPACITY_EXCEEDED`，并清空输出；
- 缩减后的矩阵奇异时返回 `FEM_SINGULAR_MATRIX`，并清空输出；
- 缩减后的载荷含 `NAN` 等非有限值时，原样透传 Stage 4 的
  `FEM_INVALID_ARGUMENT`，并清空输出；
- 原始矩阵、荷载向量和两个自由度数组在成功、奇异失败与非有限失败后
  均逐字节保持不变；
- 负自由 DOF、负约束 DOF 返回 `FEM_INVALID_ARGUMENT`；
  `constrained_count > MAX_DOF` 返回 `FEM_CAPACITY_EXCEEDED`；
- Stage 1、Stage 2、Stage 3、Stage 4 回归测试继续通过，Stage 1 示例继续通过；
- Dockerfile 两条 GCC 命令的等价本地链接验证通过；Docker 引擎不可用时
  仅记录为未执行，不宣称 Docker 镜像验收通过；
- `git diff --check` 通过，测试临时可执行文件不进入仓库。

完整的测试命令、实际输出和环境限制写入实现计划的执行结果部分；未运行 Docker 时不宣称 Docker 验收通过。
