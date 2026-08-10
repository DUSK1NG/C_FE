#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "fem.h"

#define TEST_TOL 1.0e-12
#define CONST_MATRIX(value) ((const double (*)[MAX_DOF])(value))

static void expect_status(const char *name, FemStatus actual, FemStatus expected)
{
    if (actual != expected) {
        fprintf(stderr, "FAIL: %s, actual status = %d, expected status = %d\n",
                name, actual, expected);
        exit(EXIT_FAILURE);
    }
}

static void expect_close(const char *name, double actual, double expected)
{
    if (!isfinite(actual) || !isfinite(expected) ||
        fabs(actual - expected) > TEST_TOL) {
        fprintf(stderr, "FAIL: %s, actual = %.12f, expected = %.12f\n",
                name, actual, expected);
        exit(EXIT_FAILURE);
    }
}

static void expect_zero_vector(const char *name, const double *values, int count)
{
    int i;

    for (i = 0; i < count; ++i) {
        expect_close(name, values[i], 0.0);
    }
}

static void fill_matrix(double matrix[MAX_DOF][MAX_DOF], double value)
{
    int i;
    int j;

    for (i = 0; i < MAX_DOF; ++i) {
        for (j = 0; j < MAX_DOF; ++j) {
            matrix[i][j] = value;
        }
    }
}

static void fill_vector(double vector[MAX_DOF], double value)
{
    int i;

    for (i = 0; i < MAX_DOF; ++i) {
        vector[i] = value;
    }
}

static void test_reduces_and_recovers_displacement(void)
{
    const int free_dofs[MAX_DOF] = {2, 4, 5};
    const int constrained_dofs[MAX_DOF] = {0, 1, 3};
    const double kff[3][3] = {{4.0, 1.0, 0.0},
                              {1.0, 3.0, 1.0},
                              {0.0, 1.0, 2.0}};
    const double ff[3] = {6.0, 10.0, 8.0};
    double global_k[MAX_DOF][MAX_DOF];
    double force[MAX_DOF];
    double displacement[MAX_DOF];
    double matrix_copy[MAX_DOF][MAX_DOF];
    double force_copy[MAX_DOF];
    int free_copy[MAX_DOF];
    int constrained_copy[MAX_DOF];
    int i;
    int j;

    fill_matrix(global_k, 0.0);
    fill_vector(force, 0.0);
    fill_vector(displacement, 77.0);
    for (i = 0; i < 3; ++i) {
        force[free_dofs[i]] = ff[i];
        for (j = 0; j < 3; ++j) {
            global_k[free_dofs[i]][free_dofs[j]] = kff[i][j];
        }
    }
    memcpy(matrix_copy, global_k, sizeof(matrix_copy));
    memcpy(force_copy, force, sizeof(force_copy));
    memcpy(free_copy, free_dofs, sizeof(free_copy));
    memcpy(constrained_copy, constrained_dofs, sizeof(constrained_copy));

    expect_status("reduce and recover",
                  solve_constrained_system(CONST_MATRIX(global_k), force, free_dofs, 3,
                                           constrained_dofs, 3, displacement),
                  FEM_OK);
    expect_close("displacement[2]", displacement[2], 1.0);
    expect_close("displacement[4]", displacement[4], 2.0);
    expect_close("displacement[5]", displacement[5], 3.0);
    for (i = 0; i < MAX_DOF; ++i) {
        if (i != 2 && i != 4 && i != 5) {
            expect_close("displacement constrained and tail", displacement[i], 0.0);
        }
    }
    for (i = 0; i < MAX_DOF; ++i) {
        if (force[i] != force_copy[i] || free_dofs[i] != free_copy[i] ||
            constrained_dofs[i] != constrained_copy[i]) {
            fprintf(stderr, "FAIL: inputs changed at index %d\n", i);
            exit(EXIT_FAILURE);
        }
        for (j = 0; j < MAX_DOF; ++j) {
            if (global_k[i][j] != matrix_copy[i][j]) {
                fprintf(stderr, "FAIL: matrix changed at [%d][%d]\n", i, j);
                exit(EXIT_FAILURE);
            }
        }
    }
}

