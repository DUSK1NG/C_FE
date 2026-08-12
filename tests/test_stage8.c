#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "fem.h"
#include "io.h"
#include "reactions.h"

#define TEST_TOL 1.0e-8
#define INVALID_MODEL_PATH "stage8_invalid.model"

#define ASSERT_TRUE(name, condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s\n", (name)); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

#define ASSERT_STATUS(name, actual, expected) \
    do { \
        FemStatus actual_status_ = (actual); \
        if (actual_status_ != (expected)) { \
            fprintf(stderr, "FAIL: %s, actual status = %d, expected = %d\n", \
                    (name), actual_status_, (expected)); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

#define ASSERT_NEAR(name, actual, expected, tolerance) \
    do { \
        double actual_value_ = (actual); \
        if (!isfinite(actual_value_) || \
            fabs(actual_value_ - (expected)) > (tolerance)) { \
            fprintf(stderr, "FAIL: %s, actual = %.12f, expected = %.12f\n", \
                    (name), actual_value_, (double)(expected)); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

static void assert_model_cleared(const char *name, const FemModel *model)
{
    int i;

    ASSERT_TRUE(name, model->node_count == 0);
    ASSERT_TRUE(name, model->element_count == 0);
    for (i = 0; i < MAX_NODES; ++i) {
        ASSERT_TRUE(name, model->nodes[i].id == 0);
        ASSERT_NEAR(name, model->nodes[i].x, 0.0, 0.0);
        ASSERT_NEAR(name, model->nodes[i].y, 0.0, 0.0);
        ASSERT_NEAR(name, model->nodes[i].fx, 0.0, 0.0);
        ASSERT_NEAR(name, model->nodes[i].fy, 0.0, 0.0);
        ASSERT_TRUE(name, model->nodes[i].fix_x == 0);
        ASSERT_TRUE(name, model->nodes[i].fix_y == 0);
    }
    for (i = 0; i < MAX_ELEMENTS; ++i) {
        ASSERT_TRUE(name, model->elements[i].id == 0);
        ASSERT_TRUE(name, model->elements[i].node1 == 0);
        ASSERT_TRUE(name, model->elements[i].node2 == 0);
        ASSERT_NEAR(name, model->elements[i].E, 0.0, 0.0);
        ASSERT_NEAR(name, model->elements[i].A, 0.0, 0.0);
        ASSERT_NEAR(name, model->elements[i].length, 0.0, 0.0);
        ASSERT_NEAR(name, model->elements[i].c, 0.0, 0.0);
        ASSERT_NEAR(name, model->elements[i].s, 0.0, 0.0);
    }
}

static void write_model_file(const char *path, const char *content)
{
    FILE *file = fopen(path, "w");

    ASSERT_TRUE("open model file", file != NULL);
    ASSERT_TRUE("write model file", fputs(content, file) >= 0);
    ASSERT_TRUE("close model file", fclose(file) == 0);
}

static void assert_invalid_content(const char *name, const char *content,
                                   FemStatus expected_status)
{
    FemModel model;

    memset(&model, 0xA5, sizeof(model));
    write_model_file(INVALID_MODEL_PATH, content);
    ASSERT_STATUS(name, read_model_file(INVALID_MODEL_PATH, &model),
                  expected_status);
    assert_model_cleared(name, &model);
    ASSERT_TRUE("remove invalid model file", remove(INVALID_MODEL_PATH) == 0);
}

static void write_long_prefix_model_file(const char *path)
{
    static const char valid_model[] =
        "NODES 2\n"
        "1 0 0\n"
        "2 1 0\n"
        "ELEMENTS 1\n"
        "1 1 2 1 1\n"
        "LOADS 0\n"
        "CONSTRAINTS 0\n";
    FILE *file = fopen(path, "w");
    int i;

    ASSERT_TRUE("open long-prefix model file", file != NULL);
    for (i = 0; i < 600; ++i) {
        ASSERT_TRUE("write long blank line", fputc(' ', file) != EOF);
    }
    ASSERT_TRUE("terminate long blank line", fputc('\n', file) != EOF);
    ASSERT_TRUE("start long comment line", fputc('#', file) != EOF);
    for (i = 0; i < 600; ++i) {
        ASSERT_TRUE("write long comment line", fputc('x', file) != EOF);
    }
    ASSERT_TRUE("terminate long comment line", fputc('\n', file) != EOF);
    ASSERT_TRUE("write valid model after long prefix",
                fputs(valid_model, file) >= 0);
    for (i = 0; i < 600; ++i) {
        ASSERT_TRUE("write trailing long blank line", fputc(' ', file) != EOF);
    }
    ASSERT_TRUE("terminate trailing long blank line", fputc('\n', file) != EOF);
    ASSERT_TRUE("start trailing long comment line", fputc('#', file) != EOF);
    for (i = 0; i < 600; ++i) {
        ASSERT_TRUE("write trailing long comment line", fputc('x', file) != EOF);
    }
    ASSERT_TRUE("terminate trailing long comment line", fputc('\n', file) != EOF);
    ASSERT_TRUE("close long-prefix model file", fclose(file) == 0);
}

static void write_model_with_unterminated_ignored_line(const char *path,
                                                       int line_length,
                                                       int is_comment)
{
    static const char valid_model[] =
        "NODES 2\n"
        "1 0 0\n"
        "2 1 0\n"
        "ELEMENTS 1\n"
        "1 1 2 1 1\n"
        "LOADS 0\n"
        "CONSTRAINTS 0\n";
    FILE *file = fopen(path, "w");
    int i = 0;

    ASSERT_TRUE("open unterminated ignored-line model", file != NULL);
    ASSERT_TRUE("write model before unterminated ignored line",
                fputs(valid_model, file) >= 0);
    if (is_comment) {
        ASSERT_TRUE("start unterminated comment line", fputc('#', file) != EOF);
        i = 1;
    }
    for (; i < line_length; ++i) {
        ASSERT_TRUE("write unterminated ignored line",
                    fputc(is_comment ? 'x' : ' ', file) != EOF);
    }
    ASSERT_TRUE("close unterminated ignored-line model", fclose(file) == 0);
}

static void assert_unterminated_ignored_line_accepted(const char *name,
                                                      int line_length,
                                                      int is_comment)
{
    const char *path = "stage8_unterminated_ignored.model";
    FemModel model;

    write_model_with_unterminated_ignored_line(path, line_length, is_comment);
    ASSERT_STATUS(name, read_model_file(path, &model), FEM_OK);
    ASSERT_TRUE(name, model.node_count == 2);
    ASSERT_TRUE(name, model.element_count == 1);
    ASSERT_TRUE("remove unterminated ignored-line model", remove(path) == 0);
}

static void test_reference_model(void)
{
    const Node expected_nodes[3] = {
        {1, 0.0, 0.0, 0.0, 0.0, 1, 1},
        {2, 1000.0, 0.0, 0.0, 0.0, 0, 1},
        {3, 500.0, 800.0, 0.0, -10000.0, 0, 0}
    };
    const Element expected_elements[3] = {
        {1, 0, 1, 210000.0, 100.0, 0.0, 0.0, 0.0},
        {2, 0, 2, 210000.0, 100.0, 0.0, 0.0, 0.0},
        {3, 1, 2, 210000.0, 100.0, 0.0, 0.0, 0.0}
    };
    FemModel model;
    int i;

    ASSERT_STATUS("read reference model",
                  read_model_file("tests/data/triangle.model", &model), FEM_OK);
    ASSERT_TRUE("reference node count", model.node_count == 3);
    ASSERT_TRUE("reference element count", model.element_count == 3);
    for (i = 0; i < 3; ++i) {
        ASSERT_TRUE("node id", model.nodes[i].id == expected_nodes[i].id);
        ASSERT_NEAR("node x", model.nodes[i].x, expected_nodes[i].x, 0.0);
        ASSERT_NEAR("node y", model.nodes[i].y, expected_nodes[i].y, 0.0);
        ASSERT_NEAR("node fx", model.nodes[i].fx, expected_nodes[i].fx, 0.0);
        ASSERT_NEAR("node fy", model.nodes[i].fy, expected_nodes[i].fy, 0.0);
        ASSERT_TRUE("node fix_x", model.nodes[i].fix_x == expected_nodes[i].fix_x);
        ASSERT_TRUE("node fix_y", model.nodes[i].fix_y == expected_nodes[i].fix_y);
        ASSERT_TRUE("element id", model.elements[i].id == expected_elements[i].id);
        ASSERT_TRUE("element node1", model.elements[i].node1 == expected_elements[i].node1);
        ASSERT_TRUE("element node2", model.elements[i].node2 == expected_elements[i].node2);
        ASSERT_NEAR("element E", model.elements[i].E, expected_elements[i].E, 0.0);
        ASSERT_NEAR("element A", model.elements[i].A, expected_elements[i].A, 0.0);
    }
}

static void test_noncontiguous_user_ids(void)
{
    const char *path = "stage8_noncontiguous.model";
    const char *content =
        "NODES 3\n"
        "10 0 0\n"
        "20 1000 0\n"
        "40 500 800\n"
        "\n"
        "ELEMENTS 2\n"
        "1 10 40 210000 100\n"
        "2 20 40 210000 100\n"
        "\n"
        "LOADS 1\n"
        "40 0 -10000\n"
        "\n"
        "CONSTRAINTS 3\n"
        "10 1 1\n"
        "20 0 1\n"
        "40 0 0\n";
    FemModel model;

    write_model_file(path, content);
    ASSERT_STATUS("read noncontiguous user ids",
                  read_model_file(path, &model), FEM_OK);
    ASSERT_TRUE("noncontiguous node count", model.node_count == 3);
    ASSERT_TRUE("noncontiguous element count", model.element_count == 2);
    ASSERT_TRUE("first noncontiguous node", model.nodes[0].id == 10);
    ASSERT_TRUE("second noncontiguous node", model.nodes[1].id == 20);
    ASSERT_TRUE("third noncontiguous node", model.nodes[2].id == 40);
    ASSERT_NEAR("noncontiguous load fx", model.nodes[2].fx, 0.0, 0.0);
    ASSERT_NEAR("noncontiguous load fy", model.nodes[2].fy, -10000.0, 0.0);
    ASSERT_TRUE("first noncontiguous constraint x", model.nodes[0].fix_x == 1);
    ASSERT_TRUE("first noncontiguous constraint y", model.nodes[0].fix_y == 1);
    ASSERT_TRUE("second noncontiguous constraint x", model.nodes[1].fix_x == 0);
    ASSERT_TRUE("second noncontiguous constraint y", model.nodes[1].fix_y == 1);
    ASSERT_TRUE("third noncontiguous constraint x", model.nodes[2].fix_x == 0);
    ASSERT_TRUE("third noncontiguous constraint y", model.nodes[2].fix_y == 0);
    ASSERT_TRUE("first noncontiguous element node1", model.elements[0].node1 == 0);
    ASSERT_TRUE("first noncontiguous element node2", model.elements[0].node2 == 2);
    ASSERT_TRUE("second noncontiguous element node1", model.elements[1].node1 == 1);
    ASSERT_TRUE("second noncontiguous element node2", model.elements[1].node2 == 2);
    ASSERT_TRUE("remove noncontiguous model file", remove(path) == 0);
}

static void test_reference_model_end_to_end(void)
{
    FemModel model;
    double global_k[MAX_DOF][MAX_DOF] = {{0.0}};
    double force[MAX_DOF] = {0.0};
    double displacement[MAX_DOF] = {0.0};
    double reactions[MAX_DOF] = {0.0};
    int free_dofs[MAX_DOF] = {0};
    int constrained_dofs[MAX_DOF] = {0};
    int free_count = 0;
    int constrained_count = 0;
    double residual_fx = 0.0;
    double residual_fy = 0.0;

    ASSERT_STATUS("read model for end-to-end test",
                  read_model_file("tests/data/triangle.model", &model), FEM_OK);
    ASSERT_STATUS("assemble parsed model",
                  assemble_global_stiffness(model.nodes, model.node_count,
                                            model.elements, model.element_count,
                                            global_k), FEM_OK);
    ASSERT_STATUS("build parsed force vector",
                  build_force_vector(model.nodes, model.node_count, force), FEM_OK);
    ASSERT_STATUS("identify parsed dofs",
                  identify_dofs(model.nodes, model.node_count, free_dofs,
                                &free_count, constrained_dofs,
                                &constrained_count), FEM_OK);
    ASSERT_STATUS("solve parsed model",
                  solve_constrained_system(
                                           (const double (*)[MAX_DOF])global_k,
                                           force, free_dofs, free_count,
                                           constrained_dofs, constrained_count,
                                           displacement), FEM_OK);
    ASSERT_STATUS("calculate parsed reactions",
                  calculate_support_reactions(
                                              (const double (*)[MAX_DOF])global_k,
                                              force, displacement,
                                              constrained_dofs, constrained_count,
                                              reactions), FEM_OK);
    ASSERT_STATUS("check parsed equilibrium",
                  check_global_equilibrium(force, reactions, TEST_TOL,
                                           &residual_fx, &residual_fy), FEM_OK);
    ASSERT_NEAR("node 1 vertical reaction", reactions[1], 5000.0, TEST_TOL);
    ASSERT_NEAR("node 2 vertical reaction", reactions[3], 5000.0, TEST_TOL);
    ASSERT_NEAR("horizontal equilibrium", residual_fx, 0.0, TEST_TOL);
    ASSERT_NEAR("vertical equilibrium", residual_fy, 0.0, TEST_TOL);
}

static void test_invalid_inputs(void)
{
    assert_invalid_content("missing section",
                           "NODES 1\n1 0 0\n\nELEMENTS 1\n1 1 1 1 1\n",
                           FEM_INPUT_ERROR);
    assert_invalid_content("wrong field count",
                           "NODES 1\n1 0\n\nELEMENTS 1\n1 1 1 1 1\n\nLOADS 0\n\nCONSTRAINTS 0\n",
                           FEM_INPUT_ERROR);
    assert_invalid_content("wrong section order",
                           "ELEMENTS 1\n1 1 2 1 1\n\nNODES 2\n1 0 0\n2 1 0\n\nLOADS 0\n\nCONSTRAINTS 0\n",
                           FEM_INPUT_ERROR);
    assert_invalid_content("trailing non-comment content",
                           "NODES 2\n1 0 0\n2 1 0\n\nELEMENTS 1\n1 1 2 1 1\n\nLOADS 0\n\nCONSTRAINTS 0\nextra\n",
                           FEM_INPUT_ERROR);
    assert_invalid_content("duplicate node id",
                           "NODES 2\n1 0 0\n1 1 0\n\nELEMENTS 1\n1 1 1 1 1\n\nLOADS 0\n\nCONSTRAINTS 0\n",
                           FEM_INPUT_ERROR);
    assert_invalid_content("duplicate element id",
                           "NODES 2\n1 0 0\n2 1 0\n\nELEMENTS 2\n1 1 2 1 1\n1 1 2 1 1\n\nLOADS 0\n\nCONSTRAINTS 0\n",
                           FEM_INPUT_ERROR);
    assert_invalid_content("duplicate load",
                           "NODES 2\n1 0 0\n2 1 0\n\nELEMENTS 1\n1 1 2 1 1\n\nLOADS 2\n1 0 1\n1 0 1\n\nCONSTRAINTS 0\n",
                           FEM_INPUT_ERROR);
    assert_invalid_content("duplicate constraint",
                           "NODES 2\n1 0 0\n2 1 0\n\nELEMENTS 1\n1 1 2 1 1\n\nLOADS 0\n\nCONSTRAINTS 2\n1 1 0\n1 0 1\n",
                           FEM_INPUT_ERROR);
    assert_invalid_content("unknown node",
                           "NODES 2\n1 0 0\n2 1 0\n\nELEMENTS 1\n1 1 3 1 1\n\nLOADS 0\n\nCONSTRAINTS 0\n",
                           FEM_INPUT_ERROR);
    assert_invalid_content("self-connected element",
                           "NODES 1\n1 0 0\n\nELEMENTS 1\n1 1 1 1 1\n\nLOADS 0\n\nCONSTRAINTS 0\n",
                           FEM_ZERO_LENGTH);
    assert_invalid_content("invalid modulus",
                           "NODES 2\n1 0 0\n2 1 0\n\nELEMENTS 1\n1 1 2 0 1\n\nLOADS 0\n\nCONSTRAINTS 0\n",
                           FEM_INVALID_PROPERTY);
    assert_invalid_content("invalid area",
                           "NODES 2\n1 0 0\n2 1 0\n\nELEMENTS 1\n1 1 2 1 -1\n\nLOADS 0\n\nCONSTRAINTS 0\n",
                           FEM_INVALID_PROPERTY);
    assert_invalid_content("invalid constraint flag",
                           "NODES 2\n1 0 0\n2 1 0\n\nELEMENTS 1\n1 1 2 1 1\n\nLOADS 0\n\nCONSTRAINTS 1\n1 2 0\n",
                           FEM_INVALID_CONSTRAINT);
    assert_invalid_content("nonfinite load",
                           "NODES 2\n1 0 0\n2 1 0\n\nELEMENTS 1\n1 1 2 1 1\n\nLOADS 1\n1 nan 0\n\nCONSTRAINTS 0\n",
                           FEM_INVALID_LOAD);
    assert_invalid_content("zero-length element",
                           "NODES 2\n1 0 0\n2 0 0\n\nELEMENTS 1\n1 1 2 1 1\n\nLOADS 0\n\nCONSTRAINTS 0\n",
                           FEM_ZERO_LENGTH);
    assert_invalid_content("node capacity exceeded",
                           "NODES 11\n1 0 0\n2 1 0\n3 2 0\n4 3 0\n5 4 0\n6 5 0\n7 6 0\n8 7 0\n9 8 0\n10 9 0\n11 10 0\n",
                           FEM_CAPACITY_EXCEEDED);
    assert_invalid_content("element capacity exceeded",
                           "NODES 2\n1 0 0\n2 1 0\n\nELEMENTS 21\n"
                           "1 1 2 1 1\n2 1 2 1 1\n3 1 2 1 1\n4 1 2 1 1\n"
                           "5 1 2 1 1\n6 1 2 1 1\n7 1 2 1 1\n8 1 2 1 1\n"
                           "9 1 2 1 1\n10 1 2 1 1\n11 1 2 1 1\n12 1 2 1 1\n"
                           "13 1 2 1 1\n14 1 2 1 1\n15 1 2 1 1\n16 1 2 1 1\n"
                           "17 1 2 1 1\n18 1 2 1 1\n19 1 2 1 1\n20 1 2 1 1\n"
                           "21 1 2 1 1\n",
                           FEM_CAPACITY_EXCEEDED);
}

static void test_long_blank_and_comment_lines(void)
{
    const char *path = "stage8_long_prefix.model";
    FemModel model;

    write_long_prefix_model_file(path);
    ASSERT_STATUS("read long blank and comment lines",
                  read_model_file(path, &model), FEM_OK);
    ASSERT_TRUE("long-prefix node count", model.node_count == 2);
    ASSERT_TRUE("long-prefix element count", model.element_count == 1);
    ASSERT_TRUE("remove long-prefix model", remove(path) == 0);
}

static void test_unterminated_ignored_lines_at_buffer_boundaries(void)
{
    assert_unterminated_ignored_line_accepted(
        "511-byte unterminated blank line", 511, 0);
    assert_unterminated_ignored_line_accepted(
        "1022-byte unterminated blank line", 1022, 0);
    assert_unterminated_ignored_line_accepted(
        "511-byte unterminated comment line", 511, 1);
    assert_unterminated_ignored_line_accepted(
        "1022-byte unterminated comment line", 1022, 1);
}

static void test_out_of_range_integer_tokens(void)
{
    assert_invalid_content("out-of-range section count",
                           "NODES 999999999999999999999\n",
                           FEM_INPUT_ERROR);
    assert_invalid_content("out-of-range node id",
                           "NODES 2\n4294967297 0 0\n2 1 0\n\n"
                           "ELEMENTS 1\n1 4294967297 2 1 1\n\n"
                           "LOADS 0\n\nCONSTRAINTS 0\n",
                           FEM_INPUT_ERROR);
    assert_invalid_content("out-of-range constraint flag",
                           "NODES 2\n1 0 0\n2 1 0\n\n"
                           "ELEMENTS 1\n1 1 2 1 1\n\nLOADS 0\n\n"
                           "CONSTRAINTS 1\n1 4294967296 0\n",
                           FEM_INPUT_ERROR);
}

static void test_nonfinite_geometry_is_input_error(void)
{
    assert_invalid_content("nonfinite geometry",
                           "NODES 2\n1 1e308 0\n2 -1e308 0\n\n"
                           "ELEMENTS 1\n1 1 2 1 1\n\nLOADS 0\n\n"
                           "CONSTRAINTS 0\n",
                           FEM_INPUT_ERROR);
}

static void test_missing_file_clears_model(void)
{
    FemModel model;

    memset(&model, 0xA5, sizeof(model));
    ASSERT_STATUS("missing model file",
                  read_model_file("stage8_missing.model", &model), FEM_INPUT_ERROR);
    assert_model_cleared("missing model file output", &model);
}

static void test_status_messages(void)
{
    const char *capacity_message = fem_status_message(FEM_CAPACITY_EXCEEDED);

    ASSERT_TRUE("input error status message",
                strcmp(fem_status_message(FEM_INPUT_ERROR),
                       "invalid model input") == 0);
    ASSERT_TRUE("capacity status message is generic",
                strstr(capacity_message, "capacity") != NULL &&
                strstr(capacity_message, "node") == NULL &&
                strstr(capacity_message, "element") == NULL);
}

int main(int argc, char *argv[])
{
    if (argc == 2 && strcmp(argv[1], "unterminated-boundaries") == 0) {
        test_unterminated_ignored_lines_at_buffer_boundaries();
        printf("Stage 8 unterminated-line boundary regression tests passed.\n");
        return EXIT_SUCCESS;
    }
    ASSERT_TRUE("no unsupported Stage 8 test selector", argc == 1);

    test_reference_model();
    test_noncontiguous_user_ids();
    test_reference_model_end_to_end();
    test_invalid_inputs();
    test_long_blank_and_comment_lines();
    test_status_messages();
    test_unterminated_ignored_lines_at_buffer_boundaries();
    test_out_of_range_integer_tokens();
    test_nonfinite_geometry_is_input_error();
    test_missing_file_clears_model();

    printf("Stage 8 input parser contract tests passed.\n");
    return EXIT_SUCCESS;
}
