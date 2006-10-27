/*
    Copyright (C) 2000 Paul Davis 

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

    $Id$
*/

#include <fstream>
#include <algorithm>
#include <unistd.h>
#include <locale.h>
#include <errno.h>

#include <sigc++/bind.h>

#include <glibmm/thread.h>

#include <pbd/xml++.h>

#include <ardour/audioengine.h>
#include <ardour/io.h>
#include <ardour/port.h>
#include <ardour/connection.h>
#include <ardour/session.h>
#include <ardour/cycle_timer.h>
#include <ardour/panner.h>
#include <ardour/dB.h>

#include "i18n.h"

#include <cmath>

/*
  A bug in OS X's cmath that causes isnan() and isinf() to be 
  "undeclared". the following works around that
*/

#if defined(__APPLE__) && defined(__MACH__)
extern "C" int isnan (double);
extern "C" int isinf (double);
#endif


using namespace std;
using namespace ARDOUR;
using namespace PBD;

const string IO::state_node_name = "IO";
bool         IO::connecting_legal = false;
bool         IO::ports_legal = false;
bool         IO::panners_legal = false;
sigc::signal<void>                IO::Meter;
sigc::signal<int>                 IO::ConnectingLegal;
sigc::signal<int>                 IO::PortsLegal;
sigc::signal<int>                 IO::PannersLegal;
sigc::signal<void,uint32_t>  IO::MoreOutputs;
sigc::signal<int>                 IO::PortsCreated;

Glib::StaticMutex       IO::m_meter_signal_lock = GLIBMM_STATIC_MUTEX_INIT;

/* this is a default mapper of [0 .. 1.0] control values to a gain coefficient.
   others can be imagined. 
*/

static gain_t direct_control_to_gain (double fract) { 
	/* XXX Marcus writes: this doesn't seem right to me. but i don't have a better answer ... */
	/* this maxes at +6dB */
	return pow (2.0,(sqrt(sqrt(sqrt(fract)))*198.0-192.0)/6.0);
}

static double direct_gain_to_control (gain_t gain) { 
	/* XXX Marcus writes: this doesn't seem right to me. but i don't have a better answer ... */
	if (gain == 0) return 0.0;
	
	return pow((6.0*log(gain)/log(2.0)+192.0)/198.0, 8.0);
}

static bool sort_ports_by_name (Port* a, Port* b)
{
	return a->name() < b->name();
}


/** @param default_type The type of port that will be created by ensure_io
 * and friends if no type is explicitly requested (to avoid breakage).
 */
IO::IO (Session& s, string name,
	int input_min, int input_max, int output_min, int output_max,
	DataType default_type)
	: _session (s),
	  _name (name),
	  _default_type(default_type),
	  _gain_control (X_("gaincontrol"), *this),
	  _gain_automation_curve (0.0, 2.0, 1.0),
	  _input_minimum (input_min),
	  _input_maximum (input_max),
	  _output_minimum (output_min),
	  _output_maximum (output_max)
{
	_panner = new Panner (name, _session);
	_gain = 1.0;
	_desired_gain = 1.0;
	_input_connection = 0;
	_output_connection = 0;
	pending_state_node = 0;
	_ninputs = 0;
	_noutputs = 0;
	no_panner_reset = false;
	deferred_state = 0;

	apply_gain_automation = false;
	_ignore_gain_on_deliver = false;
	
	_gain_automation_state = Off;
	_gain_automation_style = Absolute;

	{
		// IO::Meter is emitted from another thread so the
		// Meter signal must be protected.
		Glib::Mutex::Lock guard (m_meter_signal_lock);
		m_meter_connection = Meter.connect (mem_fun (*this, &IO::meter));
	}

	_session.add_controllable (&_gain_control);
}

IO::~IO ()
{
	Glib::Mutex::Lock guard (m_meter_signal_lock);
	
	Glib::Mutex::Lock lm (io_lock);
	vector<Port *>::iterator i;

	for (i = _inputs.begin(); i != _inputs.end(); ++i) {
		_session.engine().unregister_port (*i);
	}

	for (i = _outputs.begin(); i != _outputs.end(); ++i) {
		_session.engine().unregister_port (*i);
	}

	m_meter_connection.disconnect();
}

void
IO::silence (nframes_t nframes, nframes_t offset)
{
	/* io_lock, not taken: function must be called from Session::process() calltree */

	for (vector<Port *>::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
		(*i)->silence (nframes, offset);
	}
}

void
IO::apply_declick (vector<Sample *>& bufs, uint32_t nbufs, nframes_t nframes, gain_t initial, gain_t target, bool invert_polarity)
{
	nframes_t declick = min ((nframes_t)128, nframes);
	gain_t delta;
	Sample *buffer;
	double fractional_shift;
	double fractional_pos;
	gain_t polscale = invert_polarity ? -1.0f : 1.0f;

	if (nframes == 0) return;
	
	fractional_shift = -1.0/declick;

	if (target < initial) {
		/* fade out: remove more and more of delta from initial */
		delta = -(initial - target);
	} else {
		/* fade in: add more and more of delta from initial */
		delta = target - initial;
	}

	for (uint32_t n = 0; n < nbufs; ++n) {

		buffer = bufs[n];
		fractional_pos = 1.0;

		for (nframes_t nx = 0; nx < declick; ++nx) {
			buffer[nx] *= polscale * (initial + (delta * (0.5 + 0.5 * cos (M_PI * fractional_pos))));
			fractional_pos += fractional_shift;
		}
		
		/* now ensure the rest of the buffer has the target value
		   applied, if necessary.
		*/

		if (declick != nframes) {
			float this_target;

			if (invert_polarity) {
				this_target = -target;
			} else { 
				this_target = target;
			}

			if (this_target == 0.0) {
				memset (&buffer[declick], 0, sizeof (Sample) * (nframes - declick));
			} else if (this_target != 1.0) {
				for (nframes_t nx = declick; nx < nframes; ++nx) {
					buffer[nx] *= this_target;
				}
			}
		}
	}
}

void
IO::pan_automated (vector<Sample*>& bufs, uint32_t nbufs, nframes_t start, nframes_t end, nframes_t nframes, nframes_t offset)
{
	Sample* dst;

	/* io_lock, not taken: function must be called from Session::process() calltree */

	if (_noutputs == 0) {
		return;
	}

	if (_noutputs == 1) {

		dst = output(0)->get_buffer (nframes) + offset;

		for (uint32_t n = 0; n < nbufs; ++n) {
			if (bufs[n] != dst) {
				memcpy (dst, bufs[n], sizeof (Sample) * nframes);
			} 
		}

		output(0)->mark_silence (false);

		return;
	}

	uint32_t o;
	vector<Port *>::iterator out;
	vector<Sample *>::iterator in;
	Panner::iterator pan;
	Sample* obufs[_noutputs];

	/* the terrible silence ... */

	for (out = _outputs.begin(), o = 0; out != _outputs.end(); ++out, ++o) {
		obufs[o] = (*out)->get_buffer (nframes) + offset;
		memset (obufs[o], 0, sizeof (Sample) * nframes);
		(*out)->mark_silence (false);
	}

	uint32_t n;

	for (pan = _panner->begin(), n = 0; n < nbufs; ++n, ++pan) {
		(*pan)->distribute_automated (bufs[n], obufs, start, end, nframes, _session.pan_automation_buffer());
	}
}

