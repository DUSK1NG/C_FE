# Stage 4：带部分主元的高斯消元实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在固定容量二维桁架项目中加入不修改输入的带部分主元高斯消元，用于求解独立的方阵线性系统。

**Architecture:** 新增独立的 solver.c/solver.h 模块，使用 MAX_DOF 固定容量局部工作区复制输入矩阵和右端项，先前向消元再回代。求解器只负责 A x = b，不处理 Stage 3 的自由度缩减，也不改造主程序。

**Tech Stack:** C11、GCC/MinGW-w64、固定容量 C 数组、math.h、现有 FemStatus 和 MAX_DOF 配置。

## Global Constraints

- 使用 C11、double，单位体系继续使用 mm、N、MPa 和 mm²。
- MAX_NODES 继续为 10，MAX_DOF 继续为 2 * MAX_NODES。
- solve_linear_system 的签名固定为：
  FemStatus solve_linear_system(const double matrix[MAX_DOF][MAX_DOF], const double rhs[MAX_DOF], int size, double solution[MAX_DOF]);
- 输入矩阵和右端项只读；成功时只写入 solution[0..size)，并清零 solution[size..MAX_DOF)。
- solution 非空时，所有参数检查前清零完整 MAX_DOF 容量；任何失败路径都保持全零。
- size <= 0 或任一必要指针为空返回 FEM_INVALID_ARGUMENT；size > MAX_DOF 返回 FEM_CAPACITY_EXCEEDED。
- 有效输入区域包含 NaN 或无穷值时返回 FEM_INVALID_ARGUMENT。
- 奇异或近奇异矩阵返回新增的 FEM_SINGULAR_MATRIX，不得产生部分解。
- 使用独立的 SOLVER_TOL = 1.0e-12，主元阈值为 SOLVER_TOL * max(1.0, matrix_scale)；不得用 == 0 判断浮点主元。
- 使用部分主元交换：每个主元列选择当前列候选区域内绝对值最大的元素并交换行。
- 不引入动态内存、第三方矩阵库、自由度缩减、位移恢复、文件输入、后处理或主程序流程改造。
- Stage 1、Stage 2、Stage 3 既有算法和测试保持兼容。

---

## 文件结构

| 文件 | 职责 |
|---|---|
| include/config.h | 增加独立的 SOLVER_TOL 定义 |
| include/fem.h | 增加 FEM_SINGULAR_MATRIX 枚举项及状态文本兼容 |
| include/solver.h | 声明 Stage 4 线性方程组求解接口 |
| src/solver.c | 实现清零、输入检查、局部复制、部分主元消元和回代 |
| tests/test_stage4.c | 验证可解系统、换行、奇异矩阵、错误路径和输入保护 |
| docs/superpowers/specs/2026-08-10-gaussian-elimination-design.md | 已批准的 Stage 4 设计契约 |
| docs/superpowers/plans/2026-08-10-gaussian-elimination.md | 本实施计划和验收记录 |

## Task 1: 定义 Stage 4 契约并编写失败测试

**Files:**
- Modify: include/config.h
- Modify: include/fem.h
- Create: include/solver.h
- Create: tests/test_stage4.c

**Interfaces:**
- Consumes: 现有 config.h 的 MAX_DOF 和 fem.h 的 FemStatus。
- Produces: 后续 src/solver.c 必须实现的精确函数签名、状态码和测试输入。

- [ ] **Step 1: 增加容差、状态码和头文件契约**

在 include/config.h 的 GEOMETRY_TOL 后增加：

~~~c
#define SOLVER_TOL 1.0e-12
~~~

在 FemStatus 的 Stage 3 状态码后增加：

~~~c
FEM_SINGULAR_MATRIX
~~~

创建 include/solver.h：

~~~c
#ifndef SOLVER_H
#define SOLVER_H

#include "fem.h"

FemStatus solve_linear_system(
    const double matrix[MAX_DOF][MAX_DOF],
    const double rhs[MAX_DOF],
    int size,
    double solution[MAX_DOF]);

#endif
~~~

- [ ] **Step 2: 编写可执行的 Stage 4 测试契约**

