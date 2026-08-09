# Stage 1: Element Stiffness Matrix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement and verify the geometry calculation and 4×4 global-coordinate stiffness matrix of one 2D truss element.

**Architecture:** Keep Stage 1 deliberately small. `model.h` defines only the node and element data needed by this stage, `fem.h` defines the public FEM functions and status codes, and `fem.c` implements geometry and stiffness calculations. A standalone test executable validates the functions without involving the future global matrix, solver, I/O, or post-processing modules.

**Tech Stack:** Standard C11, GCC/MinGW-w64 on Windows, `double`, `<math.h>`, and no external FEM or linear-algebra library.

## Global Constraints

- Work directly in `C:\Users\Q1573\Desktop\MY_project\c_FE`; do not create an outer project directory.
- Use standard C11; do not use C++ or an external FEM library.
- Use mm, N, MPa, and mm² consistently.
- Use `double` for all geometry and stiffness values.
- Keep Stage 1 limited to one element; do not add global assembly, boundary conditions, Gaussian elimination, file input, or result post-processing.
- Important functions must have Chinese comments explaining their finite-element meaning.
- Use `GEOMETRY_TOL = 1.0e-12` to reject zero-length elements.
- Use `DEBUG = 1` in Stage 1 so the demonstration prints the element matrix.
- On the current machine, put `E:\\msys2\\ucrt64\\bin` before the Conda MinGW directory in the temporary PowerShell `PATH`; otherwise GCC may start but its `cc1.exe` child cannot load the matching runtime DLLs.

---

## File Map

| File | Responsibility in Stage 1 |
|---|---|
| `include/config.h` | Stage 1 debug switch and geometry tolerance |
| `include/model.h` | Minimal `Node` and `Element` data structures |
| `include/fem.h` | FEM status codes and public function declarations |
| `src/fem.c` | Geometry and 4×4 element stiffness implementation |
| `src/main.c` | Small diagonal-bar demonstration only |
| `tests/test_stage1.c` | Independent numerical checks for horizontal, diagonal, zero-length, and invalid-property cases |

## Reference Values

The diagonal test uses:

```text
Node 1: (0, 0) mm
Node 2: (500, 800) mm
E = 210000 MPa
A = 100 mm²
```

Expected geometry:

```text
L = 943.3981132056604 mm
c = 0.52999894000318
s = 0.847998304005088
```

The expected diagonal element matrix is approximately:

```text
 6252.796483   10004.474373  -6252.796483 -10004.474373
10004.474373  16007.158997 -10004.474373 -16007.158997
-6252.796483 -10004.474373   6252.796483  10004.474373
-10004.474373 -16007.158997  10004.474373  16007.158997
```

The horizontal test uses a 1000 mm bar with the same material and area. Its nonzero stiffness entries must be `+21000`, `-21000`, `-21000`, and `+21000` in the x-direction block; all y-direction entries are zero.

### Task 1: Define the Stage 1 interfaces and write the tests first

**Files:**
- Create: `include/config.h`
- Create: `include/model.h`
- Create: `include/fem.h`
- Create: `tests/test_stage1.c`

**Interfaces:**
- `calculate_element_geometry(const Node *node_i, const Node *node_j, Element *element)` consumes two nodes and fills `length`, `c`, and `s` in the element.
- `calculate_element_stiffness(const Element *element, double ke[4][4])` consumes a geometrically initialized element and fills its global-coordinate 4×4 matrix.
- Both functions return `FemStatus`, where `FEM_OK` is zero.

- [ ] **Step 1: Create the configuration header**

```c
#ifndef CONFIG_H
#define CONFIG_H

/* Stage 1 调试开关：开启后主程序打印单元刚度矩阵。 */
#define DEBUG 1

/* 长度小于该值时认为单元为零长度。单位：mm。 */
#define GEOMETRY_TOL 1.0e-12

#endif
```

- [ ] **Step 2: Create the minimal model types**

```c
#ifndef MODEL_H
#define MODEL_H

typedef struct {
    int id;
    double x;
    double y;
} Node;

typedef struct {
    int id;

    /* node1 和 node2 在后续组装阶段保存内部 0 基节点索引。 */
    int node1;
    int node2;

    double E;
    double A;

    /* 由单元几何计算得到。 */
    double length;
    double c;
    double s;
} Element;

#endif
```

