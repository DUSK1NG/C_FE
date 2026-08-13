# Final Whole-Branch Fix Report

## Scope and starting point

- Worktree: C:\Users\jking1\Desktop\my-project\c_FE\.worktrees\unified-input-output
- Branch: agent/unified-input-output
- Starting HEAD: 886285526c1ef79c1b736a0de77a57974aea326b
- Fix brief: .superpowers/sdd/2026-08-13-unified-input-output/final-fix-brief.md
- Plan reviewed in full: docs/superpowers/plans/2026-08-13-unified-input-output.md
- Review package reviewed in full: .superpowers/sdd/2026-08-13-unified-input-output/review-0820b16..8862855.diff
- Constraints retained: C11, standard library only, fixed capacities, no dynamic allocation, no new dependency, no public API changes, and existing Stage behavior/output contracts preserved.

The worktree was clean at the requested head before the fix:

    git branch --show-current
    agent/unified-input-output

    git rev-parse HEAD
    886285526c1ef79c1b736a0de77a57974aea326b

    git status --short --branch
    ## agent/unified-input-output

## Root-cause verification

### 1. Pre-existing output deletion

The original src/main.c stored paths in a compact array but did not track ownership. On a writer failure it incremented created_count for the failed target and passed every populated path to remove_output_files(). Earlier writers also opened targets with wb, so a pre-existing file could be truncated and then deleted during rollback.

Pre-fix black-box reproduction:

- Copied tests/data/medium.model to a pre-existing protected.txt file.
- Created protected.md as a directory to force the later Markdown writer to fail.
- Ran the unified executable with --format txt,markdown.
- Observed diagnostic:

    Output error: .../protected.md: invalid model input

- protected.txt no longer existed after the command, confirming the data-loss finding.

### 2. Duplicate scalar options

The original parser had no occurrence state. Value options overwrote previous values, --demo could be repeated, and --help returned immediately, so even --help --help was accepted.

Pre-fix reproduction:

    fem --input tests/data/triangle.model --input tests/data/medium.model \
        --output-dir <temporary-directory> --prefix duplicate_input_probe \
        --format txt

Observed result: exit 0 and output generation, rather than the required CLI error.

The new CLI regression was compiled and run before production changes:

    gcc -std=c11 -Wall -Wextra -pedantic tests/test_cli.c src/cli.c \
        -Iinclude -o test_cli_red -lm
    test_cli_red

Observed RED result:

    Assertion failed: cli_parse_args(...) == 2, file tests/test_cli.c, line 105

### 3. Markdown regression gap

tests/test_output_selection.c already distinguished selected and legacy layouts for TXT and CSV, but not Markdown. The production Markdown split was already correct, so the added coverage passed immediately and acts as a regression guard rather than requiring a production change.

### 4. Generic output diagnostics

Output writers return FEM_INPUT_ERROR for open/write/close failures. src/main.c passed that status to fem_status_message(), whose input-oriented text is invalid model input. The exit code was correct, but the explanation was for the wrong subsystem.

## Implementation

### Safe output ownership and rollback

File: src/main.c

- Output handling now has three phases:
  1. Construct every selected path in fixed TXT, Markdown, CSV order.
  2. Reserve every selected path with C11 exclusive-create mode fopen(path, "wbx").
  3. Invoke the existing selected writer APIs only after all reservations succeed.
- A fixed int created[3] array records only paths successfully created by this invocation.
- Rollback removes only entries whose ownership flag is set.
- If any selected target already exists, exclusive creation fails before any writer can overwrite it.
- If a later reservation fails, earlier invocation-owned reservations are removed while the pre-existing later target remains untouched.
- Existing writer APIs and file formats were not changed.

### Output-specific diagnostics

File: src/main.c

- Path construction failures identify the selected extension.
- Reservation failures identify the exact path and standard-library errno reason, such as File exists or No such file or directory.
- Writer failures use output-specific messages:
  - invalid output request
  - unable to create or write output file
  - output writer failed
