# 二维桁架有限元
这是一个 C11 命令行程序，用二维桁架模型计算节点位移、支座反力和单元结果，并导出 TXT、Markdown 与 CSV 报告。

## 快速开始

先将支持 C11 的 GCC 加入 PATH；Windows 使用 UCRT 运行库版本（如 MSYS2 UCRT64）。在仓库根目录编译并运行中型示例：

```powershell
$gcc = (Get-Command gcc).Source
& $gcc -std=c11 -Wall -Wextra -pedantic `
  src\main.c src\cli.c src\pipeline.c src\fem.c src\solver.c `
  src\reactions.c src\postprocess.c src\io.c src\output.c `
  -Iinclude -o fem.exe -lm
New-Item -ItemType Directory -Force results | Out-Null
& .\fem.exe --input .\tests\data\medium.model --output-dir .\results --prefix medium
Get-ChildItem results -Name
```

实际生成的文件名：

```text
medium.csv
medium.md
medium.txt
```

## 使用

输入文件依次包含 `NODES`、`ELEMENTS`、`LOADS` 和 `CONSTRAINTS` 分区，字段格式见 [网页编辑器说明](web/README.md)。程序不会换算单位；同一个模型中的坐标、弹性模量、面积和荷载必须使用一致单位制。

仓库还提供本地网页编辑器，可在浏览器中导入、编辑、分析和导出 `.model` 文件。网页分析在浏览器 JavaScript 中运行，不能启动 C 命令行求解器。网页使用步骤和模型容量见 [web/README.md](web/README.md)。

命令行输出格式、报告内容和默认文件名见[命令行使用说明](docs/usage.md)。

## 开发

网页模型测试：

```powershell
node tests/test_web_model.js
```

实际输出：

```text
web model tests passed
```

其他 C 测试程序位于 `tests/`，覆盖求解阶段、统一管线、输出选择和命令行解析。
