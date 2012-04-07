#include "bcf_surface.h"
#include "surface_port.h"
#include "mackie_midi_builder.h"

#include <cmath>

using namespace Mackie;

void BcfSurface::display_bank_start (SurfacePort & port, MackieMidiBuilder & builder, uint32_t current_bank)
{
	if  (current_bank == 0) {
		// send Ar. to 2-char display on the master
		port.write (builder.two_char_display ("Ar", ".."));
	} else {
		// write the current first remote_id to the 2-char display
		port.write (builder.two_char_display (current_bank));
	}
}

void 
BcfSurface::zero_all (SurfacePort & port, MackieMidiBuilder & builder)
{
	// clear 2-char display
	port.write (builder.two_char_display ("LC"));

	// and the led ring for the master strip
	blank_jog_ring (port, builder);
}

void 
BcfSurface::blank_jog_ring (SurfacePort & port, MackieMidiBuilder & builder)
{
	Control & control = *controls_by_name["jog"];
	port.write (builder.build_led_ring (dynamic_cast<Pot &> (control), off));
}

float 
BcfSurface::scaled_delta (const ControlState & state, float current_speed)
{
	return state.sign *  (std::pow (float(state.ticks + 1), 2) + current_speed) / 100.0;
}

void
BcfSurface::init_strips ()
{
}
