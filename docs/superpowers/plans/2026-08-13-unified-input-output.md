# Unified Input Output Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add one C11 command-line workflow that reads a `.model` file, executes the complete Stage 1–10 analysis chain, and writes user-selected TXT, Markdown, and CSV results.

**Architecture:** Keep the existing Stage APIs and tests as the numerical foundation. Add a `pipeline` module that copies the parsed model, calls the existing geometry/assembly/solver/post-processing/reaction APIs in order, and fills the existing `FemResults` structure. Add output-selection APIs and a small CLI parser; `main.c` only coordinates parsing, pipeline execution, path construction, and selected writers.

**Tech Stack:** C11, standard C library only, existing fixed-capacity arrays, MSYS2 GCC verification on Windows, existing Stage 1–10 contract tests.

## Global Constraints

- Keep `MAX_NODES=10`, `MAX_ELEMENTS=20`, and `MAX_DOF=2*MAX_NODES`; do not add dynamic allocation.
- Do not add Docker files, third-party parsers, JSON/YAML dependencies, GUI code, or server code.
- Preserve all existing Stage 1–10 public APIs, tests, model-file syntax, output format names, and `--demo` behavior.
- `--input` is required unless `--demo` or `--help` is used.
- `--output-dir` must already exist; the program must not create directories.
- Use return code `0` for success, `2` for CLI errors, `3` for input errors, `4` for analysis errors, and `5` for output errors.
- Compile new and existing tests with `-std=c11 -Wall -Wextra -pedantic`.

## File Map

- Create `include/pipeline.h`: public complete-analysis function declaration.
- Create `src/pipeline.c`: one orchestration function; no new numerical algorithm.
- Create `include/cli.h`: fixed-capacity command-line option types and parser declarations.
- Create `src/cli.c`: parsing, duplicate/unknown option validation, and help text.
- Modify `include/output.h`: section bit flags, `FemOutputOptions`, and selected-writer declarations.
- Modify `src/output.c`: section-aware TXT/Markdown/CSV writers and compatibility wrappers.
- Modify `src/main.c`: CLI entry point, `--demo`, pipeline invocation, output path construction, and process return codes.
- Create `tests/test_pipeline.c`: complete-analysis API contract test for medium and large fixtures.
- Create `tests/test_output_selection.c`: selected section/format behavior and legacy wrapper regression test.
- Create `tests/test_cli.c`: parser defaults, valid custom lists, and invalid argument tests.
- Modify `README.md`: document the unified command and custom output options in Chinese.
- Modify `docs/project-report.md`: describe the unified non-Docker execution path.

---

### Task 1: Add the failing unified-pipeline contract

**Files:**
- Create: `tests/test_pipeline.c`
- Create: `include/pipeline.h`

**Interfaces:**
- Consumes: existing `FemModel`, `FemResults`, and `read_model_file()` types.
- Produces: the exact `run_fem_analysis(const FemModel *model, FemResults *results)` signature used by later tasks.

- [ ] **Step 1: Write the failing test**

Create a test that reads both fixtures and calls the planned function:

```c
#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "io.h"
#include "output.h"
#include "pipeline.h"

static void run_case(const char *path, int nodes, int elements)
{
    FemModel model;
    FemResults results = {0};
    int i;

    assert(read_model_file(path, &model) == FEM_OK);
    assert(model.node_count == nodes);
    assert(model.element_count == elements);
    assert(run_fem_analysis(&model, &results) == FEM_OK);
    assert(results.constrained_count >= 0);
    assert(isfinite(results.residual_fx));
    assert(isfinite(results.residual_fy));
    for (i = 0; i < model.element_count; ++i) {
        assert(isfinite(results.element_results[i].axial_force));
    }
}

int main(void)
{
    run_case("tests/data/medium.model", 6, 8);
    run_case("tests/data/large.model", 10, 20);
    puts("Unified pipeline tests passed.");
    return 0;
}
```

Create `include/pipeline.h` with the declaration used by this test:

```c
#ifndef PIPELINE_H
#define PIPELINE_H

#include "io.h"
#include "output.h"

FemStatus run_fem_analysis(const FemModel *model, FemResults *results);

#endif
```

- [ ] **Step 2: Run the test to verify it fails**

Run from the repository root:

```text
gcc -std=c11 -Wall -Wextra -pedantic tests/test_pipeline.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c -Iinclude -o test_pipeline -lm
```

Expected: compilation fails because `run_fem_analysis()` has not been implemented yet.

- [ ] **Step 3: Commit the failing contract**

```text
git add include/pipeline.h tests/test_pipeline.c
git commit -m "test: define unified analysis pipeline contract"
```

