#ifndef PBD_LOCALTIME_R
#define PBD_LOCALTIME_R
#include <time.h>

#ifdef COMPILER_MSVC
	#define localtime_r( _clock, _result ) \
		( *(_result) = *localtime( (_clock) ), \
		(_result) )
#else
	extern struct tm *localtime_r(const time_t *const timep, struct tm *p_tm);
#endif

#endif
