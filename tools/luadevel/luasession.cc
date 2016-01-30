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
static Session *_session = NULL;
static LuaState *lua;

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
				//cout << string_compose ("%1/%2 direct dispatch of call slot via functor @ %3, invalidation %4\n", event_loop_name(), pthread_name(), &f, ir);
				f ();
			} else {
				//cout << string_compose ("%1/%2 queue call-slot using functor @ %3, invalidation %4\n", event_loop_name(), pthread_name(), &f, ir);
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

static int do_audio_midi_setup (uint32_t desired_sample_rate)
{
	return AudioEngine::instance ()->start ();
}

static MyEventLoop *event_loop = NULL;

static void init ()
{
	if (!ARDOUR::init (false, true, localedir)) {
		cerr << "Ardour failed to initialize\n" << endl;
		::exit (EXIT_FAILURE);
	}

	assert (!event_loop);
	event_loop = new MyEventLoop ("lua");
	EventLoop::set_event_loop_for_thread (event_loop);
	SessionEvent::create_per_thread_pool ("lua", 512);

	static LuaReceiver lua_receiver;

	lua_receiver.listen_to (error);
	lua_receiver.listen_to (info);
	lua_receiver.listen_to (fatal);
	lua_receiver.listen_to (warning);

	ARDOUR::Session::AudioEngineSetupRequired.connect_same_thread (engine_connections, &do_audio_midi_setup);
}

static void set_session (ARDOUR::Session *s)
{
	_session = s;
	assert (lua);
	lua_State* L = lua->getState ();
	LuaBindings::set_session (L, _session);
	lua->collect_garbage (); // drop references
}

static void unset_session ()
{
	session_connections.drop_connections ();
	set_session (NULL);
}

static Session * _load_session (string dir, string state)
{
	AudioEngine* engine = AudioEngine::instance ();

	if (!engine->current_backend ()) {
		if (!engine->set_backend ("None (Dummy)", "Unit-Test", "")) {
			std::cerr << "Cannot create Audio/MIDI engine\n";
			return 0;
		}
	}

	if (!engine->current_backend ()) {
		std::cerr << "Cannot create Audio/MIDI engine\n";
		return 0;
	}

	if (engine->running ()) {
		engine->stop ();
	}

	float sr;
	SampleFormat sf;

	std::string s = Glib::build_filename (dir, state + statefile_suffix);
	if (!Glib::file_test (dir, Glib::FILE_TEST_EXISTS)) {
		std::cerr << "Cannot find session: " << s << "\n";
		return 0;
	}

	if (Session::get_info_from_path (s, sr, sf) == 0) {
		if (engine->set_sample_rate (sr)) {
			std::cerr << "Cannot set session's samplerate.\n";
			return 0;
		}
	} else {
		std::cerr << "Cannot get samplerate from session.\n";
		return 0;
	}

	init_post_engine ();

	if (engine->start () != 0) {
		std::cerr << "Cannot start Audio/MIDI engine\n";
		return 0;
	}

	Session* session = new Session (*engine, dir, state);
	return session;
}

static Session* load_session (string dir, string state)
{
	Session* s = 0;
	if (_session) {
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
	Glib::usleep (1000000); // allo signal propagation, callback/thread-pool setup
	assert (s);
	set_session (s);
	s->DropReferences.connect_same_thread (session_connections, &unset_session);
	return s;
}

static void close_session ()
{
	delete _session;
	assert (!_session);
}

static int close_session_lua (lua_State *L)
{
	if (!_session) {
		cerr << "No open session" << "\n";
		return 0;
	}
	close_session ();
	return 0;
}

/* extern VST functions */
int vstfx_init (void*) { return 0; }
void vstfx_exit () {}
void vstfx_destroy_editor (VSTState*) {}

static void my_lua_print (std::string s) {
	std::cout << s << "\n";
}

static void delay (float d) {
	if (d > 0) {
		Glib::usleep (d * 1000000);
	}
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

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("_G")
		.addFunction ("load_session", &load_session)
		.addFunction ("close_session", &close_session)
		.addFunction ("sleep", &delay)
		.endNamespace ();

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ARDOUR")
		.beginClass <Session> ("Session")
		.addExtCFunction ("close", &close_session_lua)
		.endClass ()
		.endNamespace ();

	// push instance to global namespace (C++ lifetime)
	luabridge::push <AudioEngine *> (L, AudioEngine::create ());
	lua_setglobal (L, "AudioEngine");
}

int main (int argc, char **argv)
{
	init ();
	setup_lua ();

	using_history ();
	std::string histfile = Glib::build_filename (user_config_directory(), "/luahist");

	read_history (histfile.c_str());

	char *line;
	while ((line = readline ("> "))) {
		event_loop->run();
		if (!strcmp (line, "quit")) {
			break;
		}
		if (strlen (line) == 0) {
			continue;
		}
		if (!lua->do_command (line)) {
			add_history (line); // OK
		} else {
			add_history (line); // :)
		}
		event_loop->run();
		free (line);
	}
	printf ("\n");

	if (_session) {
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
