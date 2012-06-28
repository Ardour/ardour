#ifdef WAF_BUILD
#include "libpbd-config.h"
#endif

#include <boost/scoped_ptr.hpp>
#include <string>
#include <glibmm/spawn.h>

#include "pbd/epa.h"
#include "pbd/openuri.h"

bool
PBD::open_uri (const char* uri)
{
#ifdef __APPLE__
	extern bool cocoa_open_url (const char*);
	return cocoa_open_url (uri);
#else
	EnvironmentalProtectionAgency* global_epa = EnvironmentalProtectionAgency::get_global_epa ();
	boost::scoped_ptr<EnvironmentalProtectionAgency> current_epa;

	/* revert all environment settings back to whatever they were when ardour started
	 */

	if (global_epa) {
		current_epa.reset (new EnvironmentalProtectionAgency(true)); /* will restore settings when we leave scope */
		global_epa->restore ();
	}

	std::string command = "xdg-open ";
	command += uri;
	command += " &";
	system (command.c_str());

	return true;
#endif
}

