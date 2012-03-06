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

#ifdef WINDOWS_VST_SUPPORT
#include <fst.h>
#endif

#ifdef LXVST_SUPPORT
#include "ardour/linux_vst_support.h"
#endif

#ifdef AUDIOUNIT_SUPPORT
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
#include "ardour/dB.h"
#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"
#include "ardour/midi_region.h"
#include "ardour/mix.h"
#include "ardour/audioplaylist.h"
#include "ardour/panner_manager.h"
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
			// mix_buffers_with_gain = x86_sse_mix_buffers_with_gain;
			mix_buffers_with_gain = default_mix_buffers_with_gain;
			mix_buffers_no_gain   = x86_sse_mix_buffers_no_gain;

			generic_mix_functions = false;

		}

#elif defined (__APPLE__) && defined (BUILD_VECLIB_OPTIMIZATIONS)
		SInt32 sysVersion = 0;

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
ARDOUR::init (bool use_windows_vst, bool try_optimization)
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
	MidiRegion::make_property_quarks ();
	AudioRegion::make_property_quarks ();
	RouteGroup::make_property_quarks ();
        Playlist::make_property_quarks ();
        AudioPlaylist::make_property_quarks ();

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

	Config->set_use_windows_vst (use_windows_vst);
#ifdef LXVST_SUPPORT
	Config->set_use_lxvst(true);
#endif

	Profile = new RuntimeProfile;


#ifdef WINDOWS_VST_SUPPORT
	if (Config->get_use_windows_vst() && fst_init (0)) {
		return -1;
	}
#endif

#ifdef LXVST_SUPPORT
	if (Config->get_use_lxvst() && vstfx_init (0)) {
		return -1;
	}
#endif

#ifdef AUDIOUNIT_SUPPORT
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
	(void) PluginManager::instance();

        ProcessThread::init ();
        BufferManager::init (10); // XX should be num_processors_for_dsp + 1 for the GUI thread

        PannerManager::instance().discover_panners();

	// Initialize parameter metadata
	EventTypeMap::instance().new_parameter(NullAutomation);
	EventTypeMap::instance().new_parameter(GainAutomation);
	EventTypeMap::instance().new_parameter(PanAzimuthAutomation);
	EventTypeMap::instance().new_parameter(PanElevationAutomation);
	EventTypeMap::instance().new_parameter(PanWidthAutomation);
	EventTypeMap::instance().new_parameter(PluginAutomation);
	EventTypeMap::instance().new_parameter(SoloAutomation);
	EventTypeMap::instance().new_parameter(MuteAutomation);
	EventTypeMap::instance().new_parameter(MidiCCAutomation);
	EventTypeMap::instance().new_parameter(MidiPgmChangeAutomation);
	EventTypeMap::instance().new_parameter(MidiPitchBenderAutomation);
	EventTypeMap::instance().new_parameter(MidiChannelPressureAutomation);
	EventTypeMap::instance().new_parameter(FadeInAutomation);
	EventTypeMap::instance().new_parameter(FadeOutAutomation);
	EventTypeMap::instance().new_parameter(EnvelopeAutomation);
	EventTypeMap::instance().new_parameter(MidiCCAutomation);

	return 0;
}

void
ARDOUR::init_post_engine ()
{
	/* the MIDI Manager is needed by the ControlProtocolManager */
	MIDI::Manager::create (AudioEngine::instance()->jack());

	ControlProtocolManager::instance().discover_control_protocols ();

	XMLNode* node;
	if ((node = Config->control_protocol_state()) != 0) {
		ControlProtocolManager::instance().set_state (*node, Stateful::loading_state_version);
	}

	/* find plugins */

	ARDOUR::PluginManager::instance().refresh ();
}

int
ARDOUR::cleanup ()
{
	delete Library;
	lrdf_cleanup ();
	delete &ControlProtocolManager::instance();
#ifdef WINDOWS_VST_SUPPORT
	fst_exit ();
#endif

#ifdef LXVST_SUPPORT
	vstfx_exit();
#endif
	return 0;
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

#ifdef DEBUG_DENORMAL_EXCEPTION
	/* This will raise a FP exception if a denormal is detected */
	MXCSR &= ~_MM_MASK_DENORM;
#endif	

	switch (Config->get_denormal_model()) {
	case DenormalNone:
		MXCSR &= ~(_MM_FLUSH_ZERO_ON | 0x40);
		break;

	case DenormalFTZ:
		if (fpu.has_flush_to_zero()) {
			MXCSR |= _MM_FLUSH_ZERO_ON;
		}
		break;

	case DenormalDAZ:
		MXCSR &= ~_MM_FLUSH_ZERO_ON;
		if (fpu.has_denormals_are_zero()) {
			MXCSR |= 0x40;
		}
		break;

	case DenormalFTZDAZ:
		if (fpu.has_flush_to_zero()) {
			if (fpu.has_denormals_are_zero()) {
				MXCSR |= _MM_FLUSH_ZERO_ON | 0x40;
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

string
ARDOUR::translation_kill_path ()
{
        return Glib::build_filename (user_config_directory().to_string(), ".love_is_the_language_of_audio");
}

bool
ARDOUR::translations_are_disabled ()
{
        /* if file does not exist, we don't translate (bundled ardour only) */
        return Glib::file_test (translation_kill_path(), Glib::FILE_TEST_EXISTS) == false;
}
