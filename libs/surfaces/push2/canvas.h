/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_push2_canvas_h__
#define __ardour_push2_canvas_h__

#include <cairomm/refptr.h>
#include <glibmm/threads.h>

#include "canvas/canvas.h"

namespace Cairo {
	class ImageSurface;
	class Context;
	class Region;
}

namespace Pango {
	class Context;
}

namespace ArdourSurface {

class Push2;

/* A canvas which renders to the Push2 display */

class Push2Canvas : public ArdourCanvas::Canvas
{
  public:
	Push2Canvas (Push2& p2, int cols, int rows);
	~Push2Canvas();

	void request_redraw ();
	void request_redraw (ArdourCanvas::Rect const &);
	void queue_resize ();
	bool vblank ();

	int rows() const { return _rows; }
	int cols() const { return _cols; }

	static double inter_button_spacing() { return 120.0; }

	ArdourCanvas::Coord width() const { return cols(); }
	ArdourCanvas::Coord height() const { return rows(); }
	void request_size (ArdourCanvas::Duple);
	ArdourCanvas::Rect visible_area () const;

	/* API that does nothing since we have no input events */
	void ungrab () {}
	void grab (ArdourCanvas::Item*) {}
	void focus (ArdourCanvas::Item*) {}
	void unfocus (ArdourCanvas::Item*) {}
	void re_enter() {}
	void pick_current_item (int) {}
	void pick_current_item (ArdourCanvas::Duple const &, int) {}
	bool get_mouse_position (ArdourCanvas::Duple&) const { return false; }

	Glib::RefPtr<Pango::Context> get_pango_context ();

  private:
	Push2& _p2;
	int _cols;
	int _rows;

	static const int _pixels_per_row;

	int pixel_area () const { return _rows * _pixels_per_row; }

	uint8_t   _sample_header[16];
	uint16_t* _device_sample_buffer;

	Cairo::RefPtr<Cairo::ImageSurface> _sample_buffer;
	Cairo::RefPtr<Cairo::Context>      _context;
	Cairo::RefPtr<Cairo::Region>       _expose_region;
	Glib::RefPtr<Pango::Context>       _pango_context;

	bool expose ();
	int blit_to_device_sample_buffer ();
};

} /* namespace ArdourSurface */

#endif /* __ardour_push2_canvas_h__ */
