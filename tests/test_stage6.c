#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "postprocess.h"

#define TEST_TOL 1.0e-12

static void expect_status(const char *name, FemStatus actual, FemStatus expected)
{
    if (actual != expected) {
        fprintf(stderr, "FAIL: %s, actual status = %d, expected status = %d\n",
                name, actual, expected);
        exit(EXIT_FAILURE);
    }
}

static void expect_close(const char *name, double actual, double expected)
{
    if (!isfinite(actual) || !isfinite(expected) ||
        fabs(actual - expected) > TEST_TOL) {
        fprintf(stderr, "FAIL: %s, actual = %.12f, expected = %.12f\n",
                name, actual, expected);
        exit(EXIT_FAILURE);
    }
}

static void expect_result_zero(const char *name, const ElementResult *result)
{
    expect_close(name, result->elongation, 0.0);
    expect_close(name, result->strain, 0.0);
    expect_close(name, result->stress, 0.0);
    expect_close(name, result->axial_force, 0.0);
    if (result->state != ELEMENT_NEUTRAL) {
        fprintf(stderr, "FAIL: %s state is not neutral\n", name);
        exit(EXIT_FAILURE);
    }
}

static void expect_bytes_unchanged(const char *name,
                                   const void *actual,
                                   const void *expected,
                                   size_t size)
{
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "FAIL: %s changed\n", name);
        exit(EXIT_FAILURE);
    }
}

static void expect_result(const char *name,
                          const ElementResult *result,
                          double elongation,
                          double strain,
                          double stress,
                          double axial_force,
                          ElementState state,
                          double tolerance)
{
    if (!isfinite(result->elongation) ||
        fabs(result->elongation - elongation) > tolerance) {
        fprintf(stderr, "FAIL: %s elongation = %.12f, expected = %.12f\n",
                name, result->elongation, elongation);
        exit(EXIT_FAILURE);
    }
    if (!isfinite(result->strain) || fabs(result->strain - strain) > tolerance) {
        fprintf(stderr, "FAIL: %s strain = %.12f, expected = %.12f\n",
                name, result->strain, strain);
        exit(EXIT_FAILURE);
    }
    if (!isfinite(result->stress) || fabs(result->stress - stress) > tolerance) {
        fprintf(stderr, "FAIL: %s stress = %.12f, expected = %.12f\n",
                name, result->stress, stress);
        exit(EXIT_FAILURE);
    }
    if (!isfinite(result->axial_force) ||
        fabs(result->axial_force - axial_force) > tolerance) {
        fprintf(stderr, "FAIL: %s axial force = %.12f, expected = %.12f\n",
                name, result->axial_force, axial_force);
        exit(EXIT_FAILURE);
    }
    if (result->state != state) {
        fprintf(stderr, "FAIL: %s state = %d, expected = %d\n",
                name, result->state, state);
        exit(EXIT_FAILURE);
    }
}

static Element horizontal_element(void)
{
    Element element = {1, 0, 1, 210000.0, 100.0,
                       1000.0, 1.0, 0.0};
    return element;
}

static void test_horizontal_tension(void)
{
    Element element = horizontal_element();
    double displacement[MAX_DOF] = {0.0};
    ElementResult result;

    displacement[2] = 1.0;
    expect_status("horizontal tension",
                  calculate_element_result(&element, displacement, &result),
                  FEM_OK);
    expect_result("horizontal tension", &result, 1.0, 0.001, 210.0,
                  21000.0, ELEMENT_TENSION, TEST_TOL);
}

static void test_horizontal_compression(void)
{
    Element element = horizontal_element();
    double displacement[MAX_DOF] = {0.0};
    ElementResult result;

    displacement[2] = -1.0;
    expect_status("horizontal compression",
                  calculate_element_result(&element, displacement, &result),
                  FEM_OK);
    expect_result("horizontal compression", &result, -1.0, -0.001, -210.0,
                  -21000.0, ELEMENT_COMPRESSION, TEST_TOL);
}

static void test_zero_force_is_neutral(void)
{
    Element element = horizontal_element();
    double displacement[MAX_DOF] = {0.0};
    ElementResult result;

    expect_status("zero force",
                  calculate_element_result(&element, displacement, &result),
                  FEM_OK);
    expect_result_zero("zero force result", &result);
}

