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

#ifndef __ardour_push2_h__
#define __ardour_push2_h__

#include <vector>
#include <map>
#include <list>
#include <set>

#include <libusb.h>

#include <cairomm/refptr.h>
#include <glibmm/threads.h>

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"
#include "midi++/types.h"
#include "ardour/types.h"
#include "control_protocol/control_protocol.h"

#include "midi_byte_array.h"

namespace Cairo {
	class ImageSurface;
}

namespace MIDI {
	class Parser;
	class Port;
}

namespace ARDOUR {
	class AsyncMIDIPort;
	class Port;
}

namespace ArdourSurface {

struct Push2Request : public BaseUI::BaseRequestObject {
public:
	Push2Request () {}
	~Push2Request () {}
};

class Push2 : public ARDOUR::ControlProtocol
            , public AbstractUI<Push2Request>
{
   public:
	Push2 (ARDOUR::Session&);
	~Push2 ();

	static bool probe ();
	static void* request_factory (uint32_t);

	int set_active (bool yn);

   private:
	libusb_device_handle *handle;
	Glib::Threads::Mutex fb_lock;
	uint8_t   frame_header[16];
	uint16_t* device_frame_buffer[2];
	int  device_buffer;
	Cairo::RefPtr<Cairo::ImageSurface> frame_buffer;
	sigc::connection vblank_connection;

	static const int cols;
	static const int rows;
	static const int pixels_per_row;

	void do_request (Push2Request*);
	int stop ();
	int open ();
	int close ();
	int render ();
	bool vblank ();

	struct LED
	{
		enum State {
			Off,
			OneShot24th,
			OneShot16th,
			OneShot8th,
			OneShot4th,
			OneShot2th,
			Pulsing24th,
			Pulsing16th,
			Pulsing8th,
			Pulsing4th,
			Pulsing2th,
			Blinking24th,
			Blinking16th,
			Blinking8th,
			Blinking4th,
			Blinking2th
		};

		enum Type {
			Pad,
			ColorButton,
			WhiteButton,
			TouchStrip,
		};



		uint8_t id;
		Type    type;
		uint8_t extra;
		uint8_t color_index;
		uint8_t state;

		LED (uint8_t i, Type t, uint8_t e) : id (i), type (t), extra (e), color_index (0), state (Off) {}
		LED () : id (0), type (Pad), extra (0), color_index (0), state (Off) {}

		MidiByteArray update ();

		void set_color (uint8_t color_index);
		void set_state (State state);
	};

	std::map<int,LED> leds;
	void set_led_color (uint32_t id, uint8_t color_index);
	void set_led_state (uint32_t id, LED::State);
	void build_led_map ();

	MIDI::Port* _input_port[2];
	MIDI::Port* _output_port[2];
	boost::shared_ptr<ARDOUR::Port> _async_in[2];
	boost::shared_ptr<ARDOUR::Port> _async_out[2];

	void write (int port, const MidiByteArray&);
	bool midi_input_handler (Glib::IOCondition ioc, MIDI::Port* port);
};


} /* namespace */

#endif /* __ardour_push2_h__ */
