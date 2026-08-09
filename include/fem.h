#ifndef FEM_H
#define FEM_H

#include "model.h"

typedef enum {
    FEM_OK = 0,
    FEM_INVALID_ARGUMENT,
    FEM_ZERO_LENGTH,
    FEM_INVALID_PROPERTY
} FemStatus;

/* 计算单元长度和方向余弦。 */
FemStatus calculate_element_geometry(const Node *node_i,
                                     const Node *node_j,
                                     Element *element);

/* 计算二维桁架单元在全局坐标系中的 4×4 刚度矩阵。 */
FemStatus calculate_element_stiffness(const Element *element,
                                      double ke[4][4]);

/* 将状态码转换为可读错误信息。 */
const char *fem_status_message(FemStatus status);

#endif
