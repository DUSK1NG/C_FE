#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "solver.h"

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

static void expect_zero_vector(const char *name,
                               const double *values,
                               int count)
{
    int i;

    for (i = 0; i < count; ++i) {
        expect_close(name, values[i], 0.0);
    }
}

static void fill_vector(double values[MAX_DOF], double value)
{
    int i;

    for (i = 0; i < MAX_DOF; ++i) {
        values[i] = value;
    }
}

static void expect_matrix_unchanged(
    const double *actual,
    const double *expected)
{
    int i;
    int j;

    for (i = 0; i < MAX_DOF; ++i) {
        for (j = 0; j < MAX_DOF; ++j) {
            char name[64];
            snprintf(name, sizeof(name), "matrix[%d][%d] unchanged", i, j);
            expect_close(name,
                         actual[i * MAX_DOF + j],
                         expected[i * MAX_DOF + j]);
        }
    }
}

static void test_two_by_two_system(void)
{
    const double matrix[MAX_DOF][MAX_DOF] = {
        {3.0, 2.0},
        {1.0, 2.0}
    };
    const double rhs[MAX_DOF] = {18.0, 14.0};
    double solution[MAX_DOF];

    expect_status("2x2 system",
                  solve_linear_system(matrix, rhs, 2, solution), FEM_OK);
    expect_close("2x2 solution[0]", solution[0], 2.0);
    expect_close("2x2 solution[1]", solution[1], 6.0);
    expect_zero_vector("2x2 solution tail", solution + 2, MAX_DOF - 2);
}

static void test_partial_pivoting(void)
{
    const double matrix[MAX_DOF][MAX_DOF] = {
        {0.0, 2.0},
        {1.0, 3.0}
    };
    const double rhs[MAX_DOF] = {4.0, 7.0};
    double solution[MAX_DOF];

    expect_status("partial pivoting",
                  solve_linear_system(matrix, rhs, 2, solution), FEM_OK);
    expect_close("pivot solution[0]", solution[0], 1.0);
    expect_close("pivot solution[1]", solution[1], 2.0);
}

static void test_three_by_three_system(void)
{
    const double matrix[MAX_DOF][MAX_DOF] = {
        {1.0, 2.0, 3.0},
        {0.0, 1.0, 4.0},
        {5.0, 6.0, 0.0}
    };
    const double rhs[MAX_DOF] = {14.0, 14.0, 17.0};
    double solution[MAX_DOF];

    expect_status("3x3 system",
                  solve_linear_system(matrix, rhs, 3, solution), FEM_OK);
    expect_close("3x3 solution[0]", solution[0], 1.0);
    expect_close("3x3 solution[1]", solution[1], 2.0);
    expect_close("3x3 solution[2]", solution[2], 3.0);
}

static void test_singular_matrix(void)
{
    const double matrix[MAX_DOF][MAX_DOF] = {
        {1.0, 2.0},
        {2.0, 4.0}
    };
    const double rhs[MAX_DOF] = {3.0, 6.0};
    double solution[MAX_DOF];
    int i;

    for (i = 0; i < MAX_DOF; ++i) {
        solution[i] = 77.0;
    }
    expect_status("singular matrix",
                  solve_linear_system(matrix, rhs, 2, solution),
                  FEM_SINGULAR_MATRIX);
    expect_zero_vector("singular solution", solution, MAX_DOF);
}

static void test_invalid_arguments(void)
{
    const double matrix[MAX_DOF][MAX_DOF] = {{1.0}};
    const double rhs[MAX_DOF] = {1.0};
    double solution[MAX_DOF];

    fill_vector(solution, 77.0);
    expect_status("null matrix", solve_linear_system(NULL, rhs, 1, solution),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("null matrix solution", solution, MAX_DOF);

    fill_vector(solution, 77.0);
    expect_status("null rhs", solve_linear_system(matrix, NULL, 1, solution),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("null rhs solution", solution, MAX_DOF);

    expect_status("null solution", solve_linear_system(matrix, rhs, 1, NULL),
                  FEM_INVALID_ARGUMENT);

    fill_vector(solution, 77.0);
    expect_status("zero size", solve_linear_system(matrix, rhs, 0, solution),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("zero size solution", solution, MAX_DOF);

    fill_vector(solution, 77.0);
    expect_status("negative size", solve_linear_system(matrix, rhs, -1, solution),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("negative size solution", solution, MAX_DOF);

    fill_vector(solution, 77.0);
    expect_status("size over capacity",
                  solve_linear_system(matrix, rhs, MAX_DOF + 1, solution),
                  FEM_CAPACITY_EXCEEDED);
    expect_zero_vector("size over capacity solution", solution, MAX_DOF);
}

static void test_nonfinite_input(void)
{
    const double nan_matrix[MAX_DOF][MAX_DOF] = {{NAN}};
    const double valid_matrix[MAX_DOF][MAX_DOF] = {{1.0}};
    const double finite_rhs[MAX_DOF] = {1.0};
    const double infinite_rhs[MAX_DOF] = {INFINITY};
    double solution[MAX_DOF];

    expect_status("NaN matrix",
                  solve_linear_system(nan_matrix, finite_rhs, 1, solution),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("NaN solution", solution, MAX_DOF);

    expect_status("infinite rhs",
                  solve_linear_system(valid_matrix, infinite_rhs, 1, solution),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("infinite rhs solution", solution, MAX_DOF);
}

static void test_input_unchanged(void)
{
    const double matrix[MAX_DOF][MAX_DOF] = {
        {1.0, 2.0, 3.0},
        {0.0, 1.0, 4.0},
        {5.0, 6.0, 0.0}
    };
    const double rhs[MAX_DOF] = {14.0, 14.0, 17.0};
    double matrix_copy[MAX_DOF][MAX_DOF];
    double rhs_copy[MAX_DOF];
    double solution[MAX_DOF];
    int i;

    memcpy(matrix_copy, matrix, sizeof(matrix_copy));
    memcpy(rhs_copy, rhs, sizeof(rhs_copy));
    expect_status("input unchanged solve",
                  solve_linear_system(matrix, rhs, 3, solution), FEM_OK);
    expect_matrix_unchanged(&matrix[0][0], &matrix_copy[0][0]);
    for (i = 0; i < MAX_DOF; ++i) {
        char name[64];
        snprintf(name, sizeof(name), "rhs[%d] unchanged", i);
        expect_close(name, rhs[i], rhs_copy[i]);
    }
}

static void test_tail_is_zero(void)
{
    const double matrix[MAX_DOF][MAX_DOF] = {
        {3.0, 2.0},
        {1.0, 2.0}
    };
    const double rhs[MAX_DOF] = {18.0, 14.0};
    double solution[MAX_DOF];
    int i;

    for (i = 0; i < MAX_DOF; ++i) {
        solution[i] = 77.0;
    }
    expect_status("tail clearing",
                  solve_linear_system(matrix, rhs, 2, solution), FEM_OK);
    for (i = 2; i < MAX_DOF; ++i) {
        expect_close("solution tail", solution[i], 0.0);
    }
}

static void test_status_message(void)
{
    const char *message = fem_status_message(FEM_SINGULAR_MATRIX);

    if (message == NULL || message[0] == '\0') {
        fprintf(stderr, "FAIL: singular status message is empty\n");
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    test_two_by_two_system();
    test_partial_pivoting();
    test_three_by_three_system();
    test_singular_matrix();
    test_invalid_arguments();
    test_nonfinite_input();
    test_input_unchanged();
    test_tail_is_zero();
    test_status_message();

    printf("Stage 4 tests passed.\n");
    return EXIT_SUCCESS;
}
