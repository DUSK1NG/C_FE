# Stage 3：荷载与约束 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在固定容量二维桁架模型中保存节点荷载和方向约束，并将它们映射为总体荷载向量、自由自由度集合和约束自由度集合。

**Architecture:** 扩展现有 Node 结构，直接保存 fx、fy、fix_x、fix_y。在现有 src/fem.c 中增加两个独立的数据转换函数：build_force_vector 负责荷载映射，identify_dofs 负责自由度分类；两个函数都先完整校验输入，再写入固定容量输出，失败时返回明确状态并清空可写输出。

**Tech Stack:** C11、MSYS2 UCRT64 GCC 16.1.0、<math.h>、固定容量数组、现有 Stage 1/Stage 2 测试风格。

## Global Constraints

- 使用 C11、double 和固定容量 MAX_NODES = 10、MAX_DOF = 2 * MAX_NODES。
- fx、fy 使用 N；fix_x、fix_y 只能使用 0 或 1。
- 内部节点索引 i 的自由度始终为 2 * i 和 2 * i + 1，不使用 Node.id 重新编号。
- build_force_vector 的输出必须覆盖并清零整个 MAX_DOF 容量；identify_dofs 的数组和计数必须在失败时为空。
- NaN 或无穷荷载返回 FEM_INVALID_LOAD；非法约束标志返回 FEM_INVALID_CONSTRAINT。
- 不引入动态内存、第三方矩阵库、文件输入、求解器、位移字段或主程序流程改造。
- 不修改 src/main.c、Dockerfile、Compose 文件、Stage 1 测试或 Stage 2 测试的行为。
- 所有编译必须使用 -std=c11 -Wall -Wextra -pedantic，并确保 MSYS2 UCRT64 bin 目录在本次进程的 PATH 前部。

---

## 文件结构

| 文件 | 职责 |
|---|---|
| include/model.h | 扩展 Node 的节点荷载和约束字段 |
| include/fem.h | 增加 Stage 3 状态码和两个公共函数声明 |
| tests/test_stage3.c | 验证荷载映射、自由度分类、清零策略和错误状态 |
| src/fem.c | 实现两个 Stage 3 函数及新状态文本 |
| docs/superpowers/plans/2026-08-10-loads-constraints.md | 记录实施步骤、实际命令和验收结果 |

### Task 1: Define the Stage 3 contract and write failing tests

**Files:**
- Modify: include/model.h
- Modify: include/fem.h
- Create: tests/test_stage3.c

**Interfaces:**
- Consumes: existing Node, MAX_DOF, MAX_NODES, FemStatus, fem_status_message.
- Produces: Node.fx, Node.fy, Node.fix_x, Node.fix_y; FEM_INVALID_CONSTRAINT; FEM_INVALID_LOAD; declarations for build_force_vector and identify_dofs; a standalone Stage 3 test executable.

- [ ] Step 1: Extend Node without breaking existing initializers

In include/model.h, keep id, x, and y in their current order and append the four Stage 3 fields:

~~~c
typedef struct {
    int id;
    double x;
    double y;

    double fx;
    double fy;
    int fix_x;
    int fix_y;
} Node;
~~~

Do not add ux or uy; those belong to Stage 5. Existing three-field initializers must continue to compile with zero values for the appended fields.

- [ ] Step 2: Add statuses and public declarations

Append these statuses after FEM_CAPACITY_EXCEEDED in include/fem.h:

~~~c
    FEM_INVALID_CONSTRAINT,
    FEM_INVALID_LOAD
~~~

Add these declarations after assemble_global_stiffness:

~~~c
FemStatus build_force_vector(const Node *nodes,
                             int node_count,
                             double force[MAX_DOF]);

FemStatus identify_dofs(const Node *nodes,
                        int node_count,
                        int free_dofs[MAX_DOF],
                        int *free_count,
                        int constrained_dofs[MAX_DOF],
                        int *constrained_count);
