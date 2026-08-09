# Stage 2：总体刚度矩阵组装实施计划

> For agentic workers: REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax.

目标：在 Stage 1 单元计算的基础上，实现固定容量二维桁架总体刚度矩阵的组装，并用三角桁架测试自由度映射、累加和对称性。

架构：保留现有 Node、Element 和 Stage 1 函数。新增 assemble_global_stiffness，在一个边界内完成输入检查、单元几何初始化、4×4 单元刚度计算和总体矩阵累加；不加入荷载、边界条件或求解器。

技术栈：标准 C11、GCC/MinGW-w64、double、<math.h>，不引入外部矩阵库或构建系统。

## 全局约束

- 使用 mm、N、MPa 和 mm²，所有几何和刚度值使用 double。
- 使用固定容量 MAX_NODES = 10 和 MAX_DOF = 2 * MAX_NODES。
- Element.node1 与 Element.node2 是从 0 开始的内部节点索引。
- 组装必须使用 +=，不能覆盖已经组装的总体矩阵项。
- 非法节点索引返回 FEM_INVALID_INDEX；超过固定容量返回 FEM_CAPACITY_EXCEEDED。
- 组装失败时清零总体矩阵；不修改 Stage 1 主程序、测试和 Docker 文件。

## 文件结构

| 文件 | 责任 |
|---|---|
| include/config.h | 固定节点数和自由度容量 |
| include/fem.h | 状态码和总体组装公开接口 |
| src/fem.c | 矩阵清零、校验、映射与累加 |
| tests/test_stage2.c | 三角桁架数值、对称性、清零和错误路径测试 |

新增接口必须精确为：

    FemStatus assemble_global_stiffness(const Node *nodes,
                                        int node_count,
                                        Element *elements,
                                        int element_count,
                                        double global_k[MAX_DOF][MAX_DOF]);

### Task 1：先写失败测试并定义接口

文件：修改 include/config.h、include/fem.h；创建 tests/test_stage2.c。

- [x] Step 1：增加容量宏

在 include/config.h 的 GEOMETRY_TOL 后加入：

    #define MAX_NODES 10
    #define MAX_DOF (2 * MAX_NODES)

- [x] Step 2：增加状态码和声明

在 FemStatus 末尾加入 FEM_INVALID_INDEX、FEM_CAPACITY_EXCEEDED；让 fem.h 包含 config.h，并加入上面的 assemble_global_stiffness 声明及中文有限元语义注释。

- [x] Step 3：写独立测试

tests/test_stage2.c 必须测试：三角桁架 3 节点/3 单元的完整 6×6 矩阵；三个单元的长度和方向余弦；矩阵对称性；第 6 行/列之外的容量区域为零；非法节点索引清零并返回 FEM_INVALID_INDEX；MAX_NODES + 1 返回 FEM_CAPACITY_EXCEEDED；新增状态文本。

测试模型：

    const Node nodes[3] = {
        {1, 0.0, 0.0}, {2, 1000.0, 0.0}, {3, 500.0, 800.0}
    };
    Element elements[3] = {
        {1, 0, 1, 210000.0, 100.0, 0.0, 0.0, 0.0},
        {2, 0, 2, 210000.0, 100.0, 0.0, 0.0, 0.0},
        {3, 1, 2, 210000.0, 100.0, 0.0, 0.0, 0.0}
    };

参考矩阵：

    27252.796483  10004.474373 -21000.000000      0.000000  -6252.796483 -10004.474373
    10004.474373  16007.158997      0.000000      0.000000 -10004.474373 -16007.158997
    -21000.000000     0.000000  27252.796483 -10004.474373 -6252.796483  10004.474373
         0.000000     0.000000 -10004.474373  16007.158997 10004.474373 -16007.158997
    -6252.796483 -10004.474373 -6252.796483  10004.474373 12505.592966      0.000000
    -10004.474373 -16007.158997 10004.474373 -16007.158997     0.000000 32014.317994

使用 Stage 1 的 expect_close / expect_status 风格，成功时输出 Stage 2 tests passed.。

- [x] Step 4：确认红状态并提交测试契约

    gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage1.c src\fem.c -Iinclude -o stage1_tests.exe -lm
    gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage2.c src\fem.c -Iinclude -o stage2_tests.exe -lm

预期 Stage 1 编译成功，Stage 2 在链接阶段因新函数尚未实现而失败。删除临时程序并提交：

    Remove-Item -LiteralPath '.\stage1_tests.exe', '.\stage2_tests.exe' -ErrorAction SilentlyContinue
    git add include\config.h include\fem.h tests\test_stage2.c
    git commit -m "test: define stage 2 stiffness assembly contract"