static void test_diagonal_projection(void)
{
    Element element = {1, 0, 1, 1000.0, 2.0,
                       1000.0, sqrt(0.5), sqrt(0.5)};
    double displacement[MAX_DOF] = {0.0};
    const double elongation = sqrt(2.0);
    ElementResult result;

    displacement[3] = 2.0;
    expect_status("diagonal projection",
                  calculate_element_result(&element, displacement, &result),
                  FEM_OK);
    expect_result("diagonal projection", &result, elongation,
                  elongation / 1000.0, elongation, 2.0 * elongation,
                  ELEMENT_TENSION, TEST_TOL);
}

static void test_triangle_reference_results(void)
{
    Element elements[3] = {
        {1, 0, 1, 210000.0, 100.0, 1000.0, 1.0, 0.0},
        {2, 0, 2, 210000.0, 100.0, 943.398113205660,
         0.529998940003, 0.847998304005},
        {3, 1, 2, 210000.0, 100.0, 943.398113205660,
         -0.529998940003, 0.847998304005}
    };
    double displacement[MAX_DOF] = {0.0};
    const double expected_forces[3] = {3125.000, -5896.238, -5896.238};
    const ElementState expected_states[3] = {
        ELEMENT_TENSION, ELEMENT_COMPRESSION, ELEMENT_COMPRESSION
    };
    ElementResult result;
    int i;

    displacement[2] = 0.1488095;
    displacement[4] = 0.0744048;
    displacement[5] = -0.3588632;

    for (i = 0; i < 3; ++i) {
        char name[64];

        snprintf(name, sizeof(name), "triangle element %d", i + 1);
        expect_status(name,
                      calculate_element_result(&elements[i], displacement,
                                               &result),
                      FEM_OK);
        if (!isfinite(result.axial_force) ||
            fabs(result.axial_force - expected_forces[i]) > 1.0e-2) {
            fprintf(stderr,
                    "FAIL: %s axial force = %.12f, expected = %.12f\n",
                    name, result.axial_force, expected_forces[i]);
            exit(EXIT_FAILURE);
        }
        if (result.state != expected_states[i]) {
            fprintf(stderr, "FAIL: %s state = %d, expected = %d\n",
                    name, result.state, expected_states[i]);
            exit(EXIT_FAILURE);
        }
    }
}

static void expect_invalid_preserves_inputs(const char *name,
                                            Element element,
                                            const double displacement[MAX_DOF],
                                            FemStatus expected_status)
{
    Element element_copy = element;
    double displacement_copy[MAX_DOF];
    ElementResult result = {77.0, 77.0, 77.0, 77.0, ELEMENT_TENSION};

    memcpy(displacement_copy, displacement, sizeof(displacement_copy));
    expect_status(name,
                  calculate_element_result(&element, displacement, &result),
                  expected_status);
    expect_result_zero(name, &result);
    expect_bytes_unchanged("invalid element", &element, &element_copy,
                           sizeof(element));
    expect_bytes_unchanged("invalid displacement", displacement,
                           displacement_copy, sizeof(displacement_copy));
}

static void test_null_element_clears_result(void)
{
    double displacement[MAX_DOF] = {0.0};
    double displacement_copy[MAX_DOF];
    ElementResult result = {77.0, 77.0, 77.0, 77.0, ELEMENT_TENSION};

    displacement[2] = 1.0;
    memcpy(displacement_copy, displacement, sizeof(displacement_copy));
    expect_status("null element",
                  calculate_element_result(NULL, displacement, &result),
                  FEM_INVALID_ARGUMENT);
    expect_result_zero("null element result", &result);
    expect_bytes_unchanged("null element displacement", displacement,
                           displacement_copy, sizeof(displacement_copy));
}

static void test_null_displacement_clears_result(void)
{
    Element element = horizontal_element();
    Element element_copy = element;
    ElementResult result = {77.0, 77.0, 77.0, 77.0, ELEMENT_TENSION};

    expect_status("null displacement",
                  calculate_element_result(&element, NULL, &result),
                  FEM_INVALID_ARGUMENT);
    expect_result_zero("null displacement result", &result);
    expect_bytes_unchanged("null displacement element", &element, &element_copy,
                           sizeof(element));
}

