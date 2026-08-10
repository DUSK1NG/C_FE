# Stage 5 最终审查修复报告

- 日期：2026-08-10
- 工作树：`C:/Users/jking1/Desktop/my-project/c_FE-stage5-dof-reduction`
- 最终审查输入 HEAD：`af1bd07`
- 完整分支基线：`5b5fc6b`
- 报告生成基线：`5beb9f8`
- 编译器：`C:/msys64/ucrt64/bin/gcc.exe`（GCC 16.1.0）

## Status

最终审查列出的 2 个 Important 和 3 个 Minor finding 均已处理。Stage 1～5、
demo、Dockerfile 两条等价 GCC 链接命令和 `git diff --check` 的实际退出码均为
0。Docker CLI/引擎在当前环境中不可用，因此 Docker 镜像验收未执行，本报告不
宣称 Docker 验收通过。

## Findings 处理

### Important 1 — Docker 链接回归

- 根因：Stage 5 的 `src/fem.c` 调用 `solve_linear_system()`，但 Dockerfile 的
  demo 和 Stage 1 测试命令仍只链接 `src/fem.c`。
- 修改：`Dockerfile:17-21` 的两条命令均加入 `src/solver.c`；镜像阶段、
  `RUN ./test_stage1` 和 runtime `CMD ["./fem"]` 均未改变。
- 文档：设计和计划明确允许这项受限的编译源列表更新，不再保留“不修改 Docker
  文件”的旧约束。
- 回归证据：旧等价命令均输出
  `undefined reference to 'solve_linear_system'` 并退出 1；修复后的两条等价命令
  均退出 0。
- 文件：`Dockerfile`、设计文档、实施计划。
- 提交：`8ab9157 fix: link solver in Docker builds`；文档边界由
  `5beb9f8 docs: finalize stage 5 review scope` 记录。

### Important 2 — 失败路径契约覆盖不足

- `tests/test_stage5.c` 的奇异 Kff 用例现在保存 `global_k`、`force`、
  `free_dofs`、`constrained_dofs` 的完整快照，并在断言
  `FEM_SINGULAR_MATRIX` 与完整位移清零后，使用 `memcmp()` 对四类输入做逐字节
  比较。
- 新增非有限缩减输入用例：自由 DOF 对应 `force` 设为 `NAN`，真实调用
  `solve_constrained_system()` 和 `src/solver.c`，断言 Stage 4 的
  `FEM_INVALID_ARGUMENT` 原样透传、完整位移清零、四类输入逐字节不变。
- 未使用 mock，未修改生产接口或 Stage 4 数值算法。
- 文件：`tests/test_stage5.c`。
- 提交：`f83ffcc test: cover stage 5 failure contracts`。

### Minor 1 — 边界测试缺口

- 新增负自由 DOF 和负约束 DOF 用例，均断言 `FEM_INVALID_ARGUMENT` 和输出
  全零。
- 新增 `constrained_count = MAX_DOF + 1` 用例，断言
  `FEM_CAPACITY_EXCEEDED` 和输出全零。
- 文件：`tests/test_stage5.c`。
- 提交：`f83ffcc test: cover stage 5 failure contracts`。

### Minor 2 — 验收文档陈旧

- 设计状态更新为“已实现、已验证”。
- 设计和计划同步记录 Dockerfile 的受限变更边界、失败路径契约和新增边界覆盖。
- 实施计划新增以完整分支基线为起点的权威最终执行结果，区分代码/集成、测试、
  设计/验证文档范围，并记录真实退出码、提交链、Docker 限制和既有警告。
- 文件：
  `docs/superpowers/specs/2026-08-10-dof-reduction-design.md`、
  `docs/superpowers/plans/2026-08-10-dof-reduction.md`。
- 提交：`5beb9f8 docs: finalize stage 5 review scope`。

### Minor 3 — 头文件注释

- `fem_status_message()` 的状态消息注释已移动到该声明正前方。
- `solve_constrained_system()` 现在有描述 DOF 校验、缩减求解和完整位移回填的
  接口注释。
- 文件：`include/fem.h`。
- 提交：`8ab9157 fix: link solver in Docker builds`。

