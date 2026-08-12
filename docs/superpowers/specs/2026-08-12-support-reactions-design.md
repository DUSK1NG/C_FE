# Stage 7 支座反力与整体平衡设计

## 目标

在 Stage 5 的约束系统求解和 Stage 6 的单元后处理中，增加支座反力计算与整体平衡检查。Stage 7 只负责从已经得到的完整位移恢复支座反力，并验证外力与支反力的全局平衡；不改变既有求解器、不扩展输入文件格式，也不把后处理状态写入 `Element`。

## 计算约定

对原始、未修改的整体方程

```text
K_original * U_complete = F_original + R_support
```

支座反力按下式计算：

```text
R = K_original * U_complete - F_original
```

其中：

- `U_complete` 是 Stage 5 返回的完整自由度位移向量；
- `F_original` 是施加约束消元前的原始荷载向量；
- `K_original` 是施加约束消元前的原始整体刚度矩阵；
- 只有受约束自由度的 `R` 才是支座反力；自由自由度的输出固定为 `0`；
- 反力正号表示支承作用方向，单位与输入荷载一致。

三角桁架参考模型的支座反力应为 Node1 `Ry=5000 N`、Node2 `Ry=5000 N`，Node1 `Rx=0 N`。

## 模块与接口

新增 `include/reactions.h` 和 `src/reactions.c`，提供两个彼此独立的接口：

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

`calculate_support_reactions` 先在局部数组中计算完整 `K*U-F`，校验全部输入和中间结果，再将受约束自由度的值写入 `reactions`；函数开始时清零输出，失败时保持全零。约束数量可以为零，此时合法输入得到全零反力。约束自由度必须在 `[0, MAX_DOF)` 内且不得重复。

`check_global_equilibrium` 按二维桁架自由度布局累加：偶数自由度计入 X 方向，奇数自由度计入 Y 方向，分别计算

```text
residual_fx = Σ(force[2*i]   + reactions[2*i])
residual_fy = Σ(force[2*i+1] + reactions[2*i+1])
```

当两个残差绝对值都不超过 `tolerance` 时返回 `FEM_OK`；输入合法但超过容差时返回新增的 `FEM_EQUILIBRIUM_ERROR`，同时保留实际残差供诊断。非法指针、负数或非有限容差、非有限向量值返回 `FEM_INVALID_ARGUMENT`，并清零残差输出。

在 `FemStatus` 末尾追加 `FEM_EQUILIBRIUM_ERROR`，并在 `fem_status_message` 中提供对应文本，避免改变既有状态值的顺序和含义。

## 错误处理与数值边界

- 所有指针参数都必须非空。
- `global_k`、`force`、`displacement`、`reactions` 和残差中间值必须为有限浮点数。
- `constrained_count` 必须在 `0..MAX_DOF` 范围内。
- 平衡容差必须为有限且不小于零。
- 计算使用 `double`，不额外引入动态内存或全局可变状态。
- 函数不修改任何输入数组；输出在失败路径上保持确定的清零状态。

## 测试策略

新增 `tests/test_stage7.c`，使用已有三角桁架模型的刚度、荷载、完整位移和 Stage 5 约束自由度，验证：

1. Node1 `Ry` 和 Node2 `Ry` 为 `5000 N`，Node1 `Rx` 为 `0 N`；
2. 非约束自由度的反力保持为 `0`；
3. X/Y 平衡残差在指定容差内；
4. 越界/重复约束、非法容差和非有限输入被拒绝，失败输出清零；
5. 扰动位移使平衡超出容差时返回 `FEM_EQUILIBRIUM_ERROR`，并返回实际残差；
6. 既有 Stage 1–6 测试仍可独立编译运行。

Dockerfile 仅追加 Stage 7 独立测试的编译和运行步骤，保留现有 demo 与 Stage 1–6 测试行为。

## 非目标

- 不修改 `solve_constrained_system` 的算法或输入输出；
- 不把反力、应力等结果字段加入 `Element`；
- 不实现 Stage 8 文本输入、Stage 9 结果导出或 Stage 10 大型案例；
- 不在本阶段合并分支或修改 Stage6 分支。
