
#ifndef NAN_INF_H
#define NAN_INF_H

#include <math.h>

#ifdef sun

#include <ieeefp.h>
#define ISNAN(x) ((sizeof(x)==sizeof(float))?isnanf(x):isnand(x))
#define ISINF(x) (!finite(x))

#else

#define ISNAN(x) isnan(x)
#define ISINF(x) isinf(x)

#endif

#endif
