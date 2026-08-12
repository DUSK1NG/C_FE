#include "output.h"

#include <math.h>
#include <stdio.h>

enum {
    FEM_OUTPUT_ALL_SECTIONS = FEM_OUTPUT_NODES | FEM_OUTPUT_ELEMENTS |
                              FEM_OUTPUT_REACTIONS | FEM_OUTPUT_SUMMARY
};

static int valid_element_state(ElementState state)
{
    return state == ELEMENT_NEUTRAL || state == ELEMENT_TENSION ||
           state == ELEMENT_COMPRESSION;
}

static const char *element_state_name(ElementState state)
{
    switch (state) {
    case ELEMENT_NEUTRAL:
        return "NEUTRAL";
    case ELEMENT_TENSION:
        return "TENSION";
    case ELEMENT_COMPRESSION:
        return "COMPRESSION";
    default:
        return NULL;
    }
}

static FemStatus validate_model_basics(const char *path, const FemModel *model)
{
    if (path == NULL || path[0] == '\0' || model == NULL) {
        return FEM_INVALID_ARGUMENT;
    }
    if (model->node_count < 1 || model->node_count > MAX_NODES ||
        model->element_count < 0 || model->element_count > MAX_ELEMENTS) {
        return FEM_INVALID_ARGUMENT;
    }
    return FEM_OK;
}

static FemStatus validate_nodes_section(const FemModel *model,
                                        const FemResults *results)
{
    int dof_count;
    int i;

    if (results == NULL) {
        return FEM_INVALID_ARGUMENT;
    }

    dof_count = 2 * model->node_count;
    for (i = 0; i < model->node_count; ++i) {
        const Node *node = &model->nodes[i];

        if (node->id <= 0 || !isfinite(node->x) || !isfinite(node->y) ||
            !isfinite(node->fx) || !isfinite(node->fy) ||
            (node->fix_x != 0 && node->fix_x != 1) ||
            (node->fix_y != 0 && node->fix_y != 1)) {
            return FEM_INVALID_ARGUMENT;
        }
    }

    for (i = 0; i < dof_count; ++i) {
        if (!isfinite(results->displacement[i])) {
            return FEM_INVALID_ARGUMENT;
        }
    }

    return FEM_OK;
}

static FemStatus validate_elements_section(const FemModel *model,
                                           const FemResults *results)
{
    int i;

    if (results == NULL) {
        return FEM_INVALID_ARGUMENT;
    }

    for (i = 0; i < model->element_count; ++i) {
        const ElementResult *element_result = &results->element_results[i];

        if (!isfinite(element_result->elongation) ||
            !isfinite(element_result->strain) ||
            !isfinite(element_result->stress) ||
            !isfinite(element_result->axial_force) ||
            !valid_element_state(element_result->state)) {
            return FEM_INVALID_ARGUMENT;
        }
    }

    return FEM_OK;
}

static FemStatus validate_reactions_section(const FemModel *model,
                                            const FemResults *results,
                                            int constrained[MAX_DOF])
{
    int dof_count;
    int i;

    if (results == NULL || constrained == NULL) {
        return FEM_INVALID_ARGUMENT;
    }

    if (results->constrained_count < 0 ||
        results->constrained_count > MAX_DOF) {
        return FEM_INVALID_ARGUMENT;
    }

    dof_count = 2 * model->node_count;
    for (i = 0; i < MAX_DOF; ++i) {
        constrained[i] = 0;
    }

    for (i = 0; i < results->constrained_count; ++i) {
        int dof = results->constrained_dofs[i];

        if (dof < 0 || dof >= dof_count || constrained[dof] != 0 ||
            !isfinite(results->reactions[dof])) {
            return FEM_INVALID_ARGUMENT;
        }
        constrained[dof] = 1;
    }

    return FEM_OK;
}

