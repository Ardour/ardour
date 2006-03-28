/*
    Copyright (C) 2000 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#include <cstdio> // Needed so that libraptor (included in lrdf) won't complain
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <locale.h>

#ifdef VST_SUPPORT
#include <fst.h>
#endif

#include <lrdf.h>

#include <pbd/error.h>
#include <pbd/strsplit.h>

#include <midi++/port.h>
#include <midi++/port_request.h>
#include <midi++/manager.h>
#include <midi++/mmc.h>

#include <ardour/ardour.h>
#include <ardour/audio_library.h>
#include <ardour/configuration.h>
#include <ardour/plugin_manager.h>
#include <ardour/source.h>
#include <ardour/utils.h>
#include <ardour/session.h>

#include <ardour/mix.h>

#if defined (__APPLE__)
       #include <Carbon/Carbon.h> // For Gestalt
#endif
       
#include "i18n.h"

ARDOUR::Configuration* ARDOUR::Config = 0;
ARDOUR::AudioLibrary* ARDOUR::Library = 0;

using namespace ARDOUR;
using namespace std;

MIDI::Port *default_mmc_port = 0;
MIDI::Port *default_mtc_port = 0;
MIDI::Port *default_midi_port = 0;

Change ARDOUR::StartChanged = ARDOUR::new_change ();
Change ARDOUR::LengthChanged = ARDOUR::new_change ();
Change ARDOUR::PositionChanged = ARDOUR::new_change ();
Change ARDOUR::NameChanged = ARDOUR::new_change ();
Change ARDOUR::BoundsChanged = Change (0); // see init(), below


static int 
setup_midi ()
{
	std::map<string,Configuration::MidiPortDescriptor*>::iterator i;
	int nports;

	if ((nports = Config->midi_ports.size()) == 0) {
		warning << _("no MIDI ports specified: no MMC or MTC control possible") << endmsg;
		return 0;
	}

	for (i = Config->midi_ports.begin(); i != Config->midi_ports.end(); ++i) {
		Configuration::MidiPortDescriptor* port_descriptor;

		port_descriptor = (*i).second;

		MIDI::PortRequest request (port_descriptor->device, 
					   port_descriptor->tag, 
					   port_descriptor->mode, 
					   port_descriptor->type);

		if (request.status != MIDI::PortRequest::OK) {
			error << string_compose(_("MIDI port specifications for \"%1\" are not understandable."), port_descriptor->tag) << endmsg;
			continue;
		}
		
		MIDI::Manager::instance()->add_port (request);
	}

	if (nports > 1) {

		/* More than one port, so try using specific names for each port */

		map<string,Configuration::MidiPortDescriptor *>::iterator i;

		if (Config->get_mmc_port_name() != N_("default")) {
			default_mmc_port =  MIDI::Manager::instance()->port (Config->get_mmc_port_name());
		} 

		if (Config->get_mtc_port_name() != N_("default")) {
			default_mtc_port =  MIDI::Manager::instance()->port (Config->get_mtc_port_name());
		} 

		if (Config->get_midi_port_name() != N_("default")) {
			default_midi_port =  MIDI::Manager::instance()->port (Config->get_midi_port_name());
		} 
		
		/* If that didn't work, just use the first listed port */

		if (default_mmc_port == 0) {
			default_mmc_port = MIDI::Manager::instance()->port (0);
		}

		if (default_mtc_port == 0) {
			default_mtc_port = MIDI::Manager::instance()->port (0);
		}

		if (default_midi_port == 0) {
			default_midi_port = MIDI::Manager::instance()->port (0);
		}
		
	} else {

		/* Only one port described, so use it for both MTC and MMC */

		default_mmc_port = MIDI::Manager::instance()->port (0);
		default_mtc_port = default_mmc_port;
		default_midi_port = default_mmc_port;
	}

	if (default_mmc_port == 0) {
		warning << string_compose (_("No MMC control (MIDI port \"%1\" not available)"), Config->get_mmc_port_name()) 
			<< endmsg;
		return 0;
	} 

	if (default_mtc_port == 0) {
		warning << string_compose (_("No MTC support (MIDI port \"%1\" not available)"), Config->get_mtc_port_name()) 
			<< endmsg;
	}

	if (default_midi_port == 0) {
		warning << string_compose (_("No MIDI parameter support (MIDI port \"%1\" not available)"), Config->get_midi_port_name()) 
			<< endmsg;
	}

	return 0;
}