### Task 2: Implement the complete analysis pipeline

**Files:**
- Modify: `include/pipeline.h`
- Create: `src/pipeline.c`
- Test: `tests/test_pipeline.c`

**Interfaces:**
- Consumes: `read_model_file()` output and existing Stage 1–7 functions.
- Produces: `run_fem_analysis(const FemModel *model, FemResults *results)` that fills displacement, reactions, element results, constrained DOFs, and equilibrium residuals.

- [ ] **Step 1: Implement the minimal pipeline**

Use a local `FemModel working_model = *model` so the assembly step can calculate and store element geometry without changing the caller's `const` input. The function must:

1. Validate `model` and `results` and clear `*results`.
2. Reject invalid node/element counts using existing `FemStatus` values.
3. Call `assemble_global_stiffness()` on `working_model`.
4. Call `build_force_vector()` and `identify_dofs()`.
5. Call `solve_constrained_system()`.
6. Call `calculate_element_result()` for every working element.
7. Call `calculate_support_reactions()` and `check_global_equilibrium()`.
8. Copy displacement, reactions, element results, constrained DOFs, count, and residuals into `FemResults`.
9. Return immediately on the first non-`FEM_OK` status after leaving result arrays in a cleared state.

The implementation must include `fem.h`, `io.h`, `output.h`, `postprocess.h`, and `reactions.h`; it must not duplicate formulas from those modules.

- [ ] **Step 2: Run the focused test**

```text
gcc -std=c11 -Wall -Wextra -pedantic tests/test_pipeline.c src/pipeline.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c -Iinclude -o test_pipeline -lm
./test_pipeline
```

Expected: both fixtures pass and the test prints `Unified pipeline tests passed.`.

- [ ] **Step 3: Add failure-path assertions**

Extend `tests/test_pipeline.c` with null model/results and an oversized-count case. Assert that the result structure is cleared and the returned status is `FEM_INVALID_ARGUMENT` or `FEM_CAPACITY_EXCEEDED` as appropriate.

- [ ] **Step 4: Re-run and commit**

```text
./test_pipeline
git diff --check
git add include/pipeline.h src/pipeline.c tests/test_pipeline.c
git commit -m "feat: add unified FEM analysis pipeline"
```

### Task 3: Add selectable output sections and formats

**Files:**
- Modify: `include/output.h`
- Modify: `src/output.c`
- Create: `tests/test_output_selection.c`

**Interfaces:**
- Consumes: `FemModel`, `FemResults`, and `run_fem_analysis()`.
- Produces: `FemOutputOptions`, section masks, and selected writers with these exact signatures:

```c
typedef enum {
    FEM_OUTPUT_NODES = 1u << 0,
    FEM_OUTPUT_ELEMENTS = 1u << 1,
    FEM_OUTPUT_REACTIONS = 1u << 2,
    FEM_OUTPUT_SUMMARY = 1u << 3
} FemOutputSection;

typedef struct {
    unsigned sections;
} FemOutputOptions;

typedef enum {
    FEM_FORMAT_TXT = 1u << 0,
    FEM_FORMAT_MARKDOWN = 1u << 1,
    FEM_FORMAT_CSV = 1u << 2
} FemOutputFormat;

FemStatus write_results_txt_selected(const char *path,
                                     const FemModel *model,
                                     const FemResults *results,
                                     const FemOutputOptions *options);
FemStatus write_results_markdown_selected(const char *path,
                                          const FemModel *model,
                                          const FemResults *results,
                                          const FemOutputOptions *options);
FemStatus write_results_csv_selected(const char *path,
                                     const FemModel *model,
                                     const FemResults *results,
                                     const FemOutputOptions *options);
```

- [ ] **Step 1: Write the failing selection test**

Read `tests/data/medium.model`, call `run_fem_analysis()`, set `options.sections` to `FEM_OUTPUT_NODES | FEM_OUTPUT_SUMMARY`, and write all three selected files. Assert that each file contains the node and summary markers but does not contain the element and reaction section markers. Also call legacy `write_results_txt()` and assert that its output still contains all sections.

- [ ] **Step 2: Run the test to verify it fails**

```text
gcc -std=c11 -Wall -Wextra -pedantic tests/test_output_selection.c src/pipeline.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c -Iinclude -o test_output_selection -lm
```

Expected: compilation fails because the selected writer interfaces do not exist.

- [ ] **Step 3: Implement selected writers**

Refactor each existing writer into a section-aware implementation. Validate only data required by selected sections, reject `options == NULL`, zero masks, unknown mask bits, and invalid model/results. Use these section meanings consistently:

