#include <assert.h>
#include <getopt.h>
#include <stdint.h>

#include <cstdio>
#include <iostream>
#include <list>
#include <string>
#include <vector>

#ifdef PLATFORM_WINDOWS
# include <io.h>
# include <windows.h>
#else
# include <unistd.h>
#endif

#include <glibmm.h>

#include "pbd/basename.h"
#include "pbd/debug.h"
#include "pbd/error.h"
#include "pbd/event_loop.h"
#include "pbd/failed_constructor.h"
#include "pbd/pthread_utils.h"
#include "pbd/reallocpool.h"
#include "pbd/receiver.h"
#include "pbd/transmitter.h"
#include "pbd/win_console.h"

#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/filename_extensions.h"
#include "ardour/filesystem_paths.h"
#include "ardour/luabindings.h"
#include "ardour/session.h"
#include "ardour/types.h"
#include "ardour/vst_types.h"

#include <readline/history.h>
#include <readline/readline.h>

#include "LuaBridge/LuaBridge.h"
#include "lua/luastate.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

static const char*               localedir = LOCALEDIR;
static PBD::ScopedConnectionList engine_connections;
static PBD::ScopedConnectionList session_connections;
static Session*                  session = NULL;
static LuaState*                 lua;

static bool keep_running          = true;
static bool terminate_when_halted = false;

/* extern VST functions */
int vstfx_init (void*) { return 0; }
void vstfx_exit () {}
void vstfx_destroy_editor (VSTState*) {}

class LuaReceiver : public Receiver
{
protected:
	void receive (Transmitter::Channel chn, const char* str)
	{
		const char* prefix = "";

		switch (chn) {
			case Transmitter::Debug:
				/* ignore */
				return;
			case Transmitter::Info:
				/* ignore */
				return;
			case Transmitter::Warning:
				prefix = "[WARNING]: ";
				break;
			case Transmitter::Error:
				prefix = "[ERROR]: ";
				break;
			case Transmitter::Fatal:
				prefix = "[FATAL]: ";
				break;
			case Transmitter::Throw:
				/* this isn't supposed to happen */
				abort ();
		}

		/* note: iostreams are already thread-safe: no external
		 * lock required. */
		std::cout << prefix << str << std::endl;

		if (chn == Transmitter::Fatal) {
			console_madness_end ();
			::exit (9);
		}
	}
};

class MyEventLoop : public sigc::trackable, public EventLoop
{
public:
	MyEventLoop (std::string const& name)
	    : EventLoop (name)
	{
		run_loop_thread = Glib::Threads::Thread::self ();
	}

	void call_slot (InvalidationRecord* ir, const boost::function<void()>& f)
	{
		if (Glib::Threads::Thread::self () == run_loop_thread) {
			cout << string_compose ("%1/%2 direct dispatch of call slot via functor @ %3, invalidation %4\n", event_loop_name (), pthread_name (), &f, ir);
			f ();
		} else {
			cout << string_compose ("%1/%2 queue call-slot using functor @ %3, invalidation %4\n", event_loop_name (), pthread_name (), &f, ir);
			assert (!ir);
			f (); // XXX TODO, queue and process during run ()
		}
	}

	void run ()
	{
		; // TODO process Events, if any
	}

	Glib::Threads::Mutex& slot_invalidation_mutex ()
	{
		return request_buffer_map_lock;
	}

private:
	Glib::Threads::Thread* run_loop_thread;
	Glib::Threads::Mutex   request_buffer_map_lock;
};

static MyEventLoop* event_loop = NULL;

/* ****************************************************************************/
/* internal helper fn and callbacks */

static void
init ()
{
	if (!ARDOUR::init (true, localedir)) {
		cerr << "Ardour failed to initialize\n" << endl;
		console_madness_end ();
		::exit (EXIT_FAILURE);
	}

	assert (!event_loop);
	event_loop = new MyEventLoop ("lua");
	EventLoop::set_event_loop_for_thread (event_loop);
	SessionEvent::create_per_thread_pool ("lua", 4096);

	static LuaReceiver lua_receiver;

	lua_receiver.listen_to (warning);
	lua_receiver.listen_to (error);
	lua_receiver.listen_to (fatal);
}

