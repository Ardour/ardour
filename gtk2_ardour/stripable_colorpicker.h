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

#pragma once

#include <memory>

#include <ytkmm/colorbutton.h>
#include <ytkmm/colorselection.h>

#include "ardour/presentation_info.h"

namespace ARDOUR {
	class Stripable;
}

class ArdourColorDialog : public Gtk::ColorSelectionDialog
{
  public:
	ArdourColorDialog ();

	void popup (const std::string& name, uint32_t color, Gtk::Window* parent);
	ARDOUR::PresentationInfo::color_t initial_color() const { return _initial_color; }
	virtual void color_changed() {}

  protected:
	ARDOUR::PresentationInfo::color_t _initial_color;

  private:
	void initialize_color_palette ();

	static bool palette_initialized;
	static void palette_changed_hook (const Glib::RefPtr<Gdk::Screen>&, const Gdk::ArrayHandle_Color&);
	static Gtk::ColorSelection::SlotChangePaletteHook gtk_palette_changed_hook;
};

class StripableColorDialog : public ArdourColorDialog
{
public:
	StripableColorDialog (std::shared_ptr<ARDOUR::Stripable>);
	~StripableColorDialog ();
	void popup (Gtk::Window*);

private:
	void finish_color_edit (int response);
	void color_changed ();

	std::shared_ptr<ARDOUR::Stripable> _stripable;

	sigc::connection _color_changed_connection;
	PBD::ScopedConnectionList _connections;
};

class ArdourColorButton : public Gtk::ColorButton
{
public:
	ArdourColorButton ();

protected:
	void on_clicked();
	void color_selected ();
	void finish (int response);

private:
	ArdourColorDialog _color_picker;
};
