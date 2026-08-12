# Stage 9 Results Output Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a fixed-capacity TXT/Markdown/CSV results exporter and `DEBUG`-controlled matrix/vector printers without changing the Stage 1–8 solver interfaces or the existing Demo entry point.

**Architecture:** Add an `output` module with a public `FemResults` container and five output functions. The module consumes the existing `FemModel` and `ElementResult` structures, validates only the data it serializes, and writes deterministic human-readable, report-friendly Markdown, or machine-readable output. Debug printers remain side-effect-only console helpers; callers decide whether to invoke them with the existing `DEBUG` macro.

**Tech Stack:** C11, standard C library (`stdio.h`, `math.h`, `string.h`), fixed arrays from `config.h`, existing `FemStatus`, GCC/Docker verification.

## Global Constraints

- Keep `MAX_NODES`, `MAX_ELEMENTS`, and `MAX_DOF` fixed; do not add dynamic allocation.
- Do not change `FemModel`, `ElementResult`, solver, reaction, postprocess, or Stage 1–8 public signatures.
- Keep the existing `main.c` Stage1 Demo behavior unchanged.
- TXT and Markdown numeric fields use `%.12g`; CSV uses the exact header and `record_type` values from the approved design.
- Output functions return an error for null pointers, invalid counts, duplicate/out-of-range constrained DOFs, non-finite serialized values, file-open failures, and write failures.
- User-facing node and element IDs come from `Node.id` and `Element.id`; internal array indices must never be exported as IDs.
- No mock-based output assertions; tests use real files and real output functions.
- Every production change is preceded by a failing contract test and followed by strict C11 verification.

---

### Task 1: Define the Stage9 output contract with failing tests

**Files:**
- Create: `tests/test_stage9.c`

**Interfaces:**
- The test defines the required public API by including `output.h` and calling `write_results_txt`, `write_results_markdown`, `write_results_csv`, `print_debug_matrix`, and `print_debug_vector`.
- The expected result container is `FemResults` with displacement, reactions, element results, constrained DOFs/count, and equilibrium residual fields.

- [ ] **Step 1: Add deterministic fixture helpers and assertions**

Create a two-node, one-element `FemModel` using non-contiguous user IDs `10` and `40`; create a `FemResults` fixture with finite displacements, one constrained DOF pair, one `ElementResult` in `ELEMENT_TENSION`, and finite residuals. Add helpers to read a generated file into a bounded buffer and remove it after each test.

- [ ] **Step 2: Add TXT contract assertions**

Call `write_results_txt("stage9_results.txt", &model, &results)` and assert `FEM_OK`. Read the file and assert it contains `2D Truss FEM Results`, `Nodal Displacements`, the user ID `10`, `Element Results`, `TENSION`, `Support Reactions`, and both residual labels/values.

- [ ] **Step 3: Add CSV contract assertions**

Call `write_results_csv("stage9_results.csv", &model, &results)` and assert `FEM_OK`. Assert the first line exactly equals:

```text
record_type,id,ux,uy,elongation,strain,stress,axial_force,state,rx,ry,residual_fx,residual_fy
```

Assert the file contains `NODE,10`, `ELEMENT,<element-id>`, `REACTION,10`, and `SUMMARY` rows, and does not contain the internal node index as the node ID.

- [ ] **Step 4: Add Markdown contract assertions**

Call `write_results_markdown("stage9_results.md", &model, &results)` and assert `FEM_OK`. Read the file and assert it contains `# 2D Truss FEM Results`, the four fixed section headings, the table headers `| Node ID | ux | uy |` and `| Element ID | Elongation | Strain | Stress | Axial Force | State |`, the user ID `10`, and `TENSION`.

- [ ] **Step 5: Add validation and file-error tests**

For each case, pre-fill output fixtures and assert a non-`FEM_OK` status from TXT, Markdown, and CSV: null path, null model, null results, zero/over-capacity node count, over-capacity element count, negative constrained count, duplicate constrained DOF, out-of-range constrained DOF, non-finite displacement, non-finite element result, and an output path whose parent does not exist. Assert no successful output is claimed.

- [ ] **Step 5: Add Debug call coverage**

Call `print_debug_matrix("K_original", matrix, 2)` and `print_debug_vector("F_original", vector, 2)` with known values. The test exits successfully; its captured stdout is checked in the verification command for both names, dimensions, and values.

- [ ] **Step 6: Run the contract test to verify RED**

Run:

```text
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage9.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c -Iinclude -o test_stage9 -lm
```

Expected: compilation fails because `include/output.h` and the output implementation do not exist yet. Do not add production code in this task.

- [ ] **Step 7: Commit the failing contract**

```text
git add tests/test_stage9.c
git commit -m "test: define stage 9 results output contract"
```

---

### Task 2: Implement TXT/Markdown/CSV export and Debug printers

**Files:**
- Create: `include/output.h`
- Create: `src/output.c`
- Test: `tests/test_stage9.c`