- `FEM_OUTPUT_NODES`: nodal IDs, coordinates, loads, constraints, and displacement table; retain existing displacement headers and add fields without removing existing data.
- `FEM_OUTPUT_ELEMENTS`: element result table.
- `FEM_OUTPUT_REACTIONS`: support reaction table.
- `FEM_OUTPUT_SUMMARY`: model counts and equilibrium residuals.

Use stable section markers in tests: TXT headings `Nodal Displacements`, `Element Results`, `Support Reactions`, and `Equilibrium`; Markdown headings with the same names; CSV row prefixes `NODE,`, `ELEMENT,`, `REACTION,`, and `SUMMARY,`.

Keep `write_results_txt()`, `write_results_markdown()`, and `write_results_csv()` as wrappers that pass all four section bits, so existing Stage 9 and Stage 10 tests remain valid.

- [ ] **Step 4: Run focused and legacy output tests**

```text
./test_output_selection
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage9.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c -Iinclude -o test_stage9 -lm
./test_stage9
```

Expected: selected files contain only requested sections; the legacy Stage 9 contract passes.

- [ ] **Step 5: Commit**

```text
git diff --check
git add include/output.h src/output.c tests/test_output_selection.c
git commit -m "feat: support custom result sections"
```

### Task 4: Implement and test command-line parsing

**Files:**
- Create: `include/cli.h`
- Create: `src/cli.c`
- Create: `tests/test_cli.c`

**Interfaces:**
- Consumes: `argc`, `argv`, output section/format constants.
- Produces: `CliOptions`, `cli_parse_args()`, and `cli_print_help()` for `main.c`.

`include/cli.h` must include `<stddef.h>`, `<stdio.h>`, and `output.h` so the `size_t`, `FILE`, and format/section bit types used by the interface are defined.

Use fixed fields and pointers into `argv`:

```c
typedef struct {
    const char *input_path;
    const char *output_dir;
    const char *prefix;
    unsigned formats;
    unsigned sections;
    int demo;
    int help;
} CliOptions;

int cli_parse_args(int argc, char *argv[], CliOptions *options,
                   char error_message[], size_t error_size);
void cli_print_help(FILE *stream);
```

`formats` uses `FEM_FORMAT_TXT`, `FEM_FORMAT_MARKDOWN`, and `FEM_FORMAT_CSV` bits. Defaults are `output_dir="."`, `prefix="fem_results"`, all formats, and all sections. Reject missing option values, unknown options, duplicate list entries, empty list entries, unknown format/section names, `--input` without a path, and simultaneous `--demo` plus `--input`.

- [ ] **Step 1: Write parser tests**

Cover default values, a valid custom command, `--help`, missing input, unknown format, unknown section, duplicate `txt`, and an option missing its value. Assert nonzero parser status for invalid cases and that `error_message` is non-empty.

- [ ] **Step 2: Run parser tests to verify they fail**

```text
gcc -std=c11 -Wall -Wextra -pedantic tests/test_cli.c src/cli.c -Iinclude -o test_cli -lm
```

Expected: compilation fails until the parser types and functions are implemented.

- [ ] **Step 3: Implement parser and help text**

Parse options left-to-right, store paths as `argv` pointers, initialize defaults before parsing, and return `2` for all invalid CLI forms. Keep help text in Chinese and include the complete command examples from the design.

- [ ] **Step 4: Run and commit**

```text
./test_cli
git diff --check
git add include/cli.h src/cli.c tests/test_cli.c
git commit -m "feat: add unified CLI option parser"
```

### Task 5: Integrate the CLI with analysis and output

**Files:**
- Modify: `src/main.c`
- Test: `tests/test_cli.c`, `tests/test_pipeline.c`, `tests/test_output_selection.c`

**Interfaces:**
- Consumes: `cli_parse_args()`, `run_fem_analysis()`, selected writers, and existing Stage 1 Demo helper.
- Produces: one executable that supports `--input`, `--output-dir`, `--prefix`, `--format`, `--include`, `--demo`, and `--help`.

- [ ] **Step 1: Preserve the Demo path**

Move the current Stage 1 matrix-printing logic into a static `run_demo()` function and make `main()` call it only when `options.demo` is true.

- [ ] **Step 2: Add input-to-results flow**

For normal execution, call `read_model_file(options.input_path, &model)`, then `run_fem_analysis(&model, &results)`. Map failures to return code `3` or `4` and print the failed stage plus `fem_status_message(status)` to `stderr`.

- [ ] **Step 3: Add safe output path construction**

