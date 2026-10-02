# 命令行使用

命令行程序读取 `.model` 文件，求解后把所选结果格式写入已有目录。模型分区和字段格式见[网页编辑器说明](../web/README.md)。程序不会换算单位；一个模型内的坐标、面积、弹性模量和荷载必须使用一致单位制。

## 选项

| 选项 | 默认值 | 作用 |
| --- | --- | --- |
| `--input PATH` | 必填，除非使用 `--demo` | 输入 `.model` 文件。 |
| `--output-dir DIR` | `.` | 输出目录；目录必须已经存在。 |
| `--prefix NAME` | `fem_results` | 输出文件名的前缀。 |
| `--format LIST` | `txt,markdown,csv` | 选择一种或多种输出格式，逗号分隔。 |
| `--include LIST` | `nodes,elements,reactions,summary` | 选择报告内容，逗号分隔。 |
| `--demo` | — | 运行内置 Stage 1 演示；不能与 `--input` 同用。 |
| `--help` | — | 显示选项说明。 |

可用格式为 `txt`、`markdown`、`csv`；可选报告内容为 `nodes`、`elements`、`reactions`、`summary`。每个选项最多出现一次。

## 输出文件

程序按所选格式写出 `<前缀>.txt`、`<前缀>.md` 或 `<前缀>.csv`。默认前缀和格式会生成 `fem_results.txt`、`fem_results.md`、`fem_results.csv`。已有同名输出文件不会被覆盖，程序会报错退出。