static FemStatus validate_summary_section(const FemResults *results)
{
    if (results == NULL || !isfinite(results->residual_fx) ||
        !isfinite(results->residual_fy)) {
        return FEM_INVALID_ARGUMENT;
    }

    return FEM_OK;
}

static FemStatus validate_output_selection(const char *path,
                                           const FemModel *model,
                                           const FemResults *results,
                                           const FemOutputOptions *options,
                                           int constrained[MAX_DOF])
{
    unsigned sections;
    FemStatus status;

    status = validate_model_basics(path, model);
    if (status != FEM_OK || results == NULL || options == NULL) {
        return FEM_INVALID_ARGUMENT;
    }

    sections = options->sections;
    if (sections == 0u || (sections & ~FEM_OUTPUT_ALL_SECTIONS) != 0u) {
        return FEM_INVALID_ARGUMENT;
    }

    if ((sections & FEM_OUTPUT_NODES) != 0u) {
        status = validate_nodes_section(model, results);
        if (status != FEM_OK) {
            return status;
        }
    }

    if ((sections & FEM_OUTPUT_ELEMENTS) != 0u) {
        status = validate_elements_section(model, results);
        if (status != FEM_OK) {
            return status;
        }
    }

    if ((sections & FEM_OUTPUT_REACTIONS) != 0u) {
        status = validate_reactions_section(model, results, constrained);
        if (status != FEM_OK) {
            return status;
        }
    } else if (constrained != NULL) {
        int i;

        for (i = 0; i < MAX_DOF; ++i) {
            constrained[i] = 0;
        }
    }

    if ((sections & FEM_OUTPUT_SUMMARY) != 0u) {
        status = validate_summary_section(results);
        if (status != FEM_OK) {
            return status;
        }
    }

    return FEM_OK;
}

static FemStatus finish_file(FILE *file, int write_failed)
{
    int close_failed;

    if (file == NULL) {
        return FEM_INPUT_ERROR;
    }
    if (ferror(file) != 0) {
        write_failed = 1;
    }
    close_failed = fclose(file) != 0;
    return write_failed || close_failed ? FEM_INPUT_ERROR : FEM_OK;
}

static int write_txt_nodes(FILE *file,
                           const FemModel *model,
                           const FemResults *results,
                           int compatibility_mode)
{
    int i;

    if (compatibility_mode) {
        if (fprintf(file, "Nodal Displacements\nnode_id ux uy\n") < 0) {
            return 1;
        }
        for (i = 0; i < model->node_count; ++i) {
            if (fprintf(file, "%d %.12g %.12g\n", model->nodes[i].id,
                        results->displacement[2 * i],
                        results->displacement[2 * i + 1]) < 0) {
                return 1;
            }
        }
        return 0;
    }

    if (fprintf(file,
                "Nodal Displacements\n"
                "node_id ux uy x y fx fy fix_x fix_y\n") < 0) {
        return 1;
    }
    for (i = 0; i < model->node_count; ++i) {
        const Node *node = &model->nodes[i];

        if (fprintf(file, "%d %.12g %.12g %.12g %.12g %.12g %.12g %d %d\n",
                    node->id, results->displacement[2 * i],
                    results->displacement[2 * i + 1], node->x, node->y,
                    node->fx, node->fy, node->fix_x, node->fix_y) < 0) {
            return 1;
        }
    }

    return 0;
}

static int write_txt_elements(FILE *file,
                              const FemModel *model,
                              const FemResults *results)
{
    int i;

    if (fprintf(file,
                "Element Results\n"
                "element_id elongation strain stress axial_force state\n") < 0) {
        return 1;
    }
    for (i = 0; i < model->element_count; ++i) {
        const ElementResult *element_result = &results->element_results[i];

        if (fprintf(file, "%d %.12g %.12g %.12g %.12g %s\n",
                    model->elements[i].id, element_result->elongation,
                    element_result->strain, element_result->stress,
                    element_result->axial_force,
                    element_state_name(element_result->state)) < 0) {
            return 1;
        }
    }

    return 0;
}

