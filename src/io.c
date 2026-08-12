#include "io.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define LINE_BUFFER_SIZE 512

typedef struct {
    int node_id;
    double fx;
    double fy;
} LoadRecord;

typedef struct {
    int node_id;
    int fix_x;
    int fix_y;
} ConstraintRecord;

static void clear_model(FemModel *model)
{
    if (model != NULL) {
        memset(model, 0, sizeof(*model));
    }
}

static int is_blank_or_comment(const char *line)
{
    while (isspace((unsigned char)*line)) {
        ++line;
    }
    return *line == '\0' || *line == '#';
}

static FemStatus read_content_line(FILE *file, char line[LINE_BUFFER_SIZE])
{
    while (fgets(line, LINE_BUFFER_SIZE, file) != NULL) {
        if (strchr(line, '\n') == NULL && !feof(file)) {
            return FEM_INPUT_ERROR;
        }
        if (!is_blank_or_comment(line)) {
            return FEM_OK;
        }
    }
    return FEM_INPUT_ERROR;
}

static int parse_header(const char *line, const char *section, int *count)
{
    char name[16];
    char extra;

    return sscanf(line, "%15s %d %c", name, count, &extra) == 2 &&
           strcmp(name, section) == 0;
}

static int find_node_index(const int node_ids[MAX_NODES], int node_count,
                           int node_id)
{
    int i;

    for (i = 0; i < node_count; ++i) {
        if (node_ids[i] == node_id) {
            return i;
        }
    }
    return -1;
}

static int has_element_id(const FemModel *model, int element_count,
                          int element_id)
{
    int i;

    for (i = 0; i < element_count; ++i) {
        if (model->elements[i].id == element_id) {
            return 1;
        }
    }
    return 0;
}

static FemStatus read_nodes(FILE *file, FemModel *parsed)
{
    char line[LINE_BUFFER_SIZE];
    int count;
    int i;

    if (read_content_line(file, line) != FEM_OK ||
        !parse_header(line, "NODES", &count)) {
        return FEM_INPUT_ERROR;
    }
    if (count <= 0) {
        return FEM_INPUT_ERROR;
    }
    if (count > MAX_NODES) {
        return FEM_CAPACITY_EXCEEDED;
    }

    for (i = 0; i < count; ++i) {
        Node *node = &parsed->nodes[i];
        char extra;

        if (read_content_line(file, line) != FEM_OK ||
            sscanf(line, "%d %lf %lf %c", &node->id, &node->x,
                   &node->y, &extra) != 3) {
            return FEM_INPUT_ERROR;
        }
    }
    parsed->node_count = count;
    return FEM_OK;
}

static FemStatus read_elements(FILE *file, FemModel *parsed)
{
    char line[LINE_BUFFER_SIZE];
    int count;
    int i;

    if (read_content_line(file, line) != FEM_OK ||
        !parse_header(line, "ELEMENTS", &count)) {
        return FEM_INPUT_ERROR;
    }
    if (count <= 0) {
        return FEM_INPUT_ERROR;
    }
    if (count > MAX_ELEMENTS) {
        return FEM_CAPACITY_EXCEEDED;
    }

    for (i = 0; i < count; ++i) {
        Element *element = &parsed->elements[i];
        int node1_id;
        int node2_id;
        char extra;

        if (read_content_line(file, line) != FEM_OK ||
            sscanf(line, "%d %d %d %lf %lf %c", &element->id, &node1_id,
                   &node2_id, &element->E, &element->A, &extra) != 5) {
            return FEM_INPUT_ERROR;
        }
        element->node1 = node1_id;
        element->node2 = node2_id;
    }
    parsed->element_count = count;
    return FEM_OK;
}

static FemStatus read_loads(FILE *file, LoadRecord records[MAX_NODES],
                            int *record_count)
{
    char line[LINE_BUFFER_SIZE];
    int count;
    int i;

    if (read_content_line(file, line) != FEM_OK ||
        !parse_header(line, "LOADS", &count) || count < 0) {
        return FEM_INPUT_ERROR;
    }
    if (count > MAX_NODES) {
        return FEM_INPUT_ERROR;
    }

    for (i = 0; i < count; ++i) {
        char extra;

        if (read_content_line(file, line) != FEM_OK ||
            sscanf(line, "%d %lf %lf %c", &records[i].node_id,
                   &records[i].fx, &records[i].fy, &extra) != 3) {
            return FEM_INPUT_ERROR;
        }
    }
    *record_count = count;
    return FEM_OK;
}

static FemStatus read_constraints(FILE *file,
                                  ConstraintRecord records[MAX_NODES],
                                  int *record_count)
{
    char line[LINE_BUFFER_SIZE];
    int count;
    int i;

    if (read_content_line(file, line) != FEM_OK ||
        !parse_header(line, "CONSTRAINTS", &count) || count < 0) {
        return FEM_INPUT_ERROR;
    }
    if (count > MAX_NODES) {
        return FEM_INPUT_ERROR;
    }

    for (i = 0; i < count; ++i) {
        char extra;

        if (read_content_line(file, line) != FEM_OK ||
            sscanf(line, "%d %d %d %c", &records[i].node_id,
                   &records[i].fix_x, &records[i].fix_y, &extra) != 3) {
            return FEM_INPUT_ERROR;
        }
    }
    *record_count = count;
    return FEM_OK;
}

