#ifdef WAF_BUILD
#include "libpbd-config.h"
#endif

#ifndef HAVE_LOCALTIME_R
#include <time.h>
#include <string.h>

#include "pbd/pthread_utils.h"
#include "pbd/localtime_r.h"

#ifdef localtime_r
#undef localtime_r
#endif

struct tm *
localtime_r(const time_t *const timep, struct tm *p_tm)
{
	static pthread_mutex_t time_mutex;
	static int time_mutex_inited = 0;
	struct tm *tmp;

	if (!time_mutex_inited)
	{
		time_mutex_inited = 1;
		pthread_mutex_init(&time_mutex, NULL);
	}

	pthread_mutex_lock(&time_mutex);
	tmp = localtime(timep);
	if (tmp)
	{
		memcpy(p_tm, tmp, sizeof(struct tm));
		tmp = p_tm;
	}
	pthread_mutex_unlock(&time_mutex);

	return tmp;
}

#endif
