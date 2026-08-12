# 编译警告清理与可维护性优化实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** 让 Stage1–Stage10 契约测试和 Stage1 演示程序在 C11、-Wall -Wextra -pedantic 下无编译警告，同时保持 FEM 公共接口、数值算法和输出行为不变。

**Architecture:** 只修改测试与演示代码的初始化表达，以及新增测试专用的只读矩阵适配头文件。生产 FEM 头文件、矩阵 API、求解流程和固定容量模型保持不变；所有修改通过严格编译和现有契约测试验证。

**Tech Stack:** C11；GCC；标准 C 库；Windows MSYS2 UCRT64；Docker GCC 13 builder。

## Global Constraints

- 所有编译使用 -std=c11 -Wall -Wextra -pedantic、-Iinclude 和 -lm。
- 不修改生产公共函数签名、数值常量、容差、矩阵布局或输出格式。
- 不引入动态内存、第三方库或新的运行时依赖。
- 不通过关闭编译器警告选项来隐藏问题。
- 实施在 optimization-warning-cleanup 隔离工作区完成，stage9-results-output 分支保持不变。

---

## 文件边界

- Create: tests/test_helpers.h — 测试代码共享的 C11 只读二维矩阵适配函数。
- Modify: tests/test_stage7.c — 使用共享矩阵适配函数调用只读矩阵 API。
- Modify: tests/test_stage10.c — 使用共享矩阵适配函数调用只读矩阵 API。
- Modify: tests/test_stage1.c — 用指定字段完整初始化 Node。
- Modify: tests/test_stage2.c — 用指定字段完整初始化 Node。
- Modify: src/main.c — 用指定字段完整初始化演示程序的 Node。
- Create: docs/superpowers/verification/2026-08-12-warning-free-maintainability.md — 记录严格编译、契约测试和 Docker 验证结果。

## Task 1: 建立共享矩阵限定符适配并清理 Stage7 警告

**Files:**
- Create: tests/test_helpers.h
- Modify: tests/test_stage7.c
- Test: tests/test_stage7.c

**Interfaces:**
- Produces: test_readonly_matrix(double (*matrix)[MAX_DOF])，返回类型为 const double (*)[MAX_DOF]。
- Consumes: include/config.h 中的 MAX_DOF，以及 include/reactions.h、include/fem.h 的现有只读矩阵参数。

- [ ] **Step 1: 创建测试辅助头文件**

在 tests/test_helpers.h 中写入以下完整内容：

~~~c
#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include "config.h"

/* C11 不允许可写二维数组指针隐式增加元素 const 限定。 */
static inline const double (*test_readonly_matrix(
    double (*matrix)[MAX_DOF]))[MAX_DOF]
{
    return (const double (*)[MAX_DOF])matrix;
}

#endif
~~~

- [ ] **Step 2: 更新 Stage7 的矩阵调用点**

在 tests/test_stage7.c 增加：

~~~c
#include "test_helpers.h"
~~~

将 solve_constrained_system 的矩阵参数从：

~~~c
(const double (*)[MAX_DOF])reference->global_k
~~~

改为：

~~~c
test_readonly_matrix(reference->global_k)
~~~

将所有传入 calculate_support_reactions 的 reference.global_k 改为 test_readonly_matrix(reference.global_k)。空指针错误分支继续直接传入 NULL。

- [ ] **Step 3: 严格编译并运行 Stage7**

运行：

~~~powershell
$env:Path = 'C:\msys64\ucrt64\bin;' + $env:Path
$gcc = 'C:\msys64\ucrt64\bin\gcc.exe'
$out = Join-Path ([System.IO.Path]::GetTempPath()) 'cfe_stage7_warning_cleanup.exe'
& $gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage7.c src\fem.c src\solver.c src\reactions.c -Iinclude -o $out -lm
if ($LASTEXITCODE -ne 0) { throw 'Stage7 compile failed' }
& $out
if ($LASTEXITCODE -ne 0) { throw 'Stage7 test failed' }
~~~

预期：编译无 warning 输出，运行输出为 Stage 7 contract tests passed.

- [ ] **Step 4: 检查差异并提交**

运行 git diff --check，确认只有 tests/test_helpers.h 和 tests/test_stage7.c 相关变更，然后提交：

~~~powershell
git add tests/test_helpers.h tests/test_stage7.c
git commit -m "test: centralize C11 matrix qualifier adapter"
~~~

## Task 2: 清理 Node 聚合初始化警告

**Files:**
- Modify: tests/test_stage1.c
- Modify: tests/test_stage2.c
- Modify: src/main.c
- Test: Stage1、Stage2 和演示程序的严格编译与运行

