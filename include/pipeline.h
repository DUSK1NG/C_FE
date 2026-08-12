#ifndef PIPELINE_H
#define PIPELINE_H

#include "io.h"
#include "output.h"

FemStatus run_fem_analysis(const FemModel *model, FemResults *results);

#endif
