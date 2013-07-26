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

#include <iostream>
#include <cstdlib>

#include <glibmm/optioncontext.h>
#include <glibmm/thread.h>

#include "pbd/pbd.h"
#include "pbd/debug.h"
#include "pbd/id.h"
#include "pbd/enumwriter.h"

#include "i18n.h"

namespace {

static bool libpbd_initialized = false;
static bool libpbd_options_parsed = false;

}

namespace PBD {

class OptionGroup : public Glib::OptionGroup
{
public:
	OptionGroup();

	Glib::ustring m_arg_debug;

	virtual bool on_post_parse (Glib::OptionContext&, Glib::OptionGroup&);
};

OptionGroup::OptionGroup()
	: Glib::OptionGroup("libpbd", _("libpbd options"), _("Command-line options for libpbd"))
	, m_arg_debug()
{
	Glib::OptionEntry entry;

	entry.set_long_name("debug");
	entry.set_short_name('D');
	entry.set_description(_("Set debug flags. Use \"-D list\" to see available options."));
	add_entry(entry, m_arg_debug);
}

bool
OptionGroup::on_post_parse (Glib::OptionContext&, Glib::OptionGroup&)
{
	if (!m_arg_debug.empty()) {
		if (PBD::parse_debug_options (m_arg_debug.c_str())) {
			exit(0);
		}
	}

	libpbd_options_parsed = true;
	return true;
}

} // namespace PBD

Glib::OptionGroup&
PBD::get_options ()
{
	static OptionGroup options;
	return options;
}

bool
parse_options (int *argc, char ***argv)
{
	if (libpbd_options_parsed) {
		// already been parsed by another OptionContext
		return true;
	}

	// i18n?

	Glib::OptionContext context;

	context.set_main_group(PBD::get_options());

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

	return true;
}

bool
PBD::init (int *argc, char ***argv)
{
	if (libpbd_initialized) {
		return true;
	}

	if (!Glib::thread_supported()) {
		Glib::thread_init();
	}

	if (!parse_options (argc, argv)) {
		return false;
	}

	PBD::ID::init ();

	libpbd_initialized = true;
	return true;
}

void
PBD::cleanup ()
{
	EnumWriter::destroy ();
}
