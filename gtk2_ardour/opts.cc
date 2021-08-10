/*
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2007 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2006-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2017 Len Ovens <len@ovenwerks.net>
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
bool  ARDOUR_COMMAND_LINE::show_key_actions = false;
bool  ARDOUR_COMMAND_LINE::show_actions = false;
bool ARDOUR_COMMAND_LINE::no_splash = false;
bool ARDOUR_COMMAND_LINE::just_version = false;
bool ARDOUR_COMMAND_LINE::new_session = false;
bool ARDOUR_COMMAND_LINE::try_hw_optimization = true;
bool ARDOUR_COMMAND_LINE::no_connect_ports = false;
string ARDOUR_COMMAND_LINE::keybindings_path = ""; /* empty means use builtin default */
std::string ARDOUR_COMMAND_LINE::menus_file = "ardour.menus";
bool ARDOUR_COMMAND_LINE::finder_invoked_ardour = false;
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
		<< _("  -A, --actions               Print all possible menu action names\n")
		<< _("  -b, --bindings              Display all current key bindings\n")
		<< _("  -B, --bypass-plugins        Bypass all plugins in an existing session\n")
		<< _("  -c, --name <name>           Use a specific backend client name, default is ardour\n")
		<< _("  -d, --disable-plugins       Disable all plugins (safe mode)\n")
#ifndef NDEBUG
		<< _("  -D, --debug <options>       Set debug flags. Use \"-D list\" to see available options\n")
#endif
		<< _("  -h, --help                  Print this message\n")
		<< _("  -k, --keybindings <file>    Path to the key bindings file to load\n")
		<< _("  -m, --menus file            Use \"file\" to define menus\n")
		<< _("  -n, --no-splash             Do not show splash screen\n")
		<< _("  -N, --new <session-name>    Create a new session from the command line\n")
		<< _("  -O, --no-hw-optimizations   Disable h/w specific optimizations\n")
		<< _("  -P, --no-connect-ports      Do not connect any ports at startup\n")
		<< _("  -S, --sync                  Draw the GUI synchronously\n")
		<< _("  -T, --template <name>       Use given template for new session\n")
		<< _("  -v, --version               Print version and exit\n")
		<< "\n\n"
		<< _("Report bugs to http://tracker.ardour.org\n")
		<< _("Website http://ardour.org\n")
		;
	return 1;
}

int
ARDOUR_COMMAND_LINE::parse_opts (int argc, char *argv[])
{
	const char *optstring = "aAbBc:C:dD:hHk:E:m:N:nOp:PST:U:v";
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
		{ "actions", 0, 0, 'A' },
		{ "bindings", 0, 0, 'b' },
		{ "bypass-plugins", 0, 0, 'B' },
		{ "disable-plugins", 0, 0, 'd' },
		{ "debug", 1, 0, 'D' },
		{ "no-splash", 0, 0, 'n' },
		{ "menus", 1, 0, 'm' },
		{ "name", 1, 0, 'c' },
		{ "new", 1, 0, 'N' },
		{ "no-hw-optimizations", 0, 0, 'O' },
		{ "sync", 0, 0, 'S' },
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
			exit (EXIT_SUCCESS);
			break;
		case 'H':
#ifndef NDEBUG
			ProcessorBox::show_all_processors = true;
#endif
			break;
		case 'a':
			check_announcements = false;
			break;

		case 'A':
			show_actions = true;
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
#ifndef NDEBUG
			if (PBD::parse_debug_options (optarg)) {
				exit (EXIT_SUCCESS);
			}
#endif /* NDEBUG */
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

		case 'c':
			backend_client_name = optarg;
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
