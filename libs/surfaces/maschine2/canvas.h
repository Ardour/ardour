/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_maschine2_canvas_h_
#define _ardour_maschine2_canvas_h_

#include <cairomm/refptr.h>
#include <glibmm/threads.h>

#include "canvas/canvas.h"

namespace Cairo {
	class ImageSurface;
	class Context;
	class Region;
}

namespace ArdourSurface {

class M2Device;
class Maschine2;

/* A canvas which renders to the Push2 display */

class Maschine2Canvas : public ArdourCanvas::Canvas
{
  public:
	Maschine2Canvas (Maschine2&, M2Device*);
	~Maschine2Canvas();

	void request_redraw ();
	void request_redraw (ArdourCanvas::Rect const &);
	void queue_resize ();
	bool vblank ();

	Cairo::RefPtr<Cairo::Context> image_context() { return context; }

	ArdourCanvas::Coord width() const { return _width; }
	ArdourCanvas::Coord height() const { return _height; }

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
	int _width;
	int _height;

	Cairo::RefPtr<Cairo::Context> context;
	Cairo::RefPtr<Cairo::Region> expose_region;
	Glib::RefPtr<Pango::Context> pango_context;

	Maschine2& m2;
	PBD::ScopedConnection vblank_connections;

	bool expose ();
};

} /* namespace ArdourSurface */

#endif
