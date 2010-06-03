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
*/


#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <cstdio> // Needed so that libraptor (included in lrdf) won't complain
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifdef VST_SUPPORT
#include <fst.h>
#endif

#ifdef HAVE_AUDIOUNITS
#include "ardour/audio_unit.h"
#endif

#ifdef __SSE__
#include <xmmintrin.h>
#endif

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include <lrdf.h>

#include "pbd/error.h"
#include "pbd/id.h"
#include "pbd/strsplit.h"
#include "pbd/fpu.h"
#include "pbd/file_utils.h"
#include "pbd/enumwriter.h"

#include "midi++/port.h"
#include "midi++/manager.h"
#include "midi++/mmc.h"

#include "ardour/analyser.h"
#include "ardour/ardour.h"
#include "ardour/audio_library.h"
#include "ardour/audioengine.h"
#include "ardour/audioregion.h"
#include "ardour/audiosource.h"
#include "ardour/buffer_manager.h"
#include "ardour/control_protocol_manager.h"
#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"
#include "ardour/mix.h"
#include "ardour/playlist.h"
#include "ardour/plugin_manager.h"
#include "ardour/process_thread.h"
#include "ardour/profile.h"
#include "ardour/region.h"
#include "ardour/rc_configuration.h"
#include "ardour/route_group.h"
#include "ardour/runtime_functions.h"
#include "ardour/session.h"
#include "ardour/session_event.h"
#include "ardour/source_factory.h"
#include "ardour/utils.h"

#include "audiographer/routines.h"

#if defined (__APPLE__)
       #include <Carbon/Carbon.h> // For Gestalt
#endif

#include "i18n.h"

ARDOUR::RCConfiguration* ARDOUR::Config = 0;
ARDOUR::RuntimeProfile* ARDOUR::Profile = 0;
ARDOUR::AudioLibrary* ARDOUR::Library = 0;

using namespace ARDOUR;
using namespace std;
using namespace PBD;

MIDI::Port *ARDOUR::default_mmc_port = 0;
MIDI::Port *ARDOUR::default_mtc_port = 0;
MIDI::Port *ARDOUR::default_midi_port = 0;
MIDI::Port *ARDOUR::default_midi_clock_port = 0;

compute_peak_t          ARDOUR::compute_peak = 0;
find_peaks_t            ARDOUR::find_peaks = 0;
apply_gain_to_buffer_t  ARDOUR::apply_gain_to_buffer = 0;
mix_buffers_with_gain_t ARDOUR::mix_buffers_with_gain = 0;
mix_buffers_no_gain_t   ARDOUR::mix_buffers_no_gain = 0;

PBD::Signal1<void,std::string> ARDOUR::BootMessage;

void ARDOUR::setup_enum_writer ();

/* this is useful for quite a few things that want to check
   if any bounds-related property has changed
*/
PBD::PropertyChange ARDOUR::bounds_change;

namespace ARDOUR { 
	namespace Properties {

		/* the envelope and fades are not scalar items and so
		   currently (2010/02) are not stored using Property.
		   However, these descriptors enable us to notify
		   about changes to them via PropertyChange. 

		   Declared in ardour/audioregion.h ...
		*/

		PBD::PropertyDescriptor<bool> fade_in;
		PBD::PropertyDescriptor<bool> fade_out;
		PBD::PropertyDescriptor<bool> envelope;
	}
}

void
ARDOUR::make_property_quarks ()
{
	Properties::fade_in.property_id = g_quark_from_static_string (X_("fade_in_FAKE"));
        DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for fade_in_FAKE = %1\n", 	Properties::fade_in.property_id));
	Properties::fade_out.property_id = g_quark_from_static_string (X_("fade_out_FAKE"));
        DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for fade_out_FAKE = %1\n", 	Properties::fade_out.property_id));
	Properties::envelope.property_id = g_quark_from_static_string (X_("envelope_FAKE"));
        DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for envelope_FAKE = %1\n", 	Properties::envelope.property_id));
}