static int write_txt_reactions(FILE *file,
                               const FemModel *model,
                               const FemResults *results,
                               const int constrained[MAX_DOF])
{
    int i;

    if (fprintf(file, "Support Reactions\nnode_id rx ry\n") < 0) {
        return 1;
    }
    for (i = 0; i < model->node_count; ++i) {
        int x_dof = 2 * i;
        int y_dof = x_dof + 1;

        if (constrained[x_dof] != 0 || constrained[y_dof] != 0) {
            double rx = constrained[x_dof] != 0 ? results->reactions[x_dof] : 0.0;
            double ry = constrained[y_dof] != 0 ? results->reactions[y_dof] : 0.0;

            if (fprintf(file, "%d %.12g %.12g\n", model->nodes[i].id, rx, ry) <
                0) {
                return 1;
            }
        }
    }

    return 0;
}

static int write_txt_summary(FILE *file,
                             const FemModel *model,
                             const FemResults *results,
                             int compatibility_mode)
{
    if (compatibility_mode) {
        return fprintf(file,
                       "Equilibrium\n"
                       "residual_fx residual_fy\n"
                       "%.12g %.12g\n",
                       results->residual_fx, results->residual_fy) < 0;
    }

    return fprintf(file,
                   "Equilibrium\n"
                   "node_count element_count residual_fx residual_fy\n"
                   "%d %d %.12g %.12g\n",
                   model->node_count, model->element_count,
                   results->residual_fx, results->residual_fy) < 0;
}

static int write_markdown_nodes(FILE *file,
                                const FemModel *model,
                                const FemResults *results,
                                int compatibility_mode)
{
    int i;

    if (compatibility_mode) {
        if (fprintf(file,
                    "## Nodal Displacements\n"
                    "| Node ID | ux | uy |\n"
                    "|---:|---:|---:|\n") < 0) {
            return 1;
        }
        for (i = 0; i < model->node_count; ++i) {
            if (fprintf(file, "| %d | %.12g | %.12g |\n", model->nodes[i].id,
                        results->displacement[2 * i],
                        results->displacement[2 * i + 1]) < 0) {
                return 1;
            }
        }
        return 0;
    }

    if (fprintf(file,
                "## Nodal Displacements\n"
                "| Node ID | ux | uy | x | y | fx | fy | fix_x | fix_y |\n"
                "|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n") < 0) {
        return 1;
    }
    for (i = 0; i < model->node_count; ++i) {
        const Node *node = &model->nodes[i];

        if (fprintf(file,
                    "| %d | %.12g | %.12g | %.12g | %.12g | %.12g | %.12g | %d | %d |\n",
                    node->id, results->displacement[2 * i],
                    results->displacement[2 * i + 1], node->x, node->y,
                    node->fx, node->fy, node->fix_x, node->fix_y) < 0) {
            return 1;
        }
    }

    return 0;
}

static int write_markdown_elements(FILE *file,
                                   const FemModel *model,
                                   const FemResults *results)
{
    int i;

    if (fprintf(file,
                "## Element Results\n"
                "| Element ID | Elongation | Strain | Stress | Axial Force | State |\n"
                "|---:|---:|---:|---:|---:|---|\n") < 0) {
        return 1;
    }
    for (i = 0; i < model->element_count; ++i) {
        const ElementResult *element_result = &results->element_results[i];

        if (fprintf(file, "| %d | %.12g | %.12g | %.12g | %.12g | %s |\n",
                    model->elements[i].id, element_result->elongation,
                    element_result->strain, element_result->stress,
                    element_result->axial_force,
                    element_state_name(element_result->state)) < 0) {
            return 1;
        }
    }

    return 0;
}

