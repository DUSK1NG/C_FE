# Stage 10 Project Organization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add two scale-up truss fixtures, a real Stage10 end-to-end regression, project README/report documentation, and Docker verification while preserving Stage1–9 behavior.

**Architecture:** Keep the existing fixed-capacity FEM modules and `main.c` Demo unchanged. Add model data under `tests/data`, one test-only pipeline in `tests/test_stage10.c` that composes the existing input, assembly, solver, postprocess, reactions, and output APIs, and project-level documentation that describes only verified behavior. Docker runs the new regression after Stage9.

**Tech Stack:** C11, existing fixed arrays and FEM APIs, Stage8 model format, Stage9 TXT/Markdown/CSV exporters, GCC/Docker, Markdown documentation.

## Global Constraints

- Keep `MAX_NODES=10`, `MAX_ELEMENTS=20`, and `MAX_DOF=(2 * MAX_NODES)`; do not add dynamic allocation.
- Do not change existing FEM public signatures, `FemModel`, `FemResults`, `ElementResult`, or `main.c` Demo behavior.
- Use the Stage8 model sections `NODES`, `ELEMENTS`, `LOADS`, and `CONSTRAINTS` with positive unique IDs and finite values.
- Add `medium.model` with exactly 6 nodes and 8 elements, and `large.model` with exactly 10 nodes and 20 elements.
- The Stage10 pipeline must use the existing functions `read_model_file`, `assemble_global_stiffness`, `build_force_vector`, `identify_dofs`, `solve_constrained_system`, `calculate_element_result`, `calculate_support_reactions`, `check_global_equilibrium`, `write_results_txt`, `write_results_markdown`, and `write_results_csv`.
- All generated result files must be removed by the test; no mock or external FEM solver is allowed.
- README and project report must describe only behavior supported by code or fresh verification evidence.

---

### Task 1: Define the failing end-to-end contract

**Files:**
- Create: `tests/test_stage10.c`

**Interfaces:**
- The test consumes the existing Stage8, solver, postprocess, reactions, and Stage9 output interfaces.
- The test helper `run_model_case(const char *path, int expected_nodes, int expected_elements, const char *tag)` reads one fixture and runs the complete pipeline.
- No production API is added.

- [ ] **Step 1: Add the failing Stage10 pipeline test**

In `tests/test_stage10.c`, define the following fixed-array pipeline for each fixture. Create only this C file in Task 1; do not create either model fixture until Task 2:

```c
FemModel model;
double global_k[MAX_DOF][MAX_DOF];
double force[MAX_DOF];
double displacement[MAX_DOF] = {0};
double reactions[MAX_DOF] = {0};
int free_dofs[MAX_DOF];
int constrained_dofs[MAX_DOF];
int free_count;
int constrained_count;
ElementResult element_results[MAX_ELEMENTS];
FemResults results = {0};

read_model_file(path, &model);
assemble_global_stiffness(model.nodes, model.node_count,
                          model.elements, model.element_count, global_k);
build_force_vector(model.nodes, model.node_count, force);
identify_dofs(model.nodes, model.node_count, free_dofs, &free_count,
              constrained_dofs, &constrained_count);
solve_constrained_system(global_k, force, free_dofs, free_count,
                         constrained_dofs, constrained_count, displacement);
```

Continue the helper with this exact control flow: calculate every element result, calculate reactions, check equilibrium with tolerance `1.0e-6`, copy displacement/reactions/element results/constrained DOFs/residuals into `FemResults`, and export to case-tagged paths `stage10_<tag>.txt`, `stage10_<tag>.md`, and `stage10_<tag>.csv`:

