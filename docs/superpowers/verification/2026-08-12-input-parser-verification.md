# Stage 8 Input Parser Verification

Date: 2026-08-12
Task: Stage 8 Task 3 — build integration, regression verification, and record

## Review status

| Task | Status | Evidence |
| --- | --- | --- |
| Task 1 | Approved before Task 3 | Completed implementation is present in the reviewed Stage 8 history. |
| Task 2 | Approved before Task 3 | `9b5ceac fix: harden stage 8 input parser` is the reviewed final parser hardening commit. |
| Task 3 | Verified and recorded | This record covers the Docker integration and the fresh local/container checks below. |

## Docker integration

`Dockerfile` retains the existing demo, Stage 1, Stage 6, Stage 7, and `CMD ["./fem"]` commands. Immediately after `RUN ./test_stage7`, it now compiles and runs Stage 8:

```dockerfile
RUN gcc -std=c11 -Wall -Wextra -pedantic \
        tests/test_stage8.c src/fem.c src/solver.c src/reactions.c src/io.c \
        -Iinclude -o test_stage8 -lm

RUN ./test_stage8
```

## Local verification

Compiler: MSYS2 UCRT64 GCC, with `C:\\msys64\\ucrt64\\bin;C:\\msys64\\usr\\bin` prepended to `PATH`.

All executables were written to a unique directory under `%LOCALAPPDATA%\\Temp`.

| Check | Compile exit | Run exit | Result |
| --- | ---: | ---: | --- |
| Stage 1: `tests/test_stage1.c src/fem.c src/solver.c` | 0 | 0 | `Stage 1 tests passed.` |
| Stage 2: `tests/test_stage2.c src/fem.c src/solver.c` | 0 | 0 | `Stage 2 tests passed.` |
| Stage 3: `tests/test_stage3.c src/fem.c src/solver.c` | 0 | 0 | `Stage 3 tests passed.` |
| Stage 4: `tests/test_stage4.c src/fem.c src/solver.c` | 0 | 0 | `Stage 4 tests passed.` |
| Stage 5: `tests/test_stage5.c src/fem.c src/solver.c` | 0 | 0 | `Stage 5 contract tests passed.` |
| Stage 6: `tests/test_stage6.c src/fem.c src/solver.c src/postprocess.c` | 0 | 0 | `Stage 6 element postprocess contract tests passed.` |
| Stage 7: `tests/test_stage7.c src/fem.c src/solver.c src/reactions.c` | 0 | 0 | `Stage 7 contract tests passed.` |
| Stage 8: `tests/test_stage8.c src/fem.c src/solver.c src/reactions.c src/io.c` | 0 | 0 | `Stage 8 input parser contract tests passed.` |
| Demo: `src/main.c src/fem.c src/solver.c` | 0 | 0 | Printed the Stage 1 truss heading, length `943.398113205660 mm`, direction cosines, and the 4×4 stiffness matrix. |

Each command used `gcc -std=c11 -Wall -Wextra -pedantic`, `-Iinclude`, a temporary executable path, and `-lm`.

Stage 8 required one environment-only rerun. Its first compiled executable exited 1 because the test writes temporary `.model` files into its current directory, while the repository checkout is read-only for this execution identity. The test data file was copied to `tests/data/triangle.model` under the temporary directory and the existing temporary executable was run from that directory. No repository source or test files were changed; the final Stage 8 run exit was 0 as recorded above.

## Warning facts

Warnings did not make any compilation fail.

- Stage 1 compilation: six existing `-Wmissing-field-initializers` warnings for `Node.fx` in `tests/test_stage1.c`.
- Stage 2 compilation: six existing `-Wmissing-field-initializers` warnings for `Node.fx` in `tests/test_stage2.c`.
- Stage 7 compilation: fifteen existing `-Wpedantic` warnings about pointers to arrays with different qualifiers before C23 in `tests/test_stage7.c`.
- Stage 8 compilation: no warnings.
- Demo compilation: two existing `-Wmissing-field-initializers` warnings for `Node.fx` in `src/main.c`.

## Docker verification

| Command | Exit | Observation |
| --- | ---: | --- |
| `docker build -t c-fe-stage8-input-parser .` | 0 | Builder ran Stage 1, 6, 7, and the new Stage 8 command; all emitted their passing messages. The configured `docker-container` driver warned that no output was specified, so the tagged image remained only in build cache. |
| `docker run --rm c-fe-stage8-input-parser` | 125 | Not executed against the newly built image: the driver had not loaded it locally, and Docker reported that the image could not be found locally. This is an environment/driver output-mode limitation, not a Dockerfile test failure. |
| `docker build --load -t c-fe-stage8-input-parser .` | 0 | Explicitly loaded the same Dockerfile build into the local image store. |
| `docker run --rm c-fe-stage8-input-parser` after `--load` | 0 | Ran the unchanged runtime `CMD ["./fem"]` and printed the expected Stage 1 demo output. |

The Docker build also displayed the same existing demo/Stage 1 missing-field and Stage 7 pedantic warnings described above; Stage 8 emitted no compiler warning.

## Static and branch checks

First post-commit check (commit `529efe6`) produced the following actual results:

- `git diff --check 24d3854..HEAD`: exit 2. It reported the pre-existing `docs/superpowers/plans/2026-08-12-input-parser.md:282` blank line at EOF and one trailing space in this new verification document. The Task 3 trailing space was removed before the final amend; the earlier Task 1/2 plan file is outside Task 3 scope and was not modified.
- `rg -n "\\b(malloc|calloc|realloc|free)\\s*\\(" src include tests`: exit 1 with no matches, confirming no dynamic-allocation calls in the searched paths.
- `git status --short --branch`: exit 0, output `## stage8-input-parser` (clean tracked worktree).
- `git diff --name-status 24d3854..HEAD`: exit 0. It lists the expected Stage 8 source/test history plus `M Dockerfile` and `A docs/superpowers/verification/2026-08-12-input-parser-verification.md`.

Final check results after the Task 3 whitespace correction:

- `git diff --check 24d3854..HEAD`: exit 2, with exactly one finding: `docs/superpowers/plans/2026-08-12-input-parser.md:282: new blank line at EOF.` This predates Task 3 and is out of scope.
- `git diff --check HEAD^..HEAD`: exit 0, confirming the Task 3 Dockerfile and verification-record commit itself has no whitespace errors.
- `rg -n "\\b(malloc|calloc|realloc|free)\\s*\\(" src include tests`: exit 1 with no matches.
- `git status --short --branch`: exit 0, output `## stage8-input-parser`; the tracked worktree is clean.
- `git diff --name-status 24d3854..HEAD`: exit 0 and lists the expected Stage 8 history, including only Task 3's `M Dockerfile` and `A docs/superpowers/verification/2026-08-12-input-parser-verification.md` at this task boundary.

## Scope confirmation

Task 3 changes are limited to `Dockerfile` and this tracked verification record. The required detailed Task 3 report is written separately at `.superpowers/sdd/2026-08-12-input-parser/task-3-report.md`; it is not included in the requested commit.
