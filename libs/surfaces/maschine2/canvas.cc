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

#include <vector>

#include <cairomm/region.h>
#include <cairomm/surface.h>
#include <cairomm/context.h>

#include "pbd/compose.h"
#include "pbd/error.h"

#include "canvas.h"
#include "layout.h"

#include "maschine2.h"
#include "m2device.h"

#include "pbd/i18n.h"

#ifdef __APPLE__
#define Rect ArdourCanvas::Rect
#endif

using namespace ArdourCanvas;
using namespace ArdourSurface;
using namespace PBD;

Maschine2Canvas::Maschine2Canvas (Maschine2&m, M2Device* hw)
	: m2 (m)
{
	context = Cairo::Context::create (hw->surface ());
	expose_region = Cairo::Region::create ();
	_width = hw->surface ()->get_width ();
	_height = hw->surface ()->get_height ();

	hw->vblank.connect_same_thread (vblank_connections, boost::bind (&Maschine2Canvas::expose, this));
}

Maschine2Canvas::~Maschine2Canvas ()
{
}

void
Maschine2Canvas::request_redraw ()
{
	request_redraw (Rect (0, 0, _width, _height));
}

void
Maschine2Canvas::queue_resize ()
{
	/* nothing to do here, for now */
}

void
Maschine2Canvas::request_redraw (Rect const & r)
{
	Cairo::RectangleInt cr;

	cr.x = r.x0;
	cr.y = r.y0;
	cr.width = r.width();
	cr.height = r.height();

	expose_region->do_union (cr);

	/* next vblank will redraw */
}

bool
Maschine2Canvas::expose ()
{
	if (expose_region->empty()) {
		return false; /* nothing drawn */
	}

	/* set up clipping */

	const int nrects = expose_region->get_num_rectangles ();

	for (int n = 0; n < nrects; ++n) {
		Cairo::RectangleInt r = expose_region->get_rectangle (n);
		context->rectangle (r.x, r.y, r.width, r.height);
	}

	context->clip ();

	Maschine2Layout* layout = m2.current_layout();

	if (layout) {
		/* all layouts cover (at least) the full size of the video
		   display, so we do not need to check if the layout intersects
		   the bounding box of the full expose region.
		*/
		Cairo::RectangleInt r = expose_region->get_extents();
		Rect rr (r.x, r.y, r.x + r.width, r.y + r.height);
		layout->render (Rect (r.x, r.y, r.x + r.width, r.y + r.height), context);
	}

	context->reset_clip ();

	/* why is there no "reset()" method for Cairo::Region? */
	expose_region = Cairo::Region::create ();
	return true;
}

void
Maschine2Canvas::request_size (Duple)
{
	/* fixed size canvas */
}

Rect
Maschine2Canvas::visible_area () const
{
	/* may need to get more sophisticated once we do scrolling */
	return Rect (0, 0, _width, _height);
}

Glib::RefPtr<Pango::Context>
Maschine2Canvas::get_pango_context ()
{
	if (!pango_context) {
		PangoFontMap* map = pango_cairo_font_map_get_default ();
		if (!map) {
			error << _("Default Cairo font map is null!") << endmsg;
			return Glib::RefPtr<Pango::Context> ();
		}

		PangoContext* context = pango_font_map_create_context (map);

		if (!context) {
			error << _("cannot create new PangoContext from cairo font map") << endmsg;
			return Glib::RefPtr<Pango::Context> ();
		}

		pango_context = Glib::wrap (context);
	}

	return pango_context;
}
