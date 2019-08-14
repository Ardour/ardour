/*
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#include <glibmm.h>

#include "ardour/audioengine.h"
#include "ardour/filename_extensions.h"
#include "ardour/template_utils.h"

#include "common.h"

using namespace std;
using namespace ARDOUR;
using namespace SessionUtils;

static void usage ()
{
	// help2man compatible format (standard GNU help-text)
	printf (UTILNAME " - create a new session from the commandline.\n\n");
	printf ("Usage: " UTILNAME " [ OPTIONS ] <session-dir> [session-name]\n\n");
	printf ("Options:\n\
  -L, --list-templates          List available templates and exit\n\
  -h, --help                    Display this help and exit\n\
  -m, --master-channels <chn>   Master-bus channel count (default 2)\n\
  -s, --samplerate <rate>       Samplerate to use (default 48000)\n\
  -t, --template <template>     Use given template for new session\n\
  -V, --version                 Print version information and exit\n\
\n");

	printf ("\n\
This tool creates a new Ardour session, optionally based on a\n\
session-template.\n\
\n\
If the session-name is unspecified, the sesion-dir-name is used.\n\
If specified, the tool expects a session-name without .ardour\n\
file-name extension.\n\
\n\
If no template is specified, an empty session with a stereo master\n\
bus is created. The -m option allows to specify the master-bus channel\n\
count. If zero is used as channel count, no master-bus is created.\n\
\n\
Note: this tool can only use static session templates.\n\
Interactive Lua init-scripts or dynamic templates are not supported.\n\
\n");

	printf ("\n\
Examples:\n\
" UTILNAME " -s 44100 -m 4 /tmp/NewSession\n\
\n");

	printf ("Report bugs to <http://tracker.ardour.org/>\n"
	        "Website: <http://ardour.org/>\n");
	::exit (EXIT_SUCCESS);
}

static void
list_templates ()
{
	vector<TemplateInfo> templates;
	find_session_templates (templates, false);

	cout << "---- List of Session Templates ----\n";
	for (vector<TemplateInfo>::iterator x = templates.begin (); x != templates.end (); ++x) {
		cout << "[TPL] " << (*x).name << "\n";
	}
	cout << "----\n";
}

static std::string
template_path_from_name (std::string const& name)
{
	vector<TemplateInfo> templates;
	find_session_templates (templates, false);

	for (vector<TemplateInfo>::iterator x = templates.begin (); x != templates.end (); ++x) {
		if ((*x).name == name) {
			return (*x).path;
		}
	}
	return "";
}

static Session*
create_new_session (string const& dir, string const& state, float sample_rate, int master_bus_chn, string const& template_path)
{
	AudioEngine* engine = AudioEngine::create ();

	if (!engine->set_backend ("None (Dummy)", "Unit-Test", "")) {
		cerr << "Cannot create Audio/MIDI engine\n";
		::exit (EXIT_FAILURE);
	}

	engine->set_input_channels (256);
	engine->set_output_channels (256);

	if (engine->set_sample_rate (sample_rate)) {
		cerr << "Cannot set session's samplerate.\n";
		return 0;
	}

	if (engine->start () != 0) {
		cerr << "Cannot start Audio/MIDI engine\n";
		return 0;
	}

	string s = Glib::build_filename (dir, state + statefile_suffix);

	if (Glib::file_test (dir, Glib::FILE_TEST_EXISTS)) {
		cerr << "Session folder already exists '" << dir << "'\n";
	}
	if (Glib::file_test (s, Glib::FILE_TEST_EXISTS)) {
		cerr << "Session file exists '" << s << "'\n";
		return 0;
	}

	BusProfile  bus_profile;
	BusProfile* bus_profile_ptr = NULL;

	if (master_bus_chn > 0) {
		bus_profile_ptr = &bus_profile;
		bus_profile.master_out_channels = master_bus_chn;
	}

	if (!template_path.empty ()) {
		bus_profile_ptr = NULL;
	}

	Session* session = new Session (*engine, dir, state, bus_profile_ptr, template_path);
	engine->set_session (session);
	return session;
}

int
main (int argc, char* argv[])
{
	int    sample_rate    = 48000;
	int    master_bus_chn = 2;
	string template_path;

	const char* optstring = "Lm:hs:t:V";

	/* clang-format off */
	const struct option longopts[] = {
		{ "list-templates",  no_argument,       0, 'L' },
		{ "help",            no_argument,       0, 'h' },
		{ "master-channels", no_argument,       0, 'm' },
		{ "samplerate",      required_argument, 0, 's' },
		{ "template",        required_argument, 0, 't' },
		{ "version",         no_argument,       0, 'V' },
	};
	/* clang-format on */

	int c = 0;
	while (EOF != (c = getopt_long (argc, argv,
	                                optstring, longopts, (int*)0))) {
		switch (c) {
			case 'L':
				list_templates ();
				exit (EXIT_SUCCESS);
				break;
			case 'm': {
				const int mc = atoi (optarg);
				if (mc >= 0 && mc < 128) {
					master_bus_chn = mc;
				} else {
					cerr << "Invalid master bus channel count\n";
				}
			} break;
			case 's': {
				const int sr = atoi (optarg);
				if (sr >= 8000 && sr <= 192000) {
					sample_rate = sr;
				} else {
					cerr << "Invalid Samplerate\n";
				}
			} break;

			case 't':
				template_path = template_path_from_name (optarg);
				if (template_path.empty ()) {
					cerr << "Invalid (non-existent) template:" << optarg << "\n";
					::exit (EXIT_FAILURE);
				}
				break;

			case 'V':
				printf ("ardour-utils version %s\n\n", VERSIONSTRING);
				printf ("Copyright (C) GPL 2019 Robin Gareus <robin@gareus.org>\n");
				exit (EXIT_SUCCESS);
				break;

			case 'h':
				usage ();
				break;

			default:
				cerr << "Error: unrecognized option. See --help for usage information.\n";
				::exit (EXIT_FAILURE);
				break;
		}
	}

	string snapshot_name;

	if (optind + 2 == argc) {
		snapshot_name = argv[optind + 1];
	} else if (optind + 1 == argc) {
		snapshot_name = Glib::path_get_basename (argv[optind]);
	} else {
		cerr << "Error: Missing parameter. See --help for usage information.\n";
		::exit (EXIT_FAILURE);
	}

	if (snapshot_name.empty ()) {
		cerr << "Error: Invalid empty session/snapshot name.\n";
		::exit (EXIT_FAILURE);
	}

	/* all systems go */

	SessionUtils::init ();
	Session* s = 0;

	try {
		s = create_new_session (argv[optind], snapshot_name, sample_rate, master_bus_chn, template_path);
	} catch (ARDOUR::SessionException& e) {
		cerr << "Error: " << e.what () << "\n";
	} catch (...) {
		cerr << "Error: unknown exception.\n";
	}

	/* save is implicit when creating a new session */

	if (s) {
		cout << "Created session in '" << s->path () << "'" << endl;
	}

	SessionUtils::unload_session (s);
	SessionUtils::cleanup ();

	return 0;
}