static int write_markdown_reactions(FILE *file,
                                    const FemModel *model,
                                    const FemResults *results,
                                    const int constrained[MAX_DOF])
{
    int i;

    if (fprintf(file,
                "## Support Reactions\n"
                "| Node ID | rx | ry |\n"
                "|---:|---:|---:|\n") < 0) {
        return 1;
    }
    for (i = 0; i < model->node_count; ++i) {
        int x_dof = 2 * i;
        int y_dof = x_dof + 1;

        if (constrained[x_dof] != 0 || constrained[y_dof] != 0) {
            double rx = constrained[x_dof] != 0 ? results->reactions[x_dof] : 0.0;
            double ry = constrained[y_dof] != 0 ? results->reactions[y_dof] : 0.0;

            if (fprintf(file, "| %d | %.12g | %.12g |\n", model->nodes[i].id,
                        rx, ry) < 0) {
                return 1;
            }
        }
    }

    return 0;
}

static int write_markdown_summary(FILE *file,
                                  const FemModel *model,
                                  const FemResults *results,
                                  int compatibility_mode)
{
    if (compatibility_mode) {
        return fprintf(file,
                       "## Equilibrium\n"
                       "| Residual Fx | Residual Fy |\n"
                       "|---:|---:|\n"
                       "| %.12g | %.12g |\n",
                       results->residual_fx, results->residual_fy) < 0;
    }

    return fprintf(file,
                   "## Equilibrium\n"
                   "| Node Count | Element Count | Residual Fx | Residual Fy |\n"
                   "|---:|---:|---:|---:|\n"
                   "| %d | %d | %.12g | %.12g |\n",
                   model->node_count, model->element_count,
                   results->residual_fx, results->residual_fy) < 0;
}

static int write_csv_nodes(FILE *file,
                           const FemModel *model,
                           const FemResults *results,
                           int compatibility_mode)
{
    int i;

    if (compatibility_mode) {
        for (i = 0; i < model->node_count; ++i) {
            if (fprintf(file, "NODE,%d,%.12g,%.12g,,,,,,,,,\n",
                        model->nodes[i].id, results->displacement[2 * i],
                        results->displacement[2 * i + 1]) < 0) {
                return 1;
            }
        }
        return 0;
    }

    for (i = 0; i < model->node_count; ++i) {
        const Node *node = &model->nodes[i];

        if (fprintf(file,
                    "NODE,%d,%.12g,%.12g,,,,,,,,,%.12g,%.12g,%.12g,%.12g,%d,%d\n",
                    node->id, results->displacement[2 * i],
                    results->displacement[2 * i + 1], node->x, node->y,
                    node->fx, node->fy, node->fix_x, node->fix_y) < 0) {
            return 1;
        }
    }

    return 0;
}

static int write_csv_elements(FILE *file,
                              const FemModel *model,
                              const FemResults *results,
                              int compatibility_mode)
{
    int i;

    for (i = 0; i < model->element_count; ++i) {
        const ElementResult *element_result = &results->element_results[i];

        if (compatibility_mode) {
            if (fprintf(file, "ELEMENT,%d,,,%.12g,%.12g,%.12g,%.12g,%s,,,,\n",
                        model->elements[i].id, element_result->elongation,
                        element_result->strain, element_result->stress,
                        element_result->axial_force,
                        element_state_name(element_result->state)) < 0) {
                return 1;
            }
        } else if (fprintf(file,
                           "ELEMENT,%d,,,%.12g,%.12g,%.12g,%.12g,%s,,,,,,,,\n",
                           model->elements[i].id, element_result->elongation,
                           element_result->strain, element_result->stress,
                           element_result->axial_force,
                           element_state_name(element_result->state)) < 0) {
            return 1;
        }
    }

    return 0;
}