int
ARDOUR::init (AudioEngine& engine, bool use_vst, bool try_optimization, void (*sighandler)(int,siginfo_t*,void*))
{
        bool generic_mix_functions = true;

	(void) bindtextdomain(PACKAGE, LOCALEDIR);

	Config = new Configuration;

	if (Config->load_state ()) {
		return -1;
	}

	Config->set_use_vst (use_vst);

	if (setup_midi ()) {
		return -1;
	}

#ifdef VST_SUPPORT
	if (Config->get_use_vst() && fst_init (sighandler)) {
		return -1;
	}
#endif

	if (try_optimization) {

#if defined (ARCH_X86) && defined (BUILD_SSE_OPTIMIZATIONS)
	
		unsigned int use_sse = 0;

#ifndef USE_X86_64_ASM
		asm volatile (
				 "mov $1, %%eax\n"
				 "pushl %%ebx\n"
				 "cpuid\n"
				 "popl %%ebx\n"
				 "andl $33554432, %%edx\n"
				 "movl %%edx, %0\n"
		 	     : "=m" (use_sse)
	   		     : 
 	    		 : "%eax", "%ecx", "%edx", "memory");
#else

		asm volatile (
				 "movq $1, %%rax\n"
				 "pushq %%rbx\n"
				 "cpuid\n"
				 "popq %%rbx\n"
				 "andq $33554432, %%rdx\n"
				 "movq %%rdx, %0\n"
		 	     : "=m" (use_sse)
	   		     : 
 	    		 : "%rax", "%rcx", "%rdx", "memory");

#endif /* USE_X86_64_ASM */
		
		if (use_sse) {
			cerr << "Enabling SSE optimized routines" << endl;
	
			// SSE SET
			Session::compute_peak 			= x86_sse_compute_peak;
			Session::apply_gain_to_buffer 	= x86_sse_apply_gain_to_buffer;
			Session::mix_buffers_with_gain 	= x86_sse_mix_buffers_with_gain;
			Session::mix_buffers_no_gain 	= x86_sse_mix_buffers_no_gain;

			generic_mix_functions = false;

                }

#elif defined (__APPLE__) && defined (BUILD_VECLIB_OPTIMIZATIONS)
                long sysVersion = 0;

                if (noErr != Gestalt(gestaltSystemVersion, &sysVersion))
                        sysVersion = 0;

                if (sysVersion >= 0x00001040) { // Tiger at least
                        Session::compute_peak           = veclib_compute_peak;
                        Session::apply_gain_to_buffer   = veclib_apply_gain_to_buffer;
                        Session::mix_buffers_with_gain  = veclib_mix_buffers_with_gain;
                        Session::mix_buffers_no_gain    = veclib_mix_buffers_no_gain;

                        generic_mix_functions = false;

                        info << "Apple VecLib H/W specific optimizations in use" << endmsg;
                }
#endif
        }

        if (generic_mix_functions) {

		Session::compute_peak 			= compute_peak;
		Session::apply_gain_to_buffer 	= apply_gain_to_buffer;
		Session::mix_buffers_with_gain 	= mix_buffers_with_gain;
		Session::mix_buffers_no_gain 	= mix_buffers_no_gain;
		
		info << "No H/W specific optimizations in use" << endmsg;
	}
	
	lrdf_init();
	Library = new AudioLibrary;

	/* singleton - first object is "it" */
	new PluginManager (engine);
	
	BoundsChanged = Change (StartChanged|PositionChanged|LengthChanged);

	return 0;
}

int
ARDOUR::cleanup ()
{
	delete Library;
	lrdf_cleanup ();
	return 0;
}

ARDOUR::id_t
ARDOUR::new_id ()
{
	return get_uid();
}

ARDOUR::Change
ARDOUR::new_change ()
{
	Change c;
	static uint32_t change_bit = 1;

	/* XXX catch out-of-range */

	c = Change (change_bit);
	change_bit <<= 1;

	return c;
}

