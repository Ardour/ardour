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

#ifdef interface
#undef interface
#endif

#include <cstdio> // Needed so that libraptor (included in lrdf) won't complain
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#ifndef PLATFORM_WINDOWS
#include <sys/resource.h>
#endif
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include <glib.h>
#include "pbd/gstdio_compat.h"

#ifdef PLATFORM_WINDOWS
#include <stdio.h> // for _setmaxstdio
#include <windows.h> // for LARGE_INTEGER
#endif

#ifdef WINDOWS_VST_SUPPORT
#include <fst.h>
#endif

#ifdef LXVST_SUPPORT
#include "ardour/linux_vst_support.h"
#endif

#ifdef AUDIOUNIT_SUPPORT
#include "ardour/audio_unit.h"
#endif

#if defined(__SSE__) || defined(USE_XMMINTRIN)
#include <xmmintrin.h>
#endif

#ifdef check
#undef check /* stupid Apple and their un-namespaced, generic Carbon macros */
#endif

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#ifdef HAVE_LRDF
#include <lrdf.h>
#endif

#include "pbd/cpus.h"
#include "pbd/error.h"
#include "pbd/id.h"
#include "pbd/pbd.h"
#include "pbd/strsplit.h"
#include "pbd/fpu.h"
#include "pbd/file_utils.h"
#include "pbd/enumwriter.h"

#include "midi++/port.h"
#include "midi++/mmc.h"

#include "ardour/analyser.h"
#include "ardour/audio_library.h"
#include "ardour/audio_backend.h"
#include "ardour/audioengine.h"
#include "ardour/audioplaylist.h"
#include "ardour/audioregion.h"
#include "ardour/buffer_manager.h"
#include "ardour/control_protocol_manager.h"
#include "ardour/directory_names.h"
#include "ardour/event_type_map.h"
#include "ardour/filesystem_paths.h"
#include "ardour/midi_region.h"
#include "ardour/midi_ui.h"
#include "ardour/midiport_manager.h"
#include "ardour/mix.h"
#include "ardour/operations.h"
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
#include "ardour/uri_map.h"

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

bool libardour_initialized = false;

compute_peak_t          ARDOUR::compute_peak = 0;
find_peaks_t            ARDOUR::find_peaks = 0;
apply_gain_to_buffer_t  ARDOUR::apply_gain_to_buffer = 0;
mix_buffers_with_gain_t ARDOUR::mix_buffers_with_gain = 0;
mix_buffers_no_gain_t   ARDOUR::mix_buffers_no_gain = 0;
copy_vector_t			ARDOUR::copy_vector = 0;

PBD::Signal1<void,std::string> ARDOUR::BootMessage;
PBD::Signal3<void,std::string,std::string,bool> ARDOUR::PluginScanMessage;
PBD::Signal1<void,int> ARDOUR::PluginScanTimeout;
PBD::Signal0<void> ARDOUR::GUIIdle;
PBD::Signal3<bool,std::string,std::string,int> ARDOUR::CopyConfigurationFiles;

std::vector<std::string> ARDOUR::reserved_io_names;

static bool have_old_configuration_files = false;

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

		FPU* fpu = FPU::instance();

#if defined (ARCH_X86) && defined (BUILD_SSE_OPTIMIZATIONS)

#ifdef PLATFORM_WINDOWS
		/* We have AVX-optimized code for Windows */

		if (fpu->has_avx()) {
#else
		/* AVX code doesn't compile on Linux yet */

		if (false) {
#endif
			info << "Using AVX optimized routines" << endmsg;

			// AVX SET
			compute_peak          = x86_sse_avx_compute_peak;
			find_peaks            = x86_sse_avx_find_peaks;
			apply_gain_to_buffer  = x86_sse_avx_apply_gain_to_buffer;
			mix_buffers_with_gain = x86_sse_avx_mix_buffers_with_gain;
			mix_buffers_no_gain   = x86_sse_avx_mix_buffers_no_gain;
			copy_vector           = x86_sse_avx_copy_vector;

			generic_mix_functions = false;

		} else if (fpu->has_sse()) {

			info << "Using SSE optimized routines" << endmsg;

			// SSE SET
			compute_peak          = x86_sse_compute_peak;
			find_peaks            = x86_sse_find_peaks;
			apply_gain_to_buffer  = x86_sse_apply_gain_to_buffer;
			mix_buffers_with_gain = x86_sse_mix_buffers_with_gain;
			mix_buffers_no_gain   = x86_sse_mix_buffers_no_gain;
			copy_vector           = default_copy_vector;

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
			copy_vector            = default_copy_vector;

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
		copy_vector           = default_copy_vector;

		info << "No H/W specific optimizations in use" << endmsg;
	}

	AudioGrapher::Routines::override_compute_peak (compute_peak);
	AudioGrapher::Routines::override_apply_gain_to_buffer (apply_gain_to_buffer);
}

