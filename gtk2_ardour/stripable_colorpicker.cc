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

#include "pbd/compose.h"
#include "pbd/i18n.h"

#include "stripable_colorpicker.h"
#include "ui_config.h"
#include "utils.h"

using namespace Gtk;
using namespace ARDOUR_UI_UTILS;

bool StripableColorDialog::palette_initialized = false;
Gtk::ColorSelection::SlotChangePaletteHook StripableColorDialog::gtk_palette_changed_hook;

StripableColorDialog::StripableColorDialog ()
{
	initialize_color_palette ();
	signal_response().connect (sigc::mem_fun (*this, &StripableColorDialog::finish_color_edit));
}

StripableColorDialog::~StripableColorDialog ()
{
	reset ();
}

void
StripableColorDialog::palette_changed_hook (const Glib::RefPtr<Gdk::Screen>& s, const Gdk::ArrayHandle_Color& c)
{
	std::string p = std::string (ColorSelection::palette_to_string (c));
	UIConfiguration::instance ().set_stripable_color_palette (p);
	gtk_palette_changed_hook (s, c);
}

void
StripableColorDialog::initialize_color_palette ()
{
	// non-static member, because it needs a screen()
	if (palette_initialized) {
		return;
	}
	gtk_palette_changed_hook =
		get_colorsel()->set_change_palette_hook (&StripableColorDialog::palette_changed_hook);

	std::string cp = UIConfiguration::instance ().get_stripable_color_palette ();
	if (!cp.empty()) {
		Gdk::ArrayHandle_Color c = ColorSelection::palette_from_string (cp);
		gtk_palette_changed_hook (get_screen (), c);
	}
	palette_initialized = true;
}

void
StripableColorDialog::reset ()
{
	hide ();
	if (_stripable && _stripable->active_color_picker() == this) {
		_stripable->set_active_color_picker (0);
	}
	_stripable.reset ();
	_color_changed_connection.disconnect ();
}

void
StripableColorDialog::popup (boost::shared_ptr<ARDOUR::Stripable> s)
{
	if (s && s->active_color_picker()) {
		s->active_color_picker()->present ();
		return;
	}
	if (_stripable == s) {
		/* keep modified color */
		present ();
		return;
	}

	_stripable = s;
	_stripable->set_active_color_picker (this);
	_initial_color = _stripable->presentation_info().color ();
	set_title (string_compose (_("Color Selection: %1"), s->name()));

	get_colorsel()->set_has_opacity_control (false);
	get_colorsel()->set_has_palette (true);

	Gdk::Color c = gdk_color_from_rgba (_initial_color);

	get_colorsel()->set_previous_color (c);
	get_colorsel()->set_current_color (c);
	_color_changed_connection = get_colorsel()->signal_color_changed().connect (sigc::mem_fun (*this, &StripableColorDialog::color_changed));

	present ();
}

void
StripableColorDialog::finish_color_edit (int response)
{
	if (_stripable && response == RESPONSE_OK) {
		_stripable->presentation_info().set_color (gdk_color_to_rgba (get_colorsel()->get_current_color()));
	} else {
		_stripable->presentation_info().set_color (_initial_color);
	}
	reset ();
}

void
StripableColorDialog::color_changed ()
{
	if (_stripable) {
		_stripable->presentation_info().set_color (gdk_color_to_rgba (get_colorsel()->get_current_color()));
	}
}