## TDD / 回归测试证据

新增测试先通过临时故障注入验证其能捕获真实回归；每次 RED 均使用
`tests/test_stage5.c + src/fem.c + src/solver.c`，没有 mock。临时故障随后恢复，
`git diff -- src/fem.c` 为空。

统一编译命令：

```powershell
$env:Path = 'C:\msys64\ucrt64\bin;' + $env:Path
$gcc = 'C:\msys64\ucrt64\bin\gcc.exe'
& $gcc -std=c11 -Wall -Wextra -pedantic `
    tests\test_stage5.c src\fem.c src\solver.c `
    -Iinclude -o $out -lm
```

实际 RED/GREEN 输出：

| 检查 | 编译退出码 | 运行退出码 | 实际失败/成功输出 |
|---|---:|---:|---|
| Stage 4 非有限状态透传 RED | 0 | 1 | `FAIL: nonfinite reduced input, actual status = 8, expected status = 1` |
| 奇异失败输入快照 RED | 0 | 1 | `FAIL: singular force changed` |
| 非有限失败输入快照 RED | 0 | 1 | `FAIL: nonfinite force changed` |
| 负自由 DOF RED | 0 | 1 | `FAIL: negative free DOF, actual status = 5, expected status = 1` |
| 负约束 DOF RED | 0 | 1 | `FAIL: negative constrained DOF, actual status = 5, expected status = 1` |
| 约束计数超限 RED | 0 | 1 | `FAIL: constrained count over capacity, actual status = 1, expected status = 5` |
| 恢复真实实现后的 GREEN | 0 | 0 | `Stage 5 contract tests passed.` |

所有临时 `.exe` 均放在 `$env:TEMP` 并在各次运行后删除；每次记录均为
`temp_removed=True`。

## 完整验证命令

### Stage 1～5 与 demo

实际脚本对下列每个测试执行同一组编译/运行操作：

```powershell
$temp = Join-Path $env:TEMP 'c_fe_stage5_final_verify'
$cases = @(
    @{ Name = 'stage1'; Source = 'tests\test_stage1.c' },
    @{ Name = 'stage2'; Source = 'tests\test_stage2.c' },
    @{ Name = 'stage3'; Source = 'tests\test_stage3.c' },
    @{ Name = 'stage4'; Source = 'tests\test_stage4.c' },
    @{ Name = 'stage5'; Source = 'tests\test_stage5.c' }
)
foreach ($case in $cases) {
    $out = Join-Path $temp ($case.Name + '.exe')
    & $gcc -std=c11 -Wall -Wextra -pedantic `
        $case.Source src\fem.c src\solver.c -Iinclude -o $out -lm
    & $out
}
$demo = Join-Path $temp 'demo.exe'
& $gcc -std=c11 -Wall -Wextra -pedantic `
    src\main.c src\fem.c src\solver.c -Iinclude -o $demo -lm
& $demo
```

### Dockerfile 回归与修复后等价链接

```powershell
# 修复前源列表（预期链接失败）
& $gcc -std=c11 -Wall -Wextra -pedantic `
    src\main.c src\fem.c -Iinclude -o $oldDemo -lm
& $gcc -std=c11 -Wall -Wextra -pedantic `
    tests\test_stage1.c src\fem.c -Iinclude -o $oldStage1 -lm

# Dockerfile 当前源列表（预期链接成功）
& $gcc -std=c11 -Wall -Wextra -pedantic `
    src\main.c src\fem.c src\solver.c -Iinclude -o $dockerDemo -lm
& $gcc -std=c11 -Wall -Wextra -pedantic `
    tests\test_stage1.c src\fem.c src\solver.c `
    -Iinclude -o $dockerStage1 -lm
