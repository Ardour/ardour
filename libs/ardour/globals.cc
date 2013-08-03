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
#include <cstdlib>
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

#ifdef check
#undef check /* stupid Apple and their un-namespaced, generic Carbon macros */
#endif 

#include <giomm.h>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include <lrdf.h>

#include "pbd/cpus.h"
#include "pbd/error.h"
#include "pbd/id.h"
#include "pbd/strsplit.h"
#include "pbd/fpu.h"
#include "pbd/file_utils.h"
#include "pbd/enumwriter.h"
#include "pbd/basename.h"

#include "midi++/port.h"
#include "midi++/manager.h"
#include "midi++/mmc.h"

#include "ardour/analyser.h"
#include "ardour/audio_library.h"
#include "ardour/audio_backend.h"
#include "ardour/audioengine.h"
#include "ardour/audioplaylist.h"
#include "ardour/audioregion.h"
#include "ardour/buffer_manager.h"
#include "ardour/control_protocol_manager.h"
#include "ardour/filesystem_paths.h"
#include "ardour/midi_region.h"
#include "ardour/mix.h"
#include "ardour/panner_manager.h"
#include "ardour/plugin_manager.h"
#include "ardour/process_thread.h"
#include "ardour/profile.h"
#include "ardour/rc_configuration.h"
#include "ardour/region.h"
#include "ardour/route_group.h"
#include "ardour/runtime_functions.h"
#include "ardour/session_event.h"
#include "ardour/source_factory.h"

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
PBD::Signal0<void> ARDOUR::GUIIdle;

namespace ARDOUR {
extern void setup_enum_writer ();
}

/* this is useful for quite a few things that want to check
   if any bounds-related property has changed
*/
PBD::PropertyChange ARDOUR::bounds_change;

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
			if (rl.rlim_cur != RLIM_INFINITY) {
				info << string_compose (_("Your system is configured to limit %1 to only %2 open files"), PROGRAM_NAME, rl.rlim_cur) << endmsg;
			}
		}
	} else {
		error << string_compose (_("Could not get system open files limit (%1)"), strerror (errno)) << endmsg;
	}
}

int
ARDOUR::init (bool use_windows_vst, bool try_optimization, const char* localedir)
{
	if (!Glib::thread_supported()) {
		Glib::thread_init();
	}

	// this really should be in PBD::init..if there was one
	Gio::init ();

#ifdef ENABLE_NLS
	(void) bindtextdomain(PACKAGE, localedir);
#endif

	PBD::ID::init ();
	SessionEvent::init_event_pool ();

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

	ARDOUR::setup_enum_writer ();

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

	setup_hardware_optimization (try_optimization);

	SourceFactory::init ();
	Analyser::init ();

	/* singleton - first object is "it" */
	(void) PluginManager::instance();

        ProcessThread::init ();
	/* the + 4 is a bit of a handwave. i don't actually know
	   how many more per-thread buffer sets we need above
	   the h/w concurrency, but its definitely > 1 more.
	*/
        BufferManager::init (hardware_concurrency() + 4); 

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

	ARDOUR::AudioEngine::create ();

	return 0;
}

void
ARDOUR::init_post_engine ()
{
	/* the MIDI Manager is needed by the ControlProtocolManager */
	MIDI::Manager::create (AudioEngine::instance()->port_engine());

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
	EnumWriter::destroy ();
	return 0;
}

void
ARDOUR::find_bindings_files (map<string,string>& files)
{
	vector<std::string> found;
	SearchPath spath = ardour_config_search_path();

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

	for (vector<std::string>::iterator x = found.begin(); x != found.end(); ++x) {
		std::string path(*x);
		pair<string,string> namepath;
		namepath.second = path;
		namepath.first = PBD::basename_nosuffix (path);
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

/* this can be changed to modify the translation behaviour for
   cases where the user has never expressed a preference.
*/
static const bool translate_by_default = true;

string
ARDOUR::translation_enable_path ()
{
        return Glib::build_filename (user_config_directory(), ".translate");
}

bool
ARDOUR::translations_are_enabled ()
{
	int fd = ::open (ARDOUR::translation_enable_path().c_str(), O_RDONLY);

	if (fd < 0) {
		return translate_by_default;
	}

	char c;
	bool ret = false;

	if (::read (fd, &c, 1) == 1 && c == '1') {
		ret = true;
	}

	::close (fd);

	return ret;
}

bool
ARDOUR::set_translations_enabled (bool yn)
{
	string i18n_enabler = ARDOUR::translation_enable_path();
	int fd = ::open (i18n_enabler.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0644);

	if (fd < 0) {
		return false;
	}
	
	char c;
	
	if (yn) {
		c = '1';
	} else {
		c = '0';
	}
	
	::write (fd, &c, 1);
	::close (fd);

	return true;
}


vector<SyncSource>
ARDOUR::get_available_sync_options ()
{
	vector<SyncSource> ret;

	ret.push_back (JACK);
	ret.push_back (MTC);
	ret.push_back (MIDIClock);
	ret.push_back (LTC);

	return ret;
}
