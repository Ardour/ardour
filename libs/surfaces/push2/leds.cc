#include <algorithm>

#include "push2.h"

using namespace ArdourSurface;
using std::make_pair;
using std::max;
using std::min;

void
Push2::LED::set_color (uint8_t ci)
{
	color_index = max (uint8_t(0), min (uint8_t(127), ci));
}

void
Push2::LED::set_state (LED::State s)
{
	state = s;
}

MidiByteArray
Push2::LED::update ()
{
	MidiByteArray msg;

	switch (type) {
	case Pad:
	case TouchStrip:
		msg.push_back (0x90);
		break;
	case ColorButton:
	case WhiteButton:
		msg.push_back (0xb0);
		break;
	}

	msg.push_back (state);
	msg.push_back (color_index);

	return msg;
}

void
Push2::set_led_color (uint32_t id, uint8_t color_index)
{
	leds[id].set_color (color_index);
	// write (leds[id].update ());
}

void
Push2::build_led_map ()
{
	uint8_t id = 0;
	uint8_t extra;

	/* Touch strip - there is only one */

	leds.insert (make_pair (id, LED (id, LED::TouchStrip, 12)));
	id++;

	/* Pads

	   Pad 0 is in the bottom left corner, id rises going left=>right
	   across each row
	*/

	for (extra = 36; id < 64; ++id, ++extra) {
		leds.insert (make_pair (id, LED (id, LED::Pad, extra)));
	}

	/* Buttons

	   We start with Button 0 at the upper left of the surface, increasing
	   across the device and wrapping, until we're at the Master button on
	   the right.

	   Then we descend down the left side. Then down the right side of the
	   pads. Finally the column on the far right., going clockwise around
	   each 4-way diagonal button.

	   66 buttons in total
	*/

	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 3)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 9)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 102)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 103)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 104)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 105)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 106)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 107)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 108)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 109)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 30)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 59)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 118)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 52)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 110)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 112)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 119)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 53)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 111)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 113)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 60)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 61)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 29)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 20)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 21)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 22)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 23)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 24)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 25)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 26)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 27)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 28)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 35)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 117)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 116)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 88)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 87)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 90)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 89)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 86)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 85)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 43)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 42)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 41)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 40)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 39)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 38)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 37)));
	leds.insert (make_pair (id, LED (id, LED::ColorButton, 36)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 46)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 45)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 47)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 44)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 56)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 57)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 58)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 31)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 50)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 51)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 55)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 63)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 54)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 62)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 49)));
	leds.insert (make_pair (id, LED (id, LED::WhiteButton, 48)));
}
