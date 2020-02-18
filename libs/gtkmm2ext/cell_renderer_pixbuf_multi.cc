/*
 * Copyright (C) 2009-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#include <iostream>
#include <gtkmm.h>

#include <gtkmm2ext/cell_renderer_pixbuf_multi.h>

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace Gtkmm2ext;


CellRendererPixbufMulti::CellRendererPixbufMulti() :
	Glib::ObjectBase( typeid(CellRendererPixbufMulti) ),
	Gtk::CellRenderer(),
	property_state_(*this, "active", 0)
{
	property_mode() = Gtk::CELL_RENDERER_MODE_ACTIVATABLE;
	property_xpad() = 2;
	property_ypad() = 2;
	property_sensitive() = false;
}

Glib::PropertyProxy<uint32_t>
CellRendererPixbufMulti::property_state()
{
	return property_state_.get_proxy();
}

// Overridden methods of the parent CellRenderer
Glib::PropertyProxy_Base
CellRendererPixbufMulti::_property_renderable()
{
	return property_state();
}

bool
CellRendererPixbufMulti::activate_vfunc(GdkEvent*, Gtk::Widget&, const Glib::ustring& path, const Gdk::Rectangle&, const Gdk::Rectangle&, Gtk::CellRendererState)
{
	signal_changed_(path);
	return true;
}

void
CellRendererPixbufMulti::render_vfunc (const Glib::RefPtr<Gdk::Drawable>& window, Gtk::Widget& /*widget*/, const Gdk::Rectangle& /*background_area*/, const Gdk::Rectangle& cell_area, const Gdk::Rectangle& /*expose_area*/, Gtk::CellRendererState /*flags*/)
{
	int offset_width = 0;
	int offset_height = 0;
	Glib::RefPtr<Pixbuf> pb = _pixbufs[property_state()];

	offset_width = cell_area.get_x() +  (int)(cell_area.get_width() - pb->get_width())/2;
	offset_height = cell_area.get_y() + (int)(cell_area.get_height() - pb->get_height())/2;

	window->draw_pixbuf (RefPtr<GC>(), pb, 0, 0, offset_width, offset_height, -1, -1, Gdk::RGB_DITHER_NORMAL, 0, 0);
}

void
CellRendererPixbufMulti::get_size_vfunc (Gtk::Widget& /*widget*/, const Gdk::Rectangle* /*cell_area*/, int* /*x_offset*/, int* /*y_offset*/, int* /*width*/, int* /*height*/) const
{
}

void
CellRendererPixbufMulti::set_pixbuf(uint32_t which, Glib::RefPtr<Gdk::Pixbuf> pixbuf){
	_pixbufs[which] = pixbuf;
}

CellRendererPixbufMulti::SignalChanged&
CellRendererPixbufMulti::signal_changed()
{
  return signal_changed_;
}