```c
for (i = 0; i < model.element_count; ++i) {
    assert(calculate_element_result(&model.elements[i], displacement,
                                    &element_results[i]) == FEM_OK);
    assert(isfinite(element_results[i].elongation));
    assert(isfinite(element_results[i].strain));
    assert(isfinite(element_results[i].stress));
    assert(isfinite(element_results[i].axial_force));
    assert(element_results[i].state == ELEMENT_NEUTRAL ||
           element_results[i].state == ELEMENT_TENSION ||
           element_results[i].state == ELEMENT_COMPRESSION);
}
assert(calculate_support_reactions(global_k, force, displacement,
                                   constrained_dofs, constrained_count,
                                   reactions) == FEM_OK);
assert(check_global_equilibrium(force, reactions, 1.0e-6,
                                &residual_fx, &residual_fy) == FEM_OK);
assert(fabs(residual_fx) <= 1.0e-6);
assert(fabs(residual_fy) <= 1.0e-6);

memcpy(results.displacement, displacement, sizeof(displacement));
memcpy(results.reactions, reactions, sizeof(reactions));
memcpy(results.element_results, element_results, sizeof(element_results));
memcpy(results.constrained_dofs, constrained_dofs, sizeof(constrained_dofs));
results.constrained_count = constrained_count;
results.residual_fx = residual_fx;
results.residual_fy = residual_fy;

snprintf(txt_path, sizeof(txt_path), "stage10_%s.txt", tag);
snprintf(md_path, sizeof(md_path), "stage10_%s.md", tag);
snprintf(csv_path, sizeof(csv_path), "stage10_%s.csv", tag);
assert(write_results_txt(txt_path, &model, &results) == FEM_OK);
assert(write_results_markdown(md_path, &model, &results) == FEM_OK);
assert(write_results_csv(csv_path, &model, &results) == FEM_OK);
snprintf(last_node_id_text, sizeof(last_node_id_text), "%d", expected_nodes);
snprintf(last_element_id_text, sizeof(last_element_id_text), "%d",
         expected_elements);
assert(file_contains(txt_path, last_node_id_text));
assert(file_contains(md_path, last_element_id_text));
assert(file_contains(csv_path, last_element_id_text));
assert(remove(txt_path) == 0);
assert(remove(md_path) == 0);
assert(remove(csv_path) == 0);
```

The test must include `<assert.h>`, `<math.h>`, `<stdio.h>`, and `<string.h>`, declare `int i;`, `double residual_fx, residual_fy;`, three `char` path buffers, and two 32-byte ID buffers in `run_model_case`, and define this helper:

```c
static int file_contains(const char *path, const char *needle)
{
    char buffer[4096];
    size_t length;
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        return 0;
    }
    length = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[length] = '\0';
    fclose(file);
    return strstr(buffer, needle) != NULL;
}
```

Use exact assertions for counts `6/8` and `10/20`. The test should reference `tests/data/medium.model` and `tests/data/large.model`, which are intentionally absent during the RED run.

- [ ] **Step 2: Run the contract to verify RED**

Compile and run before the two model files exist:

```text
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage10.c \
    src/fem.c src/solver.c src/reactions.c src/postprocess.c \
    src/io.c src/output.c -Iinclude -o test_stage10 -lm
```

Expected: compilation exits 0, then the test exits nonzero with a failed `read_model_file` assertion because the referenced fixture files do not exist. Do not create model fixtures or change production code in this task.

- [ ] **Step 3: Commit the failing contract**

```text
git add tests/test_stage10.c
git commit -m "test: define stage 10 scale-up regression"
```

---

### Task 2: Add fixtures and make the scale-up pipeline green

**Files:**
- Create: `tests/data/medium.model`
- Create: `tests/data/large.model`
- Modify: `tests/test_stage10.c` (only if fixture-specific assertions need correction)

**Interfaces:**
- Use only the existing APIs and the `FemResults` structure from `include/output.h`.
- No production source or public header changes are expected.

- [ ] **Step 1: Write `medium.model`**

Create a valid six-node, eight-element two-row truss. Use bottom nodes `(0,0)`, `(1000,0)`, `(2000,0)`, top nodes `(500,800)`, `(1500,800)`, `(2500,800)`. Use bottom chord elements, top chord elements, two end posts, and two diagonal web elements for exactly 8 elements. Use `E=210000`, `A=100` for every element, constrain node 1 in X/Y and node 3 in Y, and apply finite downward loads to top nodes 4–6.