Build `<output-dir>/<prefix>.<extension>` with `snprintf` into a fixed buffer. Treat truncation, empty directory, and empty prefix as output errors. Use `.txt`, `.md`, and `.csv` extensions in that fixed order.

- [ ] **Step 4: Write only selected formats**

Call the selected writer corresponding to each format bit. If a writer fails, print the exact path, remove any files already created in this invocation, and return `5`. Do not create files for unselected formats.

- [ ] **Step 5: Compile and run black-box CLI checks**

```text
gcc -std=c11 -Wall -Wextra -pedantic src/main.c src/cli.c src/pipeline.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c -Iinclude -o fem -lm
./fem --help
./fem --demo
./fem --input tests/data/medium.model --output-dir . --prefix unified_medium --format txt,markdown --include nodes,reactions,summary
```

Expected: help and Demo exit `0`; the custom command exits `0`, creates only `unified_medium.txt` and `unified_medium.md`, and neither file contains the element section. Remove those generated files after the check.

- [ ] **Step 6: Commit**

```text
git diff --check
git add src/main.c
git commit -m "feat: add unified FEM command workflow"
```

### Task 6: Update documentation and perform full verification

**Files:**
- Modify: `README.md`
- Modify: `docs/project-report.md`

**Interfaces:**
- Consumes: final CLI syntax and output section names.
- Produces: Chinese user documentation that matches the executable exactly.

- [ ] **Step 1: Document the unified command**

Add the default and custom commands, parameter table, supported formats, section meanings, return codes, and the requirement that `--output-dir` already exists. Remove any stale statement that Stage 10 requires separate test-program orchestration.

- [ ] **Step 2: Run all Stage tests**

Run from the repository root after compiling each target into a temporary verification directory:

```text
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage1.c src/fem.c src/solver.c -Iinclude -o verify_stage1 -lm; ./verify_stage1
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage2.c src/fem.c src/solver.c -Iinclude -o verify_stage2 -lm; ./verify_stage2
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage3.c src/fem.c src/solver.c -Iinclude -o verify_stage3 -lm; ./verify_stage3
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage4.c src/fem.c src/solver.c -Iinclude -o verify_stage4 -lm; ./verify_stage4
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage5.c src/fem.c src/solver.c -Iinclude -o verify_stage5 -lm; ./verify_stage5
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage6.c src/fem.c src/solver.c src/postprocess.c -Iinclude -o verify_stage6 -lm; ./verify_stage6
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage7.c src/fem.c src/solver.c src/reactions.c -Iinclude -o verify_stage7 -lm; ./verify_stage7
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage8.c src/fem.c src/solver.c src/reactions.c src/io.c -Iinclude -o verify_stage8 -lm; ./verify_stage8
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage9.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c -Iinclude -o verify_stage9 -lm; ./verify_stage9
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage10.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c -Iinclude -o verify_stage10 -lm; ./verify_stage10
gcc -std=c11 -Wall -Wextra -pedantic tests/test_pipeline.c src/pipeline.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c -Iinclude -o verify_pipeline -lm; ./verify_pipeline
gcc -std=c11 -Wall -Wextra -pedantic tests/test_output_selection.c src/pipeline.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c -Iinclude -o verify_output_selection -lm; ./verify_output_selection
gcc -std=c11 -Wall -Wextra -pedantic tests/test_cli.c src/cli.c -Iinclude -o verify_cli -lm; ./verify_cli
```

Every program must exit `0`; Stage 9's Windows full-device write-failure skip remains an expected environment-specific message. Remove the temporary executables after verification.

- [ ] **Step 3: Run final CLI checks and inspect generated files**

Run the default all-format command and the custom command from Task 5, inspect TXT/Markdown/CSV section markers, remove generated files, and verify no generated artifacts remain.

- [ ] **Step 4: Run repository checks**

```text
git diff --check
git status --short --branch
```

Verify only intended source, test, and documentation files changed and the branch remains `main`.

- [ ] **Step 5: Commit documentation and verification**

```text
git add README.md docs/project-report.md
git commit -m "docs: document unified FEM input and output"
```

## Self-Review Checklist

- The pipeline task covers every Stage 1–7 API and uses Stage 8 input plus Stage 9 output.
- The output task defines stable section bits and preserves all legacy writer wrappers.
- The CLI task defines exact defaults, accepted names, invalid cases, path behavior, and return codes.
- The test task covers both fixture sizes, all formats, partial output, legacy tests, parser errors, Demo compatibility, and cleanup.
- No task adds Docker, dynamic allocation, or third-party dependencies.
- Every task has concrete files, interfaces, commands, and expected outcomes; no unspecified function names remain in the plan.