static void
set_session (ARDOUR::Session* s)
{
	session = s;
	assert (lua);
	lua_State* L = lua->getState ();
	LuaBindings::set_session (L, session);
	lua->collect_garbage (); // drop references
}

static void
unset_session ()
{
	session_connections.drop_connections ();
	set_session (NULL);
}

static int
prepare_engine ()
{
	AudioEngine* engine = AudioEngine::instance ();

	if (!engine->current_backend ()) {
		if (!engine->set_backend ("None (Dummy)", "Unit-Test", "")) {
			std::cerr << "Cannot create Audio/MIDI engine\n";
			return -1;
		}
	}

	if (!engine->current_backend ()) {
		std::cerr << "Cannot create Audio/MIDI engine\n";
		return -1;
	}

	if (engine->running ()) {
		engine->stop ();
	}
	return 0;
}

static int
start_engine (uint32_t rate)
{
	AudioEngine* engine = AudioEngine::instance ();

	if (engine->set_sample_rate (rate)) {
		std::cerr << "Cannot set session's samplerate.\n";
		return -1;
	}

	if (engine->start () != 0) {
		std::cerr << "Cannot start Audio/MIDI engine\n";
		return -1;
	}

	return 0;
}

static Session*
_create_session (string dir, string state, uint32_t rate) // throws
{
	if (prepare_engine ()) {
		return 0;
	}

	std::string s = Glib::build_filename (dir, state + statefile_suffix);
	if (Glib::file_test (dir, Glib::FILE_TEST_EXISTS)) {
		std::cerr << "Session already exists: " << s << "\n";
		return 0;
	}

	if (start_engine (rate)) {
		return 0;
	}

	// TODO add option/bindings for this
	BusProfile bus_profile;
	bus_profile.master_out_channels = 2;

	AudioEngine* engine  = AudioEngine::instance ();
	Session*     session = new Session (*engine, dir, state, &bus_profile);
	return session;
}

static Session*
_load_session (string const& dir, string state) // throws
{
	if (prepare_engine ()) {
		return 0;
	}

	if (state.empty ()) {
		state = Session::get_snapshot_from_instant (dir);
	}
	if (state.empty ()) {
		state = PBD::basename_nosuffix (dir);
	}

	float        sr;
	SampleFormat sf;
	std::string  v;
	std::string  s = Glib::build_filename (dir, state + statefile_suffix);
	if (!Glib::file_test (dir, Glib::FILE_TEST_EXISTS)) {
		std::cerr << "Cannot find session: " << s << "\n";
		return 0;
	}

	if (Session::get_info_from_path (s, sr, sf, v) != 0) {
		std::cerr << "Cannot get samplerate from session.\n";
		return 0;
	}

	if (start_engine (sr)) {
		return 0;
	}

	AudioEngine* engine  = AudioEngine::instance ();
	Session*     session = new Session (*engine, dir, state);
	return session;
}

/* ****************************************************************************/
/* lua bound functions */

static Session*
create_session (string dir, string state, uint32_t rate)
{
	Session* s = 0;
	if (session) {
		cerr << "Session already open"
		     << "\n";
		return 0;
	}
	try {
		s = _create_session (dir, state, rate);
	} catch (failed_constructor& e) {
		cerr << "failed_constructor: " << e.what () << "\n";
		return 0;
	} catch (AudioEngine::PortRegistrationFailure& e) {
		cerr << "PortRegistrationFailure: " << e.what () << "\n";
		return 0;
	} catch (exception& e) {
		cerr << "exception: " << e.what () << "\n";
		return 0;
	} catch (...) {
		cerr << "unknown exception.\n";
		return 0;
	}
	Glib::usleep (1000000); // allow signal propagation, callback/thread-pool setup
	if (!s) {
		return 0;
	}
	set_session (s);
	s->DropReferences.connect_same_thread (session_connections, &unset_session);
	return s;
}

