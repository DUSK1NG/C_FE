#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "fem.h"

static void print_matrix(double matrix[4][4])
{
    int i;
    int j;

    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            printf("%14.6f", matrix[i][j]);
        }
        printf("\n");
    }
}

int main(void)
{
    const Node node_i = {1, 0.0, 0.0};
    const Node node_j = {2, 500.0, 800.0};
    Element element = {1, 0, 1, 210000.0, 100.0, 0.0, 0.0, 0.0};
    double ke[4][4];
    FemStatus status;

    status = calculate_element_geometry(&node_i, &node_j, &element);
    if (status != FEM_OK) {
        fprintf(stderr, "Geometry error: %s\n", fem_status_message(status));
        return EXIT_FAILURE;
    }

    status = calculate_element_stiffness(&element, ke);
    if (status != FEM_OK) {
        fprintf(stderr, "Stiffness error: %s\n", fem_status_message(status));
        return EXIT_FAILURE;
    }

    printf("Stage 1: single 2D truss element\n");
    printf("Length = %.12f mm\n", element.length);
    printf("c = %.12f\n", element.c);
    printf("s = %.12f\n", element.s);

#if DEBUG
    printf("Element stiffness matrix [N/mm]:\n");
    print_matrix(ke);
#endif

    return EXIT_SUCCESS;
}
