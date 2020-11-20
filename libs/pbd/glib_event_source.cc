#include "pbd/glib_event_source.h"

bool
GlibEventLoopSource::prepare (int& timeout)
{
	return false;
}

bool
GlibEventLoopSource::check ()
{
	return false;
}

bool
GlibEventLoopSource::dispatch (sigc::slot_base*)
{
	return false;
}
