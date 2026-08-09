#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fem.h"

#define TEST_TOL 1.0e-9

static void expect_close(const char *name, double actual, double expected)
{
    if (fabs(actual - expected) > TEST_TOL) {
        fprintf(stderr,
                "FAIL: %s, actual = %.12f, expected = %.12f\n",
                name,
                actual,
                expected);
        exit(EXIT_FAILURE);
    }
}

static void expect_status(const char *name,
                          FemStatus actual,
                          FemStatus expected)
{
    if (actual != expected) {
        fprintf(stderr,
                "FAIL: %s, actual status = %d, expected status = %d\n",
                name,
                actual,
                expected);
        exit(EXIT_FAILURE);
    }
}

static void expect_matrix(double actual[4][4],
                          const double expected[4][4])
{
    int i;
    int j;

    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            char name[32];
            snprintf(name, sizeof(name), "ke[%d][%d]", i, j);
            expect_close(name, actual[i][j], expected[i][j]);
        }
    }
}

static void test_horizontal_element(void)
{
    Node node_i = {1, 0.0, 0.0};
    Node node_j = {2, 1000.0, 0.0};
    Element element = {1, 0, 1, 210000.0, 100.0, 0.0, 0.0, 0.0};
    double ke[4][4];
    const double expected[4][4] = {
        {21000.0, 0.0, -21000.0, 0.0},
        {0.0, 0.0, 0.0, 0.0},
        {-21000.0, 0.0, 21000.0, 0.0},
        {0.0, 0.0, 0.0, 0.0}
    };

    expect_status("horizontal geometry",
                  calculate_element_geometry(&node_i, &node_j, &element),
                  FEM_OK);
    expect_close("horizontal length", element.length, 1000.0);
    expect_close("horizontal c", element.c, 1.0);
    expect_close("horizontal s", element.s, 0.0);

    expect_status("horizontal stiffness",
                  calculate_element_stiffness(&element, ke),
                  FEM_OK);
    expect_matrix(ke, expected);
}

static void test_diagonal_element(void)
{
    Node node_i = {1, 0.0, 0.0};
    Node node_j = {2, 500.0, 800.0};
    Element element = {1, 0, 1, 210000.0, 100.0, 0.0, 0.0, 0.0};
    double ke[4][4];
    const double expected[4][4] = {
        {6252.796483183585, 10004.474373093737,
         -6252.796483183585, -10004.474373093737},
        {10004.474373093737, 16007.158996949980,
         -10004.474373093737, -16007.158996949980},
        {-6252.796483183585, -10004.474373093737,
         6252.796483183585, 10004.474373093737},
        {-10004.474373093737, -16007.158996949980,
         10004.474373093737, 16007.158996949980}
    };

    expect_status("diagonal geometry",
                  calculate_element_geometry(&node_i, &node_j, &element),
                  FEM_OK);
    expect_close("diagonal length", element.length, 943.3981132056604);
    expect_close("diagonal c", element.c, 0.52999894000318);
    expect_close("diagonal s", element.s, 0.847998304005088);

    expect_status("diagonal stiffness",
                  calculate_element_stiffness(&element, ke),
                  FEM_OK);
    expect_matrix(ke, expected);
}

static void test_zero_length_element(void)
{
    Node node_i = {1, 10.0, 20.0};
    Node node_j = {2, 10.0, 20.0};
    Element element = {1, 0, 1, 210000.0, 100.0, 0.0, 0.0, 0.0};

    expect_status("zero-length geometry",
                  calculate_element_geometry(&node_i, &node_j, &element),
                  FEM_ZERO_LENGTH);
}

static void test_invalid_property(void)
{
    Element element = {1, 0, 1, 0.0, 100.0, 1000.0, 1.0, 0.0};
    double ke[4][4];

    expect_status("invalid elastic modulus",
                  calculate_element_stiffness(&element, ke),
                  FEM_INVALID_PROPERTY);
}

static void test_status_messages(void)
{
    if (strcmp(fem_status_message(FEM_OK), "success") != 0) {
        fprintf(stderr, "FAIL: FEM_OK status message\n");
        exit(EXIT_FAILURE);
    }

    if (strcmp(fem_status_message(FEM_ZERO_LENGTH),
               "zero-length element") != 0) {
        fprintf(stderr, "FAIL: FEM_ZERO_LENGTH status message\n");
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    test_horizontal_element();
    test_diagonal_element();
    test_zero_length_element();
    test_invalid_property();
    test_status_messages();

    printf("Stage 1 tests passed.\n");
    return EXIT_SUCCESS;
}