### Task 2：实现总体矩阵组装

文件：修改 src/fem.c。

- [x] Step 1：增加完整矩阵清零函数

    static void clear_global_matrix(double global_k[MAX_DOF][MAX_DOF])
    {
        int i;
        int j;

        for (i = 0; i < MAX_DOF; ++i) {
            for (j = 0; j < MAX_DOF; ++j) {
                global_k[i][j] = 0.0;
            }
        }
    }

- [x] Step 2：按固定流程实现组装

实现 assemble_global_stiffness：先检查空指针、正数计数和 MAX_NODES；再清零矩阵并验证所有端点索引；逐个调用 Stage 1 的 calculate_element_geometry 与 calculate_element_stiffness；使用 int dof_map[4] = {2*node1, 2*node1+1, 2*node2, 2*node2+1}；双层循环执行 global_k[dof_map[a]][dof_map[b]] += ke[a][b]。任何中途错误都再次清零并返回原状态码。

函数骨架：

    FemStatus assemble_global_stiffness(const Node *nodes, int node_count,
                                        Element *elements, int element_count,
                                        double global_k[MAX_DOF][MAX_DOF])
    {
        int element_index, a, b, dof_map[4];
        double ke[4][4];
        FemStatus status;

        if (nodes == NULL || elements == NULL || global_k == NULL ||
            node_count <= 0 || element_count <= 0) {
            return FEM_INVALID_ARGUMENT;
        }
        if (node_count > MAX_NODES) {
            return FEM_CAPACITY_EXCEEDED;
        }
        clear_global_matrix(global_k);
        /* 验证每个 node1/node2 在 [0, node_count) 内。 */
        /* 计算几何、单元刚度，映射 dof_map，并使用 += 累加。 */
        return FEM_OK;
    }

代码实现不得留下未使用变量；必须在 -Wall -Wextra -pedantic 下无警告。函数注释需说明总体刚度矩阵是各单元刚度贡献按全局自由度累加的结果。

- [x] Step 3：补充状态文本

    case FEM_INVALID_INDEX:
        return "invalid node index";
    case FEM_CAPACITY_EXCEEDED:
        return "model exceeds fixed node capacity";

- [x] Step 4：运行绿测试和 Stage 1 回归

    gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage2.c src\fem.c -Iinclude -o stage2_tests.exe -lm
    .\stage2_tests.exe
    gcc -std=c11 -Wall -Wextra -pedantic tests\test_stage1.c src\fem.c -Iinclude -o stage1_tests.exe -lm
    .\stage1_tests.exe

预期分别输出 Stage 2 tests passed. 和 Stage 1 tests passed.，且无编译警告。

- [x] Step 5：提交实现

    Remove-Item -LiteralPath '.\stage1_tests.exe', '.\stage2_tests.exe' -ErrorAction SilentlyContinue
    git add include\config.h include\fem.h src\fem.c tests\test_stage2.c
    git commit -m "feat: assemble stage 2 global stiffness matrix"

### Task 3：范围、Docker 和环境验收

文件：修改本计划文档记录实际结果；不修改 Stage 1 代码、主程序、Dockerfile、Compose 或 .dockerignore。

- [x] Step 1：检查范围和空白错误

    git diff --check
    git status --short
    rg --files src include tests

确认没有 solver.c、matrix.c、io.c、postprocess.c、荷载向量或约束处理。

- [ ] Step 2：检查环境（Docker 引擎待 WSL 2 就绪）

    gcc --version
    docker --version
    docker compose version

若命令不存在，记录为环境验收阻塞，不伪造通过结果。Docker 可用后再执行：

    docker compose config
    docker compose build app
    docker compose run --rm app

- [x] Step 3：更新计划并提交记录

将实际完成的步骤标记为 [x]，在文末写明测试输出、提交号和环境命令结果，然后提交：

    git add docs\superpowers\plans\2026-08-10-stage2-global-stiffness.md
    git commit -m "docs: record stage 2 assembly verification"

## 执行结果（2026-08-10）

- Stage 2 测试：通过，输出 `Stage 2 tests passed.`。
- Stage 1 回归：通过，输出 `Stage 1 tests passed.`。
- Stage 1 演示：通过，长度、方向余弦和 4×4 矩阵输出保持不变。
- 编译器：MSYS2 UCRT64 GCC 16.1.0 已安装并可用。
- Docker CLI/Compose：Docker 29.6.2、Compose 5.3.1 已安装。
- Docker 引擎：未完成；当前 WSL 2 尚未就绪，`docker info` 超时，因此未宣称 Docker 构建验收通过。
- 提交：`0b85c55`（测试契约）和 `cc33e8a`（组装实现）。