static FemStatus validate_nodes(FemModel *parsed, int node_ids[MAX_NODES])
{
    int i;

    for (i = 0; i < parsed->node_count; ++i) {
        Node *node = &parsed->nodes[i];

        if (node->id <= 0 || !isfinite(node->x) || !isfinite(node->y) ||
            find_node_index(node_ids, i, node->id) >= 0) {
            return FEM_INPUT_ERROR;
        }
        node_ids[i] = node->id;
    }
    return FEM_OK;
}

static FemStatus validate_elements(FemModel *parsed,
                                   const int node_ids[MAX_NODES])
{
    int i;

    for (i = 0; i < parsed->element_count; ++i) {
        Element *element = &parsed->elements[i];
        int node1_index;
        int node2_index;
        FemStatus status;

        if (element->id <= 0 || has_element_id(parsed, i, element->id)) {
            return FEM_INPUT_ERROR;
        }
        if (!isfinite(element->E) || !isfinite(element->A) ||
            element->E <= 0.0 || element->A <= 0.0) {
            return FEM_INVALID_PROPERTY;
        }
        node1_index = find_node_index(node_ids, parsed->node_count,
                                      element->node1);
        node2_index = find_node_index(node_ids, parsed->node_count,
                                      element->node2);
        if (node1_index < 0 || node2_index < 0) {
            return FEM_INPUT_ERROR;
        }
        element->node1 = node1_index;
        element->node2 = node2_index;
        status = calculate_element_geometry(&parsed->nodes[node1_index],
                                            &parsed->nodes[node2_index],
                                            element);
        if (status != FEM_OK) {
            return status;
        }
    }
    return FEM_OK;
}

static FemStatus validate_loads(FemModel *parsed,
                                const int node_ids[MAX_NODES],
                                const LoadRecord records[MAX_NODES],
                                int record_count)
{
    int seen[MAX_NODES] = {0};
    int i;

    for (i = 0; i < record_count; ++i) {
        int node_index = find_node_index(node_ids, parsed->node_count,
                                         records[i].node_id);

        if (node_index < 0 || seen[node_index] != 0) {
            return FEM_INPUT_ERROR;
        }
        if (!isfinite(records[i].fx) || !isfinite(records[i].fy)) {
            return FEM_INVALID_LOAD;
        }
        parsed->nodes[node_index].fx = records[i].fx;
        parsed->nodes[node_index].fy = records[i].fy;
        seen[node_index] = 1;
    }
    return FEM_OK;
}

static FemStatus validate_constraints(FemModel *parsed,
                                      const int node_ids[MAX_NODES],
                                      const ConstraintRecord records[MAX_NODES],
                                      int record_count)
{
    int seen[MAX_NODES] = {0};
    int i;

    for (i = 0; i < record_count; ++i) {
        int node_index = find_node_index(node_ids, parsed->node_count,
                                         records[i].node_id);

        if (node_index < 0 || seen[node_index] != 0) {
            return FEM_INPUT_ERROR;
        }
        if ((records[i].fix_x != 0 && records[i].fix_x != 1) ||
            (records[i].fix_y != 0 && records[i].fix_y != 1)) {
            return FEM_INVALID_CONSTRAINT;
        }
        parsed->nodes[node_index].fix_x = records[i].fix_x;
        parsed->nodes[node_index].fix_y = records[i].fix_y;
        seen[node_index] = 1;
    }
    return FEM_OK;
}

static FemStatus validate_end_of_file(FILE *file)
{
    char line[LINE_BUFFER_SIZE];

    while (fgets(line, LINE_BUFFER_SIZE, file) != NULL) {
        if (strchr(line, '\n') == NULL && !feof(file)) {
            return FEM_INPUT_ERROR;
        }
        if (!is_blank_or_comment(line)) {
            return FEM_INPUT_ERROR;
        }
    }
    return ferror(file) ? FEM_INPUT_ERROR : FEM_OK;
}

FemStatus read_model_file(const char *path, FemModel *model)
{
    FemModel parsed = {0};
    int node_ids[MAX_NODES] = {0};
    LoadRecord loads[MAX_NODES] = {{0}};
    ConstraintRecord constraints[MAX_NODES] = {{0}};
    int load_count = 0;
    int constraint_count = 0;
    FILE *file;
    FemStatus status;

    clear_model(model);
    if (path == NULL || model == NULL) {
        return FEM_INVALID_ARGUMENT;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        return FEM_INPUT_ERROR;
    }

    status = read_nodes(file, &parsed);
    if (status == FEM_OK) {
        status = read_elements(file, &parsed);
    }
    if (status == FEM_OK) {
        status = read_loads(file, loads, &load_count);
    }
    if (status == FEM_OK) {
        status = read_constraints(file, constraints, &constraint_count);
    }
    if (status == FEM_OK) {
        status = validate_end_of_file(file);
    }
    if (status == FEM_OK) {
        status = validate_nodes(&parsed, node_ids);
    }
    if (status == FEM_OK) {
        status = validate_elements(&parsed, node_ids);
    }
    if (status == FEM_OK) {
        status = validate_loads(&parsed, node_ids, loads, load_count);
    }
    if (status == FEM_OK) {
        status = validate_constraints(&parsed, node_ids, constraints,
                                      constraint_count);
    }

    fclose(file);
    if (status != FEM_OK) {
        clear_model(model);
        return status;
    }

    *model = parsed;
    return FEM_OK;
}
