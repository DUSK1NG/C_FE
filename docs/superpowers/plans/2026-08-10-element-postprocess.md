# Stage 6：单元后处理实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在固定容量二维桁架模型中新增单元后处理 API，使用完整节点位移计算杆件伸长、应变、应力、轴力和拉压状态。

**Architecture:** 新增独立的 `postprocess` 模块，不把后处理逻辑塞入现有 `fem.c` 或 `solver.c`。模块只读 `Element` 和 Stage 5 产生的完整位移向量，使用全局 DOF 映射完成方向投影；失败时清零结果且不修改输入。测试先定义公共契约，再实现最小生产代码，最后接入 Dockerfile 的 Stage 6 测试步骤并记录整条阶段链路的验证结果。

**Tech Stack:** C11；`C:\\msys64\\ucrt64\\bin\\gcc.exe`；`-Wall -Wextra -pedantic`；标准 C 库 `math.h`、`string.h`、`float.h`；固定容量栈上数组；无第三方库、无动态内存。

## Global Constraints

- 公共接口必须保持为 `FemStatus calculate_element_result(const Element *element, const double displacement[MAX_DOF], ElementResult *result);`。
- 计算必须使用 `elongation = c * (Ujx - Uix) + s * (Ujy - Uiy)`、`strain = elongation / length`、`stress = E * strain`、`axial_force = stress * A`。
- `axial_force > 0` 返回 `ELEMENT_TENSION`，小于 0 返回 `ELEMENT_COMPRESSION`，等于 0 返回 `ELEMENT_NEUTRAL`。
- 函数开始清零非空 `result`；任何失败路径保持完整结果为零值和 `ELEMENT_NEUTRAL`，且不修改 `element` 或 `displacement`。
- 空指针或非有限几何/位移/计算结果返回 `FEM_INVALID_ARGUMENT`；索引越界或两端索引相同返回 `FEM_INVALID_INDEX`；长度过小返回 `FEM_ZERO_LENGTH`；非有限或非正 `E/A` 返回 `FEM_INVALID_PROPERTY`。
- 不修改 `src/main.c`，不修改 Stage 1–5 公共 API，不加入 Stage 7 支座反力、文件输入、结果导出或动态内存。
- 每次验证使用 `-std=c11 -Wall -Wextra -pedantic`；Docker 引擎不可用时只能记录为未执行真实镜像验收，不得宣称 Docker 构建通过。

---

## 文件与职责

| 文件 | 责任 |
|---|---|
| `include/postprocess.h` | 声明 `ElementState`、`ElementResult` 和 Stage 6 公共函数 |
| `src/postprocess.c` | 校验输入，投影相对位移并计算单元结果 |
| `tests/test_stage6.c` | 定义正常结果、符号状态、三角桁架回归和失败语义 |
| `Dockerfile` | 增加 Stage 6 测试的编译与运行步骤，不改变 Demo 运行语义 |
| `docs/superpowers/specs/2026-08-10-element-postprocess-design.md` | 已批准的设计规范 |
| `docs/superpowers/plans/2026-08-10-element-postprocess.md` | 本实施计划及最终实际验证记录 |

## Task 1: 定义 Stage 6 公共契约和失败测试

**Files:**
- Create: `include/postprocess.h`
- Create: `tests/test_stage6.c`

**Interfaces:**
- Consumes: `include/fem.h` 的 `FemStatus`、`MAX_DOF`，以及 `include/model.h` 的 `Element`。
- Produces: `ElementState`、`ElementResult` 和后续实现必须提供的 `calculate_element_result()` 精确签名。

- [ ] **Step 1: 创建公共头文件契约**

在 `include/postprocess.h` 中加入 include guard，并包含 `fem.h`。声明以下类型和函数，不添加节点位移字段：

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

- [ ] **Step 2: 编写测试辅助函数和水平杆正常用例**

在 `tests/test_stage6.c` 中使用 `expect_status()`、`expect_close()`、`expect_result_zero()` 和 `expect_bytes_unchanged()` 辅助断言。测试使用以下固定元素：

```c
Element element = {1, 0, 1, 210000.0, 100.0,
                   1000.0, 1.0, 0.0};
double displacement[MAX_DOF] = {0.0};
displacement[2] = 1.0;
```

调用 `calculate_element_result()` 后断言 `FEM_OK`，并验证 `elongation = 1.0`、`strain = 0.001`、`stress = 210.0`、`axial_force = 21000.0`、`state = ELEMENT_TENSION`。

- [ ] **Step 3: 添加压缩、零力和斜杆投影测试**

加入三个独立测试：

