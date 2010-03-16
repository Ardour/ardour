#ifdef WAF_BUILD
#include "libpbd-config.h"
#endif

#include <string>
#include <glibmm/spawn.h>

#include "pbd/openuri.h"

bool
PBD::open_uri (const char* uri)
{
#ifdef HAVE_GTK_OPEN_URI
	GError* err;
	return gtk_open_uri (0, uri, GDK_CURRENT_TIME, &err);
#else
#ifdef __APPLE__
	extern bool cocoa_open_url (const char*);
	return cocoa_open_url (uri);
#else
	std::string command = "xdg-open ";
	command += uri;
        Glib::spawn_command_line_async (command);

	return true;
#endif
#endif
}
