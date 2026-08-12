#include "reactions.h"

#include <math.h>
#include <stddef.h>

static void clear_reactions(double reactions[MAX_DOF])
{
    int i;

    if (reactions == NULL) {
        return;
    }

    for (i = 0; i < MAX_DOF; ++i) {
        reactions[i] = 0.0;
    }
}

static int has_finite_matrix(const double global_k[MAX_DOF][MAX_DOF])
{
    int row;
    int col;

    for (row = 0; row < MAX_DOF; ++row) {
        for (col = 0; col < MAX_DOF; ++col) {
            if (!isfinite(global_k[row][col])) {
                return 0;
            }
        }
    }

    return 1;
}

static int has_finite_vector(const double values[MAX_DOF])
{
    int i;

    for (i = 0; i < MAX_DOF; ++i) {
        if (!isfinite(values[i])) {
            return 0;
        }
    }

    return 1;
}

FemStatus calculate_support_reactions(
    const double global_k[MAX_DOF][MAX_DOF],
    const double force[MAX_DOF],
    const double displacement[MAX_DOF],
    const int constrained_dofs[MAX_DOF],
    int constrained_count,
    double reactions[MAX_DOF])
{
    double residual[MAX_DOF];
    int seen[MAX_DOF] = {0};
    int row;
    int col;
    int i;

    clear_reactions(reactions);

    if (global_k == NULL || force == NULL || displacement == NULL ||
        constrained_dofs == NULL || reactions == NULL) {
        return FEM_INVALID_ARGUMENT;
    }
    if (constrained_count < 0 || constrained_count > MAX_DOF) {
        return FEM_INVALID_ARGUMENT;
    }

    for (i = 0; i < constrained_count; ++i) {
        int dof = constrained_dofs[i];

        if (dof < 0 || dof >= MAX_DOF || seen[dof] != 0) {
            return FEM_INVALID_ARGUMENT;
        }
        seen[dof] = 1;
    }

    if (!has_finite_matrix(global_k) || !has_finite_vector(force) ||
        !has_finite_vector(displacement)) {
        return FEM_INVALID_ARGUMENT;
    }

    for (row = 0; row < MAX_DOF; ++row) {
        double value = -force[row];

        if (!isfinite(value)) {
            return FEM_INVALID_ARGUMENT;
        }

        for (col = 0; col < MAX_DOF; ++col) {
            double product = global_k[row][col] * displacement[col];
            double updated_value = value + product;

            if (!isfinite(product) || !isfinite(updated_value)) {
                return FEM_INVALID_ARGUMENT;
            }
            value = updated_value;
        }
        if (!isfinite(value)) {
            return FEM_INVALID_ARGUMENT;
        }
        residual[row] = value;
    }

    for (i = 0; i < constrained_count; ++i) {
        reactions[constrained_dofs[i]] = residual[constrained_dofs[i]];
    }

    return FEM_OK;
}

FemStatus check_global_equilibrium(
    const double force[MAX_DOF],
    const double reactions[MAX_DOF],
    double tolerance,
    double *residual_fx,
    double *residual_fy)
{
    double fx = 0.0;
    double fy = 0.0;
    int dof;

    if (residual_fx != NULL) {
        *residual_fx = 0.0;
    }
    if (residual_fy != NULL) {
        *residual_fy = 0.0;
    }

    if (force == NULL || reactions == NULL || residual_fx == NULL ||
        residual_fy == NULL || !isfinite(tolerance) || tolerance < 0.0 ||
        !has_finite_vector(force) || !has_finite_vector(reactions)) {
        return FEM_INVALID_ARGUMENT;
    }

    for (dof = 0; dof < MAX_DOF; dof += 2) {
        double force_x = force[dof] + reactions[dof];
        double force_y = force[dof + 1] + reactions[dof + 1];
        double updated_fx = fx + force_x;
        double updated_fy = fy + force_y;

        if (!isfinite(force_x) || !isfinite(force_y) ||
            !isfinite(updated_fx) || !isfinite(updated_fy)) {
            return FEM_INVALID_ARGUMENT;
        }
        fx = updated_fx;
        fy = updated_fy;
    }

    *residual_fx = fx;
    *residual_fy = fy;

    if (fabs(fx) > tolerance || fabs(fy) > tolerance) {
        return FEM_EQUILIBRIUM_ERROR;
    }

    return FEM_OK;
}