void
IO::pan (vector<Sample*>& bufs, uint32_t nbufs, nframes_t nframes, nframes_t offset, gain_t gain_coeff)
{
	Sample* dst;
	Sample* src;

	/* io_lock, not taken: function must be called from Session::process() calltree */

	if (_noutputs == 0) {
		return;
	}

	/* the panner can be empty if there are no inputs to the 
	   route, but still outputs
	*/

	if (_panner->bypassed() || _panner->empty()) {
		deliver_output_no_pan (bufs, nbufs, nframes, offset);
		return;
	}

	if (_noutputs == 1) {

		dst = output(0)->get_buffer (nframes) + offset;

		if (gain_coeff == 0.0f) {

			/* only one output, and gain was zero, so make it silent */

			memset (dst, 0, sizeof (Sample) * nframes); 
			
		} else if (gain_coeff == 1.0f){

			/* mix all buffers into the output */

			uint32_t n;
			
			memcpy (dst, bufs[0], sizeof (Sample) * nframes);
			
			for (n = 1; n < nbufs; ++n) {
				src = bufs[n];
				
				for (nframes_t n = 0; n < nframes; ++n) {
					dst[n] += src[n];
				}
			}

			output(0)->mark_silence (false);

		} else {

			/* mix all buffers into the output, scaling them all by the gain */

			uint32_t n;

			src = bufs[0];
			
			for (nframes_t n = 0; n < nframes; ++n) {
				dst[n] = src[n] * gain_coeff;
			}	

			for (n = 1; n < nbufs; ++n) {
				src = bufs[n];
				
				for (nframes_t n = 0; n < nframes; ++n) {
					dst[n] += src[n] * gain_coeff;
				}	
			}
			
			output(0)->mark_silence (false);
		}

		return;
	}

	uint32_t o;
	vector<Port *>::iterator out;
	vector<Sample *>::iterator in;
	Panner::iterator pan;
	Sample* obufs[_noutputs];

	/* the terrible silence ... */

	/* XXX this is wasteful but i see no way to avoid it */
	
	for (out = _outputs.begin(), o = 0; out != _outputs.end(); ++out, ++o) {
		obufs[o] = (*out)->get_buffer (nframes) + offset;
		memset (obufs[o], 0, sizeof (Sample) * nframes);
		(*out)->mark_silence (false);
	}

	uint32_t n;

	for (pan = _panner->begin(), n = 0; n < nbufs; ++n) {
		Panner::iterator tmp;

		tmp = pan;
		++tmp;

		(*pan)->distribute (bufs[n], obufs, gain_coeff, nframes);

		if (tmp != _panner->end()) {
			pan = tmp;
		}
	}
}

void
IO::deliver_output (vector<Sample *>& bufs, uint32_t nbufs, nframes_t nframes, nframes_t offset)
{
	/* io_lock, not taken: function must be called from Session::process() calltree */

	if (_noutputs == 0) {
		return;
	}
	
	if (_panner->bypassed() || _panner->empty()) {
		deliver_output_no_pan (bufs, nbufs, nframes, offset);
		return;
	}


	gain_t dg;
	gain_t pangain = _gain;
	
	{
		Glib::Mutex::Lock dm (declick_lock, Glib::TRY_LOCK);
		
		if (dm.locked()) {
			dg = _desired_gain;
		} else {
			dg = _gain;
		}
	}

	if (dg != _gain) {
		apply_declick (bufs, nbufs, nframes, _gain, dg, false);
		_gain = dg;
		pangain = 1.0f;
	} 

	/* simple, non-automation panning to outputs */

	if (_session.transport_speed() > 1.5f || _session.transport_speed() < -1.5f) {
		pan (bufs, nbufs, nframes, offset, pangain * speed_quietning);
	} else {
		pan (bufs, nbufs, nframes, offset, pangain);
	}
}

void
IO::deliver_output_no_pan (vector<Sample *>& bufs, uint32_t nbufs, nframes_t nframes, nframes_t offset)
{
	/* io_lock, not taken: function must be called from Session::process() calltree */

	if (_noutputs == 0) {
		return;
	}

	gain_t dg;
	gain_t old_gain = _gain;

	if (apply_gain_automation || _ignore_gain_on_deliver) {

		/* gain has already been applied by automation code. do nothing here except
		   speed quietning.
		*/

		_gain = 1.0f;
		dg = _gain;
		
	} else {

		Glib::Mutex::Lock dm (declick_lock, Glib::TRY_LOCK);
		
		if (dm.locked()) {
			dg = _desired_gain;
		} else {
			dg = _gain;
		}
	}

	Sample* src;
	Sample* dst;
	uint32_t i;
	vector<Port*>::iterator o;
	vector<Sample*> outs;
	gain_t actual_gain;

	if (dg != _gain) {
		/* unlikely condition */
		for (o = _outputs.begin(), i = 0; o != _outputs.end(); ++o, ++i) {
			outs.push_back ((*o)->get_buffer (nframes) + offset);
		}
	}

	/* reduce nbufs to the index of the last input buffer */

	nbufs--;

	if (_session.transport_speed() > 1.5f || _session.transport_speed() < -1.5f) {
		actual_gain = _gain * speed_quietning;
	} else {
		actual_gain = _gain;
	}
	
	for (o = _outputs.begin(), i = 0; o != _outputs.end(); ++o, ++i) {

		dst = (*o)->get_buffer (nframes) + offset;
		src = bufs[min(nbufs,i)];

		if (dg != _gain || actual_gain == 1.0f) {
			memcpy (dst, src, sizeof (Sample) * nframes);
		} else if (actual_gain == 0.0f) {
			memset (dst, 0, sizeof (Sample) * nframes);
		} else {
			for (nframes_t x = 0; x < nframes; ++x) {
				dst[x] = src[x] * actual_gain;
			}
		}
		
		(*o)->mark_silence (false);
	}

	if (dg != _gain) {
		apply_declick (outs, outs.size(), nframes, _gain, dg, false);
		_gain = dg;
	}

	if (apply_gain_automation || _ignore_gain_on_deliver) {
		_gain = old_gain;
	}
}

void
IO::collect_input (vector<Sample *>& bufs, uint32_t nbufs, nframes_t nframes, nframes_t offset)
{
	/* io_lock, not taken: function must be called from Session::process() calltree */

	vector<Port *>::iterator i;
	uint32_t n;
	Sample *last = 0;
	
	/* we require that bufs.size() >= 1 */

	for (n = 0, i = _inputs.begin(); n < nbufs; ++i, ++n) {
		if (i == _inputs.end()) {
			break;
		}
		
		/* XXX always read the full extent of the port buffer that
		   we need. One day, we may use jack_port_get_buffer_at_offset()
		   or something similar. For now, this simple hack will
		   have to do.

		   Hack? Why yes .. we only need to read nframes-worth of
		   data, but the data we want is at `offset' within the
		   buffer.
		*/

		last = (*i)->get_buffer (nframes+offset) + offset;
		// the dest buffer's offset has already been applied
		memcpy (bufs[n], last, sizeof (Sample) * nframes);
	}

	/* fill any excess outputs with the last input */
	
	while (n < nbufs && last) {
		// the dest buffer's offset has already been applied
		memcpy (bufs[n], last, sizeof (Sample) * nframes);
		++n;
	}
}

void
IO::just_meter_input (nframes_t start_frame, nframes_t end_frame, 
		      nframes_t nframes, nframes_t offset)
{
	vector<Sample*>& bufs = _session.get_passthru_buffers ();
	uint32_t nbufs = n_process_buffers ();

	collect_input (bufs, nbufs, nframes, offset);

	for (uint32_t n = 0; n < nbufs; ++n) {
		_peak_power[n] = Session::compute_peak (bufs[n], nframes, _peak_power[n]);
	}
}

void
IO::drop_input_connection ()
{
	_input_connection = 0;
	input_connection_configuration_connection.disconnect();
	input_connection_connection_connection.disconnect();
	_session.set_dirty ();
}

void
IO::drop_output_connection ()
{
	_output_connection = 0;
	output_connection_configuration_connection.disconnect();
	output_connection_connection_connection.disconnect();
	_session.set_dirty ();
}

