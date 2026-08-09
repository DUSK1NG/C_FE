# Stage 3：荷载与约束设计

- 设计日期：2026-08-10
- 设计状态：已获用户确认
- 工作分支：`stage3-loads-constraints`

## 1. 目标与边界

Stage 3 在 Stage 2 总体刚度矩阵的基础上，为每个节点增加二维节点荷载和方向约束，并将它们映射到固定容量的总体自由度数据结构中。

本阶段包含：

- 在 `Node` 中保存 `fx`、`fy`、`fix_x` 和 `fix_y`；
- 构建 `F_original[MAX_DOF]`；
- 按全局自由度顺序识别自由自由度和约束自由度；
- 验证荷载值、约束标志和节点容量；
- 通过独立测试验证荷载与约束的自由度映射。

本阶段不包含：

- 总体矩阵缩减；
- 高斯消元或其他线性方程求解；
- 位移、应变、应力和支座反力；
- 文件输入、动态内存和主程序流程改造；
- 对 Stage 1、Stage 2 算法或 Docker 配置的无关重构。

## 2. 数据模型

继续使用 C11、`double` 以及 mm、N、MPa 和 mm² 单位。`Node` 扩展为：

```c
typedef struct {
    int id;
    double x;
    double y;

    double fx;
    double fy;

    int fix_x;
    int fix_y;
} Node;
```

`fx` 和 `fy` 的单位为 N。`fix_x` 和 `fix_y` 必须是 `0` 或 `1`，分别表示对应方向是否约束。位移字段 `ux`、`uy` 延后到 Stage 5，不在本阶段加入。

现有只提供 `id`、`x`、`y` 的节点初始化仍然有效；C 会将新增尾部字段初始化为零，因此 Stage 1 和 Stage 2 测试不需要改变其行为。

## 3. 全局自由度映射

对于内部节点索引 `i`：

```text
Ux = 2 * i
Uy = 2 * i + 1
```

节点数组按内部索引顺序处理，忽略 `Node.id` 对自由度编号的影响。所有输出自由度数组按从小到大的顺序排列，每个节点先处理 `Ux`，再处理 `Uy`。

荷载向量的有效区域为 `[0, 2 * node_count)`；`[2 * node_count, MAX_DOF)` 必须保持为零。

## 4. 公共接口

在 `include/fem.h` 中增加：

```c
FemStatus build_force_vector(const Node *nodes,
                             int node_count,
                             double force[MAX_DOF]);

FemStatus identify_dofs(const Node *nodes,
                        int node_count,
                        int free_dofs[MAX_DOF],
                        int *free_count,
                        int constrained_dofs[MAX_DOF],
                        int *constrained_count);
```

`build_force_vector` 只读取节点数据，将节点荷载写入总体荷载向量，不修改 `nodes`。`identify_dofs` 只读取节点约束标志，并将两组自由度写入调用方提供的固定容量数组。

## 5. 状态码和错误处理

在现有 `FemStatus` 中增加：

- `FEM_INVALID_CONSTRAINT`：`fix_x` 或 `fix_y` 不是 `0` 或 `1`；
- `FEM_INVALID_LOAD`：`fx` 或 `fy` 不是有限数值，例如 NaN 或无穷大。

两个接口遵循以下规则：

1. 空指针、非正节点数返回 `FEM_INVALID_ARGUMENT`；
2. 节点数大于 `MAX_NODES` 返回 `FEM_CAPACITY_EXCEEDED`；
3. `build_force_vector` 在输出向量非空时先清零完整容量，失败时保持全零；
4. `identify_dofs` 在输出数组和计数指针非空时先清空数组和计数，失败时保持空结果；
5. `build_force_vector` 在写入前检查所有荷载值，避免产生部分有效、部分无效的向量；
6. `identify_dofs` 在写入自由度集合前检查所有约束标志，避免产生部分有效集合；
7. 两个接口不得修改节点坐标、节点编号或 Stage 2 单元数据。

`fem_status_message` 同步增加两个新状态的可读文本。

## 6. 数据流

Stage 3 的调用顺序为：

```text
Node 数组
   ├─ build_force_vector() ──> F_original[MAX_DOF]
   └─ identify_dofs() ───────> free_dofs[] / constrained_dofs[]
```

本阶段只建立原始荷载向量和自由度集合。后续 Stage 4/Stage 5 才会使用这些结果构造缩减矩阵和求解位移；原始总体刚度矩阵、原始荷载向量以及自由度集合必须保留。

## 7. 验证模型与测试

使用 Stage 2 的三角桁架节点作为映射模型：

```c
const Node nodes[3] = {
    {1, 0.0,    0.0,   0.0,      0.0,      1, 1},
    {2, 1000.0, 0.0,   0.0,      0.0,      0, 1},
    {3, 500.0,  800.0, 0.0, -10000.0,      0, 0}
};
```

验收结果：

- 荷载向量前 6 项为 `[0, 0, 0, 0, 0, -10000]`；
- `free_dofs` 为 `[2, 4, 5]`；
- `constrained_dofs` 为 `[0, 1, 3]`；
- 两个输出数组的剩余容量区域为零；
- 非法约束标志返回 `FEM_INVALID_CONSTRAINT` 且结果清空；
- NaN/无穷荷载返回 `FEM_INVALID_LOAD` 且向量清零；
- 空指针、非法节点数和超容量分别返回对应状态；
- Stage 1 和 Stage 2 测试继续通过。

新增 `tests/test_stage3.c`，保持现有测试的 `expect_close`、`expect_status` 风格，并在成功时输出 `Stage 3 tests passed.`。

## 8. 文件范围

| 文件 | 职责 |
|---|---|
| `include/model.h` | 扩展 `Node` 的荷载与约束字段 |
| `include/fem.h` | 增加 Stage 3 状态码和公共接口 |
| `src/fem.c` | 实现荷载向量构建、自由度集合识别和错误文本 |
| `tests/test_stage3.c` | 荷载、约束、容量和错误路径测试 |
| `docs/superpowers/plans/2026-08-10-loads-constraints.md` | 记录实施步骤和验收结果 |

Stage 3 不修改 `src/main.c`、Dockerfile、Compose 文件以及 Stage 1/Stage 2 测试文件，除非回归编译证明新增 `Node` 字段需要最小兼容调整。
