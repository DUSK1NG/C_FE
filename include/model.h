#ifndef MODEL_H
#define MODEL_H

typedef struct {
    int id;
    double x;
    double y;
} Node;

typedef struct {
    int id;

    /* node1 和 node2 在后续组装阶段保存内部 0 基节点索引。 */
    int node1;
    int node2;

    double E;
    double A;

    /* 由单元几何计算得到。 */
    double length;
    double c;
    double s;
} Element;

#endif