~~~

- [ ] Step 3: Write the Stage 3 test contract

Create tests/test_stage3.c. It must contain the following concrete checks:

1. A three-node triangle with nodes
   {1, 0.0, 0.0, 125.0, 0.0, 1, 1},
   {2, 1000.0, 0.0, 0.0, 0.0, 0, 1},
   {3, 500.0, 800.0, 0.0, -10000.0, 0, 0}
   produces the force vector
   [125.0, 0.0, 0.0, 0.0, 0.0, -10000.0, 0.0, ..., 0.0].
2. The same constraints produce free_dofs [2, 4, 5], free_count 3, constrained_dofs [0, 1, 3], constrained_count 3.
3. Every unused force and DOF-array slot is zero.
4. A fix_x value of 2 returns FEM_INVALID_CONSTRAINT, sets both counts to zero, and clears both arrays.
5. A NAN or INFINITY load returns FEM_INVALID_LOAD and clears the full force vector.
6. MAX_NODES + 1 returns FEM_CAPACITY_EXCEEDED and clears writable outputs.
7. Null pointers and non-positive node_count return FEM_INVALID_ARGUMENT.
8. fem_status_message returns exactly "constraint flags must be 0 or 1" and "loads must be finite".
9. main prints "Stage 3 tests passed." on success.

Use the existing Stage 1/Stage 2 helper style. The test file must include math.h, stdio.h, stdlib.h, and string.h; use fabs with TEST_TOL = 1.0e-12; use NAN and INFINITY for invalid-load cases; and exit(EXIT_FAILURE) with a named diagnostic on every mismatch.

The core test functions must follow these concrete bodies; the helper functions only format the failure name:

~~~c
static void fill_int_array(int values[MAX_DOF], int count, int value)
{
    int i;

    for (i = 0; i < count; ++i) {
        values[i] = value;
    }
}

static void fill_double_array(double values[MAX_DOF], int count, double value)
{
    int i;

    for (i = 0; i < count; ++i) {
        values[i] = value;
    }
}

static void expect_zero_force(double force[MAX_DOF])
{
    int i;

    for (i = 0; i < MAX_DOF; ++i) {
        expect_close("force clear", force[i], 0.0);
    }
}

static void expect_zero_dofs(int dofs[MAX_DOF])
{
    int i;

    for (i = 0; i < MAX_DOF; ++i) {
        if (dofs[i] != 0) {
            fprintf(stderr, "FAIL: DOF array was not cleared at %d\n", i);
            exit(EXIT_FAILURE);
        }
    }
}

static void test_force_vector(void)
{
    const Node nodes[3] = {
        {1, 0.0,    0.0,   125.0,     0.0,     1, 1},
        {2, 1000.0, 0.0,   0.0,       0.0,     0, 1},
        {3, 500.0,  800.0, 0.0,  -10000.0,     0, 0}
    };
    static const double expected[6] = {
        125.0, 0.0, 0.0, 0.0, 0.0, -10000.0
    };
    double force[MAX_DOF];
    int i;

    expect_status("force vector",
                  build_force_vector(nodes, 3, force),
                  FEM_OK);
    for (i = 0; i < 6; ++i) {
        char name[32];
        snprintf(name, sizeof(name), "force[%d]", i);
        expect_close(name, force[i], expected[i]);
    }
    for (i = 6; i < MAX_DOF; ++i) {
        expect_close("force tail", force[i], 0.0);
    }
}

