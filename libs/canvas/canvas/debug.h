#ifndef __CANVAS_DEBUG_H__
#define __CANVAS_DEBUG_H__

#include <sys/time.h>
#include <map>
#include "pbd/debug.h"

namespace PBD {
	namespace DEBUG {
		extern uint64_t CanvasItems;
		extern uint64_t CanvasItemsDirtied;
		extern uint64_t CanvasEvents;
	}
}

#ifdef CANVAS_DEBUG
#define CANVAS_DEBUG_NAME(i, n) i->name = n;
#else
#define CANVAS_DEBUG(i, n) /* empty */
#endif

namespace ArdourCanvas {
	extern struct timeval epoch;
	extern std::map<std::string, struct timeval> last_time;
	extern void checkpoint (std::string, std::string);
	extern void set_epoch ();
	extern int render_count;
}

#endif
