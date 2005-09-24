#ifndef __ardour_timestamps_h__
#define __ardour_timestamps_h__

#ifdef WITH_JACK_TIMESTAMPS
#include <jack/timestamps.h>
#else
#define jack_timestamp(s)
#define jack_init_timestamps(n)
#define jack_dump_timestamps(o)
#define jack_reset_timestamps()
#endif

#endif /* __ardour_timestamps_h__ */
