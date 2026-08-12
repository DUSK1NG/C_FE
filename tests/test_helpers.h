#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include "config.h"

/* C11 不允许可写二维数组指针隐式增加元素 const 限定。 */
static inline const double (*test_readonly_matrix(
    double (*matrix)[MAX_DOF]))[MAX_DOF]
{
    return (const double (*)[MAX_DOF])matrix;
}

#endif
