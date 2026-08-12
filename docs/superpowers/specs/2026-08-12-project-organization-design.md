# Stage 10 规模化算例与项目整理设计

## 目标

在 Stage 1–9 已通过的基础上，增加不同规模的固定容量桁架算例，验证文件输入、求解、后处理、反力检查和结果导出的完整链路，并整理项目 README 与项目报告。Stage10 不改变有限元核心算法和既有公共接口。

## 范围与边界

- 继续使用 C11、固定容量数组和现有 `MAX_NODES=10`、`MAX_ELEMENTS=20` 限制。
- 不引入动态内存、稀疏矩阵、GUI、外部 FEM 库或新的命令行框架。
- 保留现有 `main.c` 的 Stage1 演示行为；规模化验证由独立 Stage10 契约测试驱动。
- 所有新增模型都使用 Stage8 已批准的 `NODES/ELEMENTS/LOADS/CONSTRAINTS` 格式。
- README 和项目报告只记录已经由测试或 Docker 验证的事实。

## 规模化模型

新增两个可读模型文件：

### `tests/data/medium.model`

- 6 个节点、8 个单元；满足 Stage10 的 5–10 节点、8–20 单元范围。
- 使用上下两排节点组成的三角化桁架：底部节点为主要支承节点，顶部节点施加竖向荷载。
- 左端约束 X/Y，右端约束 Y，避免刚体运动并保持可解。
- 所有单元使用正的 `E`、`A`，节点 ID 和单元 ID 为正且唯一。

### `tests/data/large.model`

- 10 个节点、20 个单元，达到固定容量上限的规模化回归边界。
- 使用五跨上下弦杆、端部杆和交替腹杆构成三角化桁架。
- 左端约束 X/Y，右端约束 Y；多个顶部节点施加有限竖向荷载。
- 所有连接引用有效节点，模型几何无零长度单元，整体刚度可通过现有求解器求解。

模型文件中的用户 ID 可以保持连续，但测试另行确认输出使用模型 ID 而非内部数组索引。两个文件都保留在 `tests/data`，作为可复现的教学输入样例。

## 端到端测试设计

新增 `tests/test_stage10.c`，对两个模型执行相同流程：

```text
read_model_file
    ↓
assemble_global_stiffness
    ↓
build_force_vector + identify_dofs
    ↓
solve_constrained_system
    ↓
calculate_element_result（每个单元）
    ↓
calculate_support_reactions
    ↓
check_global_equilibrium
    ↓
write_results_txt / write_results_markdown / write_results_csv
```

每个算例必须验证：

- 读取成功，节点数/单元数分别为 6/8 与 10/20；
- 总体刚度组装成功，且求解返回 `FEM_OK`；
- 所有位移、单元结果、反力和平衡残差为有限值；
- 每个单元均能完成后处理；
- 约束反力计算成功，整体 `residual_fx`、`residual_fy` 在 `SOLVER_TOL` 的放大容差内；
- TXT、Markdown、CSV 三种结果文件均生成，包含算例中的节点/单元数量和关键用户 ID；
- 输出文件在测试结束时删除，测试不依赖外部工具或 mock。

测试还包含一个规模边界断言：10 节点和 20 单元模型必须被接受，证明当前固定容量边界可用；不会测试超过容量的模型，因为 Stage8 已覆盖容量拒绝。

## README 内容

新增根目录 `README.md`，包括：

1. 项目定位和二维桁架假设；
2. Stage1–Stage10 状态表；
3. Docker 构建与运行方式；
4. 本地 C11 编译方式；
5. Stage8 输入文件格式示例；
6. Stage9 TXT、Markdown、CSV 输出说明；
7. Stage10 `medium.model` 与 `large.model` 的运行/测试方式；
8. 固定容量限制和已知限制；
9. 不属于第一版的后续方向。

## 项目报告内容

新增 `docs/project-report.md`，以教学报告形式总结：

- 问题定义、单位和理论假设；
- 从单元刚度到规模化回归的 Stage1–Stage10 路线；
- 各模块职责与数据流；
- 三角桁架、medium、large 算例的验证方法；
- 输入校验、结果导出、Debug 和 Docker 验证；
- 现有固定容量和稠密矩阵限制；
- 实际通过的测试和已知编译警告。

报告不复制完整源码，不虚构位移参考值；数值结论只引用测试输出或验证记录。

## Docker 集成

在现有 Stage9 Docker 检查之后，增加 Stage10 编译和运行：

```dockerfile
RUN gcc -std=c11 -Wall -Wextra -pedantic \
        tests/test_stage10.c src/fem.c src/solver.c src/reactions.c \
        src/postprocess.c src/io.c src/output.c \
        -Iinclude -o test_stage10 -lm

RUN ./test_stage10
```

运行时镜像和 `CMD ["./fem"]` 保持不变。Docker 验证记录应区分构建阶段的 Stage10 测试与运行阶段的既有 Demo。

## 验收标准

- 6/8 与 10/20 两个模型都能完成输入、求解、后处理、反力、平衡和三种结果导出；
- Stage1–Stage10 回归测试全部通过；
- Docker 构建和运行通过；
- README 与项目报告内容完整具体，且无与实现矛盾的描述；
- 不引入动态内存或未批准的公共 API 变化；
- 当前 Stage9 分支不被修改，Stage10 在独立分支完成。
