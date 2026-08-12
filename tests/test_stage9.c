#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "io.h"
#include "output.h"
#include "postprocess.h"

typedef FemStatus (*WriteResults)(const char *, const FemModel *,
                                  const FemResults *);

static void build_fixture(FemModel *model, FemResults *results)
{
    int i;

    memset(model, 0, sizeof(*model));
    memset(results, 0, sizeof(*results));

    model->node_count = 2;
    model->nodes[0].id = 10;
    model->nodes[0].x = 0.0;
    model->nodes[0].y = 0.0;
    model->nodes[1].id = 40;
    model->nodes[1].x = 3.0;
    model->nodes[1].y = 4.0;

    model->element_count = 1;
    model->elements[0].id = 7;
    model->elements[0].node1 = 0;
    model->elements[0].node2 = 1;
    model->elements[0].E = 200.0;
    model->elements[0].A = 2.0;
    model->elements[0].length = 5.0;
    model->elements[0].c = 0.6;
    model->elements[0].s = 0.8;

    results->displacement[0] = 0.0;
    results->displacement[1] = 0.0;
    results->displacement[2] = 0.125;
    results->displacement[3] = 0.25;
    results->reactions[0] = 12.5;
    results->reactions[1] = -6.25;
    results->reactions[2] = 0.0;
    results->reactions[3] = 0.0;

    results->element_results[0].elongation = 0.275;
    results->element_results[0].strain = 0.055;
    results->element_results[0].stress = 11.0;
    results->element_results[0].axial_force = 22.0;
    results->element_results[0].state = ELEMENT_TENSION;

    results->constrained_dofs[0] = 0;
    results->constrained_dofs[1] = 1;
    results->constrained_count = 2;
    results->residual_fx = 1.5e-9;
    results->residual_fy = -2.5e-9;

    for (i = 4; i < MAX_DOF; ++i) {
        results->displacement[i] = 0.0;
        results->reactions[i] = 0.0;
    }
}

static int read_file(const char *path, char *buffer, size_t capacity)
{
    FILE *file;
    size_t length;

    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    length = fread(buffer, 1, capacity - 1, file);
    buffer[length] = '\0';
    if (ferror(file) != 0) {
        fclose(file);
        return 0;
    }
    fclose(file);
    return 1;
}

static void assert_file_contains(const char *path, const char *text)
{
    char buffer[8192];

    assert(read_file(path, buffer, sizeof(buffer)) != 0);
    assert(strstr(buffer, text) != NULL);
}

static void test_txt_contract(void)
{
    FemModel model;
    FemResults results;
    char buffer[8192];
    const char *path = "stage9_results.txt";

    build_fixture(&model, &results);
    assert(write_results_txt(path, &model, &results) == FEM_OK);
    assert(read_file(path, buffer, sizeof(buffer)) != 0);
    assert(strstr(buffer, "2D Truss FEM Results") != NULL);
    assert(strstr(buffer, "Nodal Displacements") != NULL);
    assert(strstr(buffer, "10") != NULL);
    assert(strstr(buffer, "Element Results") != NULL);
    assert(strstr(buffer, "TENSION") != NULL);
    assert(strstr(buffer, "Support Reactions") != NULL);
    assert(strstr(buffer, "residual_fx") != NULL);
    assert(strstr(buffer, "residual_fy") != NULL);
    assert(strstr(buffer, "1.5e-09") != NULL);
    assert(strstr(buffer, "-2.5e-09") != NULL);
    assert(remove(path) == 0);
}

static void test_markdown_contract(void)
{
    FemModel model;
    FemResults results;
    char buffer[8192];
    const char *path = "stage9_results.md";

    build_fixture(&model, &results);
    assert(write_results_markdown(path, &model, &results) == FEM_OK);
    assert(read_file(path, buffer, sizeof(buffer)) != 0);
    assert(strstr(buffer, "# 2D Truss FEM Results") != NULL);
    assert(strstr(buffer, "## Nodal Displacements") != NULL);
    assert(strstr(buffer, "## Element Results") != NULL);
    assert(strstr(buffer, "## Support Reactions") != NULL);
    assert(strstr(buffer, "## Equilibrium") != NULL);
    assert(strstr(buffer, "| Node ID | ux | uy |") != NULL);
    assert(strstr(buffer, "| Element ID | Elongation | Strain | Stress | Axial Force | State |") != NULL);
    assert(strstr(buffer, "| 10 |") != NULL);
    assert(strstr(buffer, "TENSION") != NULL);
    assert(remove(path) == 0);
}