- [ ] **Step 3: Create the public FEM interface**

```c
#ifndef FEM_H
#define FEM_H

#include "model.h"

typedef enum {
    FEM_OK = 0,
    FEM_INVALID_ARGUMENT,
    FEM_ZERO_LENGTH,
    FEM_INVALID_PROPERTY
} FemStatus;

/* 计算单元长度和方向余弦。 */
FemStatus calculate_element_geometry(const Node *node_i,
                                     const Node *node_j,
                                     Element *element);

/* 计算二维桁架单元在全局坐标系中的 4×4 刚度矩阵。 */
FemStatus calculate_element_stiffness(const Element *element,
                                      double ke[4][4]);

/* 将状态码转换为可读错误信息。 */
const char *fem_status_message(FemStatus status);

#endif
```

- [ ] **Step 4: Write the failing numerical tests**

```c
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fem.h"

#define TEST_TOL 1.0e-9

static void expect_close(const char *name, double actual, double expected)
{
    if (fabs(actual - expected) > TEST_TOL) {
        fprintf(stderr,
                "FAIL: %s, actual = %.12f, expected = %.12f\n",
                name,
                actual,
                expected);
        exit(EXIT_FAILURE);
    }
}

static void expect_status(const char *name,
                          FemStatus actual,
                          FemStatus expected)
{
    if (actual != expected) {
        fprintf(stderr,
                "FAIL: %s, actual status = %d, expected status = %d\n",
                name,
                actual,
                expected);
        exit(EXIT_FAILURE);
    }
}

static void expect_matrix(double actual[4][4],
                          const double expected[4][4])
{
    int i;
    int j;

    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            char name[32];
            snprintf(name, sizeof(name), "ke[%d][%d]", i, j);
            expect_close(name, actual[i][j], expected[i][j]);
        }
    }
}

static void test_horizontal_element(void)
{
    Node node_i = {1, 0.0, 0.0};
    Node node_j = {2, 1000.0, 0.0};
    Element element = {1, 0, 1, 210000.0, 100.0, 0.0, 0.0, 0.0};
    double ke[4][4];
    const double expected[4][4] = {
        {21000.0, 0.0, -21000.0, 0.0},
        {0.0, 0.0, 0.0, 0.0},
        {-21000.0, 0.0, 21000.0, 0.0},
        {0.0, 0.0, 0.0, 0.0}
    };

    expect_status("horizontal geometry",
                  calculate_element_geometry(&node_i, &node_j, &element),
                  FEM_OK);
    expect_close("horizontal length", element.length, 1000.0);
    expect_close("horizontal c", element.c, 1.0);
    expect_close("horizontal s", element.s, 0.0);

    expect_status("horizontal stiffness",
                  calculate_element_stiffness(&element, ke),
                  FEM_OK);
    expect_matrix(ke, expected);
}

static void test_diagonal_element(void)
{
    Node node_i = {1, 0.0, 0.0};
    Node node_j = {2, 500.0, 800.0};
    Element element = {1, 0, 1, 210000.0, 100.0, 0.0, 0.0, 0.0};
    double ke[4][4];
    const double expected[4][4] = {
        {6252.796483183585, 10004.474373093737,
         -6252.796483183585, -10004.474373093737},
        {10004.474373093737, 16007.158996949980,
         -10004.474373093737, -16007.158996949980},
        {-6252.796483183585, -10004.474373093737,
         6252.796483183585, 10004.474373093737},
        {-10004.474373093737, -16007.158996949980,
         10004.474373093737, 16007.158996949980}
    };

    expect_status("diagonal geometry",
                  calculate_element_geometry(&node_i, &node_j, &element),
                  FEM_OK);
    expect_close("diagonal length", element.length, 943.3981132056604);
    expect_close("diagonal c", element.c, 0.52999894000318);
    expect_close("diagonal s", element.s, 0.847998304005088);

    expect_status("diagonal stiffness",
                  calculate_element_stiffness(&element, ke),
                  FEM_OK);
    expect_matrix(ke, expected);
}

static void test_zero_length_element(void)
{
    Node node_i = {1, 10.0, 20.0};
    Node node_j = {2, 10.0, 20.0};
    Element element = {1, 0, 1, 210000.0, 100.0, 0.0, 0.0, 0.0};

    expect_status("zero-length geometry",
                  calculate_element_geometry(&node_i, &node_j, &element),
                  FEM_ZERO_LENGTH);
}

static void test_invalid_property(void)
{
    Element element = {1, 0, 1, 0.0, 100.0, 1000.0, 1.0, 0.0};
    double ke[4][4];

    expect_status("invalid elastic modulus",
                  calculate_element_stiffness(&element, ke),
                  FEM_INVALID_PROPERTY);
}

static void test_status_messages(void)
{
    if (strcmp(fem_status_message(FEM_OK), "success") != 0) {
        fprintf(stderr, "FAIL: FEM_OK status message\n");
        exit(EXIT_FAILURE);
    }

    if (strcmp(fem_status_message(FEM_ZERO_LENGTH),
               "zero-length element") != 0) {
        fprintf(stderr, "FAIL: FEM_ZERO_LENGTH status message\n");
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    test_horizontal_element();
    test_diagonal_element();
    test_zero_length_element();
    test_invalid_property();
    test_status_messages();

    printf("Stage 1 tests passed.\n");
    return EXIT_SUCCESS;
}
```