static void test_dof_sets(void)
{
    const Node nodes[3] = {
        {1, 0.0,    0.0, 0.0, 0.0, 1, 1},
        {2, 1000.0, 0.0, 0.0, 0.0, 0, 1},
        {3, 500.0,  800.0, 0.0, 0.0, 0, 0}
    };
    static const int expected_free[3] = {2, 4, 5};
    static const int expected_constrained[3] = {0, 1, 3};
    int free_dofs[MAX_DOF];
    int constrained_dofs[MAX_DOF];
    int free_count = 0;
    int constrained_count = 0;
    int i;

    expect_status("dof sets",
                  identify_dofs(nodes, 3,
                                free_dofs, &free_count,
                                constrained_dofs, &constrained_count),
                  FEM_OK);
    if (free_count != 3 || constrained_count != 3) {
        fprintf(stderr, "FAIL: unexpected DOF counts\n");
        exit(EXIT_FAILURE);
    }
    for (i = 0; i < 3; ++i) {
        if (free_dofs[i] != expected_free[i] ||
            constrained_dofs[i] != expected_constrained[i]) {
            fprintf(stderr, "FAIL: DOF mapping at index %d\n", i);
            exit(EXIT_FAILURE);
        }
    }
}

static void test_invalid_constraint_clears_outputs(void)
{
    const Node nodes[1] = {{1, 0.0, 0.0, 0.0, 0.0, 2, 0}};
    int free_dofs[MAX_DOF];
    int constrained_dofs[MAX_DOF];
    int free_count = 99;
    int constrained_count = 99;

    fill_int_array(free_dofs, MAX_DOF, 77);
    fill_int_array(constrained_dofs, MAX_DOF, 77);
    expect_status("invalid constraint",
                  identify_dofs(nodes, 1,
                                free_dofs, &free_count,
                                constrained_dofs, &constrained_count),
                  FEM_INVALID_CONSTRAINT);
    expect_zero_dofs(free_dofs);
    expect_zero_dofs(constrained_dofs);
    if (free_count != 0 || constrained_count != 0) {
        fprintf(stderr, "FAIL: invalid constraint counts\n");
        exit(EXIT_FAILURE);
    }
}

static void test_invalid_load_clears_vector(void)
{
    const Node nodes[2] = {
        {1, 0.0, 0.0, NAN, 0.0, 0, 0},
        {2, 1.0, 0.0, 0.0, INFINITY, 0, 0}
    };
    double force[MAX_DOF];

    fill_double_array(force, MAX_DOF, 77.0);
    expect_status("invalid load",
                  build_force_vector(nodes, 2, force),
                  FEM_INVALID_LOAD);
    expect_zero_force(force);
}
~~~

- [ ] Step 4: Run the new test before implementation

Run from the worktree using the installed compiler and a temporary process PATH:

~~~powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
$gcc = "C:\msys64\ucrt64\bin\gcc.exe"
$out = Join-Path $env:TEMP "c_fe_stage3_contract.exe"
& $gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage3.c src\fem.c -Iinclude -o $out -lm
~~~

Expected result: compilation reaches the link stage and fails with undefined references to build_force_vector and identify_dofs, proving the new tests exercise the not-yet-written implementation. Remove the explicitly created output if it exists:

~~~powershell
Remove-Item -LiteralPath (Join-Path $env:TEMP "c_fe_stage3_contract.exe") -Force -ErrorAction SilentlyContinue
~~~

- [ ] Step 5: Commit the contract and failing tests

~~~powershell
git add include\model.h include\fem.h tests\test_stage3.c
git commit -m "test: define stage 3 loads and constraints contract"
~~~

Expected result: one commit containing only the Node fields, public declarations/statuses, and Stage 3 tests.

### Task 2: Implement force-vector construction and DOF classification

**Files:**
- Modify: src/fem.c
- Test: tests/test_stage3.c

**Interfaces:**
- Consumes: Node fields from Task 1 and the exact declarations build_force_vector(...) and identify_dofs(...).
- Produces: working Stage 3 data transformations and status strings used by Task 3 and future Stage 4/5 code.

- [ ] Step 1: Add fixed-capacity clearing helpers

In src/fem.c, add these internal helpers near clear_global_matrix:

~~~c
static void clear_force_vector(double force[MAX_DOF])
{
    int i;

    for (i = 0; i < MAX_DOF; ++i) {
        force[i] = 0.0;
    }
}

