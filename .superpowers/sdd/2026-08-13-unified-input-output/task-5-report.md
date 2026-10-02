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