测试文件保持现有测试风格，包含 math.h、stdio.h、string.h、solver.h 和 config.h，提供 expect_close、expect_status、expect_zero_vector 和 expect_matrix_unchanged 辅助函数。测试必须包含以下精确案例：

~~~c
static void test_two_by_two_system(void)
{
    const double matrix[MAX_DOF][MAX_DOF] = {
        {3.0, 2.0},
        {1.0, 2.0}
    };
    const double rhs[MAX_DOF] = {18.0, 14.0};
    double solution[MAX_DOF];

    expect_status(solve_linear_system(matrix, rhs, 2, solution), FEM_OK);
    expect_close(solution[0], 2.0);
    expect_close(solution[1], 6.0);
    expect_zero_vector(solution, 2);
}

static void test_partial_pivoting(void)
{
    const double matrix[MAX_DOF][MAX_DOF] = {
        {0.0, 2.0},
        {1.0, 3.0}
    };
    const double rhs[MAX_DOF] = {4.0, 7.0};
    double solution[MAX_DOF];

    expect_status(solve_linear_system(matrix, rhs, 2, solution), FEM_OK);
    expect_close(solution[0], 1.0);
    expect_close(solution[1], 2.0);
}

static void test_three_by_three_system(void)
{
    const double matrix[MAX_DOF][MAX_DOF] = {
        {1.0, 2.0, 3.0},
        {0.0, 1.0, 4.0},
        {5.0, 6.0, 0.0}
    };
    const double rhs[MAX_DOF] = {14.0, 14.0, 17.0};
    double solution[MAX_DOF];

    expect_status(solve_linear_system(matrix, rhs, 3, solution), FEM_OK);
    expect_close(solution[0], 1.0);
    expect_close(solution[1], 2.0);
    expect_close(solution[2], 3.0);
}
~~~

另外实现以下测试：
- test_singular_matrix：使用 {{1,2},{2,4}} 和 {3,6}，预填解向量非零，断言返回 FEM_SINGULAR_MATRIX 且完整 MAX_DOF 输出为零；
- test_invalid_arguments：覆盖空矩阵、空右端项、空输出、size == 0、size == -1 和 size == MAX_DOF + 1；
- test_nonfinite_input：将有效矩阵项设为 NAN，再将右端项设为 INFINITY，均断言 FEM_INVALID_ARGUMENT 和全零输出；
- test_input_unchanged：复制 3×3 矩阵和右端项，求解后逐项比较原始数据；
- test_tail_is_zero：预填整个输出数组后求解 2×2 系统，断言索引 2..MAX_DOF-1 全为零；
- test_status_message：断言 fem_status_message(FEM_SINGULAR_MATRIX) 返回非空字符串。

测试 main 只有全部断言通过才输出 Stage 4 tests passed.。

- [ ] **Step 3: 运行失败测试并确认失败原因**

运行：

~~~powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage4.c src\fem.c -Iinclude -o "$env:TEMP\c_fe_stage4_contract.exe" -lm
~~~

预期：编译阶段因 solve_linear_system 尚无实现而链接失败；若出现其他头文件或类型错误，先修正契约文件，不能添加空实现掩盖失败。

- [ ] **Step 4: 提交契约和失败测试**

~~~powershell
git add include/config.h include/fem.h include/solver.h tests/test_stage4.c
git commit -m "test: define stage 4 gaussian elimination contract"
~~~

## Task 2: 实现带部分主元的高斯消元

**Files:**
- Create: src/solver.c
- Modify: include/fem.h，补充 FEM_SINGULAR_MATRIX 的可读状态文本

**Interfaces:**
- Consumes: Task 1 的 solve_linear_system 签名、MAX_DOF、SOLVER_TOL 和 FemStatus。
- Produces: 可被 Stage 4 测试和未来 Stage 5 复用的只读输入线性求解器。

- [ ] **Step 1: 实现清零、复制和有限性检查辅助函数**

在 src/solver.c 中使用 math.h 和 stddef.h，实现以下内部职责：

~~~c
static void clear_solution(double solution[MAX_DOF]);
static int is_finite_system(const double matrix[MAX_DOF][MAX_DOF],
                            const double rhs[MAX_DOF],
                            int size);
