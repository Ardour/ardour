/*
    Copyright (C) 2016 Paul Davis

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

#include <cairomm/region.h>
#include <cairomm/surface.h>
#include <cairomm/context.h>

#include "pbd/compose.h"

#include "ardour/debug.h"

#include "canvas.h"
#include "layout.h"
#include "push2.h"

using namespace ArdourCanvas;
using namespace ArdourSurface;
using namespace PBD;

const int Push2Canvas::pixels_per_row = 1024;

Push2Canvas::Push2Canvas (Push2& pr, int c, int r)
	: p2 (pr)
	, _cols (c)
	, _rows (r)
	, frame_buffer (Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, _cols, _rows))
{
	context = Cairo::Context::create (frame_buffer);
	expose_region = Cairo::Region::create ();

	device_frame_buffer = new uint16_t[pixel_area()];
	memset (device_frame_buffer, 0, sizeof (uint16_t) * pixel_area());

	frame_header[0] = 0xef;
	frame_header[1] = 0xcd;
	frame_header[2] = 0xab;
	frame_header[3] = 0x89;

	memset (&frame_header[4], 0, 12);
}

Push2Canvas::~Push2Canvas ()
{
	delete [] device_frame_buffer;
	device_frame_buffer = 0;
}

bool
Push2Canvas::vblank ()
{
	/* re-render dirty areas, if any */

	if (expose ()) {
		DEBUG_TRACE (DEBUG::Push2, "re-blit to device frame buffer\n");
		/* something rendered, update device_frame_buffer */
		blit_to_device_frame_buffer ();
	}

	int transferred = 0;
	const int timeout_msecs = 1000;
	int err;

	/* transfer to device */

	if ((err = libusb_bulk_transfer (p2.usb_handle(), 0x01, frame_header, sizeof (frame_header), &transferred, timeout_msecs))) {
		return false;
	}

	if ((err = libusb_bulk_transfer (p2.usb_handle(), 0x01, (uint8_t*) device_frame_buffer, 2 * pixel_area (), &transferred, timeout_msecs))) {
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
	cr.width = r.height();

	DEBUG_TRACE (DEBUG::Push2, string_compose ("invalidate rect %1\n", r));

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

	DEBUG_TRACE (DEBUG::Push2, string_compose ("expose with %1 rects\n", nrects));

	for (int n = 0; n < nrects; ++n) {
		Cairo::RectangleInt r = expose_region->get_rectangle (n);
		context->rectangle (r.x, r.y, r.width, r.height);
	}

	context->clip ();

	Push2Layout* layout = p2.current_layout();

	if (layout) {
		Cairo::RectangleInt r = expose_region->get_extents();
		Rect rr (r.x, r.y, r.x + r.width, r.y + r.height);
		DEBUG_TRACE (DEBUG::Push2, string_compose ("render layout with %1\n", rr));
		layout->render (Rect (r.x, r.y, r.x + r.width, r.y + r.height), context);
	}

	context->reset_clip ();

	/* why is there no "reset()" method for Cairo::Region? */

	expose_region = Cairo::Region::create ();

	return true;
}

/** render host-side frame buffer (a Cairo ImageSurface) to the current
 * device-side frame buffer. The device frame buffer will be pushed to the
 * device on the next call to vblank()
 */

int
Push2Canvas::blit_to_device_frame_buffer ()
{
	/* ensure that all drawing has been done before we fetch pixel data */

	frame_buffer->flush ();

	const int stride = 3840; /* bytes per row for Cairo::FORMAT_ARGB32 */
	const uint8_t* data = frame_buffer->get_data ();

	/* fill frame buffer (320kB) */

	uint16_t* fb = (uint16_t*) device_frame_buffer;

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
