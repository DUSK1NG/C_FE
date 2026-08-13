#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "cli.h"

static void assert_error_nonempty(const char *message)
{
    assert(message[0] != '\0');
}

static void assert_string_equal(const char *actual, const char *expected)
{
    assert(strcmp(actual, expected) == 0);
}

static void assert_defaults(void)
{
    CliOptions options;
    char error_message[256];
    char arg0[] = "fem";
    char arg1[] = "--input";
    char arg2[] = "tests/data/medium.model";
    char *argv[] = {arg0, arg1, arg2};

    memset(&options, 0, sizeof(options));
    memset(error_message, 0, sizeof(error_message));

    assert(cli_parse_args(3, argv, &options, error_message,
                          sizeof(error_message)) == 0);
    assert(options.input_path == argv[2]);
    assert_string_equal(options.output_dir, ".");
    assert_string_equal(options.prefix, "fem_results");
    assert(options.formats == (FEM_FORMAT_TXT | FEM_FORMAT_MARKDOWN |
                               FEM_FORMAT_CSV));
    assert(options.sections == (FEM_OUTPUT_NODES | FEM_OUTPUT_ELEMENTS |
                                FEM_OUTPUT_REACTIONS | FEM_OUTPUT_SUMMARY));
    assert(options.demo == 0);
    assert(options.help == 0);
    assert(error_message[0] == '\0');
}

static void assert_custom_values(void)
{
    CliOptions options;
    char error_message[256];
    char arg0[] = "fem";
    char arg1[] = "--input";
    char arg2[] = "tests/data/medium.model";
    char arg3[] = "--output-dir";
    char arg4[] = "results";
    char arg5[] = "--prefix";
    char arg6[] = "medium";
    char arg7[] = "--format";
    char arg8[] = "txt,markdown";
    char arg9[] = "--include";
    char arg10[] = "nodes,reactions,summary";
    char *argv[] = {
        arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10
    };

    memset(&options, 0, sizeof(options));
    memset(error_message, 0, sizeof(error_message));

    assert(cli_parse_args((int)(sizeof(argv) / sizeof(argv[0])), argv, &options,
                          error_message, sizeof(error_message)) == 0);
    assert(error_message[0] == '\0');
    assert(options.input_path == argv[2]);
    assert(options.output_dir == argv[4]);
    assert(options.prefix == argv[6]);
    assert(options.formats == (FEM_FORMAT_TXT | FEM_FORMAT_MARKDOWN));
    assert(options.sections == (FEM_OUTPUT_NODES | FEM_OUTPUT_REACTIONS |
                                FEM_OUTPUT_SUMMARY));
    assert(options.demo == 0);
    assert(options.help == 0);
}

static void assert_help(void)
{
    CliOptions options;
    char error_message[256];
    char arg0[] = "fem";
    char arg1[] = "--help";
    char *argv[] = {arg0, arg1};

    memset(&options, 0, sizeof(options));
    memset(error_message, 0, sizeof(error_message));

    assert(cli_parse_args(2, argv, &options, error_message,
                          sizeof(error_message)) == 0);
    assert(error_message[0] == '\0');
    assert(options.help == 1);
    assert(options.demo == 0);
    assert(options.input_path == NULL);
}

static void assert_invalid_args(int argc, char *argv[])
{
    CliOptions options;
    char error_message[256];

    memset(&options, 0, sizeof(options));
    memset(error_message, 0, sizeof(error_message));

    assert(cli_parse_args(argc, argv, &options, error_message,
                          sizeof(error_message)) == 2);
    assert_error_nonempty(error_message);
}

static void assert_invalid_forms(void)
{
    char missing_input_0[] = "fem";
    char *missing_input_argv[] = {missing_input_0};

    char unknown_format_0[] = "fem";
    char unknown_format_1[] = "--input";
    char unknown_format_2[] = "tests/data/medium.model";
    char unknown_format_3[] = "--format";
    char unknown_format_4[] = "xml";
    char *unknown_format_argv[] = {
        unknown_format_0, unknown_format_1, unknown_format_2,
        unknown_format_3, unknown_format_4
    };

    char unknown_section_0[] = "fem";
    char unknown_section_1[] = "--input";
    char unknown_section_2[] = "tests/data/medium.model";
    char unknown_section_3[] = "--include";
    char unknown_section_4[] = "widgets";
    char *unknown_section_argv[] = {
        unknown_section_0, unknown_section_1, unknown_section_2,
        unknown_section_3, unknown_section_4
    };

    char duplicate_format_0[] = "fem";
    char duplicate_format_1[] = "--input";
    char duplicate_format_2[] = "tests/data/medium.model";
    char duplicate_format_3[] = "--format";
    char duplicate_format_4[] = "txt,txt";
    char *duplicate_format_argv[] = {
        duplicate_format_0, duplicate_format_1, duplicate_format_2,
        duplicate_format_3, duplicate_format_4
    };

    char missing_value_0[] = "fem";
    char missing_value_1[] = "--input";
    char *missing_value_argv[] = {missing_value_0, missing_value_1};

    char demo_input_0[] = "fem";
    char demo_input_1[] = "--demo";
    char demo_input_2[] = "--input";
    char demo_input_3[] = "tests/data/medium.model";
    char *demo_input_argv[] = {
        demo_input_0, demo_input_1, demo_input_2, demo_input_3
    };

    char empty_entry_0[] = "fem";
    char empty_entry_1[] = "--input";
    char empty_entry_2[] = "tests/data/medium.model";
    char empty_entry_3[] = "--include";
    char empty_entry_4[] = "nodes,,summary";
    char *empty_entry_argv[] = {
        empty_entry_0, empty_entry_1, empty_entry_2, empty_entry_3,
        empty_entry_4
    };

    assert_invalid_args((int)(sizeof(missing_input_argv) /
                              sizeof(missing_input_argv[0])),
                        missing_input_argv);
    assert_invalid_args((int)(sizeof(unknown_format_argv) /
                              sizeof(unknown_format_argv[0])),
                        unknown_format_argv);
    assert_invalid_args((int)(sizeof(unknown_section_argv) /
                              sizeof(unknown_section_argv[0])),
                        unknown_section_argv);
    assert_invalid_args((int)(sizeof(duplicate_format_argv) /
                              sizeof(duplicate_format_argv[0])),
                        duplicate_format_argv);
    assert_invalid_args((int)(sizeof(missing_value_argv) /
                              sizeof(missing_value_argv[0])),
                        missing_value_argv);
    assert_invalid_args((int)(sizeof(demo_input_argv) /
                              sizeof(demo_input_argv[0])),
                        demo_input_argv);
    assert_invalid_args((int)(sizeof(empty_entry_argv) /
                              sizeof(empty_entry_argv[0])),
                        empty_entry_argv);
}

static void assert_help_text(void)
{
    FILE *stream;

    stream = tmpfile();
    assert(stream != NULL);

    cli_print_help(stream);
    fflush(stream);
    rewind(stream);

    {
        char buffer[4096];
        size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, stream);
        buffer[bytes_read] = '\0';
        assert(strstr(buffer, "--input PATH") != NULL);
        assert(strstr(buffer, "--format txt,markdown,csv") != NULL);
        assert(strstr(buffer, "fem --input tests/data/medium.model") != NULL);
    }

    fclose(stream);
}

int main(void)
{
    assert_defaults();
    assert_custom_values();
    assert_help();
    assert_invalid_forms();
    assert_help_text();
    puts("CLI parser tests passed.");
    return 0;
}
