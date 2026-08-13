#ifndef CLI_H
#define CLI_H

#include <stddef.h>
#include <stdio.h>

#include "output.h"

typedef struct {
    const char *input_path;
    const char *output_dir;
    const char *prefix;
    unsigned formats;
    unsigned sections;
    int demo;
    int help;
} CliOptions;

int cli_parse_args(int argc, char *argv[], CliOptions *options,
                   char error_message[], size_t error_size);
void cli_print_help(FILE *stream);

#endif