int
ARDOUR::setup_midi ()
{
	if (Config->midi_ports.size() == 0) {
		return 0;
	}

	BootMessage (_("Configuring MIDI ports"));

	for (std::map<string,XMLNode>::iterator i = Config->midi_ports.begin(); i != Config->midi_ports.end(); ++i) {
		MIDI::Manager::instance()->add_port (i->second);
	}

	MIDI::Port* first;
	const MIDI::Manager::PortList& ports = MIDI::Manager::instance()->get_midi_ports();

	if (ports.size() > 1) {

		first = ports.front();

		/* More than one port, so try using specific names for each port */

		default_mmc_port =  MIDI::Manager::instance()->port (Config->get_mmc_port_name());
		default_mtc_port =  MIDI::Manager::instance()->port (Config->get_mtc_port_name());
		default_midi_port =  MIDI::Manager::instance()->port (Config->get_midi_port_name());
		default_midi_clock_port =  MIDI::Manager::instance()->port (Config->get_midi_clock_port_name());

		/* If that didn't work, just use the first listed port */

		if (default_mmc_port == 0) {
			default_mmc_port = first;
		}

		if (default_mtc_port == 0) {
			default_mtc_port = first;
		}

		if (default_midi_port == 0) {
			default_midi_port = first;
		}

		if (default_midi_clock_port == 0) {
			default_midi_clock_port = first;
		}

	} else if (ports.size() == 1) {

		first = ports.front();

		/* Only one port described, so use it for both MTC and MMC */

		default_mmc_port = first;
		default_mtc_port = default_mmc_port;
		default_midi_port = default_mmc_port;
		default_midi_clock_port = default_mmc_port;
	}

	if (default_mmc_port == 0) {
		warning << string_compose (_("No MMC control (MIDI port \"%1\" not available)"), Config->get_mmc_port_name())
			<< endmsg;
	}


	if (default_mtc_port == 0) {
		warning << string_compose (_("No MTC support (MIDI port \"%1\" not available)"), Config->get_mtc_port_name())
			<< endmsg;
	}

	if (default_midi_port == 0) {
		warning << string_compose (_("No MIDI parameter support (MIDI port \"%1\" not available)"), Config->get_midi_port_name())
			<< endmsg;
	}

	if (default_midi_clock_port == 0) {
		warning << string_compose (_("No MIDI Clock support (MIDI port \"%1\" not available)"), Config->get_midi_clock_port_name())
			<< endmsg;
	}

	return 0;
}

void
setup_hardware_optimization (bool try_optimization)
{
	bool generic_mix_functions = true;

	if (try_optimization) {

		FPU fpu;

#if defined (ARCH_X86) && defined (BUILD_SSE_OPTIMIZATIONS)

		if (fpu.has_sse()) {

			info << "Using SSE optimized routines" << endmsg;

			// SSE SET
			compute_peak          = x86_sse_compute_peak;
			find_peaks            = x86_sse_find_peaks;
			apply_gain_to_buffer  = x86_sse_apply_gain_to_buffer;
			mix_buffers_with_gain = x86_sse_mix_buffers_with_gain;
			mix_buffers_no_gain   = x86_sse_mix_buffers_no_gain;

			generic_mix_functions = false;

		}

#elif defined (__APPLE__) && defined (BUILD_VECLIB_OPTIMIZATIONS)
		long sysVersion = 0;

		if (noErr != Gestalt(gestaltSystemVersion, &sysVersion))
			sysVersion = 0;

		if (sysVersion >= 0x00001040) { // Tiger at least
			compute_peak           = veclib_compute_peak;
			find_peaks             = veclib_find_peaks;
			apply_gain_to_buffer   = veclib_apply_gain_to_buffer;
			mix_buffers_with_gain  = veclib_mix_buffers_with_gain;
			mix_buffers_no_gain    = veclib_mix_buffers_no_gain;

			generic_mix_functions = false;

			info << "Apple VecLib H/W specific optimizations in use" << endmsg;
		}
#endif

		/* consider FPU denormal handling to be "h/w optimization" */

		setup_fpu ();
	}

	if (generic_mix_functions) {

		compute_peak          = default_compute_peak;
		find_peaks            = default_find_peaks;
		apply_gain_to_buffer  = default_apply_gain_to_buffer;
		mix_buffers_with_gain = default_mix_buffers_with_gain;
		mix_buffers_no_gain   = default_mix_buffers_no_gain;

		info << "No H/W specific optimizations in use" << endmsg;
	}
	
	AudioGrapher::Routines::override_compute_peak (compute_peak);
	AudioGrapher::Routines::override_apply_gain_to_buffer (apply_gain_to_buffer);
}

