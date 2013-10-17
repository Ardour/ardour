/*
    Copyright (C) 2000-2009 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __gtkmm2ext_cell_renderer_pixbuf_multi_h__ 
#define __gtkmm2ext_cell_renderer_pixbuf_multi_h__

#include <map>
#include <stdint.h>

#include <gtkmm/drawingarea.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/widget.h>
#include <gtkmm/cellrenderer.h>
#include <gdkmm.h>

#include "gtkmm2ext/visibility.h"

using namespace Gtk;

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API CellRendererPixbufMulti : public Gtk::CellRenderer
{
  public:

	CellRendererPixbufMulti();  
	virtual ~CellRendererPixbufMulti(){};

	virtual void render_vfunc (const Glib::RefPtr<Gdk::Drawable>& window, Gtk::Widget& widget, const Gdk::Rectangle& background_area, const Gdk::Rectangle& cell_area, const Gdk::Rectangle& expose_area, Gtk::CellRendererState flags);

	virtual void get_size_vfunc (Gtk::Widget& widget, const Gdk::Rectangle* cell_area, int* x_offset, int* y_offset, int* width, int* height) const;

	virtual bool activate_vfunc(GdkEvent*, Gtk::Widget&, const Glib::ustring& path, const Gdk::Rectangle&, const Gdk::Rectangle&, Gtk::CellRendererState);

	Glib::PropertyProxy_Base _property_renderable();

	Glib::PropertyProxy<uint32_t> property_state();

	void set_pixbuf(uint32_t state, Glib::RefPtr<Gdk::Pixbuf> pixbuf);
	
	typedef sigc::signal<void, const Glib::ustring&> SignalChanged;

	SignalChanged& signal_changed();

  protected:
  
  private:
	Glib::Property<uint32_t> property_state_; 

	std::map<uint32_t,Glib::RefPtr<Gdk::Pixbuf> > _pixbufs;

	//void on_cell_toggled(const Glib::ustring& path_string);

	SignalChanged signal_changed_;
};

} // namespace

#endif /* __gtkmm2ext_cell_renderer_pixbuf_multi_h__ */
