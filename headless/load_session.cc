/*
 * Copyright (C) 2014-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <cstdlib>
#include <getopt.h>
#include <iostream>

#ifndef PLATFORM_WINDOWS
#include <signal.h>
#endif

#include <glibmm.h>

#include "pbd/convert.h"
#include "pbd/crossthread.h"
#include "pbd/debug.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"

#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/revision.h"
#include "ardour/session.h"

#include "control_protocol/control_protocol.h"

#include "misc.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

static const char* localedir = LOCALEDIR;

static string             backend_client_name;
static string             backend_name = "JACK";
static CrossThreadChannel xthread (true);
static TestReceiver       test_receiver;

/** @param dir Session directory.
 *  @param state Session state file, without .ardour suffix.
 */
static Session*
load_session (string dir, string state)
{
	SessionEvent::create_per_thread_pool ("test", 512);

	test_receiver.listen_to (warning);
	test_receiver.listen_to (error);
	test_receiver.listen_to (fatal);

	AudioEngine* engine = AudioEngine::create ();

	if (!engine->set_backend (backend_name, backend_client_name, "")) {
		std::cerr << "Cannot set Audio/MIDI engine backend\n";
		exit (EXIT_FAILURE);
	}

	if (engine->start () != 0) {
		std::cerr << "Cannot start Audio/MIDI engine\n";
		exit (EXIT_FAILURE);
	}

	Session* session = new Session (*engine, dir, state);
	engine->set_session (session);
	return session;
}

static void
access_action (const std::string& action_group, const std::string& action_item)
{
	if (action_group == "Common" && action_item == "Quit") {
		xthread.deliver ('x');
	}
}

static void
engine_halted (const char* reason)
{
	cerr << "The audio backend has been shutdown";
	if (reason && strlen (reason) > 0) {
		cerr << ": " << reason;
	} else {
		cerr << ".";
	}
	cerr << endl;
	xthread.deliver ('x');
}

#ifndef PLATFORM_WINDOWS
static void
wearedone (int)
{
	cerr << "caught signal - terminating." << endl;
	xthread.deliver ('x');
}
#endif

static void
print_version ()
{
	cout
	    << PROGRAM_NAME
	    << VERSIONSTRING
	    << " (built using "
	    << ARDOUR::revision
#ifdef __GNUC__
	    << " and GCC version " << __VERSION__
#endif
	    << ')'
	    << endl;
}

static void
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

int
main (int argc, char* argv[])
{
	const char* optstring = "vhBdD:c:OU:P";

	/* clang-format off */
	const struct option longopts[] = {
		{ "version",             no_argument,       0, 'v' },
		{ "help",                no_argument,       0, 'h' },
		{ "bypass-plugins",      no_argument,       0, 'B' },
		{ "disable-plugins",     no_argument,       0, 'd' },
		{ "debug",               required_argument, 0, 'D' },
		{ "name",                required_argument, 0, 'c' },
		{ "no-hw-optimizations", no_argument,       0, 'O' },
		{ "no-connect-ports",    no_argument,       0, 'P' },
		{ 0, 0, 0, 0 }
	};
	/* clang-format on */

	bool try_hw_optimization = true;

	backend_client_name = PBD::downcase (std::string (PROGRAM_NAME));

	int c;
	while ((c = getopt_long (argc, argv, optstring, longopts, (int*)0)) != EOF) {
		switch (c) {
			case 0:
				break;

			case 'v':
				print_version ();
				exit (EXIT_SUCCESS);
				break;

			case 'h':
				print_help ();
				exit (EXIT_SUCCESS);
				break;

			case 'c':
				backend_client_name = optarg;
				break;

			case 'B':
				ARDOUR::Session::set_bypass_all_loaded_plugins (true);
				break;

			case 'd':
				ARDOUR::Session::set_disable_all_loaded_plugins (true);
				break;

			case 'D':
				if (PBD::parse_debug_options (optarg)) {
					exit (EXIT_SUCCESS);
				}
				break;

			case 'O':
				try_hw_optimization = false;
				break;

			case 'P':
				ARDOUR::Port::set_connecting_blocked (true);
				break;

			default:
				print_help ();
				exit (EXIT_FAILURE);
		}
	}

	if (argc < 3) {
		print_help ();
		exit (EXIT_FAILURE);
	}

	if (!ARDOUR::init (try_hw_optimization, localedir)) {
		cerr << "Ardour failed to initialize\n"
		     << endl;
		exit (EXIT_FAILURE);
	}

	Session* s = 0;

	try {
		s = load_session (argv[optind], argv[optind + 1]);
	} catch (failed_constructor& e) {
		cerr << "failed_constructor: " << e.what () << "\n";
		exit (EXIT_FAILURE);
	} catch (AudioEngine::PortRegistrationFailure& e) {
		cerr << "PortRegistrationFailure: " << e.what () << "\n";
		exit (EXIT_FAILURE);
	} catch (exception& e) {
		cerr << "exception: " << e.what () << "\n";
		exit (EXIT_FAILURE);
	} catch (...) {
		cerr << "unknown exception.\n";
		exit (EXIT_FAILURE);
	}

	/* allow signal propagation, callback/thread-pool setup, etc
	 * similar to to GUI "first idle"
	 */
	Glib::usleep (1000000); // 1 sec

	if (!s) {
		cerr << "failed_to load session\n";
		exit (EXIT_FAILURE);
	}

	PBD::ScopedConnectionList con;
	BasicUI::AccessAction.connect_same_thread (con, boost::bind (&access_action, _1, _2));
	AudioEngine::instance ()->Halted.connect_same_thread (con, boost::bind (&engine_halted, _1));

#ifndef PLATFORM_WINDOWS
	signal (SIGINT, wearedone);
	signal (SIGTERM, wearedone);
#endif

	s->request_roll ();

	char msg;
	do {
	} while (0 == xthread.receive (msg, true));

	AudioEngine::instance ()->remove_session ();
	delete s;
	AudioEngine::instance ()->stop ();

	AudioEngine::destroy ();
	return 0;
}
