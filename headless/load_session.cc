#ifdef PLATFORM_WINDOWS
	#include <windows.h>
#endif

#include <iostream>
#include <cstdlib>
#include <getopt.h>

#include "pbd/failed_constructor.h"
#include "pbd/error.h"
#include "pbd/debug.h"

#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/session.h"

#include "misc.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#define sleep(X) Sleep((X) * 1000)
#endif

static const char* localedir = LOCALEDIR;

TestReceiver test_receiver;

/** @param dir Session directory.
 *  @param state Session state file, without .ardour suffix.
 */
Session *
load_session (string dir, string state)
{
	SessionEvent::create_per_thread_pool ("test", 512);

	test_receiver.listen_to (error);
	test_receiver.listen_to (info);
	test_receiver.listen_to (fatal);
	test_receiver.listen_to (warning);

	AudioEngine* engine = AudioEngine::create ();

	if (!engine->set_default_backend ()) {
		std::cerr << "Cannot create Audio/MIDI engine\n";
		::exit (1);
	}

	init_post_engine ();

	if (engine->start () != 0) {
		std::cerr << "Cannot start Audio/MIDI engine\n";
		::exit (1);
	}

	Session* session = new Session (*engine, dir, state);
	engine->set_session (session);
	return session;
}

string session_name = "";
string backend_client_name = "tracks";
string backend_session_uuid;
bool just_version = false;
bool use_vst = true;
bool try_hw_optimization = true;
bool no_connect_ports = false;

void
print_help ()
{
	cout << "Usage: hardour [OPTIONS]... DIR SNAPSHOT_NAME\n\n"
	     << "  DIR                         Directory/Folder to load session from\n"
	     << "  SNAPSHOT_NAME               Name of session/snapshot to load (without .ardour at end\n"
	     << "  -v, --version               Show version information\n"
	     << "  -h, --help                  Print this message\n"
	     << "  -c, --name <name>           Use a specific backend client name, default is ardour\n"
	     << "  -d, --disable-plugins       Disable all plugins in an existing session\n"
	     << "  -D, --debug <options>       Set debug flags. Use \"-D list\" to see available options\n"
	     << "  -O, --no-hw-optimizations   Disable h/w specific optimizations\n"
	     << "  -P, --no-connect-ports      Do not connect any ports at startup\n"
#ifdef WINDOWS_VST_SUPPORT
	     << "  -V, --novst                 Do not use VST support\n"
#endif
		;
}

int main (int argc, char* argv[])
{
	const char *optstring = "vhdD:c:VOU:P";

	const struct option longopts[] = {
		{ "version", 0, 0, 'v' },
		{ "help", 0, 0, 'h' },
		{ "disable-plugins", 1, 0, 'd' },
		{ "debug", 1, 0, 'D' },
		{ "name", 1, 0, 'c' },
		{ "novst", 0, 0, 'V' },
		{ "no-hw-optimizations", 0, 0, 'O' },
		{ "uuid", 1, 0, 'U' },
		{ "no-connect-ports", 0, 0, 'P' },
		{ 0, 0, 0, 0 }
	};

	int option_index = 0;
	int c = 0;

	while (1) {
		c = getopt_long (argc, argv, optstring, longopts, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 0:
			break;

		case 'v':
			just_version = true;
			break;

		case 'h':
			print_help ();
			exit (0);
			break;

		case 'c':
			backend_client_name = optarg;
			break;

		case 'd':
			ARDOUR::Session::set_disable_all_loaded_plugins (true);
			break;

		case 'D':
			if (PBD::parse_debug_options (optarg)) {
				::exit (1);
			}
			break;

		case 'O':
			try_hw_optimization = false;
			break;

		case 'P':
			no_connect_ports = true;
			break;

		case 'V':
#ifdef WINDOWS_VST_SUPPORT
			use_vst = false;
#endif /* WINDOWS_VST_SUPPORT */
			break;

		case 'U':
			backend_session_uuid = optarg;
                        break;

		default:
			print_help ();
			::exit (1);
		}
	}

	if (argc < 3) {
		print_help ();
		::exit (1);
	}

	if (!ARDOUR::init (false, true, localedir)) {
		cerr << "Ardour failed to initialize\n" << endl;
		::exit (1);
	}

	Session* s = 0;
	
	try {
		s = load_session (argv[optind], argv[optind+1]);
	} catch (failed_constructor& e) {
		cerr << "failed_constructor: " << e.what() << "\n";
		exit (EXIT_FAILURE);
	} catch (AudioEngine::PortRegistrationFailure& e) {
		cerr << "PortRegistrationFailure: " << e.what() << "\n";
		exit (EXIT_FAILURE);
	} catch (exception& e) {
		cerr << "exception: " << e.what() << "\n";
		exit (EXIT_FAILURE);
	} catch (...) {
		cerr << "unknown exception.\n";
		exit (EXIT_FAILURE);
	}

	s->request_transport_speed (1.0);
	
	sleep (-1);

	AudioEngine::instance()->remove_session ();
	delete s;
	AudioEngine::instance()->stop ();

	AudioEngine::destroy ();

	return 0;
}
