#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "io.h"
#include "output.h"
#include "pipeline.h"

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

static void assert_contains(const char *buffer, const char *needle)
{
    assert(strstr(buffer, needle) != NULL);
}

static void assert_not_contains(const char *buffer, const char *needle)
{
    assert(strstr(buffer, needle) == NULL);
}

static void test_selected_sections_and_legacy_wrapper(void)
{
    FemModel model;
    FemResults results = {0};
    FemOutputOptions options;
    char buffer[32768];
    const char *txt_path = "selected_results.txt";
    const char *markdown_path = "selected_results.md";
    const char *csv_path = "selected_results.csv";
    const char *legacy_path = "legacy_results.txt";

    assert(read_model_file("tests/data/medium.model", &model) == FEM_OK);
    assert(run_fem_analysis(&model, &results) == FEM_OK);

    options.sections = FEM_OUTPUT_NODES | FEM_OUTPUT_SUMMARY;

    assert(write_results_txt_selected(txt_path, &model, &results, &options) ==
           FEM_OK);
    assert(read_file(txt_path, buffer, sizeof(buffer)) != 0);
    assert_contains(buffer, "Nodal Displacements");
    assert_contains(buffer, "Equilibrium");
    assert_not_contains(buffer, "Element Results");
    assert_not_contains(buffer, "Support Reactions");
    assert(remove(txt_path) == 0);

    assert(write_results_markdown_selected(markdown_path, &model, &results,
                                           &options) == FEM_OK);
    assert(read_file(markdown_path, buffer, sizeof(buffer)) != 0);
    assert_contains(buffer, "## Nodal Displacements");
    assert_contains(buffer, "## Equilibrium");
    assert_not_contains(buffer, "## Element Results");
    assert_not_contains(buffer, "## Support Reactions");
    assert(remove(markdown_path) == 0);

    assert(write_results_csv_selected(csv_path, &model, &results, &options) ==
           FEM_OK);
    assert(read_file(csv_path, buffer, sizeof(buffer)) != 0);
    assert_contains(buffer, "NODE,");
    assert_contains(buffer, "SUMMARY,");
    assert_not_contains(buffer, "ELEMENT,");
    assert_not_contains(buffer, "REACTION,");
    assert(remove(csv_path) == 0);

    assert(write_results_txt(legacy_path, &model, &results) == FEM_OK);
    assert(read_file(legacy_path, buffer, sizeof(buffer)) != 0);
    assert_contains(buffer, "Nodal Displacements");
    assert_contains(buffer, "Element Results");
    assert_contains(buffer, "Support Reactions");
    assert_contains(buffer, "Equilibrium");
    assert(remove(legacy_path) == 0);
}

static void test_selected_writer_validation(void)
{
    FemModel model;
    FemResults results = {0};
    FemOutputOptions options;

    assert(read_model_file("tests/data/medium.model", &model) == FEM_OK);
    assert(run_fem_analysis(&model, &results) == FEM_OK);

    options.sections = FEM_OUTPUT_NODES;
    assert(write_results_txt_selected(NULL, &model, &results, &options) ==
           FEM_INVALID_ARGUMENT);
    assert(write_results_txt_selected("selected_invalid.txt", NULL, &results,
                                      &options) == FEM_INVALID_ARGUMENT);
    assert(write_results_txt_selected("selected_invalid.txt", &model, NULL,
                                      &options) == FEM_INVALID_ARGUMENT);
    assert(write_results_txt_selected("selected_invalid.txt", &model, &results,
                                      NULL) == FEM_INVALID_ARGUMENT);

    options.sections = 0u;
    assert(write_results_txt_selected("selected_invalid.txt", &model, &results,
                                      &options) == FEM_INVALID_ARGUMENT);
    options.sections = FEM_OUTPUT_NODES | (1u << 7);
    assert(write_results_txt_selected("selected_invalid.txt", &model, &results,
                                      &options) == FEM_INVALID_ARGUMENT);
    remove("selected_invalid.txt");
}

int main(void)
{
    test_selected_sections_and_legacy_wrapper();
    test_selected_writer_validation();
    puts("Selected output section tests passed.");
    return 0;
}