**Interfaces:**
- `output.h` includes `config.h`, `fem.h`, `io.h`, and `postprocess.h` and exposes the approved `FemResults` type and five functions.
- `write_results_txt`, `write_results_markdown`, and `write_results_csv` accept a path, a `FemModel`, and a `FemResults`; successful writes return `FEM_OK`.
- `print_debug_matrix` and `print_debug_vector` accept a label, a fixed-capacity matrix/vector, and a runtime size; invalid parameters produce no output.

- [ ] **Step 1: Implement shared validation helpers**

In `src/output.c`, add internal helpers for finite checks, model/result count checks, constrained-DOF uniqueness/range checks, element-state validation, and `ElementState` to string conversion. Validate only entries that the selected format emits, while validating every node displacement, every element result, every constrained reaction, and both residuals before opening the output file.

- [ ] **Step 2: Implement TXT serialization**

Write the exact section titles and columns from the design. Emit one node row per `model->node_count`, one element row per `model->element_count`, one reaction row per node with at least one constrained direction, and one equilibrium summary. Map displacement and reaction DOFs as `2 * node_index` and `2 * node_index + 1`; use user IDs from the model.

- [ ] **Step 3: Implement CSV serialization**

Write the exact stable header and one wide row for each `NODE`, `ELEMENT`, `REACTION`, and `SUMMARY` record. Leave unused fields empty, preserve the fixed field order, use user IDs, and check every `fprintf`/`fputs` result. Close the file on every return path after opening it.

- [ ] **Step 4: Implement Markdown serialization**

Write the exact fixed headings and table headers from the design. Emit one row per model node, one row per model element, one row per node with at least one constrained direction, and one equilibrium row. Use Markdown-safe numeric/state strings and user IDs; check every write and close the file on every return path.

- [ ] **Step 5: Implement Debug printers**

For valid `size` values, print a label, dimensions, and `%.12g` values. Reject null labels/data and sizes outside `1..MAX_DOF` without output. Do not put `#if DEBUG` inside these functions; the caller controls invocation, so the functions remain directly testable.

- [ ] **Step 6: Run Stage9 tests to verify GREEN**

Run the strict compile command from Task 1, then execute from a writable temporary directory so the file-based tests can create and remove their fixtures. Expected output includes the Stage9 pass line and the Debug labels/values; exit code is 0.

- [ ] **Step 7: Run static checks and commit**

Run:

```text
git diff --check
rg -n "\b(malloc|calloc|realloc|free)\s*\(" src include tests
```

Expected: no whitespace findings and no dynamic-allocation matches. Commit:

```text
git add include/output.h src/output.c tests/test_stage9.c
git commit -m "feat: add stage 9 results exporters"
```

---

### Task 3: Integrate Stage9 into Docker verification and record evidence

**Files:**
- Modify: `Dockerfile`
- Create: `docs/superpowers/verification/2026-08-12-results-output-verification.md`

**Interfaces:**
- Docker retains the existing Demo, Stage1, Stage6, and Stage7 checks and adds a strict Stage9 compile/run after Stage8.
- The verification record names exact commands, exit codes, expected output, warnings, static checks, and branch/worktree state.

- [ ] **Step 1: Extend Docker builder checks**

After the existing Stage8 compile/run, add:

```dockerfile
RUN gcc -std=c11 -Wall -Wextra -pedantic \
        tests/test_stage9.c src/fem.c src/solver.c src/reactions.c \
        src/postprocess.c src/io.c src/output.c \
        -Iinclude -o test_stage9 -lm

RUN ./test_stage9 > stage9.out && \
    grep -F "Stage 9 results output contract tests passed." stage9.out && \
    grep -F "K_original" stage9.out && \
    grep -F "F_original" stage9.out
```

Keep the runtime image and `CMD ["./fem"]` unchanged.

- [ ] **Step 2: Run all local regression checks**

In a writable temporary directory, compile and run `tests/test_stage1.c` through `tests/test_stage9.c` with their existing source lists, then compile and run the Demo. Confirm all exit codes are 0 and the Stage1 Demo output remains unchanged.

- [ ] **Step 3: Build and run Docker**

Run:

```text
docker build --load -t c-fe-stage9-results-output .
docker run --rm c-fe-stage9-results-output
```

Expected: build exit 0, Stage9 contract output during the builder phase, and the unchanged Stage1 Demo from the runtime image.

- [ ] **Step 4: Write verification evidence**

Record the Stage1–9 compile/run results, Demo result, Docker result, warning facts, `git diff --check`, dynamic-allocation search, and clean `git status --short --branch`. Distinguish inherited warnings from new Stage9 warnings.

- [ ] **Step 5: Commit integration evidence**

```text
git add Dockerfile docs/superpowers/verification/2026-08-12-results-output-verification.md
git commit -m "test: verify stage 9 results output"
```

---

## Final review checklist

- [ ] Stage9 TXT, Markdown, and CSV formats match the approved design exactly.
- [ ] Non-contiguous user IDs are preserved in all exported records.
- [ ] Invalid result data and file failures return errors and never report success.
- [ ] Debug output is separate from formal files and contains matrix/vector labels and values.
- [ ] Existing Stage1–8 tests and Demo remain unchanged in behavior.
- [ ] No dynamic allocation or unrelated refactoring was introduced.
- [ ] Docker build/run and static checks have fresh exit-code evidence.
