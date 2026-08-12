# Stage 7 支座反力实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 基于原始整体刚度方程计算受约束自由度的支座反力，并验证二维桁架的 X/Y 全局平衡。

**Architecture:** 新增独立的 `reactions` 模块，不改变 Stage 5 求解器。模块计算 `K_original * U_complete - F_original`，只输出受约束自由度；另一个接口汇总荷载与反力并返回方向残差。测试独立编译，同时回归 Stage1–6。

**Tech Stack:** C11、现有 `FemStatus` API、固定大小 `MAX_DOF` 数组、GCC、现有 Dockerfile 测试流程。

## Global Constraints

- 使用原始、未消元的整体刚度矩阵、荷载向量和完整位移计算 `R = K_original * U_complete - F_original`。
- 只有受约束自由度的反力写入输出；自由自由度的反力必须为 `0`。
- `constrained_count` 必须在 `0..MAX_DOF` 范围内；约束自由度必须在 `[0, MAX_DOF)` 且不得重复。
- 矩阵、向量和中间结果必须为有限浮点数。
- 平衡容差必须为有限且不小于零；残差超过容差返回 `FEM_EQUILIBRIUM_ERROR`。
- 失败路径清零输出；不修改输入数组；不引入动态内存或全局可变状态。
- 不修改 `solve_constrained_system`，不扩展 `Element`，不实现 Stage8–10 功能。
- Dockerfile 保留现有 demo 和 Stage1–6 测试，只追加 Stage7 测试。

---

## 文件结构

- Create: `include/reactions.h` — 两个公开接口的声明。
- Create: `src/reactions.c` — 反力计算、平衡检查和输入校验。
- Create: `tests/test_stage7.c` — 数值结果与错误路径测试。
- Modify: `include/fem.h` — 在 `FemStatus` 末尾追加 `FEM_EQUILIBRIUM_ERROR`。
- Modify: `src/fem.c` — 为新状态追加 `fem_status_message` 文本。
- Modify: `Dockerfile` — 编译并运行 `test_stage7`。
- Reference: `docs/superpowers/specs/2026-08-12-support-reactions-design.md` — 已确认并提交的设计基线。

## 接口契约

```c
FemStatus calculate_support_reactions(
    const double global_k[MAX_DOF][MAX_DOF],
    const double force[MAX_DOF],
    const double displacement[MAX_DOF],
    const int constrained_dofs[MAX_DOF],
    int constrained_count,
    double reactions[MAX_DOF]);

FemStatus check_global_equilibrium(
    const double force[MAX_DOF],
    const double reactions[MAX_DOF],
    double tolerance,
    double *residual_fx,
    double *residual_fy);
```

### Task 1: 建立测试契约和状态接口

**Files:**
- Create: `tests/test_stage7.c`
- Modify: `include/fem.h`
- Modify: `src/fem.c`

**Interfaces:**
- Consumes: 既有节点、单元、整体刚度、荷载、DOF 识别和约束求解 API。
- Produces: `FEM_EQUILIBRIUM_ERROR` 和 Stage7 测试契约；Task 2 必须使测试通过。

- [ ] **Step 1: 编写参考三角桁架夹具**

在 `tests/test_stage7.c` 中定义 `ASSERT_TRUE`、`ASSERT_NEAR` 和 `build_reference_triangle`。使用现有 API 建立以下模型，不手写 Stage7 专用刚度矩阵：

```c
Node nodes[3] = {
    {1, 0.0, 0.0, 0.0, 0.0, 1, 1},
    {2, 1000.0, 0.0, 0.0, 0.0, 0, 1},
    {3, 500.0, 800.0, 0.0, -10000.0, 0, 0}
};
Element elements[3] = {
    {1, 1, 2, 210000.0, 100.0, 0.0, 0.0, 0.0},
    {2, 1, 3, 210000.0, 100.0, 0.0, 0.0, 0.0},
    {3, 2, 3, 210000.0, 100.0, 0.0, 0.0, 0.0}
};
```

通过既有 Stage1–5 API 得到 `global_k`、`force`、`displacement` 和约束自由度 `{0, 1, 3}`。

