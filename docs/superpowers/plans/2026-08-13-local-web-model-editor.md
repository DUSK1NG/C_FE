# 本地网页模型配置器 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建一个无需服务器和第三方依赖的本地网页，用于编辑、导入、校验和导出二维桁架有限元 `.model` 文件。

**Architecture:** `web/index.html` 提供页面结构，`web/styles.css` 提供响应式样式，`web/app.js` 同时包含可测试的模型解析/校验/序列化函数和浏览器 UI 控制。浏览器通过 `Blob` 下载 `.model` 文件，不调用后端，也不修改现有 C11 求解器。

**Tech Stack:** 原生 HTML5、CSS3、现代 JavaScript；Node.js 内置 `assert` 用于无依赖逻辑测试；现有 C11 `fem` CLI 作为后续求解入口。

## Global Constraints

- 不引入 npm、框架、CDN、服务器或第三方运行时依赖。
- 页面必须直接双击 `web/index.html` 使用。
- 输入格式固定为 `NODES → ELEMENTS → LOADS → CONSTRAINTS`。
- 最多 10 个节点、20 个单元、10 条荷载和 10 条约束。
- 节点和单元 ID 必须是正整数且在对应区段内唯一。
- 单元 `E`、`A` 必须大于零；约束值只能是 `0` 或 `1`。
- 导入失败不得覆盖当前有效模型；导出前必须通过校验。
- 不修改 `src/`、`include/`、现有测试和命令行行为。

---

### Task 1: 建立模型逻辑测试和 JavaScript 模块边界

**Files:**
- Create: `tests/test_web_model.js`
- Create: `web/app.js`

**Interfaces:**
- Produces `createEmptyModel()`、`parseModel(text)`、`validateModel(model)`、`serializeModel(model)` 和 `buildCommand(fileName)`。
- Each parser result uses `{ok: true, model}` or `{ok: false, error}`.
- Each validation result uses `{valid: boolean, errors: string[]}`.
- Browser APIs are initialized only when `document` exists; Node can load the pure functions without a DOM.

- [ ] **Step 1: Write failing Node tests for the model contract**

```javascript
const assert = require('node:assert/strict');
const { parseModel, validateModel, serializeModel } = require('../web/app.js');

const source = `NODES 2
1 0 0
2 1000 0

ELEMENTS 1
1 1 2 210000 100

LOADS 1
2 0 -1000

CONSTRAINTS 2
1 1 1
2 0 1
`;

const parsed = parseModel(source);
assert.equal(parsed.ok, true);
assert.equal(parsed.model.nodes.length, 2);
assert.equal(validateModel(parsed.model).valid, true);
assert.match(serializeModel(parsed.model), /NODES 2/);
assert.match(serializeModel(parsed.model), /CONSTRAINTS 2/);
console.log('web model tests passed');
```

- [ ] **Step 2: Run the test and confirm it fails because `web/app.js` is missing**

Run: `node tests/test_web_model.js`

Expected: FAIL with a module/function-not-found error.

- [ ] **Step 3: Add the minimal CommonJS/browser-compatible module shell**

Implement the five named functions and export them with:

```javascript
if (typeof module !== 'undefined' && module.exports) {
  module.exports = { createEmptyModel, parseModel, validateModel,
    serializeModel, buildCommand };
}
```

Guard all DOM setup with `if (typeof document !== 'undefined')`.

- [ ] **Step 4: Run the focused test and confirm the basic contract passes**

Run: `node tests/test_web_model.js`

Expected: `web model tests passed`.

- [ ] **Step 5: Commit the model logic contract**

```powershell
git add tests/test_web_model.js web/app.js
git commit -m "feat: add web model codec contract"
```

### Task 2: Implement strict parsing, validation, and serialization

**Files:**
- Modify: `web/app.js`
- Modify: `tests/test_web_model.js`

**Interfaces:** Uses the five functions from Task 1; all UI tasks consume the same model object shape:

```javascript
{
  nodes: [{id, x, y}],
  elements: [{id, node1, node2, E, A}],
  loads: [{node, fx, fy}],
  constraints: [{node, fix_x, fix_y}]
}
```

- [ ] **Step 1: Extend tests for valid samples and comments**

Add tests that parse `tests/data/triangle.model` and a string containing blank lines and full-line `#` comments. Assert the four counts and that serializing then parsing again preserves all values.

- [ ] **Step 2: Extend tests for every invalid input class**

Cover missing sections, malformed counts, duplicate node IDs, duplicate element IDs, unknown element node references, duplicate load/constraint node records, non-positive `E`/`A`, invalid constraint values, zero-length elements, and counts above the configured limits. Assert `ok === false` or `valid === false` and a non-empty error message.

- [ ] **Step 3: Implement the parser**

Parse the four sections in order, ignore blank/full-comment lines, require exact token counts, convert numeric tokens with finite-number checks, and return a failed result without throwing. Preserve the current model when the caller chooses not to apply a failed result.

- [ ] **Step 4: Implement validation and deterministic serialization**

Validate IDs, references, limits and numeric rules. Serialize all four headers even when `LOADS 0` or `CONSTRAINTS 0`; use one record per line and a trailing newline. Generate a command using the selected filename:

```text
fem --input custom.model --output-dir results --prefix custom
```

- [ ] **Step 5: Run the focused tests and commit**

Run: `node tests/test_web_model.js`

Expected: all parser, validator and round-trip assertions pass.

```powershell
git add web/app.js tests/test_web_model.js
git commit -m "feat: validate and serialize web models"
```

### Task 3: Build the page shell and responsive editor layout

**Files:**
- Create: `web/index.html`
- Create: `web/styles.css`
- Modify: `web/app.js`