1. 将水平杆 `displacement[2]` 设为 `-1.0`，验证应变、应力和轴力为负且状态为 `ELEMENT_COMPRESSION`。
2. 保持所有位移为零，验证五个结果字段为零且状态为 `ELEMENT_NEUTRAL`。
3. 使用 `length = 1000.0`、`c = sqrt(0.5)`、`s = sqrt(0.5)`、`E = 1000.0`、`A = 2.0`，仅设置 `displacement[3] = 2.0`；验证 `elongation = sqrt(2.0)`、`strain = sqrt(2.0) / 1000.0`、`stress = sqrt(2.0)`、`axial_force = 2.0 * sqrt(2.0)`，状态为 `ELEMENT_TENSION`。

- [ ] **Step 4: 添加三角桁架参考结果测试**

用设计文档中的 `E = 210000 MPa`、`A = 100 mm²`、方向余弦和位移构造三个 `Element`：

```c
Element elements[3] = {
    {1, 0, 1, 210000.0, 100.0, 1000.0, 1.0, 0.0},
    {2, 0, 2, 210000.0, 100.0, 943.398113205660,
     0.529998940003, 0.847998304005},
    {3, 1, 2, 210000.0, 100.0, 943.398113205660,
     -0.529998940003, 0.847998304005}
};
double displacement[MAX_DOF] = {0.0};
displacement[2] = 0.1488095;
displacement[4] = 0.0744048;
displacement[5] = -0.3588632;
```

分别断言轴力约为 `3125.000 N`、`-5896.238 N`、`-5896.238 N`，并断言状态依次为拉、压、压；使用 `1.0e-2` 的数值容差匹配设计文档给出的四舍五入位移。

- [ ] **Step 5: 添加非法输入和结果清零测试**

对每个失败场景先以非零值填充 `ElementResult`，复制 `Element` 和完整位移数组，调用函数后检查状态、结果全零、元素和位移逐字节不变。覆盖：

- `element == NULL`、`displacement == NULL`、`result == NULL`；
- `node1 < 0`、`node2 >= MAX_NODES`、`node1 == node2`；
- `length = 0.0`；
- `E = 0.0`、`A < 0.0`、`E = NAN`；
- `length = NAN`、`c = INFINITY`、四个位移分量之一为 `NAN`；
- 使用 `E = 2.0`、`A = 1.0`、`length = 1.0`、`c = 1.0`、`s = 0.0` 及 `displacement[2] = DBL_MAX`，验证计算溢出返回 `FEM_INVALID_ARGUMENT` 且结果清零。

- [ ] **Step 6: 运行 RED 编译检查**

运行：

```powershell
$env:Path = 'C:\msys64\ucrt64\bin;' + $env:Path
$gcc = 'C:\msys64\ucrt64\bin\gcc.exe'
& $gcc -std=c11 -Wall -Wextra -pedantic `
    tests\test_stage6.c -Iinclude -o `
    (Join-Path $env:TEMP 'c_fe_stage6_contract_red.exe') -lm
```

预期：编译成功但链接失败，报告 `calculate_element_result` 未定义；此时不能把测试标记为通过。删除临时可执行文件。

- [ ] **Step 7: 提交测试契约**

```powershell
git add include/postprocess.h tests/test_stage6.c
git commit -m "test: define stage 6 element postprocess contract"
```

## Task 2: 实现单元后处理计算

**Files:**
- Create: `src/postprocess.c`

**Interfaces:**
- Consumes: Task 1 的 `ElementState`、`ElementResult` 和函数声明；`Element` 的 `node1/node2/E/A/length/c/s`；Stage 5 完整位移向量。
- Produces: 可通过全部 Stage 6 契约测试的 `calculate_element_result()` 实现，不修改任何既有模块。

- [ ] **Step 1: 添加结果清零辅助函数**

在 `src/postprocess.c` 中包含 `postprocess.h`、`math.h` 和 `config.h`，实现只写 `ElementResult` 的静态辅助函数：

```c
static void clear_element_result(ElementResult *result)
{
    if (result != NULL) {
        result->elongation = 0.0;
        result->strain = 0.0;
        result->stress = 0.0;
        result->axial_force = 0.0;
        result->state = ELEMENT_NEUTRAL;
    }
}
```

- [ ] **Step 2: 按设计顺序实现参数校验**

在 `calculate_element_result()` 开头调用清零辅助函数。随后按以下顺序返回状态：空指针；节点索引范围或两端相同；长度非有限或小于 `GEOMETRY_TOL`；材料参数非有限或非正；方向余弦非有限；四个位移分量非有限。所有检查只读输入。

- [ ] **Step 3: 实现投影和结果计算**

使用局部 `int i_dof = 2 * element->node1` 和 `int j_dof = 2 * element->node2`，计算：

```c
result->elongation = element->c *
    (displacement[j_dof] - displacement[i_dof]) +
    element->s * (displacement[j_dof + 1] - displacement[i_dof + 1]);
result->strain = result->elongation / element->length;
result->stress = element->E * result->strain;
result->axial_force = result->stress * element->A;
```

