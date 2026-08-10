# Stage 4：带部分主元的高斯消元设计

- 设计日期：2026-08-10
- 设计状态：已获用户确认，待文档审阅
- 基础分支：`stage3-loads-constraints`
- 实现分支：`stage4-gaussian-elimination`

## 1. 目标与边界

Stage 4 在 Stage 3 的固定容量数据模型之上，提供一个独立的线性方程组求解器，用带部分主元交换的高斯消元求解

```text
A x = b
```

本阶段包含：

- 支持 `1 <= size <= MAX_DOF` 的方阵；
- 使用局部固定容量工作区，不引入动态内存或第三方矩阵库；
- 每个消元列选择绝对值最大的候选主元并进行行交换；
- 使用相对于矩阵整体尺度的主元容差识别奇异或近奇异矩阵；
- 在输入错误、容量超限或求解失败时清空完整解向量；
- 通过独立测试验证换行、回代、奇异矩阵和错误路径。

本阶段不包含：

- 自由度集合到 `Kff Uf = Ff` 的缩减；
- 完整位移向量恢复；
- 支座反力、单元后处理或结果输出；
- 文件输入、动态内存和主程序流程改造；
- 对 Stage 1、Stage 2 和 Stage 3 算法的重构。

## 2. 公共接口

在 `include/solver.h` 中新增：

```c
FemStatus solve_linear_system(
    const double matrix[MAX_DOF][MAX_DOF],
    const double rhs[MAX_DOF],
    int size,
    double solution[MAX_DOF]);
```

接口约定：

- `matrix` 和 `rhs` 只读，函数不得修改调用方数据；
- 只使用左上角 `size × size` 的矩阵和前 `size` 个右端项；
- 成功时将解写入 `solution[0..size)`，并将 `solution[size..MAX_DOF)` 清零；
- `solution` 非空时，函数在所有参数检查前清零完整容量；
- `matrix`、`rhs` 或 `solution` 为空，或 `size <= 0`，返回 `FEM_INVALID_ARGUMENT`；
- `size > MAX_DOF` 返回 `FEM_CAPACITY_EXCEEDED`；
- 有效输入区域含 NaN 或无穷值时返回 `FEM_INVALID_ARGUMENT`；
- 奇异或近奇异矩阵返回 `FEM_SINGULAR_MATRIX`，输出保持全零。

在 `include/fem.h` 的 `FemStatus` 中新增：

```c
FEM_SINGULAR_MATRIX
```

`fem_status_message` 返回对应的可读文本 `matrix is singular or ill-conditioned`。

## 3. 数值算法

`src/solver.c` 使用以下流程：

1. 清零 `solution[MAX_DOF]`；
2. 检查指针、`size`、容量和有效输入区域的有限性；
3. 将有效矩阵和右端项复制到局部 `work_matrix[MAX_DOF][MAX_DOF]` 与 `work_rhs[MAX_DOF]`；
4. 计算有效矩阵的最大绝对值 `matrix_scale`，用
   `SOLVER_TOL * max(1.0, matrix_scale)` 作为主元判定阈值；
5. 对每个主元列，在当前行及以下候选行中选择绝对值最大的项；
6. 若候选主元不超过阈值，立即返回 `FEM_SINGULAR_MATRIX`；否则交换行并消去主元下方元素；
7. 从最后一行开始回代，得到 `solution[0..size)`；
8. 保持解向量的其余容量为零。

所有失败路径都不得留下部分有效解。输入矩阵的原始值保留在调用方内存中，便于后续 Stage 5 继续使用原始总体矩阵。

在 `include/config.h` 中新增独立的 `SOLVER_TOL`，避免复用几何长度容差。该容差只用于主元相对于矩阵尺度的比较，不直接用 `== 0` 判断浮点数。

## 4. 文件职责

| 文件 | 职责 |
|---|---|
| `include/fem.h` | 增加 `FEM_SINGULAR_MATRIX` 状态码及状态文本声明的兼容项 |
| `include/solver.h` | 声明线性方程组求解接口 |
| `include/config.h` | 定义 `SOLVER_TOL` |
| `src/solver.c` | 实现输入检查、局部复制、部分主元消元和回代 |
| `tests/test_stage4.c` | 覆盖可解、换行、奇异和错误路径 |
| `docs/superpowers/plans/2026-08-10-gaussian-elimination.md` | 记录实现步骤和验收结果 |

不修改 `src/main.c`、Docker 配置、Stage 1/2 测试和 Stage 3 测试。

## 5. 测试与验收

新增 `tests/test_stage4.c`，保持现有 `expect_close`、`expect_status` 和成功提示风格，至少覆盖：

1. 一个已知 `2×2` 方程组，验证解值；
2. 首个候选主元为零、必须交换行的 `2×2` 方程组；
3. 一个已知 `3×3` 方程组，验证多步消元和回代；
4. 奇异矩阵返回 `FEM_SINGULAR_MATRIX`，并验证完整输出清零；
5. 空指针、非正尺寸和超过 `MAX_DOF` 的输入；
6. 输入矩阵与右端项在求解后保持不变；
7. 小于 `size` 的解有效，`solution[size..MAX_DOF)` 保持为零；
8. Stage 1、Stage 2、Stage 3 回归测试继续通过。

验收命令使用 C11、`-Wall -Wextra -pedantic` 编译 Stage 4 测试，并运行全部阶段测试和现有示例程序。代码范围检查不得出现自由度缩减、位移恢复、文件输入或后处理模块。

## 6. 后续衔接

Stage 5 将复用本阶段的 `solve_linear_system`，先使用 Stage 3 的自由度集合从原始总体矩阵和荷载向量构造 `Kff`、`Ff`，再求解自由位移并恢复完整位移向量。Stage 4 不应提前承担这些职责。
