#include "output.h"

#include <math.h>
#include <stdio.h>

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

static FemStatus validate_output(const char *path,
                                 const FemModel *model,
                                 const FemResults *results,
                                 int constrained[MAX_DOF])
{
    int dof_count;
    int i;

    if (path == NULL || path[0] == '\0' || model == NULL || results == NULL ||
        constrained == NULL) {
        return FEM_INVALID_ARGUMENT;
    }
    if (model->node_count < 1 || model->node_count > MAX_NODES ||
        model->element_count < 0 || model->element_count > MAX_ELEMENTS) {
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

    for (i = 0; i < dof_count; ++i) {
        if (!isfinite(results->displacement[i])) {
            return FEM_INVALID_ARGUMENT;
        }
    }

    for (i = 0; i < results->constrained_count; ++i) {
        int dof = results->constrained_dofs[i];

        if (dof < 0 || dof >= dof_count || constrained[dof] != 0 ||
            !isfinite(results->reactions[dof])) {
            return FEM_INVALID_ARGUMENT;
        }
        constrained[dof] = 1;
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

    if (!isfinite(results->residual_fx) ||
        !isfinite(results->residual_fy)) {
        return FEM_INVALID_ARGUMENT;
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

FemStatus write_results_txt(const char *path,
                            const FemModel *model,
                            const FemResults *results)
{
    int constrained[MAX_DOF];
    FILE *file;
    int i;
    int write_failed = 0;

    if (validate_output(path, model, results, constrained) != FEM_OK) {
        return FEM_INVALID_ARGUMENT;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return FEM_INPUT_ERROR;
    }

    if (fprintf(file,
                "2D Truss FEM Results\n\n"
                "Nodal Displacements\n"
                "node_id ux uy\n") < 0) {
        write_failed = 1;
    }
    for (i = 0; i < model->node_count && !write_failed; ++i) {
        if (fprintf(file, "%d %.12g %.12g\n", model->nodes[i].id,
                    results->displacement[2 * i],
                    results->displacement[2 * i + 1]) < 0) {
            write_failed = 1;
        }
    }

    if (!write_failed &&
        fprintf(file,
                "\nElement Results\n"
                "element_id elongation strain stress axial_force state\n") < 0) {
        write_failed = 1;
    }
    for (i = 0; i < model->element_count && !write_failed; ++i) {
        const ElementResult *element_result = &results->element_results[i];

        if (fprintf(file, "%d %.12g %.12g %.12g %.12g %s\n",
                    model->elements[i].id, element_result->elongation,
                    element_result->strain, element_result->stress,
                    element_result->axial_force,
                    element_state_name(element_result->state)) < 0) {
            write_failed = 1;
        }
    }

    if (!write_failed &&
        fprintf(file,
                "\nSupport Reactions\n"
                "node_id rx ry\n") < 0) {
        write_failed = 1;
    }
    for (i = 0; i < model->node_count && !write_failed; ++i) {
        int x_dof = 2 * i;
        int y_dof = x_dof + 1;

        if (constrained[x_dof] != 0 || constrained[y_dof] != 0) {
            double rx = constrained[x_dof] != 0 ? results->reactions[x_dof] : 0.0;
            double ry = constrained[y_dof] != 0 ? results->reactions[y_dof] : 0.0;

            if (fprintf(file, "%d %.12g %.12g\n", model->nodes[i].id,
                        rx, ry) < 0) {
                write_failed = 1;
            }
        }
    }

    if (!write_failed &&
        fprintf(file,
                "\nEquilibrium\n"
                "residual_fx residual_fy\n"
                "%.12g %.12g\n",
                results->residual_fx, results->residual_fy) < 0) {
        write_failed = 1;
    }

    return finish_file(file, write_failed);
}

FemStatus write_results_markdown(const char *path,
                                 const FemModel *model,
                                 const FemResults *results)
{
    int constrained[MAX_DOF];
    FILE *file;
    int i;
    int write_failed = 0;

    if (validate_output(path, model, results, constrained) != FEM_OK) {
        return FEM_INVALID_ARGUMENT;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return FEM_INPUT_ERROR;
    }

    if (fprintf(file,
                "# 2D Truss FEM Results\n\n"
                "## Nodal Displacements\n"
                "| Node ID | ux | uy |\n"
                "|---:|---:|---:|\n") < 0) {
        write_failed = 1;
    }
    for (i = 0; i < model->node_count && !write_failed; ++i) {
        if (fprintf(file, "| %d | %.12g | %.12g |\n", model->nodes[i].id,
                    results->displacement[2 * i],
                    results->displacement[2 * i + 1]) < 0) {
            write_failed = 1;
        }
    }

    if (!write_failed &&
        fprintf(file,
                "\n## Element Results\n"
                "| Element ID | Elongation | Strain | Stress | Axial Force | State |\n"
                "|---:|---:|---:|---:|---:|---|\n") < 0) {
        write_failed = 1;
    }
    for (i = 0; i < model->element_count && !write_failed; ++i) {
        const ElementResult *element_result = &results->element_results[i];

        if (fprintf(file, "| %d | %.12g | %.12g | %.12g | %.12g | %s |\n",
                    model->elements[i].id, element_result->elongation,
                    element_result->strain, element_result->stress,
                    element_result->axial_force,
                    element_state_name(element_result->state)) < 0) {
            write_failed = 1;
        }
    }

    if (!write_failed &&
        fprintf(file,
                "\n## Support Reactions\n"
                "| Node ID | rx | ry |\n"
                "|---:|---:|---:|\n") < 0) {
        write_failed = 1;
    }
    for (i = 0; i < model->node_count && !write_failed; ++i) {
        int x_dof = 2 * i;
        int y_dof = x_dof + 1;

        if (constrained[x_dof] != 0 || constrained[y_dof] != 0) {
            double rx = constrained[x_dof] != 0 ? results->reactions[x_dof] : 0.0;
            double ry = constrained[y_dof] != 0 ? results->reactions[y_dof] : 0.0;

            if (fprintf(file, "| %d | %.12g | %.12g |\n",
                        model->nodes[i].id, rx, ry) < 0) {
                write_failed = 1;
            }
        }
    }

    if (!write_failed &&
        fprintf(file,
                "\n## Equilibrium\n"
                "| Residual Fx | Residual Fy |\n"
                "|---:|---:|\n"
                "| %.12g | %.12g |\n",
                results->residual_fx, results->residual_fy) < 0) {
        write_failed = 1;
    }

    return finish_file(file, write_failed);
}

FemStatus write_results_csv(const char *path,
                            const FemModel *model,
                            const FemResults *results)
{
    int constrained[MAX_DOF];
    FILE *file;
    int i;
    int write_failed = 0;

    if (validate_output(path, model, results, constrained) != FEM_OK) {
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
    for (i = 0; i < model->node_count && !write_failed; ++i) {
        if (fprintf(file, "NODE,%d,%.12g,%.12g,,,,,,,,,\n",
                    model->nodes[i].id, results->displacement[2 * i],
                    results->displacement[2 * i + 1]) < 0) {
            write_failed = 1;
        }
    }

    for (i = 0; i < model->element_count && !write_failed; ++i) {
        const ElementResult *element_result = &results->element_results[i];

        if (fprintf(file, "ELEMENT,%d,,,%.12g,%.12g,%.12g,%.12g,%s,,,,\n",
                    model->elements[i].id, element_result->elongation,
                    element_result->strain, element_result->stress,
                    element_result->axial_force,
                    element_state_name(element_result->state)) < 0) {
            write_failed = 1;
        }
    }

    for (i = 0; i < model->node_count && !write_failed; ++i) {
        int x_dof = 2 * i;
        int y_dof = x_dof + 1;

        if (constrained[x_dof] != 0 || constrained[y_dof] != 0) {
            double rx = constrained[x_dof] != 0 ? results->reactions[x_dof] : 0.0;
            double ry = constrained[y_dof] != 0 ? results->reactions[y_dof] : 0.0;

            if (fprintf(file, "REACTION,%d,,,,,,,,%.12g,%.12g,,\n",
                        model->nodes[i].id, rx, ry) < 0) {
                write_failed = 1;
            }
        }
    }

    if (!write_failed &&
        fprintf(file, "SUMMARY,,,,,,,,,,,%.12g,%.12g\n",
                results->residual_fx, results->residual_fy) < 0) {
        write_failed = 1;
    }

    return finish_file(file, write_failed);
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