对四个数值结果逐项执行 `isfinite()`；若任何一项非有限，重新清零结果并返回 `FEM_INVALID_ARGUMENT`。否则按轴力的正、负、零设置 `ElementState`，返回 `FEM_OK`。

- [ ] **Step 4: 运行 Stage 6 GREEN 测试**

运行：

```powershell
$env:Path = 'C:\msys64\ucrt64\bin;' + $env:Path
$gcc = 'C:\msys64\ucrt64\bin\gcc.exe'
$out = Join-Path $env:TEMP 'c_fe_stage6.exe'
& $gcc -std=c11 -Wall -Wextra -pedantic `
    tests\test_stage6.c src\postprocess.c `
    -Iinclude -o $out -lm
& $out
```

预期输出：`Stage 6 element postprocess tests passed.`，编译和运行退出码均为 0；随后删除 `$out`。

- [ ] **Step 5: 提交生产实现**

```powershell
git add src/postprocess.c
git commit -m "feat: calculate stage 6 element results"
```

## Task 3: 接入 Docker、回归验证并记录结果

**Files:**
- Modify: `Dockerfile`
- Modify: `docs/superpowers/plans/2026-08-10-element-postprocess.md`

**Interfaces:**
- Consumes: Task 2 的 `src/postprocess.c` 和 `include/postprocess.h`。
- Produces: Dockerfile 中可独立构建并运行的 Stage 6 测试，以及有真实退出码的最终验收记录。

- [ ] **Step 1: 增加 Dockerfile Stage 6 测试命令**

在现有 Stage 1 测试命令之后增加：

```dockerfile
RUN gcc -std=c11 -Wall -Wextra -pedantic \
        tests/test_stage6.c src/postprocess.c -Iinclude -o test_stage6 -lm

RUN ./test_stage6
```

不修改 `RUN gcc ... src/main.c ... -o fem` 和 `CMD ["./fem"]` 的运行语义；Stage 6 测试独立链接 `postprocess.c`，不需要把后处理模块提前接入 Demo。

- [ ] **Step 2: 运行 Stage 1–6 和 Demo 回归**

在 `$env:TEMP` 下创建限定临时目录，逐项使用以下源文件组合编译和运行，并记录每个编译/运行退出码：

```powershell
$env:Path = 'C:\msys64\ucrt64\bin;' + $env:Path
$gcc = 'C:\msys64\ucrt64\bin\gcc.exe'
$temp = Join-Path $env:TEMP 'c_fe_stage6_final_verify'
if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force }
New-Item -ItemType Directory -Path $temp | Out-Null
$tests = @(
    @{ Name = 'stage1'; Source = 'tests\test_stage1.c'; Sources = @('src\fem.c', 'src\solver.c') },
    @{ Name = 'stage2'; Source = 'tests\test_stage2.c'; Sources = @('src\fem.c', 'src\solver.c') },
    @{ Name = 'stage3'; Source = 'tests\test_stage3.c'; Sources = @('src\fem.c', 'src\solver.c') },
    @{ Name = 'stage4'; Source = 'tests\test_stage4.c'; Sources = @('src\fem.c', 'src\solver.c') },
    @{ Name = 'stage5'; Source = 'tests\test_stage5.c'; Sources = @('src\fem.c', 'src\solver.c') },
    @{ Name = 'stage6'; Source = 'tests\test_stage6.c'; Sources = @('src\postprocess.c') }
)
foreach ($test in $tests) {
    $out = Join-Path $temp ($test.Name + '.exe')
    & $gcc -std=c11 -Wall -Wextra -pedantic `
        $test.Source $test.Sources -Iinclude -o $out -lm
    $compileExit = $LASTEXITCODE
    & $out
    $runExit = $LASTEXITCODE
}
$demo = Join-Path $temp 'demo.exe'
& $gcc -std=c11 -Wall -Wextra -pedantic `
    src\main.c src\fem.c src\solver.c -Iinclude -o $demo -lm
$demoCompileExit = $LASTEXITCODE
& $demo
$demoRunExit = $LASTEXITCODE
Remove-Item -LiteralPath $temp -Recurse -Force
```

预期：Stage 1–6 和 Demo 的编译、运行退出码全部为 0；输出包含各阶段通过行及现有 Demo 数值。无论成功或失败都删除临时目录。

- [ ] **Step 3: 运行 Dockerfile 等价本地链接检查**

使用同一编译器执行 Dockerfile 两类命令的等价检查：

```powershell
$dockerStage6 = Join-Path $env:TEMP 'c_fe_stage6_docker_equivalent.exe'
& $gcc -std=c11 -Wall -Wextra -pedantic `
    tests\test_stage6.c src\postprocess.c -Iinclude -o $dockerStage6 -lm
