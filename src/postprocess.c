#include <math.h>
#include <stddef.h>

#include "config.h"
#include "postprocess.h"

static void clear_element_result(ElementResult *result)
{
    if (result != NULL) {
        result->elongation = 0.0;
        result->strain = 0.0;
        result->stress = 0.0;
        result->axial_force = 0.0;
        result->state = ELEMENT_NEUTRAL;
    }
}

FemStatus calculate_element_result(
    const Element *element,
    const double displacement[MAX_DOF],
    ElementResult *result)
{
    int i_dof;
    int j_dof;

    clear_element_result(result);

    if (element == NULL || displacement == NULL || result == NULL) {
        return FEM_INVALID_ARGUMENT;
    }

    if (element->node1 < 0 || element->node1 >= MAX_NODES ||
        element->node2 < 0 || element->node2 >= MAX_NODES ||
        element->node1 == element->node2) {
        return FEM_INVALID_INDEX;
    }

    if (!isfinite(element->length)) {
        return FEM_INVALID_ARGUMENT;
    }
    if (element->length < GEOMETRY_TOL) {
        return FEM_ZERO_LENGTH;
    }

    if (!isfinite(element->E) || !isfinite(element->A) ||
        element->E <= 0.0 || element->A <= 0.0) {
        return FEM_INVALID_PROPERTY;
    }

    if (!isfinite(element->c) || !isfinite(element->s)) {
        return FEM_INVALID_ARGUMENT;
    }

    i_dof = 2 * element->node1;
    j_dof = 2 * element->node2;
    if (!isfinite(displacement[i_dof]) ||
        !isfinite(displacement[i_dof + 1]) ||
        !isfinite(displacement[j_dof]) ||
        !isfinite(displacement[j_dof + 1])) {
        return FEM_INVALID_ARGUMENT;
    }

    result->elongation = element->c *
        (displacement[j_dof] - displacement[i_dof]) +
        element->s * (displacement[j_dof + 1] - displacement[i_dof + 1]);
    if (!isfinite(result->elongation)) {
        clear_element_result(result);
        return FEM_INVALID_ARGUMENT;
    }

    result->strain = result->elongation / element->length;
    if (!isfinite(result->strain)) {
        clear_element_result(result);
        return FEM_INVALID_ARGUMENT;
    }

    result->stress = element->E * result->strain;
    if (!isfinite(result->stress)) {
        clear_element_result(result);
        return FEM_INVALID_ARGUMENT;
    }

    result->axial_force = result->stress * element->A;
    if (!isfinite(result->axial_force)) {
        clear_element_result(result);
        return FEM_INVALID_ARGUMENT;
    }

    if (result->axial_force > 0.0) {
        result->state = ELEMENT_TENSION;
    } else if (result->axial_force < 0.0) {
        result->state = ELEMENT_COMPRESSION;
    }

    return FEM_OK;
}
