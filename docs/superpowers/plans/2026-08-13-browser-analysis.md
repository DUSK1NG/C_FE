# 浏览器端有限元分析 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在本地静态网页中实现浏览器端有限元分析，使内置示例和用户上传的 `.model` 文件能够直接得到表格、变形图和轴力图结果。

**Architecture:** 在 `web/app.js` 中新增独立的 `analyzeModel(model)` 求解边界，复用现有解析和校验逻辑；网页状态保存最新分析结果，结果渲染拆分为表格、SVG 变形图和 SVG 柱状图。所有分析在浏览器中使用固定上限的 JavaScript 数组完成，不引入后端、WebAssembly 或第三方图表库。

**Tech Stack:** 原生 HTML5、CSS3、SVG、现代 JavaScript、Node.js 内置 `assert` 测试。

## Global Constraints

- 保持 `.model` 文件格式、现有编辑/导入/导出行为和 C11 命令行行为不变。
- 只允许分析通过 `validateModel(model)` 的模型；分析过程不得修改输入模型对象。
- 支持当前固定容量：最多 10 个节点、20 个单元、10 个荷载、10 个约束、20 个自由度。
- 结果表格使用文本节点或 HTML 转义；模型字段不得成为未转义的 HTML。
- 不添加服务器、Docker、npm 依赖、WebAssembly 或第三方图表库。
- 每个任务先写失败测试，再实现最小代码，并在任务结束时运行相关测试。

---

### Task 1: 建立浏览器求解器的可测试契约

**Files:**
- Modify: `tests/test_web_model.js`
- Modify: `web/app.js`

**Interfaces:**
- Produces `analyzeModel(model)` returning either `{ ok: true, results }` or `{ ok: false, error }`.
- `results.nodeDisplacements` records `{ node, ux, uy, magnitude }`.
- `results.elementResults` records `{ element, length, elongation, strain, stress, axialForce, status }`.
- `results.reactions` records `{ node, fx, fy }`.
- `results.summary` records `{ totalLoadX, totalLoadY, totalReactionX, totalReactionY, residualX, residualY, maxDisplacement }`.
- Exports `analyzeModel` from `web/app.js` for Node tests.

- [ ] **Step 1: Write failing solver contract tests**

Add tests using the existing `source` two-node vertical-load model and `triangleSource` fixture:

```js
const { analyzeModel } = require('../web/app.js');

const parsed = parseModel(source);
assert.equal(parsed.ok, true);
const analyzed = analyzeModel(parsed.model);
assert.equal(analyzed.ok, true);
assert.equal(analyzed.results.nodeDisplacements.length, 2);
assert.equal(analyzed.results.elementResults.length, 1);
assert.equal(analyzed.results.reactions.length, 1);
assert.ok(Number.isFinite(analyzed.results.nodeDisplacements[1].uy));
assert.ok(Number.isFinite(analyzed.results.elementResults[0].stress));
assert.ok(Math.abs(analyzed.results.summary.residualY) < 1e-7);
```

Add failure assertions for an invalid model, a zero-length element, and a mechanism with insufficient constraints. Assert `ok === false`, a non-empty error string, and no partial `results` object.

- [ ] **Step 2: Run the focused test and verify the expected failure**

Run from the worktree:

```text
node tests/test_web_model.js
```

Expected: the test fails because `analyzeModel` is not exported or does not yet return the required result contract. Do not change production code before observing this failure.

- [ ] **Step 3: Implement model preparation and fixed-capacity linear algebra**

Add small internal helpers in `web/app.js`:

```js
function createZeroMatrix(size) {}
function solveLinearSystem(matrix, vector) {}
function mapNodeDofs(model) {}
```

Use a dense `2 * model.nodes.length` matrix. Build a node-ID map, calculate each element’s length and direction cosines, assemble the 4×4 truss stiffness matrix into the global matrix, and sum loads by node ID. Apply constraints by partitioning free DOFs and solving only the free system. Return a clear error when the free system is empty, singular, non-finite, or has a zero-length element.

- [ ] **Step 4: Implement result recovery and post-processing**

Complete `analyzeModel(model)` so it:

1. Calls `validateModel(model)` and converts validation details into one error message.
2. Copies model records before calculation.
3. Computes displacements and displacement magnitudes.
4. Computes element length, elongation, strain, stress, axial force and `status` (`tension`, `compression`, or `zero`).
5. Computes reactions from `K * displacement - load` at constrained DOFs.
6. Computes load/reaction totals and residuals.
7. Verifies every returned number is finite before returning success.

- [ ] **Step 5: Run the focused solver tests and commit**

Run:

```text
node --check web/app.js
node tests/test_web_model.js
```

Expected: the new solver assertions and all existing web tests pass.

Commit:

```text
git add web/app.js tests/test_web_model.js
git commit -m "feat: add browser finite element solver"
```

### Task 2: Add analysis result tables and state lifecycle

**Files:**
- Modify: `web/index.html`
- Modify: `web/app.js`
- Modify: `web/styles.css`
- Modify: `tests/test_web_model.js`

**Interfaces:**
- Adds `state.analysis`, initially `null`.
- Adds `renderAnalysisTables(dom, results)` and `clearAnalysis(dom, state)`.
- Adds `analysis-panel`, `analysis-status`, `node-results-rows`, `element-results-rows`, `reaction-results-rows`, and `summary-results` DOM targets.

- [ ] **Step 1: Write failing DOM tests for result panel and lifecycle**

Extend the fake DOM setup with the analysis button and result targets. Assert that initialization creates no stale result rows, and that after calling the analysis action on the sample model the panel becomes visible and contains node, element, reaction, and summary text. Add a test that editing a model field clears `state.analysis` and hides the old results.

- [ ] **Step 2: Run the DOM tests and verify they fail**

Run:

```text
node tests/test_web_model.js
```

Expected: the new DOM assertions fail because the analysis targets and handlers do not exist.

- [ ] **Step 3: Add semantic HTML result containers**

In `web/index.html`, add an `analysis-panel` after the editor grid. Include:

```html
<section id="analysis-panel" class="analysis-panel analysis-panel--hidden" aria-labelledby="analysis-title">
  <div class="analysis-header">
    <div>
      <p class="section-kicker">Browser analysis</p>
      <h2 id="analysis-title">分析结果</h2>
    </div>
    <p id="analysis-status" class="status" aria-live="polite"></p>
  </div>
  <div class="results-grid">
    <section><h3>节点位移</h3><table><tbody id="node-results-rows"></tbody></table></section>
    <section><h3>单元结果</h3><table><tbody id="element-results-rows"></tbody></table></section>
    <section><h3>支座反力</h3><table><tbody id="reaction-results-rows"></tbody></table></section>
    <section><h3>平衡汇总</h3><dl id="summary-results"></dl></section>
  </div>
</section>
```

Add an `id="analyze-model-button"` button beside the existing model actions.

- [ ] **Step 4: Render escaped result rows and connect lifecycle**

Add DOM lookup and rendering functions. Format numbers with a single shared formatter, escape all string values, and set text content for status messages. On `newModel`, successful import, or any field/add/delete edit, call `clearAnalysis`. On analysis success, store a deep-copied result and render all four tables. On failure, clear old rows and display the error.

- [ ] **Step 5: Add styles and run tests**

Add responsive result-panel styles, scrollable result tables, hidden state, and narrow-screen stacking. Run:

```text
node --check web/app.js
node tests/test_web_model.js
```

Commit:

```text
git add web/index.html web/app.js web/styles.css tests/test_web_model.js
git commit -m "feat: show browser analysis tables"
```

### Task 3: Add SVG deformation and axial-force charts

**Files:**
- Modify: `web/index.html`
- Modify: `web/app.js`
- Modify: `web/styles.css`
- Modify: `tests/test_web_model.js`

**Interfaces:**
- Adds `renderDeformationSvg(model, results)` returning an SVG string.
- Adds `renderAxialForceSvg(results)` returning an SVG string.
- Adds `deformation-plot` and `axial-force-plot` containers.

- [ ] **Step 1: Write failing chart tests**

Add tests that call both renderer functions with a valid analyzed sample and assert:

```js
assert.match(renderDeformationSvg(model, results), /<svg/);
assert.match(renderDeformationSvg(model, results), /data-node/);
assert.match(renderAxialForceSvg(results), /<svg/);
assert.match(renderAxialForceSvg(results), /data-element/);
```

