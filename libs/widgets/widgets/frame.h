/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
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

#ifndef _WIDGETS_FRAME_H_
#define _WIDGETS_FRAME_H_

#include <boost/optional.hpp>
#include <gtkmm/bin.h>

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/cairo_theme.h"
#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API Frame : public Gtk::Bin, public Gtkmm2ext::CairoTheme
{
public:
	enum Orientation {
		Horizontal,
		Vertical
	};

	Frame (Orientation orientation = Horizontal, bool force_boxy = false);
	~Frame ();

	void set_label (std::string const&);
	void set_padding (int);
	void set_edge_color (Gtkmm2ext::Color);
	void reset_edge_color ();

protected:
	void on_add (Gtk::Widget*);
	void on_remove (Gtk::Widget*);
	void on_size_request (GtkRequisition*);
	void on_size_allocate (Gtk::Allocation&);
	bool on_expose_event (GdkEventExpose*);
	void on_style_changed (const Glib::RefPtr<Gtk::Style>&);
	void on_name_changed ();

private:
	Glib::RefPtr<Gtk::Style> get_parent_style ();

	Orientation                 _orientation;
	Gtk::Widget*                _w;
	Gtk::Widget*                _current_parent;
	sigc::connection            _parent_style_change;
	Glib::RefPtr<Pango::Layout> _layout;
	std::string                 _label_text;
	boost::optional<Gdk::Color> _edge_color;
	GtkRequisition              _min_size;

	int  _border;
	int  _padding;
	int  _label_pad_w;
	int  _label_pad_h;
	int  _label_left;
	int  _text_width;
	int  _text_height;
	int  _alloc_x0;
	int  _alloc_y0;
	bool _boxy;
};

} // namespace ArdourWidgets

#endif