Approved interpretation: because the top row is offset by half a panel, the two planned interior members are web diagonals rather than geometric verticals. Preserve the exact coordinates and element connectivity below; treat the earlier phrase as two additional interior web diagonals.

Use the following exact file content:

```text
# Stage 10 medium six-node eight-element truss

NODES 6
1 0 0
2 1000 0
3 2000 0
4 500 800
5 1500 800
6 2500 800

ELEMENTS 8
1 1 2 210000 100
2 2 3 210000 100
3 4 5 210000 100
4 5 6 210000 100
5 1 4 210000 100
6 3 6 210000 100
7 2 4 210000 100
8 2 5 210000 100

LOADS 3
4 0 -10000
5 0 -10000
6 0 -10000

CONSTRAINTS 2
1 1 1
3 0 1
```

- [ ] **Step 2: Write `large.model`**

Create a valid ten-node, twenty-element two-row five-panel truss. Use bottom nodes `(0,0)`, `(1000,0)`, `(2000,0)`, `(3000,0)`, `(4000,0)` and top nodes `(500,800)`, `(1500,800)`, `(2500,800)`, `(3500,800)`, `(4500,800)`. Use 4 bottom-chord elements, 4 top-chord elements, 2 end posts, 8 alternating web diagonals, and 2 interior verticals for exactly 20 elements. Constrain node 1 in X/Y and node 5 in Y; apply finite downward loads to top nodes 6–10.

Use the following exact file content:

```text
# Stage 10 large ten-node twenty-element truss

NODES 10
1 0 0
2 1000 0
3 2000 0
4 3000 0
5 4000 0
6 500 800
7 1500 800
8 2500 800
9 3500 800
10 4500 800

ELEMENTS 20
1 1 2 210000 100
2 2 3 210000 100
3 3 4 210000 100
4 4 5 210000 100
5 6 7 210000 100
6 7 8 210000 100
7 8 9 210000 100
8 9 10 210000 100
9 1 6 210000 100
10 5 10 210000 100
11 1 7 210000 100
12 2 6 210000 100
13 2 8 210000 100
14 3 7 210000 100
15 3 9 210000 100
16 4 8 210000 100
17 4 10 210000 100
18 5 9 210000 100
19 2 7 210000 100
20 4 9 210000 100

LOADS 5
6 0 -10000
7 0 -10000
8 0 -10000
9 0 -10000
10 0 -10000

CONSTRAINTS 2
1 1 1
5 0 1
```

- [ ] **Step 3: Compile the Stage10 test strictly**

Run the strict command from Task 1 after fixtures and test exist. Expected: compile exit code 0 under C11 with `-Wall -Wextra -pedantic`.

- [ ] **Step 4: Run from a writable repository directory**

Run `./test_stage10` from the repository root so `tests/data/medium.model` and `tests/data/large.model` resolve exactly as written. Expected: both cases pass and all generated TXT/Markdown/CSV files are removed.

- [ ] **Step 5: Verify negative and boundary facts in the test**

Add assertions that the medium case has exactly 6 nodes/8 elements, the large case has exactly 10 nodes/20 elements, all element states are one of the three existing enum values, and every output file contains the last user node ID and last element ID. Do not hard-code displacement values that are not part of an approved analytical reference.

- [ ] **Step 6: Run all Stage1–Stage10 tests**

Compile and run the existing Stage1–Stage9 tests with their established source lists, then compile/run Stage10. Expected: every test exits 0; inherited Stage1/Stage2 initializer and Stage7 qualifier warnings may remain, but Stage10 must introduce no new warning category.

For the Stage1–Stage9 portion of this step, run these exact commands from the repository root before running the Stage10 command from Step 3:

```text
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage1.c src/fem.c src/solver.c -Iinclude -o test_stage1 -lm && ./test_stage1
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage2.c src/fem.c src/solver.c -Iinclude -o test_stage2 -lm && ./test_stage2
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage3.c src/fem.c src/solver.c -Iinclude -o test_stage3 -lm && ./test_stage3
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage4.c src/fem.c src/solver.c -Iinclude -o test_stage4 -lm && ./test_stage4
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage5.c src/fem.c src/solver.c -Iinclude -o test_stage5 -lm && ./test_stage5
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage6.c src/fem.c src/solver.c src/postprocess.c -Iinclude -o test_stage6 -lm && ./test_stage6
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage7.c src/fem.c src/solver.c src/reactions.c -Iinclude -o test_stage7 -lm && ./test_stage7
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage8.c src/fem.c src/solver.c src/reactions.c src/io.c -Iinclude -o test_stage8 -lm && ./test_stage8
gcc -std=c11 -Wall -Wextra -pedantic tests/test_stage9.c src/fem.c src/solver.c src/reactions.c src/postprocess.c src/io.c src/output.c -Iinclude -o test_stage9 -lm && ./test_stage9
```

Expected: every test exits 0; inherited Stage1/Stage2 initializer and Stage7 qualifier warnings may remain, but Stage10 must introduce no new warning category. Remove the temporary `test_stage1` through `test_stage10` executables after verification.

- [ ] **Step 7: Run static checks and commit the green regression**

Run:

```text
git diff --check
rg -n "\b(malloc|calloc|realloc|free)\s*\(" src include tests
```

Expected: no whitespace findings and no dynamic-allocation matches. Commit:

```text
git add tests/data/medium.model tests/data/large.model tests/test_stage10.c
git commit -m "feat: add stage 10 scale-up regression"
```

---

### Task 3: Integrate Docker and organize project documentation

**Files:**
- Modify: `Dockerfile`
- Create: `README.md`
- Create: `docs/project-report.md`

**Interfaces:**
- Docker adds only a Stage10 compile/run layer after Stage9 and preserves the runtime image and `CMD ["./fem"]`.
- Documentation references the actual Stage8 input format, Stage9 output APIs, fixed capacities, and verified test commands.

- [ ] **Step 1: Add the Docker Stage10 check**

After the existing Stage9 test block, add:

```dockerfile
RUN gcc -std=c11 -Wall -Wextra -pedantic \
        tests/test_stage10.c src/fem.c src/solver.c src/reactions.c \
        src/postprocess.c src/io.c src/output.c \
        -Iinclude -o test_stage10 -lm

RUN ./test_stage10
```

- [ ] **Step 2: Write the README**

Document project purpose, assumptions, Stage1–Stage10 status, Docker and local build commands, the four Stage8 input sections, Stage9 TXT/Markdown/CSV outputs, Stage10 fixture names/counts, fixed-capacity limits, and explicit non-goals. Use concrete commands and paths from the repository.

- [ ] **Step 3: Write the project report**

Summarize the FEM theory, module data flow, stage progression, medium/large model verification method, output/Debug verification, Docker evidence, known inherited warnings, and limitations. Do not claim a numerical reference value unless it appears in a test or verification record.

- [ ] **Step 4: Run Docker build and runtime checks**

Run:

```text
docker build --load -t c-fe-stage10-project-organization .
docker run --rm c-fe-stage10-project-organization
```

Expected: Stage10 runs during the builder phase, build exits 0, runtime exits 0, and the unchanged Stage1 Demo is printed.

- [ ] **Step 5: Commit documentation and Docker integration**

```text
git add Dockerfile README.md docs/project-report.md
git commit -m "docs: organize stage 10 project and verification"
```

---

## Final review checklist

- [ ] `medium.model` is exactly 6 nodes/8 elements and solves successfully.
- [ ] `large.model` is exactly 10 nodes/20 elements and solves successfully.
- [ ] Stage10 exercises input, assembly, solve, postprocess, reactions, equilibrium, and all three exporters.
- [ ] Stage1–9 regressions and Demo remain passing.
- [ ] Docker builder runs Stage10 and runtime `CMD ["./fem"]` remains unchanged.
- [ ] README and `docs/project-report.md` contain no unsupported claims.
- [ ] No dynamic allocation or unrelated API changes are introduced.
- [ ] Final static checks and worktree status are clean.
