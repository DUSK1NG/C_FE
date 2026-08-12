#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "io.h"
#include "output.h"
#include "pipeline.h"

static void fill_results(FemResults *results)
{
    memset(results, 0xA5, sizeof(*results));
}

static void assert_results_cleared(const FemResults *results)
{
    int i;

    for (i = 0; i < MAX_DOF; ++i) {
        assert(results->displacement[i] == 0.0);
        assert(results->reactions[i] == 0.0);
        assert(results->constrained_dofs[i] == 0);
    }
    for (i = 0; i < MAX_ELEMENTS; ++i) {
        assert(results->element_results[i].elongation == 0.0);
        assert(results->element_results[i].strain == 0.0);
        assert(results->element_results[i].stress == 0.0);
        assert(results->element_results[i].axial_force == 0.0);
        assert(results->element_results[i].state == ELEMENT_NEUTRAL);
    }
    assert(results->constrained_count == 0);
    assert(results->residual_fx == 0.0);
    assert(results->residual_fy == 0.0);
}

static void run_case(const char *path, int nodes, int elements)
{
    FemModel model;
    FemResults results = {0};
    int i;

    assert(read_model_file(path, &model) == FEM_OK);
    assert(model.node_count == nodes);
    assert(model.element_count == elements);
    assert(run_fem_analysis(&model, &results) == FEM_OK);
    assert(results.constrained_count >= 0);
    assert(isfinite(results.residual_fx));
    assert(isfinite(results.residual_fy));
    for (i = 0; i < model.element_count; ++i) {
        assert(isfinite(results.element_results[i].axial_force));
    }
}

static void run_failure_cases(void)
{
    FemModel model = {0};
    FemResults results;

    fill_results(&results);
    assert(run_fem_analysis(NULL, &results) == FEM_INVALID_ARGUMENT);
    assert_results_cleared(&results);

    fill_results(&results);
    assert(run_fem_analysis(&model, NULL) == FEM_INVALID_ARGUMENT);

    model.node_count = MAX_NODES + 1;
    model.element_count = 1;
    fill_results(&results);
    assert(run_fem_analysis(&model, &results) == FEM_CAPACITY_EXCEEDED);
    assert_results_cleared(&results);

    model.node_count = 1;
    model.element_count = MAX_ELEMENTS + 1;
    fill_results(&results);
    assert(run_fem_analysis(&model, &results) == FEM_CAPACITY_EXCEEDED);
    assert_results_cleared(&results);
}

int main(void)
{
    run_case("tests/data/medium.model", 6, 8);
    run_case("tests/data/large.model", 10, 20);
    run_failure_cases();
    puts("Unified pipeline tests passed.");
    return 0;
}
