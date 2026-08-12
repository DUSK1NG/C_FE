# Stage 8 文本输入实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** 读取单一分段式模型文件，构造经过校验的 FemModel，并将其接入既有 Stage1–7 验证流程。

**Architecture:** 新增独立 io 模块和固定容量 FemModel 容器。解析器按 NODES → ELEMENTS → LOADS → CONSTRAINTS 的严格顺序逐行读取，先在局部数据中校验和映射用户编号，再一次性提交到输出模型；失败始终清空模型。既有 FEM 计算模块只消费解析后的 Node、Element 数组，不改变 Stage1–7 算法。

**Tech Stack:** C11、GCC、固定数组、标准 C 文件 I/O、现有 FemStatus 与 FEM API、Dockerfile。

## Global Constraints

- 输入文件必须按 NODES、ELEMENTS、LOADS、CONSTRAINTS 固定顺序出现。
- 允许空行和以 # 开头的整行注释，不支持行尾注释。
- NODES 和 ELEMENTS 数量必须大于 0；LOADS 和 CONSTRAINTS 数量可以为 0。
- 节点 ID、单元 ID 必须为正整数且各自唯一；用户节点 ID 映射为内部 0 基索引。
- 未列出的荷载默认为零，未列出的约束默认为自由；重复荷载/约束记录拒绝。
- 数值必须有限；E、A 必须大于零；约束标志必须为 0 或 1；拒绝未知节点、单元自连接和零长度单元。
- MAX_ELEMENTS=20；超过 MAX_NODES 或 MAX_ELEMENTS 返回 FEM_CAPACITY_EXCEEDED。
- 所有解析失败路径清空 FemModel；不使用动态内存、第三方库或全局可变状态。
- 不修改 Stage1–7 计算逻辑、main.c、Stage1–7 测试语义或反力符号。
- Dockerfile 保留现有 demo、Stage1、Stage6、Stage7 和 runtime CMD，只追加 Stage8 测试。

---

## 文件结构

- Create: include/io.h — FemModel 与 read_model_file 声明。
- Create: src/io.c — 逐行解析、用户 ID 映射、输入校验和失败清空。
- Create: tests/test_stage8.c — Stage8 契约、参考模型和错误路径测试。
- Create: tests/data/triangle.model — 参考三角桁架输入样例。
- Modify: include/config.h — 追加 MAX_ELEMENTS 20。
- Modify: include/fem.h — 追加 FEM_INPUT_ERROR。
- Modify: src/fem.c — 追加 invalid model input 状态文本。
- Modify: Dockerfile — 编译运行 Stage8 独立测试。
- Reference: docs/superpowers/specs/2026-08-12-input-parser-design.md — 已确认的设计基线。

## 接口契约

~~~c
typedef struct {
    Node nodes[MAX_NODES];
    int node_count;
    Element elements[MAX_ELEMENTS];
    int element_count;
} FemModel;

FemStatus read_model_file(const char *path, FemModel *model);
~~~

---

### Task 1: 建立 Stage8 测试契约和状态接口

**Files:**
- Create: tests/test_stage8.c
- Create: tests/data/triangle.model
- Modify: include/config.h
- Modify: include/fem.h
- Modify: src/fem.c

**Interfaces:**
- Consumes: 既有 Node、Element、Stage1–7 FEM API 与 Stage7 反力接口。
- Produces: MAX_ELEMENTS=20、FEM_INPUT_ERROR 和可由 Task 2 实现的 Stage8 测试契约。

- [ ] **Step 1: 编写参考输入文件**

创建 tests/data/triangle.model，内容必须为：

~~~text
# Reference three-bar truss

NODES 3
1 0 0
2 1000 0
3 500 800

ELEMENTS 3
1 1 2 210000 100
2 1 3 210000 100
3 2 3 210000 100

LOADS 1
3 0 -10000

CONSTRAINTS 3
1 1 1
2 0 1
3 0 0
~~~

- [ ] **Step 2: 增加容量和错误状态**

在 include/config.h 追加：

~~~c
#define MAX_ELEMENTS 20
~~~

在 include/fem.h 的 FemStatus 末尾追加 FEM_INPUT_ERROR，不重排已有成员；在 src/fem.c 的 fem_status_message 追加：

~~~c
case FEM_INPUT_ERROR:
    return "invalid model input";
~~~

- [ ] **Step 3: 编写参考模型和成功路径测试**

