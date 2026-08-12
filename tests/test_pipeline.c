#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "io.h"
#include "output.h"
#include "pipeline.h"

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

int main(void)
{
    run_case("tests/data/medium.model", 6, 8);
    run_case("tests/data/large.model", 10, 20);
    puts("Unified pipeline tests passed.");
    return 0;
}