static void test_csv_contract(void)
{
    FemModel model;
    FemResults results;
    char buffer[8192];
    const char *path = "stage9_results.csv";
    const char *header =
        "record_type,id,ux,uy,elongation,strain,stress,axial_force,state,rx,ry,residual_fx,residual_fy\n";

    build_fixture(&model, &results);
    assert(write_results_csv(path, &model, &results) == FEM_OK);
    assert(read_file(path, buffer, sizeof(buffer)) != 0);
    assert(strncmp(buffer, header, strlen(header)) == 0);
    assert(strstr(buffer, "NODE,10") != NULL);
    assert(strstr(buffer, "ELEMENT,7") != NULL);
    assert(strstr(buffer, "REACTION,10") != NULL);
    assert(strstr(buffer, "SUMMARY") != NULL);
    assert(strstr(buffer, "NODE,0") == NULL);
    assert(remove(path) == 0);
}

static void assert_all_writers_reject(const char *path,
                                      const FemModel *model,
                                      const FemResults *results)
{
    WriteResults writers[] = {
        write_results_txt,
        write_results_markdown,
        write_results_csv
    };
    size_t i;

    for (i = 0; i < sizeof(writers) / sizeof(writers[0]); ++i) {
        assert(writers[i](path, model, results) != FEM_OK);
    }
    if (path != NULL) {
        remove(path);
    }
}

static void test_validation_and_file_errors(void)
{
    FemModel model;
    FemResults results;
    FemModel invalid_model;
    FemResults invalid_results;

    build_fixture(&model, &results);

    assert_all_writers_reject(NULL, &model, &results);
    assert_all_writers_reject("stage9_invalid.txt", NULL, &results);
    assert_all_writers_reject("stage9_invalid.txt", &model, NULL);
    assert_all_writers_reject("stage9_invalid.txt", NULL, NULL);

    invalid_model = model;
    invalid_model.node_count = -1;
    assert_all_writers_reject("stage9_invalid.txt", &invalid_model, &results);
    invalid_model = model;
    invalid_model.node_count = 0;
    assert_all_writers_reject("stage9_invalid.txt", &invalid_model, &results);
    invalid_model.node_count = MAX_NODES + 1;
    assert_all_writers_reject("stage9_invalid.txt", &invalid_model, &results);
    invalid_model = model;
    invalid_model.element_count = -1;
    assert_all_writers_reject("stage9_invalid.txt", &invalid_model, &results);
    invalid_model.element_count = MAX_ELEMENTS + 1;
    assert_all_writers_reject("stage9_invalid.txt", &invalid_model, &results);

    invalid_results = results;
    invalid_results.constrained_count = -1;
    assert_all_writers_reject("stage9_invalid.txt", &model, &invalid_results);
    invalid_results = results;
    invalid_results.constrained_count = MAX_DOF + 1;
    assert_all_writers_reject("stage9_invalid.txt", &model, &invalid_results);
    invalid_results = results;
    invalid_results.constrained_dofs[1] = invalid_results.constrained_dofs[0];
    assert_all_writers_reject("stage9_invalid.txt", &model, &invalid_results);
    invalid_results = results;
    invalid_results.constrained_dofs[0] = -1;
    assert_all_writers_reject("stage9_invalid.txt", &model, &invalid_results);
    invalid_results = results;
    invalid_results.constrained_dofs[0] = MAX_DOF;
    assert_all_writers_reject("stage9_invalid.txt", &model, &invalid_results);
    invalid_results = results;
    invalid_results.displacement[0] = NAN;
    assert_all_writers_reject("stage9_invalid.txt", &model, &invalid_results);
    invalid_results = results;
    invalid_results.element_results[0].stress = INFINITY;
    assert_all_writers_reject("stage9_invalid.txt", &model, &invalid_results);
    invalid_results = results;
    invalid_results.reactions[0] = NAN;
    assert_all_writers_reject("stage9_invalid.txt", &model, &invalid_results);
    invalid_results = results;
    invalid_results.element_results[0].state = (ElementState)99;
    assert_all_writers_reject("stage9_invalid.txt", &model, &invalid_results);
    invalid_results = results;
    invalid_results.residual_fx = INFINITY;
    assert_all_writers_reject("stage9_invalid.txt", &model, &invalid_results);
    invalid_results = results;
    invalid_results.residual_fy = NAN;
    assert_all_writers_reject("stage9_invalid.txt", &model, &invalid_results);

    assert_all_writers_reject("missing-parent-stage9/output.txt", &model,
                              &results);
}

static void test_debug_contract(void)
{
    double matrix[MAX_DOF][MAX_DOF] = {{0.0}};
    double vector[MAX_DOF] = {0.0};

    matrix[0][0] = 12.5;
    matrix[0][1] = -3.25;
    matrix[1][0] = 4.75;
    matrix[1][1] = 8.0;
    vector[0] = 6.5;
    vector[1] = -1.25;

    print_debug_matrix("K_original", matrix, 2);
    print_debug_vector("F_original", vector, 2);
}

int main(void)
{
    test_txt_contract();
    test_markdown_contract();
    test_csv_contract();
    test_validation_and_file_errors();
    test_debug_contract();
    puts("Stage 9 results output contract tests passed.");
    return 0;
}
