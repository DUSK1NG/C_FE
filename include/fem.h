#ifndef FEM_H
#define FEM_H

#include "config.h"
#include "model.h"

typedef enum {
    FEM_OK = 0,
    FEM_INVALID_ARGUMENT,
    FEM_ZERO_LENGTH,
    FEM_INVALID_PROPERTY,
    FEM_INVALID_INDEX,
    FEM_CAPACITY_EXCEEDED,
    FEM_INVALID_CONSTRAINT,
    FEM_INVALID_LOAD,
    FEM_SINGULAR_MATRIX /* matrix is singular or ill-conditioned */
} FemStatus;

/* 计算单元长度和方向余弦。 */
FemStatus calculate_element_geometry(const Node *node_i,
                                     const Node *node_j,
                                     Element *element);

/* 计算二维桁架单元在全局坐标系中的 4×4 刚度矩阵。 */
FemStatus calculate_element_stiffness(const Element *element,
                                      double ke[4][4]);

/* 将各单元刚度矩阵按全局自由度映射并累加到总体矩阵。 */
FemStatus assemble_global_stiffness(const Node *nodes,
                                    int node_count,
                                    Element *elements,
                                    int element_count,
                                    double global_k[MAX_DOF][MAX_DOF]);

FemStatus build_force_vector(const Node *nodes,
                             int node_count,
                             double force[MAX_DOF]);

FemStatus identify_dofs(const Node *nodes,
                        int node_count,
                        int free_dofs[MAX_DOF],
                        int *free_count,
                        int constrained_dofs[MAX_DOF],
                        int *constrained_count);

/* 校验自由度分区，求解缩减系统，并将自由位移回填到完整向量。 */
FemStatus solve_constrained_system(
    const double global_k[MAX_DOF][MAX_DOF],
    const double force[MAX_DOF],
    const int free_dofs[MAX_DOF],
    int free_count,
    const int constrained_dofs[MAX_DOF],
    int constrained_count,
    double displacement[MAX_DOF]);

/* 将状态码转换为可读错误信息。 */
const char *fem_status_message(FemStatus status);

#endif
