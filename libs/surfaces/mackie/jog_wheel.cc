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

