/*
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
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

#include "pbd/compose.h"

#include "gtkmm2ext/colors.h"

#include "public_editor.h"
#include "stripable_colorpicker.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace Gtk;

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
		get_color_selection()->set_change_palette_hook (&StripableColorDialog::palette_changed_hook);

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
StripableColorDialog::popup (const std::string& name, uint32_t color, Gtk::Window* parent)
{
	set_title (string_compose (_("Color Selection: %1"), name));
	_initial_color = color;

	get_color_selection()->set_has_opacity_control (false);
	get_color_selection()->set_has_palette (true);

	Gdk::Color c = Gtkmm2ext::gdk_color_from_rgba (_initial_color);

	get_color_selection()->set_previous_color (c);
	get_color_selection()->set_current_color (c);
	_color_changed_connection.disconnect ();
	_color_changed_connection = get_color_selection()->signal_color_changed().connect (sigc::mem_fun (*this, &StripableColorDialog::color_changed));

	if (parent) {
		set_transient_for (*parent);
	}
	set_position (Gtk::WIN_POS_MOUSE);
	present ();
}

void
StripableColorDialog::popup (std::shared_ptr<ARDOUR::Stripable> s, Gtk::Window* parent)
{
	if (s && s->active_color_picker()) {
		if (parent) {
			s->active_color_picker()->set_transient_for (*parent);
		}
		s->active_color_picker()->set_position (Gtk::WIN_POS_CENTER_ALWAYS); // force update
		s->active_color_picker()->set_position (Gtk::WIN_POS_MOUSE);
		s->active_color_picker()->present ();
		return;
	}
	if (_stripable == s) {
		/* keep modified color */
		if (parent) {
			set_transient_for (*parent);
		}
		set_position (Gtk::WIN_POS_CENTER_ALWAYS); // force update
		set_position (Gtk::WIN_POS_MOUSE);
		present ();
		return;
	}

	_stripable = s;
	_stripable->set_active_color_picker (this);
	popup (s->name(), _stripable->presentation_info().color (), parent);
}

void
StripableColorDialog::finish_color_edit (int response)
{
	ARDOUR::RouteList rl = PublicEditor::instance().get_selection().tracks.routelist();

	if (response == RESPONSE_OK) {
		ColorChanged (Gtkmm2ext::gdk_color_to_rgba (get_color_selection()->get_current_color())); /* EMIT SIGNAL */
	}
	if (_stripable && response == RESPONSE_OK) {
		for (ARDOUR::RouteList::iterator i = rl.begin(); i != rl.end(); ++i) {
			(*i)->presentation_info().set_color (Gtkmm2ext::gdk_color_to_rgba (get_color_selection()->get_current_color()));
		}
		_stripable->presentation_info().set_color (Gtkmm2ext::gdk_color_to_rgba (get_color_selection()->get_current_color()));
	} else if (_stripable) {
		_stripable->presentation_info().set_color (_initial_color);
	}
	reset ();
}

void
StripableColorDialog::color_changed ()
{
	if (_stripable) {
		_stripable->presentation_info().set_color (Gtkmm2ext::gdk_color_to_rgba (get_color_selection()->get_current_color()));
	}
}


ArdourColorButton::ArdourColorButton ()
{
	_color_picker.ColorChanged.connect (sigc::mem_fun(*this, &ArdourColorButton::color_selected));
}

void
ArdourColorButton::on_clicked ()
{
	_color_picker.popup ("", Gtkmm2ext::gdk_color_to_rgba (get_color ()), dynamic_cast<Gtk::Window*> (get_toplevel()));
	_color_picker.get_window ()->set_transient_for (get_window ());
}

void
ArdourColorButton::color_selected (uint32_t color)
{
	Gdk::Color c;
	Gtkmm2ext::set_color_from_rgba (c, color);
	set_color (c);
	g_signal_emit_by_name (GTK_WIDGET(gobj()), "color-set", 0);
}