static double matrix_scale(const double matrix[MAX_DOF][MAX_DOF], int size);
~~~

clear_solution 清零完整容量；is_finite_system 只检查有效区域；matrix_scale 返回有效矩阵项绝对值的最大值。

- [ ] **Step 2: 实现参数检查和局部工作区复制**

solve_linear_system 必须先执行：

~~~c
if (solution != NULL) {
    clear_solution(solution);
}
if (matrix == NULL || rhs == NULL || solution == NULL || size <= 0) {
    return FEM_INVALID_ARGUMENT;
}
if (size > MAX_DOF) {
    return FEM_CAPACITY_EXCEEDED;
}
if (!is_finite_system(matrix, rhs, size)) {
    return FEM_INVALID_ARGUMENT;
}
~~~

然后将 matrix[0..size)、rhs[0..size) 复制到 work_matrix 与 work_rhs，不能直接在输入数组上消元。

- [ ] **Step 3: 实现部分主元前向消元**

对每个 pivot = 0..size-1：
1. 在行 pivot..size-1 中寻找当前列绝对值最大的候选行；
2. 计算 pivot_threshold = SOLVER_TOL * fmax(1.0, matrix_scale)；
3. 若最大绝对值 <= pivot_threshold，返回 FEM_SINGULAR_MATRIX；
4. 将候选行与当前行交换；
5. 对每个 row = pivot + 1..size-1，计算消元因子并更新当前行的剩余列及右端项。

更新从当前主元列开始，随后将已消去的列显式置为 0.0，避免残余舍入值影响回代。

- [ ] **Step 4: 实现回代和状态文本**

从 row = size - 1 递减到 0，使用：

~~~c
solution[row] = (work_rhs[row] - sum) / work_matrix[row][row];
~~~

回代前再次检查对角主元是否不超过同一阈值；失败时清零并返回 FEM_SINGULAR_MATRIX。成功后保持输出尾部零值。

在 fem_status_message 中加入：

~~~c
case FEM_SINGULAR_MATRIX:
    return "matrix is singular or ill-conditioned";
~~~

- [ ] **Step 5: 运行 Stage 4 测试并确认通过**

~~~powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage4.c src\solver.c src\fem.c -Iinclude -o "$env:TEMP\c_fe_stage4_tests.exe" -lm
& "$env:TEMP\c_fe_stage4_tests.exe"
~~~

预期输出 Stage 4 tests passed.，进程退出码为 0。

- [ ] **Step 6: 提交实现**

~~~powershell
git add src/solver.c include/fem.h
git commit -m "feat: add partial-pivot gaussian elimination"
~~~

## Task 3: 完成回归验证并记录验收

**Files:**
- Modify: docs/superpowers/plans/2026-08-10-gaussian-elimination.md，记录实际提交、命令和结果
- No changes: src/main.c、Docker 配置，以及 Stage 1/2/3 测试文件

**Interfaces:**
- Consumes: Task 2 的 solver 实现和全部既有阶段接口。
- Produces: 可审查的 Stage 4 验收记录和干净的提交范围。

- [ ] **Step 1: 编译并运行全部阶段测试**

使用 UCRT64 GCC 依次运行下列四组命令，每条编译和运行命令都检查 LASTEXITCODE == 0：

~~~powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage1.c src\fem.c -Iinclude -o "$env:TEMP\c_fe_stage1.exe" -lm
& "$env:TEMP\c_fe_stage1.exe"
gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage2.c src\fem.c -Iinclude -o "$env:TEMP\c_fe_stage2.exe" -lm
& "$env:TEMP\c_fe_stage2.exe"
gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage3.c src\fem.c -Iinclude -o "$env:TEMP\c_fe_stage3.exe" -lm
& "$env:TEMP\c_fe_stage3.exe"
gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage4.c src\solver.c src\fem.c -Iinclude -o "$env:TEMP\c_fe_stage4.exe" -lm
& "$env:TEMP\c_fe_stage4.exe"
~~~

四个测试分别应输出 Stage 1, Stage 2, Stage 3, Stage 4 passed.

