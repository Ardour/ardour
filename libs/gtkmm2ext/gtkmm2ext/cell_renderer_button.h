/*
 * Copyright (C) 2026 Robin Gareus <robin@gareus.org>
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

#include <ytkmm/cellrenderer.h>
#include <ytkmm/drawingarea.h>
#include <ytkmm/widget.h>

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext
{

class LIBGTKMM2EXT_API CellRendererButton : public Gtk::CellRenderer
{
public:
	CellRendererButton ();
	virtual ~CellRendererButton () {};

	void render_vfunc (const Glib::RefPtr<Gdk::Drawable>& window, Gtk::Widget& widget, const Gdk::Rectangle& background_area, const Gdk::Rectangle& cell_area, const Gdk::Rectangle& expose_area, Gtk::CellRendererState flags);
	void get_size_vfunc (Gtk::Widget& widget, const Gdk::Rectangle* cell_area, int* x_offset, int* y_offset, int* width, int* height) const;
	bool activate_vfunc (GdkEvent*, Gtk::Widget&, const Glib::ustring& path, const Gdk::Rectangle&, const Gdk::Rectangle&, Gtk::CellRendererState);

	Glib::PropertyProxy_Base         _property_renderable ();
	Glib::PropertyProxy<std::string> property_label ();

	typedef sigc::signal<void, const Glib::ustring&> SignalClicked;

	SignalClicked& signal_clicked ();

protected:
private:
	Glib::Property<std::string> _property_label;
	SignalClicked               _signal_clicked;
};

} // namespace Gtkmm2ext
