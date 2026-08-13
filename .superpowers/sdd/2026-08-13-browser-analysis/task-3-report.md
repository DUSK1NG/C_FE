# Task 3 实现报告：浏览器分析 SVG 图形

## 实现摘要

在分析面板中新增两个原生 SVG 图形：

- 节点与杆件变形图：显示浅色虚线的原始杆件和醒目蓝色的变形后杆件；依据有限模型边界构造 `viewBox`，并依据最大位移缩放变形。零位移时仍保留完整模型。
- 单元轴力图：每个单元对应一根以零基线为起点的柱条；`bar--tension`、`bar--compression` 和 `bar--zero` 分别表示拉力、压力和零轴力。

两个渲染器均导出给 Node 测试使用：

- `renderDeformationSvg(model, results)`
- `renderAxialForceSvg(results)`

空结果会返回含 `svg-empty-state` 的可读 SVG。所有动态图形数值经过有限性保护，ID、状态和文本经 SVG/HTML 转义；极大但有限的数值不会在格式化时溢出为 `Infinity`。分析成功时渲染图形，清除分析时同步清空图形容器。

## 修改文件

- `web/app.js`：新增两个 SVG 渲染器、安全数值格式化以及分析面板渲染/清除接入。
- `web/index.html`：新增变形图和轴力图容器。
- `web/styles.css`：新增响应式图形布局、原始/变形线、轴力柱和空状态样式。
- `tests/test_web_model.js`：新增有效结果、空结果、零位移、NaN/Infinity、防注入、极端有限数值及浏览器清除状态测试。

## TDD 记录

首次新增图形测试后执行：

```text
C:\Users\jking1\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe tests\test_web_model.js
```

实际输出（预期失败）：

```text
TypeError: renderDeformationSvg is not a function
```

随后为极大有限轴力补充回归测试；修复前实际输出（预期失败）：

```text
AssertionError: expected SVG not to match /(?:NaN|Infinity)/
... <title>单元 1: Infinity (tension)</title> ...
```

## 最终验证

```text
C:\Users\jking1\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe --check web\app.js
```

实际输出：无输出，退出码 0。

```text
C:\Users\jking1\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe tests\test_web_model.js
```

实际输出：

```text
web model tests passed
```

```text
git diff --check
```

实际输出：无差异错误，退出码 0。

## 提交

网页实现提交：`0e0f2bc016f751e35aa757abe9a8c710408e90f6` (`feat: add browser analysis svg charts`)。

## Concerns

无已知功能问题。验证覆盖 Node 渲染与假 DOM 工作流；未启动真实浏览器进行人工视觉检查。

## 审查修复：偏移坐标变形图

审查发现原始变形图先按坐标绝对值归一化，并对跨度使用固定 `0.01` 下限。对于坐标整体偏移很大、但实际跨度较小的模型，这会将几何压缩到极小区域，且原点图例不在 `viewBox` 内。

修复内容：

- 以节点原始坐标的 `minX` / `minY` 平移所有图形坐标到局部原点，再由真实的 `maxX - minX` / `maxY - minY` 计算跨度和 `viewBox`。
- 移除固定 `0.01` 跨度下限；不可用的非正或非有限跨度返回安全空状态。
- 图例的位置和尺寸按计算后的跨度派生，确保其位于带 padding 的 `viewBox` 内。
- 增加 `1_000_000` 坐标偏移、实际 `100` 跨度模型的回归测试，断言有限 `viewBox`、局部有效几何、图例和无 `NaN` / `Infinity`。

TDD 红灯命令：

```text
C:\Users\jking1\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe tests\test_web_model.js
```

实际失败输出：

```text
AssertionError: The input did not match /viewBox="-14 -14 128 128"/
... viewBox="0.9985 0.9985 0.0128 0.0128" ...
```

最终验证命令和输出：

```text
node --check web\app.js                 # 退出码 0，无输出
node tests\test_web_model.js            # web model tests passed
git diff --check                         # 退出码 0，无差异错误
```

修复实现提交：`920e5c7149c9e803470b6e3ae84b037418e32033` (`fix: preserve deformation chart bounds for offset models`)。