static int write_csv_reactions(FILE *file,
                               const FemModel *model,
                               const FemResults *results,
                               const int constrained[MAX_DOF],
                               int compatibility_mode)
{
    int i;

    for (i = 0; i < model->node_count; ++i) {
        int x_dof = 2 * i;
        int y_dof = x_dof + 1;

        if (constrained[x_dof] != 0 || constrained[y_dof] != 0) {
            double rx = constrained[x_dof] != 0 ? results->reactions[x_dof] : 0.0;
            double ry = constrained[y_dof] != 0 ? results->reactions[y_dof] : 0.0;

            if (compatibility_mode) {
                if (fprintf(file, "REACTION,%d,,,,,,,,%.12g,%.12g,,\n",
                            model->nodes[i].id, rx, ry) < 0) {
                    return 1;
                }
            } else if (fprintf(file, "REACTION,%d,,,,,,,,%.12g,%.12g,,,,,,\n",
                               model->nodes[i].id, rx, ry) < 0) {
                return 1;
            }
        }
    }

    return 0;
}

static int write_csv_summary(FILE *file,
                             const FemModel *model,
                             const FemResults *results,
                             int compatibility_mode)
{
    if (compatibility_mode) {
        return fprintf(file, "SUMMARY,,,,,,,,,,,%.12g,%.12g\n",
                       results->residual_fx, results->residual_fy) < 0;
    }

    return fprintf(file, "SUMMARY,,,,,,,,,,,%.12g,%.12g,,,,,,,%d,%d\n",
                   results->residual_fx, results->residual_fy,
                   model->node_count, model->element_count) < 0;
}

static FemStatus write_results_txt_legacy(const char *path,
                                          const FemModel *model,
                                          const FemResults *results)
{
    int constrained[MAX_DOF];
    FILE *file;
    int write_failed = 0;

    if (validate_output_selection(path, model, results,
                                  &(const FemOutputOptions){FEM_OUTPUT_ALL_SECTIONS},
                                  constrained) != FEM_OK) {
        return FEM_INVALID_ARGUMENT;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return FEM_INPUT_ERROR;
    }

    if (fprintf(file, "2D Truss FEM Results\n\n") < 0) {
        write_failed = 1;
    }
    if (!write_failed) {
        write_failed = write_txt_nodes(file, model, results, 1);
    }
    if (!write_failed && fprintf(file, "\n") < 0) {
        write_failed = 1;
    }
    if (!write_failed) {
        write_failed = write_txt_elements(file, model, results);
    }
    if (!write_failed && fprintf(file, "\n") < 0) {
        write_failed = 1;
    }
    if (!write_failed) {
        write_failed = write_txt_reactions(file, model, results, constrained);
    }
    if (!write_failed && fprintf(file, "\n") < 0) {
        write_failed = 1;
    }
    if (!write_failed) {
        write_failed = write_txt_summary(file, model, results, 1);
    }

    return finish_file(file, write_failed);
}

static FemStatus write_results_markdown_legacy(const char *path,
                                               const FemModel *model,
                                               const FemResults *results)
{
    int constrained[MAX_DOF];
    FILE *file;
    int write_failed = 0;

    if (validate_output_selection(path, model, results,
                                  &(const FemOutputOptions){FEM_OUTPUT_ALL_SECTIONS},
                                  constrained) != FEM_OK) {
        return FEM_INVALID_ARGUMENT;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return FEM_INPUT_ERROR;
    }

    if (fprintf(file, "# 2D Truss FEM Results\n\n") < 0) {
        write_failed = 1;
    }
    if (!write_failed) {
        write_failed = write_markdown_nodes(file, model, results, 1);
    }
    if (!write_failed && fprintf(file, "\n") < 0) {
        write_failed = 1;
    }
    if (!write_failed) {
        write_failed = write_markdown_elements(file, model, results);
    }
    if (!write_failed && fprintf(file, "\n") < 0) {
        write_failed = 1;
    }
    if (!write_failed) {
        write_failed = write_markdown_reactions(file, model, results, constrained);
    }
    if (!write_failed && fprintf(file, "\n") < 0) {
        write_failed = 1;
    }
    if (!write_failed) {
        write_failed = write_markdown_summary(file, model, results, 1);
    }

    return finish_file(file, write_failed);
}

