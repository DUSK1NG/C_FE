# Stage 5：自由度缩减与位移回代实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (\`- [ ]\`) syntax for tracking.

**Goal:** 在固定容量二维桁架模型中新增约束系统求解接口，构造自由自由度子系统并将自由位移回填到完整位移向量。

**Architecture:** 在 src/fem.c 中增加 Stage 5 高层编排函数和固定容量校验/缩减逻辑，在 src/solver.c 中复用现有的 solve_linear_system()。函数只读原始总体矩阵、荷载向量和 DOF 集合，使用局部固定容量数组构造 Kff、Ff，成功后回填自由位移，失败时保持完整输出为零。

**Tech Stack:** C11、GCC/MinGW-w64、固定容量 C 数组、FemStatus、现有 Stage 4 高斯消元器、PowerShell 验证脚本。

## Global Constraints

- 使用 C11、double 和现有 MAX_DOF 固定容量，不引入动态内存或第三方矩阵库。
- 公共接口签名固定为：
  FemStatus solve_constrained_system(const double global_k[MAX_DOF][MAX_DOF], const double force[MAX_DOF], const int free_dofs[MAX_DOF], int free_count, const int constrained_dofs[MAX_DOF], int constrained_count, double displacement[MAX_DOF]);
- displacement 非空时，函数在所有参数检查前清零完整 MAX_DOF 容量；所有失败路径保持完整输出为零。
- 所有输入矩阵、荷载向量和 DOF 数组只读，调用前后内容必须一致。
- 负计数、空指针、越界 DOF、重复 DOF 和自由/约束集合交集返回 FEM_INVALID_ARGUMENT。
- 单个计数或计数之和超过 MAX_DOF 返回 FEM_CAPACITY_EXCEEDED。
- free_count == 0 在其他参数和 DOF 集合合法时返回 FEM_OK，不调用线性求解器，完整位移保持为零。
- 线性求解器返回的状态原样传递；缩减矩阵奇异时返回 FEM_SINGULAR_MATRIX。
- 不修改 src/main.c、include/model.h、include/solver.h、src/solver.c、Docker 文件或已有 Stage 1～4 测试。

---

## 文件结构

| 文件 | 职责 |
|---|---|
| include/fem.h | 声明 Stage 5 公共接口 |
| src/fem.c | 校验 DOF 集合、提取 Kff/Ff、调用求解器、回填位移 |
| tests/test_stage5.c | Stage 5 契约、成功路径、失败路径和输入保护测试 |
| docs/superpowers/specs/2026-08-10-dof-reduction-design.md | 已批准设计契约 |
| docs/superpowers/plans/2026-08-10-dof-reduction.md | 本实施计划及实际验收记录 |

## Task 1: 定义公共接口并编写失败优先的契约测试

**Files:**

- Modify: include/fem.h，在现有 Stage 3 接口后声明 solve_constrained_system()。
- Create: tests/test_stage5.c。

**Interfaces:**

- Consumes: MAX_DOF、FemStatus、Stage 4 FEM_SINGULAR_MATRIX 和现有 solve_linear_system() 的行为。
- Produces: 后续 src/fem.c 必须实现的精确函数签名、输出清零规则、DOF 校验规则和回代结果。

- [ ] **Step 1: 添加 Stage 5 函数声明**

在 include/fem.h 的 identify_dofs() 声明之后加入：

~~~c
FemStatus solve_constrained_system(
    const double global_k[MAX_DOF][MAX_DOF],
    const double force[MAX_DOF],
    const int free_dofs[MAX_DOF],
    int free_count,
    const int constrained_dofs[MAX_DOF],
    int constrained_count,
    double displacement[MAX_DOF]);
~~~

- [ ] **Step 2: 写入测试辅助函数和可解模型**

tests/test_stage5.c 包含 config.h、fem.h、math.h、stdio.h、stdlib.h、string.h，使用现有 Stage 4 测试风格，定义 TEST_TOL = 1.0e-12，并提供 expect_status、expect_close、expect_zero_vector、fill_matrix、fill_vector 五个辅助函数。