### Task 2: Run the tests and verify the expected red state

**Files:**
- Use: `include/config.h`
- Use: `include/model.h`
- Use: `include/fem.h`
- Use: `tests/test_stage1.c`

- [ ] **Step 1: Compile without the implementation**

Run from the project root in PowerShell:

```powershell
gcc -std=c11 -Wall -Wextra -pedantic `
    tests\test_stage1.c -Iinclude -o stage1_tests.exe -lm
```

Expected result: compilation reaches the link stage and fails with undefined references to `calculate_element_geometry`, `calculate_element_stiffness`, and `fem_status_message` or the corresponding functions used by the test. This confirms the tests exercise the not-yet-written implementation.

- [ ] **Step 2: Remove the temporary failed-test executable if it was created**

Run only if `stage1_tests.exe` exists:

```powershell
if (Test-Path -LiteralPath '.\stage1_tests.exe') {
    Remove-Item -LiteralPath '.\stage1_tests.exe'
}
```

### Task 3: Implement the minimal geometry and stiffness functions

**Files:**
- Create: `src/fem.c`

**Interfaces:**
- Produces the functions declared in `include/fem.h`.
- Does not depend on any future global matrix, solver, or I/O module.

- [ ] **Step 1: Implement `src/fem.c`**

```c
#include "fem.h"

#include <math.h>
#include <stddef.h>

#include "config.h"

FemStatus calculate_element_geometry(const Node *node_i,
                                     const Node *node_j,
                                     Element *element)
{
    double dx;
    double dy;
    double length;

    if (node_i == NULL || node_j == NULL || element == NULL) {
        return FEM_INVALID_ARGUMENT;
    }

    dx = node_j->x - node_i->x;
    dy = node_j->y - node_i->y;
    length = sqrt(dx * dx + dy * dy);

    if (length < GEOMETRY_TOL) {
        return FEM_ZERO_LENGTH;
    }

    element->length = length;
    element->c = dx / length;
    element->s = dy / length;

    return FEM_OK;
}

FemStatus calculate_element_stiffness(const Element *element,
                                      double ke[4][4])
{
    double factor;
    double c2;
    double s2;
    double cs;
    double values[4][4];
    int i;
    int j;

    if (element == NULL || ke == NULL) {
        return FEM_INVALID_ARGUMENT;
    }

    if (element->E <= 0.0 || element->A <= 0.0) {
        return FEM_INVALID_PROPERTY;
    }

    if (element->length < GEOMETRY_TOL) {
        return FEM_ZERO_LENGTH;
    }

    factor = element->E * element->A / element->length;
    c2 = element->c * element->c;
    s2 = element->s * element->s;
    cs = element->c * element->s;

    /*
     * 该矩阵就是二维桁架单元的全局坐标刚度矩阵。
     * 每个矩阵项对应两个全局自由度之间的刚度耦合。
     */
    values[0][0] = c2;
    values[0][1] = cs;
    values[0][2] = -c2;
    values[0][3] = -cs;

    values[1][0] = cs;
    values[1][1] = s2;
    values[1][2] = -cs;
    values[1][3] = -s2;

    values[2][0] = -c2;
    values[2][1] = -cs;
    values[2][2] = c2;
    values[2][3] = cs;

    values[3][0] = -cs;
    values[3][1] = -s2;
    values[3][2] = cs;
    values[3][3] = s2;

    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            ke[i][j] = factor * values[i][j];
        }
    }

    return FEM_OK;
}