static Session*
load_session (string dir, string state)
{
	Session* s = 0;
	if (session) {
		cerr << "Session already open"
		     << "\n";
		return 0;
	}
	try {
		s = _load_session (dir, state);
	} catch (failed_constructor& e) {
		cerr << "failed_constructor: " << e.what () << "\n";
		return 0;
	} catch (AudioEngine::PortRegistrationFailure& e) {
		cerr << "PortRegistrationFailure: " << e.what () << "\n";
		return 0;
	} catch (exception& e) {
		cerr << "exception: " << e.what () << "\n";
		return 0;
	} catch (...) {
		cerr << "unknown exception.\n";
		return 0;
	}
	Glib::usleep (1000000); // allow signal propagation, callback/thread-pool setup
	if (!s) {
		return 0;
	}
	set_session (s);
	s->DropReferences.connect_same_thread (session_connections, &unset_session);
	return s;
}

static int
set_debug_options (const char* opts)
{
	return PBD::parse_debug_options (opts);
}

static void
close_session ()
{
	delete session;
	assert (!session);
}

static int
close_session_lua (lua_State* L)
{
	if (!session) {
		cerr << "No open session"
		     << "\n";
		return 0;
	}
	close_session ();
	return 0;
}

static void
delay (float d)
{
	if (d > 0) {
		Glib::usleep (d * 1000000);
	}
}

static int
do_quit (lua_State* L)
{
	keep_running = false;
	return 0;
}

static void
engine_halted (const char* err)
{
	if (terminate_when_halted) {
		cerr << "Engine halted: " << err << "\n";
		console_madness_end ();
		::exit (EXIT_FAILURE);
	}
}

/* ****************************************************************************/

static void
my_lua_print (std::string s)
{
	std::cout << s << "\n";
}

static void
setup_lua ()
{
	assert (!lua);

	lua = new LuaState ();
	lua->Print.connect (&my_lua_print);
	lua_State* L = lua->getState ();

	LuaBindings::stddef (L);
	LuaBindings::common (L);
	LuaBindings::session (L);
	LuaBindings::osc (L);

	luabridge::getGlobalNamespace (L)
	    .beginNamespace ("_G")
	    .addFunction ("create_session", &create_session)
	    .addFunction ("load_session", &load_session)
	    .addFunction ("close_session", &close_session)
	    .addFunction ("sleep", &delay)
	    .addFunction ("quit", &do_quit)
	    .addFunction ("set_debug_options", &set_debug_options)
	    .endNamespace ();

	// add a Session::close() method
	luabridge::getGlobalNamespace (L)
	    .beginNamespace ("ARDOUR")
	    .beginClass<Session> ("Session")
	    .addExtCFunction ("close", &close_session_lua)
	    .endClass ()
	    .endNamespace ();

	// push instance to global namespace (C++ lifetime)
	luabridge::push<AudioEngine*> (L, AudioEngine::create ());
	lua_setglobal (L, "AudioEngine");

	AudioEngine::instance ()->stop ();

	AudioEngine::instance()->Halted.connect_same_thread (engine_connections, boost::bind (&engine_halted, _1));
}

static int
incomplete (lua_State* L, int status)
{
	if (status == LUA_ERRSYNTAX) {
		size_t      lmsg;
		const char* msg = lua_tolstring (L, -1, &lmsg);
		if (lmsg >= 5 && strcmp (msg + lmsg - 5, "<eof>") == 0) {
			lua_pop (L, 1);
			return 1;
		}
	}
	return 0;
}

static void
interactive_interpreter ()
{
	using_history ();
	std::string histfile = Glib::build_filename (user_config_directory (), "/luahist");

	rl_bind_key ('\t', rl_insert); // disable completion
	read_history (histfile.c_str ());

	char* line = NULL;
	while (keep_running && (line = readline ("> "))) {
		event_loop->run ();
		if (!strcmp (line, "quit")) {
			free (line);
			line = NULL;
			break;
		}

		if (strlen (line) == 0) {
			free (line);
			line = NULL;
			continue;
		}

		do {
			LuaState   lt;
			lua_State* L      = lt.getState ();
			int        status = luaL_loadbuffer (L, line, strlen (line), "=stdin");
			if (!incomplete (L, status)) {
				break;
			}
			char* l2 = readline (">> ");
			if (!l2) {
				break;
			}
			if (strlen (l2) == 0) {
				continue;
			}
			line = (char*)realloc ((void*)line, (strlen (line) + strlen (l2) + 2) * sizeof (char));
			strcat (line, "\n");
			strcat (line, l2);
			free (l2);
		} while (1);

		if (lua->do_command (line)) {
			/* error */
			free (line);
			line = NULL;
			continue;
		}

		add_history (line);
		event_loop->run ();
		free (line);
		line = NULL;
	}
	free (line);
	printf ("\n");
	write_history (histfile.c_str ());
}

