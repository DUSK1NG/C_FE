# 本地网页模型配置器

这是一个无服务器、无第三方依赖的静态网页，用来编辑、导入、校验、预览和导出二维桁架有限元 `.model` 文件。它与现有 C11 求解器使用同一种输入格式，但不会修改或直接调用 C 源代码。

## 打开页面

从仓库根目录执行：

```powershell
# Windows PowerShell
Start-Process .\web\index.html
```

```bash
# Linux
xdg-open web/index.html

# macOS
open web/index.html
```

也可以直接在文件管理器中双击 `web/index.html`。页面通过 `file://` 本地打开，不需要 `npm install`、Web 服务器或网络连接。

## 使用流程

1. 点击“加载示例”，或点击“导入 .model”并选择本地模型文件。
2. 在节点、单元、荷载和约束表格中新增、删除或修改记录。
3. 查看“当前状态”和 `.model` 预览；存在错误时导出按钮会被禁用。
4. 在“导出文件名”中输入名称，点击“导出 .model”。浏览器会把规范化后的文本文件下载到浏览器配置的下载位置。
5. 将导出的文件放到仓库根目录，或在命令中使用它的实际路径，然后在终端运行页面显示的 `fem --input ...` 命令。

页面显示的命令契约只包含输入文件，例如 `fem --input 'custom.model'`。Windows PowerShell 使用已编译的 `fem.exe` 时，将命令开头改成 `.\fem.exe`：

```powershell
& .\fem.exe --input .\custom.model
```

Linux/macOS 使用已编译的 `fem`：

```bash
./fem --input ./custom.model
```

默认会在当前目录生成：

- `fem_results.txt`
- `fem_results.md`
- `fem_results.csv`

如果希望写入 `results/custom.*` 等自定义位置，请在终端里显式追加输出选项；输出目录必须事先存在。

Windows PowerShell：

```powershell
New-Item -ItemType Directory -Force results | Out-Null
& .\fem.exe --input .\custom.model --output-dir .\results --prefix custom
```

Linux/macOS：

```bash
mkdir -p results
./fem --input ./custom.model --output-dir ./results --prefix custom
```

## 字段与格式

模型必须按以下四个分区排列：

| 分区 | 每行字段 | 说明 |
|---|---|---|
| `NODES` | `id x y` | 节点 ID、X 坐标、Y 坐标；ID 必须是唯一正整数。 |
| `ELEMENTS` | `id node1 node2 E A` | 单元 ID、两端节点 ID、杨氏模量、截面面积；ID 必须唯一，节点必须存在，`E` 和 `A` 必须大于 0。 |
| `LOADS` | `node fx fy` | 受载节点 ID、X/Y 方向集中力；同一节点最多一条荷载记录。 |
| `CONSTRAINTS` | `node fix_x fix_y` | 约束节点 ID、X/Y 方向约束；同一节点最多一条约束记录。 |

`fix_x` 和 `fix_y` 只能是 `0` 或 `1`：

- `0`：该方向自由。
- `1`：该方向固定。

网页和求解器不做单位换算，整个模型必须使用一致单位制。例如使用 N–mm 制时，坐标用 mm、面积用 mm²、`E` 用 N/mm²（MPa）、荷载用 N；不要在同一模型中混用 m、mm、N、kN 等单位。

## 容量限制

| 分区 | 最大记录数 |
|---|---:|
| `NODES` | 10 |
| `ELEMENTS` | 20 |
| `LOADS` | 10 |
| `CONSTRAINTS` | 10 |

达到容量后，相应“新增”按钮会被禁用。导入的文件如果语法可解析但超过容量，记录仍会显示以便修正，但导出保持禁用。

## 导入与导出

- 导入接受空行和以 `#` 开头的整行注释，但四个分区的顺序必须是 `NODES → ELEMENTS → LOADS → CONSTRAINTS`。
- 无法解析的导入不会覆盖页面中已有模型；错误会显示在状态区域。
- 导出前会校验容量、ID、节点引用、数值和约束值。只有校验通过时才能下载。
- 导出始终写出四个分区、统一空行和行尾换行；原文件中的注释及排版不会保留。
- 下载位置和同名文件处理由浏览器决定；网页不会覆盖仓库中的源文件。

## 浏览器限制

当前版本只是通过本地文件打开的静态页面。出于浏览器安全边界，它不能直接启动仓库中的 `fem`/`fem.exe`，也不能替用户创建结果目录或写入 TXT、Markdown、CSV 求解结果。页面中的命令仅是终端操作提示；请复制或按平台调整后，在仓库根目录手动运行。
