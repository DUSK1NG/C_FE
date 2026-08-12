#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "fem.h"
#include "reactions.h"
#include "test_helpers.h"

#define REACTION_TOL 1.0e-6
#define EQUILIBRIUM_TOL 1.0e-8

typedef struct {
    double global_k[MAX_DOF][MAX_DOF];
    double force[MAX_DOF];
    double displacement[MAX_DOF];
    int constrained_dofs[MAX_DOF];
    int constrained_count;
} ReferenceTriangle;

static void fail(const char *name, const char *detail)
{
    fprintf(stderr, "FAIL: %s%s%s\n", name, detail == NULL ? "" : ": ",
            detail == NULL ? "" : detail);
    exit(EXIT_FAILURE);
}

static void expect_status(const char *name, FemStatus actual, FemStatus expected)
{
    if (actual != expected) {
        char detail[128];
        snprintf(detail, sizeof(detail), "actual status = %d, expected = %d",
                 actual, expected);
        fail(name, detail);
    }
}

static void expect_near(const char *name, double actual, double expected,
                        double tolerance)
{
    if (!isfinite(actual) || fabs(actual - expected) > tolerance) {
        char detail[160];
        snprintf(detail, sizeof(detail),
                 "actual = %.12f, expected = %.12f, tolerance = %.3e",
                 actual, expected, tolerance);
        fail(name, detail);
    }
}

static void expect_zero_vector(const char *name, const double values[MAX_DOF])
{
    int i;

    for (i = 0; i < MAX_DOF; ++i) {
        expect_near(name, values[i], 0.0, 0.0);
    }
}

static void fill_vector(double values[MAX_DOF], double value)
{
    int i;

    for (i = 0; i < MAX_DOF; ++i) {
        values[i] = value;
    }
}

static FemStatus build_reference_triangle(ReferenceTriangle *reference)
{
    Node nodes[3] = {
        {1, 0.0, 0.0, 0.0, 0.0, 1, 1},
        {2, 1000.0, 0.0, 0.0, 0.0, 0, 1},
        {3, 500.0, 800.0, 0.0, -10000.0, 0, 0}
    };
    Element elements[3] = {
        {1, 0, 1, 210000.0, 100.0, 0.0, 0.0, 0.0},
        {2, 0, 2, 210000.0, 100.0, 0.0, 0.0, 0.0},
        {3, 1, 2, 210000.0, 100.0, 0.0, 0.0, 0.0}
    };
    int free_dofs[MAX_DOF] = {0};
    int constrained_dofs[MAX_DOF] = {0};
    int free_count = 0;
    int constrained_count = 0;
    FemStatus status;

    if (reference == NULL) {
        return FEM_INVALID_ARGUMENT;
    }

    status = assemble_global_stiffness(nodes, 3, elements, 3,
                                       reference->global_k);
    if (status != FEM_OK) {
        return status;
    }
    status = build_force_vector(nodes, 3, reference->force);
    if (status != FEM_OK) {
        return status;
    }
    status = identify_dofs(nodes, 3, free_dofs, &free_count,
                           constrained_dofs, &constrained_count);
    if (status != FEM_OK) {
        return status;
    }
    if (constrained_count != 3 || constrained_dofs[0] != 0 ||
        constrained_dofs[1] != 1 || constrained_dofs[2] != 3) {
        return FEM_INVALID_ARGUMENT;
    }
    status = solve_constrained_system(
        test_readonly_matrix(reference->global_k),
        reference->force, free_dofs, free_count, constrained_dofs,
        constrained_count, reference->displacement);
    if (status != FEM_OK) {
        return status;
    }

    memcpy(reference->constrained_dofs, constrained_dofs,
           sizeof(reference->constrained_dofs));
    reference->constrained_count = constrained_count;
    return FEM_OK;
}

static void test_reference_reactions_and_equilibrium(void)
{
    ReferenceTriangle reference;
    double reactions[MAX_DOF];
    double residual_fx = 0.0;
    double residual_fy = 0.0;

    expect_status("build reference triangle",
                  build_reference_triangle(&reference), FEM_OK);
    expect_status(
        "calculate reference support reactions",
        calculate_support_reactions(test_readonly_matrix(reference.global_k), reference.force,
                                    reference.displacement,
                                    reference.constrained_dofs, 3, reactions),
        FEM_OK);
    expect_near("reaction at DOF 0", reactions[0], 0.0, REACTION_TOL);
    expect_near("reaction at DOF 1", reactions[1], 5000.0, REACTION_TOL);
    expect_near("reaction at DOF 3", reactions[3], 5000.0, REACTION_TOL);
    expect_near("free DOF reaction", reactions[2], 0.0, EQUILIBRIUM_TOL);

    expect_status(
        "reference global equilibrium",
        check_global_equilibrium(reference.force, reactions, EQUILIBRIUM_TOL,
                                 &residual_fx, &residual_fy),
        FEM_OK);
    expect_near("horizontal equilibrium residual", residual_fx, 0.0,
                EQUILIBRIUM_TOL);
    expect_near("vertical equilibrium residual", residual_fy, 0.0,
                EQUILIBRIUM_TOL);
}

