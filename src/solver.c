#include "solver.h"

#include <math.h>
#include <stddef.h>

#include "config.h"

static void clear_solution(double solution[MAX_DOF])
{
    int i;

    for (i = 0; i < MAX_DOF; ++i) {
        solution[i] = 0.0;
    }
}

static int is_finite_system(const double matrix[MAX_DOF][MAX_DOF],
                            const double rhs[MAX_DOF],
                            int size)
{
    int row;
    int col;

    for (row = 0; row < size; ++row) {
        if (!isfinite(rhs[row])) {
            return 0;
        }

        for (col = 0; col < size; ++col) {
            if (!isfinite(matrix[row][col])) {
                return 0;
            }
        }
    }

    return 1;
}

static double matrix_scale(const double matrix[MAX_DOF][MAX_DOF], int size)
{
    double scale = 0.0;
    int row;
    int col;

    for (row = 0; row < size; ++row) {
        for (col = 0; col < size; ++col) {
            double value = fabs(matrix[row][col]);

            if (value > scale) {
                scale = value;
            }
        }
    }

    return scale;
}

static void swap_rows(double matrix[MAX_DOF][MAX_DOF],
                      double rhs[MAX_DOF],
                      int size,
                      int first,
                      int second)
{
    int col;
    double temp;

    if (first == second) {
        return;
    }

    for (col = 0; col < size; ++col) {
        temp = matrix[first][col];
        matrix[first][col] = matrix[second][col];
        matrix[second][col] = temp;
    }

    temp = rhs[first];
    rhs[first] = rhs[second];
    rhs[second] = temp;
}

FemStatus solve_linear_system(const double matrix[MAX_DOF][MAX_DOF],
                              const double rhs[MAX_DOF],
                              int size,
                              double solution[MAX_DOF])
{
    double work_matrix[MAX_DOF][MAX_DOF];
    double work_rhs[MAX_DOF];
    double scale;
    double pivot_threshold;
    int pivot;
    int row;
    int col;

    if (solution != NULL) {
        clear_solution(solution);
    }
    if (matrix == NULL || rhs == NULL || solution == NULL || size <= 0) {
        return FEM_INVALID_ARGUMENT;
    }
    if (size > MAX_DOF) {
        return FEM_CAPACITY_EXCEEDED;
    }
    if (!is_finite_system(matrix, rhs, size)) {
        return FEM_INVALID_ARGUMENT;
    }

    for (row = 0; row < size; ++row) {
        work_rhs[row] = rhs[row];
        for (col = 0; col < size; ++col) {
            work_matrix[row][col] = matrix[row][col];
        }
    }

    scale = matrix_scale((const double(*)[MAX_DOF])work_matrix, size);
    pivot_threshold = SOLVER_TOL * fmax(1.0, scale);

    for (pivot = 0; pivot < size; ++pivot) {
        int pivot_row = pivot;
        double pivot_abs = fabs(work_matrix[pivot][pivot]);

        for (row = pivot + 1; row < size; ++row) {
            double candidate_abs = fabs(work_matrix[row][pivot]);

            if (candidate_abs > pivot_abs) {
                pivot_abs = candidate_abs;
                pivot_row = row;
            }
        }

        if (pivot_abs <= pivot_threshold) {
            clear_solution(solution);
            return FEM_SINGULAR_MATRIX;
        }

        swap_rows(work_matrix, work_rhs, size, pivot, pivot_row);

        for (row = pivot + 1; row < size; ++row) {
            double factor = work_matrix[row][pivot] / work_matrix[pivot][pivot];

            for (col = pivot; col < size; ++col) {
                work_matrix[row][col] -= factor * work_matrix[pivot][col];
            }
            work_matrix[row][pivot] = 0.0;
            work_rhs[row] -= factor * work_rhs[pivot];
        }
    }

    for (row = size - 1; row >= 0; --row) {
        double sum = 0.0;
        double diagonal = work_matrix[row][row];

        if (fabs(diagonal) <= pivot_threshold) {
            clear_solution(solution);
            return FEM_SINGULAR_MATRIX;
        }

        for (col = row + 1; col < size; ++col) {
            sum += work_matrix[row][col] * solution[col];
        }

        solution[row] = (work_rhs[row] - sum) / diagonal;
    }

    return FEM_OK;
}
