# Project report: 2D truss FEM, Stages 1–10

## FEM theory and data flow

Each truss element contributes a 4-by-4 global-coordinate stiffness matrix derived from its length and direction cosines. Element matrices are assembled into a fixed global stiffness matrix. Nodal loads form the global force vector. Support constraints partition the degrees of freedom; the solver computes free displacements and restores a complete displacement vector.

The post-processing flow computes element elongation, strain, stress, axial force, and state. The reaction module evaluates reactions at constrained degrees of freedom and checks global force equilibrium. The input module builds a `FemModel` from the Stage 8 sectioned text format. The output module serializes the completed model and `FemResults` into TXT, Markdown, or CSV and provides explicit Debug matrix/vector printers.

## Stage progression

1. Stage 1 establishes element geometry, direction cosines, stiffness, and the unchanged Demo entry point.
2. Stage 2 adds global assembly and force-vector behavior.
3. Stage 3 defines free and constrained degree-of-freedom identification.
4. Stage 4 covers constrained-system preparation and numerical boundary cases.
5. Stage 5 solves the fixed-capacity constrained linear system.
6. Stage 6 computes element-level result quantities and tension/compression/neutral states.
7. Stage 7 computes support reactions and equilibrium residuals.
8. Stage 8 parses fixed-capacity model files with ordered `NODES`, `ELEMENTS`, `LOADS`, and `CONSTRAINTS` sections.
9. Stage 9 exports deterministic TXT, Markdown, and CSV results and exposes Debug printers.
10. Stage 10 verifies medium and large model fixtures end to end and organizes Docker and project documentation.

## Medium and large model verification

The Stage 10 contract test reads `tests/data/medium.model` and requires 6 nodes and 8 elements. It reads `tests/data/large.model` and requires 10 nodes and 20 elements. For each case it assembles stiffness, builds loads, identifies degrees of freedom, solves displacements, computes element results, calculates support reactions, checks equilibrium, exports all three result formats, checks representative final node and element IDs, and removes generated files.

The medium fixture uses the approved stable topology with X/Y supports at nodes 1 and 3. The large fixture treats elements 19 and 20 as interior web diagonals. The verification checks finite element results and equilibrium tolerance through the existing test contract; this report does not add unverified reference numbers.

## Output, Debug, and Docker verification

Stage 9 output functions are `write_results_txt`, `write_results_markdown`, and `write_results_csv`. They use user-facing node and element IDs, fixed numeric formatting, support reaction rows, and equilibrium summaries. `print_debug_matrix` and `print_debug_vector` are separate console diagnostics.

The Stage 10 Docker builder retains the existing Stage 9 contract and Debug-output checks, then compiles and runs `tests/test_stage10.c` with the Stage 8 and Stage 9 production modules. The runtime image copies the Demo binary and retains `CMD ["./fem"]`. The exact Docker commands used for the current verification are:

```text
docker build --load -t c-fe-stage10-project-organization .
docker run --rm c-fe-stage10-project-organization
```

The command results and exit codes for this task are recorded in the accompanying Task 3 report.

## Warnings and fixed-capacity limitations

The project intentionally uses fixed arrays: `MAX_NODES=10`, `MAX_ELEMENTS=20`, and `MAX_DOF=20`. Inputs and results beyond those limits are not supported. Dynamic allocation is not used.

Known compiler warnings are inherited from earlier stages and are not introduced by the Stage 10 documentation/Docker changes. The verification record distinguishes any such warnings from Stage 10 output.

The project does not attempt nonlinear analysis, dynamic analysis, arbitrary-dimensional finite elements, automatic mesh generation, a general command-line workflow, or persistent result storage.