**Interfaces:** The page loads `styles.css` and `app.js`; `app.js` binds only to IDs present in `index.html` and keeps the model state in memory.

- [ ] **Step 1: Add the HTML structure**

Create a Chinese page with a header, action toolbar, four sections (`nodes`, `elements`, `loads`, `constraints`), error/status area, serialized preview `<pre>`, command preview `<code>`, hidden `.model` file input, and row templates implemented by JavaScript.

- [ ] **Step 2: Add responsive styling**

Use a centered page container, card sections, horizontally scrollable tables, visible focus states, compact numeric inputs, and red/green status styles. At viewport widths below 760px, stack toolbar controls and keep tables scrollable instead of shrinking columns below readable widths.

- [ ] **Step 3: Add a DOM smoke test**

Run `node --check web/app.js` and `node --check tests/test_web_model.js`. Confirm `web/index.html` references `./styles.css` and `./app.js` and contains all four section IDs.

- [ ] **Step 4: Open the page directly and verify the shell**

Open `web/index.html` in a browser. Confirm the page renders without a console error, all four sections are visible, and the narrow-window layout remains usable.

- [ ] **Step 5: Commit the page shell**

```powershell
git add web/index.html web/styles.css web/app.js
git commit -m "feat: add local model editor page shell"
```

### Task 4: Add table editing, examples, import, export, and previews

**Files:**
- Modify: `web/index.html`
- Modify: `web/app.js`
- Modify: `web/styles.css`

- [ ] **Step 1: Add row rendering and edit events**

Render each model array into its table. Each section gets “新增” and row-level “删除” controls. Use event delegation for `input`, `change`, `click`, and `change` on the file input so edits update the in-memory model and rerender only the affected section.

- [ ] **Step 2: Add the sample model action**

Add a built-in triangle model constant matching `tests/data/triangle.model`. The “加载示例” action parses it through `parseModel` and replaces the current state only after successful validation.

- [ ] **Step 3: Add `.model` import**

Read the selected file with `File.text()`, call `parseModel`, validate the result, and replace the current state only when both succeed. On failure, show the error and leave existing rows unchanged.

- [ ] **Step 4: Add preview and download**

After every state change, update the preview with `serializeModel(model)`. Disable “导出” while validation fails. On export, create a `Blob` with MIME type `text/plain;charset=utf-8`, use the requested filename or `custom.model`, click a temporary anchor, and revoke the object URL.

- [ ] **Step 5: Add command preview**

Update the command preview whenever the filename changes. Escape the displayed filename as plain text and never inject it as HTML.

- [ ] **Step 6: Verify the complete browser workflow**

In a browser: load example → edit a coordinate → add/delete a row → import `tests/data/triangle.model` → export `custom.model` → open the downloaded file in a text editor. Confirm section order and values match the UI.

- [ ] **Step 7: Commit the editor workflow**

```powershell
git add web/index.html web/styles.css web/app.js
git commit -m "feat: add model editing and file workflow"
```

### Task 5: Add validation feedback and capacity guards

**Files:**
- Modify: `web/app.js`
- Modify: `web/index.html`
- Modify: `web/styles.css`
- Modify: `tests/test_web_model.js`

- [ ] **Step 1: Add assertions for row-level validation messages**

Test duplicate IDs, unknown node references, invalid numeric values, invalid constraints, and over-capacity models. Assert error messages identify the affected section or field and that invalid models cannot be exported.

- [ ] **Step 2: Implement centralized validation rendering**

Call `validateModel` after every edit. Render errors in the status area, add an invalid class to affected rows when available, update counts such as `3/10`, and disable export while errors exist.

- [ ] **Step 3: Implement capacity guards**

Disable or reject “新增” when a section reaches its limit and show the exact limit. Keep imported over-capacity data visible as an error state but do not allow export.

- [ ] **Step 4: Verify recovery behavior**

Create an invalid duplicate ID, confirm export is disabled, fix the ID, confirm the error clears and export becomes enabled. Import an invalid file and confirm the prior valid model remains unchanged.

- [ ] **Step 5: Run all web tests and commit**

Run: `node tests/test_web_model.js`

Expected: parser, validation, capacity and recovery tests pass.

```powershell
git add web/index.html web/styles.css web/app.js tests/test_web_model.js
git commit -m "feat: add web validation feedback"
```

### Task 6: Documentation and final verification

**Files:**
- Modify: `README.md`
- Create: `web/README.md`

- [ ] **Step 1: Document opening the page**

Add Windows and Linux/macOS instructions for opening `web/index.html`, importing or exporting a model, and running the generated `fem` command. State clearly that the first version does not execute the C program directly from the browser.

- [ ] **Step 2: Document supported fields and limits**

List the four sections, `0/1` constraint semantics, consistent units, capacity limits, and import/export behavior.

- [ ] **Step 3: Run static and logic checks**

```powershell
node --check web/app.js
node tests/test_web_model.js
git diff --check
```

Expected: syntax check, web model tests and whitespace check all pass.

- [ ] **Step 4: Run the existing C regression suite**

Re-run the existing Stage 1–10, pipeline, output-selection and CLI tests using the compiler command already documented in `README.md`. Confirm the web feature does not alter any C result.

- [ ] **Step 5: Perform the final manual acceptance check**

Open the page directly, load `triangle.model` and `medium.model`, export each to a temporary directory, compare the exported structure with the imported preview, and verify that the generated command produces the expected TXT/Markdown/CSV files when run from the repository root.

- [ ] **Step 6: Commit documentation and hand off**

```powershell
git add README.md web/README.md
git commit -m "docs: document local web model editor"
```

Report the final files, test commands, known limitation that the browser does not execute `fem.exe`, and whether the user wants the completed changes pushed to GitHub.