static void test_preserves_supplied_free_order(void)
{
    const int free_dofs[MAX_DOF] = {5, 2, 4};
    const int constrained_dofs[MAX_DOF] = {0, 1, 3};
    const double kff[3][3] = {{4.0, 1.0, 0.0},
                              {1.0, 3.0, 1.0},
                              {0.0, 1.0, 2.0}};
    const double rhs[MAX_DOF] = {0.0, 0.0, 6.0, 0.0, 10.0, 8.0};
    double global_k[MAX_DOF][MAX_DOF];
    double force[MAX_DOF];
    double displacement[MAX_DOF];
    int i;
    int j;

    fill_matrix(global_k, 0.0);
    fill_vector(force, 0.0);
    fill_vector(displacement, 77.0);
    for (i = 0; i < 3; ++i) {
        force[free_dofs[i]] = rhs[free_dofs[i]];
        for (j = 0; j < 3; ++j) {
            global_k[free_dofs[i]][free_dofs[j]] = kff[2 - i][2 - j];
        }
    }

    expect_status("preserve free order",
                  solve_constrained_system(CONST_MATRIX(global_k), force, free_dofs, 3,
                                           constrained_dofs, 3, displacement),
                  FEM_OK);
    expect_close("ordered displacement[2]", displacement[2], 1.0);
    expect_close("ordered displacement[4]", displacement[4], 2.0);
    expect_close("ordered displacement[5]", displacement[5], 3.0);
}

static void test_zero_free_dofs_returns_zero_solution(void)
{
    double matrix[MAX_DOF][MAX_DOF];
    double force[MAX_DOF];
    double displacement[MAX_DOF];
    const int constrained[MAX_DOF] = {0};

    fill_matrix(matrix, 0.0);
    fill_vector(force, 0.0);
    fill_vector(displacement, 77.0);
    expect_status("zero free DOFs",
                  solve_constrained_system(CONST_MATRIX(matrix), force, NULL, 0,
                                           constrained, 1, displacement), FEM_OK);
    expect_zero_vector("zero free DOFs solution", displacement, MAX_DOF);
}

