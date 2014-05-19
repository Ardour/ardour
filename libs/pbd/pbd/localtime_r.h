#ifndef PBD_LOCALTIME_R
#define PBD_LOCALTIME_R
#include <time.h>

#ifdef COMPILER_MSVC

#define localtime_r( _clock, _result ) \
	( *(_result) = *localtime( (_clock) ), (_result) )

#elif defined COMPILER_MINGW

#  ifdef localtime_r
#  undef localtime_r
#  endif

// As in 64 bit time_t is 64 bit integer, compiler breaks compilation
// everytime implicit cast from long int* to time_t* worked in
// the past (32 bit). To unblock such a cast we added the localtime below:
extern struct tm *localtime(const long int *_Time);
extern struct tm *localtime_r(const time_t *const timep, struct tm *p_tm);

#endif

#endif
