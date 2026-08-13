#include "cli.h"

#include <string.h>

typedef struct {
    const char *name;
    unsigned bit;
} NameBit;

static const unsigned k_all_formats = FEM_FORMAT_TXT | FEM_FORMAT_MARKDOWN |
                                       FEM_FORMAT_CSV;
static const unsigned k_all_sections = FEM_OUTPUT_NODES | FEM_OUTPUT_ELEMENTS |
                                       FEM_OUTPUT_REACTIONS |
                                       FEM_OUTPUT_SUMMARY;

static int starts_with_option(const char *value)
{
    return value != NULL && value[0] == '-' && value[1] == '-';
}

static void set_error(char error_message[], size_t error_size,
                      const char *message)
{
    if (error_message != NULL && error_size > 0) {
        snprintf(error_message, error_size, "%s", message);
    }
}

static int reject(char error_message[], size_t error_size, const char *message)
{
    set_error(error_message, error_size, message);
    return 2;
}

static int match_name(const char *value, size_t length, const NameBit *items,
                      size_t item_count, unsigned *bit)
{
    size_t i;

    for (i = 0; i < item_count; ++i) {
        if (strlen(items[i].name) == length &&
            strncmp(value, items[i].name, length) == 0) {
            *bit = items[i].bit;
            return 1;
        }
    }

    return 0;
}

static int parse_list(const char *value, const NameBit *items,
                      size_t item_count, unsigned *mask,
                      char error_message[], size_t error_size,
                      const char *option_name)
{
    unsigned parsed_mask = 0u;
    const char *cursor;

    if (value == NULL || value[0] == '\0') {
        char message[128];
        snprintf(message, sizeof(message), "%s 需要非空列表值", option_name);
        return reject(error_message, error_size, message);
    }

    cursor = value;
    while (*cursor != '\0') {
        const char *start = cursor;
        size_t length = 0;
        unsigned bit = 0u;

        while (cursor[length] != '\0' && cursor[length] != ',') {
            ++length;
        }

        if (length == 0) {
            char message[128];
            snprintf(message, sizeof(message), "%s 包含空条目", option_name);
            return reject(error_message, error_size, message);
        }

        if (!match_name(start, length, items, item_count, &bit)) {
            char token[64];
            char message[160];

            if (length >= sizeof(token)) {
                length = sizeof(token) - 1;
            }
            memcpy(token, start, length);
            token[length] = '\0';
            snprintf(message, sizeof(message), "%s 包含未知条目：%s",
                     option_name, token);
            return reject(error_message, error_size, message);
        }

        if ((parsed_mask & bit) != 0u) {
            char message[160];
            snprintf(message, sizeof(message), "%s 包含重复条目",
                     option_name);
            return reject(error_message, error_size, message);
        }

        parsed_mask |= bit;
        cursor += length;
        if (*cursor == ',') {
            ++cursor;
        }
    }

    if (parsed_mask == 0u) {
        char message[128];
        snprintf(message, sizeof(message), "%s 不能为空", option_name);
        return reject(error_message, error_size, message);
    }

    *mask = parsed_mask;
    return 0;
}

