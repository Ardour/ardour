#ifndef PBD_LOCALTIME_R
#define PBD_LOCALTIME_R
#include <time.h>

extern struct tm *localtime_r(const time_t *const timep, struct tm *p_tm);

#endif
