# 二维桁架有限元（C11）

一个固定容量、无动态内存分配的二维桁架有限元示例项目。项目已完成 Stage 1–10，支持读取 `.model` 文件、求解并输出 TXT、Markdown、CSV 结果。

## 快速开始

在仓库根目录编译并运行中型示例。

### Windows PowerShell（MSYS2 UCRT64）

```powershell
$gcc = "C:\msys64\ucrt64\bin\gcc.exe"
& $gcc -std=c11 -Wall -Wextra -pedantic `
  src\main.c src\cli.c src\pipeline.c src\fem.c src\solver.c `
  src\reactions.c src\postprocess.c src\io.c src\output.c `
  -Iinclude -o fem.exe -lm

New-Item -ItemType Directory -Force results | Out-Null
& .\fem.exe --input .\tests\data\medium.model `
  --output-dir .\results --prefix medium
```

### Linux / macOS

```bash
gcc -std=c11 -Wall -Wextra -pedantic \
  src/main.c src/cli.c src/pipeline.c src/fem.c src/solver.c \
  src/reactions.c src/postprocess.c src/io.c src/output.c \
  -Iinclude -o fem -lm

mkdir -p results
./fem --input tests/data/medium.model --output-dir results --prefix medium
```

结果文件会生成在 `results/`：

```text
medium.txt
medium.md
medium.csv
```

TXT 和 Markdown 使用中英文双语标题、字段名和单元状态；CSV 保留原有英文机器字段，并在末尾追加 `record_label_zh` 与 `state_bilingual`，便于程序读取和人工查看。

## 本地网页编辑器

直接打开 [`web/index.html`](web/index.html)，无需安装 Docker、Node.js、npm 或启动服务器。

网页支持：

- 加载示例或导入 `.model` 文件
- 编辑节点、单元、荷载和约束
- 校验数据并预览模型
- 在浏览器内分析示例和上传的 `.model` 文件
- 导出规范化的 `.model` 文件

网页分析由浏览器中的 JavaScript 完成，不能直接启动 `fem`；C11 CLI 仍可在终端生成 TXT、Markdown、CSV 结果。详细说明见 [`web/README.md`](web/README.md)。

## `.model` 输入格式

文件必须按以下顺序包含四个分区：

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

支持空行和以 `#` 开头的整行注释。示例文件位于 [`tests/data`](tests/data)。

## 常用命令

```bash
# 查看帮助
./fem --help

# 运行 Stage 1 演示
./fem --demo

# 使用默认输出文件名
./fem --input tests/data/medium.model

# 自定义输出目录、文件前缀和格式
./fem --input tests/data/medium.model \
  --output-dir results --prefix medium \
  --format txt,markdown --include nodes,reactions,summary
```

Windows PowerShell 将 `./fem` 替换为 `& .\fem.exe`。未指定输出选项时，默认生成 `fem_results.txt`、`fem_results.md` 和 `fem_results.csv`。

## 项目结构

```text
src/       C11 实现
include/   头文件
tests/     Stage 1–10 及回归测试
web/       本地网页模型编辑器
docs/      设计和开发文档
```

## 测试

网页模型测试：

```bash
node tests/test_web_model.js
```

C 语言测试程序位于 `tests/`，覆盖 Stage 1–10、统一管线、输出选择和 CLI 解析。

## 限制

- 使用固定容量数组，不支持无限扩展模型规模。
- 网页编辑器不会直接执行 C 程序或写入求解结果。
- 输入模型需要使用一致的单位制，程序不会自动换算单位。
