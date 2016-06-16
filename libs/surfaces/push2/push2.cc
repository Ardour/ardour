/*
	Copyright (C) 2016 Paul Davis

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <cairomm/context.h>
#include <cairomm/surface.h>
#include <pangomm/layout.h>

#include "pbd/compose.h"
#include "pbd/debug.h"
#include "pbd/failed_constructor.h"

#include "ardour/debug.h"

#include "push2.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;

#include "i18n.h"

#include "pbd/abstract_ui.cc" // instantiate template

const int Push2::cols = 960;
const int Push2::rows = 160;
const int Push2::pixels_per_row = 1024;

#define ABLETON 0x2982
#define PUSH2   0x1967

Push2::Push2 (Session& s)
	: ControlProtocol (s, string (X_("Ableton Push2")))
	, AbstractUI<Push2Request> (name())
	, handle (0)
	, device_buffer (0)
	, frame_buffer (Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, cols, rows))
{
}

Push2::~Push2 ()
{
	close ();
}

int
Push2::open ()
{
	if ((handle = libusb_open_device_with_vid_pid (NULL, ABLETON, PUSH2)) == 0) {
		return -1;
	}

	libusb_claim_interface (handle, 0x00);

	device_frame_buffer[0] = new uint16_t[rows*pixels_per_row];
	device_frame_buffer[1] = new uint16_t[rows*pixels_per_row];

	memset (device_frame_buffer[0], 0, sizeof (uint16_t) * rows * pixels_per_row);
	memset (device_frame_buffer[1], 0, sizeof (uint16_t) * rows * pixels_per_row);

	frame_header[0] = 0xef;
	frame_header[1] = 0xcd;
	frame_header[2] = 0xab;
	frame_header[3] = 0x89;
	memset (&frame_header[4], 0, 12);

	return 0;
}

int
Push2::close ()
{
	vblank_connection.disconnect ();

	if (handle) {
		libusb_release_interface (handle, 0x00);
		libusb_close (handle);
	}

	delete [] device_frame_buffer[0];
	device_frame_buffer[0] = 0;

	delete [] device_frame_buffer[1];
	device_frame_buffer[1] = 0;

	return 0;
}

bool
Push2::probe ()
{
	libusb_device_handle *h;
	libusb_init (NULL);

	if ((h = libusb_open_device_with_vid_pid (NULL, ABLETON, PUSH2)) == 0) {
		DEBUG_TRACE (DEBUG::Push2, "no Push2 device found\n");
		return false;
	}

	libusb_close (h);
	DEBUG_TRACE (DEBUG::Push2, "Push2 device located\n");
	return true;
}

void*
Push2::request_factory (uint32_t num_requests)
{
	/* AbstractUI<T>::request_buffer_factory() is a template method only
	   instantiated in this source module. To provide something visible for
	   use in the interface/descriptor, we have this static method that is
	   template-free.
	*/
	return request_buffer_factory (num_requests);
}

void
Push2::do_request (Push2Request * req)
{
	DEBUG_TRACE (DEBUG::Push2, string_compose ("doing request type %1\n", req->type));
	if (req->type == CallSlot) {

		call_slot (MISSING_INVALIDATOR, req->the_slot);

	} else if (req->type == Quit) {

		stop ();
	}
}

int
Push2::stop ()
{
	close ();
	BaseUI::quit ();

	return 0;
}

/** render host-side frame buffer (a Cairo ImageSurface) to the current
 * device-side frame buffer. The device frame buffer will be pushed to the
 * device on the next call to vblank()
 */

int
Push2::render ()
{
	/* ensure that all drawing has been done before we fetch pixel data */

	frame_buffer->flush ();

	const int stride = 3840; /* bytes per row for Cairo::FORMAT_ARGB32 */
	const uint8_t* data = frame_buffer->get_data ();

	/* fill frame buffer (320kB) */

	Glib::Threads::Mutex::Lock lm (fb_lock);

	uint16_t* fb = (uint16_t*) device_frame_buffer[device_buffer];

	for (int row = 0; row < rows; ++row) {

		const uint8_t* dp = data + row * stride;

		for (int col = 0; col < cols; ++col) {

			/* fetch r, g, b (range 0..255). Ignore alpha */

			const int r = (*((const uint32_t*)dp) >> 16) & 0xff;
			const int g = (*((const uint32_t*)dp) >> 8) & 0xff;
			const int b = *((const uint32_t*)dp) & 0xff;

			/* convert to 5 bits, 6 bits, 5 bits, respectively */
			/* generate 16 bit BGB565 value */

			*fb++ = (r >> 3) | ((g & 0xfc) << 3) | ((b & 0xf8) << 8);

			dp += 4;
		}

		/* skip 128 bytes to next line. This is filler, used to avoid line borders occuring in the middle of 512
		   byte USB buffers
		*/

		fb += 64; /* 128 bytes = 64 int16_t */
	}

	/* swap buffers (under lock protection) */
	// device_buffer = (device_buffer ? 0 : 1);

	return 0;
}

bool
Push2::vblank ()
{
	int transferred = 0;
	const int timeout_msecs = 1000;
	int err;

	if ((err = libusb_bulk_transfer (handle, 0x01, frame_header, sizeof (frame_header), &transferred, timeout_msecs))) {
		return false;
	}

	{
		Glib::Threads::Mutex::Lock lm (fb_lock);

		if ((err = libusb_bulk_transfer (handle, 0x01, (uint8_t*) device_frame_buffer[device_buffer] , 2 * rows * pixels_per_row, &transferred, timeout_msecs))) {
			return false;
		}
	}

	return true;
}

int
Push2::set_active (bool yn)
{
	DEBUG_TRACE (DEBUG::Push2, string_compose("Push2Protocol::set_active init with yn: '%1'\n", yn));

	if (yn == active()) {
		return 0;
	}

	if (yn) {

		if (open ()) {
			DEBUG_TRACE (DEBUG::Push2, "device open failed\n");
			close ();
			return -1;
		}

		/* start event loop */

		BaseUI::run ();

		// connect_session_signals ();

		/* say hello */

		Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create (frame_buffer);
		if (!context) {
			cerr << "Cannot create context\n";
			return -1;
		}
		Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);
		if (!layout) {
			cerr << "Cannot create layout\n";
			return -1;
		}

		layout->set_text ("hello, Ardour");
		Pango::FontDescription fd ("Sans Bold 12");
		layout->set_font_description (fd);

		context->set_source_rgb (0.0, 1.0, 1.0);
		context->rectangle (0, 0, 960, 160);
		context->fill ();
		context->set_source_rgb (0.0, 0.0, 0.0);
		context->rectangle (50, 50, 860, 60);
		context->fill ();
		context->move_to (60, 60);
		context->set_source_rgb ((random()%255) / 255.0, (random()%255) / 255.0, (random()%255) / 255.0);
		layout->update_from_cairo_context (context);
		layout->show_in_cairo_context (context);

		render ();

		/* set up periodic task used to push a frame buffer to the
		 * device (25fps). The device can handle 60fps, but we don't
		 * need that frame rate.
		 */

		Glib::RefPtr<Glib::TimeoutSource> vblank_timeout = Glib::TimeoutSource::create (40); // milliseconds
		vblank_connection = vblank_timeout->connect (sigc::mem_fun (*this, &Push2::vblank));
		vblank_timeout->attach (main_loop()->get_context());

	} else {

		BaseUI::quit ();
		close ();

	}

	ControlProtocol::set_active (yn);

	DEBUG_TRACE (DEBUG::Push2, string_compose("Push2Protocol::set_active done with yn: '%1'\n", yn));

	return 0;
}
