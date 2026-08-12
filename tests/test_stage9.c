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

static void test_txt_contract(void)
{
    FemModel model;
    FemResults results;
    char buffer[8192];
    const char *path = "stage9_results.txt";
    const char *expected =
        "2D Truss FEM Results\n\n"
        "Nodal Displacements\n"
        "node_id ux uy\n"
        "10 0 0\n"
        "40 0.125 0.25\n\n"
        "Element Results\n"
        "element_id elongation strain stress axial_force state\n"
        "7 0.275 0.055 11 22 TENSION\n\n"
        "Support Reactions\n"
        "node_id rx ry\n"
        "10 12.5 -6.25\n\n"
        "Equilibrium\n"
        "residual_fx residual_fy\n"
        "1.5e-09 -2.5e-09\n";

    build_fixture(&model, &results);
    assert(write_results_txt(path, &model, &results) == FEM_OK);
    assert(read_file(path, buffer, sizeof(buffer)) != 0);
    assert(strcmp(buffer, expected) == 0);
    assert(remove(path) == 0);
}

static void test_markdown_contract(void)
{
    FemModel model;
    FemResults results;
    char buffer[8192];
    const char *path = "stage9_results.md";
    const char *expected =
        "# 2D Truss FEM Results\n\n"
        "## Nodal Displacements\n"
        "| Node ID | ux | uy |\n"
        "|---:|---:|---:|\n"
        "| 10 | 0 | 0 |\n"
        "| 40 | 0.125 | 0.25 |\n\n"
        "## Element Results\n"
        "| Element ID | Elongation | Strain | Stress | Axial Force | State |\n"
        "|---:|---:|---:|---:|---:|---|\n"
        "| 7 | 0.275 | 0.055 | 11 | 22 | TENSION |\n\n"
        "## Support Reactions\n"
        "| Node ID | rx | ry |\n"
        "|---:|---:|---:|\n"
        "| 10 | 12.5 | -6.25 |\n\n"
        "## Equilibrium\n"
        "| Residual Fx | Residual Fy |\n"
        "|---:|---:|\n"
        "| 1.5e-09 | -2.5e-09 |\n";

    build_fixture(&model, &results);
    assert(write_results_markdown(path, &model, &results) == FEM_OK);
    assert(read_file(path, buffer, sizeof(buffer)) != 0);
    assert(strcmp(buffer, expected) == 0);
    assert(remove(path) == 0);
}

static void test_csv_contract(void)
{
    FemModel model;
    FemResults results;
    char buffer[8192];
    const char *path = "stage9_results.csv";
    const char *expected =
        "record_type,id,ux,uy,elongation,strain,stress,axial_force,state,rx,ry,residual_fx,residual_fy\n"
        "NODE,10,0,0,,,,,,,,,\n"
        "NODE,40,0.125,0.25,,,,,,,,,\n"
        "ELEMENT,7,,,0.275,0.055,11,22,TENSION,,,,\n"
        "REACTION,10,,,,,,,,12.5,-6.25,,\n"
        "SUMMARY,,,,,,,,,,,1.5e-09,-2.5e-09\n";

    build_fixture(&model, &results);
    assert(write_results_csv(path, &model, &results) == FEM_OK);
    assert(read_file(path, buffer, sizeof(buffer)) != 0);
    assert(strcmp(buffer, expected) == 0);
    assert(remove(path) == 0);
}

static void test_element_state_strings(void)
{
    FemModel model;
    FemResults results;
    char buffer[8192];
    const char *txt_path = "stage9_state.txt";
    const char *markdown_path = "stage9_state.md";
    const char *csv_path = "stage9_state.csv";
    const ElementState states[] = {
        ELEMENT_NEUTRAL,
        ELEMENT_TENSION,
        ELEMENT_COMPRESSION
    };
    const char *names[] = {"NEUTRAL", "TENSION", "COMPRESSION"};
    size_t i;

    for (i = 0; i < sizeof(states) / sizeof(states[0]); ++i) {
        build_fixture(&model, &results);
        results.element_results[0].state = states[i];

        assert(write_results_txt(txt_path, &model, &results) == FEM_OK);
        assert(read_file(txt_path, buffer, sizeof(buffer)) != 0);
        assert(strstr(buffer, names[i]) != NULL);
        assert(remove(txt_path) == 0);

        assert(write_results_markdown(markdown_path, &model, &results) == FEM_OK);
        assert(read_file(markdown_path, buffer, sizeof(buffer)) != 0);
        assert(strstr(buffer, names[i]) != NULL);
        assert(remove(markdown_path) == 0);

        assert(write_results_csv(csv_path, &model, &results) == FEM_OK);
        assert(read_file(csv_path, buffer, sizeof(buffer)) != 0);
        assert(strstr(buffer, names[i]) != NULL);
        assert(remove(csv_path) == 0);
    }
}