static void
lotsa_files_please ()
{
#ifndef PLATFORM_WINDOWS
	struct rlimit rl;

	if (getrlimit (RLIMIT_NOFILE, &rl) == 0) {

#ifdef __APPLE__
                /* See the COMPATIBILITY note on the Apple setrlimit() man page */
		rl.rlim_cur = min ((rlim_t) OPEN_MAX, rl.rlim_max);
#else
		rl.rlim_cur = rl.rlim_max;
#endif

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
#else
	/* this only affects stdio. 2048 is the maxium possible (512 the default).
	 *
	 * If we want more, we'll have to replaces the POSIX I/O interfaces with
	 * Win32 API calls (CreateFile, WriteFile, etc) which allows for 16K.
	 *
	 * see http://stackoverflow.com/questions/870173/is-there-a-limit-on-number-of-open-files-in-windows
	 * and http://bugs.mysql.com/bug.php?id=24509
	 */
	int newmax = _setmaxstdio (2048);
	if (newmax > 0) {
		info << string_compose (_("Your system is configured to limit %1 to only %2 open files"), PROGRAM_NAME, newmax) << endmsg;
	} else {
		error << string_compose (_("Could not set system open files limit. Current limit is %1 open files"), _getmaxstdio)  << endmsg;
	}
#endif
}

static int
copy_configuration_files (string const & old_dir, string const & new_dir, int old_version)
{
	string old_name;
	string new_name;

	/* ensure target directory exists */

	if (g_mkdir_with_parents (new_dir.c_str(), 0755)) {
		return -1;
	}

	if (old_version == 3) {

		old_name = Glib::build_filename (old_dir, X_("recent"));
		new_name = Glib::build_filename (new_dir, X_("recent"));

		copy_file (old_name, new_name);

		old_name = Glib::build_filename (old_dir, X_("sfdb"));
		new_name = Glib::build_filename (new_dir, X_("sfdb"));

		copy_file (old_name, new_name);

		/* can only copy ardour.rc/config - UI config is not compatible */

		/* users who have been using git/nightlies since the last
		 * release of 3.5 will have $CONFIG/config rather than
		 * $CONFIG/ardour.rc. Pick up the newer "old" config file,
		 * to avoid confusion.
		 */

		string old_name = Glib::build_filename (old_dir, X_("config"));

		if (!Glib::file_test (old_name, Glib::FILE_TEST_EXISTS)) {
			old_name = Glib::build_filename (old_dir, X_("ardour.rc"));
		}

		new_name = Glib::build_filename (new_dir, X_("config"));

		copy_file (old_name, new_name);

		/* copy templates and route templates */

		old_name = Glib::build_filename (old_dir, X_("templates"));
		new_name = Glib::build_filename (new_dir, X_("templates"));

		copy_recurse (old_name, new_name);

		old_name = Glib::build_filename (old_dir, X_("route_templates"));
		new_name = Glib::build_filename (new_dir, X_("route_templates"));

		copy_recurse (old_name, new_name);

		/* presets */

		old_name = Glib::build_filename (old_dir, X_("presets"));
		new_name = Glib::build_filename (new_dir, X_("presets"));

		copy_recurse (old_name, new_name);

		/* presets */

		old_name = Glib::build_filename (old_dir, X_("plugin_statuses"));
		new_name = Glib::build_filename (new_dir, X_("plugin_statuses"));

		copy_file (old_name, new_name);

		/* export formats */

		old_name = Glib::build_filename (old_dir, export_formats_dir_name);
		new_name = Glib::build_filename (new_dir, export_formats_dir_name);

		vector<string> export_formats;
		g_mkdir_with_parents (Glib::build_filename (new_dir, export_formats_dir_name).c_str(), 0755);
		find_files_matching_pattern (export_formats, old_name, X_("*.format"));
		for (vector<string>::iterator i = export_formats.begin(); i != export_formats.end(); ++i) {
			std::string from = *i;
			std::string to = Glib::build_filename (new_name, Glib::path_get_basename (*i));
			copy_file (from, to);
		}
	}

	return 0;
}

void
ARDOUR::check_for_old_configuration_files ()
{
	int current_version = atoi (X_(PROGRAM_VERSION));

	if (current_version <= 1) {
		return;
	}

	int old_version = current_version - 1;

	string old_config_dir = user_config_directory (old_version);
	/* pass in the current version explicitly to avoid creation */
	string current_config_dir = user_config_directory (current_version);

	if (!Glib::file_test (current_config_dir, Glib::FILE_TEST_IS_DIR)) {
		if (Glib::file_test (old_config_dir, Glib::FILE_TEST_IS_DIR)) {
			have_old_configuration_files = true;
		}
	}
}

int
ARDOUR::handle_old_configuration_files (boost::function<bool (std::string const&, std::string const&, int)> ui_handler)
{
	if (have_old_configuration_files) {
		int current_version = atoi (X_(PROGRAM_VERSION));
		assert (current_version > 1); // established in check_for_old_configuration_files ()
		int old_version = current_version - 1;
		string old_config_dir = user_config_directory (old_version);
		string current_config_dir = user_config_directory (current_version);

		if (ui_handler (old_config_dir, current_config_dir, old_version)) {
			copy_configuration_files (old_config_dir, current_config_dir, old_version);
			return 1;
		}
	}
	return 0;
}

bool
ARDOUR::init (bool use_windows_vst, bool try_optimization, const char* localedir)
{
	if (libardour_initialized) {
		return true;
	}

	if (!PBD::init()) return false;

#ifdef ENABLE_NLS
	(void) bindtextdomain(PACKAGE, localedir);
	(void) bind_textdomain_codeset (PACKAGE, "UTF-8");
#endif

	SessionEvent::init_event_pool ();

	Operations::make_operations_quarks ();
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

#ifdef HAVE_LRDF
	lrdf_init();
#endif
	Library = new AudioLibrary;

	BootMessage (_("Loading configuration"));

	Config = new RCConfiguration;

	if (Config->load_state ()) {
		return false;
	}

	Config->set_use_windows_vst (use_windows_vst);
#ifdef LXVST_SUPPORT
	Config->set_use_lxvst(true);
#endif

	Profile = new RuntimeProfile;


#ifdef WINDOWS_VST_SUPPORT
	if (Config->get_use_windows_vst() && fst_init (0)) {
		return false;
	}
#endif

#ifdef LXVST_SUPPORT
	if (Config->get_use_lxvst() && vstfx_init (0)) {
		return false;
	}
#endif

#ifdef AUDIOUNIT_SUPPORT
	AUPluginInfo::load_cached_info ();
#endif

	setup_hardware_optimization (try_optimization);

	SourceFactory::init ();
	Analyser::init ();

	/* singletons - first object is "it" */
	(void) PluginManager::instance();
#ifdef LV2_SUPPORT
	(void) URIMap::instance();
#endif
	(void) EventTypeMap::instance();

	ControlProtocolManager::instance().discover_control_protocols ();

	/* for each control protocol, check for a request buffer factory method
	   and if it exists, store it in the EventLoop list of such
	   methods. This allows the relevant threads to register themselves
	   with EventLoops so that signal emission can be RT-safe.
	*/

	ControlProtocolManager::instance().register_request_buffer_factories ();
	/* it would be nice if this could auto-register itself in the
	   constructor, since MidiControlUI is a singleton, but it can't be
	   created until after the engine is running. Therefore we have to
	   explicitly register it here.
	*/
	EventLoop::register_request_buffer_factory (X_("midiUI"), MidiControlUI::request_factory);

        ProcessThread::init ();
	/* the + 4 is a bit of a handwave. i don't actually know
	   how many more per-thread buffer sets we need above
	   the h/w concurrency, but its definitely > 1 more.
	*/
        BufferManager::init (hardware_concurrency() + 4);

        PannerManager::instance().discover_panners();

	ARDOUR::AudioEngine::create ();

	/* it is unfortunate that we need to include reserved names here that
	   refer to control surfaces. But there's no way to ensure a complete
	   lack of collisions without doing this, since the control surface
	   support may not even be active. Without adding an API to control
	   surface support that would list their port names, we do have to
	   list them here.
	*/

	char const * const reserved[] = {
		_("Monitor"),
		_("Master"),
		_("Control"),
		_("Click"),
		_("Mackie"),
		0
	};

	for (int n = 0; reserved[n]; ++n) {
		reserved_io_names.push_back (reserved[n]);
	}

	libardour_initialized = true;

	return true;
}

void
ARDOUR::init_post_engine ()
{
	XMLNode* node;
	if ((node = Config->control_protocol_state()) != 0) {
		ControlProtocolManager::instance().set_state (*node, Stateful::loading_state_version);
	}

	/* find plugins */

	ARDOUR::PluginManager::instance().refresh (!Config->get_discover_vst_on_start());
}

void
ARDOUR::cleanup ()
{
	if (!libardour_initialized) {
		return;
	}

	ARDOUR::AudioEngine::destroy ();

	delete Library;
#ifdef HAVE_LRDF
	lrdf_cleanup ();
#endif
	delete &ControlProtocolManager::instance();
#ifdef WINDOWS_VST_SUPPORT
	fst_exit ();
#endif

#ifdef LXVST_SUPPORT
	vstfx_exit();
#endif
	delete &PluginManager::instance();
	delete Config;
	PBD::cleanup ();

	return;
}

bool
ARDOUR::no_auto_connect()
{
	return getenv ("ARDOUR_NO_AUTOCONNECT") != 0;
}

void
ARDOUR::setup_fpu ()
{
	FPU* fpu = FPU::instance ();

	if (getenv ("ARDOUR_RUNNING_UNDER_VALGRIND")) {
		// valgrind doesn't understand this assembler stuff
		// September 10th, 2007
		return;
	}

#if defined(ARCH_X86) && defined(USE_XMMINTRIN)

	int MXCSR;

	if (!fpu->has_flush_to_zero() && !fpu->has_denormals_are_zero()) {
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
		if (fpu->has_flush_to_zero()) {
			MXCSR |= _MM_FLUSH_ZERO_ON;
		}
		break;

	case DenormalDAZ:
		MXCSR &= ~_MM_FLUSH_ZERO_ON;
		if (fpu->has_denormals_are_zero()) {
			MXCSR |= 0x40;
		}
		break;

	case DenormalFTZDAZ:
		if (fpu->has_flush_to_zero()) {
			if (fpu->has_denormals_are_zero()) {
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
	int fd = g_open (ARDOUR::translation_enable_path().c_str(), O_RDONLY, 0444);

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
	int fd = g_open (i18n_enabler.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0644);

	if (fd < 0) {
		return false;
	}

	char c;

	if (yn) {
		c = '1';
	} else {
		c = '0';
	}

	(void) ::write (fd, &c, 1);
	(void) ::close (fd);

	return true;
}


vector<SyncSource>
ARDOUR::get_available_sync_options ()
{
	vector<SyncSource> ret;

	boost::shared_ptr<AudioBackend> backend = AudioEngine::instance()->current_backend();
	if (backend && backend->name() == "JACK") {
		ret.push_back (Engine);
	}

	ret.push_back (MTC);
	ret.push_back (MIDIClock);
	ret.push_back (LTC);

	return ret;
}

/** Return a monotonic value for the number of microseconds that have elapsed
 * since an arbitrary zero origin.
 */

#ifdef __MACH__
/* Thanks Apple for not implementing this basic SUSv2, POSIX.1-2001 function
 */
#include <mach/mach_time.h>
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 0
int
clock_gettime (int /*clk_id*/, struct timespec *t)
{
        static bool initialized = false;
        static mach_timebase_info_data_t timebase;
        if (!initialized) {
                mach_timebase_info(&timebase);
                initialized = true;
        }
        uint64_t time;
        time = mach_absolute_time();
        double nseconds = ((double)time * (double)timebase.numer)/((double)timebase.denom);
        double seconds = ((double)time * (double)timebase.numer)/((double)timebase.denom * 1e9);
        t->tv_sec = seconds;
        t->tv_nsec = nseconds;
        return 0;
}
#endif

microseconds_t
ARDOUR::get_microseconds ()
{
#ifdef PLATFORM_WINDOWS
	microseconds_t ret = 0;
	LARGE_INTEGER freq, time;

	if (QueryPerformanceFrequency(&freq))
		if (QueryPerformanceCounter(&time))
			ret = (microseconds_t)((time.QuadPart * 1000000) / freq.QuadPart);

	return ret;
#else
	struct timespec ts;
	if (clock_gettime (CLOCK_MONOTONIC, &ts) != 0) {
		/* EEEK! */
		return 0;
	}
	return (microseconds_t) ts.tv_sec * 1000000 + (ts.tv_nsec/1000);
#endif
}

/** Return the number of bits per sample for a given sample format.
 *
 * This is closely related to sndfile_data_width() but does NOT
 * return a "magic" value to differentiate between 32 bit integer
 * and 32 bit floating point values.
 */

int
ARDOUR::format_data_width (ARDOUR::SampleFormat format)
{



	switch (format) {
	case ARDOUR::FormatInt16:
		return 16;
	case ARDOUR::FormatInt24:
		return 24;
	default:
		return 32;
	}
}