const char *fem_status_message(FemStatus status)
{
    switch (status) {
    case FEM_OK:
        return "success";
    case FEM_INVALID_ARGUMENT:
        return "invalid argument";
    case FEM_ZERO_LENGTH:
        return "zero-length element";
    case FEM_INVALID_PROPERTY:
        return "elastic modulus and area must be positive";
    default:
        return "unknown FEM status";
    }
}
```

### Task 4: Add the demonstration program and turn the tests green

**Files:**
- Create: `src/main.c`

- [ ] **Step 1: Create the Stage 1 demonstration**

```c
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "fem.h"

static void print_matrix(double matrix[4][4])
{
    int i;
    int j;

    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            printf("%14.6f", matrix[i][j]);
        }
        printf("\n");
    }
}

int main(void)
{
    const Node node_i = {1, 0.0, 0.0};
    const Node node_j = {2, 500.0, 800.0};
    Element element = {1, 0, 1, 210000.0, 100.0, 0.0, 0.0, 0.0};
    double ke[4][4];
    FemStatus status;

    status = calculate_element_geometry(&node_i, &node_j, &element);
    if (status != FEM_OK) {
        fprintf(stderr, "Geometry error: %s\n", fem_status_message(status));
        return EXIT_FAILURE;
    }

    status = calculate_element_stiffness(&element, ke);
    if (status != FEM_OK) {
        fprintf(stderr, "Stiffness error: %s\n", fem_status_message(status));
        return EXIT_FAILURE;
    }

    printf("Stage 1: single 2D truss element\n");
    printf("Length = %.12f mm\n", element.length);
    printf("c = %.12f\n", element.c);
    printf("s = %.12f\n", element.s);

#if DEBUG
    printf("Element stiffness matrix [N/mm]:\n");
    print_matrix(ke);
#endif

    return EXIT_SUCCESS;
}
```

- [ ] **Step 2: Compile and run the standalone tests**

```powershell
gcc -std=c11 -Wall -Wextra -pedantic `
    tests\test_stage1.c src\fem.c -Iinclude `
    -o stage1_tests.exe -lm
.\stage1_tests.exe
```

Expected output:

```text
Stage 1 tests passed.
```

- [ ] **Step 3: Compile and run the demonstration**

```powershell
gcc -std=c11 -Wall -Wextra -pedantic `
    src\main.c src\fem.c -Iinclude `
    -o fem_stage1.exe -lm
.\fem_stage1.exe
```

Expected output must contain approximately:

```text
Length = 943.398113205660 mm
c = 0.529998940003
s = 0.847998304005
```

and the 4×4 matrix shown in the reference-values section.

### Task 5: Verify the stage boundary and commit only Stage 1

**Files:**
- Include in commit: `include/config.h`, `include/model.h`, `include/fem.h`, `src/fem.c`, `src/main.c`, `tests/test_stage1.c`, and this plan document.
- Do not include generated `.exe` files.

- [ ] **Step 1: Check compiler warnings and test output**

Run both compile commands from Task 4 again. The expected result is exit code 0, no compiler warnings, and `Stage 1 tests passed.`.

- [ ] **Step 2: Confirm the scope boundary**

Confirm that no files named `solver.c`, `matrix.c`, `io.c`, `postprocess.c` or input data files were created. Stage 1 must contain no global stiffness assembly or structural solve.

- [ ] **Step 3: Remove generated executables**

```powershell
foreach ($file in @('.\stage1_tests.exe', '.\fem_stage1.exe')) {
    if (Test-Path -LiteralPath $file) {
        Remove-Item -LiteralPath $file
    }
}
```

- [ ] **Step 4: Commit the verified Stage 1 implementation**

```powershell
git add include\config.h include\model.h include\fem.h `
    src\fem.c src\main.c tests\test_stage1.c `
    docs\superpowers\plans\2026-08-09-stage1-element-stiffness.md
git commit -m "feat: implement stage 1 truss element stiffness"
```

The commit must contain only the Stage 1 source, headers, tests, and the already committed plan document. The next stage begins only after this acceptance review.
