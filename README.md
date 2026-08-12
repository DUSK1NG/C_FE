# 2D Truss FEM

This repository contains a fixed-capacity C11 implementation of a two-dimensional truss finite-element workflow. It demonstrates element geometry and stiffness, global assembly, loads and constraints, constrained solving, support reactions, text input, result export, and project-level verification.

## Scope and assumptions

- Structures are planar, pin-jointed trusses with two translational degrees of freedom per node.
- Elements are straight axial members with constant Young's modulus `E` and area `A`.
- Nodes and elements use positive user IDs; model input maps those IDs to internal fixed-array indices.
- Loads and support constraints are applied at nodal degrees of freedom.
- The implementation uses C11 and the standard C library only.

## Stage status

Stages 1–10 are represented in the current project:

- Stage 1: single-element geometry, direction cosines, and stiffness demonstration.
- Stage 2: global stiffness assembly and force-vector behavior.
- Stage 3: degree-of-freedom identification and validation.
- Stage 4: constrained-system preparation and numerical edge-case coverage.
- Stage 5: fixed-capacity constrained linear solving.
- Stage 6: element post-processing and state classification.
- Stage 7: support reactions and global equilibrium checks.
- Stage 8: fixed-capacity model-file parsing.
- Stage 9: TXT, Markdown, and CSV result exporters plus Debug matrix/vector printers.
- Stage 10: medium/large model fixtures, end-to-end result export, Docker verification, and project documentation.

## Docker verification

From the repository root:

```text
docker build --load -t c-fe-stage10-project-organization .
docker run --rm c-fe-stage10-project-organization
```

The builder compiles the Stage 1, Stage 6, Stage 7, Stage 8, Stage 9, and Stage 10 checks. The runtime image retains the Stage 1 Demo entry point:

```text
CMD ["./fem"]
```

## Local C11 commands

The Stage 10 contract test can be compiled and run from the repository root with:

```text
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage10.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c -Iinclude -o test_stage10 -lm
./test_stage10
```

The Stage 1 Demo can be built with:

```text
gcc -std=c11 -Wall -Wextra -pedantic src/main.c src/fem.c src/solver.c -Iinclude -o fem -lm
./fem
```

## Stage 8 input format

Model files contain these sections in order:

```text
NODES <count>
<id> <x> <y>

ELEMENTS <count>
<id> <node1_id> <node2_id> <E> <A>

LOADS <count>
<node_id> <fx> <fy>

CONSTRAINTS <count>
<node_id> <fix_x> <fix_y>
```

Blank lines and full-line `#` comments are accepted. Node and element capacities are fixed at 10 and 20 respectively; the parser does not allocate memory dynamically.

## Stage 9 result output

The output module provides:

- `write_results_txt` for a human-readable sectioned report.
- `write_results_markdown` for a report with fixed headings and tables.
- `write_results_csv` for stable wide records with `NODE`, `ELEMENT`, `REACTION`, and `SUMMARY` rows.
- `print_debug_matrix` and `print_debug_vector` for explicit Debug diagnostics.

## Stage 10 fixtures

- `tests/data/medium.model` contains 6 nodes and 8 elements. Its approved stable topology uses X/Y supports at nodes 1 and 3.
- `tests/data/large.model` contains 10 nodes and 20 elements. Elements 19–20 are treated as interior web diagonals.

The Stage 10 contract reads both fixtures, assembles and solves each model, computes reactions and element results, checks equilibrium, and writes case-specific TXT, Markdown, and CSV files before removing those generated files.

## Fixed capacities and non-goals

The project uses `MAX_NODES=10`, `MAX_ELEMENTS=20`, and `MAX_DOF=2*MAX_NODES` fixed arrays. Dynamic allocation and third-party numerical libraries are outside the project scope. Stage 10 does not add a command-line interface, persistent result database, mesh generator, nonlinear material model, or general-purpose solver beyond the fixed-capacity 2D truss workflow.
