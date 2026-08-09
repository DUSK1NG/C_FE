# c_FE Docker 迁移设计

- 设计日期：2026-08-09
- 设计状态：已获用户确认
- 项目根目录：当前工作区 `c_FE`

## 1. 目标

为当前 C11 二维桁架有限元命令行项目提供可迁移的 Docker 环境，使另一台 PC 在只安装 Docker Desktop 的情况下能够：

- 构建项目；
- 自动编译并运行 Stage 1 测试；
- 运行 `fem` 主程序；
- 挂载源码继续开发、编译和测试。

本次工作只增加容器化配置，不修改现有 C 源码、头文件和算法行为。

## 2. 现状和约束

当前项目使用 GCC/MinGW-w64 和 C11，现有 Stage 1 编译入口为：

```text
src/main.c
src/fem.c
include/*.h
tests/test_stage1.c
```

当前没有 Makefile、CMake 配置、数据库、网络服务或需要持久化的外部数据。项目的主程序和测试程序都是一次性执行并退出的命令行程序。

现有编译参数保留为：

```text
-std=c11 -Wall -Wextra -pedantic -lm
```

## 3. 方案选择

已比较以下方案：

1. 单阶段 Dockerfile：实现简单，但编译环境和运行环境混在一起，镜像较大。
2. 多阶段 Dockerfile 加 Compose：同时支持开发、测试和精简运行镜像，且不需要引入新的构建系统。
3. Docker 加 CMake 加 VS Code Dev Container：开发体验更完整，但对当前 Stage 1 项目增加了不必要的工具和配置。

选择方案 2。

## 4. 容器架构

### 4.1 `development` 阶段

- 基础镜像：`gcc:13-bookworm`；
- 工作目录：`/workspace`；
- 不在镜像构建时复制源码；
- 由 Compose 将宿主机项目根目录挂载到 `/workspace`；
- 默认启动 shell，允许用户在容器内修改后的源码上重新编译、执行测试和运行程序。

### 4.2 `builder` 阶段

- 基础镜像：`gcc:13-bookworm`；
- 复制 `src`、`include` 和 `tests`；
- 编译主程序 `fem`；
- 编译测试程序 `test_stage1`；
- 执行 `./test_stage1`；
- 任一编译或测试命令失败时，镜像构建失败。

### 4.3 `runtime` 阶段

- 基础镜像：`debian:bookworm-slim`；
- 只从 `builder` 阶段复制 `fem`；
- 工作目录：`/app`；
- 默认命令：`./fem`；
- 不包含 GCC、源码或测试二进制。

## 5. 新增配置文件

### `Dockerfile`

包含 `development`、`builder` 和 `runtime` 三个阶段。`builder` 使用现有 GCC 编译参数构建并验证程序，`runtime` 只保留可执行文件。

### `compose.yaml`

定义两个服务：

```text
dev：使用 development 阶段，挂载当前源码目录，供开发和测试。
app：使用 runtime 阶段，构建并运行最终程序。
```

`dev` 服务开启交互终端，使用 `--rm` 运行时自动清理临时容器。`app` 不暴露端口，也不创建数据卷。

### `.dockerignore`

排除 Git 元数据、编辑器配置、宿主机生成的可执行文件和目标文件等内容，同时保留 `src`、`include` 和 `tests`。

## 6. 使用流程

### 6.1 构建并验证正式镜像

```powershell
docker compose build app
```

该命令会执行编译和 Stage 1 测试。

### 6.2 运行正式程序

```powershell
docker compose run --rm app
```

程序输出与直接使用 GCC 编译运行时保持一致。

### 6.3 进入开发容器

```powershell
docker compose run --rm dev sh
```

进入容器后可以执行：

```sh
gcc -std=c11 -Wall -Wextra -pedantic \
    src/main.c src/fem.c -Iinclude -o fem -lm

gcc -std=c11 -Wall -Wextra -pedantic \
    tests/test_stage1.c src/fem.c -Iinclude -o test_stage1 -lm

./test_stage1
./fem
```

源码通过目录挂载提供给容器，修改文件后重新执行编译命令即可验证变更。

### 6.4 迁移到另一台 PC

复制整个项目目录，或将代码推送到 Git 仓库。在新 PC 安装 Docker Desktop 后，在项目根目录执行：

```powershell
docker compose build app
docker compose run --rm app
```

新 PC 不要求安装 GCC、Make 或 MinGW-w64。

## 7. 测试和验收

容器化完成后必须验证：

1. `docker compose build app` 成功完成；
2. 构建日志显示 `Stage 1 tests passed.`；
3. `docker compose run --rm app` 能输出单元长度、方向余弦和刚度矩阵；
4. `docker compose run --rm dev sh` 能进入容器；
5. 在宿主机修改源码后，开发容器能重新编译并运行测试；
6. 主机不安装 GCC 时，正式镜像仍能运行；
7. 现有 C 测试在容器内保持通过。

## 8. 错误处理和边界

- 编译失败时 Docker 构建立即失败；
- 测试失败时 Docker 构建立即失败，不生成可用的正式运行镜像；
- `app` 服务按命令行程序运行，执行结束后容器退出，属于预期行为；
- 当前无端口、数据库和持久化目录，因此不配置端口映射或数据卷；
- 不将 `.env`、密钥或宿主机私密配置加入镜像上下文。

## 9. 完成标准

完成后，项目根目录具备可复现的 Docker 构建和开发入口，另一台 PC 只需安装 Docker Desktop 即可完成构建、测试和运行；现有 Stage 1 测试和程序行为不变。