static void test_one_axis_reaction_outputs(void)
{
    FemModel model;
    FemResults results;
    char buffer[8192];
    const char *txt_path = "stage9_one_axis.txt";
    const char *markdown_path = "stage9_one_axis.md";
    const char *csv_path = "stage9_one_axis.csv";

    build_fixture(&model, &results);
    results.constrained_count = 1;
    results.constrained_dofs[0] = 0;

    assert(write_results_txt(txt_path, &model, &results) == FEM_OK);
    assert(read_file(txt_path, buffer, sizeof(buffer)) != 0);
    assert(strstr(buffer, "10 12.5 0\n") != NULL);
    assert(strstr(buffer, "10 12.5 -6.25\n") == NULL);
    assert(remove(txt_path) == 0);

    assert(write_results_markdown(markdown_path, &model, &results) == FEM_OK);
    assert(read_file(markdown_path, buffer, sizeof(buffer)) != 0);
    assert(strstr(buffer, "| 10 | 12.5 | 0 |\n") != NULL);
    assert(strstr(buffer, "| 10 | 12.5 | -6.25 |\n") == NULL);
    assert(remove(markdown_path) == 0);

    assert(write_results_csv(csv_path, &model, &results) == FEM_OK);
    assert(read_file(csv_path, buffer, sizeof(buffer)) != 0);
    assert(strstr(buffer, "REACTION,10,,,,,,,,12.5,0,,\n") != NULL);
    assert(strstr(buffer, "REACTION,10,,,,,,,,12.5,-6.25,,\n") == NULL);
    assert(remove(csv_path) == 0);
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

static void test_write_failures(void)
{
    FemModel model;
    FemResults results;

    build_fixture(&model, &results);
#ifdef _WIN32
    puts("Stage 9 write-failure test skipped: Windows has no portable deterministic full-device equivalent.");
#else
    assert(write_results_txt("/dev/full", &model, &results) != FEM_OK);
    assert(write_results_markdown("/dev/full", &model, &results) != FEM_OK);
    assert(write_results_csv("/dev/full", &model, &results) != FEM_OK);
#endif
}

static void emit_debug_fixture(void)
{
    const double matrix[MAX_DOF][MAX_DOF] = {
        {12.5, -3.25},
        {4.75, 8.0}
    };
    const double vector[MAX_DOF] = {6.5, -1.25};

    print_debug_matrix("K_original", matrix, 2);
    print_debug_vector("F_original", vector, 2);
}

static void emit_invalid_debug_fixture(void)
{
    const double matrix[MAX_DOF][MAX_DOF] = {{1.0}};
    const double vector[MAX_DOF] = {1.0};

    print_debug_matrix(NULL, matrix, 1);
    print_debug_matrix("null-matrix", NULL, 1);
    print_debug_matrix("zero-matrix", matrix, 0);
    print_debug_matrix("large-matrix", matrix, MAX_DOF + 1);
    print_debug_vector(NULL, vector, 1);
    print_debug_vector("null-vector", NULL, 1);
    print_debug_vector("zero-vector", vector, 0);
    print_debug_vector("large-vector", vector, MAX_DOF + 1);
}

static void run_child_capture(const char *executable_path,
                              const char *argument,
                              const char *path)
{
    char command[1024];
    int length;

#ifdef _WIN32
    length = snprintf(command, sizeof(command),
                      "\"\"%s\" %s > \"%s\"\"",
                      executable_path, argument, path);
#else
    length = snprintf(command, sizeof(command), "\"%s\" %s > \"%s\"",
                      executable_path, argument, path);
#endif
    assert(length > 0 && (size_t)length < sizeof(command));
    assert(system(command) == 0);
}

static void test_debug_contract(const char *executable_path)
{
    char buffer[8192];
    const char *valid_path = "stage9_debug.out";
    const char *invalid_path = "stage9_invalid_debug.out";

    remove(valid_path);
    run_child_capture(executable_path, "--emit-debug", valid_path);
    assert(read_file(valid_path, buffer, sizeof(buffer)) != 0);
    assert(strstr(buffer, "K_original") != NULL);
    assert(strstr(buffer, "K_original (2x2)") != NULL);
    assert(strstr(buffer, "12.5") != NULL);
    assert(strstr(buffer, "-3.25") != NULL);
    assert(strstr(buffer, "4.75") != NULL);
    assert(strstr(buffer, "8") != NULL);
    assert(strstr(buffer, "F_original (2)") != NULL);
    assert(strstr(buffer, "6.5") != NULL);
    assert(strstr(buffer, "-1.25") != NULL);
    assert(remove(valid_path) == 0);

    remove(invalid_path);
    run_child_capture(executable_path, "--emit-invalid-debug", invalid_path);
    assert(read_file(invalid_path, buffer, sizeof(buffer)) != 0);
    assert(buffer[0] == '\0');
    assert(remove(invalid_path) == 0);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--emit-debug") == 0) {
        emit_debug_fixture();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--emit-invalid-debug") == 0) {
        emit_invalid_debug_fixture();
        return 0;
    }

    assert(argc > 0);
    test_txt_contract();
    test_markdown_contract();
    test_csv_contract();
    test_element_state_strings();
    test_one_axis_reaction_outputs();
    test_validation_and_file_errors();
    test_write_failures();
    test_debug_contract(argv[0]);
    puts("Stage 9 results output contract tests passed.");
    return 0;
}