static void
lotsa_files_please ()
{
	struct rlimit rl;

	if (getrlimit (RLIMIT_NOFILE, &rl) == 0) {

		rl.rlim_cur = rl.rlim_max;

		if (setrlimit (RLIMIT_NOFILE, &rl) != 0) {
			if (rl.rlim_cur == RLIM_INFINITY) {
				error << _("Could not set system open files limit to \"unlimited\"") << endmsg;
			} else {
				error << string_compose (_("Could not set system open files limit to %1"), rl.rlim_cur) << endmsg;
			}
		} else {
			if (rl.rlim_cur == RLIM_INFINITY) {
				info << _("Removed open file count limit. Excellent!") << endmsg;
			} else {
				info << string_compose (_("%1 will be limited to %2 open files"), PROGRAM_NAME, rl.rlim_cur) << endmsg;
			}
		}
	} else {
		error << string_compose (_("Could not get system open files limit (%1)"), strerror (errno)) << endmsg;
	}
}

int
ARDOUR::init (bool use_vst, bool try_optimization)
{
	if (!Glib::thread_supported()) {
		Glib::thread_init();
	}

	(void) bindtextdomain(PACKAGE, LOCALEDIR);

	PBD::ID::init ();
	SessionEvent::init_event_pool ();
	
	make_property_quarks ();
	SessionObject::make_property_quarks ();
	Region::make_property_quarks ();
	AudioRegion::make_property_quarks ();
	RouteGroup::make_property_quarks ();
        Playlist::make_property_quarks ();

	/* this is a useful ready to use PropertyChange that many
	   things need to check. This avoids having to compose
	   it every time we want to check for any of the relevant
	   property changes.
	*/

	bounds_change.add (ARDOUR::Properties::start);
	bounds_change.add (ARDOUR::Properties::position);
	bounds_change.add (ARDOUR::Properties::length);

	/* provide a state version for the few cases that need it and are not
	   driven by reading state from disk (e.g. undo/redo)
	*/

	Stateful::current_state_version = CURRENT_SESSION_FILE_VERSION;

	setup_enum_writer ();

	// allow ardour the absolute maximum number of open files
	lotsa_files_please ();

	lrdf_init();
	Library = new AudioLibrary;

	BootMessage (_("Loading configuration"));

	Config = new RCConfiguration;

	if (Config->load_state ()) {
		return -1;
	}

	
	Config->set_use_vst (use_vst);

	cerr << "After config loaded, MTC port name = " << Config->get_mtc_port_name() << endl;

	Profile = new RuntimeProfile;


#ifdef VST_SUPPORT
	if (Config->get_use_vst() && fst_init (0)) {
		return -1;
	}
#endif

#ifdef HAVE_AUDIOUNITS
	AUPluginInfo::load_cached_info ();
#endif

	/* Make VAMP look in our library ahead of anything else */

	char *p = getenv ("VAMP_PATH");
	string vamppath = VAMP_DIR;
	if (p) {
		vamppath += ':';
		vamppath += p;
	}
	setenv ("VAMP_PATH", vamppath.c_str(), 1);


	setup_hardware_optimization (try_optimization);

	SourceFactory::init ();
	Analyser::init ();

	/* singleton - first object is "it" */
	new PluginManager ();

        ProcessThread::init ();
        BufferManager::init (10); // XX should be num_processors_for_dsp

	return 0;
}

