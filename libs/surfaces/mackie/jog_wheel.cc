/*
 * Copyright (C) 2006-2007 John Anderson
 * Copyright (C) 2012-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#include <cmath>

#include "ardour/session.h"

#include "button.h"
#include "jog_wheel.h"
#include "mackie_control_protocol.h"
#include "surface_port.h"
#include "controls.h"
#include "surface.h"

#include <algorithm>

using namespace ArdourSurface;
using namespace Mackie;

JogWheel::JogWheel (MackieControlProtocol & mcp)
  : _mcp (mcp)
  , _mode (scroll)
{
	/* do it again to get the LED in the correct state */
	set_mode (scroll);
}

void
JogWheel::set_mode (Mode m)
{
	_mode = m;
	if (_mode == shuttle) {
		_mcp.update_global_button (Button::Scrub, on);
	} else {
		_mcp.update_global_button (Button::Scrub, off);
	}
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
		if (delta > 0) {
			_mcp.button_varispeed (true);
		} else if (delta < 0) {
			_mcp.button_varispeed (false);
		}
		break;
	}
}