- Output failures continue to return code 5.

### Duplicate option validation

File: src/cli.c

- Added a fixed seven-bit occurrence mask for:
  - --input
  - --output-dir
  - --prefix
  - --format
  - --include
  - --demo
  - --help
- Every second occurrence returns code 2 with a non-empty message naming the duplicate option.
- --help now continues scanning the argument vector so duplicate and unknown trailing options are validated; after successful validation it still bypasses the normal input requirement.
- Existing duplicate entries inside --format and --include lists remain rejected.

### Regression coverage

File: tests/test_cli.c

- Added duplicate-occurrence coverage for all seven options.
- The required minimum value options --input, --output-dir, and --prefix are explicitly covered.
- Existing unknown-option, duplicate-list-entry, empty-entry, missing-value, and demo/input conflict checks remain.

File: tests/test_output_selection.c

- Added selected-all Markdown assertions for the extended node metadata and summary-count headers.
- Added legacy Markdown assertions for the original compact node and residual-only summary headers.
- The TXT and CSV selected-versus-legacy assertions remain unchanged.

### User documentation

Files: README.md and docs/project-report.md

- Documented that each option can appear at most once.
- Documented that selected output targets must not already exist and are never overwritten or deleted by failure cleanup.
- Updated CLI help with the same constraints.

## Focused verification

### CLI

    gcc -std=c11 -Wall -Wextra -pedantic tests/test_cli.c src/cli.c \
        -Iinclude -o test_cli -lm
    test_cli

Observed:

    CLI parser tests passed.

The test covers all seven duplicate options and verifies code 2 plus a non-empty error for invalid forms.

### Output selection

    gcc -std=c11 -Wall -Wextra -pedantic tests/test_output_selection.c \
        src/pipeline.c src/fem.c src/solver.c src/reactions.c \
        src/postprocess.c src/io.c src/output.c -Iinclude \
        -o test_output_selection -lm
    test_output_selection

Observed:

    Selected output section tests passed.

This includes selected-versus-legacy checks for TXT, Markdown, and CSV.

### Unified executable

    gcc -std=c11 -Wall -Wextra -pedantic src/main.c src/cli.c \
        src/pipeline.c src/fem.c src/solver.c src/reactions.c \
        src/postprocess.c src/io.c src/output.c -Iinclude -o fem -lm

Observed: successful compilation with no warnings.

## Complete verification

Every target was compiled with:

    -std=c11 -Wall -Wextra -pedantic -Iinclude -lm

The complete matrix was compiled and run:

| Target | Production sources beyond test source | Result |
|---|---|---|
| Stage 1 | src/fem.c src/solver.c | Stage 1 tests passed. |
| Stage 2 | src/fem.c src/solver.c | Stage 2 tests passed. |
| Stage 3 | src/fem.c src/solver.c | Stage 3 tests passed. |
| Stage 4 | src/fem.c src/solver.c | Stage 4 tests passed. |
| Stage 5 | src/fem.c src/solver.c | Stage 5 contract tests passed. |
| Stage 6 | src/fem.c src/solver.c src/postprocess.c | Stage 6 element postprocess contract tests passed. |
| Stage 7 | src/fem.c src/solver.c src/reactions.c | Stage 7 contract tests passed. |
| Stage 8 | src/fem.c src/solver.c src/reactions.c src/io.c | Stage 8 input parser contract tests passed. |
| Stage 9 | src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c | Stage 9 results output contract tests passed. |
| Stage 10 | src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c | Stage 10 project organization contract tests passed. |
| Pipeline | src/pipeline.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c | Unified pipeline tests passed. |
| Output selection | src/pipeline.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c | Selected output section tests passed. |
| CLI | src/cli.c | CLI parser tests passed. |

Compilation result: 14 of 14 targets built successfully with no warnings (13 test executables plus the unified fem executable).

