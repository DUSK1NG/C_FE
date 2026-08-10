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

static double rhs_scale(const double rhs[MAX_DOF], int size)
{
    double scale = 0.0;
    int row;

    for (row = 0; row < size; ++row) {
        double value = fabs(rhs[row]);

        if (value > scale) {
            scale = value;
        }
    }

    return scale;
}

static FemStatus clear_and_return_singular(double solution[MAX_DOF])
{
    clear_solution(solution);
    return FEM_SINGULAR_MATRIX;
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
    double system_scale;
    double normalized_scale;
    double absolute_pivot_threshold;
    double pivot_threshold;
    int normalization_exponent;
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

    scale = matrix_scale(matrix, size);
    absolute_pivot_threshold = SOLVER_TOL * fmax(1.0, scale);
    if (!isfinite(absolute_pivot_threshold) ||
        scale <= absolute_pivot_threshold) {
        return clear_and_return_singular(solution);
    }

    system_scale = fmax(scale, rhs_scale(rhs, size));
    normalized_scale = frexp(system_scale, &normalization_exponent);
    pivot_threshold = scalbn(absolute_pivot_threshold,
                             -normalization_exponent);
    if (!isfinite(system_scale) || system_scale <= 0.0 ||
        !isfinite(normalized_scale) || normalized_scale <= 0.0 ||
        !isfinite(pivot_threshold) || pivot_threshold <= 0.0) {
        return clear_and_return_singular(solution);
    }

    for (row = 0; row < size; ++row) {
        work_rhs[row] = scalbn(rhs[row], -normalization_exponent);
        if (!isfinite(work_rhs[row])) {
            return clear_and_return_singular(solution);
        }

        for (col = 0; col < size; ++col) {
            work_matrix[row][col] =
                scalbn(matrix[row][col], -normalization_exponent);
            if (!isfinite(work_matrix[row][col])) {
                return clear_and_return_singular(solution);
            }
        }
    }

    for (pivot = 0; pivot < size; ++pivot) {
        int pivot_row = pivot;
        double pivot_abs = fabs(work_matrix[pivot][pivot]);

        for (row = pivot + 1; row < size; ++row) {
            double candidate_abs = fabs(work_matrix[row][pivot]);

            if (!isfinite(candidate_abs)) {
                return clear_and_return_singular(solution);
            }
            if (candidate_abs > pivot_abs) {
                pivot_abs = candidate_abs;
                pivot_row = row;
            }
        }

        if (!isfinite(pivot_abs) || pivot_abs <= pivot_threshold) {
            return clear_and_return_singular(solution);
        }

        swap_rows(work_matrix, work_rhs, size, pivot, pivot_row);

        for (row = pivot + 1; row < size; ++row) {
            double factor = work_matrix[row][pivot] / work_matrix[pivot][pivot];

            if (!isfinite(factor)) {
                return clear_and_return_singular(solution);
            }

            for (col = pivot + 1; col < size; ++col) {
                double product = factor * work_matrix[pivot][col];
                double updated = work_matrix[row][col] - product;

                if (!isfinite(product) || !isfinite(updated)) {
                    return clear_and_return_singular(solution);
                }
                work_matrix[row][col] = updated;
            }
            work_matrix[row][pivot] = 0.0;

            {
                double rhs_product = factor * work_rhs[pivot];
                double updated_rhs = work_rhs[row] - rhs_product;

                if (!isfinite(rhs_product) || !isfinite(updated_rhs)) {
                    return clear_and_return_singular(solution);
                }
                work_rhs[row] = updated_rhs;
            }
        }
    }

    for (row = size - 1; row >= 0; --row) {
        double sum = 0.0;
        double diagonal = work_matrix[row][row];

        if (!isfinite(diagonal) || fabs(diagonal) <= pivot_threshold) {
            return clear_and_return_singular(solution);
        }

        for (col = row + 1; col < size; ++col) {
            double term = work_matrix[row][col] * solution[col];
            double updated_sum = sum + term;

            if (!isfinite(term) || !isfinite(updated_sum)) {
                return clear_and_return_singular(solution);
            }
            sum = updated_sum;
        }

        {
            double numerator = work_rhs[row] - sum;
            double value;

            if (!isfinite(numerator)) {
                return clear_and_return_singular(solution);
            }
            value = numerator / diagonal;
            if (!isfinite(value)) {
                return clear_and_return_singular(solution);
            }
            solution[row] = value;
        }
    }

    for (row = 0; row < size; ++row) {
        if (!isfinite(solution[row])) {
            return clear_and_return_singular(solution);
        }
    }

    return FEM_OK;
}
