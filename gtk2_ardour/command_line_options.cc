/*
    Copyright (C) 2011 Paul Davis

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

#include <gtk/gtk.h>

#include <glibmm/optionentry.h>
#include <glibmm/optioncontext.h>

#include "pbd/pbd.h"

#include "ardour/ardour.h"
#include "ardour/session.h"

#include "command_line_options.h"

#include "i18n.h"

CommandLineOptions::CommandLineOptions()
	: Glib::OptionGroup("Ardour", _("Ardour options"), _("Command-line options for Ardour"))
	, session_name()
	, show_key_actions(false)
	, check_announcements(false)
	, no_splash(true)
	, just_version(false)
	, jack_client_name("ardour")
	, new_session(false)
	, curvetest_file()
	, no_connect_ports(false)
	, use_gtk_theme(false)
	, keybindings_path()
	, menus_file()
	, finder_invoked_ardour(false)
	, immediate_save()
	, jack_session_uuid()
	, load_template()
{
	Glib::OptionEntry entry;

	entry.set_long_name("file");
	entry.set_short_name('f');
	entry.set_description(_("The Session filename"));
	add_entry_filename(entry, session_name);

	entry.set_long_name("version");
	entry.set_short_name('v');
	entry.set_description(_("Show version information"));
	add_entry(entry, just_version);

	entry.set_long_name("bindings");
	entry.set_short_name('b');
	entry.set_description(_("Print all possible keyboard binding names"));
	add_entry(entry, show_key_actions);

	entry.set_long_name("name");
	entry.set_short_name('c');
	entry.set_description(_("Use a specific jack client name, default is ardour"));
	add_entry(entry, jack_client_name);

	entry.set_long_name("no-announcements");
	entry.set_short_name('a');
	entry.set_description(_("Do not contact website for announcements"));
	add_entry(entry, check_announcements);

	entry.set_long_name("no-splash");
	entry.set_short_name('n');
	entry.set_description(_("Don't show splash screen"));
	add_entry(entry, no_splash);

	entry.set_long_name("menus");
	entry.set_short_name('m');
	entry.set_description(_("Use \"file\" to define menus"));
	add_entry_filename(entry, menus_file);

	entry.set_long_name("new");
	entry.set_short_name('N');
	entry.set_description(_("Create a new session from the command line"));
	add_entry_filename(entry, new_session_name);

	entry.set_long_name("save");
	entry.set_short_name('E');
	entry.set_description(_("Load the specified session, save it to <file> and then quit"));
	add_entry_filename(entry, immediate_save);

	entry.set_long_name("curvetest");
	entry.set_short_name('C');
	entry.set_description(_("Curve algorithm debugger"));
	add_entry_filename(entry, curvetest_file);

	entry.set_long_name("no-connect-ports");
	entry.set_short_name('P');
	entry.set_description(_("Do not connect any ports at startup"));
	add_entry(entry, no_connect_ports);

	entry.set_long_name("keybindings");
	entry.set_short_name('k');
	entry.set_description(_("Name of key bindings to load (default is ~/.ardour3/ardour.bindings)"));
	add_entry_filename(entry, keybindings_path);

	entry.set_long_name("template");
	entry.set_short_name('T');
	entry.set_description(_("Create a new session from template"));
	add_entry_filename(entry, load_template);

}

bool
CommandLineOptions::on_pre_parse (Glib::OptionContext&, Glib::OptionGroup&)
{
	if (getenv ("ARDOUR_SAE")) {
		menus_file = "ardour-sae.menus";
		keybindings_path = "SAE";
	}

	return true;
}

bool
CommandLineOptions::on_post_parse (Glib::OptionContext&, Glib::OptionGroup&)
{
	if (!new_session_name.empty()) {
		new_session = true;
		session_name = new_session_name;
	}

	return true;
}

CommandLineOptions&
get_cmdline_opts ()
{
	static CommandLineOptions options;
	return options;
}

bool
parse_cmdline_opts (int *argc, char ***argv)
{
	Glib::OptionContext context("[SESSION_NAME]");

	Glib::OptionGroup gtk_options(gtk_get_option_group(TRUE));

	CommandLineOptions& options = get_cmdline_opts();
	context.set_main_group(options);
	context.add_group(ARDOUR::get_options());
	context.add_group(PBD::get_options());
	context.add_group(gtk_options);

	try {
		context.parse(*argc, *argv);
	}

	catch(const Glib::OptionError& ex) {
		std::cout << _("Error while parsing command-line options: ") << std::endl << ex.what() << std::endl;
		std::cout << _("Use --help to see a list of available command-line options.") << std::endl;
		return false;
	}

	catch(const Glib::Error& ex) {
		std::cout << "Error: " << ex.what() << std::endl;
		return false;
	}

	// If session name is specified without the -f
	// option it should be left unparsed in argv
	if (options.session_name.empty() && (*argc > 1)) {
		const char* filename = *argv[1];
		if(filename) {
			options.session_name = filename;
		}
	}

	return true;
}