int
IO::disconnect_input (Port* our_port, string other_port, void* src)
{
	if (other_port.length() == 0 || our_port == 0) {
		return 0;
	}

	{ 
		Glib::Mutex::Lock em (_session.engine().process_lock());
		
		{
			Glib::Mutex::Lock lm (io_lock);
			
			/* check that our_port is really one of ours */
			
			if (find (_inputs.begin(), _inputs.end(), our_port) == _inputs.end()) {
				return -1;
			}
			
			/* disconnect it from the source */
			
			if (_session.engine().disconnect (other_port, our_port->name())) {
				error << string_compose(_("IO: cannot disconnect input port %1 from %2"), our_port->name(), other_port) << endmsg;
				return -1;
			}

			drop_input_connection();
		}
	}

	input_changed (ConnectionsChanged, src); /* EMIT SIGNAL */
	_session.set_dirty ();

	return 0;
}

int
IO::connect_input (Port* our_port, string other_port, void* src)
{
	if (other_port.length() == 0 || our_port == 0) {
		return 0;
	}

	{
		Glib::Mutex::Lock em(_session.engine().process_lock());
		
		{
			Glib::Mutex::Lock lm (io_lock);
			
			/* check that our_port is really one of ours */
			
			if (find (_inputs.begin(), _inputs.end(), our_port) == _inputs.end()) {
				return -1;
			}
			
			/* connect it to the source */

			if (_session.engine().connect (other_port, our_port->name())) {
				return -1;
			}
			
			drop_input_connection ();
		}
	}

	input_changed (ConnectionsChanged, src); /* EMIT SIGNAL */
	_session.set_dirty ();
	return 0;
}

int
IO::disconnect_output (Port* our_port, string other_port, void* src)
{
	if (other_port.length() == 0 || our_port == 0) {
		return 0;
	}

	{
		Glib::Mutex::Lock em(_session.engine().process_lock());
		
		{
			Glib::Mutex::Lock lm (io_lock);
			
			if (find (_outputs.begin(), _outputs.end(), our_port) == _outputs.end()) {
				return -1;
			}
			
			/* disconnect it from the destination */
			
			if (_session.engine().disconnect (our_port->name(), other_port)) {
				error << string_compose(_("IO: cannot disconnect output port %1 from %2"), our_port->name(), other_port) << endmsg;
				return -1;
			}

			drop_output_connection ();
		}
	}

	output_changed (ConnectionsChanged, src); /* EMIT SIGNAL */
	_session.set_dirty ();
	return 0;
}

int
IO::connect_output (Port* our_port, string other_port, void* src)
{
	if (other_port.length() == 0 || our_port == 0) {
		return 0;
	}

	{
		Glib::Mutex::Lock em(_session.engine().process_lock());
		
		{
			Glib::Mutex::Lock lm (io_lock);
			
			/* check that our_port is really one of ours */
			
			if (find (_outputs.begin(), _outputs.end(), our_port) == _outputs.end()) {
				return -1;
			}
			
			/* connect it to the destination */
			
			if (_session.engine().connect (our_port->name(), other_port)) {
				return -1;
			}

			drop_output_connection ();
		}
	}

	output_changed (ConnectionsChanged, src); /* EMIT SIGNAL */
	_session.set_dirty ();
	return 0;
}

int
IO::set_input (Port* other_port, void* src)
{
	/* this removes all but one ports, and connects that one port
	   to the specified source.
	*/

	if (_input_minimum > 1 || _input_minimum == 0) {
		/* sorry, you can't do this */
		return -1;
	}

	if (other_port == 0) {
		if (_input_minimum < 0) {
			return ensure_inputs (0, false, true, src);
		} else {
			return -1;
		}
	}

	if (ensure_inputs (1, true, true, src)) {
		return -1;
	}

	return connect_input (_inputs.front(), other_port->name(), src);
}

int
IO::remove_output_port (Port* port, void* src)
{
	IOChange change (NoChange);

	{
		Glib::Mutex::Lock em(_session.engine().process_lock());
		
		{
			Glib::Mutex::Lock lm (io_lock);
			
			if (_noutputs - 1 == (uint32_t) _output_minimum) {
				/* sorry, you can't do this */
				return -1;
			}
			
			for (vector<Port *>::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
				if (*i == port) {
					change = IOChange (change|ConfigurationChanged);
					if (port->connected()) {
						change = IOChange (change|ConnectionsChanged);
					} 

					_session.engine().unregister_port (*i);
					_outputs.erase (i);
					_noutputs--;
					drop_output_connection ();

					break;
				}
			}

			if (change != NoChange) {
				setup_peak_meters ();
				reset_panner ();
			}
		}
	}
	
	if (change != NoChange) {
		output_changed (change, src); /* EMIT SIGNAL */
		_session.set_dirty ();
		return 0;
	}

	return -1;
}

/** Add an output port.
 *
 * @param destination Name of input port to connect new port to.
 * @param src Source for emitted ConfigurationChanged signal.
 * @param type Data type of port.  Default value (NIL) will use this IO's default type.
 */
int
IO::add_output_port (string destination, void* src, DataType type)
{
	Port* our_port;
	char name[64];

	if (type == DataType::NIL)
		type = _default_type;

	{
		Glib::Mutex::Lock em(_session.engine().process_lock());
		
		{ 
			Glib::Mutex::Lock lm (io_lock);
			
			if (_output_maximum >= 0 && (int) _noutputs == _output_maximum) {
				return -1;
			}
		
			/* Create a new output port */
			
			// FIXME: naming scheme for differently typed ports?
			if (_output_maximum == 1) {
				snprintf (name, sizeof (name), _("%s/out"), _name.c_str());
			} else {
				snprintf (name, sizeof (name), _("%s/out %u"), _name.c_str(), find_output_port_hole());
			}
			
			if ((our_port = _session.engine().register_output_port (type, name)) == 0) {
				error << string_compose(_("IO: cannot register output port %1"), name) << endmsg;
				return -1;
			}
			
			_outputs.push_back (our_port);
			sort (_outputs.begin(), _outputs.end(), sort_ports_by_name);
			++_noutputs;
			drop_output_connection ();
			setup_peak_meters ();
			reset_panner ();
		}

		MoreOutputs (_noutputs); /* EMIT SIGNAL */
	}

	if (destination.length()) {
		if (_session.engine().connect (our_port->name(), destination)) {
			return -1;
		}
	}
	
	// pan_changed (src); /* EMIT SIGNAL */
	output_changed (ConfigurationChanged, src); /* EMIT SIGNAL */
	_session.set_dirty ();
	return 0;
}