```

### Git、范围和环境

```powershell
git diff --check
git diff --name-status 5b5fc6b..HEAD
git log --reverse --format='%h %s' 5b5fc6b..HEAD
git status --short
Get-Command docker -ErrorAction SilentlyContinue
```

## 真实验证输出

| 目标 | 编译退出码 | 运行退出码 | 输出 |
|---|---:|---:|---|
| Stage 1 | 0 | 0 | `Stage 1 tests passed.` |
| Stage 2 | 0 | 0 | `Stage 2 tests passed.` |
| Stage 3 | 0 | 0 | `Stage 3 tests passed.` |
| Stage 4 | 0 | 0 | `Stage 4 tests passed.` |
| Stage 5 | 0 | 0 | `Stage 5 contract tests passed.` |
| demo | 0 | 0 | `Stage 1: single 2D truss element`；`Length = 943.398113205660 mm`；`c = 0.529998940003`；`s = 0.847998304005` |
| Dockerfile 等价 demo 链接 | 0 | 未运行 | 链接成功 |
| Dockerfile 等价 Stage 1 链接 | 0 | 未运行 | 链接成功 |

其他实际结果：

- 旧 Docker 等价 demo：`old_demo_link_exit=1`。
- 旧 Docker 等价 Stage 1：`old_stage1_link_exit=1`。
- `git diff --check`：无输出，退出码 0。
- `git diff --name-status 5b5fc6b..5beb9f8`：退出码 0。
- `git status --short`：无输出，退出码 0（生成本报告前）。
- 完整验证临时目录：`temp_removed=True`。

## 范围与提交链

`git diff --name-status 5b5fc6b..5beb9f8`：

```text
M  Dockerfile
A  docs/superpowers/plans/2026-08-10-dof-reduction.md
A  docs/superpowers/specs/2026-08-10-dof-reduction-design.md
M  include/fem.h
M  src/fem.c
A  tests/test_stage5.c
```

- 代码/集成：`Dockerfile`、`include/fem.h`、`src/fem.c`。
- 测试：`tests/test_stage5.c`。
- 设计/验证文档：设计文档、实施计划和本报告。
- 本次最终审查波次 `af1bd07..5beb9f8` 未修改 `src/fem.c` 或
  `src/solver.c`；`post_review_production_diff_empty=True`。
- 动态分配扫描 `rg -n "\b(malloc|calloc|realloc)\b" src include tests`
  无匹配，退出码 1（rg 的“无匹配”状态）。

完整提交链（`5b5fc6b..5beb9f8`）：

```text
67db1aa docs: define stage 5 dof reduction and displacement recovery
caca61a docs: plan stage 5 dof reduction and displacement recovery
9e7da5b test: define stage 5 dof reduction contract
5565e78 test: cover constrained dof validation gaps
6983464 feat: reduce constrained system and recover displacements
6d6a612 test: correct stage 5 fixture construction
af9da69 docs: record stage 5 verification
4a3ac1d docs: correct stage 5 verification record
af1bd07 docs: record verification correction commit
f83ffcc test: cover stage 5 failure contracts
8ab9157 fix: link solver in Docker builds
5beb9f8 docs: finalize stage 5 review scope
```

## 自审

当前会话未提供可用的代码审查子代理，因此无法分派独立 reviewer；已按审查模板
对 `af1bd07..5beb9f8` 做只读逐项自审：

- 公共 `solve_constrained_system()` 签名未变。
- `src/fem.c` 和 `src/solver.c` 在最终审查波次中无差异，Stage 4 数值算法未改。
- Dockerfile 只改变两条 GCC 命令的源文件列表，运行语义未改。
- 新测试调用真实 Stage 5 接口和真实 solver，断言值由明确夹具给出，不依赖 mock。
- 奇异和非有限失败路径均验证状态、完整输出清零及四类输入逐字节不变。
- `git diff --check 5b5fc6b..5beb9f8` 退出码 0。

自审未发现新增 Critical、Important 或 Minor finding。

## Concerns / 限制

- Docker CLI 不可用：`docker_cli_available=false`。Docker 引擎/镜像验收未执行，
  仅完成 Dockerfile 两条命令的等价本地 GCC 链接验证。
- 仍有 14 条既有 `-Wmissing-field-initializers` 警告，均为 `Node.fx` 未显式
  初始化：`tests/test_stage1.c` 6 条、`tests/test_stage2.c` 6 条、
  `src/main.c` 2 条。Stage 3、Stage 4、Stage 5 编译无警告。