& $dockerStage6
Remove-Item -LiteralPath $dockerStage6 -Force
```

记录 `docker version` 的实际结果。如果 Docker Desktop 引擎不可用，只记录 CLI/引擎限制，并保留等价 GCC 命令的真实退出码。

- [ ] **Step 4: 运行范围和静态检查**

运行：

```powershell
git diff --check e0eafaf..HEAD
git diff --name-status e0eafaf..HEAD
rg -n "\b(malloc|calloc|realloc|free)\b" src include tests
git status --short
```

预期：差异无空白错误；文件范围仅为计划内的 Stage 6 头文件、实现、测试、Dockerfile 和计划文档；动态内存搜索无匹配（`rg` 退出码 1 是预期）；工作树在提交后干净。

- [ ] **Step 5: 在计划末尾记录真实验收结果**

追加“执行结果”章节，填写实际日期、编译器版本、Stage 1–6 和 Demo 的编译/运行输出与退出码、Docker 等价命令、`docker version` 限制、既有警告、提交 ID 和最终范围。只有真实返回 0 的命令标记 `[x]`，Docker 引擎不可用时不标记真实镜像验收。

- [ ] **Step 6: 提交集成与验收记录**

```powershell
git add Dockerfile docs/superpowers/plans/2026-08-10-element-postprocess.md
git commit -m "docs: record stage 6 verification"
```

## Execution results (2026-08-10)

- [x] Compiler: `C:\\msys64\\ucrt64\\bin\\gcc.exe`; `gcc.exe (Rev5, Built by MSYS2 project) 16.1.0`; version-command exit code `0`.
- [x] Stage 1: compile `0`, run `0`; output: `Stage 1 tests passed.`
- [x] Stage 2: compile `0`, run `0`; output: `Stage 2 tests passed.`
- [x] Stage 3: compile `0`, run `0`; output: `Stage 3 tests passed.`
- [x] Stage 4: compile `0`, run `0`; output: `Stage 4 tests passed.`
- [x] Stage 5: compile `0`, run `0`; output: `Stage 5 contract tests passed.`
- [x] Stage 6: compile `0`, run `0`; output: `Stage 6 element postprocess contract tests passed.`
- [x] Demo: compile `0`, run `0`; its source set remains `src/main.c src/fem.c src/solver.c`. Actual output:

  ```text
  Stage 1: single 2D truss element
  Length = 943.398113205660 mm
  c = 0.529998940003
  s = 0.847998304005
  Element stiffness matrix [N/mm]:
     6252.796483  10004.474373  -6252.796483 -10004.474373
    10004.474373  16007.158997 -10004.474373 -16007.158997
    -6252.796483 -10004.474373   6252.796483  10004.474373
   -10004.474373 -16007.158997  10004.474373  16007.158997
  ```
- [x] Dockerfile-equivalent local check: `tests/test_stage6.c src/postprocess.c -Iinclude -o c_fe_stage6_docker_equivalent.exe -lm` compiled with exit `0` and ran with exit `0`; output: `Stage 6 element postprocess contract tests passed.`
- [ ] Real Docker image build was not run. The `docker` CLI is unavailable: direct `docker version` invocation produced PowerShell `CommandNotFoundException`, so there is no Docker process exit code to record; `where.exe docker` actually returned `1`. No image or engine validation is claimed.
- Existing warnings: Stage 1, Stage 2, and Demo compilation retain `-Wmissing-field-initializers` warnings for existing `Node` initializers that omit `fx`; every affected compile command exited `0`. This task did not alter those source files.
- Stage 6 commit chain: plan `ab060b8` -> Task 1 `9f3a811` (`test: define stage 6 element postprocess contract`) -> Task 2 `84d8dda` (`feat: calculate stage 6 element results`) -> Task 3 `4b7675c` (`docs: record stage 6 verification`).
- Final Task 3 scope: Dockerfile adds a standalone Stage 6 compile/run pair after Stage 1 without changing the `fem` source list or `CMD ["./fem"]`; this plan appends the real verification record.
- Non-blocking deferred Task 1 Minor: improve per-field failure labels in `expect_result_zero()`; it remains outside the permitted Task 3 source/test scope.

## 完成条件

- Stage 6 设计规范与本计划均已提交。
- Task 1–3 各自完成提交，并通过对应测试和任务审查。
- Stage 1–6 及 Demo 的实际编译、运行退出码全部为 0。
- 三角桁架三个单元的轴力、应力和拉压状态与参考结果一致。
- Dockerfile 的 Stage 6 等价 GCC 检查通过；真实 Docker 构建的可用性按环境实际记录。
- `git diff --check` 通过，工作树干净，Stage5 工作树和 PR #4 不受修改。
