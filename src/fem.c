#include "fem.h"

#include <math.h>
#include <stddef.h>

#include "config.h"

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
    default:
        return "unknown FEM status";
    }
}