**Interfaces:**
- Consumes: Node 的字段 id、x、y、fx、fy、fix_x、fix_y。
- Produces: 字段含义显式且不依赖结构体声明顺序的测试与演示初始化。

- [ ] **Step 1: 改写 Stage1 的六个 Node 初始化**

将 tests/test_stage1.c 中三组节点初始化改成如下形式；每个节点都保留原有 id/x/y 值，并显式设置其余字段为零：

~~~c
Node node_i = {
    .id = 1,
    .x = 0.0,
    .y = 0.0,
    .fx = 0.0,
    .fy = 0.0,
    .fix_x = 0,
    .fix_y = 0
};
~~~

node_j 和零长度测试节点按各自原有的 ID 与坐标填写同样七个字段。

- [ ] **Step 2: 改写 Stage2 的两个 Node 数组**

将 tests/test_stage2.c 中两处三节点数组的每个位置初始化改为指定字段形式。例如保留三角形节点的原值：

~~~c
{
    .id = 1,
    .x = 0.0,
    .y = 0.0,
    .fx = 0.0,
    .fy = 0.0,
    .fix_x = 0,
    .fix_y = 0
}
~~~

节点 2、节点 3 使用原有坐标，七个字段全部显式出现。

- [ ] **Step 3: 改写 Stage1 演示节点初始化**

在 src/main.c 将两个 const Node 初始化改为指定字段形式，保留原有节点 ID 与坐标，并将 fx、fy、fix_x、fix_y 显式设为零。

- [ ] **Step 4: 严格编译并运行 Stage1、Stage2 与演示程序**

运行以下三组命令：

~~~powershell
$env:Path = 'C:\msys64\ucrt64\bin;' + $env:Path
$gcc = 'C:\msys64\ucrt64\bin\gcc.exe'
$stage1 = Join-Path ([System.IO.Path]::GetTempPath()) 'cfe_stage1_warning_cleanup.exe'
$stage2 = Join-Path ([System.IO.Path]::GetTempPath()) 'cfe_stage2_warning_cleanup.exe'
$demo = Join-Path ([System.IO.Path]::GetTempPath()) 'cfe_demo_warning_cleanup.exe'
& $gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage1.c src\fem.c src\solver.c -Iinclude -o $stage1 -lm
if ($LASTEXITCODE -ne 0) { throw 'Stage1 compile failed' }
& $stage1
if ($LASTEXITCODE -ne 0) { throw 'Stage1 test failed' }
& $gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage2.c src\fem.c src\solver.c -Iinclude -o $stage2 -lm
if ($LASTEXITCODE -ne 0) { throw 'Stage2 compile failed' }
& $stage2
if ($LASTEXITCODE -ne 0) { throw 'Stage2 test failed' }
& $gcc -std=c11 -Wall -Wextra -pedantic src\main.c src\fem.c src\solver.c -Iinclude -o $demo -lm
if ($LASTEXITCODE -ne 0) { throw 'Demo compile failed' }
& $demo
if ($LASTEXITCODE -ne 0) { throw 'Demo failed' }
~~~

预期：三次编译均无 warning，Stage1/Stage2 测试通过，演示程序输出与基线一致。

- [ ] **Step 5: 检查差异并提交**

运行 git diff --check，确认初始化值未改变；然后提交：

~~~powershell
git add tests/test_stage1.c tests/test_stage2.c src/main.c
git commit -m "test: make Node initializers explicit"
~~~

## Task 3: 清理 Stage10 矩阵限定符警告并完成全量严格编译

**Files:**
- Modify: tests/test_stage10.c
- Test: tests/test_stage10.c 和 Stage1–Stage10 全部严格编译

**Interfaces:**
- Consumes: Task 1 提供的 test_readonly_matrix。
- Produces: Stage10 对只读矩阵 API 的无警告调用。

- [ ] **Step 1: 接入共享测试辅助头文件**

在 tests/test_stage10.c 增加：

~~~c
#include "test_helpers.h"
~~~

- [ ] **Step 2: 更新 Stage10 的两个只读矩阵参数**

在 run_model_case 中，将两处调用改为：

~~~c
assert(solve_constrained_system(test_readonly_matrix(global_k), force,
                                free_dofs, free_count,
                                constrained_dofs, constrained_count,
                                displacement) == FEM_OK);
~~~

~~~c
assert(calculate_support_reactions(test_readonly_matrix(global_k), force,
                                   displacement, constrained_dofs,
                                   constrained_count, reactions) == FEM_OK);
~~~