static FemStatus write_results_csv_legacy(const char *path,
                                          const FemModel *model,
                                          const FemResults *results)
{
    int constrained[MAX_DOF];
    FILE *file;
    int write_failed = 0;

    if (validate_output_selection(path, model, results,
                                  &(const FemOutputOptions){FEM_OUTPUT_ALL_SECTIONS},
                                  constrained) != FEM_OK) {
        return FEM_INVALID_ARGUMENT;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return FEM_INPUT_ERROR;
    }

    if (fprintf(file,
                "record_type,id,ux,uy,elongation,strain,stress,axial_force,"
                "state,rx,ry,residual_fx,residual_fy\n") < 0) {
        write_failed = 1;
    }
    if (!write_failed) {
        write_failed = write_csv_nodes(file, model, results, 1);
    }
    if (!write_failed) {
        write_failed = write_csv_elements(file, model, results, 1);
    }
    if (!write_failed) {
        write_failed = write_csv_reactions(file, model, results, constrained, 1);
    }
    if (!write_failed) {
        write_failed = write_csv_summary(file, model, results, 1);
    }

    return finish_file(file, write_failed);
}

FemStatus write_results_txt_selected(const char *path,
                                     const FemModel *model,
                                     const FemResults *results,
                                     const FemOutputOptions *options)
{
    int constrained[MAX_DOF];
    FILE *file;
    int write_failed = 0;
    int wrote_section = 0;
    if (validate_output_selection(path, model, results, options, constrained) !=
        FEM_OK) {
        return FEM_INVALID_ARGUMENT;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return FEM_INPUT_ERROR;
    }

    if (fprintf(file, "2D Truss FEM Results\n\n") < 0) {
        write_failed = 1;
    }

    if (!write_failed && (options->sections & FEM_OUTPUT_NODES) != 0u) {
        write_failed = write_txt_nodes(file, model, results, 0);
        wrote_section = 1;
    }
    if (!write_failed && (options->sections & FEM_OUTPUT_ELEMENTS) != 0u) {
        if (wrote_section && fprintf(file, "\n") < 0) {
            write_failed = 1;
        }
        if (!write_failed) {
            write_failed = write_txt_elements(file, model, results);
            wrote_section = 1;
        }
    }
    if (!write_failed && (options->sections & FEM_OUTPUT_REACTIONS) != 0u) {
        if (wrote_section && fprintf(file, "\n") < 0) {
            write_failed = 1;
        }
        if (!write_failed) {
            write_failed = write_txt_reactions(file, model, results, constrained);
            wrote_section = 1;
        }
    }
    if (!write_failed && (options->sections & FEM_OUTPUT_SUMMARY) != 0u) {
        if (wrote_section && fprintf(file, "\n") < 0) {
            write_failed = 1;
        }
        if (!write_failed) {
            write_failed = write_txt_summary(file, model, results, 0);
        }
    }

    return finish_file(file, write_failed);
}

