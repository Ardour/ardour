/*
    Copyright (C) 2012 Paul Davis 

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

#include <cmath>

#include "ardour/session.h"

#include "jog_wheel.h"
#include "mackie_control_protocol.h"
#include "surface_port.h"
#include "controls.h"
#include "surface.h"

#include <algorithm>

using namespace Mackie;

JogWheel::JogWheel (MackieControlProtocol & mcp)
  : _mcp (mcp)
  , _mode (scroll)
{
}

void
JogWheel::set_mode (Mode m)
{
	_mode = m;
}

void JogWheel::jog_event (float delta)
{
	if (_mcp.zoom_mode()) {
		if  (delta > 0) {
			for  (unsigned int i = 0; i < fabs (delta); ++i) {
				_mcp.ZoomIn();
			}
		} else {
			for  (unsigned int i = 0; i < fabs (delta); ++i) {
				_mcp.ZoomOut();
			}
		}
		return;
	}

	switch  (_mode) {
	case scroll:
		_mcp.ScrollTimeline (delta/4.0);
		break;
	default:
		break;
	}
}

