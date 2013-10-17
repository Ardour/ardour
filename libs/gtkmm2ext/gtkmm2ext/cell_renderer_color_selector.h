/*
    Copyright (C) 2011 Paul Davis 

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

#ifndef __gtkmm2ext_cell_renderer_color_selector_h__ 
#define __gtkmm2ext_cell_renderer_color_selector_h__

#include <gtkmm/drawingarea.h>
#include <gtkmm/widget.h>
#include <gtkmm/cellrenderer.h>
#include <gdkmm.h>

#include "gtkmm2ext/visibility.h"

using namespace Gtk;

namespace Gtk {
	class ColorSelectionDialog;
}

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API CellRendererColorSelector : public Gtk::CellRenderer
{
  public:
	CellRendererColorSelector();  
	virtual ~CellRendererColorSelector();

	virtual void render_vfunc (const Glib::RefPtr<Gdk::Drawable>& window, Gtk::Widget& widget, const Gdk::Rectangle& background_area, const Gdk::Rectangle& cell_area, const Gdk::Rectangle& expose_area, Gtk::CellRendererState flags);

	Glib::PropertyProxy<Gdk::Color> property_color();

  private:
	Glib::Property<Gdk::Color> _property_color;
};

} // namespace

#endif /* __gtkmm2ext_cell_renderer_pixbuf_toggle_h__ */