- [ ] **Step 2: 运行现有 Stage 1 示例程序**

~~~powershell
gcc -std=c11 -Wall -Wextra -pedantic src\main.c src\fem.c -Iinclude -o "$env:TEMP\c_fe_stage1_demo.exe" -lm
& "$env:TEMP\c_fe_stage1_demo.exe"
~~~

示例程序必须退出码为 0，并继续输出既有单元长度、方向余弦和 4×4 刚度矩阵。

- [ ] **Step 3: 检查范围、禁止项和格式**

运行：

~~~powershell
git diff --check
git diff --name-status origin/stage3-loads-constraints..HEAD
rg -n "build_reduced_system|Kff|Uf|displacement|solver\.c|solver\.h" src include tests
~~~

允许出现 Stage 4 的 solver.c、solver.h、solve_linear_system 和测试引用；不得新增自由度缩减、位移恢复、文件输入、后处理或动态内存模块。确认 git status --short 只包含计划文档自身的验收记录变更。

- [ ] **Step 4: 在计划中记录验收结果并提交文档**

记录实际使用的编译器版本、每个测试退出码、git diff --check 结果、提交 ID 和任何明确延期的非阻塞观察项，然后提交：

~~~powershell
git add docs/superpowers/plans/2026-08-10-gaussian-elimination.md
git commit -m "docs: record stage 4 verification"
~~~

## Self-Review Checklist

- [ ] 设计文档中的接口、状态码、容差和失败清零语义全部有对应任务。
- [ ] 测试覆盖普通求解、部分主元换行、3×3 回代、奇异矩阵、非有限输入、参数错误、输入不变和输出尾部清零。
- [ ] 没有把 Stage 5 的自由度缩减或位移恢复提前加入 Stage 4。
- [ ] Stage 1、Stage 2、Stage 3 既有测试和主程序输出保持兼容。
- [ ] 所有编译使用 -std=c11 -Wall -Wextra -pedantic，且命令退出码被明确检查。
- [ ] 计划中的实际提交 ID、测试结果和范围检查结果在完成后补齐。

## Task 3 Acceptance Record

- Verification date: 2026-08-10; workspace: `C:\Users\jking1\Desktop\my-project\c_FE-stage4-gaussian-elimination`.
- Baseline before documentation: `e1e6b2b70e3882fff89b696b0a0680f3c871e732`.
- Acceptance-record commit: `3b4e114984294847adf34e5f5b99874bebacdb8b` (`docs: record stage 4 verification`).
- Compiler: UCRT64 GCC 16.1.0 (`gcc.exe (Rev5, Built by MSYS2 project) 16.1.0`).
- All requested test compile and run commands used `-std=c11 -Wall -Wextra -pedantic`, placed executables under `%TEMP%`, and exited 0:
  - Stage 1: `Stage 1 tests passed.`
  - Stage 2: `Stage 2 tests passed.`
  - Stage 3: `Stage 3 tests passed.`
  - Stage 4: `Stage 4 tests passed.`
- Stage 1 example compile and run exited 0. Output retained the unit length, direction cosines, and 4x4 element stiffness matrix.
- `git diff --check`: exit 0, no output.
- `git diff --name-status origin/stage3-loads-constraints..HEAD`: exit 0; listed the pre-existing Stage 4 implementation/specification baseline files, not a Task 3 source change.
- The required reference scan returned exit 0 with only allowed `solver.h`/`solver.c` references. The precise forbidden API and Stage 5 concept scans returned exit 1 for no matches; scripts explicitly interpreted that as the expected pass result.
- Protected-file scan: no changes to `src/main.c`, Docker configuration, or Stage 1/2/3 test files. The Task 3 worktree was clean before this documentation update.
- Temporary executables were removed and verified absent.
- Non-blocking observation: GCC emitted existing `-Wmissing-field-initializers` warnings in Stage 1/2 tests and `src/main.c`; all compiles and runs still exited 0.
- Full command transcript: `.superpowers/sdd/2026-08-10-gaussian-elimination/task-3-report.md`.

Task 3 acceptance steps are complete after this record is committed.