int
IO::remove_input_port (Port* port, void* src)
{
	IOChange change (NoChange);

	{
		Glib::Mutex::Lock em(_session.engine().process_lock());
		
		{
			Glib::Mutex::Lock lm (io_lock);

			if (((int)_ninputs - 1) < _input_minimum) {
				/* sorry, you can't do this */
				return -1;
			}
			for (vector<Port *>::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {

				if (*i == port) {
					change = IOChange (change|ConfigurationChanged);

					if (port->connected()) {
						change = IOChange (change|ConnectionsChanged);
					} 

					_session.engine().unregister_port (*i);
					_inputs.erase (i);
					_ninputs--;
					drop_input_connection ();

					break;
				}
			}
			
			if (change != NoChange) {
				setup_peak_meters ();
				reset_panner ();
			}
		}
	}

	if (change != NoChange) {
		input_changed (change, src);
		_session.set_dirty ();
		return 0;
	} 

	return -1;
}


/** Add an input port.
 *
 * @param type Data type of port.  The appropriate Jack port type, and @ref Port will be created.
 * @param destination Name of input port to connect new port to.
 * @param src Source for emitted ConfigurationChanged signal.
 */
int
IO::add_input_port (string source, void* src, DataType type)
{
	Port* our_port;
	char name[64];
	
	if (type == DataType::NIL)
		type = _default_type;

	{
		Glib::Mutex::Lock em (_session.engine().process_lock());
		
		{ 
			Glib::Mutex::Lock lm (io_lock);
			
			if (_input_maximum >= 0 && (int) _ninputs == _input_maximum) {
				return -1;
			}

			/* Create a new input port */
			
			// FIXME: naming scheme for differently typed ports?
			if (_input_maximum == 1) {
				snprintf (name, sizeof (name), _("%s/in"), _name.c_str());
			} else {
				snprintf (name, sizeof (name), _("%s/in %u"), _name.c_str(), find_input_port_hole());
			}
			
			if ((our_port = _session.engine().register_input_port (type, name)) == 0) {
				error << string_compose(_("IO: cannot register input port %1"), name) << endmsg;
				return -1;
			}
			
			_inputs.push_back (our_port);
			sort (_inputs.begin(), _inputs.end(), sort_ports_by_name);
			++_ninputs;
			drop_input_connection ();
			setup_peak_meters ();
			reset_panner ();
		}

		MoreOutputs (_ninputs); /* EMIT SIGNAL */
	}

	if (source.length()) {

		if (_session.engine().connect (source, our_port->name())) {
			return -1;
		}
	} 

	// pan_changed (src); /* EMIT SIGNAL */
	input_changed (ConfigurationChanged, src); /* EMIT SIGNAL */
	_session.set_dirty ();

	return 0;
}

int
IO::disconnect_inputs (void* src)
{
	{ 
		Glib::Mutex::Lock em (_session.engine().process_lock());
		
		{
			Glib::Mutex::Lock lm (io_lock);
			
			for (vector<Port *>::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
				_session.engine().disconnect (*i);
			}

			drop_input_connection ();
		}
	}
	input_changed (ConnectionsChanged, src); /* EMIT SIGNAL */
	return 0;
}

int
IO::disconnect_outputs (void* src)
{
	{
		Glib::Mutex::Lock em (_session.engine().process_lock());
		
		{
			Glib::Mutex::Lock lm (io_lock);
			
			for (vector<Port *>::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
				_session.engine().disconnect (*i);
			}

			drop_output_connection ();
		}
	}

	output_changed (ConnectionsChanged, src); /* EMIT SIGNAL */
	_session.set_dirty ();
	return 0;
}

bool
IO::ensure_inputs_locked (uint32_t n, bool clear, void* src)
{
	Port* input_port;
	bool changed = false;
	bool reduced = false;
	
	/* remove unused ports */

	while (_ninputs > n) {
		_session.engine().unregister_port (_inputs.back());
		_inputs.pop_back();
		_ninputs--;
		reduced = true;
		changed = true;
	}
		
	/* create any necessary new ports */
		
	while (_ninputs < n) {
		
		char buf[64];
		
		/* Create a new input port (of the default type) */
		
		if (_input_maximum == 1) {
			snprintf (buf, sizeof (buf), _("%s/in"), _name.c_str());
		}
		else {
			snprintf (buf, sizeof (buf), _("%s/in %u"), _name.c_str(), find_input_port_hole());
		}
		
		try {
			
			if ((input_port = _session.engine().register_input_port (_default_type, buf)) == 0) {
				error << string_compose(_("IO: cannot register input port %1"), buf) << endmsg;
				return -1;
			}
		}

		catch (AudioEngine::PortRegistrationFailure& err) {
			setup_peak_meters ();
			reset_panner ();
			/* pass it on */
			throw err;
		}
		
		_inputs.push_back (input_port);
		sort (_inputs.begin(), _inputs.end(), sort_ports_by_name);
		++_ninputs;
		changed = true;
	}
	
	if (changed) {
		drop_input_connection ();
		setup_peak_meters ();
		reset_panner ();
		MoreOutputs (_ninputs); /* EMIT SIGNAL */
		_session.set_dirty ();
	}
	
	if (clear) {
		/* disconnect all existing ports so that we get a fresh start */
			
		for (vector<Port *>::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
			_session.engine().disconnect (*i);
		}
	}

	return changed;
}

int
IO::ensure_io (uint32_t nin, uint32_t nout, bool clear, void* src)
{
	bool in_changed = false;
	bool out_changed = false;
	bool in_reduced = false;
	bool out_reduced = false;
	bool need_pan_reset;

	if (_input_maximum >= 0) {
		nin = min (_input_maximum, (int) nin);
	}

	if (_output_maximum >= 0) {
		nout = min (_output_maximum, (int) nout);
	}

	if (nin == _ninputs && nout == _noutputs && !clear) {
		return 0;
	}

	{
		Glib::Mutex::Lock em (_session.engine().process_lock());
		Glib::Mutex::Lock lm (io_lock);

		Port* port;
		
		if (_noutputs == nout) {
			need_pan_reset = false;
		} else {
			need_pan_reset = true;
		}
		
		/* remove unused ports */
		
		while (_ninputs > nin) {
			_session.engine().unregister_port (_inputs.back());
			_inputs.pop_back();
			_ninputs--;
			in_reduced = true;
			in_changed = true;
		}
		
		while (_noutputs > nout) {
			_session.engine().unregister_port (_outputs.back());
			_outputs.pop_back();
			_noutputs--;
			out_reduced = true;
			out_changed = true;
		}
		
		/* create any necessary new ports (of the default type) */
		
		while (_ninputs < nin) {
			
			char buf[64];

			/* Create a new input port */
			
			if (_input_maximum == 1) {
				snprintf (buf, sizeof (buf), _("%s/in"), _name.c_str());
			}
			else {
				snprintf (buf, sizeof (buf), _("%s/in %u"), _name.c_str(), find_input_port_hole());
			}
			
			try {
				if ((port = _session.engine().register_input_port (_default_type, buf)) == 0) {
					error << string_compose(_("IO: cannot register input port %1"), buf) << endmsg;
					return -1;
				}
			}

			catch (AudioEngine::PortRegistrationFailure& err) {
				setup_peak_meters ();
				reset_panner ();
				/* pass it on */
				throw err;
			}
		
			_inputs.push_back (port);
			++_ninputs;
			in_changed = true;
		}

		/* create any necessary new ports */
		
		while (_noutputs < nout) {
			
			char buf[64];
			
			/* Create a new output port */
			
			if (_output_maximum == 1) {
				snprintf (buf, sizeof (buf), _("%s/out"), _name.c_str());
			} else {
				snprintf (buf, sizeof (buf), _("%s/out %u"), _name.c_str(), find_output_port_hole());
			}
			
			try { 
				if ((port = _session.engine().register_output_port (_default_type, buf)) == 0) {
					error << string_compose(_("IO: cannot register output port %1"), buf) << endmsg;
					return -1;
				}
			}
			
			catch (AudioEngine::PortRegistrationFailure& err) {
				setup_peak_meters ();
				reset_panner ();
				/* pass it on */
				throw err;
			}
		
			_outputs.push_back (port);
			++_noutputs;
			out_changed = true;
		}
		
		if (clear) {
			
			/* disconnect all existing ports so that we get a fresh start */
			
			for (vector<Port *>::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
				_session.engine().disconnect (*i);
			}
			
			for (vector<Port *>::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
				_session.engine().disconnect (*i);
			}
		}
		
		if (in_changed || out_changed) {
			setup_peak_meters ();
			reset_panner ();
		}
	}

	if (out_changed) {
		sort (_outputs.begin(), _outputs.end(), sort_ports_by_name);
		drop_output_connection ();
		output_changed (ConfigurationChanged, src); /* EMIT SIGNAL */
	}
	
	if (in_changed) {
		sort (_inputs.begin(), _inputs.end(), sort_ports_by_name);
		drop_input_connection ();
		input_changed (ConfigurationChanged, src); /* EMIT SIGNAL */
	}

	if (in_changed || out_changed) {
		MoreOutputs (max (_noutputs, _ninputs)); /* EMIT SIGNAL */
		_session.set_dirty ();
	}

	return 0;
}

int
IO::ensure_inputs (uint32_t n, bool clear, bool lockit, void* src)
{
	bool changed = false;

	if (_input_maximum >= 0) {
		n = min (_input_maximum, (int) n);
		
		if (n == _ninputs && !clear) {
			return 0;
		}
	}
	
	if (lockit) {
		Glib::Mutex::Lock em (_session.engine().process_lock());
		Glib::Mutex::Lock im (io_lock);
		changed = ensure_inputs_locked (n, clear, src);
	} else {
		changed = ensure_inputs_locked (n, clear, src);
	}

	if (changed) {
		input_changed (ConfigurationChanged, src); /* EMIT SIGNAL */
		_session.set_dirty ();
	}

	return 0;
}

bool
IO::ensure_outputs_locked (uint32_t n, bool clear, void* src)
{
	Port* output_port;
	bool changed = false;
	bool reduced = false;
	bool need_pan_reset;

	if (_noutputs == n) {
		need_pan_reset = false;
	} else {
		need_pan_reset = true;
	}
	
	/* remove unused ports */
	
	while (_noutputs > n) {
		
		_session.engine().unregister_port (_outputs.back());
		_outputs.pop_back();
		_noutputs--;
		reduced = true;
		changed = true;
	}
	
	/* create any necessary new ports */
	
	while (_noutputs < n) {
		
		char buf[64];
		
		/* Create a new output port */
		
		if (_output_maximum == 1) {
			snprintf (buf, sizeof (buf), _("%s/out"), _name.c_str());
		} else {
			snprintf (buf, sizeof (buf), _("%s/out %u"), _name.c_str(), find_output_port_hole());
		}
		
		if ((output_port = _session.engine().register_output_port (_default_type, buf)) == 0) {
			error << string_compose(_("IO: cannot register output port %1"), buf) << endmsg;
			return -1;
		}
		
		_outputs.push_back (output_port);
		sort (_outputs.begin(), _outputs.end(), sort_ports_by_name);
		++_noutputs;
		changed = true;
		setup_peak_meters ();

		if (need_pan_reset) {
			reset_panner ();
		}
	}
	
	if (changed) {
		drop_output_connection ();
		MoreOutputs (_noutputs); /* EMIT SIGNAL */
		_session.set_dirty ();
	}
	
	if (clear) {
		/* disconnect all existing ports so that we get a fresh start */
		
		for (vector<Port *>::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
			_session.engine().disconnect (*i);
		}
	}

	return changed;
}

int
IO::ensure_outputs (uint32_t n, bool clear, bool lockit, void* src)
{
	bool changed = false;

	if (_output_maximum >= 0) {
		n = min (_output_maximum, (int) n);
		if (n == _noutputs && !clear) {
			return 0;
		}
	}

	/* XXX caller should hold io_lock, but generally doesn't */

	if (lockit) {
		Glib::Mutex::Lock em (_session.engine().process_lock());
		Glib::Mutex::Lock im (io_lock);
		changed = ensure_outputs_locked (n, clear, src);
	} else {
		changed = ensure_outputs_locked (n, clear, src);
	}

	if (changed) {
		 output_changed (ConfigurationChanged, src); /* EMIT SIGNAL */
	}

	return 0;
}

gain_t
IO::effective_gain () const
{
	if (gain_automation_playback()) {
		return _effective_gain;
	} else {
		return _desired_gain;
	}
}

void
IO::reset_panner ()
{
	if (panners_legal) {
		if (!no_panner_reset) {
			_panner->reset (_noutputs, pans_required());
		}
	} else {
		panner_legal_c.disconnect ();
		panner_legal_c = PannersLegal.connect (mem_fun (*this, &IO::panners_became_legal));
	}
}

int
IO::panners_became_legal ()
{
	_panner->reset (_noutputs, pans_required());
	panner_legal_c.disconnect ();
	return 0;
}

void
IO::defer_pan_reset ()
{
	no_panner_reset = true;
}

void
IO::allow_pan_reset ()
{
	no_panner_reset = false;
	reset_panner ();
}


XMLNode&
IO::get_state (void)
{
	return state (true);
}

XMLNode&
IO::state (bool full_state)
{
	XMLNode* node = new XMLNode (state_node_name);
	char buf[64];
	string str;
	bool need_ins = true;
	bool need_outs = true;
	LocaleGuard lg (X_("POSIX"));
	Glib::Mutex::Lock lm (io_lock);

	node->add_property("name", _name);
	id().print (buf, sizeof (buf));
	node->add_property("id", buf);

	str = "";

	if (_input_connection) {
		node->add_property ("input-connection", _input_connection->name());
		need_ins = false;
	}

	if (_output_connection) {
		node->add_property ("output-connection", _output_connection->name());
		need_outs = false;
	}

	if (need_ins) {
		for (vector<Port *>::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
			
			const char **connections = (*i)->get_connections();
			
			if (connections && connections[0]) {
				str += '{';
				
				for (int n = 0; connections && connections[n]; ++n) {
					if (n) {
						str += ',';
					}
					
					/* if its a connection to our own port,
					   return only the port name, not the
					   whole thing. this allows connections
					   to be re-established even when our
					   client name is different.
					*/
					
					str += _session.engine().make_port_name_relative (connections[n]);
				}	

				str += '}';
				
				free (connections);
			}
			else {
				str += "{}";
			}
		}
		
		node->add_property ("inputs", str);
	}

	if (need_outs) {
		str = "";
		
		for (vector<Port *>::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
			
			const char **connections = (*i)->get_connections();
			
			if (connections && connections[0]) {
				
				str += '{';
				
				for (int n = 0; connections[n]; ++n) {
					if (n) {
						str += ',';
					}

					str += _session.engine().make_port_name_relative (connections[n]);
				}

				str += '}';
				
				free (connections);
			}
			else {
				str += "{}";
			}
		}
		
		node->add_property ("outputs", str);
	}

	node->add_child_nocopy (_panner->state (full_state));
	node->add_child_nocopy (_gain_control.get_state ());

	snprintf (buf, sizeof(buf), "%2.12f", gain());
	node->add_property ("gain", buf);

	snprintf (buf, sizeof(buf)-1, "%d,%d,%d,%d",
		  _input_minimum,
		  _input_maximum,
		  _output_minimum,
		  _output_maximum);

	node->add_property ("iolimits", buf);

	/* automation */

	if (full_state) {

		XMLNode* autonode = new XMLNode (X_("Automation"));
		autonode->add_child_nocopy (get_automation_state());
		node->add_child_nocopy (*autonode);

		snprintf (buf, sizeof (buf), "0x%x", (int) _gain_automation_curve.automation_state());
	} else {
		/* never store anything except Off for automation state in a template */
		snprintf (buf, sizeof (buf), "0x%x", ARDOUR::Off); 
	}

	node->add_property ("automation-state", buf);
	snprintf (buf, sizeof (buf), "0x%x", (int) _gain_automation_curve.automation_style());
	node->add_property ("automation-style", buf);

	return *node;
}



int
IO::set_state (const XMLNode& node)
{
	const XMLProperty* prop;
	XMLNodeConstIterator iter;
	LocaleGuard lg (X_("POSIX"));

	/* force use of non-localized representation of decimal point,
	   since we use it a lot in XML files and so forth.
	*/

	if (node.name() != state_node_name) {
		error << string_compose(_("incorrect XML node \"%1\" passed to IO object"), node.name()) << endmsg;
		return -1;
	}

	if ((prop = node.property ("name")) != 0) {
		_name = prop->value();
		/* used to set panner name with this, but no more */
	} 

	if ((prop = node.property ("id")) != 0) {
		_id = prop->value ();
	}

	if ((prop = node.property ("iolimits")) != 0) {
		sscanf (prop->value().c_str(), "%d,%d,%d,%d", 
			&_input_minimum,
			&_input_maximum,
			&_output_minimum,
			&_output_maximum);
	}
	
	if ((prop = node.property ("gain")) != 0) {
		set_gain (atof (prop->value().c_str()), this);
		_gain = _desired_gain;
	}

	for (iter = node.children().begin(); iter != node.children().end(); ++iter) {

		if ((*iter)->name() == "Panner") {
			_panner->set_state (**iter);
		}

		if ((*iter)->name() == X_("Automation")) {

			set_automation_state (*(*iter)->children().front());
		}

		if ((*iter)->name() == X_("gaincontrol")) {
			_gain_control.set_state (**iter);
		}
	}

	if ((prop = node.property ("automation-state")) != 0) {

		long int x;
		x = strtol (prop->value().c_str(), 0, 16);
		set_gain_automation_state (AutoState (x));
	}

	if ((prop = node.property ("automation-style")) != 0) {

	       long int x;
		x = strtol (prop->value().c_str(), 0, 16);
		set_gain_automation_style (AutoStyle (x));
	}
	
	if (ports_legal) {

		if (create_ports (node)) {
			return -1;
		}

	} else {

		port_legal_c = PortsLegal.connect (mem_fun (*this, &IO::ports_became_legal));
	}

	if (panners_legal) {
		reset_panner ();
	} else {
		panner_legal_c = PannersLegal.connect (mem_fun (*this, &IO::panners_became_legal));
	}

	if (connecting_legal) {

		if (make_connections (node)) {
			return -1;
		}

	} else {
		
		connection_legal_c = ConnectingLegal.connect (mem_fun (*this, &IO::connecting_became_legal));
	}

	if (!ports_legal || !connecting_legal) {
		pending_state_node = new XMLNode (node);
	}

	return 0;
}

int
IO::set_automation_state (const XMLNode& node)
{
	return _gain_automation_curve.set_state (node);
}

XMLNode&
IO::get_automation_state ()
{
	return (_gain_automation_curve.get_state ());
}

int
IO::connecting_became_legal ()
{
	int ret;

	if (pending_state_node == 0) {
		fatal << _("IO::connecting_became_legal() called without a pending state node") << endmsg;
		/*NOTREACHED*/
		return -1;
	}

	connection_legal_c.disconnect ();

	ret = make_connections (*pending_state_node);

	if (ports_legal) {
		delete pending_state_node;
		pending_state_node = 0;
	}

	return ret;
}
int
IO::ports_became_legal ()
{
	int ret;

	if (pending_state_node == 0) {
		fatal << _("IO::ports_became_legal() called without a pending state node") << endmsg;
		/*NOTREACHED*/
		return -1;
	}

	port_legal_c.disconnect ();

	ret = create_ports (*pending_state_node);

	if (connecting_legal) {
		delete pending_state_node;
		pending_state_node = 0;
	}

	return ret;
}

int
IO::create_ports (const XMLNode& node)
{
	const XMLProperty* prop;
	int num_inputs = 0;
	int num_outputs = 0;

	if ((prop = node.property ("input-connection")) != 0) {

		Connection* c = _session.connection_by_name (prop->value());
		
		if (c == 0) {
			error << string_compose(_("Unknown connection \"%1\" listed for input of %2"), prop->value(), _name) << endmsg;

			if ((c = _session.connection_by_name (_("in 1"))) == 0) {
				error << _("No input connections available as a replacement")
				      << endmsg;
				return -1;
			}  else {
				info << string_compose (_("Connection %1 was not available - \"in 1\" used instead"), prop->value())
				     << endmsg;
			}
		} 

		num_inputs = c->nports();

	} else if ((prop = node.property ("inputs")) != 0) {

		num_inputs = count (prop->value().begin(), prop->value().end(), '{');
	}
	
	if ((prop = node.property ("output-connection")) != 0) {
		Connection* c = _session.connection_by_name (prop->value());

		if (c == 0) {
			error << string_compose(_("Unknown connection \"%1\" listed for output of %2"), prop->value(), _name) << endmsg;

			if ((c = _session.connection_by_name (_("out 1"))) == 0) {
				error << _("No output connections available as a replacement")
				      << endmsg;
				return -1;
			}  else {
				info << string_compose (_("Connection %1 was not available - \"out 1\" used instead"), prop->value())
				     << endmsg;
			}
		} 

		num_outputs = c->nports ();
		
	} else if ((prop = node.property ("outputs")) != 0) {
		num_outputs = count (prop->value().begin(), prop->value().end(), '{');
	}

	no_panner_reset = true;

	if (ensure_io (num_inputs, num_outputs, true, this)) {
		error << string_compose(_("%1: cannot create I/O ports"), _name) << endmsg;
		return -1;
	}

	no_panner_reset = false;

	set_deferred_state ();

	PortsCreated();
	return 0;
}


int
IO::make_connections (const XMLNode& node)
{
	const XMLProperty* prop;

	if ((prop = node.property ("input-connection")) != 0) {
		Connection* c = _session.connection_by_name (prop->value());
		
		if (c == 0) {
			error << string_compose(_("Unknown connection \"%1\" listed for input of %2"), prop->value(), _name) << endmsg;

			if ((c = _session.connection_by_name (_("in 1"))) == 0) {
				error << _("No input connections available as a replacement")
				      << endmsg;
				return -1;
			} else {
				info << string_compose (_("Connection %1 was not available - \"in 1\" used instead"), prop->value())
				     << endmsg;
			}
		} 

		use_input_connection (*c, this);

	} else if ((prop = node.property ("inputs")) != 0) {
		if (set_inputs (prop->value())) {
			error << string_compose(_("improper input channel list in XML node (%1)"), prop->value()) << endmsg;
			return -1;
		}
	}
	
	if ((prop = node.property ("output-connection")) != 0) {
		Connection* c = _session.connection_by_name (prop->value());
		
		if (c == 0) {
			error << string_compose(_("Unknown connection \"%1\" listed for output of %2"), prop->value(), _name) << endmsg;

			if ((c = _session.connection_by_name (_("out 1"))) == 0) {
				error << _("No output connections available as a replacement")
				      << endmsg;
				return -1;
			}  else {
				info << string_compose (_("Connection %1 was not available - \"out 1\" used instead"), prop->value())
				     << endmsg;
			}
		} 

		use_output_connection (*c, this);
		
	} else if ((prop = node.property ("outputs")) != 0) {
		if (set_outputs (prop->value())) {
			error << string_compose(_("improper output channel list in XML node (%1)"), prop->value()) << endmsg;
			return -1;
		}
	}
	
	return 0;
}

int
IO::set_inputs (const string& str)
{
	vector<string> ports;
	int i;
	int n;
	uint32_t nports;
	
	if ((nports = count (str.begin(), str.end(), '{')) == 0) {
		return 0;
	}

	if (ensure_inputs (nports, true, true, this)) {
		return -1;
	}

	string::size_type start, end, ostart;

	ostart = 0;
	start = 0;
	end = 0;
	i = 0;

	while ((start = str.find_first_of ('{', ostart)) != string::npos) {
		start += 1;

		if ((end = str.find_first_of ('}', start)) == string::npos) {
			error << string_compose(_("IO: badly formed string in XML node for inputs \"%1\""), str) << endmsg;
			return -1;
		}

		if ((n = parse_io_string (str.substr (start, end - start), ports)) < 0) {
			error << string_compose(_("bad input string in XML node \"%1\""), str) << endmsg;

			return -1;
			
		} else if (n > 0) {

			for (int x = 0; x < n; ++x) {
				connect_input (input (i), ports[x], this);
			}
		}

		ostart = end+1;
		i++;
	}

	return 0;
}

int
IO::set_outputs (const string& str)
{
	vector<string> ports;
	int i;
	int n;
	uint32_t nports;
	
	if ((nports = count (str.begin(), str.end(), '{')) == 0) {
		return 0;
	}

	if (ensure_outputs (nports, true, true, this)) {
		return -1;
	}

	string::size_type start, end, ostart;

	ostart = 0;
	start = 0;
	end = 0;
	i = 0;

	while ((start = str.find_first_of ('{', ostart)) != string::npos) {
		start += 1;

		if ((end = str.find_first_of ('}', start)) == string::npos) {
			error << string_compose(_("IO: badly formed string in XML node for outputs \"%1\""), str) << endmsg;
			return -1;
		}

		if ((n = parse_io_string (str.substr (start, end - start), ports)) < 0) {
			error << string_compose(_("IO: bad output string in XML node \"%1\""), str) << endmsg;

			return -1;
			
		} else if (n > 0) {

			for (int x = 0; x < n; ++x) {
				connect_output (output (i), ports[x], this);
			}
		}

		ostart = end+1;
		i++;
	}

	return 0;
}

int
IO::parse_io_string (const string& str, vector<string>& ports)
{
	string::size_type pos, opos;

	if (str.length() == 0) {
		return 0;
	}

	pos = 0;
	opos = 0;

	ports.clear ();

	while ((pos = str.find_first_of (',', opos)) != string::npos) {
		ports.push_back (str.substr (opos, pos - opos));
		opos = pos + 1;
	}
	
	if (opos < str.length()) {
		ports.push_back (str.substr(opos));
	}

	return ports.size();
}

int
IO::parse_gain_string (const string& str, vector<string>& ports)
{
	string::size_type pos, opos;

	pos = 0;
	opos = 0;
	ports.clear ();

	while ((pos = str.find_first_of (',', opos)) != string::npos) {
		ports.push_back (str.substr (opos, pos - opos));
		opos = pos + 1;
	}
	
	if (opos < str.length()) {
		ports.push_back (str.substr(opos));
	}

	return ports.size();
}

int
IO::set_name (string name, void* src)
{
	if (name == _name) {
		return 0;
	}

	for (vector<Port *>::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
		string current_name = (*i)->short_name();
		current_name.replace (current_name.find (_name), _name.length(), name);
		(*i)->set_name (current_name);
	}

	for (vector<Port *>::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
		string current_name = (*i)->short_name();
		current_name.replace (current_name.find (_name), _name.length(), name);
		(*i)->set_name (current_name);
	}

	_name = name;
	 name_changed (src); /* EMIT SIGNAL */

	 return 0;
}

void
IO::set_input_minimum (int n)
{
	_input_minimum = n;
}

void
IO::set_input_maximum (int n)
{
	_input_maximum = n;
}

void
IO::set_output_minimum (int n)
{
	_output_minimum = n;
}

void
IO::set_output_maximum (int n)
{
	_output_maximum = n;
}

void
IO::set_port_latency (nframes_t nframes)
{
	Glib::Mutex::Lock lm (io_lock);

	for (vector<Port *>::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
		(*i)->set_latency (nframes);
	}
}

nframes_t
IO::output_latency () const
{
	nframes_t max_latency;
	nframes_t latency;

	max_latency = 0;

	/* io lock not taken - must be protected by other means */

	for (vector<Port *>::const_iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
		if ((latency = _session.engine().get_port_total_latency (*(*i))) > max_latency) {
			max_latency = latency;
		}
	}

	return max_latency;
}

nframes_t
IO::input_latency () const
{
	nframes_t max_latency;
	nframes_t latency;

	max_latency = 0;

	/* io lock not taken - must be protected by other means */

	for (vector<Port *>::const_iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
		if ((latency = _session.engine().get_port_total_latency (*(*i))) > max_latency) {
			max_latency = latency;
		}
	}

	return max_latency;
}

int
IO::use_input_connection (Connection& c, void* src)
{
	uint32_t limit;

	{
		Glib::Mutex::Lock lm (_session.engine().process_lock());
		Glib::Mutex::Lock lm2 (io_lock);
		
		limit = c.nports();
		
		drop_input_connection ();
		
		if (ensure_inputs (limit, false, false, src)) {
			return -1;
		}

		/* first pass: check the current state to see what's correctly
		   connected, and drop anything that we don't want.
		*/
		
		for (uint32_t n = 0; n < limit; ++n) {
			const Connection::PortList& pl = c.port_connections (n);
			
			for (Connection::PortList::const_iterator i = pl.begin(); i != pl.end(); ++i) {
				
				if (!_inputs[n]->connected_to ((*i))) {
					
					/* clear any existing connections */
					
					_session.engine().disconnect (_inputs[n]);
					
				} else if (_inputs[n]->connected() > 1) {
					
					/* OK, it is connected to the port we want,
					   but its also connected to other ports.
					   Change that situation.
					*/
					
					/* XXX could be optimized to not drop
					   the one we want.
					*/
					
					_session.engine().disconnect (_inputs[n]);
					
				}
			}
		}
		
		/* second pass: connect all requested ports where necessary */
		
		for (uint32_t n = 0; n < limit; ++n) {
			const Connection::PortList& pl = c.port_connections (n);
			
			for (Connection::PortList::const_iterator i = pl.begin(); i != pl.end(); ++i) {
				
				if (!_inputs[n]->connected_to ((*i))) {
					
					if (_session.engine().connect (*i, _inputs[n]->name())) {
						return -1;
					}
				}
				
			}
		}
		
		_input_connection = &c;
		
		input_connection_configuration_connection = c.ConfigurationChanged.connect
			(mem_fun (*this, &IO::input_connection_configuration_changed));
		input_connection_connection_connection = c.ConnectionsChanged.connect
			(mem_fun (*this, &IO::input_connection_connection_changed));
	}

	input_changed (IOChange (ConfigurationChanged|ConnectionsChanged), src); /* EMIT SIGNAL */
	return 0;
}

int
IO::use_output_connection (Connection& c, void* src)
{
	uint32_t limit;	

	{
		Glib::Mutex::Lock lm (_session.engine().process_lock());
		Glib::Mutex::Lock lm2 (io_lock);

		limit = c.nports();
			
		drop_output_connection ();

		if (ensure_outputs (limit, false, false, src)) {
			return -1;
		}

		/* first pass: check the current state to see what's correctly
		   connected, and drop anything that we don't want.
		*/
			
		for (uint32_t n = 0; n < limit; ++n) {

			const Connection::PortList& pl = c.port_connections (n);
				
			for (Connection::PortList::const_iterator i = pl.begin(); i != pl.end(); ++i) {
					
				if (!_outputs[n]->connected_to ((*i))) {

					/* clear any existing connections */

					_session.engine().disconnect (_outputs[n]);

				} else if (_outputs[n]->connected() > 1) {

					/* OK, it is connected to the port we want,
					   but its also connected to other ports.
					   Change that situation.
					*/

					/* XXX could be optimized to not drop
					   the one we want.
					*/
						
					_session.engine().disconnect (_outputs[n]);
				}
			}
		}

		/* second pass: connect all requested ports where necessary */

		for (uint32_t n = 0; n < limit; ++n) {

			const Connection::PortList& pl = c.port_connections (n);
				
			for (Connection::PortList::const_iterator i = pl.begin(); i != pl.end(); ++i) {
					
				if (!_outputs[n]->connected_to ((*i))) {
						
					if (_session.engine().connect (_outputs[n]->name(), *i)) {
						return -1;
					}
				}
			}
		}

		_output_connection = &c;

		output_connection_configuration_connection = c.ConfigurationChanged.connect
			(mem_fun (*this, &IO::output_connection_configuration_changed));
		output_connection_connection_connection = c.ConnectionsChanged.connect
			(mem_fun (*this, &IO::output_connection_connection_changed));
	}

	output_changed (IOChange (ConnectionsChanged|ConfigurationChanged), src); /* EMIT SIGNAL */

	return 0;
}

int
IO::disable_connecting ()
{
	connecting_legal = false;
	return 0;
}

int
IO::enable_connecting ()
{
	connecting_legal = true;
	return ConnectingLegal ();
}

int
IO::disable_ports ()
{
	ports_legal = false;
	return 0;
}

int
IO::enable_ports ()
{
	ports_legal = true;
	return PortsLegal ();
}

int
IO::disable_panners (void)
{
	panners_legal = false;
	return 0;
}

int
IO::reset_panners ()
{
	panners_legal = true;
	return PannersLegal ();
}

void
IO::input_connection_connection_changed (int ignored)
{
	use_input_connection (*_input_connection, this);
}

void
IO::input_connection_configuration_changed ()
{
	use_input_connection (*_input_connection, this);
}

void
IO::output_connection_connection_changed (int ignored)
{
	use_output_connection (*_output_connection, this);
}

void
IO::output_connection_configuration_changed ()
{
	use_output_connection (*_output_connection, this);
}

void
IO::GainControllable::set_value (float val)
{
	io.set_gain (direct_control_to_gain (val), this);
}

float
IO::GainControllable::get_value (void) const
{
	return direct_gain_to_control (io.effective_gain());
}

void
IO::reset_peak_meters ()
{
	uint32_t limit = max (_ninputs, _noutputs);

	for (uint32_t i = 0; i < limit; ++i) {
		_peak_power[i] = 0;
	}
}

void
IO::setup_peak_meters ()
{
	uint32_t limit = max (_ninputs, _noutputs);

	while (_peak_power.size() < limit) {
		_peak_power.push_back (0);
		_visible_peak_power.push_back (0);
	}
}

/**
    Update the peak meters.

    The meter signal lock is taken to prevent modification of the 
    Meter signal while updating the meters, taking the meter signal
    lock prior to taking the io_lock ensures that all IO will remain 
    valid while metering.
*/   
void
IO::update_meters()
{
    Glib::Mutex::Lock guard (m_meter_signal_lock);
    
    Meter();
}

void
IO::meter ()
{
	Glib::Mutex::Lock lm (io_lock); // READER: meter thread.
	uint32_t limit = max (_ninputs, _noutputs);
	
	for (uint32_t n = 0; n < limit; ++n) {

		/* XXX we should use atomic exchange here */

		/* grab peak since last read */

 		float new_peak = _peak_power[n];
		_peak_power[n] = 0;
		
		/* compute new visible value using falloff */

		if (new_peak > 0.0) {
			new_peak = coefficient_to_dB (new_peak);
		} else {
			new_peak = minus_infinity();
		}
		
		if (Config->get_meter_falloff() == 0.0f || new_peak > _visible_peak_power[n]) {
			_visible_peak_power[n] = new_peak;
		} else {
			// do falloff
			new_peak = _visible_peak_power[n] - Config->get_meter_falloff();
			_visible_peak_power[n] = max (new_peak, -INFINITY);
		}
	}
}

void
IO::clear_automation ()
{
	Glib::Mutex::Lock lm (automation_lock);
	_gain_automation_curve.clear ();
	_panner->clear_automation ();
}

void
IO::set_gain_automation_state (AutoState state)
{
	bool changed = false;

	{
		Glib::Mutex::Lock lm (automation_lock);

		if (state != _gain_automation_curve.automation_state()) {
			changed = true;
			_gain_automation_curve.set_automation_state (state);
			
			if (state != Off) {
				set_gain (_gain_automation_curve.eval (_session.transport_frame()), this);
			}
		}
	}

	if (changed) {
		_session.set_dirty ();
		gain_automation_state_changed (); /* EMIT SIGNAL */
	}
}

void
IO::set_gain_automation_style (AutoStyle style)
{
	bool changed = false;

	{
		Glib::Mutex::Lock lm (automation_lock);

		if (style != _gain_automation_curve.automation_style()) {
			changed = true;
			_gain_automation_curve.set_automation_style (style);
		}
	}

	if (changed) {
		gain_automation_style_changed (); /* EMIT SIGNAL */
	}
}
void
IO::inc_gain (gain_t factor, void *src)
{
	if (_desired_gain == 0.0f)
		set_gain (0.000001f + (0.000001f * factor), src);
	else
		set_gain (_desired_gain + (_desired_gain * factor), src);
}

void
IO::set_gain (gain_t val, void *src)
{
	// max gain at about +6dB (10.0 ^ ( 6 dB * 0.05))
	if (val>1.99526231f) val=1.99526231f;

	{
		Glib::Mutex::Lock dm (declick_lock);
		_desired_gain = val;
	}

	if (_session.transport_stopped()) {
		_effective_gain = val;
		_gain = val;
	}

	gain_changed (src);
	_gain_control.Changed (); /* EMIT SIGNAL */
	
	if (_session.transport_stopped() && src != 0 && src != this && gain_automation_recording()) {
		_gain_automation_curve.add (_session.transport_frame(), val);
		
	}

	_session.set_dirty();
}

void
IO::start_gain_touch ()
{
	_gain_automation_curve.start_touch ();
}

void
IO::end_gain_touch ()
{
	_gain_automation_curve.stop_touch ();
}

void
IO::start_pan_touch (uint32_t which)
{
	if (which < _panner->size()) {
		(*_panner)[which]->automation().start_touch();
	}
}

void
IO::end_pan_touch (uint32_t which)
{
	if (which < _panner->size()) {
		(*_panner)[which]->automation().stop_touch();
	}

}

void
IO::transport_stopped (nframes_t frame)
{
	_gain_automation_curve.reposition_for_rt_add (frame);

	if (_gain_automation_curve.automation_state() != Off) {
		
		/* the src=0 condition is a special signal to not propagate 
		   automation gain changes into the mix group when locating.
		*/

		set_gain (_gain_automation_curve.eval (frame), 0);
	}

	_panner->transport_stopped (frame);
}

int32_t
IO::find_input_port_hole ()
{
	/* CALLER MUST HOLD IO LOCK */

	uint32_t n;

	if (_inputs.empty()) {
		return 1;
	}

	for (n = 1; n < UINT_MAX; ++n) {
		char buf[jack_port_name_size()];
		vector<Port*>::iterator i;

		snprintf (buf, jack_port_name_size(), _("%s/in %u"), _name.c_str(), n);

		for (i = _inputs.begin(); i != _inputs.end(); ++i) {
			if ((*i)->short_name() == buf) {
				break;
			}
		}

		if (i == _inputs.end()) {
			break;
		}
	}
	return n;
}

int32_t
IO::find_output_port_hole ()
{
	/* CALLER MUST HOLD IO LOCK */

	uint32_t n;

	if (_outputs.empty()) {
		return 1;
	}

	for (n = 1; n < UINT_MAX; ++n) {
		char buf[jack_port_name_size()];
		vector<Port*>::iterator i;

		snprintf (buf, jack_port_name_size(), _("%s/out %u"), _name.c_str(), n);

		for (i = _outputs.begin(); i != _outputs.end(); ++i) {
			if ((*i)->short_name() == buf) {
				break;
			}
		}

		if (i == _outputs.end()) {
			break;
		}
	}
	
	return n;
}
