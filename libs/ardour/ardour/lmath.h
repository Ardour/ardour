#include "math.h"

#if defined __DARWIN_NO_LONG_LONG && defined MAC_OS_X_VERSION_MIN_REQUIRED && MAC_OS_X_VERSION_MIN_REQUIRED <= 1040
static inline long long int llrint (double x)
{
	return (long long int)rint (x);
}

static inline long long int llrintf (float x)
{
	return (long long int)rintf (x);
}
#endif