static void test_null_result_preserves_inputs(void)
{
    Element element = horizontal_element();
    Element element_copy = element;
    double displacement[MAX_DOF] = {0.0};
    double displacement_copy[MAX_DOF];

    displacement[2] = 1.0;
    memcpy(displacement_copy, displacement, sizeof(displacement_copy));
    expect_status("null result",
                  calculate_element_result(&element, displacement, NULL),
                  FEM_INVALID_ARGUMENT);
    expect_bytes_unchanged("null result element", &element, &element_copy,
                           sizeof(element));
    expect_bytes_unchanged("null result displacement", displacement,
                           displacement_copy, sizeof(displacement_copy));
}

static void test_invalid_element_indices(void)
{
    Element element = horizontal_element();
    double displacement[MAX_DOF] = {0.0};

    element.node1 = -1;
    expect_invalid_preserves_inputs("negative node1", element, displacement,
                                    FEM_INVALID_INDEX);
    element = horizontal_element();
    element.node2 = MAX_NODES;
    expect_invalid_preserves_inputs("out of range node2", element, displacement,
                                    FEM_INVALID_INDEX);
    element = horizontal_element();
    element.node1 = element.node2;
    expect_invalid_preserves_inputs("duplicate element nodes", element,
                                     displacement, FEM_INVALID_INDEX);
}

static void test_invalid_element_properties(void)
{
    Element element = horizontal_element();
    double displacement[MAX_DOF] = {0.0};

    element.length = 0.0;
    expect_invalid_preserves_inputs("zero length", element, displacement,
                                    FEM_ZERO_LENGTH);
    element = horizontal_element();
    element.E = 0.0;
    expect_invalid_preserves_inputs("zero modulus", element, displacement,
                                    FEM_INVALID_PROPERTY);
    element = horizontal_element();
    element.A = -1.0;
    expect_invalid_preserves_inputs("negative area", element, displacement,
                                    FEM_INVALID_PROPERTY);
    element = horizontal_element();
    element.E = NAN;
    expect_invalid_preserves_inputs("nonfinite modulus", element, displacement,
                                    FEM_INVALID_PROPERTY);
    element = horizontal_element();
    element.length = NAN;
    expect_invalid_preserves_inputs("nonfinite length", element, displacement,
                                    FEM_INVALID_ARGUMENT);
    element = horizontal_element();
    element.c = INFINITY;
    expect_invalid_preserves_inputs("nonfinite direction cosine", element,
                                    displacement, FEM_INVALID_ARGUMENT);
}

static void test_nonfinite_displacement_is_rejected(void)
{
    Element element = horizontal_element();
    double displacement[MAX_DOF] = {0.0};
    int i;

    for (i = 0; i < 4; ++i) {
        displacement[2] = 0.0;
        displacement[3] = 0.0;
        displacement[0] = 0.0;
        displacement[1] = 0.0;
        displacement[i] = NAN;
        expect_invalid_preserves_inputs("nonfinite displacement", element,
                                        displacement, FEM_INVALID_ARGUMENT);
    }
}

static void test_overflow_is_rejected(void)
{
    Element element = {1, 0, 1, 2.0, 1.0, 1.0, 1.0, 0.0};
    double displacement[MAX_DOF] = {0.0};

    displacement[2] = DBL_MAX;
    expect_invalid_preserves_inputs("calculation overflow", element,
                                    displacement, FEM_INVALID_ARGUMENT);
}

int main(void)
{
    test_horizontal_tension();
    test_horizontal_compression();
    test_zero_force_is_neutral();
    test_diagonal_projection();
    test_triangle_reference_results();
    test_null_element_clears_result();
    test_null_displacement_clears_result();
    test_null_result_preserves_inputs();
    test_invalid_element_indices();
    test_invalid_element_properties();
    test_nonfinite_displacement_is_rejected();
    test_overflow_is_rejected();

    printf("Stage 6 element postprocess contract tests passed.\n");
    return EXIT_SUCCESS;
}