可解模型使用自由度 {2, 4, 5} 和约束自由度 {0, 1, 3}。将自由子矩阵设置为：

~~~text
Kff = [ 4  1  0 ]       Ff = [ 6 ]
      [ 1  3  1 ]            [10 ]
      [ 0  1  2 ]            [ 8 ]
~~~

其期望自由位移为 [1, 2, 3]。测试初始化完整矩阵和荷载为零，只在全局索引 {2, 4, 5} 对应位置写入上面的子矩阵和右端项。

- [ ] **Step 3: 编写成功路径测试**

实现 test_reduces_and_recovers_displacement()：

1. 以 77.0 填充 displacement，调用 solve_constrained_system()。
2. 断言返回 FEM_OK。
3. 断言 displacement[2] == 1.0、displacement[4] == 2.0、displacement[5] == 3.0。
4. 断言 displacement[0]、[1]、[3] 和其余尾部均为 0.0。
5. 复制矩阵、荷载和两个 DOF 数组，调用完成后逐项比较，证明所有输入未改变。

实现 test_preserves_supplied_free_order()：将自由度数组改为 {5, 2, 4}，将右端项按该顺序提供为 {8, 6, 10}，验证回填后仍得到 U[2] = 1.0、U[4] = 2.0、U[5] = 3.0，而不是把解按数组位置错误写入。

- [ ] **Step 4: 编写边界和失败路径测试**

在同一个测试文件中实现以下函数，每个函数只验证一个行为：

- test_zero_free_dofs_returns_zero_solution()：free_count = 0、合法约束数组，返回 FEM_OK，输出全零。
- test_null_argument_clears_solution()：分别用 NULL 传入 global_k、force、free_dofs、constrained_dofs 和 displacement；对非空输出的情况断言 FEM_INVALID_ARGUMENT 和全零结果。
- test_negative_counts_clear_solution()：free_count = -1 或 constrained_count = -1 返回 FEM_INVALID_ARGUMENT，输出全零。
- test_out_of_range_dof_clears_solution()：自由度数组含 MAX_DOF，返回 FEM_INVALID_ARGUMENT，输出全零。
- test_duplicate_or_overlapping_dof_clears_solution()：自由度数组含重复项，以及自由/约束数组含同一 DOF，均返回 FEM_INVALID_ARGUMENT，输出全零。
- test_count_over_capacity_clears_solution()：free_count = MAX_DOF + 1 返回 FEM_CAPACITY_EXCEEDED；free_count = MAX_DOF 且 constrained_count = 1 验证总计数超限，同样清零输出。
- test_singular_reduced_system_clears_solution()：将自由子矩阵的一行设为零，调用后断言 FEM_SINGULAR_MATRIX 和全零输出。

失败测试在实现缺失时必须先编译到链接阶段并因 solve_constrained_system 未定义而失败；不能把“测试文件无法编译”当作有效 RED 结果。

- [ ] **Step 5: 运行 RED 验证**

在 Stage5 Worktree 中执行：

~~~powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
$gcc = "C:\msys64\ucrt64\bin\gcc.exe"
$out = Join-Path $env:TEMP "c_fe_stage5_contract_red.exe"
& $gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage5.c src\fem.c src\solver.c -Iinclude -o $out -lm
~~~

预期：编译进入链接阶段后因 solve_constrained_system 未定义而返回非零；如果测试在实现前通过，先修正测试契约。完成 RED 记录后删除该临时可执行文件。

- [ ] **Step 6: 提交契约和失败测试**

~~~powershell
git add include\fem.h tests\test_stage5.c
git commit -m "test: define stage 5 dof reduction contract"
~~~

预期：提交只包含公共声明和 Stage 5 测试，不包含 src/fem.c 实现。

## Task 2: 实现自由度缩减、线性求解和位移回代

**Files:**

