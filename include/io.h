#ifndef IO_H
#define IO_H

#include "config.h"
#include "model.h"
#include "fem.h"

typedef struct {
    Node nodes[MAX_NODES];
    int node_count;
    Element elements[MAX_ELEMENTS];
    int element_count;
} FemModel;

FemStatus read_model_file(const char *path, FemModel *model);

#endif