static void clear_dof_array(int dofs[MAX_DOF])
{
    int i;

    for (i = 0; i < MAX_DOF; ++i) {
        dofs[i] = 0;
    }
}
~~~

Do not change the existing Stage 1 or Stage 2 helper behavior.

- [ ] Step 2: Implement build_force_vector with complete validation before writes

Add this function after assemble_global_stiffness:

~~~c
FemStatus build_force_vector(const Node *nodes,
                             int node_count,
                             double force[MAX_DOF])
{
    int i;

    if (force != NULL) {
        clear_force_vector(force);
    }
    if (nodes == NULL || force == NULL || node_count <= 0) {
        return FEM_INVALID_ARGUMENT;
    }
    if (node_count > MAX_NODES) {
        return FEM_CAPACITY_EXCEEDED;
    }

    for (i = 0; i < node_count; ++i) {
        if (!isfinite(nodes[i].fx) || !isfinite(nodes[i].fy)) {
            return FEM_INVALID_LOAD;
        }
    }

    for (i = 0; i < node_count; ++i) {
        force[2 * i] = nodes[i].fx;
        force[2 * i + 1] = nodes[i].fy;
    }

    return FEM_OK;
}
~~~

The existing <math.h> include supplies isfinite. Keep the output zeroed if any validation fails.

- [ ] Step 3: Implement identify_dofs with stable ascending order

Add this function after build_force_vector:

~~~c
FemStatus identify_dofs(const Node *nodes,
                        int node_count,
                        int free_dofs[MAX_DOF],
                        int *free_count,
                        int constrained_dofs[MAX_DOF],
                        int *constrained_count)
{
    int i;
    int dof;

    if (free_dofs != NULL) {
        clear_dof_array(free_dofs);
    }
    if (constrained_dofs != NULL) {
        clear_dof_array(constrained_dofs);
    }
    if (free_count != NULL) {
        *free_count = 0;
    }
    if (constrained_count != NULL) {
        *constrained_count = 0;
    }

    if (nodes == NULL || free_dofs == NULL || free_count == NULL ||
        constrained_dofs == NULL || constrained_count == NULL ||
        node_count <= 0) {
        return FEM_INVALID_ARGUMENT;
    }
    if (node_count > MAX_NODES) {
        return FEM_CAPACITY_EXCEEDED;
    }

    for (i = 0; i < node_count; ++i) {
        if ((nodes[i].fix_x != 0 && nodes[i].fix_x != 1) ||
            (nodes[i].fix_y != 0 && nodes[i].fix_y != 1)) {
            return FEM_INVALID_CONSTRAINT;
        }
    }

    for (i = 0; i < node_count; ++i) {
        dof = 2 * i;
        if (nodes[i].fix_x == 0) {
            free_dofs[*free_count] = dof;
            *free_count += 1;
        } else {
            constrained_dofs[*constrained_count] = dof;
            *constrained_count += 1;
        }

        dof += 1;
        if (nodes[i].fix_y == 0) {
            free_dofs[*free_count] = dof;
            *free_count += 1;
        } else {
            constrained_dofs[*constrained_count] = dof;
            *constrained_count += 1;
        }
    }

    return FEM_OK;
}
~~~

The two output arrays remain zero outside their populated prefixes because they are cleared before validation. The maximum populated entries are MAX_DOF, so no dynamic storage or bounds expansion is needed.

- [ ] Step 4: Add the two status messages

Extend fem_status_message in src/fem.c with these exact strings:

~~~c
    case FEM_INVALID_CONSTRAINT:
        return "constraint flags must be 0 or 1";
    case FEM_INVALID_LOAD:
        return "loads must be finite";
~~~

- [ ] Step 5: Run Stage 3 tests and fix only implementation failures

