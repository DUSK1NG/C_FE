#ifndef REACTIONS_H
#define REACTIONS_H

#include "fem.h"

FemStatus calculate_support_reactions(
    const double global_k[MAX_DOF][MAX_DOF],
    const double force[MAX_DOF],
    const double displacement[MAX_DOF],
    const int constrained_dofs[MAX_DOF],
    int constrained_count,
    double reactions[MAX_DOF]);

FemStatus check_global_equilibrium(
    const double force[MAX_DOF],
    const double reactions[MAX_DOF],
    double tolerance,
    double *residual_fx,
    double *residual_fy);

#endif
