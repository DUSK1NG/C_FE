# Task 5 Report: CLI/Main Integration

## Scope

- Worked only in `C:\Users\jking1\Desktop\my-project\c_FE\.worktrees\unified-input-output`
- Kept implementation scope focused on `src/main.c`
- Did not modify `cli`, `pipeline`, `output`, or documentation files

## Starting Point

- Confirmed Tasks 1–4 were already present through commit `541cf6b`
- Read `task-5-brief.md` first as requested
- Verified the pre-change failure mode by building the unified binary and running:
  - `.\fem --help`
  - `.\fem --demo`
  - `.\fem --input tests/data/medium.model --output-dir . --prefix unified_medium --format txt,markdown --include nodes,reactions,summary`
- Before the Task 5 change, all three commands incorrectly ran the legacy Stage 1 demo path

## Implementation in `src/main.c`

### 1. Preserved the Stage 1 Demo path

- Moved the existing Stage 1 demo behavior into `static int run_demo(void)`
- `main()` now dispatches to that function only when `options.demo` is set
- Demo failures now return `4` so the executable stays within the required `0/2/3/4/5` mapping

### 2. Integrated CLI parsing

- Changed `main()` signature to `int main(int argc, char *argv[])`
- Added `CliOptions` parsing with `cli_parse_args()`
- On parser failure:
  - prints the parser's message to `stderr`
  - returns parser code `2`
- On `--help`:
  - prints help to `stdout`
  - returns `0`

### 3. Added input -> analysis flow

- For non-help, non-demo execution:
  - calls `read_model_file(options.input_path, &model)`
  - calls `run_fem_analysis(&model, &results)`
- Error mapping:
  - input stage failure -> prints `Input error: ...` and returns `3`
  - analysis stage failure -> prints `Analysis error: ...` and returns `4`

### 4. Added safe output path construction

- Added fixed-capacity output path builder using `snprintf`
- Builds paths in the required fixed format order:
  1. `.txt`
  2. `.md`
  3. `.csv`
- Treats these as output errors:
  - empty output directory
  - empty prefix
  - path truncation
- Returns `5` on output path failure

### 5. Wrote only selected formats

- Added a small table-driven loop over the selected writers:
  - `write_results_txt_selected`
  - `write_results_markdown_selected`
  - `write_results_csv_selected`
- Only invokes writers for formats enabled in `options.formats`
- Uses `FemOutputOptions { options.sections }` for the include mask

### 6. Added cleanup on output failure

- Tracks the output paths created during the current invocation
- On any writer failure:
  - prints the exact failing path
  - removes files already created in that same invocation
  - removes the current partial target as well
  - returns `5`

## Verification

### Focused test binaries

Built and ran:

- `gcc -std=c11 -Wall -Wextra -pedantic tests/test_cli.c src/cli.c -Iinclude -o test_cli -lm`
- `.\test_cli`
- `gcc -std=c11 -Wall -Wextra -pedantic tests/test_pipeline.c src/pipeline.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c -Iinclude -o test_pipeline -lm`
- `.\test_pipeline`
- `gcc -std=c11 -Wall -Wextra -pedantic tests/test_output_selection.c src/pipeline.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c -Iinclude -o test_output_selection -lm`
- `.\test_output_selection`

Observed results:

- `CLI parser tests passed.`
- `Unified pipeline tests passed.`
- `Selected output section tests passed.`

### Black-box unified binary checks

Built and ran:

- `gcc -std=c11 -Wall -Wextra -pedantic src/main.c src/cli.c src/pipeline.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c -Iinclude -o fem -lm`
- `.\fem --help`
- `.\fem --demo`
- `.\fem --input tests/data/medium.model --output-dir . --prefix unified_medium --format txt,markdown --include nodes,reactions,summary`

Observed results:

- `--help` exited `0`
- `--demo` exited `0`
- custom command exited `0`
- generated only:
  - `unified_medium.txt`
  - `unified_medium.md`
- did **not** generate `unified_medium.csv`
- verified neither generated file contained the element section
- verified both files contained nodes, reactions, and summary sections

### Additional return-code checks

Ran:

- `.\fem`
- `.\fem --input tests/data/missing.model`
- `.\fem --input tests/data/medium.model --output-dir missing_output_dir --prefix cleanup_probe`

Observed results:

- no-argument invocation returned `2`
- missing input file invocation returned `3`
- missing output directory invocation returned `5`
- output failure printed the failing path:
  - `missing_output_dir/cleanup_probe.txt`
- confirmed no `cleanup_probe*` files remained afterward

### Chinese help text encoding check

- Verified the `--help` output rendered correctly in Chinese in the built executable
- No `cli.c` changes were required for help-text rendering during Task 5 integration

## Cleanup

Removed generated artifacts after verification:

- `fem.exe`
- `test_cli.exe`
- `test_pipeline.exe`
- `test_output_selection.exe`
- `unified_medium.txt`
- `unified_medium.md`

Confirmed final working tree before commit:

- only `src/main.c` modified
- `git diff --check` clean

## Notes / Residual Concern

- I directly black-box verified return codes `0`, `2`, `3`, and `5`
- Return code `4` is implemented in the `run_demo()` and pipeline-analysis failure paths, but I did not add or stage a new dedicated analysis-failure fixture because the task scope was intentionally kept to `main.c` and black-box CLI behavior