void
ARDOUR::init_post_engine ()
{
	ControlProtocolManager::instance().discover_control_protocols ();

	XMLNode* node;
	if ((node = Config->control_protocol_state()) != 0) {
		ControlProtocolManager::instance().set_state (*node, Stateful::loading_state_version);
	}
}

int
ARDOUR::cleanup ()
{
	delete Library;
	lrdf_cleanup ();
	delete &ControlProtocolManager::instance();
#ifdef VST_SUPPORT
	fst_exit ();
#endif
	return 0;
}

string
ARDOUR::get_ardour_revision ()
{
	return "$Rev$";
}

void
ARDOUR::find_bindings_files (map<string,string>& files)
{
	vector<sys::path> found;
	SearchPath spath = ardour_search_path() + user_config_directory() + system_config_search_path();

	if (getenv ("ARDOUR_SAE")) {
		Glib::PatternSpec pattern("*SAE-*.bindings");
		find_matching_files_in_search_path (spath, pattern, found);
	} else {
		Glib::PatternSpec pattern("*.bindings");
		find_matching_files_in_search_path (spath, pattern, found);
	}

	if (found.empty()) {
		return;
	}

	for (vector<sys::path>::iterator x = found.begin(); x != found.end(); ++x) {
		sys::path path = *x;
		pair<string,string> namepath;
		namepath.second = path.to_string();
		namepath.first = path.leaf().substr (0, path.leaf().find_first_of ('.'));
		files.insert (namepath);
	}
}

bool
ARDOUR::no_auto_connect()
{
	return getenv ("ARDOUR_NO_AUTOCONNECT") != 0;
}

void
ARDOUR::setup_fpu ()
{

	if (getenv ("ARDOUR_RUNNING_UNDER_VALGRIND")) {
		// valgrind doesn't understand this assembler stuff
		// September 10th, 2007
		return;
	}

#if defined(ARCH_X86) && defined(USE_XMMINTRIN)

	int MXCSR;
	FPU fpu;

	/* XXX use real code to determine if the processor supports
	   DenormalsAreZero and FlushToZero
	*/

	if (!fpu.has_flush_to_zero() && !fpu.has_denormals_are_zero()) {
		return;
	}

	MXCSR  = _mm_getcsr();

	switch (Config->get_denormal_model()) {
	case DenormalNone:
		MXCSR &= ~(_MM_FLUSH_ZERO_ON|0x8000);
		break;

	case DenormalFTZ:
		if (fpu.has_flush_to_zero()) {
			MXCSR |= _MM_FLUSH_ZERO_ON;
		}
		break;

	case DenormalDAZ:
		MXCSR &= ~_MM_FLUSH_ZERO_ON;
		if (fpu.has_denormals_are_zero()) {
			MXCSR |= 0x8000;
		}
		break;

	case DenormalFTZDAZ:
		if (fpu.has_flush_to_zero()) {
			if (fpu.has_denormals_are_zero()) {
				MXCSR |= _MM_FLUSH_ZERO_ON | 0x8000;
			} else {
				MXCSR |= _MM_FLUSH_ZERO_ON;
			}
		}
		break;
	}

	_mm_setcsr (MXCSR);

#endif
}

ARDOUR::OverlapType
ARDOUR::coverage (framepos_t sa, framepos_t ea,
		  framepos_t sb, framepos_t eb)
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

	if ((sb > sa) && (eb <= ea)) {
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
	if ((sb > sa) && (sb <= ea)) {
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

