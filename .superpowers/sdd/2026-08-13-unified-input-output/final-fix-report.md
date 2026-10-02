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
