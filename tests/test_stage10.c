#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "fem.h"
#include "io.h"
#include "output.h"
#include "postprocess.h"
#include "reactions.h"

static int file_contains(const char *path, const char *needle)
{
    FILE *file;
    char buffer[4096];
    size_t bytes_read;

    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[bytes_read] = '\0';
    fclose(file);
    return strstr(buffer, needle) != NULL;
}

static void remove_outputs(const char *tag)
{
    char path[128];

    snprintf(path, sizeof(path), "stage10_%s.txt", tag);
    remove(path);
    snprintf(path, sizeof(path), "stage10_%s.md", tag);
    remove(path);
    snprintf(path, sizeof(path), "stage10_%s.csv", tag);
    remove(path);
}

static void assert_generated_outputs(const char *tag,
                                     int last_node_id,
                                     int last_element_id)
{
    char path[128];
    char node_needle[64];
    char element_needle[64];

    snprintf(node_needle, sizeof(node_needle), "\n%d ", last_node_id);
    snprintf(path, sizeof(path), "stage10_%s.txt", tag);
    assert(file_contains(path, node_needle));
    snprintf(path, sizeof(path), "stage10_%s.md", tag);
    snprintf(node_needle, sizeof(node_needle), "\n| %d |", last_node_id);
    assert(file_contains(path, node_needle));
    snprintf(path, sizeof(path), "stage10_%s.csv", tag);
    snprintf(node_needle, sizeof(node_needle), "NODE,%d,", last_node_id);
    assert(file_contains(path, node_needle));

    snprintf(element_needle, sizeof(element_needle), "\n%d ", last_element_id);
    snprintf(path, sizeof(path), "stage10_%s.txt", tag);
    assert(file_contains(path, element_needle));
    snprintf(path, sizeof(path), "stage10_%s.md", tag);
    snprintf(element_needle, sizeof(element_needle), "\n| %d |", last_element_id);
    assert(file_contains(path, element_needle));
    snprintf(path, sizeof(path), "stage10_%s.csv", tag);
    snprintf(element_needle, sizeof(element_needle), "ELEMENT,%d,", last_element_id);
    assert(file_contains(path, element_needle));
}

static void run_model_case(const char *path,
                           int expected_nodes,
                           int expected_elements,
                           const char *tag)
{
    FemModel model;
    double global_k[MAX_DOF][MAX_DOF];
    double force[MAX_DOF];
    double displacement[MAX_DOF] = {0};
    double reactions[MAX_DOF] = {0};
    int free_dofs[MAX_DOF];
    int constrained_dofs[MAX_DOF];
    int free_count;
    int constrained_count;
    ElementResult element_results[MAX_ELEMENTS];
    FemResults results = {0};
    int i;
    double residual_fx = 0.0;
    double residual_fy = 0.0;
    char output_path[128];

    assert(read_model_file(path, &model) == FEM_OK);
    assert(model.node_count == expected_nodes);
    assert(model.element_count == expected_elements);

    assert(assemble_global_stiffness(model.nodes, model.node_count,
                                     model.elements, model.element_count,
                                     global_k) == FEM_OK);
    assert(build_force_vector(model.nodes, model.node_count, force) == FEM_OK);
    assert(identify_dofs(model.nodes, model.node_count, free_dofs, &free_count,
                         constrained_dofs, &constrained_count) == FEM_OK);
    assert(solve_constrained_system(global_k, force, free_dofs, free_count,
                                    constrained_dofs, constrained_count,
                                    displacement) == FEM_OK);

    for (i = 0; i < model.element_count; ++i) {
        assert(calculate_element_result(&model.elements[i], displacement,
                                        &element_results[i]) == FEM_OK);
        assert(isfinite(element_results[i].elongation));
        assert(isfinite(element_results[i].strain));
        assert(isfinite(element_results[i].stress));
        assert(isfinite(element_results[i].axial_force));
        assert(element_results[i].state == ELEMENT_NEUTRAL ||
               element_results[i].state == ELEMENT_TENSION ||
               element_results[i].state == ELEMENT_COMPRESSION);
    }

    assert(calculate_support_reactions(global_k, force, displacement,
                                       constrained_dofs, constrained_count,
                                       reactions) == FEM_OK);
    assert(check_global_equilibrium(force, reactions, 1.0e-6,
                                    &residual_fx, &residual_fy) == FEM_OK);
    assert(fabs(residual_fx) <= 1.0e-6);
    assert(fabs(residual_fy) <= 1.0e-6);

    for (i = 0; i < MAX_DOF; ++i) {
        results.displacement[i] = displacement[i];
        results.reactions[i] = reactions[i];
    }
    for (i = 0; i < model.element_count; ++i) {
        results.element_results[i] = element_results[i];
    }
    for (i = 0; i < constrained_count; ++i) {
        results.constrained_dofs[i] = constrained_dofs[i];
    }
    results.constrained_count = constrained_count;
    results.residual_fx = residual_fx;
    results.residual_fy = residual_fy;

    snprintf(output_path, sizeof(output_path), "stage10_%s.txt", tag);
    assert(write_results_txt(output_path, &model, &results) == FEM_OK);
    snprintf(output_path, sizeof(output_path), "stage10_%s.md", tag);
    assert(write_results_markdown(output_path, &model, &results) == FEM_OK);
    snprintf(output_path, sizeof(output_path), "stage10_%s.csv", tag);
    assert(write_results_csv(output_path, &model, &results) == FEM_OK);

    assert_generated_outputs(tag, model.nodes[model.node_count - 1].id,
                              model.elements[model.element_count - 1].id);

    remove_outputs(tag);
}

int main(void)
{
    run_model_case("tests/data/medium.model", 6, 8, "medium");
    run_model_case("tests/data/large.model", 10, 20, "large");
    puts("Stage 10 project organization contract tests passed.");
    return 0;
}
