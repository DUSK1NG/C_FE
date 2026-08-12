#include "fem.h"

#include <math.h>
#include <stddef.h>

#include "config.h"
#include "solver.h"

static void clear_global_matrix(double global_k[MAX_DOF][MAX_DOF])
{
    int i;
    int j;

    for (i = 0; i < MAX_DOF; ++i) {
        for (j = 0; j < MAX_DOF; ++j) {
            global_k[i][j] = 0.0;
        }
    }
}

static void clear_force_vector(double force[MAX_DOF])
{
    int i;

    for (i = 0; i < MAX_DOF; ++i) {
        force[i] = 0.0;
    }
}

static void clear_dof_array(int dofs[MAX_DOF])
{
    int i;

    for (i = 0; i < MAX_DOF; ++i) {
        dofs[i] = 0;
    }
}

static FemStatus validate_dof_partition(
    const int free_dofs[MAX_DOF],
    int free_count,
    const int constrained_dofs[MAX_DOF],
    int constrained_count)
{
    int seen[MAX_DOF] = {0};
    int i;
    int dof;

    if (free_count < 0 || constrained_count < 0) {
        return FEM_INVALID_ARGUMENT;
    }
    if (free_count > MAX_DOF || constrained_count > MAX_DOF ||
        free_count + constrained_count > MAX_DOF) {
        return FEM_CAPACITY_EXCEEDED;
    }
    for (i = 0; i < free_count; ++i) {
        dof = free_dofs[i];
        if (dof < 0 || dof >= MAX_DOF || seen[dof] != 0) {
            return FEM_INVALID_ARGUMENT;
        }
        seen[dof] = 1;
    }
    for (i = 0; i < constrained_count; ++i) {
        dof = constrained_dofs[i];
        if (dof < 0 || dof >= MAX_DOF || seen[dof] != 0) {
            return FEM_INVALID_ARGUMENT;
        }
        seen[dof] = 1;
    }
    return FEM_OK;
}

FemStatus calculate_element_geometry(const Node *node_i,
                                     const Node *node_j,
                                     Element *element)
{
    double dx;
    double dy;
    double length;

    if (node_i == NULL || node_j == NULL || element == NULL) {
        return FEM_INVALID_ARGUMENT;
    }

    dx = node_j->x - node_i->x;
    dy = node_j->y - node_i->y;
    length = sqrt(dx * dx + dy * dy);

    if (length < GEOMETRY_TOL) {
        return FEM_ZERO_LENGTH;
    }

    element->length = length;
    element->c = dx / length;
    element->s = dy / length;

    return FEM_OK;
}

FemStatus calculate_element_stiffness(const Element *element,
                                      double ke[4][4])
{
    double factor;
    double c2;
    double s2;
    double cs;
    double values[4][4];
    int i;
    int j;

    if (element == NULL || ke == NULL) {
        return FEM_INVALID_ARGUMENT;
    }

    if (element->E <= 0.0 || element->A <= 0.0) {
        return FEM_INVALID_PROPERTY;
    }

    if (element->length < GEOMETRY_TOL) {
        return FEM_ZERO_LENGTH;
    }

    factor = element->E * element->A / element->length;
    c2 = element->c * element->c;
    s2 = element->s * element->s;
    cs = element->c * element->s;

    /*
     * 该矩阵就是二维桁架单元的全局坐标刚度矩阵。
     * 每个矩阵项对应两个全局自由度之间的刚度耦合。
     */
    values[0][0] = c2;
    values[0][1] = cs;
    values[0][2] = -c2;
    values[0][3] = -cs;

    values[1][0] = cs;
    values[1][1] = s2;
    values[1][2] = -cs;
    values[1][3] = -s2;

    values[2][0] = -c2;
    values[2][1] = -cs;
    values[2][2] = c2;
    values[2][3] = cs;

    values[3][0] = -cs;
    values[3][1] = -s2;
    values[3][2] = cs;
    values[3][3] = s2;

    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            ke[i][j] = factor * values[i][j];
        }
    }

    return FEM_OK;
}

/* 将各单元的刚度贡献按全局自由度映射并累加到总体刚度矩阵。 */
FemStatus assemble_global_stiffness(const Node *nodes,
                                    int node_count,
                                    Element *elements,
                                    int element_count,
                                    double global_k[MAX_DOF][MAX_DOF])
{
    int element_index;
    int a;
    int b;
    int dof_map[4];
    double ke[4][4];
    FemStatus status;

    if (nodes == NULL || elements == NULL || global_k == NULL ||
        node_count <= 0 || element_count <= 0) {
        return FEM_INVALID_ARGUMENT;
    }

    if (node_count > MAX_NODES) {
        return FEM_CAPACITY_EXCEEDED;
    }

    clear_global_matrix(global_k);

    for (element_index = 0; element_index < element_count; ++element_index) {
        if (elements[element_index].node1 < 0 ||
            elements[element_index].node1 >= node_count ||
            elements[element_index].node2 < 0 ||
            elements[element_index].node2 >= node_count) {
            clear_global_matrix(global_k);
            return FEM_INVALID_INDEX;
        }
    }

    for (element_index = 0; element_index < element_count; ++element_index) {
        Element *element = &elements[element_index];

        status = calculate_element_geometry(&nodes[element->node1],
                                            &nodes[element->node2],
                                            element);
        if (status != FEM_OK) {
            clear_global_matrix(global_k);
            return status;
        }

        status = calculate_element_stiffness(element, ke);
        if (status != FEM_OK) {
            clear_global_matrix(global_k);
            return status;
        }

        dof_map[0] = 2 * element->node1;
        dof_map[1] = dof_map[0] + 1;
        dof_map[2] = 2 * element->node2;
        dof_map[3] = dof_map[2] + 1;

        for (a = 0; a < 4; ++a) {
            for (b = 0; b < 4; ++b) {
                global_k[dof_map[a]][dof_map[b]] += ke[a][b];
            }
        }
    }

    return FEM_OK;
}