其余计算顺序、断言、输出文件和清理逻辑保持不变。

- [ ] **Step 3: 严格编译并运行 Stage10**

运行：

~~~powershell
$env:Path = 'C:\msys64\ucrt64\bin;' + $env:Path
$gcc = 'C:\msys64\ucrt64\bin\gcc.exe'
$out = Join-Path ([System.IO.Path]::GetTempPath()) 'cfe_stage10_warning_cleanup.exe'
& $gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage10.c src\fem.c src\solver.c src\reactions.c src\postprocess.c src\io.c src\output.c -Iinclude -o $out -lm
if ($LASTEXITCODE -ne 0) { throw 'Stage10 compile failed' }
& $out
if ($LASTEXITCODE -ne 0) { throw 'Stage10 test failed' }
~~~

预期：无 warning，输出为 Stage 10 project organization contract tests passed.

- [ ] **Step 4: 提交 Stage10 变更**

~~~powershell
git diff --check
git add tests/test_stage10.c
git commit -m "test: apply matrix qualifier adapter to stage 10"
~~~

- [ ] **Step 5: 运行完整本地严格编译矩阵**

按以下源码映射逐项使用同一组参数 -std=c11 -Wall -Wextra -pedantic -Iinclude -o TEMP_EXE -lm 编译并运行：

~~~text
stage1:  tests/test_stage1.c  src/fem.c src/solver.c
stage2:  tests/test_stage2.c  src/fem.c src/solver.c
stage3:  tests/test_stage3.c  src/fem.c src/solver.c
stage4:  tests/test_stage4.c  src/fem.c src/solver.c
stage5:  tests/test_stage5.c  src/fem.c src/solver.c
stage6:  tests/test_stage6.c  src/postprocess.c
stage7:  tests/test_stage7.c  src/fem.c src/solver.c src/reactions.c
stage8:  tests/test_stage8.c  src/fem.c src/solver.c src/reactions.c src/io.c
stage9:  tests/test_stage9.c  src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c
stage10: tests/test_stage10.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c
demo:    src/main.c           src/fem.c src/solver.c
~~~

每个编译和运行退出码必须为 0；Stage9 允许保留 Windows 平台上的既有不可确定写失败测试跳过说明，但不得出现编译警告。

## Task 4: 完成验证记录并检查基线分支未受影响

**Files:**
- Create: docs/superpowers/verification/2026-08-12-warning-free-maintainability.md
- Test: 完整本地测试、Docker 构建与运行（Docker 引擎可用时）

**Interfaces:**
- Consumes: Task 3 的严格编译矩阵结果和现有 Dockerfile 验证流程。
- Produces: 带日期、命令、退出码、警告结论和平台限制说明的验证记录。

- [ ] **Step 1: 运行 Docker 验证**

在优化工作区根目录运行：

~~~powershell
docker build --load -t c-fe-warning-cleanup .
if ($LASTEXITCODE -ne 0) { throw 'Docker build failed' }
docker run --rm c-fe-warning-cleanup
if ($LASTEXITCODE -ne 0) { throw 'Docker run failed' }
~~~

如果 Docker 引擎不可用，在验证文档中明确记录命令、失败原因和“未执行真实镜像验收”，不得将其描述为通过。

- [ ] **Step 2: 编写验证记录**

验证文档至少记录：

~~~text
基线：f109bcd Merge pull request #6
工作分支：optimization-warning-cleanup
本地编译：Stage1–Stage10 + demo，严格 C11，全部退出码 0，无 warning
本地运行：Stage1–Stage10 + demo，全部通过
Stage9 平台说明：Windows 无便携式确定性满设备写失败等价物，因此既有测试跳过
Docker：记录实际 build/run 结果或不可用原因
基线分支：stage9-results-output 工作区干净，HEAD 未改变
~~~

- [ ] **Step 3: 最终检查并提交验证记录**

运行：

~~~powershell
git diff --check
git status --short --branch
git log --oneline --decorate -4
~~~

确认优化分支只包含本计划范围内的提交和文档，然后提交：

~~~powershell
git add docs/superpowers/verification/2026-08-12-warning-free-maintainability.md
git commit -m "docs: record warning cleanup verification"
~~~

- [ ] **Step 4: 核对原分支未变化**

在原工作区 C:\Users\jking1\Desktop\my-project\c-FE\stage9-results-output 运行：

~~~powershell
git status --short --branch
git rev-parse HEAD
git branch --show-current
~~~

预期：分支仍为 stage9-results-output，HEAD 仍为 f109bcd43e2f3db9407fcd6dafba05d9ced86e1a，工作区无未提交修改。