static void test_empty_constraints_are_valid(void)
{
    ReferenceTriangle reference;
    double reactions[MAX_DOF];

    expect_status("build triangle for empty constraints",
                  build_reference_triangle(&reference), FEM_OK);
    fill_vector(reactions, 77.0);
    expect_status("empty constraints",
                  calculate_support_reactions(test_readonly_matrix(reference.global_k),
                                              reference.force,
                                              reference.displacement,
                                              reference.constrained_dofs, 0,
                                              reactions),
                  FEM_OK);
    expect_zero_vector("empty constraints output", reactions);
}

static void test_invalid_constraints_clear_reactions(void)
{
    ReferenceTriangle reference;
    double reactions[MAX_DOF];
    int invalid_dofs[MAX_DOF] = {MAX_DOF};
    int duplicate_dofs[MAX_DOF] = {0, 0};

    expect_status("build triangle for invalid constraints",
                  build_reference_triangle(&reference), FEM_OK);
    fill_vector(reactions, 77.0);
    expect_status("out-of-range constraint",
                  calculate_support_reactions(test_readonly_matrix(reference.global_k),
                                              reference.force,
                                              reference.displacement,
                                              invalid_dofs, 1, reactions),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("out-of-range reaction output", reactions);

    fill_vector(reactions, 77.0);
    expect_status("duplicate constraint",
                  calculate_support_reactions(test_readonly_matrix(reference.global_k),
                                              reference.force,
                                              reference.displacement,
                                              duplicate_dofs, 2, reactions),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("duplicate reaction output", reactions);
}

static void test_null_arguments_clear_reactions(void)
{
    ReferenceTriangle reference;
    double reactions[MAX_DOF];

    expect_status("build triangle for null arguments",
                  build_reference_triangle(&reference), FEM_OK);

    fill_vector(reactions, 77.0);
    expect_status("null global matrix",
                  calculate_support_reactions(NULL, reference.force,
                                              reference.displacement,
                                              reference.constrained_dofs, 3,
                                              reactions),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("null global matrix output", reactions);

    fill_vector(reactions, 77.0);
    expect_status("null force vector",
                  calculate_support_reactions(test_readonly_matrix(reference.global_k), NULL,
                                              reference.displacement,
                                              reference.constrained_dofs, 3,
                                              reactions),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("null force vector output", reactions);

    fill_vector(reactions, 77.0);
    expect_status("null displacement vector",
                  calculate_support_reactions(test_readonly_matrix(reference.global_k),
                                              reference.force, NULL,
                                              reference.constrained_dofs, 3,
                                              reactions),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("null displacement vector output", reactions);

    fill_vector(reactions, 77.0);
    expect_status("null constrained DOFs",
                  calculate_support_reactions(test_readonly_matrix(reference.global_k),
                                              reference.force,
                                              reference.displacement, NULL, 3,
                                              reactions),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("null constrained DOFs output", reactions);

    expect_status("null reactions output",
                  calculate_support_reactions(test_readonly_matrix(reference.global_k),
                                              reference.force,
                                              reference.displacement,
                                              reference.constrained_dofs, 3,
                                              NULL),
                  FEM_INVALID_ARGUMENT);
}

static void test_nonfinite_and_invalid_count_inputs_clear_reactions(void)
{
    ReferenceTriangle reference;
    double reactions[MAX_DOF];
    int invalid_count_dofs[MAX_DOF] = {0};

    expect_status("build triangle for invalid inputs",
                  build_reference_triangle(&reference), FEM_OK);

    reference.global_k[0][0] = NAN;
    fill_vector(reactions, 77.0);
    expect_status("NaN global matrix",
                  calculate_support_reactions(test_readonly_matrix(reference.global_k),
                                              reference.force,
                                              reference.displacement,
                                              reference.constrained_dofs, 3,
                                              reactions),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("NaN matrix output", reactions);

    expect_status("build triangle for NaN force",
                  build_reference_triangle(&reference), FEM_OK);
    reference.force[1] = NAN;
    fill_vector(reactions, 77.0);
    expect_status("NaN force vector",
                  calculate_support_reactions(test_readonly_matrix(reference.global_k),
                                              reference.force,
                                              reference.displacement,
                                              reference.constrained_dofs, 3,
                                              reactions),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("NaN force output", reactions);

    expect_status("build triangle for NaN displacement",
                  build_reference_triangle(&reference), FEM_OK);
    reference.displacement[2] = NAN;
    fill_vector(reactions, 77.0);
    expect_status("NaN displacement vector",
                  calculate_support_reactions(test_readonly_matrix(reference.global_k),
                                              reference.force,
                                              reference.displacement,
                                              reference.constrained_dofs, 3,
                                              reactions),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("NaN displacement output", reactions);

    expect_status("build triangle for invalid counts",
                  build_reference_triangle(&reference), FEM_OK);
    fill_vector(reactions, 77.0);
    expect_status("negative constraint count",
                  calculate_support_reactions(test_readonly_matrix(reference.global_k),
                                              reference.force,
                                              reference.displacement,
                                              invalid_count_dofs, -1,
                                              reactions),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("negative count output", reactions);

    fill_vector(reactions, 77.0);
    expect_status("over-capacity constraint count",
                  calculate_support_reactions(test_readonly_matrix(reference.global_k),
                                              reference.force,
                                              reference.displacement,
                                              invalid_count_dofs, MAX_DOF + 1,
                                              reactions),
                  FEM_INVALID_ARGUMENT);
    expect_zero_vector("over-capacity count output", reactions);
}

static void test_equilibrium_input_errors_clear_residuals(void)
{
    ReferenceTriangle reference;
    double reactions[MAX_DOF];
    double residual_fx;
    double residual_fy;

    expect_status("build triangle for equilibrium errors",
                  build_reference_triangle(&reference), FEM_OK);
    expect_status("calculate reactions for equilibrium errors",
                  calculate_support_reactions(test_readonly_matrix(reference.global_k),
                                              reference.force,
                                              reference.displacement,
                                              reference.constrained_dofs, 3,
                                              reactions),
                  FEM_OK);

    residual_fx = 77.0;
    residual_fy = 77.0;
    expect_status("negative equilibrium tolerance",
                  check_global_equilibrium(reference.force, reactions, -1.0,
                                           &residual_fx, &residual_fy),
                  FEM_INVALID_ARGUMENT);
    expect_near("negative tolerance residual fx", residual_fx, 0.0, 0.0);
    expect_near("negative tolerance residual fy", residual_fy, 0.0, 0.0);

    reference.force[0] = NAN;
    residual_fx = 77.0;
    residual_fy = 77.0;
    expect_status("NaN equilibrium force",
                  check_global_equilibrium(reference.force, reactions,
                                           EQUILIBRIUM_TOL, &residual_fx,
                                           &residual_fy),
                  FEM_INVALID_ARGUMENT);
    expect_near("NaN force residual fx", residual_fx, 0.0, 0.0);
    expect_near("NaN force residual fy", residual_fy, 0.0, 0.0);

    residual_fx = 77.0;
    residual_fy = 77.0;
    expect_status("null equilibrium force",
                  check_global_equilibrium(NULL, reactions, EQUILIBRIUM_TOL,
                                           &residual_fx, &residual_fy),
                  FEM_INVALID_ARGUMENT);
    expect_near("null force residual fx", residual_fx, 0.0, 0.0);
    expect_near("null force residual fy", residual_fy, 0.0, 0.0);

    residual_fx = 77.0;
    residual_fy = 77.0;
    expect_status("null residual fx",
                  check_global_equilibrium(reference.force, reactions,
                                           EQUILIBRIUM_TOL, NULL,
                                           &residual_fy),
                  FEM_INVALID_ARGUMENT);
    expect_near("null residual fy", residual_fy, 0.0, 0.0);
}

static void test_perturbed_displacement_reports_imbalance(void)
{
    ReferenceTriangle reference;
    double reactions[MAX_DOF];
    double residual_fx = 77.0;
    double residual_fy = 77.0;

    expect_status("build triangle for perturbed displacement",
                  build_reference_triangle(&reference), FEM_OK);
    reference.displacement[4] += 1.0;
    expect_status("calculate perturbed reactions",
                  calculate_support_reactions(test_readonly_matrix(reference.global_k),
                                              reference.force,
                                              reference.displacement,
                                              reference.constrained_dofs, 3,
                                              reactions),
                  FEM_OK);
    expect_status("perturbed displacement equilibrium",
                  check_global_equilibrium(reference.force, reactions,
                                           EQUILIBRIUM_TOL, &residual_fx,
                                           &residual_fy),
                  FEM_EQUILIBRIUM_ERROR);
    if (fabs(residual_fx) <= EQUILIBRIUM_TOL &&
        fabs(residual_fy) <= EQUILIBRIUM_TOL) {
        fail("perturbed displacement residual", "expected non-zero residual");
    }
}

static void test_equilibrium_status_message(void)
{
    const char *message = fem_status_message(FEM_EQUILIBRIUM_ERROR);

    if (message == NULL || strcmp(message, "global equilibrium check failed") != 0) {
        fail("equilibrium status message", message == NULL ? "is null" : message);
    }
}

int main(void)
{
    test_reference_reactions_and_equilibrium();
    test_empty_constraints_are_valid();
    test_invalid_constraints_clear_reactions();
    test_null_arguments_clear_reactions();
    test_nonfinite_and_invalid_count_inputs_clear_reactions();
    test_equilibrium_input_errors_clear_residuals();
    test_perturbed_displacement_reports_imbalance();
    test_equilibrium_status_message();

    printf("Stage 7 contract tests passed.\n");
    return EXIT_SUCCESS;
}
