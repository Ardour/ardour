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

#include "pbd/compose.h"

#include "ardour/stripable.h"

#include "gtkmm2ext/colors.h"

#include "ardour_color_dialog.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace Gtk;

bool ArdourColorDialog::palette_initialized = false;
Gtk::ColorSelection::SlotChangePaletteHook ArdourColorDialog::gtk_palette_changed_hook;

ArdourColorDialog::ArdourColorDialog ()
	: _initial_color (0)
{
	initialize_color_palette ();
	get_color_selection()->set_has_opacity_control (false);
	get_color_selection()->set_has_palette (true);
}

void
ArdourColorDialog::palette_changed_hook (const Glib::RefPtr<Gdk::Screen>& s, const Gdk::ArrayHandle_Color& c)
{
	std::string p = std::string (ColorSelection::palette_to_string (c));
	UIConfiguration::instance ().set_stripable_color_palette (p);
	gtk_palette_changed_hook (s, c);
}

void
ArdourColorDialog::initialize_color_palette ()
{
	// non-static member, because it needs a screen()
	if (palette_initialized) {
		return;
	}
	gtk_palette_changed_hook = get_color_selection()->set_change_palette_hook (&ArdourColorDialog::palette_changed_hook);

	std::string cp = UIConfiguration::instance ().get_stripable_color_palette ();
	if (!cp.empty()) {
		Gdk::ArrayHandle_Color c = ColorSelection::palette_from_string (cp);
		gtk_palette_changed_hook (get_screen (), c);
	}
	palette_initialized = true;
}

void
ArdourColorDialog::popup (const std::string& name, uint32_t color, Gtk::Window* parent)
{
	set_title (string_compose (_("Color Selection: %1"), name));
	_initial_color = color;

	Gtk::ColorSelection* color_selection (get_color_selection());

	Gdk::Color c = Gtkmm2ext::gdk_color_from_rgba (_initial_color);

	color_selection->set_previous_color (c);
	color_selection->set_current_color (c);

	color_selection->signal_color_changed().connect (sigc::mem_fun (*this, &ArdourColorDialog::color_changed));

 	if (parent) {
 		set_transient_for (*parent);
 	}

	present ();
}

/* ---------- */

ArdourColorButton::ArdourColorButton ()
{
	_color_picker.get_color_selection()->signal_color_changed().connect (sigc::mem_fun(*this, &ArdourColorButton::color_selected));
	_color_picker.signal_response().connect (sigc::mem_fun (*this, &ArdourColorButton::finish));
}

void
ArdourColorButton::finish (int response)
{
	switch (response) {
	case Gtk::RESPONSE_OK:
		break;
	default:
		Gdk::Color c (Gtkmm2ext::gdk_color_from_rgba (_color_picker.initial_color()));
		set_color (c);
		g_signal_emit_by_name (GTK_WIDGET(gobj()), "color-set", 0);
		break;
	}

	_color_picker.hide ();
}

void
ArdourColorButton::on_clicked ()
{
	_color_picker.popup ("", Gtkmm2ext::gdk_color_to_rgba (get_color ()), dynamic_cast<Gtk::Window*> (get_toplevel()));
	_color_picker.get_window ()->set_transient_for (get_window ());
}

void
ArdourColorButton::color_selected ()
{
	Gdk::Color c (_color_picker.get_color_selection()->get_current_color());
	set_color (c);
	g_signal_emit_by_name (GTK_WIDGET(gobj()), "color-set", 0);
}

