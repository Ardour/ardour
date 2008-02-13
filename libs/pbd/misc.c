#include <pbd/misc.h>

#ifdef GTKOSX
#include <AppKit/AppKit.h>
#endif

void
disable_screen_updates ()
{
#ifdef GTKOSX
	// NSDisableScreenUpdates ();
#endif
}

void
enable_screen_updates ()
{
#ifdef GTKOSX
	// NSEnableScreenUpdates();
#endif
}