- [ ] **Step 2: 添加核心断言**

必须验证：

```c
double reactions[MAX_DOF];
int constrained_dofs[MAX_DOF] = {0, 1, 3};
FemStatus status = calculate_support_reactions(
    global_k, force, displacement, constrained_dofs, 3, reactions);
ASSERT_TRUE(status == FEM_OK);
ASSERT_NEAR(reactions[0], 0.0, 1.0e-8);
ASSERT_NEAR(reactions[1], 5000.0, 1.0e-6);
ASSERT_NEAR(reactions[3], 5000.0, 1.0e-6);
ASSERT_NEAR(reactions[2], 0.0, 1.0e-12);

double residual_fx = 0.0;
double residual_fy = 0.0;
status = check_global_equilibrium(
    force, reactions, 1.0e-8, &residual_fx, &residual_fy);
ASSERT_TRUE(status == FEM_OK);
ASSERT_NEAR(residual_fx, 0.0, 1.0e-8);
ASSERT_NEAR(residual_fy, 0.0, 1.0e-8);
```

同时覆盖空约束列表、越界和重复自由度、空指针、`NAN` 输入、负容差，以及位移扰动后返回 `FEM_EQUILIBRIUM_ERROR`。每个失败测试先写入非零哨兵值，再验证输出被清零；合法但不平衡时验证实际残差保留。

- [ ] **Step 3: 运行失败测试**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage7.c src/fem.c src/solver.c -Iinclude -o test_stage7 -lm
```

Expected: 编译失败，因为 Stage7 头文件、函数和状态尚未实现。

- [ ] **Step 4: 追加状态**

在 `include/fem.h` 的 `FemStatus` 末尾追加 `FEM_EQUILIBRIUM_ERROR`；在 `src/fem.c` 的 `fem_status_message` 追加：

```c
case FEM_EQUILIBRIUM_ERROR:
    return "global equilibrium check failed";
```

不重排已有枚举成员或修改既有状态文本。

- [ ] **Step 5: 提交契约**

```powershell
git add tests/test_stage7.c include/fem.h src/fem.c
git commit -m "test: define stage 7 support reaction contract"
```

### Task 2: 实现反力模块

**Files:**
- Create: `include/reactions.h`
- Create: `src/reactions.c`
- Test: `tests/test_stage7.c`

**Interfaces:**
- Consumes: Task 1 的状态、`MAX_DOF` 和测试夹具。
- Produces: `calculate_support_reactions` 与 `check_global_equilibrium`，供后续 Stage9 使用。

- [ ] **Step 1: 声明接口**

`include/reactions.h` 包含 `fem.h`，按接口契约逐字声明两个函数，并使用项目现有 include guard 风格。

- [ ] **Step 2: 实现校验和清零**

`src/reactions.c` 只使用固定大小局部数组，不调用 `malloc`、`calloc`、`realloc` 或 `free`。入口先清零输出；校验空指针、约束数量、范围、重复值和 `isfinite`。非法输入返回 `FEM_INVALID_ARGUMENT`，输出保持全零。

- [ ] **Step 3: 实现反力计算**

先在局部 `residual[MAX_DOF]` 计算完整 `K*U-F`，确认每项有限后再按约束列表写入：

```c
for (int row = 0; row < MAX_DOF; ++row) {
    double value = -force[row];
    for (int col = 0; col < MAX_DOF; ++col) {
        value += global_k[row][col] * displacement[col];
    }
    residual[row] = value;
}
for (int i = 0; i < constrained_count; ++i) {
    reactions[constrained_dofs[i]] = residual[constrained_dofs[i]];
}
```

成功返回 `FEM_OK`，任何输入或中间结果非有限都返回参数错误并清零。

- [ ] **Step 4: 实现平衡检查**

按偶数自由度为 X、奇数自由度为 Y 累加：

```c
double fx = 0.0;
double fy = 0.0;
for (int dof = 0; dof < MAX_DOF; dof += 2) {
    fx += force[dof] + reactions[dof];
    fy += force[dof + 1] + reactions[dof + 1];
}
*residual_fx = fx;
*residual_fy = fy;
if (fabs(fx) > tolerance || fabs(fy) > tolerance) {
    return FEM_EQUILIBRIUM_ERROR;
}
return FEM_OK;
```

非法输入时两个残差清零；合法但超容差时保留实际残差。

- [ ] **Step 5: 运行 Stage7 测试**

```powershell
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage7.c src/fem.c src/solver.c src/reactions.c -Iinclude -o test_stage7 -lm
.\\test_stage7.exe
```

Expected: 全部通过且无编译警告。

- [ ] **Step 6: 提交实现**

```powershell
git add include/reactions.h src/reactions.c tests/test_stage7.c
git commit -m "feat: calculate stage 7 support reactions"
```

### Task 3: 接入构建并回归验证

**Files:**
- Modify: `Dockerfile`

**Interfaces:**
- Consumes: Task 2 的反力模块和测试。
- Produces: Docker 中可独立运行的 `test_stage7`，以及 Stage1–7 全量验证结果。

- [ ] **Step 1: 追加 Docker 测试**

在现有 Stage6 测试之后追加：

```dockerfile
RUN gcc -std=c11 -Wall -Wextra -pedantic \
        tests/test_stage7.c src/fem.c src/solver.c src/reactions.c \
        -Iinclude -o test_stage7 -lm