static void test_null_argument_clears_solution(void)
{
    double matrix[MAX_DOF][MAX_DOF];
    double force[MAX_DOF];
    double displacement[MAX_DOF];
    int free_dofs[MAX_DOF] = {0};
    int constrained_dofs[MAX_DOF] = {1};

    fill_matrix(matrix, 0.0);
    fill_vector(force, 0.0);
    fill_vector(displacement, 77.0);
    expect_status("null global_k",
                  solve_constrained_system(NULL, force, free_dofs, 1,
                                           constrained_dofs, 1, displacement),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("null global_k solution", displacement, MAX_DOF);
    fill_vector(displacement, 77.0);
    expect_status("null force",
                  solve_constrained_system(CONST_MATRIX(matrix), NULL, free_dofs, 1,
                                           constrained_dofs, 1, displacement),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("null force solution", displacement, MAX_DOF);
    fill_vector(displacement, 77.0);
    expect_status("null free DOFs",
                  solve_constrained_system(CONST_MATRIX(matrix), force, NULL, 1,
                                           constrained_dofs, 1, displacement),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("null free DOFs solution", displacement, MAX_DOF);
    fill_vector(displacement, 77.0);
    expect_status("null constrained DOFs",
                  solve_constrained_system(CONST_MATRIX(matrix), force, free_dofs, 1,
                                           NULL, 1, displacement),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("null constrained DOFs solution", displacement, MAX_DOF);
    expect_status("null displacement",
                  solve_constrained_system(CONST_MATRIX(matrix), force, free_dofs, 1,
                                           constrained_dofs, 1, NULL),
                  FEM_INVALID_ARGUMENT);
}

static void test_negative_counts_clear_solution(void)
{
    double matrix[MAX_DOF][MAX_DOF];
    double force[MAX_DOF];
    double displacement[MAX_DOF];
    int dofs[MAX_DOF] = {0};

    fill_matrix(matrix, 0.0);
    fill_vector(force, 0.0);
    fill_vector(displacement, 77.0);
    expect_status("negative free count",
                  solve_constrained_system(CONST_MATRIX(matrix), force, dofs, -1,
                                           dofs, 0, displacement),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("negative free count solution", displacement, MAX_DOF);
    fill_vector(displacement, 77.0);
    expect_status("negative constrained count",
                  solve_constrained_system(CONST_MATRIX(matrix), force, dofs, 0,
                                           dofs, -1, displacement),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("negative constrained count solution", displacement,
                       MAX_DOF);
}

static void test_out_of_range_dof_clears_solution(void)
{
    double matrix[MAX_DOF][MAX_DOF];
    double force[MAX_DOF];
    double displacement[MAX_DOF];
    int free_dofs[MAX_DOF] = {MAX_DOF};
    int constrained_dofs[MAX_DOF] = {0};

    fill_matrix(matrix, 0.0);
    fill_vector(force, 0.0);
    fill_vector(displacement, 77.0);
    expect_status("out of range DOF",
                  solve_constrained_system(CONST_MATRIX(matrix), force, free_dofs, 1,
                                           constrained_dofs, 1, displacement),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("out of range solution", displacement, MAX_DOF);
}

static void test_duplicate_or_overlapping_dof_clears_solution(void)
{
    double matrix[MAX_DOF][MAX_DOF];
    double force[MAX_DOF];
    double displacement[MAX_DOF];
    int duplicate[MAX_DOF] = {0, 0};
    int free_dofs[MAX_DOF] = {0};
    int constrained_dofs[MAX_DOF] = {0};

    fill_matrix(matrix, 0.0);
    fill_vector(force, 0.0);
    fill_vector(displacement, 77.0);
    expect_status("duplicate free DOF",
                  solve_constrained_system(CONST_MATRIX(matrix), force, duplicate, 2,
                                           constrained_dofs, 0, displacement),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("duplicate free solution", displacement, MAX_DOF);
    fill_vector(displacement, 77.0);
    expect_status("overlapping DOF",
                  solve_constrained_system(CONST_MATRIX(matrix), force, free_dofs, 1,
                                           constrained_dofs, 1, displacement),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("overlapping solution", displacement, MAX_DOF);
}

static void test_count_over_capacity_clears_solution(void)
{
    double matrix[MAX_DOF][MAX_DOF];
    double force[MAX_DOF];
    double displacement[MAX_DOF];
    int dofs[MAX_DOF] = {0};

    fill_matrix(matrix, 0.0);
    fill_vector(force, 0.0);
    fill_vector(displacement, 77.0);
    expect_status("free count over capacity",
                  solve_constrained_system(CONST_MATRIX(matrix), force, dofs, MAX_DOF + 1,
                                           dofs, 0, displacement),
                  FEM_CAPACITY_EXCEEDED);
    expect_zero_vector("free count over capacity solution", displacement,
                       MAX_DOF);
    fill_vector(displacement, 77.0);
    expect_status("total count over capacity",
                  solve_constrained_system(CONST_MATRIX(matrix), force, dofs, MAX_DOF,
                                           dofs, 1, displacement),
                  FEM_CAPACITY_EXCEEDED);
    expect_zero_vector("total count over capacity solution", displacement,
                       MAX_DOF);
}

static void test_singular_reduced_system_clears_solution(void)
{
    const int free_dofs[MAX_DOF] = {2, 4, 5};
    const int constrained_dofs[MAX_DOF] = {0, 1, 3};
    double matrix[MAX_DOF][MAX_DOF];
    double force[MAX_DOF];
    double displacement[MAX_DOF];
    int i;

    fill_matrix(matrix, 0.0);
    fill_vector(force, 0.0);
    for (i = 0; i < 3; ++i) {
        force[free_dofs[i]] = (double)(i + 1);
        matrix[free_dofs[0]][free_dofs[i]] = (double)(i + 1);
    }
    fill_vector(displacement, 77.0);
    expect_status("singular reduced system",
                  solve_constrained_system(CONST_MATRIX(matrix), force, free_dofs, 3,
                                           constrained_dofs, 3, displacement),
                  FEM_SINGULAR_MATRIX);
    expect_zero_vector("singular solution", displacement, MAX_DOF);
}

int main(void)
{
    test_reduces_and_recovers_displacement();
    test_preserves_supplied_free_order();
    test_zero_free_dofs_returns_zero_solution();
    test_null_argument_clears_solution();
    test_negative_counts_clear_solution();
    test_out_of_range_dof_clears_solution();
    test_duplicate_or_overlapping_dof_clears_solution();
    test_count_over_capacity_clears_solution();
    test_singular_reduced_system_clears_solution();

    printf("Stage 5 contract tests passed.\n");
    return EXIT_SUCCESS;
}