- Modify: src/fem.c。
- Test: tests/test_stage5.c。

**Interfaces:**

- Consumes: Task 1 的 solve_constrained_system() 声明、Stage 3 自由度数组和 Stage 4 solve_linear_system()。
- Produces: 固定容量的 Kff/Ff 构造、完整位移回代和错误状态传递。

- [ ] **Step 1: 引入 Stage 4 求解器声明**

在 src/fem.c 的 include 区域加入：

~~~c
#include "solver.h"
~~~

保持既有 Stage 1～3 函数和状态消息不变。

- [ ] **Step 2: 实现固定容量 DOF 分区校验**

在 src/fem.c 中新增只供本文件使用的辅助函数：

~~~c
static FemStatus validate_dof_partition(
    const int free_dofs[MAX_DOF],
    int free_count,
    const int constrained_dofs[MAX_DOF],
    int constrained_count)
{
    int seen[MAX_DOF] = {0};
    int i;
    int dof;

    if (free_count < 0 || constrained_count < 0) {
        return FEM_INVALID_ARGUMENT;
    }
    if (free_count > MAX_DOF || constrained_count > MAX_DOF ||
        free_count + constrained_count > MAX_DOF) {
        return FEM_CAPACITY_EXCEEDED;
    }
    for (i = 0; i < free_count; ++i) {
        dof = free_dofs[i];
        if (dof < 0 || dof >= MAX_DOF || seen[dof] != 0) {
            return FEM_INVALID_ARGUMENT;
        }
        seen[dof] = 1;
    }
    for (i = 0; i < constrained_count; ++i) {
        dof = constrained_dofs[i];
        if (dof < 0 || dof >= MAX_DOF || seen[dof] != 0) {
            return FEM_INVALID_ARGUMENT;
        }
        seen[dof] = 1;
    }
    return FEM_OK;
}
~~~

调用前先检查各指针非空，因此该辅助函数只处理计数、范围、重复项和交集。单个计数超限后再计算总计数，避免极端 int 输入造成加法溢出。

- [ ] **Step 3: 实现最小 solve_constrained_system()**

在 identify_dofs() 后加入实现，使用固定容量局部数组：

~~~c
FemStatus solve_constrained_system(
    const double global_k[MAX_DOF][MAX_DOF],
    const double force[MAX_DOF],
    const int free_dofs[MAX_DOF],
    int free_count,
    const int constrained_dofs[MAX_DOF],
    int constrained_count,
    double displacement[MAX_DOF])
{
    double reduced_matrix[MAX_DOF][MAX_DOF] = {{0.0}};
    double reduced_force[MAX_DOF] = {0.0};
    double free_solution[MAX_DOF] = {0.0};
    FemStatus status;
    int i;
    int j;

    if (displacement != NULL) {
        for (i = 0; i < MAX_DOF; ++i) {
            displacement[i] = 0.0;
        }
    }
    if (global_k == NULL || force == NULL || free_dofs == NULL ||
        constrained_dofs == NULL || displacement == NULL) {
        return FEM_INVALID_ARGUMENT;
    }

    status = validate_dof_partition(free_dofs, free_count,
                                    constrained_dofs, constrained_count);
    if (status != FEM_OK) {
        return status;
    }
    if (free_count == 0) {
        return FEM_OK;
    }

    for (i = 0; i < free_count; ++i) {
        reduced_force[i] = force[free_dofs[i]];
        for (j = 0; j < free_count; ++j) {
            reduced_matrix[i][j] =
                global_k[free_dofs[i]][free_dofs[j]];
        }
    }

    status = solve_linear_system(reduced_matrix, reduced_force,
                                 free_count, free_solution);
    if (status != FEM_OK) {
        return status;
    }
    for (i = 0; i < free_count; ++i) {
        displacement[free_dofs[i]] = free_solution[i];
    }
    return FEM_OK;
}
~~~

实现必须保留以下行为：所有输出在失败时为零，约束 DOF 永不写入，原始输入数组不传入 Stage 4 求解器的可变工作区。

