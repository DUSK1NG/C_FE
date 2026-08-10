#include "fem.h"

#include <math.h>
#include <stddef.h>

#include "config.h"

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
    default:
        return "unknown FEM status";
    }
}
