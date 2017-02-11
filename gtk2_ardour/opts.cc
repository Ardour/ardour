/*
    Copyright (C) 2001 Paul Davis

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

#include <getopt.h>
#include <string.h>
#include <iostream>
#include <cstdlib>

#include "ardour/debug.h"
#include "ardour/session.h"

#ifndef NDEBUG // "-H"
#include "processor_box.h"
#endif

#include "opts.h"

#include "pbd/i18n.h"

using namespace std;

string ARDOUR_COMMAND_LINE::session_name = "";
string ARDOUR_COMMAND_LINE::backend_client_name = "ardour";
string ARDOUR_COMMAND_LINE::backend_session_uuid;
bool  ARDOUR_COMMAND_LINE::show_key_actions = false;
bool ARDOUR_COMMAND_LINE::no_splash = false;
bool ARDOUR_COMMAND_LINE::just_version = false;
bool ARDOUR_COMMAND_LINE::use_vst = true;
bool ARDOUR_COMMAND_LINE::new_session = false;
char* ARDOUR_COMMAND_LINE::curvetest_file = 0;
bool ARDOUR_COMMAND_LINE::try_hw_optimization = true;
bool ARDOUR_COMMAND_LINE::no_connect_ports = false;
string ARDOUR_COMMAND_LINE::keybindings_path = ""; /* empty means use builtin default */
std::string ARDOUR_COMMAND_LINE::menus_file = "ardour.menus";
bool ARDOUR_COMMAND_LINE::finder_invoked_ardour = false;
string ARDOUR_COMMAND_LINE::immediate_save;
string ARDOUR_COMMAND_LINE::load_template;
bool ARDOUR_COMMAND_LINE::check_announcements = true;

using namespace ARDOUR_COMMAND_LINE;

int
print_help (const char *execname)
{
	// help2man format, http://docopt.org/
	// https://www.gnu.org/prep/standards/standards.html#g_t_002d_002dhelp
	cout
		<< _("Usage: ") << PROGRAM_NAME << _(" [ OPTIONS ] [ SESSION-NAME ]")
		<< "\n\n"
		<< _("Ardour is a multichannel hard disk recorder (HDR) and digital audio workstation (DAW).")
		<< "\n\n"
		<< _("Options:\n")
		<< _("  -a, --no-announcements      Do not contact website for announcements\n")
		<< _("  -b, --bindings              Print all possible keyboard binding names\n")
		<< _("  -B, --bypass-plugins        Bypass all plugins in an existing session\n")
		<< _("  -c, --name <name>           Use a specific backend client name, default is ardour\n")
#ifndef NDEBUG
		<< _("  -C, --curvetest filename    Curve algorithm debugger\n")
#endif
		<< _("  -d, --disable-plugins       Disable all plugins (safe mode)\n")
#ifndef NDEBUG
		<< _("  -D, --debug <options>       Set debug flags. Use \"-D list\" to see available options\n")
#endif
		<< _("  -E, --save <file>           Load the specified session, save it to <file> and then quit\n")
		<< _("  -h, --help                  Print this message\n")
		<< _("  -k, --keybindings <file>    Name of key bindings to load\n")
		<< _("  -m, --menus file            Use \"file\" to define menus\n")
		<< _("  -n, --no-splash             Do not show splash screen\n")
		<< _("  -N, --new session-name      Create a new session from the command line\n")
		<< _("  -O, --no-hw-optimizations   Disable h/w specific optimizations\n")
		<< _("  -P, --no-connect-ports      Do not connect any ports at startup\n")
		<< _("  -S, --sync                  Draw the GUI synchronously\n")
		<< _("  -T, --template <name>       Draw the GUI synchronously\n")
		<< _("  -U, --uuid <uuid>           Set (jack) backend UUID\n")
		<< _("  -v, --version               Use session template\n")
#ifdef WINDOWS_VST_SUPPORT
		<< _("  -V, --novst                 Disable WindowsVST support\n")
#endif
		<< "\n\n"
		<< _("Report bugs to http://tracker.ardour.org\n")
		<< _("Website http://ardour.org\n")
		;
	return 1;

}

int
ARDOUR_COMMAND_LINE::parse_opts (int argc, char *argv[])
{
	const char *optstring = "abBc:C:dD:hHk:E:m:N:nOp:PST:U:vV";
	const char *execname = strrchr (argv[0], '/');

	if (execname == 0) {
		execname = argv[0];
	} else {
		execname++;
	}

	const struct option longopts[] = {
		{ "version", 0, 0, 'v' },
		{ "help", 0, 0, 'h' },
		{ "no-announcements", 0, 0, 'a' },
		{ "bindings", 0, 0, 'b' },
		{ "bypass-plugins", 0, 0, 'B' },
		{ "disable-plugins", 0, 0, 'd' },
		{ "debug", 1, 0, 'D' },
		{ "no-splash", 0, 0, 'n' },
		{ "menus", 1, 0, 'm' },
		{ "name", 1, 0, 'c' },
		{ "novst", 0, 0, 'V' },
		{ "new", 1, 0, 'N' },
		{ "no-hw-optimizations", 0, 0, 'O' },
		{ "sync", 0, 0, 'S' },
		{ "curvetest", 1, 0, 'C' },
		{ "save", 1, 0, 'E' },
		{ "uuid", 1, 0, 'U' },
		{ "template", 1, 0, 'T' },
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
			print_help (execname);
			exit (0);
			break;
		case 'H':
#ifndef NDEBUG
			ProcessorBox::show_all_processors = true;
#endif
			break;
		case 'a':
			check_announcements = false;
			break;

		case 'b':
			show_key_actions = true;
			break;

		case 'B':
			ARDOUR::Session::set_bypass_all_loaded_plugins (true);
			break;

		case 'd':
			ARDOUR::Session::set_disable_all_loaded_plugins (true);
			break;

		case 'D':
			if (PBD::parse_debug_options (optarg)) {
				exit (0);
			}
			break;

		case 'm':
			menus_file = optarg;
			break;

		case 'n':
			no_splash = true;
			break;

		case 'p':
			//undocumented OS X finder -psn_XXXXX argument
			finder_invoked_ardour = true;
			break;

		case 'S':
		//	; just pass this through to gtk it will figure it out
			break;
		case 'T':
			load_template = optarg;
			break;

		case 'N':
			new_session = true;
			session_name = optarg;
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

		case 'c':
			backend_client_name = optarg;
			break;

		case 'C':
			curvetest_file = optarg;
			break;

		case 'k':
			keybindings_path = optarg;
			break;

		case 'E':
			immediate_save = optarg;
			break;

		case 'U':
			backend_session_uuid = optarg;
			break;

		default:
			return print_help(execname);
		}
	}

	if (optind < argc) {
		if (new_session) {
			cerr << "Illogical combination: you can either create a new session, or a load an existing session but not both!" << endl;
			return print_help(execname);
		}
		session_name = argv[optind++];
	}

	return 0;
}

