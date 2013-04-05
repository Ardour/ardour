#include <sys/time.h>
#include <iostream>
#include "canvas/debug.h"

using namespace std;

uint64_t PBD::DEBUG::CanvasItems = PBD::new_debug_bit ("canvasitems");
uint64_t PBD::DEBUG::CanvasItemsDirtied = PBD::new_debug_bit ("canvasitemsdirtied");
uint64_t PBD::DEBUG::CanvasEvents = PBD::new_debug_bit ("canvasevents");

struct timeval ArdourCanvas::epoch;
map<string, struct timeval> ArdourCanvas::last_time;
int ArdourCanvas::render_count;
int ArdourCanvas::dump_depth;

void
ArdourCanvas::set_epoch ()
{
	gettimeofday (&epoch, 0);
}

void
ArdourCanvas::checkpoint (string group, string message)
{
	struct timeval now;
	gettimeofday (&now, 0);

	now.tv_sec -= epoch.tv_sec;
	now.tv_usec -= epoch.tv_usec;
	if (now.tv_usec < 0) {
		now.tv_usec += 1e6;
		--now.tv_sec;
	}
		
	map<string, struct timeval>::iterator last = last_time.find (group);

	if (last != last_time.end ()) {
		time_t seconds = now.tv_sec - last->second.tv_sec;
		suseconds_t useconds = now.tv_usec - last->second.tv_usec;
		if (useconds < 0) {
			useconds += 1e6;
			--seconds;
		}
		cout << (now.tv_sec + ((double) now.tv_usec / 1e6)) << " [" << (seconds + ((double) useconds / 1e6)) << "]: " << message << "\n";
	} else {
		cout << message << "\n";
	}

	last_time[group] = now;
}

