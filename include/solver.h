#ifndef SOLVER_H
#define SOLVER_H

#include "fem.h"

FemStatus solve_linear_system(
    const double matrix[MAX_DOF][MAX_DOF],
    const double rhs[MAX_DOF],
    int size,
    double solution[MAX_DOF]);

#endif
