/*
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2011-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2011-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2020 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/label.h>
#include "pbd/compose.h"
#include "missing_filesource_dialog.h"
#include "pbd/i18n.h"

using namespace Gtk;
using namespace std;
using namespace ARDOUR;
using namespace PBD;

MissingFileSourceDialog::MissingFileSourceDialog (Session * s, list<string> const & plugins, DataType dt)
	: ArdourDialog (_("Missing Source Files"), true, false)
{
	/* This dialog is always shown programatically. Center the window.*/
	set_position (Gtk::WIN_POS_CENTER);

	set_session (s);

	add_button (_("OK"), RESPONSE_OK);
	set_default_response (RESPONSE_OK);

	Label* m = manage (new Label);

	stringstream t;
	t << string_compose (_("This session misses following %1 files.\nThey have been replaced with silence:\n\n"), dt.to_string());

	for (list<string>::const_iterator i = plugins.begin(); i != plugins.end(); ++i) {
		t << *i << "\n";
	}

	t << _("\nThe Regions and edits have been retained.\n"
	       "If this is unexpected, manually loacate the files and restore them in the session folder.\n");

	if (dt == DataType::MIDI) {
		t << _("Editing the MIDI files by adding new content will re-create the file and disable this warning,\n"
		       "but also prevent future recovery of the original in the exising region(s).\n");
	}

	m->set_markup (t.str ());
	get_vbox()->pack_start (*m, false, false);

	show_all ();
}