~~~powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
$gcc = "C:\msys64\ucrt64\bin\gcc.exe"
$out = Join-Path $env:TEMP "c_fe_stage3_tests.exe"
& $gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage3.c src\fem.c -Iinclude -o $out -lm
& $out
Remove-Item -LiteralPath $out -Force -ErrorAction SilentlyContinue
~~~

Expected result: exit code 0, no warnings, and Stage 3 tests passed.

- [ ] Step 6: Commit the implementation

~~~powershell
git add src\fem.c
git commit -m "feat: map stage 3 loads and constraints"
~~~

Expected result: one implementation commit containing only src/fem.c; the contract/test commit remains separately reviewable.

### Task 3: Run regression, verify scope, and record acceptance

**Files:**
- Modify: docs/superpowers/plans/2026-08-10-loads-constraints.md
- Test: tests/test_stage3.c, tests/test_stage2.c, tests/test_stage1.c, src/main.c

**Interfaces:**
- Consumes: Stage 3 implementation from Task 2 and existing Stage 1/Stage 2 public behavior.
- Produces: verified Stage 3 branch with documented test results and no generated artifacts in the worktree.

- [ ] Step 1: Run all C tests with the UCRT64 runtime path

Use temporary executable names outside the repository:

~~~powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
$gcc = "C:\msys64\ucrt64\bin\gcc.exe"
$temp = Join-Path $env:TEMP "c_fe_stage3_verify"
New-Item -ItemType Directory -Force -Path $temp | Out-Null
$stage3 = Join-Path $temp "stage3.exe"
$stage2 = Join-Path $temp "stage2.exe"
$stage1 = Join-Path $temp "stage1.exe"
$demo = Join-Path $temp "demo.exe"

& $gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage3.c src\fem.c -Iinclude -o $stage3 -lm
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $stage3
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage2.c src\fem.c -Iinclude -o $stage2 -lm
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $stage2
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage1.c src\fem.c -Iinclude -o $stage1 -lm
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $stage1
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $gcc -std=c11 -Wall -Wextra -pedantic src\main.c src\fem.c -Iinclude -o $demo -lm
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $demo
$result = $LASTEXITCODE
Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
exit $result
~~~

Expected output includes Stage 3 tests passed., Stage 2 tests passed., Stage 1 tests passed., and the unchanged Stage 1 demonstration output.

- [ ] Step 2: Verify source scope and whitespace

~~~powershell
git diff --check
rg --files src include tests
rg -n "solver\.c|matrix\.c|io\.c|postprocess\.c|gaussian|reduce|displacement" src include tests
git status --short
~~~

Expected result: no whitespace errors; only the planned Stage 3 source/header/test files and plan changes are present; no solver, matrix-reduction, file-input, or post-processing module is introduced.

- [ ] Step 3: Record actual verification results in this plan

Append an "执行结果" section containing the exact date, compiler command family, output lines, commit IDs, and any environment limitation. Use [x] only for commands that actually returned exit code 0. If Docker is checked and its engine is unavailable, record it as pending rather than claiming Docker acceptance.

- [ ] Step 4: Commit the verified plan record

~~~powershell
git add docs\superpowers\plans\2026-08-10-loads-constraints.md
git commit -m "docs: record stage 3 loads and constraints verification"
~~~

Expected result: clean worktree with three Stage 3 commits: contract/tests, implementation, and verification record.

## Self-Review Checklist

- Spec coverage: Node fields, two public functions, status codes, zero-on-failure behavior, DOF ordering, fixed capacity, NaN/Inf rejection, test model, regression checks, and scope exclusions are covered by Tasks 1–3.
- Type consistency: the Node fields and both function signatures are identical in Task 1, Task 2, and the design specification.
- Empty-item scan: every step names its files, command, expected result, or exact code; no unfinished placeholder instruction or unspecified edge-case instruction is required.
- Branch safety: all implementation work is performed in stage3-loads-constraints; the original stage2-global-stiffness worktree remains unchanged.
