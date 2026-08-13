# Task 5 verification report

Date: 2026-08-13

## Scope and baseline

- Worktree: `C:/Users/jking1/Desktop/my-project/c_FE/.worktrees/browser-analysis`
- Branch: `agent/browser-analysis`
- Verified baseline: `82215171c149386f52c0341d78fa3815caffb857` (`8221517`)
- Initial worktree status: clean.
- Task 5 changes: this report was added and `progress.md` was updated. No feature source, web asset, test, or C source was changed.

## Web checks

Used Codex-provided Node at `C:/Users/jking1/AppData/Local/OpenAI/Codex/runtimes/cua_node/f1bf3cd3a5929acd/bin/node.exe`.

| Command | Result |
| --- | --- |
| `node --check web/app.js` | pass (exit 0) |
| `node --check tests/test_web_model.js` | pass (exit 0) |
| `node tests/test_web_model.js` | pass (exit 0): `web model tests passed` |

The complete web-model suite covers successful and failed analysis, result tables, lifecycle clearing/recomputation, import/upload, example loading, deformation and axial-force SVGs, offset displacement coordinates, tiny spans, and HTML/PowerShell quoting safety.

## C11 regression

Compiler: `C:/msys64/ucrt64/bin/gcc.exe` with `-std=c11 -Iinclude -lm`. Each executable was built inside a unique system temporary directory. Tests ran there with a temporary junction to the repository `tests` fixture directory; the temporary directory was removed after the run.

| Target | Compile | Run result |
| --- | --- | --- |
| stage1 | pass | `Stage 1 tests passed.` |
| stage2 | pass | `Stage 2 tests passed.` |
| stage3 | pass | `Stage 3 tests passed.` |
| stage4 | pass | `Stage 4 tests passed.` |
| stage5 | pass | `Stage 5 contract tests passed.` |
| stage6 | pass | `Stage 6 element postprocess contract tests passed.` |
| stage7 | pass | `Stage 7 contract tests passed.` |
| stage8 | pass | `Stage 8 input parser contract tests passed.` |
| stage9 | pass | Results-output contract passed; the known Windows full-device write-failure case was skipped because no portable deterministic equivalent exists. |
| stage10 | pass | `Stage 10 project organization contract tests passed.` |
| pipeline | pass | `Unified pipeline tests passed.` |
| output-selection | pass | `Selected output section tests passed.` |
| cli | pass | `CLI parser tests passed.` |

Result: 13/13 targets compiled and ran successfully. Temporary files were cleaned; no test artifact was left in the repository.

## Static acceptance

- `web/index.html` references `./styles.css` and `./app.js`.
- The page includes the analysis button, node/element/reaction result tables, summary container, deformation SVG host, and axial-force SVG host.
- `web/app.js` contains no third-party chart-library reference and no server, `fem.exe`, fetch, XMLHttpRequest, WebSocket, child-process, or spawn API. The only URL matches are the W3C SVG XML namespace attributes.
- `README.md` and `web/README.md` document the browser-local analysis and C11 CLI boundary; `web/README.md` also documents `file://` opening.
- `git diff --check` passed before the Task 5 documentation update.

## Browser limitation and deferred minor

No live visual-browser acceptance was claimed. The deliverable is documented as a static local `file://` page; browser policy and local environment differences can still affect opening behavior. No deferred minor issue was identified by this verification.

## Final-review concentrated fix pass

This section records the later final-review fixes and supersedes the initial statement above that Task 5 changed documentation only. The concentrated pass changed only `web/app.js`, `web/styles.css`, `tests/test_web_model.js`, and this report; no C11 source or unrelated documentation was changed.

### Fixes

- The reduced stiffness system is now symmetrically scaled from its diagonal before elimination. Pivot tolerance is calculated from the scaled coefficient matrix only, excluding the load vector, and the scaled solution is transformed back to physical displacements. This keeps an independent stiffness-`1` subsystem solvable alongside a stiffness-`1e20` subsystem while preserving singular/mechanism detection.
- An otherwise valid model with no free degrees of freedom now returns `{ ok: false, error }` and no `results` payload.
- Deformation coordinates now use `((coordinate - minimum) / span) * displaySpan`, so a span near `1e-320` never requires an overflowing display-scale factor. Displacements are normalized through their largest finite component before display scaling, keeping generated geometry finite. Deformation-legend font size now tracks `displaySpan`; the fixed CSS size was removed so both tiny and ordinary viewBoxes remain readable.

### TDD RED evidence

Each regression was added before production code and run against the old implementation with the Codex-provided Node executable.

1. Mixed stiffness (`1e20` and `1`) failed with exit 1:

   ```text
   AssertionError [ERR_ASSERTION]: Model stiffness matrix is singular or the structure is a mechanism
   false !== true
   at Object.<anonymous> (.../tests/test_web_model.js:476:8)
   ```

2. Fully constrained valid model failed with exit 1:

   ```text
   AssertionError [ERR_ASSERTION]: Expected values to be strictly equal:
   true !== false
   at Object.<anonymous> (.../tests/test_web_model.js:460:8)
   ```

3. Extreme deformation span failed with exit 1:

   ```text
   AssertionError [ERR_ASSERTION]: distinct extreme-span nodes should remain visually distinct
   actual: [ 0, 0 ]
   expected: [ 0, 0 ]
   at Object.<anonymous> (.../tests/test_web_model.js:475:8)
   ```

### GREEN and final verification output

Codex Node path:
`C:/Users/jking1/AppData/Local/OpenAI/Codex/runtimes/cua_node/f1bf3cd3a5929acd/bin/node.exe`

```text
> "C:/Users/jking1/AppData/Local/OpenAI/Codex/runtimes/cua_node/f1bf3cd3a5929acd/bin/node.exe" --check web/app.js
exit 0
(no stdout)

> "C:/Users/jking1/AppData/Local/OpenAI/Codex/runtimes/cua_node/f1bf3cd3a5929acd/bin/node.exe" --check tests/test_web_model.js
exit 0
(no stdout)

> "C:/Users/jking1/AppData/Local/OpenAI/Codex/runtimes/cua_node/f1bf3cd3a5929acd/bin/node.exe" tests/test_web_model.js
exit 0
web model tests passed

> git diff --check
exit 0
warning: in the working copy of 'tests/test_web_model.js', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'web/app.js', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'web/styles.css', LF will be replaced by CRLF the next time Git touches it
```

The line-ending messages are informational warnings from the repository's Windows Git configuration; `git diff --check` reported no whitespace errors.
