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

#include "public_editor.h"
#include "stripable_colorpicker.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR_UI_UTILS;

bool StripableColorDialog::palette_initialized = false;
Gtk::ColorSelection::SlotChangePaletteHook StripableColorDialog::gtk_palette_changed_hook;

StripableColorDialog::StripableColorDialog ()
{
	initialize_color_palette ();
	signal_response().connect (sigc::mem_fun (*this, &StripableColorDialog::finish_color_edit));

#ifdef __APPLE__
	/* hide eyedropper button -- which does not work on OSX:
	 * " the problem is worse than just `not getting the color' though.
	 *   The action doesn't actually complete, and window focus is in a
	 *   `weird state' until you click inside the color-picker dialog twice;
	 *   then it all seems back to normal (but no color got picked)"
	 *
	 * the alternative is to patch gtk's source:
	 * gtk/gtkcolorsel.c  gtk_color_selection_init() which packs
	 *
	 *  top_hbox [ VBOX [ triangle || hbox [ sample-area || picker-button] ] || ... ]
	 */
	ColorSelection* cs = get_colorsel(); // IS-A VBOX
	if (!cs) { return ; }
	Gtk::HBox* top_hbox = dynamic_cast<Gtk::HBox*> (cs->children()[0].get_widget());
	if (!top_hbox) { return ; }
	Gtk::VBox* vbox = dynamic_cast<Gtk::VBox*> (top_hbox->children()[0].get_widget());
	if (!vbox) { return ; }
	Gtk::HBox* hbox = dynamic_cast<Gtk::HBox*> (vbox->children()[1].get_widget());
	if (!hbox) { return ; }
	Gtk::Button* picker = dynamic_cast<Gtk::Button*> (hbox->children()[1].get_widget());
	if (!picker) { return ; }
	picker->hide ();
#endif
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
StripableColorDialog::popup (const std::string& name, uint32_t color)
{
	set_title (string_compose (_("Color Selection: %1"), name));
	_initial_color = color;

	get_colorsel()->set_has_opacity_control (false);
	get_colorsel()->set_has_palette (true);

	Gdk::Color c = gdk_color_from_rgba (_initial_color);

	get_colorsel()->set_previous_color (c);
	get_colorsel()->set_current_color (c);
	_color_changed_connection.disconnect ();
	_color_changed_connection = get_colorsel()->signal_color_changed().connect (sigc::mem_fun (*this, &StripableColorDialog::color_changed));

	present ();
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
	popup (s->name(), _stripable->presentation_info().color ());
}

void
StripableColorDialog::finish_color_edit (int response)
{
	ARDOUR::RouteList rl = PublicEditor::instance().get_selection().tracks.routelist();

	if (response == RESPONSE_OK) {
		ColorChanged (gdk_color_to_rgba (get_colorsel()->get_current_color())); /* EMIT SIGNAL */
	}
	if (_stripable && response == RESPONSE_OK) {
		for (ARDOUR::RouteList::iterator i = rl.begin(); i != rl.end(); ++i) {
			(*i)->presentation_info().set_color (gdk_color_to_rgba (get_colorsel()->get_current_color()));
		}
		_stripable->presentation_info().set_color (gdk_color_to_rgba (get_colorsel()->get_current_color()));
	} else if (_stripable) {
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


ArdourColorButton::ArdourColorButton ()
{
	_color_picker.ColorChanged.connect (sigc::mem_fun(*this, &ArdourColorButton::color_selected));
}

void
ArdourColorButton::on_clicked ()
{
	_color_picker.popup ("", gdk_color_to_rgba (get_color ()));
	_color_picker.get_window ()->set_transient_for (get_window ());
}

void
ArdourColorButton::color_selected (uint32_t color)
{
	Gdk::Color c;
	set_color_from_rgba (c, color);
	set_color (c);
	g_signal_emit_by_name (GTK_WIDGET(gobj()), "color-set", 0);
}