Execution result: 13 of 13 test executables exited 0.

Existing platform note:

    Stage 9 write-failure test skipped:
    Windows has no portable deterministic full-device equivalent.

The rest of Stage 9 passed. No new skip was added.

## Black-box verification

### Help and Demo compatibility

    fem --help
    fem --demo

Observed: both exited 0; help included the new no-duplicates/no-overwrite notice, and Demo retained the Stage 1 output.

### Successful selected output

    fem --input tests/data/medium.model \
        --output-dir <temporary-directory>/custom \
        --prefix unified_medium \
        --format txt,markdown \
        --include nodes,reactions,summary

Observed:

- Exit code 0.
- Exactly unified_medium.txt and unified_medium.md were created.
- No CSV file was created.
- TXT and Markdown contained nodes, reactions, and summary.
- Neither file contained the element section.

### Duplicate scalar option

    fem --input tests/data/triangle.model \
        --input tests/data/medium.model

Observed:

    exit_code=2
    重复选项：--input

### Pre-existing earlier target plus forced later-format failure

Setup:

- protected.txt was copied from tests/data/medium.model.
- Its SHA-256 was D84F271D6BA0C3E144580CB5CD8DE1D987B6B04A9984747EB298D979A3E1C7EB.
- protected.md was created as a directory, which is an invalid later Markdown file target.

Command:

    fem --input tests/data/medium.model \
        --output-dir <temporary-directory>/protected \
        --prefix protected --format txt,markdown

Observed:

    exit_code=5
    Output error: cannot create .../protected.txt: File exists

Postconditions:

- protected.txt still existed.
- Its SHA-256 remained D84F271D6BA0C3E144580CB5CD8DE1D987B6B04A9984747EB298D979A3E1C7EB.
- protected.md remained a directory.
- No pre-existing target was overwritten or deleted.

### Later pre-existing target and owned rollback

Setup:

- owned.txt did not exist.
- owned.md was copied from tests/data/medium.model.
- owned.md SHA-256 was D84F271D6BA0C3E144580CB5CD8DE1D987B6B04A9984747EB298D979A3E1C7EB.

Command:

    fem --input tests/data/medium.model \
        --output-dir <temporary-directory>/later-existing \
        --prefix owned --format txt,markdown

Observed:

    exit_code=5
    Output error: cannot create .../owned.md: File exists

Postconditions:

- The newly reserved owned.txt path was rolled back.
- The pre-existing owned.md file remained.
- owned.md retained the same SHA-256.

This verifies that cleanup is based on current-invocation ownership rather than path position.

### Missing output directory diagnostic

    fem --input tests/data/medium.model \
        --output-dir <temporary-directory>/missing \
        --prefix failure --format txt

Observed:

    exit_code=5
    Output error: cannot create .../failure.txt: No such file or directory

The diagnostic is output-specific and no longer reports invalid model input.

## Cleanup

All temporary executables, generated TXT/Markdown/CSV files, copied probe files, blocking directories, and verification directories were removed after the checks.

Post-cleanup scans found:

- No .exe files in the worktree.
- No stage*, selected_*, legacy_*, unified_*, fem_results*, cleanup_probe*, duplicate_input_probe*, or .out artifacts.
- No baseline-artifacts, verification-artifacts, or final-verification-artifacts directory.

## Files changed

- src/main.c
- src/cli.c
- tests/test_cli.c
- tests/test_output_selection.c
- README.md
- docs/project-report.md
- .superpowers/sdd/2026-08-13-unified-input-output/final-fix-report.md

No public header or Stage API changed. No dependency, Docker file, dynamic allocation, or capacity change was introduced.

## Deferred findings and concerns

- No required finding was deferred.
- The existing Windows Stage 9 /dev/full-equivalent test remains skipped exactly as before; deterministic writer-open failures, return code 5, diagnostics, pre-existing-file preservation, and rollback ownership were covered through Windows black-box probes instead.
