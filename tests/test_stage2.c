#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
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

static void expect_triangle_matrix(double actual[MAX_DOF][MAX_DOF])
{
    static const double expected[6][6] = {
        {27252.796483183585, 10004.474373093737, -21000.0, 0.0,
         -6252.796483183585, -10004.474373093737},
        {10004.474373093737, 16007.158996949980, 0.0, 0.0,
         -10004.474373093737, -16007.158996949980},
        {-21000.0, 0.0, 27252.796483183585, -10004.474373093737,
         -6252.796483183585, 10004.474373093737},
        {0.0, 0.0, -10004.474373093737, 16007.158996949980,
         10004.474373093737, -16007.158996949980},
        {-6252.796483183585, -10004.474373093737, -6252.796483183585,
         10004.474373093737, 12505.592966367170, 0.0},
        {-10004.474373093737, -16007.158996949980, 10004.474373093737,
         -16007.158996949980, 0.0, 32014.317993899960}
    };
    int i;
    int j;

    for (i = 0; i < 6; ++i) {
        for (j = 0; j < 6; ++j) {
            char name[32];
            snprintf(name, sizeof(name), "K[%d][%d]", i, j);
            expect_close(name, actual[i][j], expected[i][j]);
        }
    }
}

static void expect_symmetric(double matrix[MAX_DOF][MAX_DOF],
                             int dof_count)
{
    int i;
    int j;

    for (i = 0; i < dof_count; ++i) {
        for (j = 0; j < dof_count; ++j) {
            char name[64];
            snprintf(name, sizeof(name), "symmetry[%d][%d]", i, j);
            expect_close(name, matrix[i][j], matrix[j][i]);
        }
    }
}

static void expect_tail_zero(double matrix[MAX_DOF][MAX_DOF])
{
    int i;
    int j;

    for (i = 0; i < MAX_DOF; ++i) {
        for (j = 0; j < MAX_DOF; ++j) {
            if (i >= 6 || j >= 6) {
                char name[64];
                snprintf(name, sizeof(name), "tail[%d][%d]", i, j);
                expect_close(name, matrix[i][j], 0.0);
            }
        }
    }
}

static void test_triangle_assembly(void)
{
    const Node nodes[3] = {
        {.id = 1, .x = 0.0, .y = 0.0, .fx = 0.0, .fy = 0.0,
         .fix_x = 0, .fix_y = 0},
        {.id = 2, .x = 1000.0, .y = 0.0, .fx = 0.0, .fy = 0.0,
         .fix_x = 0, .fix_y = 0},
        {.id = 3, .x = 500.0, .y = 800.0, .fx = 0.0, .fy = 0.0,
         .fix_x = 0, .fix_y = 0}
    };
    Element elements[3] = {
        {1, 0, 1, 210000.0, 100.0, 0.0, 0.0, 0.0},
        {2, 0, 2, 210000.0, 100.0, 0.0, 0.0, 0.0},
        {3, 1, 2, 210000.0, 100.0, 0.0, 0.0, 0.0}
    };
    double global_k[MAX_DOF][MAX_DOF];

    expect_status("triangle assembly",
                  assemble_global_stiffness(nodes, 3, elements, 3, global_k),
                  FEM_OK);
    expect_close("element 1 length", elements[0].length, 1000.0);
    expect_close("element 2 c", elements[1].c, 0.52999894000318);
    expect_close("element 3 c", elements[2].c, -0.52999894000318);
    expect_triangle_matrix(global_k);
    expect_symmetric(global_k, 6);
    expect_tail_zero(global_k);
}

static void test_invalid_index_clears_matrix(void)
{
    const Node nodes[3] = {
        {.id = 1, .x = 0.0, .y = 0.0, .fx = 0.0, .fy = 0.0,
         .fix_x = 0, .fix_y = 0},
        {.id = 2, .x = 1000.0, .y = 0.0, .fx = 0.0, .fy = 0.0,
         .fix_x = 0, .fix_y = 0},
        {.id = 3, .x = 500.0, .y = 800.0, .fx = 0.0, .fy = 0.0,
         .fix_x = 0, .fix_y = 0}
    };
    Element elements[1] = {
        {1, 0, 3, 210000.0, 100.0, 0.0, 0.0, 0.0}
    };
    double global_k[MAX_DOF][MAX_DOF];
    int i;
    int j;

    for (i = 0; i < MAX_DOF; ++i) {
        for (j = 0; j < MAX_DOF; ++j) {
            global_k[i][j] = 123.0;
        }
    }

    expect_status("invalid node index",
                  assemble_global_stiffness(nodes, 3, elements, 1, global_k),
                  FEM_INVALID_INDEX);

    for (i = 0; i < MAX_DOF; ++i) {
        for (j = 0; j < MAX_DOF; ++j) {
            expect_close("failed assembly clears matrix", global_k[i][j], 0.0);
        }
    }
}

static void test_capacity_error(void)
{
    Node nodes[MAX_NODES + 1];
    Element element = {1, 0, 1, 210000.0, 100.0, 0.0, 0.0, 0.0};
    double global_k[MAX_DOF][MAX_DOF];
    int i;

    for (i = 0; i < MAX_NODES + 1; ++i) {
        nodes[i].id = i + 1;
        nodes[i].x = (double)i;
        nodes[i].y = 0.0;
    }

    expect_status("node capacity",
                  assemble_global_stiffness(nodes,
                                             MAX_NODES + 1,
                                             &element,
                                             1,
                                             global_k),
                  FEM_CAPACITY_EXCEEDED);
}

static void test_status_messages(void)
{
    if (strcmp(fem_status_message(FEM_INVALID_INDEX),
               "invalid node index") != 0) {
        fprintf(stderr, "FAIL: FEM_INVALID_INDEX status message\n");
        exit(EXIT_FAILURE);
    }

    if (strcmp(fem_status_message(FEM_CAPACITY_EXCEEDED),
               "model exceeds fixed capacity") != 0) {
        fprintf(stderr, "FAIL: FEM_CAPACITY_EXCEEDED status message\n");
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    test_triangle_assembly();
    test_invalid_index_clears_matrix();
    test_capacity_error();
    test_status_messages();

    printf("Stage 2 tests passed.\n");
    return EXIT_SUCCESS;
}
