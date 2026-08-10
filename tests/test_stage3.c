#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "fem.h"

#define TEST_TOL 1.0e-12

static void expect_close(const char *name, double actual, double expected)
{
    if (!isfinite(actual) || !isfinite(expected) ||
        fabs(actual - expected) > TEST_TOL) {
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

static void fill_int_array(int values[MAX_DOF], int count, int value)
{
    int i;

    for (i = 0; i < count; ++i) {
        values[i] = value;
    }
}

static void fill_double_array(double values[MAX_DOF], int count, double value)
{
    int i;

    for (i = 0; i < count; ++i) {
        values[i] = value;
    }
}

static void expect_zero_force(double force[MAX_DOF])
{
    int i;

    for (i = 0; i < MAX_DOF; ++i) {
        expect_close("force clear", force[i], 0.0);
    }
}

static void expect_zero_dofs(int dofs[MAX_DOF])
{
    int i;

    for (i = 0; i < MAX_DOF; ++i) {
        if (dofs[i] != 0) {
            fprintf(stderr, "FAIL: DOF array was not cleared at %d\n", i);
            exit(EXIT_FAILURE);
        }
    }
}

static void test_force_vector(void)
{
    const Node nodes[3] = {
        {1, 0.0,    0.0,   125.0,     0.0,     1, 1},
        {2, 1000.0, 0.0,   0.0,       0.0,     0, 1},
        {3, 500.0,  800.0, 0.0,  -10000.0,     0, 0}
    };
    static const double expected[6] = {
        125.0, 0.0, 0.0, 0.0, 0.0, -10000.0
    };
    double force[MAX_DOF];
    int i;

    expect_status("force vector",
                  build_force_vector(nodes, 3, force),
                  FEM_OK);
    for (i = 0; i < 6; ++i) {
        char name[32];
        snprintf(name, sizeof(name), "force[%d]", i);
        expect_close(name, force[i], expected[i]);
    }
    for (i = 6; i < MAX_DOF; ++i) {
        expect_close("force tail", force[i], 0.0);
    }
}

static void test_dof_sets(void)
{
    const Node nodes[3] = {
        {1, 0.0,    0.0, 0.0, 0.0, 1, 1},
        {2, 1000.0, 0.0, 0.0, 0.0, 0, 1},
        {3, 500.0,  800.0, 0.0, 0.0, 0, 0}
    };
    static const int expected_free[3] = {2, 4, 5};
    static const int expected_constrained[3] = {0, 1, 3};
    int free_dofs[MAX_DOF];
    int constrained_dofs[MAX_DOF];
    int free_count = 0;
    int constrained_count = 0;
    int i;

    fill_int_array(free_dofs, MAX_DOF, 77);
    fill_int_array(constrained_dofs, MAX_DOF, 77);

    expect_status("dof sets",
                  identify_dofs(nodes, 3,
                                free_dofs, &free_count,
                                constrained_dofs, &constrained_count),
                  FEM_OK);
    if (free_count != 3 || constrained_count != 3) {
        fprintf(stderr, "FAIL: unexpected DOF counts\n");
        exit(EXIT_FAILURE);
    }
    for (i = 0; i < 3; ++i) {
        if (free_dofs[i] != expected_free[i] ||
            constrained_dofs[i] != expected_constrained[i]) {
            fprintf(stderr, "FAIL: DOF mapping at index %d\n", i);
            exit(EXIT_FAILURE);
        }
    }
    for (i = 3; i < MAX_DOF; ++i) {
        if (free_dofs[i] != 0 || constrained_dofs[i] != 0) {
            fprintf(stderr, "FAIL: DOF tail was not cleared at %d\n", i);
            exit(EXIT_FAILURE);
        }
    }
}

static void test_invalid_constraint_clears_outputs(void)
{
    const Node nodes[1] = {{1, 0.0, 0.0, 0.0, 0.0, 2, 0}};
    int free_dofs[MAX_DOF];
    int constrained_dofs[MAX_DOF];
    int free_count = 99;
    int constrained_count = 99;

    fill_int_array(free_dofs, MAX_DOF, 77);
    fill_int_array(constrained_dofs, MAX_DOF, 77);
    expect_status("invalid constraint",
                  identify_dofs(nodes, 1,
                                free_dofs, &free_count,
                                constrained_dofs, &constrained_count),
                  FEM_INVALID_CONSTRAINT);
    expect_zero_dofs(free_dofs);
    expect_zero_dofs(constrained_dofs);
    if (free_count != 0 || constrained_count != 0) {
        fprintf(stderr, "FAIL: invalid constraint counts\n");
        exit(EXIT_FAILURE);
    }
}

static void test_invalid_fix_y_clears_outputs(void)
{
    const Node nodes[1] = {{1, 0.0, 0.0, 0.0, 0.0, 0, 2}};
    int free_dofs[MAX_DOF];
    int constrained_dofs[MAX_DOF];
    int free_count = 99;
    int constrained_count = 99;

    fill_int_array(free_dofs, MAX_DOF, 77);
    fill_int_array(constrained_dofs, MAX_DOF, 77);
    expect_status("invalid fix_y",
                  identify_dofs(nodes, 1,
                                free_dofs, &free_count,
                                constrained_dofs, &constrained_count),
                  FEM_INVALID_CONSTRAINT);
    expect_zero_dofs(free_dofs);
    expect_zero_dofs(constrained_dofs);
    if (free_count != 0 || constrained_count != 0) {
        fprintf(stderr, "FAIL: invalid fix_y counts\n");
        exit(EXIT_FAILURE);
    }
}

static void test_negative_constraint_clears_outputs(void)
{
    const Node nodes[1] = {{1, 0.0, 0.0, 0.0, 0.0, -1, 0}};
    int free_dofs[MAX_DOF];
    int constrained_dofs[MAX_DOF];
    int free_count = 99;
    int constrained_count = 99;

    fill_int_array(free_dofs, MAX_DOF, 77);
    fill_int_array(constrained_dofs, MAX_DOF, 77);
    expect_status("negative constraint",
                  identify_dofs(nodes, 1,
                                free_dofs, &free_count,
                                constrained_dofs, &constrained_count),
                  FEM_INVALID_CONSTRAINT);
    expect_zero_dofs(free_dofs);
    expect_zero_dofs(constrained_dofs);
    if (free_count != 0 || constrained_count != 0) {
        fprintf(stderr, "FAIL: negative constraint counts\n");
        exit(EXIT_FAILURE);
    }
}

static void test_nan_load_clears_vector(void)
{
    const Node nodes[1] = {{1, 0.0, 0.0, NAN, 0.0, 0, 0}};
    double force[MAX_DOF];

    fill_double_array(force, MAX_DOF, 77.0);
    expect_status("NaN load",
                  build_force_vector(nodes, 1, force),
                  FEM_INVALID_LOAD);
    expect_zero_force(force);
}

static void test_infinity_load_clears_vector(void)
{
    const Node nodes[1] = {{1, 0.0, 0.0, 0.0, INFINITY, 0, 0}};
    double force[MAX_DOF];

    fill_double_array(force, MAX_DOF, 77.0);
    expect_status("infinity load",
                  build_force_vector(nodes, 1, force),
                  FEM_INVALID_LOAD);
    expect_zero_force(force);
}

static void test_capacity_clears_outputs(void)
{
    Node nodes[MAX_NODES + 1];
    double force[MAX_DOF];
    int free_dofs[MAX_DOF];
    int constrained_dofs[MAX_DOF];
    int free_count = 99;
    int constrained_count = 99;
    int i;

    for (i = 0; i < MAX_NODES + 1; ++i) {
        nodes[i].id = i + 1;
        nodes[i].x = (double)i;
        nodes[i].y = 0.0;
        nodes[i].fx = 0.0;
        nodes[i].fy = 0.0;
        nodes[i].fix_x = 0;
        nodes[i].fix_y = 0;
    }
    fill_double_array(force, MAX_DOF, 77.0);
    fill_int_array(free_dofs, MAX_DOF, 77);
    fill_int_array(constrained_dofs, MAX_DOF, 77);

    expect_status("force capacity",
                  build_force_vector(nodes, MAX_NODES + 1, force),
                  FEM_CAPACITY_EXCEEDED);
    expect_zero_force(force);
    expect_status("DOF capacity",
                  identify_dofs(nodes, MAX_NODES + 1,
                                free_dofs, &free_count,
                                constrained_dofs, &constrained_count),
                  FEM_CAPACITY_EXCEEDED);
    expect_zero_dofs(free_dofs);
    expect_zero_dofs(constrained_dofs);
    if (free_count != 0 || constrained_count != 0) {
        fprintf(stderr, "FAIL: capacity counts\n");
        exit(EXIT_FAILURE);
    }
}

static void test_invalid_arguments(void)
{
    Node node = {1, 0.0, 0.0, 0.0, 0.0, 0, 0};
    double force[MAX_DOF];
    int free_dofs[MAX_DOF];
    int constrained_dofs[MAX_DOF];
    int free_count = 99;
    int constrained_count = 99;

    expect_status("null force nodes", build_force_vector(NULL, 1, force),
                  FEM_INVALID_ARGUMENT);
    expect_status("null force output", build_force_vector(&node, 1, NULL),
                  FEM_INVALID_ARGUMENT);
    expect_status("zero force count", build_force_vector(&node, 0, force),
                  FEM_INVALID_ARGUMENT);
    expect_status("null DOF nodes",
                  identify_dofs(NULL, 1, free_dofs, &free_count,
                                constrained_dofs, &constrained_count),
                  FEM_INVALID_ARGUMENT);
    expect_status("null free DOFs",
                  identify_dofs(&node, 1, NULL, &free_count,
                                constrained_dofs, &constrained_count),
                  FEM_INVALID_ARGUMENT);
    expect_status("null free count",
                  identify_dofs(&node, 1, free_dofs, NULL,
                                constrained_dofs, &constrained_count),
                  FEM_INVALID_ARGUMENT);
    expect_status("null constrained DOFs",
                  identify_dofs(&node, 1, free_dofs, &free_count,
                                NULL, &constrained_count),
                  FEM_INVALID_ARGUMENT);
    expect_status("null constrained count",
                  identify_dofs(&node, 1, free_dofs, &free_count,
                                constrained_dofs, NULL),
                  FEM_INVALID_ARGUMENT);
    expect_status("non-positive DOF count",
                  identify_dofs(&node, 0, free_dofs, &free_count,
                                constrained_dofs, &constrained_count),
                  FEM_INVALID_ARGUMENT);
}

static void test_status_messages(void)
{
    if (strcmp(fem_status_message(FEM_INVALID_CONSTRAINT),
               "constraint flags must be 0 or 1") != 0) {
        fprintf(stderr, "FAIL: FEM_INVALID_CONSTRAINT status message\n");
        exit(EXIT_FAILURE);
    }
    if (strcmp(fem_status_message(FEM_INVALID_LOAD), "loads must be finite") != 0) {
        fprintf(stderr, "FAIL: FEM_INVALID_LOAD status message\n");
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    test_force_vector();
    test_dof_sets();
    test_invalid_constraint_clears_outputs();
    test_invalid_fix_y_clears_outputs();
    test_negative_constraint_clears_outputs();
    test_nan_load_clears_vector();
    test_infinity_load_clears_vector();
    test_capacity_clears_outputs();
    test_invalid_arguments();
    test_status_messages();

    printf("Stage 3 tests passed.\n");
    return EXIT_SUCCESS;
}
