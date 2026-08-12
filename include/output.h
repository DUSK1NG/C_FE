#ifndef OUTPUT_H
#define OUTPUT_H

#include "config.h"
#include "fem.h"
#include "io.h"
#include "postprocess.h"

typedef struct {
    double displacement[MAX_DOF];
    double reactions[MAX_DOF];
    ElementResult element_results[MAX_ELEMENTS];
    int constrained_dofs[MAX_DOF];
    int constrained_count;
    double residual_fx;
    double residual_fy;
} FemResults;

typedef enum {
    FEM_OUTPUT_NODES = 1u << 0,
    FEM_OUTPUT_ELEMENTS = 1u << 1,
    FEM_OUTPUT_REACTIONS = 1u << 2,
    FEM_OUTPUT_SUMMARY = 1u << 3
} FemOutputSection;

typedef struct {
    unsigned sections;
} FemOutputOptions;

typedef enum {
    FEM_FORMAT_TXT = 1u << 0,
    FEM_FORMAT_MARKDOWN = 1u << 1,
    FEM_FORMAT_CSV = 1u << 2
} FemOutputFormat;

FemStatus write_results_txt_selected(const char *path,
                                     const FemModel *model,
                                     const FemResults *results,
                                     const FemOutputOptions *options);

FemStatus write_results_markdown_selected(const char *path,
                                          const FemModel *model,
                                          const FemResults *results,
                                          const FemOutputOptions *options);

FemStatus write_results_csv_selected(const char *path,
                                     const FemModel *model,
                                     const FemResults *results,
                                     const FemOutputOptions *options);

FemStatus write_results_txt(const char *path,
                            const FemModel *model,
                            const FemResults *results);

FemStatus write_results_markdown(const char *path,
                                 const FemModel *model,
                                 const FemResults *results);

FemStatus write_results_csv(const char *path,
                            const FemModel *model,
                            const FemResults *results);

void print_debug_matrix(const char *name,
                        const double matrix[MAX_DOF][MAX_DOF],
                        int size);

void print_debug_vector(const char *name,
                        const double vector[MAX_DOF],
                        int size);

#endif
