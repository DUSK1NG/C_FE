#include "pipeline.h"

#include <string.h>

#include "fem.h"
#include "io.h"
#include "output.h"
#include "postprocess.h"
#include "reactions.h"

static const double EQUILIBRIUM_TOLERANCE = 1.0e-6;

static void clear_results(FemResults *results)
{
    if (results != NULL) {
        memset(results, 0, sizeof(*results));
    }
}

static FemStatus validate_model_counts(const FemModel *model)
{
    if (model->node_count <= 0 || model->element_count <= 0) {
        return FEM_INVALID_ARGUMENT;
    }
    if (model->node_count > MAX_NODES || model->element_count > MAX_ELEMENTS) {
        return FEM_CAPACITY_EXCEEDED;
    }

    return FEM_OK;
}

FemStatus run_fem_analysis(const FemModel *model, FemResults *results)
{
    FemModel working_model;
    double global_k[MAX_DOF][MAX_DOF];
    double force[MAX_DOF];
    int free_dofs[MAX_DOF];
    int constrained_dofs[MAX_DOF];
    int free_count = 0;
    int constrained_count = 0;
    double displacement[MAX_DOF];
    double reactions[MAX_DOF];
    ElementResult element_results[MAX_ELEMENTS];
    double residual_fx = 0.0;
    double residual_fy = 0.0;
    FemStatus status;
    int i;

    if (results != NULL) {
        clear_results(results);
    }
    if (model == NULL || results == NULL) {
        return FEM_INVALID_ARGUMENT;
    }

    status = validate_model_counts(model);
    if (status != FEM_OK) {
        return status;
    }

    working_model = *model;

    status = assemble_global_stiffness(working_model.nodes,
                                       working_model.node_count,
                                       working_model.elements,
                                       working_model.element_count,
                                       global_k);
    if (status != FEM_OK) {
        return status;
    }

    status = build_force_vector(working_model.nodes,
                                working_model.node_count,
                                force);
    if (status != FEM_OK) {
        return status;
    }

    status = identify_dofs(working_model.nodes,
                           working_model.node_count,
                           free_dofs,
                           &free_count,
                           constrained_dofs,
                           &constrained_count);
    if (status != FEM_OK) {
        return status;
    }

    status = solve_constrained_system((const double (*)[MAX_DOF])global_k,
                                      force,
                                      free_dofs,
                                      free_count,
                                      constrained_dofs,
                                      constrained_count,
                                      displacement);
    if (status != FEM_OK) {
        return status;
    }

    for (i = 0; i < working_model.element_count; ++i) {
        status = calculate_element_result(&working_model.elements[i],
                                          displacement,
                                          &element_results[i]);
        if (status != FEM_OK) {
            return status;
        }
    }

    status = calculate_support_reactions(
                                         (const double (*)[MAX_DOF])global_k,
                                         force,
                                         displacement,
                                         constrained_dofs,
                                         constrained_count,
                                         reactions);
    if (status != FEM_OK) {
        return status;
    }

    status = check_global_equilibrium(force,
                                      reactions,
                                      EQUILIBRIUM_TOLERANCE,
                                      &residual_fx,
                                      &residual_fy);
    if (status != FEM_OK) {
        return status;
    }

    memcpy(results->displacement, displacement, sizeof(displacement));
    memcpy(results->reactions, reactions, sizeof(reactions));
    memcpy(results->element_results, element_results, sizeof(element_results));
    memcpy(results->constrained_dofs,
           constrained_dofs,
           sizeof(constrained_dofs));
    results->constrained_count = constrained_count;
    results->residual_fx = residual_fx;
    results->residual_fy = residual_fy;

    return FEM_OK;
}
