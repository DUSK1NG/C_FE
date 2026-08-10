#ifndef CONFIG_H
#define CONFIG_H

/* Stage 1 调试开关：开启后主程序打印单元刚度矩阵。 */
#define DEBUG 1

/* 长度小于该值时认为单元为零长度。单位：mm。 */
#define GEOMETRY_TOL 1.0e-12

#define MAX_NODES 10
#define MAX_DOF (2 * MAX_NODES)

#endif