Also test an empty result set and a zero-displacement result; both must return a valid empty-state SVG rather than throw or produce `NaN` coordinates.

- [ ] **Step 2: Run tests and verify the expected failure**

Run `node tests/test_web_model.js` and confirm the renderer functions are missing or fail the new assertions.

- [ ] **Step 3: Implement safe SVG renderers**

Use a fixed `viewBox` and calculate model bounds. Draw original elements in a muted stroke and deformed elements in a highlighted stroke. Select a deformation scale from the model span and maximum displacement, with a finite fallback when the maximum displacement is zero. Draw axes/legend text as fixed labels and use `escapeHtml` for IDs or statuses. The axial-force chart maps each element to a bar around a zero baseline, with separate classes for tension, compression, and zero.

- [ ] **Step 4: Mount charts after table rendering**

Insert the SVG strings with `innerHTML` only after all dynamic values have passed through the SVG escaping helper. Keep the chart containers inside the analysis panel and clear them with the rest of the analysis state.

- [ ] **Step 5: Run tests and commit**

Run `node --check web/app.js` and `node tests/test_web_model.js`; then commit:

```text
git add web/index.html web/app.js web/styles.css tests/test_web_model.js
git commit -m "feat: add analysis charts"
```

### Task 4: Connect example/upload analysis workflow and documentation

**Files:**
- Modify: `web/app.js`
- Modify: `web/README.md`
- Modify: `README.md`
- Modify: `tests/test_web_model.js`

**Interfaces:**
- The existing “加载示例” and `.model` upload actions must leave the model in the editor and make it analyzable with one click.
- The new “开始分析” action must use the current editor state, not reread a different file.

- [ ] **Step 1: Write failing workflow tests**

Add fake-file tests for: loading `triangle.model` through the existing file input, clicking the analysis button, observing a visible successful result, changing a load and observing a changed result, and uploading malformed text while preserving the last valid model. Assert the status text explains that analysis is browser-side.

- [ ] **Step 2: Run the tests and confirm failure**

Run `node tests/test_web_model.js`; the new analysis button and status assertions must fail before implementation.

- [ ] **Step 3: Connect event handlers and disabled states**

Register the analysis button in `initBrowserApp`. Disable it when validation is invalid or the model is blank. On click, clear old results, call `analyzeModel`, render success or error state, and keep the current editor model intact. Ensure import failures do not replace the current model and always reset the file input.

- [ ] **Step 4: Update Chinese documentation**

Update `web/README.md` with the three-step browser workflow, supported result tables/charts, unit consistency note, and the distinction between browser-side analysis and C11 CLI analysis. Update the root README web-editor paragraph to state that examples and uploaded models can be analyzed in the browser.

- [ ] **Step 5: Run workflow tests and commit**

Run:

```text
node --check web/app.js
node tests/test_web_model.js
git diff --check
```

Commit:

```text
git add web/app.js web/README.md README.md tests/test_web_model.js
git commit -m "feat: connect browser model analysis workflow"
```

### Task 5: Full verification and handoff

**Files:**
- Modify: `.superpowers/sdd/2026-08-13-browser-analysis/progress.md`
- Create: `.superpowers/sdd/2026-08-13-browser-analysis/task-5-report.md`

- [ ] **Step 1: Run the complete web verification**

Run:

```text
node --check web/app.js
node --check tests/test_web_model.js
node tests/test_web_model.js
git diff --check
```

- [ ] **Step 2: Run all existing C regression targets**

Recompile and run Stage 1–10, `pipeline`, `output-selection`, and `cli` using the source lists already documented in the previous local-web-model-editor Task 6 report. Record the result count and the existing Windows-only Stage 9 skip.

- [ ] **Step 3: Perform static browser acceptance checks**

Verify `web/index.html` references the stylesheet and script, contains the analysis button and result containers, and has no third-party chart dependency. Verify the sample analysis path through the fake DOM and model round-trip tests.

- [ ] **Step 4: Write the final report and ledger**

Record changed files, test commands, pass counts, browser `file://` limitation, and any deferred minor coverage in `task-5-report.md`. Mark Tasks 1–5 complete in `progress.md`.

- [ ] **Step 5: Commit the verification report**

```text
git add .superpowers/sdd/2026-08-13-browser-analysis
git commit -m "docs: verify browser analysis feature"
```
