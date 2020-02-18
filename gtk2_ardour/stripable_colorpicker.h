/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef __gtkardour_stripable_colorpicker_h__
#define __gtkardour_stripable_colorpicker_h__

#include <boost/shared_ptr.hpp>
#include <gtkmm/colorbutton.h>
#include <gtkmm/colorselection.h>

#include "ardour/stripable.h"

class StripableColorDialog : public Gtk::ColorSelectionDialog
{
public:
	StripableColorDialog ();
	~StripableColorDialog ();
	void reset ();
	void popup (boost::shared_ptr<ARDOUR::Stripable> s);
	void popup (const std::string&, uint32_t);
	sigc::signal<void, uint32_t> ColorChanged;

private:
	void initialize_color_palette ();
	void finish_color_edit (int response);
	void color_changed ();

	boost::shared_ptr<ARDOUR::Stripable> _stripable;
	ARDOUR::PresentationInfo::color_t _initial_color;

	sigc::connection _color_changed_connection;


	static bool palette_initialized;
	static void palette_changed_hook (const Glib::RefPtr<Gdk::Screen>&, const Gdk::ArrayHandle_Color&);
	static Gtk::ColorSelection::SlotChangePaletteHook gtk_palette_changed_hook;
};

class ArdourColorButton : public Gtk::ColorButton
{
public:
	ArdourColorButton ();

protected:
	void on_clicked();
	void color_selected (uint32_t color);

private:
	StripableColorDialog _color_picker;
};

#endif
