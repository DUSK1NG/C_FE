# Stage 9 Results Output Verification

Date: 2026-08-12

Branch: `stage9-results-output`

Task: Stage 9 Task 3 — Docker integration and verification evidence

## Review status

| Task | Final status | Reviewed commits |
| --- | --- | --- |
| Task 1 — output contract | Approved after one fix round | `feeae57` (`test: define stage 9 results output contract`) through `1dc32fe` (`test: strengthen stage 9 output contract`) |
| Task 2 — exporters and Debug printers | Approved after two scoped fix rounds | `daeaa09` (`feat: add stage 9 results exporters`), `a55d2e1` (`test: strengthen stage 9 output coverage`), and `ae774ad` (`test: cover y-only reaction export`) |
| Task 3 — Docker and evidence | Verified by the fresh checks below | This document and the corresponding `Dockerfile` change |

## Docker integration

The existing Demo, Stage 1, Stage 6, Stage 7, and Stage 8 builder checks remain in place. The Stage 9 builder check is appended after Stage 8:

```dockerfile
RUN gcc -std=c11 -Wall -Wextra -pedantic \
        tests/test_stage9.c src/fem.c src/solver.c src/reactions.c \
        src/postprocess.c src/io.c src/output.c \
        -Iinclude -o test_stage9 -lm

RUN ./test_stage9 > stage9.out && \
    ./test_stage9 --emit-debug >> stage9.out && \
    grep -F "Stage 9 results output contract tests passed." stage9.out && \
    grep -F "K_original" stage9.out && \
    grep -F "F_original" stage9.out
```

The explicit `--emit-debug` invocation is required because the normal Stage 9 test process captures its child Debug output into temporary files, validates it, and removes those files; only the pass line reaches the normal process standard output. The runtime stage and `CMD ["./fem"]` are unchanged.

## Local regression verification

Compiler: MSYS2 UCRT64 GCC 16.1.0. The verification shell did not inherit the user PATH, so the successful run prepended `C:\msys64\ucrt64\bin;C:\msys64\usr\bin` and invoked `C:\msys64\ucrt64\bin\gcc.exe` directly. Executables and writable fixtures were placed in a unique temporary directory inside the worktree and removed after verification.

Every compilation used `-std=c11 -Wall -Wextra -pedantic`, `-Iinclude`, and `-lm`.

| Check | Sources in addition to the test | Compile exit | Run exit | Observed output |
| --- | --- | ---: | ---: | --- |
| Stage 1 | `src/fem.c src/solver.c` | 0 | 0 | `Stage 1 tests passed.` |
| Stage 2 | `src/fem.c src/solver.c` | 0 | 0 | `Stage 2 tests passed.` |
| Stage 3 | `src/fem.c src/solver.c` | 0 | 0 | `Stage 3 tests passed.` |
| Stage 4 | `src/fem.c src/solver.c` | 0 | 0 | `Stage 4 tests passed.` |
| Stage 5 | `src/fem.c src/solver.c` | 0 | 0 | `Stage 5 contract tests passed.` |
| Stage 6 | `src/fem.c src/solver.c src/postprocess.c` | 0 | 0 | `Stage 6 element postprocess contract tests passed.` |
| Stage 7 | `src/fem.c src/solver.c src/reactions.c` | 0 | 0 | `Stage 7 contract tests passed.` |
| Stage 8 | `src/fem.c src/solver.c src/reactions.c src/io.c` | 0 | 0 | `Stage 8 input parser contract tests passed.` |
| Stage 9 | `src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c` | 0 | 0 | Windows `/dev/full` skip notice, then `Stage 9 results output contract tests passed.` |
| Demo | `src/main.c src/fem.c src/solver.c` | 0 | 0 | Original Stage 1 Demo shown below |

The Stage 9 local test reports that the real write-failure check is skipped on Windows because there is no portable deterministic equivalent to `/dev/full`. Open-failure coverage still runs locally, and the Linux Docker build runs the `/dev/full` assertions.

## Demo output

Both the local Demo and the runtime container exited 0 and printed:

```text
Stage 1: single 2D truss element
Length = 943.398113205660 mm
c = 0.529998940003
s = 0.847998304005
Element stiffness matrix [N/mm]:
   6252.796483  10004.474373  -6252.796483 -10004.474373
  10004.474373  16007.158997 -10004.474373 -16007.158997
  -6252.796483 -10004.474373   6252.796483  10004.474373
 -10004.474373 -16007.158997  10004.474373  16007.158997
```

## Docker verification

Docker Desktop 4.85.0 was invoked by its full executable path because this shell did not inherit the Docker PATH entry.

| Command | Exit | Evidence |
| --- | ---: | --- |
| `docker build --load -t c-fe-stage9-results-output .` before the Debug invocation correction | 1 | Stage 9 contract passed, but `stage9.out` lacked `K_original` and `F_original`; the first label grep failed. This reproduced the integration defect. |
| `docker build --load -t c-fe-stage9-results-output .` after adding `./test_stage9 --emit-debug >> stage9.out` | 0 | Builder printed the Stage 9 pass line, `K_original (2x2)`, and `F_original (2)`; the runtime image was loaded. |
| `docker run --rm c-fe-stage9-results-output` | 0 | The unchanged runtime `CMD ["./fem"]` printed the original Stage 1 Demo output above. |

## Warning facts

Warnings did not cause any compile or run failure.

- Stage 1 local compilation: six inherited `-Wmissing-field-initializers` warnings for `Node.fx`.
- Stage 2 local compilation: six inherited `-Wmissing-field-initializers` warnings for `Node.fx`.
- Stage 7 local compilation: fifteen inherited `-Wpedantic` warnings about array pointers with different qualifiers before C23.
- Demo local compilation: two inherited `-Wmissing-field-initializers` warnings for `Node.fx`.
- Stage 3, Stage 4, Stage 5, Stage 6, Stage 8, and Stage 9 local compilations emitted no warnings.
- Docker displayed the same inherited Demo, Stage 1, and Stage 7 warning categories. Stage 9 emitted no compiler warning.

## Static and worktree checks

Before creating this verification document:

- `git diff --check`: exit 0, no findings.
- `rg -n "\\b(malloc|calloc|realloc|free)\\s*\\(" src include tests`: exit 1 with no output, meaning no dynamic-allocation call matched in the searched paths.
- `git status --short --branch`: exit 0; branch output was `## stage9-results-output`, with only the Task 3 `Dockerfile` modification after temporary verification directories were removed.

The final staged whitespace check, Task 3 scope check, and post-commit clean status are recorded in the Task 3 report and were run again after this document was complete.

## Environment limitations and concerns

- The shell initially invoked the GCC executable without adding its companion directories to PATH. That diagnostic attempt returned compile exit 1 for every target without compiler diagnostics. The complete verification was rerun with the required MSYS2 directories in PATH; only the successful fresh run is used as pass evidence above.
- Windows skips the `/dev/full` write-failure branch by design. The Linux Docker run executes that branch as part of the Stage 9 contract.
- No production source, public API, test source, Demo behavior, runtime image definition, or runtime `CMD` was changed by Task 3.