static bool
is_tty ()
{
#ifdef PLATFORM_WINDOWS
	return _isatty (_fileno (stdin));
#else
	return isatty (0);
#endif
}

static void
usage ()
{
	printf ("ardour-lua - interactive Ardour Lua interpreter.\n\n");
	printf ("Usage: ardour-lua [ OPTIONS ] [ file [args] ]\n\n");
/*        1         2         3         4         5         6         7         8
 *2345678901234567890123456789012345678901234567890123456789012345678901234567890*/
  printf ("Options:\n\
  -h, --help                 display this help and exit\n\
  -i, --interactive          enter interactive mode after executing 'script',\n\
                             force the interpreter to run interactively\n\
  -X, --exit-when-halted     terminate when the audio-engine halts\n\
                             unexpectedly (disconnect, or too many xruns)\n\
  -V, --version              print version information and exit\n\
\n");
	printf ("\n\
Ardour at your finger tips...\n\
\n");
	printf ("Report bugs to <http://tracker.ardour.org/>\n"
	        "Website: <http://ardour.org/>\n");
	console_madness_end ();
	::exit (EXIT_SUCCESS);
}

int
main (int argc, char** argv)
{
	const char* optstring = "hiVX";

	const struct option longopts[] = {
		{ "help",             0, 0, 'h' },
		{ "interactive",      0, 0, 'i' },
		{ "version",          0, 0, 'V' },
		{ "exit-when-halted", 0, 0, 'X' },
	};

	bool interactive = false;
	console_madness_begin ();

	int c = 0;
	while (EOF != (c = getopt_long (argc, argv,
	                                optstring, longopts, (int*)0))) {
		switch (c) {
			case 'h':
				usage ();
				break;

			case 'i':
				interactive = true;
				break;

			case 'V':
				printf ("ardour-lua version %s\n\n", VERSIONSTRING);
				printf ("Copyright (C) GPL 2015-2020 Robin Gareus <robin@gareus.org>\n");
				console_madness_end ();
				exit (EXIT_SUCCESS);
				break;

			case 'X':
				terminate_when_halted = true;
				break;

			default:
				cerr << "Error: unrecognized option. See --help for usage information.\n";
				console_madness_end ();
				::exit (EXIT_FAILURE);
				break;
		}
	}

	init ();
	setup_lua ();

	{
		/* push arguments to script, use scoped LuaRef */
		lua_State* L = lua->getState ();
		luabridge::LuaRef arg (luabridge::newTable (L));
		for (int i = 1; i < argc - optind; ++i) {
			arg[i] = std::string (argv[i + optind]);
		}
		luabridge::push (L, arg);
		lua_setglobal (L, "arg");
	}

	int res = 0;

	if (argc > optind && 0 != strcmp (argv[optind], "-")) {
		res = lua->do_file (argv[optind]);
		if (!interactive) {
			keep_running = false;
		}
	}

	if (!keep_running) {
		/* continue to exit */
	} else if (is_tty () || interactive) {
		interactive_interpreter ();
	} else {
		res = luaL_dofile (lua->getState (), NULL);
	}

	if (session) {
		close_session ();
	}

	engine_connections.drop_connections ();

	delete lua;
	lua = NULL;

	AudioEngine::instance ()->stop ();
	AudioEngine::destroy ();

	ARDOUR::cleanup ();
	delete event_loop;
	pthread_cancel_all ();
	console_madness_end ();
	return res;
}