tests/test_stage8.c 包含 config.h、fem.h、io.h 和 reactions.h，定义现有测试风格的 ASSERT_TRUE、ASSERT_NEAR 和 ASSERT_STATUS 宏。构造内置参考模型：

~~~c
Node expected_nodes[3] = {
    {1, 0.0, 0.0, 0.0, 0.0, 1, 1},
    {2, 1000.0, 0.0, 0.0, 0.0, 0, 1},
    {3, 500.0, 800.0, 0.0, -10000.0, 0, 0}
};
Element expected_elements[3] = {
    {1, 0, 1, 210000.0, 100.0, 0.0, 0.0, 0.0},
    {2, 0, 2, 210000.0, 100.0, 0.0, 0.0, 0.0},
    {3, 1, 2, 210000.0, 100.0, 0.0, 0.0, 0.0}
};
~~~

读取 tests/data/triangle.model 后断言节点和单元数量、ID、坐标、荷载、约束、内部端点索引、材料参数全部匹配。

- [ ] **Step 4: 编写端到端结果断言**

用解析后的模型调用既有 assemble_global_stiffness、build_force_vector、identify_dofs、solve_constrained_system、calculate_support_reactions 和 check_global_equilibrium。断言所有调用返回 FEM_OK，反力 reactions[1] 约为 5000、reactions[3] 约为 5000，两个平衡残差接近零。

- [ ] **Step 5: 编写失败清空和语法错误测试**

定义 assert_invalid_content：在当前测试工作目录写入固定临时文件 stage8_invalid.model，将 FemModel 的计数和数组填入非零哨兵，调用 read_model_file，确认返回状态、node_count==0、element_count==0 和全部数组字段清零，然后删除临时文件。

至少覆盖：缺失区段、错误字段数量、错误区段顺序、尾随非注释内容、重复节点 ID、重复单元 ID、重复荷载、重复约束、未知节点、单元自连接、非法 E/A、非法约束标志、非有限数、零长度单元、容量超限和文件不存在。每个用例验证对应状态或输入错误状态。

- [ ] **Step 6: 运行失败测试确认解析器尚未实现**

~~~powershell
$env:Path = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;$env:Path"
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage8.c src/fem.c src/solver.c src/reactions.c -Iinclude -o test_stage8.exe -lm
~~~

Expected: 编译失败，因为 io.h 和 read_model_file 尚未实现；状态枚举和样例文件已经存在。

- [ ] **Step 7: 提交测试契约**

~~~powershell
git add tests/test_stage8.c tests/data/triangle.model include/config.h include/fem.h src/fem.c
git commit -m "test: define stage 8 input parser contract"
~~~

---

### Task 2: 实现固定容量文本解析器

**Files:**
- Create: include/io.h
- Create: src/io.c
- Test: tests/test_stage8.c

**Interfaces:**
- Consumes: Task 1 的 FemModel、MAX_ELEMENTS、FEM_INPUT_ERROR 和测试契约。
- Produces: read_model_file(const char *, FemModel *)，供 Stage8 测试和后续 Stage9 CLI 使用。

- [ ] **Step 1: 声明接口**

在 include/io.h 中包含 config.h、model.h 和 fem.h，使用项目现有 include guard，声明 FemModel 和 read_model_file。

- [ ] **Step 2: 实现模型清空和辅助函数**

在 src/io.c 中实现固定大小的 clear_model、行读取、空行/整行注释识别、有限数检查和用户节点 ID 查找辅助函数。使用 fgets 固定缓冲区，不调用动态内存分配函数，不使用全局可变状态。

- [ ] **Step 3: 实现严格区段读取**

按固定顺序读取四个区段头和精确数量记录。每条有效记录必须只包含规定字段；记录末尾允许空白，但不允许额外 token。文件结束后只允许空行或整行注释。任何语法、顺序或数量错误返回 FEM_INPUT_ERROR 并清空输出。

- [ ] **Step 4: 实现节点、单元和 ID 映射**

节点阶段写入局部 Node 数组并检查正 ID、唯一 ID、有限坐标和容量。单元阶段将用户端点 ID 解析为内部索引，检查未知节点、自连接、唯一单元 ID、有限材料值和正 E/A；调用 calculate_element_geometry 检查零长度，并保存 length/c/s。

- [ ] **Step 5: 实现荷载和约束应用**

