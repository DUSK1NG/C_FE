## Task 5 report

Date: 2026-08-13
Workspace: `C:\Users\jking1\Desktop\my-project\c_FE\.worktrees\local-web-model-editor`

### Scope

Implemented Task 5 in:

- `web/app.js`
- `web/index.html`
- `web/styles.css`
- `tests/test_web_model.js`

No C code or plan documents were modified.

### TDD evidence

Red:

- Added regression coverage for:
  - section/field-specific validation messages
  - invalid row styling
  - section count/limit display
  - add-button disable/reject behavior at 10/20/10/10
  - over-capacity import visibility with export blocked
  - duplicate ID recovery restoring export
  - initial no-error UI not showing a red error state
- First failing run:

```text
AssertionError [ERR_ASSERTION]: The input did not match the regular expression /CONSTRAINTS.*fix_[xy]/i.
Input:

'Constraints contains invalid values
Constraint fix values must be 0 or 1'
```

Green:

- Follow-up implementation made the new tests pass.

### Verification

Bundled Node syntax check:

```text
& 'C:\Users\jking1\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe' --check 'C:\Users\jking1\Desktop\my-project\c_FE\.worktrees\local-web-model-editor\web\app.js'
Exit code: 0
```

Bundled Node full test run:

```text
& 'C:\Users\jking1\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe' 'C:\Users\jking1\Desktop\my-project\c_FE\.worktrees\local-web-model-editor\tests\test_web_model.js'
web model tests passed
```

### What changed

- Reworked model validation to emit section/field-targeted messages and structured row-level issue metadata.
- Marked invalid rows in the editor and kept row re-rendering limited to validation-state changes so normal field editing still preserves Task 4 behavior.
- Added per-section `count/limit` UI and limit-based add-button disabling/rejection.
- Allowed parse-valid imports to remain visible even when over capacity, while keeping export disabled until fixed.
- Preserved prior valid model on parse-invalid import.
- Hid the red error panel when there is no current error message to show.

### Self-review notes

- Re-ran the required bundled Node syntax check and the full web test file after the final code changes.
- Confirmed the modified file set matches the Task 5 brief.

### Commit

- `05ae7c9` — `feat: add web validation feedback`

### Concerns

- None. A later UTF-8-aware review confirmed that the checked-in HTML/JavaScript copy is valid Chinese; the earlier mojibake concern was caused by reading UTF-8 files with the wrong console encoding, not by the repository contents.

## Fix round 1

### Scope

- Updated only `web/app.js` and `tests/test_web_model.js`.
- Preserved the existing fake-DOM test enhancements and did not modify C code or Task 6 files.

### Root cause and fix

- `updateStatusPanels` called `renderAllSections` whenever the invalid-row signature changed.
- That replaced every table body and therefore detached the currently focused input when its row became invalid or valid again.
- Replaced that redraw with an in-place row-class update, leaving the existing input nodes, focus, and selection untouched.
- Removed the unreachable legacy validation/status block after the unconditional return and the unreachable import-validation block after its return.

### TDD evidence

Red:

```text
AssertionError [ERR_ASSERTION]: focus should stay on the live input when the row becomes invalid
actual: null
```

Green:

```text
web model tests passed
```

### Verification

```text
node.exe --check web/app.js
Exit code: 0

node.exe tests/test_web_model.js
web model tests passed
```
