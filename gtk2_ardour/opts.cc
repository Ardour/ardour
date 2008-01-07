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
#include <iostream>
#include <cstdlib>

#include <ardour/session.h>

#include "opts.h"

#include "i18n.h"

using namespace std;

Glib::ustring ARDOUR_COMMAND_LINE::session_name = "";
string ARDOUR_COMMAND_LINE::jack_client_name = "ardour";
bool  ARDOUR_COMMAND_LINE::show_key_actions = false;
bool ARDOUR_COMMAND_LINE::no_splash = true;
bool ARDOUR_COMMAND_LINE::just_version = false;
bool ARDOUR_COMMAND_LINE::use_vst = true;
bool ARDOUR_COMMAND_LINE::new_session = false;
char* ARDOUR_COMMAND_LINE::curvetest_file = 0;
bool ARDOUR_COMMAND_LINE::try_hw_optimization = true;
Glib::ustring ARDOUR_COMMAND_LINE::keybindings_path = ""; /* empty means use builtin default */
Glib::ustring ARDOUR_COMMAND_LINE::menus_file = "ardour.menus";
bool ARDOUR_COMMAND_LINE::finder_invoked_ardour = false;

using namespace ARDOUR_COMMAND_LINE;

int
print_help (const char *execname)
{
	cout << _("Usage: ") << execname << "\n"
	     << _("  -v, --version                    Show version information\n")
	     << _("  -h, --help                       Print this message\n")
	     << _("  -b, --bindings                   Print all possible keyboard binding names\n")
	     << _("  -c, --name  name                 Use a specific jack client name, default is ardour\n")
	     << _("  -d, --disable-plugins            Disable all plugins in an existing session\n")
	     << _("  -n, --show-splash                Show splash screen\n")
	     << _("  -m, --menus file                 Use \"file\" for Ardour menus\n")                       
	     << _("  -N, --new session-name           Create a new session from the command line\n")
	     << _("  -O, --no-hw-optimizations        Disable h/w specific optimizations\n")
	     << _("  -S, --sync	                      Draw the gui synchronously \n")
#ifdef VST_SUPPORT
	     << _("  -V, --novst                      Do not use VST support\n")
#endif
	     << _("  [session-name]                   Name of session to load\n")
	     << _("  -C, --curvetest filename         Curve algorithm debugger\n")
	     << _("  -k, --keybindings filename       Name of key bindings to load (default is ~/.ardour2/ardour.bindings)\n")
		;
	return 1;

}

int
ARDOUR_COMMAND_LINE::parse_opts (int argc, char *argv[])
{
	const char *optstring = "U:hSbvVnOdc:C:m:N:k:p:";
	const char *execname = strrchr (argv[0], '/');

	if (getenv ("ARDOUR_SAE")) {
		menus_file = "ardour-sae.menus";
		keybindings_path = "ardour-sae";
	}

	if (execname == 0) {
		execname = argv[0];
	} else {
		execname++;
	}

	const struct option longopts[] = {
		{ "version", 0, 0, 'v' },
		{ "help", 0, 0, 'h' },
		{ "bindings", 0, 0, 'b' },
		{ "show-splash", 0, 0, 'n' },
                { "menus", 1, 0, 'm'} ,
		{ "name", 1, 0, 'c' },
		{ "novst", 0, 0, 'V' },
		{ "new", 1, 0, 'N' },
		{ "no-hw-optimizations", 0, 0, 'O' },
		{ "sync", 0, 0, 'S' },
		{ "curvetest", 1, 0, 'C' },
		{ "sillyAppleUndocumentedFinderFeature", 1, 0, 'p' },
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

		case 'b':
			show_key_actions = true;
			break;
			
		case 'd':
			ARDOUR::Session::set_disable_all_loaded_plugins (true);
			break;

                case 'm':
                        menus_file = optarg;
                        break;

		case 'n':
			no_splash = false;
			break;

		case 'p':
			//undocumented OS X finder -psn_XXXXX argument
			finder_invoked_ardour = true;
			break;
		
		case 'S':
			// just pass this through to gtk it will figure it out
			break;

		case 'N':
			new_session = true;
			session_name = optarg;
			break;

		case 'O':
			try_hw_optimization = false;
			break;

		case 'V':
#ifdef VST_SUPPORT
			use_vst = false;
#endif /* VST_SUPPORT */
			break;

		case 'c':
			jack_client_name = optarg;
			break;

		case 'C':
			curvetest_file = optarg;
			break;

		case 'k':
			keybindings_path = optarg;
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