FemStatus write_results_markdown_selected(const char *path,
                                          const FemModel *model,
                                          const FemResults *results,
                                          const FemOutputOptions *options)
{
    int constrained[MAX_DOF];
    FILE *file;
    int write_failed = 0;
    int wrote_section = 0;
    if (validate_output_selection(path, model, results, options, constrained) !=
        FEM_OK) {
        return FEM_INVALID_ARGUMENT;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return FEM_INPUT_ERROR;
    }

    if (fprintf(file, "# 2D Truss FEM Results\n\n") < 0) {
        write_failed = 1;
    }

    if (!write_failed && (options->sections & FEM_OUTPUT_NODES) != 0u) {
        write_failed = write_markdown_nodes(file, model, results, 0);
        wrote_section = 1;
    }
    if (!write_failed && (options->sections & FEM_OUTPUT_ELEMENTS) != 0u) {
        if (wrote_section && fprintf(file, "\n") < 0) {
            write_failed = 1;
        }
        if (!write_failed) {
            write_failed = write_markdown_elements(file, model, results);
            wrote_section = 1;
        }
    }
    if (!write_failed && (options->sections & FEM_OUTPUT_REACTIONS) != 0u) {
        if (wrote_section && fprintf(file, "\n") < 0) {
            write_failed = 1;
        }
        if (!write_failed) {
            write_failed = write_markdown_reactions(file, model, results,
                                                    constrained);
            wrote_section = 1;
        }
    }
    if (!write_failed && (options->sections & FEM_OUTPUT_SUMMARY) != 0u) {
        if (wrote_section && fprintf(file, "\n") < 0) {
            write_failed = 1;
        }
        if (!write_failed) {
            write_failed = write_markdown_summary(file, model, results, 0);
        }
    }

    return finish_file(file, write_failed);
}

FemStatus write_results_csv_selected(const char *path,
                                     const FemModel *model,
                                     const FemResults *results,
                                     const FemOutputOptions *options)
{
    int constrained[MAX_DOF];
    FILE *file;
    int write_failed = 0;
    if (validate_output_selection(path, model, results, options, constrained) !=
        FEM_OK) {
        return FEM_INVALID_ARGUMENT;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return FEM_INPUT_ERROR;
    }

    if (fprintf(file,
               "record_type,id,ux,uy,elongation,strain,stress,axial_force,"
               "state,rx,ry,residual_fx,residual_fy,node_x,node_y,node_fx,node_fy,node_fix_x,node_fix_y,summary_node_count,summary_element_count\n") <
        0) {
        write_failed = 1;
    }

    if (!write_failed && (options->sections & FEM_OUTPUT_NODES) != 0u) {
        write_failed = write_csv_nodes(file, model, results, 0);
    }
    if (!write_failed && (options->sections & FEM_OUTPUT_ELEMENTS) != 0u) {
        write_failed = write_csv_elements(file, model, results, 0);
    }
    if (!write_failed && (options->sections & FEM_OUTPUT_REACTIONS) != 0u) {
        write_failed = write_csv_reactions(file, model, results, constrained,
                                           0);
    }
    if (!write_failed && (options->sections & FEM_OUTPUT_SUMMARY) != 0u) {
        write_failed = write_csv_summary(file, model, results, 0);
    }

    return finish_file(file, write_failed);
}

FemStatus write_results_txt(const char *path,
                            const FemModel *model,
                            const FemResults *results)
{
    return write_results_txt_legacy(path, model, results);
}

FemStatus write_results_markdown(const char *path,
                                 const FemModel *model,
                                 const FemResults *results)
{
    return write_results_markdown_legacy(path, model, results);
}

FemStatus write_results_csv(const char *path,
                            const FemModel *model,
                            const FemResults *results)
{
    return write_results_csv_legacy(path, model, results);
}

void print_debug_matrix(const char *name,
                        const double matrix[MAX_DOF][MAX_DOF],
                        int size)
{
    int row;
    int column;

    if (name == NULL || matrix == NULL || size < 1 || size > MAX_DOF) {
        return;
    }

    printf("%s (%dx%d)\n", name, size, size);
    for (row = 0; row < size; ++row) {
        for (column = 0; column < size; ++column) {
            printf(column == 0 ? "%.12g" : " %.12g", matrix[row][column]);
        }
        putchar('\n');
    }
}

void print_debug_vector(const char *name,
                        const double vector[MAX_DOF],
                        int size)
{
    int i;

    if (name == NULL || vector == NULL || size < 1 || size > MAX_DOF) {
        return;
    }

    printf("%s (%d)\n", name, size);
    for (i = 0; i < size; ++i) {
        printf("%.12g\n", vector[i]);
    }
}