string
ARDOUR::get_user_ardour_path ()
{
	string path;
	char* envvar;
	
	if ((envvar = getenv ("HOME")) == 0 || strlen (envvar) == 0) {
		return "/";
	}
		
	path = envvar;
	path += "/.ardour2/";

	/* create it if necessary */

	mkdir (path.c_str (), 0755);

	return path;
}

string
ARDOUR::get_system_ardour_path ()
{
	string path;

	path += DATA_DIR;
	path += "/ardour2/";
	
	return path;
}

static string
find_file (string name, string dir, string subdir = "")
{
	string path;
	char* envvar = getenv("ARDOUR_PATH");

	/* stop A: any directory in ARDOUR_PATH */
	
	if (envvar != 0) {

		vector<string> split_path;
	
		split (envvar, split_path, ':');
		
		for (vector<string>::iterator i = split_path.begin(); i != split_path.end(); ++i) {
			path = *i;
			path += "/" + name;
			if (access (path.c_str(), R_OK) == 0) {
				cerr << "Using file " << path << " found in ARDOUR_PATH." << endl;
				return path;
			}
		}
	}

	/* stop B: ~/.ardour/ */

	path = get_user_ardour_path();
		
	if (subdir.length()) {
		path += subdir + "/";
	}
		
	path += name;
	if (access (path.c_str(), R_OK) == 0) {
		return path;
	}

	/* C: dir/... */
	
	path = dir;
	path += "/ardour2/";
	
	if (subdir.length()) {
		path += subdir + "/";
	}
	
	path += name;
	
	if (access (path.c_str(), R_OK) == 0) {
		return path;
	}

	return "";
}

string
ARDOUR::find_config_file (string name)
{
	char* envvar;

	if ((envvar = getenv("ARDOUR_CONFIG_PATH")) == 0) {
		envvar = CONFIG_DIR;
	}

	return find_file (name, envvar);
}

string
ARDOUR::find_data_file (string name, string subdir)
{
	char* envvar;
	if ((envvar = getenv("ARDOUR_DATA_PATH")) == 0) {
		envvar = DATA_DIR;
	}

	return find_file (name, envvar, subdir);
}

ARDOUR::LocaleGuard::LocaleGuard (const char* str)
{
	old = strdup (setlocale (LC_NUMERIC, NULL));
	if (strcmp (old, str)) {
		setlocale (LC_NUMERIC, str);
	} 
}

ARDOUR::LocaleGuard::~LocaleGuard ()
{
	setlocale (LC_NUMERIC, old);
	free ((char*)old);
}

ARDOUR::OverlapType
ARDOUR::coverage (jack_nframes_t sa, jack_nframes_t ea, 
		  jack_nframes_t sb, jack_nframes_t eb)
{
	/* OverlapType returned reflects how the second (B)
	   range overlaps the first (A).

	   The diagrams show various relative placements
	   of A and B for each OverlapType.

	   Notes:
	      Internal: the start points cannot coincide
	      External: the start and end points can coincide
	      Start: end points can coincide
	      End: start points can coincide

	   XXX Logically, Internal should disallow end
	   point equality.
	*/

	/*
	     |--------------------|   A
	          |------|            B
	        |-----------------|   B


             "B is internal to A"		

	*/
#ifdef OLD_COVERAGE
	if ((sb >= sa) && (eb <= ea)) {
#else
	if ((sb > sa) && (eb <= ea)) {
#endif
		return OverlapInternal;
	}

	/*
	     |--------------------|   A
	   ----|                      B
           -----------------------|   B
	   --|                        B
	   
	     "B overlaps the start of A"

	*/

	if ((eb >= sa) && (eb <= ea)) {
		return OverlapStart;
	}
	/* 
	     |---------------------|  A
                   |----------------- B
 	     |----------------------- B	   
                                   |- B

            "B overlaps the end of A"				   

	*/
	if ((sb >= sa) && (sb <= ea)) {
		return OverlapEnd;
	}
	/*
	     |--------------------|     A
	   --------------------------  B   
	     |-----------------------  B
	    ----------------------|    B
             |--------------------|    B


           "B overlaps all of A"
	*/
	if ((sa >= sb) && (sa <= eb) && (ea <= eb)) {
		return OverlapExternal;
	}

	return OverlapNone;
}

