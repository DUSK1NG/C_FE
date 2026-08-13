#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli.h"
#include "config.h"
#include "fem.h"
#include "io.h"
#include "output.h"
#include "pipeline.h"

enum {
    EXIT_CLI_ERROR = 2,
    EXIT_INPUT_ERROR = 3,
    EXIT_ANALYSIS_ERROR = 4,
    EXIT_OUTPUT_ERROR = 5,
    OUTPUT_PATH_CAPACITY = 512,
    OUTPUT_TARGET_COUNT = 3
};

typedef FemStatus (*OutputWriter)(const char *path,
                                  const FemModel *model,
                                  const FemResults *results,
                                  const FemOutputOptions *options);

typedef struct {
    unsigned format_bit;
    const char *extension;
    OutputWriter writer;
} OutputTarget;

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

static int run_demo(void)
{
    const Node node_i = {.id = 1, .x = 0.0, .y = 0.0, .fx = 0.0,
                         .fy = 0.0, .fix_x = 0, .fix_y = 0};
    const Node node_j = {.id = 2, .x = 500.0, .y = 800.0, .fx = 0.0,
                         .fy = 0.0, .fix_x = 0, .fix_y = 0};
    Element element = {1, 0, 1, 210000.0, 100.0, 0.0, 0.0, 0.0};
    double ke[4][4];
    FemStatus status;

    status = calculate_element_geometry(&node_i, &node_j, &element);
    if (status != FEM_OK) {
        fprintf(stderr, "Geometry error: %s\n", fem_status_message(status));
        return EXIT_ANALYSIS_ERROR;
    }

    status = calculate_element_stiffness(&element, ke);
    if (status != FEM_OK) {
        fprintf(stderr, "Stiffness error: %s\n", fem_status_message(status));
        return EXIT_ANALYSIS_ERROR;
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

static int build_output_path(char path[OUTPUT_PATH_CAPACITY],
                             const char *output_dir,
                             const char *prefix,
                             const char *extension)
{
    int written;

    if (path == NULL || output_dir == NULL || output_dir[0] == '\0' ||
        prefix == NULL || prefix[0] == '\0' ||
        extension == NULL || extension[0] == '\0') {
        return 0;
    }

    written = snprintf(path, OUTPUT_PATH_CAPACITY, "%s/%s%s", output_dir,
                       prefix, extension);
    return written >= 0 && written < OUTPUT_PATH_CAPACITY;
}

static void remove_output_files(
    char output_paths[OUTPUT_TARGET_COUNT][OUTPUT_PATH_CAPACITY],
    const int created[OUTPUT_TARGET_COUNT])
{
    int i;

    for (i = 0; i < OUTPUT_TARGET_COUNT; ++i) {
        if (created[i] != 0 && output_paths[i][0] != '\0') {
            remove(output_paths[i]);
        }
    }
}

static const char *output_status_message(FemStatus status)
{
    switch (status) {
    case FEM_INVALID_ARGUMENT:
        return "invalid output request";
    case FEM_INPUT_ERROR:
        return "unable to create or write output file";
    default:
        return "output writer failed";
    }
}

static int write_selected_outputs(const CliOptions *options,
                                  const FemModel *model,
                                  const FemResults *results)
{
    static const OutputTarget output_targets[OUTPUT_TARGET_COUNT] = {
        {FEM_FORMAT_TXT, ".txt", write_results_txt_selected},
        {FEM_FORMAT_MARKDOWN, ".md", write_results_markdown_selected},
        {FEM_FORMAT_CSV, ".csv", write_results_csv_selected},
    };
    const FemOutputOptions output_options = {options->sections};
    char output_paths[OUTPUT_TARGET_COUNT][OUTPUT_PATH_CAPACITY] = {{0}};
    int created[OUTPUT_TARGET_COUNT] = {0};
    int i;

    for (i = 0; i < OUTPUT_TARGET_COUNT; ++i) {
        if ((options->formats & output_targets[i].format_bit) == 0u) {
            continue;
        }

        if (!build_output_path(output_paths[i], options->output_dir,
                               options->prefix,
                               output_targets[i].extension)) {
            fprintf(stderr,
                    "Output error: cannot construct path for %s output.\n",
                    output_targets[i].extension);
            remove_output_files(output_paths, created);
            return EXIT_OUTPUT_ERROR;
        }
    }

    for (i = 0; i < OUTPUT_TARGET_COUNT; ++i) {
        FILE *reservation;
        int saved_errno;

        if ((options->formats & output_targets[i].format_bit) == 0u) {
            continue;
        }

        errno = 0;
        reservation = fopen(output_paths[i], "wbx");
        saved_errno = errno;
        if (reservation == NULL) {
            fprintf(stderr, "Output error: cannot create %s: %s\n",
                    output_paths[i],
                    saved_errno != 0 ? strerror(saved_errno) :
                                       "target exists or is not writable");
            remove_output_files(output_paths, created);
            return EXIT_OUTPUT_ERROR;
        }

        created[i] = 1;
        errno = 0;
        if (fclose(reservation) != 0) {
            saved_errno = errno;
            fprintf(stderr, "Output error: cannot prepare %s: %s\n",
                    output_paths[i],
                    saved_errno != 0 ? strerror(saved_errno) :
                                       "failed to close reserved output file");
            remove_output_files(output_paths, created);
            return EXIT_OUTPUT_ERROR;
        }
    }

    for (i = 0; i < OUTPUT_TARGET_COUNT; ++i) {
        FemStatus status;
        int saved_errno;

        if ((options->formats & output_targets[i].format_bit) == 0u) {
            continue;
        }

        errno = 0;
        status = output_targets[i].writer(output_paths[i], model,
                                          results, &output_options);
        saved_errno = errno;
        if (status != FEM_OK) {
            fprintf(stderr, "Output error: %s: %s",
                    output_paths[i], output_status_message(status));
            if (saved_errno != 0) {
                fprintf(stderr, " (%s)", strerror(saved_errno));
            }
            fprintf(stderr, "\n");
            remove_output_files(output_paths, created);
            return EXIT_OUTPUT_ERROR;
        }
    }

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
    CliOptions options;
    char error_message[256];
    FemModel model;
    FemResults results = {0};
    FemStatus status;
    int parse_status;

    parse_status = cli_parse_args(argc, argv, &options, error_message,
                                  sizeof(error_message));
    if (parse_status != 0) {
        if (error_message[0] != '\0') {
            fprintf(stderr, "%s\n", error_message);
        }
        return parse_status;
    }

    if (options.help != 0) {
        cli_print_help(stdout);
        return EXIT_SUCCESS;
    }

    if (options.demo != 0) {
        return run_demo();
    }

    status = read_model_file(options.input_path, &model);
    if (status != FEM_OK) {
        fprintf(stderr, "Input error: %s\n", fem_status_message(status));
        return EXIT_INPUT_ERROR;
    }

    status = run_fem_analysis(&model, &results);
    if (status != FEM_OK) {
        fprintf(stderr, "Analysis error: %s\n", fem_status_message(status));
        return EXIT_ANALYSIS_ERROR;
    }

    return write_selected_outputs(&options, &model, &results);
}
