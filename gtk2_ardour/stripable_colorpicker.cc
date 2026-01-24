/*
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2026      Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/stripable.h"

#include "gtkmm2ext/colors.h"

#include "gui_thread.h"
#include "public_editor.h"
#include "stripable_colorpicker.h"
#include "ui_config.h"

using namespace Gtk;

StripableColorDialog::StripableColorDialog (std::shared_ptr<ARDOUR::Stripable> s)
{
	assert (s);

	_stripable = s;
	_stripable->set_active_color_picker (this);

	signal_response().connect (sigc::mem_fun (*this, &StripableColorDialog::finish_color_edit));
}

StripableColorDialog::~StripableColorDialog ()
{
	hide ();
	_stripable->set_active_color_picker (nullptr);
	_stripable.reset ();
	_connections.drop_connections ();
}

void
StripableColorDialog::popup (Gtk::Window* parent)
{
	ArdourColorDialog::popup (_stripable->name(), _stripable->presentation_info().color(), parent);
}

void
StripableColorDialog::finish_color_edit (int response)
{
	ARDOUR::RouteList rl = PublicEditor::instance().get_selection().tracks.routelist();

	if (response == RESPONSE_OK) {
		for (ARDOUR::RouteList::iterator i = rl.begin(); i != rl.end(); ++i) {
			(*i)->presentation_info().set_color (Gtkmm2ext::gdk_color_to_rgba (get_color_selection()->get_current_color()));
		}
		_stripable->presentation_info().set_color (Gtkmm2ext::gdk_color_to_rgba (get_color_selection()->get_current_color()));
	} else {
		_stripable->presentation_info().set_color (_initial_color);
	}

	hide ();
}

void
StripableColorDialog::color_changed ()
{
	_stripable->presentation_info().set_color (Gtkmm2ext::gdk_color_to_rgba (get_color_selection()->get_current_color()));
}