int cli_parse_args(int argc, char *argv[], CliOptions *options,
                   char error_message[], size_t error_size)
{
    int i;

    if (options == NULL) {
        return reject(error_message, error_size, "选项输出结构不能为空");
    }

    options->input_path = NULL;
    options->output_dir = ".";
    options->prefix = "fem_results";
    options->formats = k_all_formats;
    options->sections = k_all_sections;
    options->demo = 0;
    options->help = 0;

    if (error_message != NULL && error_size > 0) {
        error_message[0] = '\0';
    }

    for (i = 1; i < argc; ++i) {
        const char *arg = argv[i];

        if (strcmp(arg, "--help") == 0) {
            options->help = 1;
            return 0;
        }

        if (strcmp(arg, "--demo") == 0) {
            options->demo = 1;
            continue;
        }

        if (strcmp(arg, "--input") == 0) {
            if (i + 1 >= argc || starts_with_option(argv[i + 1])) {
                return reject(error_message, error_size,
                              "--input 需要文件路径");
            }
            options->input_path = argv[++i];
            continue;
        }

        if (strcmp(arg, "--output-dir") == 0) {
            if (i + 1 >= argc || starts_with_option(argv[i + 1])) {
                return reject(error_message, error_size,
                              "--output-dir 需要目录路径");
            }
            options->output_dir = argv[++i];
            continue;
        }

        if (strcmp(arg, "--prefix") == 0) {
            if (i + 1 >= argc || starts_with_option(argv[i + 1])) {
                return reject(error_message, error_size,
                              "--prefix 需要文件前缀");
            }
            options->prefix = argv[++i];
            continue;
        }

        if (strcmp(arg, "--format") == 0) {
            static const NameBit formats[] = {
                {"txt", FEM_FORMAT_TXT},
                {"markdown", FEM_FORMAT_MARKDOWN},
                {"csv", FEM_FORMAT_CSV},
            };

            if (i + 1 >= argc || starts_with_option(argv[i + 1])) {
                return reject(error_message, error_size,
                              "--format 需要格式列表");
            }
            if (parse_list(argv[++i], formats, sizeof(formats) / sizeof(formats[0]),
                           &options->formats, error_message, error_size,
                           "--format") != 0) {
                return 2;
            }
            continue;
        }

        if (strcmp(arg, "--include") == 0) {
            static const NameBit sections[] = {
                {"nodes", FEM_OUTPUT_NODES},
                {"elements", FEM_OUTPUT_ELEMENTS},
                {"reactions", FEM_OUTPUT_REACTIONS},
                {"summary", FEM_OUTPUT_SUMMARY},
            };

            if (i + 1 >= argc || starts_with_option(argv[i + 1])) {
                return reject(error_message, error_size,
                              "--include 需要区段列表");
            }
            if (parse_list(argv[++i], sections, sizeof(sections) / sizeof(sections[0]),
                           &options->sections, error_message, error_size,
                           "--include") != 0) {
                return 2;
            }
            continue;
        }

        if (arg[0] == '-' && arg[1] == '-') {
            char message[128];
            snprintf(message, sizeof(message), "未知选项：%s", arg);
            return reject(error_message, error_size, message);
        }

        {
            char message[128];
            snprintf(message, sizeof(message), "未知参数：%s", arg);
            return reject(error_message, error_size, message);
        }
    }

    if (options->demo != 0 && options->input_path != NULL) {
        return reject(error_message, error_size,
                      "--demo 不能与 --input 同时使用");
    }

    if (options->demo == 0 && options->input_path == NULL) {
        return reject(error_message, error_size, "缺少 --input 或 --demo");
    }

    return 0;
}

void cli_print_help(FILE *stream)
{
    if (stream == NULL) {
        return;
    }

    fprintf(stream,
            "fem：统一输入输出命令行工具\n\n"
            "用法：\n"
            "  fem --input PATH [--output-dir DIR] [--prefix NAME]\n"
            "      [--format txt,markdown,csv]\n"
            "      [--include nodes,elements,reactions,summary]\n"
            "  fem --demo\n"
            "  fem --help\n\n"
            "默认值：\n"
            "  --output-dir .\n"
            "  --prefix fem_results\n"
            "  --format txt,markdown,csv\n"
            "  --include nodes,elements,reactions,summary\n\n"
            "说明：\n"
            "  --input PATH     指定 .model 输入文件\n"
            "  --output-dir DIR 指定输出目录，目录必须已存在\n"
            "  --prefix NAME    指定输出文件前缀\n"
            "  --format LIST    指定输出格式，可选 txt、markdown、csv\n"
            "  --include LIST   指定输出区段，可选 nodes、elements、reactions、summary\n"
            "  --demo           保留 Stage 1 演示模式\n"
            "  --help           显示帮助\n\n"
            "示例：\n"
            "  fem --input tests/data/medium.model\n"
            "  fem --demo\n"
            "  fem --help\n"
            "  fem --input tests/data/medium.model --output-dir results --prefix medium "
            "--format txt,markdown --include nodes,reactions,summary\n");
}