- [ ] **Step 4: 运行 Stage 5 GREEN 测试**

~~~powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
$gcc = "C:\msys64\ucrt64\bin\gcc.exe"
$out = Join-Path $env:TEMP "c_fe_stage5_tests.exe"
& $gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage5.c src\fem.c src\solver.c -Iinclude -o $out -lm
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $out
$result = $LASTEXITCODE
Remove-Item -LiteralPath $out -Force -ErrorAction SilentlyContinue
exit $result
~~~

预期输出为 Stage 5 tests passed. 且退出码为 0。若失败，修复实现而不是放宽测试；保持每次修改后先运行 Stage 5 测试，再运行回归测试。

- [ ] **Step 5: 提交实现**

~~~powershell
git add src\fem.c
git commit -m "feat: reduce constrained system and recover displacements"
~~~

预期：实现提交只包含 src/fem.c；Task 1 的契约提交保持独立。

## Task 3: 回归验证并记录验收结果

**Files:**

- Modify: docs/superpowers/plans/2026-08-10-dof-reduction.md，只在验证完成后追加执行结果。
- Test: tests/test_stage1.c、tests/test_stage2.c、tests/test_stage3.c、tests/test_stage4.c、tests/test_stage5.c、src/main.c。

**Interfaces:**

- Consumes: Task 1 的测试契约和 Task 2 的实现。
- Produces: 通过完整回归、范围检查和实际命令记录的 Stage 5 分支。

- [ ] **Step 1: 运行全阶段回归和示例**

使用临时目录保存可执行文件，避免把构建产物写入 Worktree：

