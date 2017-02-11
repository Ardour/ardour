#include <stdint.h>
#include <assert.h>

#include <cstdio>
#include <iostream>
#include <string>
#include <list>
#include <vector>

#include <glibmm.h>

#include "pbd/debug.h"
#include "pbd/event_loop.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/pthread_utils.h"
#include "pbd/reallocpool.h"
#include "pbd/receiver.h"
#include "pbd/transmitter.h"

#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/filename_extensions.h"
#include "ardour/filesystem_paths.h"
#include "ardour/luabindings.h"
#include "ardour/session.h"
#include "ardour/types.h"
#include "ardour/vst_types.h"

#include <readline/readline.h>
#include <readline/history.h>

#include "lua/luastate.h"
#include "LuaBridge/LuaBridge.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

static const char* localedir = LOCALEDIR;
static PBD::ScopedConnectionList engine_connections;
static PBD::ScopedConnectionList session_connections;
static Session* session = NULL;
static LuaState* lua;
static bool keep_running = true;

/* extern VST functions */
int vstfx_init (void*) { return 0; }
void vstfx_exit () {}
void vstfx_destroy_editor (VSTState*) {}

class LuaReceiver : public Receiver
{
  protected:
    void receive (Transmitter::Channel chn, const char * str)
		{
			const char *prefix = "";

			switch (chn) {
				case Transmitter::Error:
					prefix = "[ERROR]: ";
					break;
				case Transmitter::Info:
					/* ignore */
					return;
				case Transmitter::Warning:
					prefix = "[WARNING]: ";
					break;
				case Transmitter::Fatal:
					prefix = "[FATAL]: ";
					break;
				case Transmitter::Throw:
					/* this isn't supposed to happen */
					abort ();
			}

			/* note: iostreams are already thread-safe: no external
				 lock required.
				 */

			std::cout << prefix << str << std::endl;

			if (chn == Transmitter::Fatal) {
				::exit (9);
			}
		}
};

class MyEventLoop : public sigc::trackable, public EventLoop
{
	public:
		MyEventLoop (std::string const& name) : EventLoop (name) {
			run_loop_thread = Glib::Threads::Thread::self ();
		}

		void call_slot (InvalidationRecord* ir, const boost::function<void()>& f) {
			if (Glib::Threads::Thread::self () == run_loop_thread) {
				cout << string_compose ("%1/%2 direct dispatch of call slot via functor @ %3, invalidation %4\n", event_loop_name(), pthread_name(), &f, ir);
				f ();
			} else {
				cout << string_compose ("%1/%2 queue call-slot using functor @ %3, invalidation %4\n", event_loop_name(), pthread_name(), &f, ir);
				assert (!ir);
				f (); // XXX TODO, queue and process during run ()
			}
		}

		void run () {
			; // TODO process Events, if any
		}

		Glib::Threads::Mutex& slot_invalidation_mutex () { return request_buffer_map_lock; }

	private:
		Glib::Threads::Thread* run_loop_thread;
		Glib::Threads::Mutex   request_buffer_map_lock;
};

static MyEventLoop *event_loop = NULL;

/* ****************************************************************************/
/* internal helper fn and callbacks */

static int do_audio_midi_setup (uint32_t desired_sample_rate)
{
	return AudioEngine::instance ()->start ();
}

static void init ()
{
	if (!ARDOUR::init (false, true, localedir)) {
		cerr << "Ardour failed to initialize\n" << endl;
		::exit (EXIT_FAILURE);
	}

	assert (!event_loop);
	event_loop = new MyEventLoop ("lua");
	EventLoop::set_event_loop_for_thread (event_loop);
	SessionEvent::create_per_thread_pool ("lua", 4096);

	static LuaReceiver lua_receiver;

	lua_receiver.listen_to (error);
	lua_receiver.listen_to (info);
	lua_receiver.listen_to (fatal);
	lua_receiver.listen_to (warning);

	ARDOUR::Session::AudioEngineSetupRequired.connect_same_thread (engine_connections, &do_audio_midi_setup);
}

static void set_session (ARDOUR::Session *s)
{
	session = s;
	assert (lua);
	lua_State* L = lua->getState ();
	LuaBindings::set_session (L, session);
	lua->collect_garbage (); // drop references
}

static void unset_session ()
{
	session_connections.drop_connections ();
	set_session (NULL);
}

static int prepare_engine ()
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

static int start_engine (uint32_t rate)
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

	init_post_engine ();
	return 0;
}

static Session * _create_session (string dir, string state, uint32_t rate) // throws
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
	bus_profile.input_ac = AutoConnectPhysical;
	bus_profile.output_ac = AutoConnectMaster;
	bus_profile.requested_physical_in = 0; // use all available
	bus_profile.requested_physical_out = 0; // use all available

	AudioEngine* engine = AudioEngine::instance ();
	Session* session = new Session (*engine, dir, state, &bus_profile);
	return session;
}

static Session * _load_session (string dir, string state) // throws
{
	if (prepare_engine ()) {
		return 0;
	}

	float sr;
	SampleFormat sf;
	std::string v;
	std::string s = Glib::build_filename (dir, state + statefile_suffix);
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

	AudioEngine* engine = AudioEngine::instance ();
	Session* session = new Session (*engine, dir, state);
	return session;
}

/* ****************************************************************************/
/* lua bound functions */

static Session* create_session (string dir, string state, uint32_t rate)
{
	Session* s = 0;
	if (session) {
		cerr << "Session already open" << "\n";
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

static Session* load_session (string dir, string state)
{
	Session* s = 0;
	if (session) {
		cerr << "Session already open" << "\n";
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

static int set_debug_options (const char *opts)
{
	return PBD::parse_debug_options (opts);
}

static void close_session ()
{
	delete session;
	assert (!session);
}

static int close_session_lua (lua_State *L)
{
	if (!session) {
		cerr << "No open session" << "\n";
		return 0;
	}
	close_session ();
	return 0;
}

static void delay (float d) {
	if (d > 0) {
		Glib::usleep (d * 1000000);
	}
}

static int do_quit (lua_State *L)
{
	keep_running = false;
	return 0;
}

/* ****************************************************************************/

static void my_lua_print (std::string s) {
	std::cout << s << "\n";
}

static void setup_lua ()
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
		.beginClass <Session> ("Session")
		.addExtCFunction ("close", &close_session_lua)
		.endClass ()
		.endNamespace ();

	// push instance to global namespace (C++ lifetime)
	luabridge::push <AudioEngine *> (L, AudioEngine::create ());
	lua_setglobal (L, "AudioEngine");

	AudioEngine::instance ()->stop ();
}

int main (int argc, char **argv)
{
	init ();
	setup_lua ();

	using_history ();
	std::string histfile = Glib::build_filename (user_config_directory(), "/luahist");

	read_history (histfile.c_str());

	char *line = NULL;
	while (keep_running && (line = readline ("> "))) {
		event_loop->run();
		if (!strcmp (line, "quit")) {
			free (line); line = NULL;
			break;
		}

		if (strlen (line) == 0) {
			free (line); line = NULL;
			continue;
		}

		if (lua->do_command (line)) {
			// error
		}

		add_history (line);
		event_loop->run();
		free (line); line = NULL;
	}
	free (line);
	printf ("\n");

	if (session) {
		close_session ();
	}

	engine_connections.drop_connections ();

	delete lua;
	lua = NULL;

	write_history (histfile.c_str());

	AudioEngine::instance ()->stop ();
	AudioEngine::destroy ();

	// cleanup
	ARDOUR::cleanup ();
	delete event_loop;
	pthread_cancel_all ();
	return 0;
}
