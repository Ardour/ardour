/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <cairo/cairo.h>
#include <gtkmm/notebook.h>

#include "gtkmm2ext/cairo_widget.h"

#include "ardour/export_status.h"

#include "ardour_dialog.h"

class CimgArea : public CairoWidget
{
public:
	CimgArea (Cairo::RefPtr<Cairo::ImageSurface> sf)
		: CairoWidget()
		, _surface(sf)
	{
		set_size_request (sf->get_width (), sf->get_height ());
	}

	virtual void render (cairo_t* cr, cairo_rectangle_t* r)
	{
		cairo_rectangle (cr, r->x, r->y, r->width, r->height);
		cairo_clip (cr);
		cairo_set_source_surface (cr, _surface->cobj(), 0, 0);
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		cairo_paint (cr);
	}

private:
	Cairo::RefPtr<Cairo::ImageSurface> _surface;
};

class ExportReport : public ArdourDialog
{
public:
	typedef boost::shared_ptr<ARDOUR::ExportStatus> StatusPtr;
	ExportReport (ARDOUR::Session*, StatusPtr);
	int run ();

private:
	void open_clicked (std::string);

	StatusPtr       status;
	Gtk::Notebook   pages;
};
