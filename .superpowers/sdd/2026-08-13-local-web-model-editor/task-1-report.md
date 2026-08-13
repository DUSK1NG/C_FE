# Task 1 Report

## Status

Done.

## Commit

- `98557667ce5a243bf2c92351513832eced7adaaf` — `feat: add web model codec contract`

## Tests Run

1. Red test, before implementation:

   `& 'C:\Users\jking1\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe' 'tests\test_web_model.js'`

   Output:

   ```text
   Error: Cannot find module '../web/app.js'
   ```

2. Green test, after implementation:

   `& 'C:\Users\jking1\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe' 'tests\test_web_model.js'`

   Output:

   ```text
   web model tests passed
   ```

3. Auxiliary verification:

   `& 'C:\Users\jking1\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe' -e "const assert = require('node:assert/strict'); const api = require('./web/app.js'); const empty = api.createEmptyModel(); assert.deepEqual(empty, { nodes: [], elements: [], loads: [], constraints: [] }); assert.equal(api.buildCommand('sample.model'), 'fem --input sample.model'); console.log('aux checks passed');"`

   Output:

   ```text
   aux checks passed
   ```

4. Diff hygiene:

   `git diff --check`

   Output:

   ```text
   ```

## Self-Review Notes

- The test-first sequence was followed: the Node test failed before `web/app.js` existed, then passed after the module was added.
- `web/app.js` exports the five required functions and keeps DOM setup behind a `typeof document !== 'undefined'` guard.
- The module is CommonJS-compatible for Node and still exposes an optional browser hook when loaded in a document context.
- The parser accepts the required section order and blank/comment lines, and serialization preserves the same section ordering.

## Concerns

- `serializeModel()` currently emits a trailing newline after the last section; this is usually harmless, but if a later task needs byte-for-byte formatting parity with another serializer, that may need a follow-up.
- The browser initialization is intentionally minimal because this task only required the pure model contract and DOM safety, not the full editor UI.

## Fix Round 1

### Regression Test Added

`tests/test_web_model.js` now asserts safe shell quoting for:

- `my model.model`
- `unsafe;$(echo pwned).model`

### Red Run Before Fix

`& 'C:\Users\jking1\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe' 'tests\test_web_model.js'`

Output:

```text
AssertionError [ERR_ASSERTION]: Expected values to be strictly equal:
+ actual - expected

+ 'fem --input my model.model'
- "fem --input 'my model.model'"
```

### Fix Applied

`web/app.js` now wraps command arguments with shell-safe single-quote escaping and keeps embedded single quotes safe by closing/reopening the quoted segment.

### Green Runs After Fix

`& 'C:\Users\jking1\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe' 'tests\test_web_model.js'`

Output:

```text
web model tests passed
```

`& 'C:\Users\jking1\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe' -e "const { buildCommand } = require('./web/app.js'); console.log(buildCommand('unsafe;`$(echo pwned).model')); console.log(buildCommand('my model.model')); console.log(buildCommand('sample.model'));"`

Output:

```text
fem --input 'unsafe;$(echo pwned).model'
fem --input 'my model.model'
fem --input 'sample.model'
```
