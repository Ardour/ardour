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
#include <locale.h>
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

#include "midi++/port.h"
#include "midi++/manager.h"
#include "midi++/mmc.h"

#include "ardour/ardour.h"
#include "ardour/analyser.h"
#include "ardour/audio_library.h"
#include "ardour/rc_configuration.h"
#include "ardour/profile.h"
#include "ardour/plugin_manager.h"
#include "ardour/audiosource.h"
#include "ardour/utils.h"
#include "ardour/session.h"
#include "ardour/source_factory.h"
#include "ardour/control_protocol_manager.h"
#include "ardour/audioengine.h"
#include "ardour/filesystem_paths.h"

#include "ardour/mix.h"
#include "ardour/runtime_functions.h"

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

Change ARDOUR::StartChanged = ARDOUR::new_change ();
Change ARDOUR::LengthChanged = ARDOUR::new_change ();
Change ARDOUR::PositionChanged = ARDOUR::new_change ();
Change ARDOUR::NameChanged = ARDOUR::new_change ();
Change ARDOUR::BoundsChanged = Change (0); // see init(), below

compute_peak_t          ARDOUR::compute_peak = 0;
find_peaks_t            ARDOUR::find_peaks = 0;
apply_gain_to_buffer_t  ARDOUR::apply_gain_to_buffer = 0;
mix_buffers_with_gain_t ARDOUR::mix_buffers_with_gain = 0;
mix_buffers_no_gain_t   ARDOUR::mix_buffers_no_gain = 0;

sigc::signal<void,std::string> ARDOUR::BootMessage;

int
ARDOUR::setup_midi ()
{
	if (Config->midi_ports.size() == 0) {
		//warning << _("no MIDI ports specified: no MMC or MTC control possible") << endmsg;
		return 0;
	}

	BootMessage (_("Configuring MIDI ports"));

	for (std::map<string,XMLNode>::iterator i = Config->midi_ports.begin(); i != Config->midi_ports.end(); ++i) {
		MIDI::Manager::instance()->add_port (i->second);
	}

	MIDI::Port* first;
	const MIDI::Manager::PortMap& ports = MIDI::Manager::instance()->get_midi_ports();

	if (ports.size() > 1) {

		first = ports.begin()->second;

		/* More than one port, so try using specific names for each port */

		if (Config->get_mmc_port_name() != N_("control")) {
			default_mmc_port =  MIDI::Manager::instance()->port (Config->get_mmc_port_name());
		}

		if (Config->get_mtc_port_name() != N_("control")) {
			default_mtc_port =  MIDI::Manager::instance()->port (Config->get_mtc_port_name());
		}

		if (Config->get_midi_port_name() != N_("control")) {
			default_midi_port =  MIDI::Manager::instance()->port (Config->get_midi_port_name());
		}

		if (Config->get_midi_clock_port_name() != N_("control")) {
			default_midi_port =  MIDI::Manager::instance()->port (Config->get_midi_clock_port_name());
		}

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

		first = ports.begin()->second;

		/* Only one port described, so use it for both MTC and MMC */

		default_mmc_port = first;
		default_mtc_port = default_mmc_port;
		default_midi_port = default_mmc_port;
		default_midi_clock_port = default_mmc_port;
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
				info << string_compose (_("Ardour will be limited to %1 open files"), rl.rlim_cur) << endmsg;
			}
		}
	} else {
		error << string_compose (_("Could not get system open files limit (%1)"), strerror (errno)) << endmsg;
	}
}

int
ARDOUR::init (bool use_vst, bool try_optimization)
{
	extern void setup_enum_writer ();

	(void) bindtextdomain(PACKAGE, LOCALEDIR);

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

	BoundsChanged = Change (StartChanged|PositionChanged|LengthChanged);

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


microseconds_t
ARDOUR::get_microseconds ()
{
	return (microseconds_t) jack_get_time ();
}

ARDOUR::Change
ARDOUR::new_change ()
{
	Change c;
	static uint32_t change_bit = 1;

	/* catch out-of-range */
	if (!change_bit)
	{
		fatal << _("programming error: ")
			<< "change_bit out of range in ARDOUR::new_change()"
			<< endmsg;
		/*NOTREACHED*/
	}

	c = Change (change_bit);
	change_bit <<= 1;	// if it shifts too far, change_bit == 0

	return c;
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
ARDOUR::coverage (nframes_t sa, nframes_t ea,
		  nframes_t sb, nframes_t eb)
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

/* not sure where to put these */

template<class T>
std::istream& int_to_type (std::istream& o, T& hf) {
	int val;
	o >> val;
	hf = (T) val;
	return o;
}

std::istream& operator>>(std::istream& o, HeaderFormat& var) { return int_to_type<HeaderFormat> (o, var); }
std::istream& operator>>(std::istream& o, SampleFormat& var) { return int_to_type<SampleFormat> (o, var); }
std::istream& operator>>(std::istream& o, AutoConnectOption& var) { return int_to_type<AutoConnectOption> (o, var); }
std::istream& operator>>(std::istream& o, MonitorModel& var) { return int_to_type<MonitorModel> (o, var); }
std::istream& operator>>(std::istream& o, RemoteModel& var) { return int_to_type<RemoteModel> (o, var); }
std::istream& operator>>(std::istream& o, EditMode& var) { return int_to_type<EditMode> (o, var); }
std::istream& operator>>(std::istream& o, ListenPosition& var) { return int_to_type<ListenPosition> (o, var); }
std::istream& operator>>(std::istream& o, LayerModel& var) { return int_to_type<LayerModel> (o, var); }
std::istream& operator>>(std::istream& o, CrossfadeModel& var) { return int_to_type<CrossfadeModel> (o, var); }
std::istream& operator>>(std::istream& o, SlaveSource& var) { return int_to_type<SlaveSource> (o, var); }
std::istream& operator>>(std::istream& o, ShuttleBehaviour& var) { return int_to_type<ShuttleBehaviour> (o, var); }
std::istream& operator>>(std::istream& o, ShuttleUnits& var) { return int_to_type<ShuttleUnits> (o, var); }
std::istream& operator>>(std::istream& o, SmpteFormat& var) { return int_to_type<SmpteFormat> (o, var); }
std::istream& operator>>(std::istream& o, DenormalModel& var) { return int_to_type<DenormalModel> (o, var); }
std::istream& operator>>(std::istream& o, WaveformScale& var) { return int_to_type<WaveformScale> (o, var); }
std::istream& operator>>(std::istream& o, WaveformShape& var) { return int_to_type<WaveformShape> (o, var); }