RUN ./test_stage7
```

保持 demo 编译和 `CMD ["./fem"]` 不变。

- [ ] **Step 2: 编译运行 Stage1–7**

```powershell
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage1.c src/fem.c src/solver.c -Iinclude -o test_stage1 -lm
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage2.c src/fem.c src/solver.c -Iinclude -o test_stage2 -lm
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage3.c src/fem.c src/solver.c -Iinclude -o test_stage3 -lm
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage4.c src/fem.c src/solver.c -Iinclude -o test_stage4 -lm
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage5.c src/fem.c src/solver.c -Iinclude -o test_stage5 -lm
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage6.c src/fem.c src/solver.c src/postprocess.c -Iinclude -o test_stage6 -lm
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage7.c src/fem.c src/solver.c src/reactions.c -Iinclude -o test_stage7 -lm
.\\test_stage1.exe; .\\test_stage2.exe; .\\test_stage3.exe; .\\test_stage4.exe; .\\test_stage5.exe; .\\test_stage6.exe; .\\test_stage7.exe
```

Expected: 7 个测试程序退出码均为 `0`，编译无 `-Wall -Wextra -pedantic` 警告。

- [ ] **Step 3: 回归 Stage1 demo**

```powershell
gcc -std=c11 -Wall -Wextra -pedantic src/main.c src/fem.c src/solver.c -Iinclude -o fem -lm
.\\fem.exe
```

Expected: 继续输出 Stage1 单杆示例，长度约 `943.398113205660 mm`，`c=0.529998940003`、`s=0.847998304005`。

- [ ] **Step 4: 执行静态检查**

Docker 引擎可用时运行既有镜像构建与测试；不可用时重复 Dockerfile 中的 GCC 编译/运行命令。另行运行：

```powershell
git diff --check 379ef28..HEAD
rg -n "\\b(malloc|calloc|realloc|free)\\s*\\(" src include tests
git status --short --branch
```

Expected: diff 无空白错误；Stage7 无动态内存调用；工作区干净且仍在 `stage7-support-reactions` 分支。

- [ ] **Step 5: 提交构建变更**

```powershell
git add Dockerfile
git commit -m "test: verify stage 7 in project build"
```

提交前再次查看 Stage1–7 新鲜测试输出。暂不推送、不创建合并提交；Stage8–10 将基于本分支最新提交继续开发。

## 完成标准

- 参考三角桁架支座反力正确。
- 平衡检查正确区分容差内和超容差结果。
- 非法输入与失败清零语义有测试覆盖。
- Stage1–7 本地编译运行通过，Stage1 demo 无回归。
- Dockerfile 包含 Stage7 测试入口。
- 分支独立、工作区干净，不合并 Stage6 或其他分支。