~~~powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
$gcc = "C:\msys64\ucrt64\bin\gcc.exe"
$temp = Join-Path $env:TEMP "c_fe_stage5_verify"
New-Item -ItemType Directory -Force -Path $temp | Out-Null
$cases = @(
    @{ Source = "tests\test_stage1.c"; Output = "stage1.exe" },
    @{ Source = "tests\test_stage2.c"; Output = "stage2.exe" },
    @{ Source = "tests\test_stage3.c"; Output = "stage3.exe" },
    @{ Source = "tests\test_stage4.c"; Output = "stage4.exe" },
    @{ Source = "tests\test_stage5.c"; Output = "stage5.exe" }
)
foreach ($case in $cases) {
    $out = Join-Path $temp $case.Output
    & $gcc -std=c11 -Wall -Wextra -pedantic $case.Source src\fem.c src\solver.c -Iinclude -o $out -lm
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $out
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
$demo = Join-Path $temp "demo.exe"
& $gcc -std=c11 -Wall -Wextra -pedantic src\main.c src\fem.c src\solver.c -Iinclude -o $demo -lm
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $demo
$result = $LASTEXITCODE
Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
exit $result
~~~

预期包含 Stage 1 tests passed.、Stage 2 tests passed.、Stage 3 tests passed.、Stage 4 tests passed.、Stage 5 tests passed. 和既有 Stage 1 示例输出，所有命令退出码为 0。

- [ ] **Step 2: 检查差异、范围和临时产物**

~~~powershell
git diff --check
rg --files src include tests
git status --short
git log --oneline --decorate -6
~~~

预期：没有空白错误；源码范围只新增 Stage 5 计划中的声明、实现和测试；git status --short 不显示临时可执行文件。

- [ ] **Step 3: 记录实际执行结果**

在本计划末尾追加“执行结果”章节，填写实际日期、编译器路径、每条测试输出、测试退出码、提交 ID、警告和环境限制。只有实际返回 0 的命令才标记为 [x]。如果 Docker 引擎不可用，记录为未执行，不宣称 Docker 验收通过。

- [ ] **Step 4: 提交验证记录**

~~~powershell
git add docs\superpowers\plans\2026-08-10-dof-reduction.md
git commit -m "docs: record stage 5 verification"
~~~

预期：Worktree 干净，提交历史包含契约测试、实现和验证记录三个 Stage 5 变更提交。

## 自审清单

- 设计文档的所有目标均有 Task 1～3 对应步骤：接口、缩减、回代、清零、状态传递、输入不变性、固定容量和回归。
- 契约测试先于实现运行，并预期在链接阶段因缺少实现而失败。
- 计划中的函数名、参数顺序、状态码和 MAX_DOF 使用方式在所有任务中保持一致。
- 没有引入动态内存、非零支座位移、反力计算、主程序修改或 Docker 代码变更。
- 每个代码步骤都给出目标文件、具体 API、命令和预期结果，没有依赖未定义的后续接口。

## 执行结果（2026-08-10）

- [x] Task 3 Step 1: 使用 `C:\\msys64\\ucrt64\\bin\\gcc.exe` 执行计划中的完整 PowerShell 回归脚本，结果退出码为 0。临时可执行文件位于 `$temp` （`Join-Path $env:TEMP 'c_fe_stage5_verify'`）并在脚本结束时删除。
  - `tests\\test_stage1.c`: `Stage 1 tests passed.`
  - `tests\\test_stage2.c`: `Stage 2 tests passed.`
  - `tests\\test_stage3.c`: `Stage 3 tests passed.`
  - `tests\\test_stage4.c`: `Stage 4 tests passed.`
  - `tests\\test_stage5.c`: `Stage 5 contract tests passed.`
  - `src\\main.c` 示例已运行：输出 `Stage 1: single 2D truss element`、`Length = 943.398113205660 mm`、`c = 0.529998940003` 和 `s = 0.847998304005`。
- [x] Task 3 Step 2: `git diff --check` 退出码 0；`rg --files src include tests` 退出码 0；`git status --short` 在记录前为空，未发现临时 `.exe` 产物。Git 命令以临时 `safe.directory=C:/Users/jking1/Desktop/my-project/c_FE-stage5-dof-reduction` 设置执行，未更改全局 Git 配置。
- [x] 范围检查: `git diff --name-status caca61a..HEAD` 退出码 0，输出包含 `include/fem.h`、`src/fem.c`、`tests/test_stage5.c` 以及验证记录文档 `docs/superpowers/plans/2026-08-10-dof-reduction.md`。代码/测试文件范围仅为前三者；第四者仅是 Task 3 验证记录。提交链为 `9e7da5b test: define stage 5 dof reduction contract`、`6983464 feat: reduce constrained system and recover displacements`、`6d6a612 test: correct stage 5 fixture construction` 和 `af9da69 docs: record stage 5 verification`。
- [x] 警告记录: 编译仍有 14 条 `-Wmissing-field-initializers` 警告，均指向既有 `Node.fx` 未显式初始化：`tests/test_stage1.c` 6 条、`tests/test_stage2.c` 6 条、`src/main.c` 2 条。它们未导致编译或运行失败。
- Docker 验收未执行: `docker version` 无法识别 `docker` 命令（该环境未提供 Docker CLI/引擎）。本记录不宣称 Docker 验收通过。

### 审查修正与重新执行（2026-08-10）

以下是实际重新执行的 PowerShell 命令。其中 `$env:Path`、`$gcc` 和 `$temp` 为实际取值；临时目录只通过 `$env:TEMP` 与 `Join-Path` 构造。脚本在编译前清理 `$temp`，运行后删除该目录。

~~~powershell
$env:Path = 'C:\msys64\ucrt64\bin;' + $env:Path
$gcc = 'C:\msys64\ucrt64\bin\gcc.exe'
$temp = Join-Path $env:TEMP 'c_fe_stage5_verify'
$safe = 'safe.directory=C:/Users/jking1/Desktop/my-project/c_FE-stage5-dof-reduction'
Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $temp | Out-Null

$stage1 = Join-Path $temp 'stage1.exe'
& $gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage1.c src\fem.c src\solver.c -Iinclude -o $stage1 -lm
$stage1Compile = $LASTEXITCODE
& $stage1
$stage1Run = $LASTEXITCODE

$stage2 = Join-Path $temp 'stage2.exe'
& $gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage2.c src\fem.c src\solver.c -Iinclude -o $stage2 -lm
$stage2Compile = $LASTEXITCODE
& $stage2
$stage2Run = $LASTEXITCODE

$stage3 = Join-Path $temp 'stage3.exe'
& $gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage3.c src\fem.c src\solver.c -Iinclude -o $stage3 -lm
$stage3Compile = $LASTEXITCODE
& $stage3
$stage3Run = $LASTEXITCODE

$stage4 = Join-Path $temp 'stage4.exe'
& $gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage4.c src\fem.c src\solver.c -Iinclude -o $stage4 -lm
$stage4Compile = $LASTEXITCODE
& $stage4
$stage4Run = $LASTEXITCODE

$stage5 = Join-Path $temp 'stage5.exe'
& $gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage5.c src\fem.c src\solver.c -Iinclude -o $stage5 -lm
$stage5Compile = $LASTEXITCODE
& $stage5
$stage5Run = $LASTEXITCODE

$demo = Join-Path $temp 'demo.exe'
& $gcc -std=c11 -Wall -Wextra -pedantic src\main.c src\fem.c src\solver.c -Iinclude -o $demo -lm
$demoCompile = $LASTEXITCODE
& $demo
$demoRun = $LASTEXITCODE

git -c $safe diff --check
$diffCheck = $LASTEXITCODE
rg --files src include tests
$rgScope = $LASTEXITCODE
git -c $safe status --short
$statusCheck = $LASTEXITCODE
git -c $safe diff --name-status caca61a..HEAD
$rangeCheck = $LASTEXITCODE

Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
~~~

实际重新执行输出与退出码：

- [x] Stage 1: 编译 `stage1_compile_exit=0`；运行 `Stage 1 tests passed.`、`stage1_run_exit=0`。
- [x] Stage 2: 编译 `stage2_compile_exit=0`；运行 `Stage 2 tests passed.`、`stage2_run_exit=0`。
- [x] Stage 3: 编译 `stage3_compile_exit=0`；运行 `Stage 3 tests passed.`、`stage3_run_exit=0`。
- [x] Stage 4: 编译 `stage4_compile_exit=0`；运行 `Stage 4 tests passed.`、`stage4_run_exit=0`。
- [x] Stage 5: 编译 `stage5_compile_exit=0`；运行 `Stage 5 contract tests passed.`、`stage5_run_exit=0`。
- [x] Stage 1 demo: 编译 `demo_compile_exit=0`；运行 `Stage 1: single 2D truss element`、`Length = 943.398113205660 mm`、`c = 0.529998940003`、`s = 0.847998304005`、`demo_run_exit=0`。
- [x] `git diff --check`: 无输出，`git_diff_check_exit=0`。
- [x] `rg --files src include tests`: 输出 12 个源码/头文件/测试文件，`rg_scope_exit=0`。
- [x] `git status --short`: 无输出（本次文档修正之前），`git_status_exit=0`。
- [x] `git diff --name-status caca61a..HEAD`: `M docs/superpowers/plans/2026-08-10-dof-reduction.md`、`M include/fem.h`、`M src/fem.c`、`A tests/test_stage5.c`，`git_scope_range_exit=0`。
此范围中的代码/测试变更仅包括 `include/fem.h`、`src/fem.c` 和 `tests/test_stage5.c`；`docs/superpowers/plans/2026-08-10-dof-reduction.md` 是验证记录文档，不属于代码或测试范围。`af9da69` （`docs: record stage 5 verification`）是本次审查修正前的验证提交，已在本执行结果中明确记录。

审查修正提交：`4a3ac1d` （`docs: correct stage 5 verification record`）。此提交包含可直接执行的命令记录、逐项实际退出码以及细分的代码/测试与文档范围说明。
