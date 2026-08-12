#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#include "fem.h"

typedef enum {
    ELEMENT_NEUTRAL = 0,
    ELEMENT_TENSION,
    ELEMENT_COMPRESSION
} ElementState;

typedef struct {
    double elongation;
    double strain;
    double stress;
    double axial_force;
    ElementState state;
} ElementResult;

FemStatus calculate_element_result(
    const Element *element,
    const double displacement[MAX_DOF],
    ElementResult *result);

#endif
