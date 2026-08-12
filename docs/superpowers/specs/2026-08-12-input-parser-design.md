# Stage 8 文本输入设计

## 目标

为二维桁架增加单一模型文本文件的读取能力，使节点、单元、荷载和约束可以从文件构造，并在进入既有 Stage1–7 有限元流程前完成结构化校验。Stage8 只增加输入解析，不改变刚度组装、约束求解、单元后处理或支座反力算法。

## 输入格式

文件由四个按固定顺序出现的区段组成：`NODES`、`ELEMENTS`、`LOADS`、`CONSTRAINTS`。每个区段首行声明记录数量，随后读取精确数量的记录。

```text
NODES <count>
<id> <x> <y>

ELEMENTS <count>
<id> <node1_id> <node2_id> <E> <A>

LOADS <count>
<node_id> <fx> <fy>

CONSTRAINTS <count>
<node_id> <fix_x> <fix_y>
```

规则如下：

- 区段名和字段数量必须精确匹配；
- 允许空行和以 `#` 开头的整行注释，不支持行尾注释；
- 四个区段必须各出现一次且按上述顺序出现；
- `NODES` 和 `ELEMENTS` 数量必须大于 0，`LOADS` 和 `CONSTRAINTS` 数量可以为 0；
- 实际记录数量必须与声明数量一致，文件结束后只能有空行或注释；
- 节点 ID、单元 ID 必须为正整数且在各自区段内唯一，允许编号不连续；
- 单元、荷载和约束使用用户节点 ID，解析后转换为内部 0 基节点索引；
- 未列出的节点荷载默认为 `fx=0`、`fy=0`，未列出的节点约束默认为 `fix_x=0`、`fix_y=0`；
- 同一节点重复出现在 `LOADS` 或 `CONSTRAINTS` 中直接拒绝；
- 所有数值必须为有限数，材料参数 `E` 和 `A` 必须大于零，约束标志必须为 0 或 1；
- 拒绝未知节点、单元自连接、零长度单元和超过固定容量的模型。

示例三角桁架文件：

```text
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
```

## 数据结构与接口

新增 `include/io.h`：

```c
typedef struct {
    Node nodes[MAX_NODES];
    int node_count;
    Element elements[MAX_ELEMENTS];
    int element_count;
} FemModel;

FemStatus read_model_file(const char *path, FemModel *model);
```

`FemModel` 将既有节点、单元和数量封装在一个固定大小对象中，调用者可以直接将 `model.nodes`、`model.elements` 传给现有 FEM API。`Element.node1` 和 `Element.node2` 保存内部 0 基索引；`Node.id` 和 `Element.id` 保留文件中的用户编号。

新增 `src/io.c`，使用固定大小缓冲区和局部数组解析，不使用动态内存或全局可变状态。解析函数在入口清空 `FemModel`；任何失败路径都保持空模型，禁止向调用者暴露部分解析结果。

在 `include/config.h` 增加：

```c
#define MAX_ELEMENTS 20
```

该容量覆盖 Stage10 规划的 8–20 单元规模，并与当前 `MAX_NODES=10`、`MAX_DOF=20` 的固定容量策略一致。

## 状态与错误处理

在 `FemStatus` 末尾追加 `FEM_INPUT_ERROR`，在 `fem_status_message` 中返回 `"invalid model input"`。状态规则：

- 空路径、空模型指针返回 `FEM_INVALID_ARGUMENT`；
- 文件无法打开、区段顺序错误、语法错误、重复 ID、未知引用或重复荷载/约束返回 `FEM_INPUT_ERROR`；
- 节点或单元数量超过固定容量返回 `FEM_CAPACITY_EXCEEDED`；
- `E/A <= 0` 返回 `FEM_INVALID_PROPERTY`；
- 单元两端相同或几何长度低于 `GEOMETRY_TOL` 返回 `FEM_ZERO_LENGTH`；
- 非法约束标志返回 `FEM_INVALID_CONSTRAINT`；
- 非有限荷载返回 `FEM_INVALID_LOAD`；
- 所有失败状态都清空 `FemModel`，输入路径和调用者数据不被修改。

解析器先读取节点并建立用户 ID 到数组索引的固定大小映射，再解析单元、荷载和约束。重复或未找到的 ID 在写入最终模型前被拒绝。单元几何校验复用 `calculate_element_geometry`，避免 Stage8 重新定义零长度判定。

## 测试策略

新增 `tests/test_stage8.c` 和 `tests/data/triangle.model`：

1. 读取参考文件，验证 3 个节点、3 个单元、Node3 `Fy=-10000`、约束 `{Node1:1,1; Node2:0,1; Node3:0,0}`；
2. 将解析模型送入既有 Stage1–7 API，验证参考反力 Node1 `Ry=5000 N`、Node2 `Ry=5000 N`，且全局平衡通过；
3. 验证空行、整行注释和不连续但合法的用户 ID；
4. 验证缺失区段、错误字段数量、错误区段顺序、尾随非注释内容、重复节点/单元 ID、重复荷载/约束、未知节点、单元自连接、非法 `E/A`、非法约束标志、非有限数、零长度单元和容量超限；
5. 每个失败用例先写入非零模型哨兵，再确认返回状态符合约定且模型清空；
6. Stage1–7 测试和 Stage1 Demo 保持回归通过。

Dockerfile 仅追加 Stage8 测试的编译和运行步骤，不改变既有 demo、Stage1、Stage6、Stage7 测试和 runtime CMD。

## 非目标

- 不在 Stage8 修改 `main.c` 为命令行程序；
- 不实现 Stage9 TXT/CSV 导出或 DEBUG 输出；
- 不引入动态内存、第三方解析库或任意规模矩阵；
- 不修改 Stage1–7 的计算公式、接口和结果语义；
- 不推送、不创建合并提交，Stage8 完成后仍等待全部 10 个 Stage 完成再整合。
