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

#include <gtkmm2ext/cell_renderer_pixbuf_toggle.h>

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace Gtkmm2ext;


CellRendererPixbufToggle::CellRendererPixbufToggle() :
	Glib::ObjectBase( typeid(CellRendererPixbufToggle) ),
	Gtk::CellRenderer(),
	property_pixbuf_(*this, "pixbuf"),
	property_active_(*this, "active", false)
{
	property_mode() = Gtk::CELL_RENDERER_MODE_ACTIVATABLE;
	property_xpad() = 2;
	property_ypad() = 2;
	property_sensitive() = false;
}

Glib::PropertyProxy< Glib::RefPtr<Gdk::Pixbuf> >
CellRendererPixbufToggle::property_pixbuf()
{
	return property_pixbuf_.get_proxy();
}

Glib::PropertyProxy<bool>
CellRendererPixbufToggle::property_active()
{
	return property_active_.get_proxy();
}

// Overridden methods of the parent CellRenderer
Glib::PropertyProxy_Base
CellRendererPixbufToggle::_property_renderable()
{
	return property_pixbuf();
}

bool
CellRendererPixbufToggle::activate_vfunc(GdkEvent*, Gtk::Widget&, const Glib::ustring& path, const Gdk::Rectangle&, const Gdk::Rectangle&, Gtk::CellRendererState)
{
	signal_toggled_(path);
	return true;
}



void
CellRendererPixbufToggle::render_vfunc (const Glib::RefPtr<Gdk::Drawable>& window, Gtk::Widget& /*widget*/, const Gdk::Rectangle& /*background_area*/, const Gdk::Rectangle& cell_area, const Gdk::Rectangle& /*expose_area*/, Gtk::CellRendererState /*flags*/)
{
	int offset_width = 0;
	int offset_height = 0;

	if(property_active() == true){

		offset_width = cell_area.get_x() +  (int)(cell_area.get_width() - inactive_pixbuf->get_width())/2;
		offset_height = cell_area.get_y() + (int)(cell_area.get_height() - inactive_pixbuf->get_height())/2;

		window->draw_pixbuf (RefPtr<GC>(), active_pixbuf, 0, 0, offset_width, offset_height, -1, -1, Gdk::RGB_DITHER_NORMAL, 0, 0);
	}
	else {
		offset_width = cell_area.get_x() + (int)(cell_area.get_width() - inactive_pixbuf->get_width())/2;
		offset_height = cell_area.get_y() + (int)(cell_area.get_height() - inactive_pixbuf->get_height())/2;

		window->draw_pixbuf (RefPtr<GC>(), inactive_pixbuf, 0, 0, offset_width, offset_height, -1, -1, Gdk::RGB_DITHER_NORMAL, 0, 0);
	}
}

void
CellRendererPixbufToggle::get_size_vfunc (Gtk::Widget& /*widget*/, const Gdk::Rectangle* /*cell_area*/, int* /*x_offset*/, int* /*y_offset*/, int* /*width*/, int* /*height*/) const
{
//cerr << "cell_renderer_pixbuf_toggle get_size" << endl;

}

void
CellRendererPixbufToggle::set_active_pixbuf(Glib::RefPtr<Gdk::Pixbuf> pixbuf){
	active_pixbuf = pixbuf;
}

void
CellRendererPixbufToggle::set_inactive_pixbuf(Glib::RefPtr<Gdk::Pixbuf> pixbuf){
	inactive_pixbuf = pixbuf;
}

CellRendererPixbufToggle::SignalToggled&
CellRendererPixbufToggle::signal_toggled()
{
  return signal_toggled_;
}
