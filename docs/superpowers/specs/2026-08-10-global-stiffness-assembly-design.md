# Stage 2：总体刚度矩阵组装设计

- 设计日期：2026-08-10
- 设计状态：已获用户确认
- 项目根目录：`c_FE`

## 1. 目标与边界

Stage 2 在 Stage 1 单元几何和单元刚度矩阵的基础上，实现多个二维桁架单元向总体刚度矩阵的映射与累加。

本阶段包含：

- 使用节点数组和单元数组描述有限容量模型；
- 根据单元两端节点计算并保存 `length`、`c`、`s`；
- 将每个单元的 4×4 刚度矩阵映射到对应的全局自由度；
- 使用累加而不是覆盖完成总体矩阵组装；
- 检查节点索引和节点容量；
- 通过三角桁架模型验证矩阵位置、数值和对称性。

本阶段不包含荷载、边界条件、线性方程求解、位移、后处理、文件输入或动态内存。

## 2. 容量和接口

继续使用 C11、`double` 以及 mm、N、MPa、mm² 单位。为保持与总体设计一致，本阶段使用固定容量：

```c
#define MAX_NODES 10
#define MAX_DOF (2 * MAX_NODES)
```

在 `include/fem.h` 中新增：

```c
FemStatus assemble_global_stiffness(const Node *nodes,
                                    int node_count,
                                    Element *elements,
                                    int element_count,
                                    double global_k[MAX_DOF][MAX_DOF]);
```

`node1` 和 `node2` 继续表示从 0 开始的内部节点索引。`elements` 使用非 `const` 指针，因为组装过程中会把每个单元的几何结果写回单元结构，供后续阶段复用。

新增状态：

- `FEM_INVALID_INDEX`：单元端点索引不在 `[0, node_count)` 内；
- `FEM_CAPACITY_EXCEEDED`：节点数超过 `MAX_NODES`。

空指针、非正节点数或非正单元数返回 `FEM_INVALID_ARGUMENT`。零长度和非法材料属性继续复用 Stage 1 状态码。

## 3. 组装流程

`assemble_global_stiffness` 按以下顺序工作：

1. 检查数组、矩阵指针和计数；
2. 检查节点数不超过 `MAX_NODES`；
3. 检查所有单元端点索引有效；
4. 清零完整的 `MAX_DOF × MAX_DOF` 矩阵；
5. 对每个单元计算几何和 4×4 局部全局坐标刚度矩阵；
6. 按 `[2*i, 2*i+1, 2*j, 2*j+1]` 映射到总体自由度；
7. 对每个全局矩阵项执行 `global_k[map[a]][map[b]] += ke[a][b]`。

如果几何、材料或参数检查在组装过程中失败，函数清零矩阵后返回对应状态，避免调用方误用部分结果。

## 4. 验证模型

使用设计文档中的三角桁架：

```text
Node 1: (0, 0) mm
Node 2: (1000, 0) mm
Node 3: (500, 800) mm

Element 1: 0 - 1
Element 2: 0 - 2
Element 3: 1 - 2

E = 210000 MPa
A = 100 mm²
```

测试要求：

- 三个单元长度和方向余弦与参考值一致；
- 水平杆对总体 x 自由度产生 `+21000` 和 `-21000` 项；
- 两根斜杆进入正确的节点自由度，且共享节点的贡献发生累加；
- 6×6 有效总体矩阵保持对称；
- 预填充的矩阵在组装后不残留旧值；
- 非法节点索引和超过固定容量的模型被拒绝。

Stage 1 的独立测试和 Docker 配置保持不变。Stage 2 不要求主程序改变，避免破坏已有单元演示和 Docker 运行契约。

## 5. 文件变更

| 文件 | 责任 |
|---|---|
| `include/config.h` | 增加 `MAX_NODES` 和 `MAX_DOF` |
| `include/fem.h` | 增加状态码和总体组装声明 |
| `src/fem.c` | 实现总体矩阵清零、校验、映射和累加 |
| `tests/test_stage2.c` | 三角桁架数值、对称性和错误路径测试 |
| `docs/superpowers/plans/2026-08-10-stage2-global-stiffness.md` | 记录逐步实施和验收命令 |

