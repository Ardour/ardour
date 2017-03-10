/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "stripable_colorpicker.h"
#include "utils.h"

using namespace Gtk;
using namespace ARDOUR_UI_UTILS;

StripableColorDialog::StripableColorDialog ()
{
	signal_response().connect (sigc::mem_fun (*this, &StripableColorDialog::finish_color_edit));
}

StripableColorDialog::~StripableColorDialog ()
{
	reset ();
}

void
StripableColorDialog::reset ()
{
	hide ();
	_stripable.reset ();
}

void
StripableColorDialog::popup (boost::shared_ptr<ARDOUR::Stripable> s)
{
	if (_stripable == s) {
		/* keep modified color */
		present ();
		return;
	}

	_stripable = s;

	get_colorsel()->set_has_opacity_control (false);
	get_colorsel()->set_has_palette (true);

	Gdk::Color c = gdk_color_from_rgba (_stripable->presentation_info().color ());

	get_colorsel()->set_previous_color (c);
	get_colorsel()->set_current_color (c);

	present ();
}

void
StripableColorDialog::finish_color_edit (int response)
{
	if (_stripable && response == RESPONSE_OK) {
		_stripable->presentation_info().set_color (gdk_color_to_rgba (get_colorsel()->get_current_color()));
	}
	reset ();
}
