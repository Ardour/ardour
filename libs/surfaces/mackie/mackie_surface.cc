#include <cmath>
#include <sstream>
#include <string>
#include <cstdio>

#include "controls.h"
#include "mackie_surface.h"
#include "mackie_midi_builder.h"
#include "surface_port.h"

using namespace Mackie;

void 
MackieSurface::display_timecode (SurfacePort & port, MackieMidiBuilder & builder, const std::string & timecode, const std::string & timecode_last)
{
	port.write (builder.timecode_display (port, timecode, timecode_last));
}

float 
MackieSurface::scaled_delta (const ControlState & state, float current_speed)
{
	return state.sign *  (std::pow (float(state.ticks + 1), 2) + current_speed) / 100.0;
}

