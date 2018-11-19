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

#include <vector>

#include <cairomm/region.h>
#include <cairomm/surface.h>
#include <cairomm/context.h>

#include "pbd/compose.h"
#include "pbd/error.h"

#include "ardour/debug.h"

#include "canvas.h"
#include "layout.h"
#include "push2.h"

#include "pbd/i18n.h"

#ifdef __APPLE__
#define Rect ArdourCanvas::Rect
#endif

using namespace ArdourCanvas;
using namespace ArdourSurface;
using namespace PBD;

const int Push2Canvas::pixels_per_row = 1024;

Push2Canvas::Push2Canvas (Push2& pr, int c, int r)
	: p2 (pr)
	, _cols (c)
	, _rows (r)
	, sample_buffer (Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, _cols, _rows))
{
	context = Cairo::Context::create (sample_buffer);
	expose_region = Cairo::Region::create ();

	device_sample_buffer = new uint16_t[pixel_area()];
	memset (device_sample_buffer, 0, sizeof (uint16_t) * pixel_area());

	sample_header[0] = 0xef;
	sample_header[1] = 0xcd;
	sample_header[2] = 0xab;
	sample_header[3] = 0x89;

	memset (&sample_header[4], 0, 12);
}

Push2Canvas::~Push2Canvas ()
{
	delete [] device_sample_buffer;
	device_sample_buffer = 0;
}

void
Push2Canvas::queue_resize ()
{
	/* nothing to do here, for now */
}

bool
Push2Canvas::vblank ()
{
	if (_root.resize_queued()) {
		_root.layout ();
	}

	/* re-render dirty areas, if any */

	if (expose ()) {
		/* something rendered, update device_sample_buffer */
		blit_to_device_sample_buffer ();

#undef RENDER_LAYOUTS
#ifdef RENDER_LAYOUTS
		if (p2.current_layout()) {
			std::string s = p2.current_layout()->name();
			s += ".png";
			sample_buffer->write_to_png (s);
		}
#endif
	}

	int transferred = 0;
	const int timeout_msecs = 1000;
	int err;

	/* transfer to device */

	if ((err = libusb_bulk_transfer (p2.usb_handle(), 0x01, sample_header, sizeof (sample_header), &transferred, timeout_msecs))) {
		return false;
	}

	if ((err = libusb_bulk_transfer (p2.usb_handle(), 0x01, (uint8_t*) device_sample_buffer, 2 * pixel_area (), &transferred, timeout_msecs))) {
		return false;
	}

	return true;
}

void
Push2Canvas::request_redraw ()
{
	request_redraw (Rect (0, 0, _cols, _rows));
}

void
Push2Canvas::request_redraw (Rect const & r)
{
	Cairo::RectangleInt cr;

	cr.x = r.x0;
	cr.y = r.y0;
	cr.width = r.width();
	cr.height = r.height();

	// DEBUG_TRACE (DEBUG::Push2, string_compose ("invalidate rect %1\n", r));

	expose_region->do_union (cr);

	/* next vblank will redraw */
}

bool
Push2Canvas::expose ()
{
	if (expose_region->empty()) {
		return false; /* nothing drawn */
	}

	/* set up clipping */

	const int nrects = expose_region->get_num_rectangles ();

	//DEBUG_TRACE (DEBUG::Push2, string_compose ("expose with %1 rects\n", nrects));

	for (int n = 0; n < nrects; ++n) {
		Cairo::RectangleInt r = expose_region->get_rectangle (n);
		context->rectangle (r.x, r.y, r.width, r.height);
	}

	context->clip ();

	Push2Layout* layout = p2.current_layout();

	if (layout) {
		/* all layouts cover (at least) the full size of the video
		   display, so we do not need to check if the layout intersects
		   the bounding box of the full expose region.
		*/
		Cairo::RectangleInt r = expose_region->get_extents();
		Rect rr (r.x, r.y, r.x + r.width, r.y + r.height);
		//DEBUG_TRACE (DEBUG::Push2, string_compose ("render layout with %1\n", rr));
		layout->render (Rect (r.x, r.y, r.x + r.width, r.y + r.height), context);
	}

	context->reset_clip ();

	/* why is there no "reset()" method for Cairo::Region? */

	expose_region = Cairo::Region::create ();

	return true;
}

/** render host-side sample buffer (a Cairo ImageSurface) to the current
 * device-side sample buffer. The device sample buffer will be pushed to the
 * device on the next call to vblank()
 */

int
Push2Canvas::blit_to_device_sample_buffer ()
{
	/* ensure that all drawing has been done before we fetch pixel data */

	sample_buffer->flush ();

	const int stride = 3840; /* bytes per row for Cairo::FORMAT_ARGB32 */
	const uint8_t* data = sample_buffer->get_data ();

	/* fill sample buffer (320kB) */

	uint16_t* fb = (uint16_t*) device_sample_buffer;

	for (int row = 0; row < _rows; ++row) {

		const uint8_t* dp = data + row * stride;

		for (int col = 0; col < _cols; ++col) {

			/* fetch r, g, b (range 0..255). Ignore alpha */

			const int r = (*((const uint32_t*)dp) >> 16) & 0xff;
			const int g = (*((const uint32_t*)dp) >> 8) & 0xff;
			const int b = *((const uint32_t*)dp) & 0xff;

			/* convert to 5 bits, 6 bits, 5 bits, respectively */
			/* generate 16 bit BGB565 value */

			*fb++ = (r >> 3) | ((g & 0xfc) << 3) | ((b & 0xf8) << 8);

			/* the push2 docs state that we should xor the pixel
			 * data. Doing so doesn't work correctly, and not doing
			 * so seems to work fine (colors roughly match intended
			 * values).
			 */

			dp += 4;
		}

		/* skip 128 bytes to next line. This is filler, used to avoid line borders occuring in the middle of 512
		   byte USB buffers
		*/

		fb += 64; /* 128 bytes = 64 int16_t */
	}

	return 0;
}

void
Push2Canvas::request_size (Duple)
{
	/* fixed size canvas */
}

Rect
Push2Canvas::visible_area () const
{
	/* may need to get more sophisticated once we do scrolling */
	return Rect (0, 0, 960, 160);
}

Glib::RefPtr<Pango::Context>
Push2Canvas::get_pango_context ()
{
	if (!pango_context) {
		PangoFontMap* map = pango_cairo_font_map_new ();
		if (!map) {
			error << _("Default Cairo font map is null!") << endmsg;
			return Glib::RefPtr<Pango::Context> ();
		}

		PangoContext* context = pango_font_map_create_context (map);
		pango_cairo_context_set_resolution (context, 96);

		if (!context) {
			error << _("cannot create new PangoContext from cairo font map") << endmsg;
			return Glib::RefPtr<Pango::Context> ();
		}

		pango_context = Glib::wrap (context);
	}

	return pango_context;
}
