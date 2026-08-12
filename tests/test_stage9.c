#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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

static void assert_txt_node_row(const char *buffer, int expected_id,
                                double expected_ux, double expected_uy)
{
    const char *section;
    const char *section_end;
    const char *line;

    section = strstr(buffer, "Nodal Displacements");
    assert(section != NULL);
    section_end = strstr(section, "Element Results");
    assert(section_end != NULL);

    line = section;
    while (line < section_end) {
        int id;
        double ux;
        double uy;
        int consumed;
        const char *tail;

        consumed = 0;
        if (sscanf(line, "%d %lf %lf%n", &id, &ux, &uy, &consumed) == 3) {
            tail = line + consumed;
            while (tail < section_end && (*tail == ' ' || *tail == '\t' ||
                                           *tail == '\r')) {
                ++tail;
            }
            if ((tail == section_end || *tail == '\n') && id == expected_id &&
                ux == expected_ux && uy == expected_uy) {
                return;
            }
        }
        line = strchr(line, '\n');
        if (line == NULL || line >= section_end) {
            break;
        }
        ++line;
    }

    assert(0 && "expected node row missing from TXT displacement table");
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
    assert_txt_node_row(buffer, 10, 0.0, 0.0);
    assert_txt_node_row(buffer, 40, 0.125, 0.25);
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
    assert(strstr(buffer, "| 10 | 0 | 0 |") != NULL);
    assert(strstr(buffer, "| 40 | 0.125 | 0.25 |") != NULL);
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
    assert(strstr(buffer, "\nNODE,10,0,0,") != NULL);
    assert(strstr(buffer, "\nNODE,40,0.125,0.25,") != NULL);
    assert(strstr(buffer, "ELEMENT,7") != NULL);
    assert(strstr(buffer, "REACTION,10") != NULL);
    assert(strstr(buffer, "SUMMARY") != NULL);
    assert(strstr(buffer, "\nNODE,0,") == NULL);
    assert(strstr(buffer, "\nNODE,1,") == NULL);
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
    FILE *blocking_file;
    const char *blocking_path = "stage9_not_a_directory";
    const char *unwritable_path = "stage9_not_a_directory/output.txt";

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
    invalid_results.element_results[0].elongation = NAN;
    assert_all_writers_reject("stage9_invalid.txt", &model, &invalid_results);
    invalid_results = results;
    invalid_results.element_results[0].strain = INFINITY;
    assert_all_writers_reject("stage9_invalid.txt", &model, &invalid_results);
    invalid_results = results;
    invalid_results.element_results[0].stress = INFINITY;
    assert_all_writers_reject("stage9_invalid.txt", &model, &invalid_results);
    invalid_results = results;
    invalid_results.element_results[0].axial_force = NAN;
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

    remove(blocking_path);
    blocking_file = fopen(blocking_path, "wb");
    assert(blocking_file != NULL);
    assert(fclose(blocking_file) == 0);
    assert_all_writers_reject(unwritable_path, &model, &results);
    assert(remove(blocking_path) == 0);
}

static void emit_debug_fixture(void)
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

static void test_debug_contract(const char *executable_path)
{
    char command[1024];
    char buffer[8192];
    const char *path = "stage9_debug.out";
    int length;

    remove(path);
    length = snprintf(command, sizeof(command), "\"%s\" --emit-debug > \"%s\"",
                      executable_path, path);
    assert(length > 0 && (size_t)length < sizeof(command));
    assert(system(command) == 0);
    assert(read_file(path, buffer, sizeof(buffer)) != 0);
    assert(strstr(buffer, "K_original") != NULL);
    assert(strstr(buffer, "2x2") != NULL);
    assert(strstr(buffer, "12.5") != NULL);
    assert(strstr(buffer, "-3.25") != NULL);
    assert(strstr(buffer, "4.75") != NULL);
    assert(strstr(buffer, "8") != NULL);
    assert(strstr(buffer, "F_original") != NULL);
    assert(strstr(buffer, "6.5") != NULL);
    assert(strstr(buffer, "-1.25") != NULL);
    assert(remove(path) == 0);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--emit-debug") == 0) {
        emit_debug_fixture();
        return 0;
    }

    assert(argc > 0);
    test_txt_contract();
    test_markdown_contract();
    test_csv_contract();
    test_validation_and_file_errors();
    test_debug_contract(argv[0]);
    puts("Stage 9 results output contract tests passed.");
    return 0;
}
