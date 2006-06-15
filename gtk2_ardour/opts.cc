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

    $Id$
*/

#include <getopt.h>
#include <iostream>
#include <cstdlib>

#include "opts.h"

#include "i18n.h"

using namespace std;

string GTK_ARDOUR::session_name = "";
string GTK_ARDOUR::jack_client_name = "ardour";
bool  GTK_ARDOUR::show_key_actions = false;
bool GTK_ARDOUR::no_splash = true;
bool GTK_ARDOUR::just_version = false;
bool GTK_ARDOUR::use_vst = true;
bool GTK_ARDOUR::new_session = false;
char* GTK_ARDOUR::curvetest_file = 0;
bool GTK_ARDOUR::try_hw_optimization = true;

using namespace GTK_ARDOUR;

int
print_help (const char *execname)
{
	cout << _("Usage: ") << execname << "\n"
	     << _("  -v, --version                    Show version information\n")
	     << _("  -h, --help                       Print this message\n")
	     << _("  -b, --bindings                   Print all possible keyboard binding names\n")
	     << _("  -n, --show-splash                Show splash screen\n")
	     << _("  -c, --name  name                 Use a specific jack client name, default is ardour\n")
	     << _("  -N, --new session-name           Create a new session from the command line\n")                       
	     << _("  -o, --use-hw-optimizations        Try to use h/w specific optimizations\n")
#ifdef VST_SUPPORT
	     << _("  -V, --novst                      Do not use VST support\n")
#endif
	     << _("  [session-name]                   Name of session to load\n")
	     << _("  -C, --curvetest filename         Curve algorithm debugger\n")
	     << _("  -g, --gtktheme                   Allow GTK to load a theme\n")
		;
	return 1;

}

int
GTK_ARDOUR::parse_opts (int argc, char *argv[])

{
	const char *optstring = "U:hbvVnoc:C:N:g";
	const char *execname = strrchr (argv[0], '/');

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
		{ "name", 1, 0, 'c' },
		{ "novst", 0, 0, 'V' },
		{ "new", 1, 0, 'N' },
		{ "no-hw-optimizations", 0, 0, 'O' },
		{ "curvetest", 1, 0, 'C' },
		{ "gtktheme", 0, 0, 'g' },
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

		case 'n':
			no_splash = false;
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

		default:
			break;
		}
	}

	if (optind < argc) {
		session_name = argv[optind++];
	}

	return 0;
}

