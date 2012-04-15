#include <cmath>

#include "ardour/session.h"

#include "mackie_jog_wheel.h"
#include "mackie_control_protocol.h"
#include "surface_port.h"
#include "controls.h"
#include "surface.h"

#include <algorithm>

using namespace Mackie;

JogWheel::JogWheel (MackieControlProtocol & mcp)
: _mcp (mcp)
, _transport_speed (4.0)
, _transport_direction (0)
, _shuttle_speed (0.0)
{
}

JogWheel::State JogWheel::jog_wheel_state() const
{
	if  (!_jog_wheel_states.empty())
		return _jog_wheel_states.top();
	else
		return scroll;
}

void JogWheel::zoom_event (SurfacePort &, Control &, const ControlState &)
{
}

void JogWheel::scrub_event (SurfacePort &, Control &, const ControlState &)
{
}

void JogWheel::speed_event (SurfacePort &, Control &, const ControlState &)
{
}

void JogWheel::scroll_event (SurfacePort &, Control &, const ControlState &)
{
}

void JogWheel::jog_event (SurfacePort &, Control &, float delta)
{
	// TODO use current snap-to setting?
	switch  (jog_wheel_state())
	{
	case scroll:
		_mcp.ScrollTimeline (delta);
		break;
	
	case zoom:
		// Chunky Zoom.
		// TODO implement something similar to ScrollTimeline which
		// ends up in Editor::control_scroll for smoother zooming.
		if  (delta > 0) {
			for  (unsigned int i = 0; i < fabs (delta); ++i) {
				_mcp.ZoomIn();
			}
		} else {
			for  (unsigned int i = 0; i < fabs (delta); ++i) {
				_mcp.ZoomOut();
			}
		}
		break;
		
	case speed:
		// locally, _transport_speed is an positive value
		_transport_speed += _mcp.surfaces.front()->scaled_delta (delta, _mcp.get_session().transport_speed());

		// make sure no weirdness gets to the session
		if  (_transport_speed < 0 || isnan (_transport_speed))
		{
			_transport_speed = 0.0;
		}
		
		// translate _transport_speed speed to a signed transport velocity
		_mcp.get_session().request_transport_speed_nonzero (transport_speed() * transport_direction());
		break;
	
	case scrub:
	{
		if  (delta != 0) {
			add_scrub_interval (_scrub_timer.restart());
			// x clicks per second => speed == 1.0
			float speed = _mcp.surfaces.front()->scrub_scaling_factor() / average_scrub_interval() * delta;
			_mcp.get_session().request_transport_speed_nonzero (speed);
		}
		else
		{
			// we have a stop event
			check_scrubbing();
		}
		break;
	}
	
	case shuttle:
		_shuttle_speed = _mcp.get_session().transport_speed();
		_shuttle_speed += _mcp.surfaces.front()->scaled_delta (delta, _mcp.get_session().transport_speed());
		_mcp.get_session().request_transport_speed_nonzero (_shuttle_speed);
		break;
	
	case select:
		std::cout << "JogWheel select not implemented" << std::endl;
		break;
	}
}

void JogWheel::check_scrubbing()
{
	// if the last elapsed is greater than the average + std deviation, then stop
	if  (!_scrub_intervals.empty() && _scrub_timer.elapsed() > average_scrub_interval() + std_dev_scrub_interval())
	{
		_mcp.get_session().request_transport_speed (0.0);
		_scrub_intervals.clear();
	}
}

void JogWheel::push (State state)
{
	_jog_wheel_states.push (state);
}

void JogWheel::pop()
{
	if  (_jog_wheel_states.size() > 0)
	{
		_jog_wheel_states.pop();
	}
}

void JogWheel::zoom_state_toggle()
{
	if  (jog_wheel_state() == zoom)
		pop();
	else
		push (zoom);
}

JogWheel::State JogWheel::scrub_state_cycle()
{
	State top = jog_wheel_state();
	if  (top == scrub)
	{
		// stop scrubbing and go to shuttle
		pop();
		push (shuttle);
		_shuttle_speed = 0.0;
	}
	else if  (top == shuttle)
	{
		// default to scroll, or the last selected
		pop();
	}
	else
	{
		// start with scrub
		push (scrub);
	}
	
	return jog_wheel_state();
}

void JogWheel::add_scrub_interval (unsigned long elapsed)
{
	if  (_scrub_intervals.size() > 5)
	{
		_scrub_intervals.pop_front();
	}
	_scrub_intervals.push_back (elapsed);
}

float JogWheel::average_scrub_interval()
{
	float sum = 0.0;
	for  (std::deque<unsigned long>::iterator it = _scrub_intervals.begin(); it != _scrub_intervals.end(); ++it)
	{
		sum += *it;
	}
	return sum / _scrub_intervals.size(); 
}

float JogWheel::std_dev_scrub_interval()
{
	float average = average_scrub_interval();
	
	// calculate standard deviation
	float sum = 0.0;
	for  (std::deque<unsigned long>::iterator it = _scrub_intervals.begin(); it != _scrub_intervals.end(); ++it)
	{
		sum += pow (*it - average, 2);
	}
	return sqrt (sum / _scrub_intervals.size() -1);
}