FemStatus build_force_vector(const Node *nodes,
                             int node_count,
                             double force[MAX_DOF])
{
    int i;

    if (force != NULL) {
        clear_force_vector(force);
    }
    if (nodes == NULL || force == NULL || node_count <= 0) {
        return FEM_INVALID_ARGUMENT;
    }
    if (node_count > MAX_NODES) {
        return FEM_CAPACITY_EXCEEDED;
    }

    for (i = 0; i < node_count; ++i) {
        if (!isfinite(nodes[i].fx) || !isfinite(nodes[i].fy)) {
            return FEM_INVALID_LOAD;
        }
    }

    for (i = 0; i < node_count; ++i) {
        force[2 * i] = nodes[i].fx;
        force[2 * i + 1] = nodes[i].fy;
    }

    return FEM_OK;
}

FemStatus identify_dofs(const Node *nodes,
                        int node_count,
                        int free_dofs[MAX_DOF],
                        int *free_count,
                        int constrained_dofs[MAX_DOF],
                        int *constrained_count)
{
    int i;
    int dof;

    if (free_dofs != NULL) {
        clear_dof_array(free_dofs);
    }
    if (constrained_dofs != NULL) {
        clear_dof_array(constrained_dofs);
    }
    if (free_count != NULL) {
        *free_count = 0;
    }
    if (constrained_count != NULL) {
        *constrained_count = 0;
    }

    if (nodes == NULL || free_dofs == NULL || free_count == NULL ||
        constrained_dofs == NULL || constrained_count == NULL ||
        node_count <= 0) {
        return FEM_INVALID_ARGUMENT;
    }
    if (node_count > MAX_NODES) {
        return FEM_CAPACITY_EXCEEDED;
    }

    for (i = 0; i < node_count; ++i) {
        if ((nodes[i].fix_x != 0 && nodes[i].fix_x != 1) ||
            (nodes[i].fix_y != 0 && nodes[i].fix_y != 1)) {
            return FEM_INVALID_CONSTRAINT;
        }
    }

    for (i = 0; i < node_count; ++i) {
        dof = 2 * i;
        if (nodes[i].fix_x == 0) {
            free_dofs[*free_count] = dof;
            *free_count += 1;
        } else {
            constrained_dofs[*constrained_count] = dof;
            *constrained_count += 1;
        }

        dof += 1;
        if (nodes[i].fix_y == 0) {
            free_dofs[*free_count] = dof;
            *free_count += 1;
        } else {
            constrained_dofs[*constrained_count] = dof;
            *constrained_count += 1;
        }
    }

    return FEM_OK;
}

FemStatus solve_constrained_system(
    const double global_k[MAX_DOF][MAX_DOF],
    const double force[MAX_DOF],
    const int free_dofs[MAX_DOF],
    int free_count,
    const int constrained_dofs[MAX_DOF],
    int constrained_count,
    double displacement[MAX_DOF])
{
    double reduced_matrix[MAX_DOF][MAX_DOF] = {{0.0}};
    double reduced_force[MAX_DOF] = {0.0};
    double free_solution[MAX_DOF] = {0.0};
    FemStatus status;
    int i;
    int j;

    if (displacement != NULL) {
        for (i = 0; i < MAX_DOF; ++i) {
            displacement[i] = 0.0;
        }
    }
    if (global_k == NULL || force == NULL || free_dofs == NULL ||
        constrained_dofs == NULL || displacement == NULL) {
        return FEM_INVALID_ARGUMENT;
    }

    status = validate_dof_partition(free_dofs, free_count,
                                    constrained_dofs, constrained_count);
    if (status != FEM_OK) {
        return status;
    }
    if (free_count == 0) {
        return FEM_OK;
    }

    for (i = 0; i < free_count; ++i) {
        reduced_force[i] = force[free_dofs[i]];
        for (j = 0; j < free_count; ++j) {
            reduced_matrix[i][j] =
                global_k[free_dofs[i]][free_dofs[j]];
        }
    }

    status = solve_linear_system(
                                 (const double (*)[MAX_DOF])reduced_matrix,
                                 reduced_force,
                                 free_count, free_solution);
    if (status != FEM_OK) {
        return status;
    }
    for (i = 0; i < free_count; ++i) {
        displacement[free_dofs[i]] = free_solution[i];
    }
    return FEM_OK;
}

const char *fem_status_message(FemStatus status)
{
    switch (status) {
    case FEM_OK:
        return "success";
    case FEM_INVALID_ARGUMENT:
        return "invalid argument";
    case FEM_ZERO_LENGTH:
        return "zero-length element";
    case FEM_INVALID_PROPERTY:
        return "elastic modulus and area must be positive";
    case FEM_INVALID_INDEX:
        return "invalid node index";
    case FEM_CAPACITY_EXCEEDED:
        return "model exceeds fixed node capacity";
    case FEM_INVALID_CONSTRAINT:
        return "constraint flags must be 0 or 1";
    case FEM_INVALID_LOAD:
        return "loads must be finite";
    case FEM_SINGULAR_MATRIX:
        return "matrix is singular or ill-conditioned";
    case FEM_EQUILIBRIUM_ERROR:
        return "global equilibrium check failed";
    default:
        return "unknown FEM status";
    }
}