荷载阶段检查节点引用、有限 fx/fy 和重复节点，然后写入对应局部节点。约束阶段检查节点引用、fix_x/fix_y 为 0/1 和重复节点，然后写入对应局部节点。未出现节点保持默认零荷载/自由约束。

- [ ] **Step 6: 实现原子提交和状态映射**

所有阶段成功后将局部模型复制到输出 FemModel 并返回 FEM_OK。任何失败先清空输出，再返回约定状态；文件打开失败返回 FEM_INPUT_ERROR，空路径或空模型指针返回 FEM_INVALID_ARGUMENT。成功结果不包含临时用户 ID 映射数据。

- [ ] **Step 7: 运行 Stage8 测试并修正到通过**

~~~powershell
$env:Path = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;$env:Path"
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage8.c src/fem.c src/solver.c src/reactions.c src/io.c -Iinclude -o test_stage8.exe -lm
.\test_stage8.exe
~~~

Expected: Stage8 contract tests passed，且没有新的编译错误。

- [ ] **Step 8: 提交解析器实现**

~~~powershell
git add include/io.h src/io.c
git commit -m "feat: parse fixed-capacity truss model files"
~~~

---

### Task 3: 接入构建、回归验证和记录

**Files:**
- Modify: Dockerfile
- Create: docs/superpowers/verification/2026-08-12-input-parser-verification.md

**Interfaces:**
- Consumes: Task 2 的 io 模块、样例文件和 Stage8 测试。
- Produces: Docker 中可独立执行的 test_stage8，以及 Stage1–8 和 Demo 的可审计验证记录。

- [ ] **Step 1: 追加 Docker Stage8 测试**

在现有 Stage7 测试之后追加：

~~~dockerfile
RUN gcc -std=c11 -Wall -Wextra -pedantic \
        tests/test_stage8.c src/fem.c src/solver.c src/reactions.c src/io.c \
        -Iinclude -o test_stage8 -lm

RUN ./test_stage8
~~~

保持 demo、Stage1、Stage6、Stage7 编译运行命令和 CMD ["./fem"] 不变。

- [ ] **Step 2: 执行本地 Stage1–8 编译运行**

使用 MSYS2 GCC 编译 tests/test_stage1.c 到 tests/test_stage8.c 的全部测试，并分别运行；Stage6 追加 src/postprocess.c，Stage7 追加 src/reactions.c，Stage8 追加 src/reactions.c 和 src/io.c。Expected：8 个测试程序均退出码 0。

- [ ] **Step 3: 执行 Demo 回归**

~~~powershell
gcc -std=c11 -Wall -Wextra -pedantic src/main.c src/fem.c src/solver.c -Iinclude -o fem.exe -lm
.\fem.exe
~~~

Expected：保持 Stage1 单杆示例输出，长度约 943.398113205660 mm，方向余弦约 c=0.529998940003、s=0.847998304005。

- [ ] **Step 4: 执行 Docker 构建和运行**

~~~powershell
$docker = Join-Path $env:LOCALAPPDATA "Programs\DockerDesktop\resources\bin\docker.exe"
& $docker build -t c-fe-stage8-input-parser .
& $docker run --rm c-fe-stage8-input-parser
~~~

Expected：构建退出码 0，Stage1、Stage6、Stage7、Stage8 的 Docker 测试步骤通过，runtime Demo 正常退出。

- [ ] **Step 5: 执行静态和分支检查**

~~~powershell
git diff --check 24d3854..HEAD
rg -n "\b(malloc|calloc|realloc|free)\s*\(" src include tests
git status --short --branch
git diff --name-status 24d3854..HEAD
~~~

Expected：无空白错误、Stage8 不引入动态内存、工作区干净且分支为 stage8-input-parser；未修改 Stage7 工作区。

- [ ] **Step 6: 提交验证记录**

~~~powershell
git add Dockerfile docs/superpowers/verification/2026-08-12-input-parser-verification.md
git commit -m "test: verify stage 8 input parser"
~~~

只有在本地和 Docker 验证均获得真实退出码后，才可报告 Stage8 完成。暂不推送、创建 PR 或合并；Stage9–10 仍需在后续阶段完成。

## 完成标准

- 参考文本文件可构造与内置三角桁架等价的 FemModel。
- 用户节点编号正确转换为内部 0 基索引。
- 非法语法、引用、容量、数值和几何输入被拒绝且输出清空。
- Stage1–8 和 Demo 本地通过，Docker Stage8 测试通过。
- 静态检查通过，分支独立且工作区干净。
