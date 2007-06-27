/*
    Copyright (C) 2000-2006 Paul Davis 

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

#include <fstream>
#include <algorithm>
#include <unistd.h>
#include <locale.h>
#include <errno.h>

#include <sigc++/bind.h>

#include <glibmm/thread.h>

#include <pbd/xml++.h>
#include <pbd/replace_all.h>

#include <ardour/audioengine.h>
#include <ardour/io.h>
#include <ardour/port.h>
#include <ardour/audio_port.h>
#include <ardour/midi_port.h>
#include <ardour/bundle.h>
#include <ardour/session.h>
#include <ardour/cycle_timer.h>
#include <ardour/panner.h>
#include <ardour/buffer_set.h>
#include <ardour/meter.h>
#include <ardour/amp.h>

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

#define BLOCK_PROCESS_CALLBACK() Glib::Mutex::Lock em (_session.engine().process_lock())

using namespace std;
using namespace ARDOUR;
using namespace PBD;

nframes_t    IO::_automation_interval = 0;

const string                 IO::state_node_name = "IO";
bool                         IO::connecting_legal = false;
bool                         IO::ports_legal = false;
bool                         IO::panners_legal = false;
sigc::signal<void>           IO::Meter;
sigc::signal<int>            IO::ConnectingLegal;
sigc::signal<int>            IO::PortsLegal;
sigc::signal<int>            IO::PannersLegal;
sigc::signal<void,ChanCount> IO::MoreChannels;
sigc::signal<int>            IO::PortsCreated;

Glib::StaticMutex IO::m_meter_signal_lock = GLIBMM_STATIC_MUTEX_INIT;

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


/** @param default_type The type of port that will be created by ensure_io
 * and friends if no type is explicitly requested (to avoid breakage).
 */
IO::IO (Session& s, const string& name,
	int input_min, int input_max, int output_min, int output_max,
	DataType default_type)
	: Automatable (s, name),
      _output_buffers (new BufferSet()),
	  _default_type (default_type),
	  _gain_control (X_("gaincontrol"), *this),
	  _input_minimum (ChanCount::ZERO),
	  _input_maximum (ChanCount::INFINITE),
	  _output_minimum (ChanCount::ZERO),
	  _output_maximum (ChanCount::INFINITE)
{
	_panner = new Panner (name, _session);
	_meter = new PeakMeter (_session);

	if (input_min > 0) {
		_input_minimum = ChanCount(_default_type, input_min);
	}
	if (input_max >= 0) {
		_input_maximum = ChanCount(_default_type, input_max);
	}
	if (output_min > 0) {
		_output_minimum = ChanCount(_default_type, output_min);
	}
	if (output_max >= 0) {
		_output_maximum = ChanCount(_default_type, output_max);
	}

	_gain = 1.0;
	_desired_gain = 1.0;
	_input_bundle = 0;
	_output_bundle = 0;
	pending_state_node = 0;
	no_panner_reset = false;
	_phase_invert = false;
	deferred_state = 0;

	add_automation_parameter(new AutomationList(ParamID(GainAutomation), 0.0, 2.0, 1.0));

	apply_gain_automation = false;
	
	last_automation_snapshot = 0;

	/*_gain_automation_state = Off;
	_gain_automation_style = Absolute;*/

	{
		// IO::Meter is emitted from another thread so the
		// Meter signal must be protected.
		Glib::Mutex::Lock guard (m_meter_signal_lock);
		m_meter_connection = Meter.connect (mem_fun (*this, &IO::meter));
	}
	
	// Connect to our own MoreChannels signal to connect output buffers
	IO::MoreChannels.connect (mem_fun (*this, &IO::attach_buffers));

	_session.add_controllable (&_gain_control);
}

IO::IO (Session& s, const XMLNode& node, DataType dt)
	: Automatable (s, "unnamed io"),
      _output_buffers (new BufferSet()),
	  _default_type (dt),
	  _gain_control (X_("gaincontrol"), *this)
{
	_meter = new PeakMeter (_session);

	_panner = 0;
	deferred_state = 0;
	no_panner_reset = false;
	_desired_gain = 1.0;
	_gain = 1.0;
	_input_bundle = 0;
	_output_bundle = 0;

	apply_gain_automation = false;

	set_state (node);

	{
		// IO::Meter is emitted from another thread so the
		// Meter signal must be protected.
		Glib::Mutex::Lock guard (m_meter_signal_lock);
		m_meter_connection = Meter.connect (mem_fun (*this, &IO::meter));
	}
	
	// Connect to our own MoreChannels signal to connect output buffers
	IO::MoreChannels.connect (mem_fun (*this, &IO::attach_buffers));

	_session.add_controllable (&_gain_control);
}

IO::~IO ()
{
	Glib::Mutex::Lock guard (m_meter_signal_lock);
	
	Glib::Mutex::Lock lm (io_lock);

	for (PortSet::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
		_session.engine().unregister_port (*i);
	}

	for (PortSet::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
		_session.engine().unregister_port (*i);
	}

	m_meter_connection.disconnect();

	delete _meter;
	delete _panner;
	delete _output_buffers;
}

void
IO::silence (nframes_t nframes, nframes_t offset)
{
	/* io_lock, not taken: function must be called from Session::process() calltree */

	for (PortSet::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
		i->get_buffer().silence (nframes, offset);
	}
}

/** Deliver bufs to the IO's Jack outputs.
 *
 * This function should automatically do whatever it necessary to correctly deliver bufs
 * to the outputs, eg applying gain or pan or whatever else needs to be done.
 */
void
IO::deliver_output (BufferSet& bufs, nframes_t start_frame, nframes_t end_frame, nframes_t nframes, nframes_t offset)
{
	// FIXME: type specific code doesn't actually need to be here, it will go away in time

	/* ********** AUDIO ********** */

	// Apply gain if gain automation isn't playing
	if ( ! apply_gain_automation) {
		
		gain_t dg = _gain; // desired gain

		{
			Glib::Mutex::Lock dm (declick_lock, Glib::TRY_LOCK);

			if (dm.locked()) {
				dg = _desired_gain;
			}

		}

		if (dg != _gain || dg != 1.0)
			Amp::run(bufs, nframes, _gain, dg, _phase_invert);
	}
	
	// Use the panner to distribute audio to output port buffers
	if (_panner && !_panner->empty() && !_panner->bypassed()) {
		_panner->distribute (bufs, output_buffers(), start_frame, end_frame, nframes, offset);
	} else {
		const DataType type = DataType::AUDIO;
		
		// Copy any audio 1:1 to outputs
		
		BufferSet::iterator o = output_buffers().begin(type);
		BufferSet::iterator i = bufs.begin(type);
		BufferSet::iterator prev = i;
		
		while (i != bufs.end(type) && o != output_buffers().end (type)) {
			o->read_from(*i, nframes, offset);
			prev = i;
			++i;
			++o;
		}

		/* extra outputs get a copy of the last buffer */

		while (o != output_buffers().end(type)) {
			o->read_from(*prev, nframes, offset);
			++o;
		}
	}

	/* ********** MIDI ********** */

	// No MIDI, we're done here
	if (bufs.count().n_midi() == 0) {
		return;
	}

	const DataType type = DataType::MIDI;

	// Copy any MIDI 1:1 to outputs
	assert(bufs.count().n_midi() == output_buffers().count().n_midi());
	BufferSet::iterator o = output_buffers().begin(type);
	for (BufferSet::iterator i = bufs.begin(type); i != bufs.end(type); ++i, ++o) {
		o->read_from(*i, nframes, offset);
	}
}

void
IO::collect_input (BufferSet& outs, nframes_t nframes, nframes_t offset)
{
	assert(outs.available() >= n_inputs());

	outs.set_count(n_inputs());
	
	if (outs.count() == ChanCount::ZERO)
		return;

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		
		BufferSet::iterator o = outs.begin(*t);
		for (PortSet::iterator i = _inputs.begin(*t); i != _inputs.end(*t); ++i, ++o) {
			o->read_from(i->get_buffer(), nframes, offset);
		}

	}
}

void
IO::just_meter_input (nframes_t start_frame, nframes_t end_frame, 
		      nframes_t nframes, nframes_t offset)
{
	BufferSet& bufs = _session.get_scratch_buffers (n_inputs());

	collect_input (bufs, nframes, offset);

	_meter->run(bufs, start_frame, end_frame, nframes, offset);
}

void
IO::drop_input_bundle ()
{
	_input_bundle = 0;
	input_bundle_configuration_connection.disconnect();
	input_bundle_connection_connection.disconnect();
	_session.set_dirty ();
}

void
IO::drop_output_bundle ()
{
	_output_bundle = 0;
	output_bundle_configuration_connection.disconnect();
	output_bundle_connection_connection.disconnect();
	_session.set_dirty ();
}

int
IO::disconnect_input (Port* our_port, string other_port, void* src)
{
	if (other_port.length() == 0 || our_port == 0) {
		return 0;
	}

	{ 
		BLOCK_PROCESS_CALLBACK ();
		
		{
			Glib::Mutex::Lock lm (io_lock);
			
			/* check that our_port is really one of ours */
			
			if ( ! _inputs.contains(our_port)) {
				return -1;
			}
			
			/* disconnect it from the source */
			
			if (_session.engine().disconnect (other_port, our_port->name())) {
				error << string_compose(_("IO: cannot disconnect input port %1 from %2"), our_port->name(), other_port) << endmsg;
				return -1;
			}

			drop_input_bundle ();
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
		BLOCK_PROCESS_CALLBACK ();
		
		{
			Glib::Mutex::Lock lm (io_lock);
			
			/* check that our_port is really one of ours */
			
			if ( ! _inputs.contains(our_port) ) {
				return -1;
			}
			
			/* connect it to the source */

			if (_session.engine().connect (other_port, our_port->name())) {
				return -1;
			}
			
			drop_input_bundle ();
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
		BLOCK_PROCESS_CALLBACK ();
		
		{
			Glib::Mutex::Lock lm (io_lock);
			
			/* check that our_port is really one of ours */
			
			if ( ! _outputs.contains(our_port) ) {
				return -1;
			}
			
			/* disconnect it from the destination */
			
			if (_session.engine().disconnect (our_port->name(), other_port)) {
				error << string_compose(_("IO: cannot disconnect output port %1 from %2"), our_port->name(), other_port) << endmsg;
				return -1;
			}

			drop_output_bundle ();
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
		BLOCK_PROCESS_CALLBACK ();

		
		{
			Glib::Mutex::Lock lm (io_lock);
			
			/* check that our_port is really one of ours */
			
			if ( ! _outputs.contains(our_port) ) {
				return -1;
			}
			
			/* connect it to the destination */
			
			if (_session.engine().connect (our_port->name(), other_port)) {
				return -1;
			}

			drop_output_bundle ();
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

	if (_input_minimum.n_total() > 1) {
		/* sorry, you can't do this */
		return -1;
	}

	if (other_port == 0) {
		if (_input_minimum == ChanCount::ZERO) {
			return ensure_inputs (ChanCount::ZERO, false, true, src);
		} else {
			return -1;
		}
	}

	if (ensure_inputs (ChanCount(other_port->type(), 1), true, true, src)) {
		return -1;
	}

	return connect_input (_inputs.port(0), other_port->name(), src);
}

int
IO::remove_output_port (Port* port, void* src)
{
	IOChange change (NoChange);

	{
		BLOCK_PROCESS_CALLBACK ();

		
		{
			Glib::Mutex::Lock lm (io_lock);

			if (n_outputs() <= _output_minimum) {
				/* sorry, you can't do this */
				return -1;
			}

			if (_outputs.remove(port)) {
				change = IOChange (change|ConfigurationChanged);

				if (port->connected()) {
					change = IOChange (change|ConnectionsChanged);
				} 

				_session.engine().unregister_port (*port);
				drop_output_bundle ();
				
				setup_peak_meters ();
				reset_panner ();
			}
		}
	}

	if (change != NoChange) {
		output_changed (change, src);
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
		BLOCK_PROCESS_CALLBACK ();

		
		{ 
			Glib::Mutex::Lock lm (io_lock);
			
			if (n_outputs() >= _output_maximum) {
				return -1;
			}
		
			/* Create a new output port */
			
			// FIXME: naming scheme for differently typed ports?
			if (_output_maximum.get(type) == 1) {
				snprintf (name, sizeof (name), _("%s/out"), _name.c_str());
			} else {
				snprintf (name, sizeof (name), _("%s/out %u"), _name.c_str(), find_output_port_hole());
			}
			
			if ((our_port = _session.engine().register_output_port (type, name)) == 0) {
				error << string_compose(_("IO: cannot register output port %1"), name) << endmsg;
				return -1;
			}
			
			_outputs.add (our_port);
			drop_output_bundle ();
			setup_peak_meters ();
			reset_panner ();
		}

		MoreChannels (n_outputs()); /* EMIT SIGNAL */
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
		BLOCK_PROCESS_CALLBACK ();

		
		{
			Glib::Mutex::Lock lm (io_lock);

			if (n_inputs() <= _input_minimum) {
				/* sorry, you can't do this */
				return -1;
			}

			if (_inputs.remove(port)) {
				change = IOChange (change|ConfigurationChanged);

				if (port->connected()) {
					change = IOChange (change|ConnectionsChanged);
				} 

				_session.engine().unregister_port (*port);
				drop_input_bundle ();
				
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
		BLOCK_PROCESS_CALLBACK ();
		
		{ 
			Glib::Mutex::Lock lm (io_lock);
			
			if (n_inputs() >= _input_maximum) {
				return -1;
			}

			/* Create a new input port */
			
			// FIXME: naming scheme for differently typed ports?
			if (_input_maximum.get(type) == 1) {
				snprintf (name, sizeof (name), _("%s/in"), _name.c_str());
			} else {
				snprintf (name, sizeof (name), _("%s/in %u"), _name.c_str(), find_input_port_hole());
			}
			
			if ((our_port = _session.engine().register_input_port (type, name)) == 0) {
				error << string_compose(_("IO: cannot register input port %1"), name) << endmsg;
				return -1;
			}

			_inputs.add (our_port);
			drop_input_bundle ();
			setup_peak_meters ();
			reset_panner ();
		}

		MoreChannels (n_inputs()); /* EMIT SIGNAL */
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
		BLOCK_PROCESS_CALLBACK ();
		
		{
			Glib::Mutex::Lock lm (io_lock);
			
			for (PortSet::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
				_session.engine().disconnect (*i);
			}

			drop_input_bundle ();
		}
	}
	
	input_changed (ConnectionsChanged, src); /* EMIT SIGNAL */
	
	return 0;
}

int
IO::disconnect_outputs (void* src)
{
	{
		BLOCK_PROCESS_CALLBACK ();
		
		{
			Glib::Mutex::Lock lm (io_lock);
			
			for (PortSet::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
				_session.engine().disconnect (*i);
			}

			drop_output_bundle ();
		}
	}

	output_changed (ConnectionsChanged, src); /* EMIT SIGNAL */
	_session.set_dirty ();
	
	return 0;
}

bool
IO::ensure_inputs_locked (ChanCount count, bool clear, void* src)
{
	Port* input_port = 0;
	bool  changed    = false;


	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		
		const size_t n = count.get(*t);
	
		/* remove unused ports */
		for (size_t i = n_inputs().get(*t); i > n; --i) {
			input_port = _inputs.port(*t, i-1);

			assert(input_port);
			_inputs.remove(input_port);
			_session.engine().unregister_port (*input_port);

			changed = true;
		}

		/* create any necessary new ports */
		while (n_inputs().get(*t) < n) {

			char buf[64];

			if (_input_maximum.get(*t) == 1) {
				snprintf (buf, sizeof (buf), _("%s/in"), _name.c_str());
			} else {
				snprintf (buf, sizeof (buf), _("%s/in %u"), _name.c_str(), find_input_port_hole());
			}

			try {

				if ((input_port = _session.engine().register_input_port (*t, buf)) == 0) {
					error << string_compose(_("IO: cannot register input port %1"), buf) << endmsg;
					return -1;
				}
			}

			catch (AudioEngine::PortRegistrationFailure& err) {
				setup_peak_meters ();
				reset_panner ();
				/* pass it on */
				throw AudioEngine::PortRegistrationFailure();
			}

			_inputs.add (input_port);
			changed = true;
		}
	}
	
	if (changed) {
		drop_input_bundle ();
		setup_peak_meters ();
		reset_panner ();
		MoreChannels (n_inputs()); /* EMIT SIGNAL */
		_session.set_dirty ();
	}
	
	if (clear) {
		/* disconnect all existing ports so that we get a fresh start */
		for (PortSet::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
			_session.engine().disconnect (*i);
		}
	}

	return changed;
}

/** Attach output_buffers to port buffers.
 * 
 * Connected to IO's own MoreChannels signal.
 */
void
IO::attach_buffers(ChanCount ignored)
{
	_output_buffers->attach_buffers(_outputs);
}

int
IO::ensure_io (ChanCount in, ChanCount out, bool clear, void* src)
{
	bool in_changed     = false;
	bool out_changed    = false;
	bool need_pan_reset = false;

	in = min (_input_maximum, in);

	out = min (_output_maximum, out);

	if (in == n_inputs() && out == n_outputs() && !clear) {
		return 0;
	}

	{
		BLOCK_PROCESS_CALLBACK ();
		Glib::Mutex::Lock lm (io_lock);

		Port* port;
		
		if (n_outputs() != out) {
			need_pan_reset = true;
		}
		
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {

			const size_t nin = in.get(*t);
			const size_t nout = out.get(*t);

			Port* output_port = 0;
			Port* input_port = 0;

			/* remove unused output ports */
			for (size_t i = n_outputs().get(*t); i > nout; --i) {
				output_port = _outputs.port(*t, i-1);

				assert(output_port);
				_outputs.remove(output_port);
				_session.engine().unregister_port (*output_port);

				out_changed = true;
			}

			/* remove unused input ports */
			for (size_t i = n_inputs().get(*t); i > nin; --i) {
				input_port = _inputs.port(*t, i-1);

				assert(input_port);
				_inputs.remove(input_port);
				_session.engine().unregister_port (*input_port);

				in_changed = true;
			}

			/* create any necessary new input ports */

			while (n_inputs().get(*t) < nin) {

				char buf[64];

				/* Create a new input port */

				if (_input_maximum.get(*t) == 1) {
					snprintf (buf, sizeof (buf), _("%s/in"), _name.c_str());
				} else {
					snprintf (buf, sizeof (buf), _("%s/in %u"), _name.c_str(), find_input_port_hole());
				}

				try {
					if ((port = _session.engine().register_input_port (*t, buf)) == 0) {
						error << string_compose(_("IO: cannot register input port %1"), buf) << endmsg;
						return -1;
					}
				}
				
				catch (AudioEngine::PortRegistrationFailure& err) {
					setup_peak_meters ();
					reset_panner ();
					/* pass it on */
					throw AudioEngine::PortRegistrationFailure();
				}

				_inputs.add (port);
				in_changed = true;
			}

			/* create any necessary new output ports */

			while (n_outputs().get(*t) < nout) {

				char buf[64];

				/* Create a new output port */

				if (_output_maximum.get(*t) == 1) {
					snprintf (buf, sizeof (buf), _("%s/out"), _name.c_str());
				} else {
					snprintf (buf, sizeof (buf), _("%s/out %u"), _name.c_str(), find_output_port_hole());
				}

				try { 
					if ((port = _session.engine().register_output_port (*t, buf)) == 0) {
						error << string_compose(_("IO: cannot register output port %1"), buf) << endmsg;
						return -1;
					}
				}

				catch (AudioEngine::PortRegistrationFailure& err) {
					setup_peak_meters ();
					reset_panner ();
					/* pass it on */
					throw AudioEngine::PortRegistrationFailure ();
				}

				_outputs.add (port);
				out_changed = true;
			}
		}
		
		if (clear) {
			
			/* disconnect all existing ports so that we get a fresh start */
			
			for (PortSet::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
				_session.engine().disconnect (*i);
			}
			
			for (PortSet::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
				_session.engine().disconnect (*i);
			}
		}
		
		if (in_changed || out_changed) {
			setup_peak_meters ();
			reset_panner ();
		}
	}

	if (out_changed) {
		drop_output_bundle ();
		output_changed (ConfigurationChanged, src); /* EMIT SIGNAL */
	}
	
	if (in_changed) {
		drop_input_bundle ();
		input_changed (ConfigurationChanged, src); /* EMIT SIGNAL */
	}

	if (in_changed || out_changed) {
		MoreChannels (max (n_outputs(), n_inputs())); /* EMIT SIGNAL */
		_session.set_dirty ();
	}

	return 0;
}

int
IO::ensure_inputs (ChanCount count, bool clear, bool lockit, void* src)
{
	bool changed = false;

	count = min (_input_maximum, count);

	if (count == n_inputs() && !clear) {
		return 0;
	}

	if (lockit) {
		BLOCK_PROCESS_CALLBACK ();
		Glib::Mutex::Lock im (io_lock);
		changed = ensure_inputs_locked (count, clear, src);
	} else {
		changed = ensure_inputs_locked (count, clear, src);
	}

	if (changed) {
		input_changed (ConfigurationChanged, src); /* EMIT SIGNAL */
		_session.set_dirty ();
	}
	return 0;
}

bool
IO::ensure_outputs_locked (ChanCount count, bool clear, void* src)
{
	Port* output_port    = 0;
	bool  changed        = false;
	bool  need_pan_reset = false;

	if (n_outputs() != count) {
		need_pan_reset = true;
	}
	
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {

		const size_t n = count.get(*t);

		/* remove unused ports */
		for (size_t i = n_outputs().get(*t); i > n; --i) {
			output_port = _outputs.port(*t, i-1);

			assert(output_port);
			_outputs.remove(output_port);
			_session.engine().unregister_port (*output_port);

			changed = true;
		}

		/* create any necessary new ports */
		while (n_outputs().get(*t) < n) {

			char buf[64];

			if (_output_maximum.get(*t) == 1) {
				snprintf (buf, sizeof (buf), _("%s/out"), _name.c_str());
			} else {
				snprintf (buf, sizeof (buf), _("%s/out %u"), _name.c_str(), find_output_port_hole());
			}

			if ((output_port = _session.engine().register_output_port (*t, buf)) == 0) {
				error << string_compose(_("IO: cannot register output port %1"), buf) << endmsg;
				return -1;
			}

			_outputs.add (output_port);
			changed = true;
			setup_peak_meters ();

			if (need_pan_reset) {
				reset_panner ();
			}
		}
	}
	
	if (changed) {
		drop_output_bundle ();
		MoreChannels (n_outputs()); /* EMIT SIGNAL */
		_session.set_dirty ();
	}
	
	if (clear) {
		/* disconnect all existing ports so that we get a fresh start */
		for (PortSet::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
			_session.engine().disconnect (*i);
		}
	}

	return changed;
}

int
IO::ensure_outputs (ChanCount count, bool clear, bool lockit, void* src)
{
	bool changed = false;

	if (_output_maximum < ChanCount::INFINITE) {
		count = min (_output_maximum, count);
		if (count == n_outputs() && !clear) {
			return 0;
		}
	}

	/* XXX caller should hold io_lock, but generally doesn't */

	if (lockit) {
		BLOCK_PROCESS_CALLBACK ();
		Glib::Mutex::Lock im (io_lock);
		changed = ensure_outputs_locked (count, clear, src);
	} else {
		changed = ensure_outputs_locked (count, clear, src);
	}

	if (changed) {
		 output_changed (ConfigurationChanged, src); /* EMIT SIGNAL */
	}

	return 0;
}

gain_t
IO::effective_gain () const
{
	if (gain_automation().automation_playback()) {
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
			_panner->reset (n_outputs().n_audio(), pans_required());
		}
	} else {
		panner_legal_c.disconnect ();
		panner_legal_c = PannersLegal.connect (mem_fun (*this, &IO::panners_became_legal));
	}
}

int
IO::panners_became_legal ()
{
	_panner->reset (n_outputs().n_audio(), pans_required());
	_panner->load (); // automation
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

	if (_input_bundle) {
		node->add_property ("input-connection", _input_bundle->name());
		need_ins = false;
	}

	if (_output_bundle) {
		node->add_property ("output-connection", _output_bundle->name());
		need_outs = false;
	}

	if (need_ins) {
		for (PortSet::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
			
			const char **connections = i->get_connections();
			
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
		
		for (PortSet::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
			
			const char **connections = i->get_connections();
			
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

	// FIXME: this is NOT sufficient!
	const int in_min  = (_input_minimum == ChanCount::ZERO) ? -1 : _input_minimum.get(_default_type);
	const int in_max  = (_input_maximum == ChanCount::INFINITE) ? -1 : _input_maximum.get(_default_type);
	const int out_min = (_output_minimum == ChanCount::ZERO) ? -1 : _output_minimum.get(_default_type);
	const int out_max = (_output_maximum == ChanCount::INFINITE) ? -1 : _output_maximum.get(_default_type);

	snprintf (buf, sizeof(buf)-1, "%d,%d,%d,%d", in_min, in_max, out_min, out_max);

	node->add_property ("iolimits", buf);

	/* automation */
	
	if (full_state)
		node->add_child_nocopy (get_automation_state());

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

	size_t in_min =  -1;
	size_t in_max  = -1;
	size_t out_min = -1;
	size_t out_max = -1;

	if ((prop = node.property ("iolimits")) != 0) {
		sscanf (prop->value().c_str(), "%zd,%zd,%zd,%zd",
			&in_min, &in_max, &out_min, &out_max);
		_input_minimum = ChanCount(_default_type, in_min);
		_input_maximum = ChanCount(_default_type, in_max);
		_output_minimum = ChanCount(_default_type, out_min);
		_output_maximum = ChanCount(_default_type, out_max);
	}
	
	if ((prop = node.property ("gain")) != 0) {
		set_gain (atof (prop->value().c_str()), this);
		_gain = _desired_gain;
	}

	if ((prop = node.property ("automation-state")) != 0 || (prop = node.property ("automation-style")) != 0) {
		/* old school automation handling */
	}

	for (iter = node.children().begin(); iter != node.children().end(); ++iter) {

		if ((*iter)->name() == "Panner") {
			if (_panner == 0) {
				_panner = new Panner (_name, _session);
			}
			_panner->set_state (**iter);
		}

		if ((*iter)->name() == X_("Automation")) {

			set_automation_state (*(*iter), ParamID(GainAutomation));
		}

		if ((*iter)->name() == X_("controllable")) {
			if ((prop = (*iter)->property("name")) != 0 && prop->value() == "gaincontrol") {
				_gain_control.set_state (**iter);
			}
		}
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

	last_automation_snapshot = 0;

	return 0;
}

int
IO::load_automation (string path)
{
	string fullpath;
	ifstream in;
	char line[128];
	uint32_t linecnt = 0;
	float version;
	LocaleGuard lg (X_("POSIX"));

	fullpath = _session.automation_dir();
	fullpath += path;

	in.open (fullpath.c_str());

	if (!in) {
		fullpath = _session.automation_dir();
		fullpath += _session.snap_name();
		fullpath += '-';
		fullpath += path;

		in.open (fullpath.c_str());

		if (!in) {
			error << string_compose(_("%1: cannot open automation event file \"%2\""), _name, fullpath) << endmsg;
			return -1;
		}
	}

	clear_automation ();

	while (in.getline (line, sizeof(line), '\n')) {
		char type;
		nframes_t when;
		double value;

		if (++linecnt == 1) {
			if (memcmp (line, "version", 7) == 0) {
				if (sscanf (line, "version %f", &version) != 1) {
					error << string_compose(_("badly formed version number in automation event file \"%1\""), path) << endmsg;
					return -1;
				}
			} else {
				error << string_compose(_("no version information in automation event file \"%1\""), path) << endmsg;
				return -1;
			}

			continue;
		}

		if (sscanf (line, "%c %" PRIu32 " %lf", &type, &when, &value) != 3) {
			warning << string_compose(_("badly formatted automation event record at line %1 of %2 (ignored)"), linecnt, path) << endmsg;
			continue;
		}

		switch (type) {
		case 'g':
			gain_automation().fast_simple_add (when, value);
			break;

		case 's':
			break;

		case 'm':
			break;

		case 'p':
			/* older (pre-1.0) versions of ardour used this */
			break;

		default:
			warning << _("dubious automation event found (and ignored)") << endmsg;
		}
	}

	return 0;
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

	/* XXX: we could change *-connection to *-bundle, but it seems a bit silly to
	 * break the session file format.
	 */
	if ((prop = node.property ("input-connection")) != 0) {

		Bundle* c = _session.bundle_by_name (prop->value());
		
		if (c == 0) {
			error << string_compose(_("Unknown bundle \"%1\" listed for input of %2"), prop->value(), _name) << endmsg;

			if ((c = _session.bundle_by_name (_("in 1"))) == 0) {
				error << _("No input bundles available as a replacement")
				      << endmsg;
				return -1;
			}  else {
				info << string_compose (_("Bundle %1 was not available - \"in 1\" used instead"), prop->value())
				     << endmsg;
			}
		} 

		num_inputs = c->nchannels();

	} else if ((prop = node.property ("inputs")) != 0) {

		num_inputs = count (prop->value().begin(), prop->value().end(), '{');
	}
	
	if ((prop = node.property ("output-connection")) != 0) {
		Bundle* c = _session.bundle_by_name (prop->value());

		if (c == 0) {
			error << string_compose(_("Unknown bundle \"%1\" listed for output of %2"), prop->value(), _name) << endmsg;

			if ((c = _session.bundle_by_name (_("out 1"))) == 0) {
				error << _("No output bundles available as a replacement")
				      << endmsg;
				return -1;
			}  else {
				info << string_compose (_("Bundle %1 was not available - \"out 1\" used instead"), prop->value())
				     << endmsg;
			}
		} 

		num_outputs = c->nchannels ();
		
	} else if ((prop = node.property ("outputs")) != 0) {
		num_outputs = count (prop->value().begin(), prop->value().end(), '{');
	}

	no_panner_reset = true;

	// FIXME: audio-only
	if (ensure_io (ChanCount(DataType::AUDIO, num_inputs), ChanCount(DataType::AUDIO, num_outputs), true, this)) {
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
		Bundle* c = _session.bundle_by_name (prop->value());
		
		if (c == 0) {
			error << string_compose(_("Unknown connection \"%1\" listed for input of %2"), prop->value(), _name) << endmsg;

			if ((c = _session.bundle_by_name (_("in 1"))) == 0) {
				error << _("No input connections available as a replacement")
				      << endmsg;
				return -1;
			} else {
				info << string_compose (_("Bundle %1 was not available - \"in 1\" used instead"), prop->value())
				     << endmsg;
			}
		} 

		use_input_bundle (*c, this);

	} else if ((prop = node.property ("inputs")) != 0) {
		if (set_inputs (prop->value())) {
			error << string_compose(_("improper input channel list in XML node (%1)"), prop->value()) << endmsg;
			return -1;
		}
	}
	
	if ((prop = node.property ("output-bundle")) != 0) {
		Bundle* c = _session.bundle_by_name (prop->value());
		
		if (c == 0) {
			error << string_compose(_("Unknown bundle \"%1\" listed for output of %2"), prop->value(), _name) << endmsg;

			if ((c = _session.bundle_by_name (_("out 1"))) == 0) {
				error << _("No output bundles available as a replacement")
				      << endmsg;
				return -1;
			}  else {
				info << string_compose (_("Bundle %1 was not available - \"out 1\" used instead"), prop->value())
				     << endmsg;
			}
		} 

		use_output_bundle (*c, this);
		
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

	// FIXME: audio-only
	if (ensure_inputs (ChanCount(DataType::AUDIO, nports), true, true, this)) {
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

	// FIXME: audio-only
	if (ensure_outputs (ChanCount(DataType::AUDIO, nports), true, true, this)) {
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

bool
IO::set_name (const string& str)
{
	if (str == _name) {
		return true;
	}
	
	/* replace all colons in the name. i wish we didn't have to do this */
	string name = str;

	if (replace_all (name, ":", "-")) {
		warning << _("you cannot use colons to name objects with I/O connections") << endmsg;
	}

	for (PortSet::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
		string current_name = i->short_name();
		current_name.replace (current_name.find (_name), _name.length(), name);
		i->set_name (current_name);
	}

	for (PortSet::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
		string current_name = i->short_name();
		current_name.replace (current_name.find (_name), _name.length(), name);
		i->set_name (current_name);
	}

	return SessionObject::set_name(name);
}

void
IO::set_input_minimum (ChanCount n)
{
	_input_minimum = n;
}

void
IO::set_input_maximum (ChanCount n)
{
	_input_maximum = n;
}

void
IO::set_output_minimum (ChanCount n)
{
	_output_minimum = n;
}

void
IO::set_output_maximum (ChanCount n)
{
	_output_maximum = n;
}

void
IO::set_port_latency (nframes_t nframes)
{
	Glib::Mutex::Lock lm (io_lock);

	for (PortSet::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
		i->set_latency (nframes);
	}
}

nframes_t
IO::output_latency () const
{
	nframes_t max_latency;
	nframes_t latency;

	max_latency = 0;

	/* io lock not taken - must be protected by other means */

	for (PortSet::const_iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
		if ((latency = _session.engine().get_port_total_latency (*i)) > max_latency) {
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

	for (PortSet::const_iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
		if ((latency = _session.engine().get_port_total_latency (*i)) > max_latency) {
			max_latency = latency;
		}
	}

	return max_latency;
}

int
IO::use_input_bundle (Bundle& c, void* src)
{
	uint32_t limit;

	{
		BLOCK_PROCESS_CALLBACK ();
		Glib::Mutex::Lock lm2 (io_lock);
		
		limit = c.nchannels();
		
		drop_input_bundle ();
		
		// FIXME bundles only work for audio-only
		if (ensure_inputs (ChanCount(DataType::AUDIO, limit), false, false, src)) {
			return -1;
		}

		/* first pass: check the current state to see what's correctly
		   connected, and drop anything that we don't want.
		*/
		
		for (uint32_t n = 0; n < limit; ++n) {
			const Bundle::PortList& pl = c.channel_ports (n);
			
			for (Bundle::PortList::const_iterator i = pl.begin(); i != pl.end(); ++i) {
				
				if (!_inputs.port(n)->connected_to ((*i))) {
					
					/* clear any existing connections */
					
					_session.engine().disconnect (*_inputs.port(n));
					
				} else if (_inputs.port(n)->connected() > 1) {
					
					/* OK, it is connected to the port we want,
					   but its also connected to other ports.
					   Change that situation.
					*/
					
					/* XXX could be optimized to not drop
					   the one we want.
					*/
					
					_session.engine().disconnect (*_inputs.port(n));
					
				}
			}
		}
		
		/* second pass: connect all requested ports where necessary */
		
		for (uint32_t n = 0; n < limit; ++n) {
			const Bundle::PortList& pl = c.channel_ports (n);
			
			for (Bundle::PortList::const_iterator i = pl.begin(); i != pl.end(); ++i) {
				
				if (!_inputs.port(n)->connected_to ((*i))) {
					
					if (_session.engine().connect (*i, _inputs.port(n)->name())) {
						return -1;
					}
				}
				
			}
		}
		
		_input_bundle = &c;
		
		input_bundle_configuration_connection = c.ConfigurationChanged.connect
			(mem_fun (*this, &IO::input_bundle_configuration_changed));
		input_bundle_connection_connection = c.PortsChanged.connect
			(mem_fun (*this, &IO::input_bundle_connection_changed));
	}

	input_changed (IOChange (ConfigurationChanged|ConnectionsChanged), src); /* EMIT SIGNAL */
	return 0;
}

int
IO::use_output_bundle (Bundle& c, void* src)
{
	uint32_t limit;	

	{
		BLOCK_PROCESS_CALLBACK ();
		Glib::Mutex::Lock lm2 (io_lock);

		limit = c.nchannels();
			
		drop_output_bundle ();

		// FIXME: audio-only
		if (ensure_outputs (ChanCount(DataType::AUDIO, limit), false, false, src)) {
			return -1;
		}

		/* first pass: check the current state to see what's correctly
		   connected, and drop anything that we don't want.
		*/
			
		for (uint32_t n = 0; n < limit; ++n) {

			const Bundle::PortList& pl = c.channel_ports (n);
				
			for (Bundle::PortList::const_iterator i = pl.begin(); i != pl.end(); ++i) {
					
				if (!_outputs.port(n)->connected_to ((*i))) {

					/* clear any existing connections */

					_session.engine().disconnect (*_outputs.port(n));

				} else if (_outputs.port(n)->connected() > 1) {

					/* OK, it is connected to the port we want,
					   but its also connected to other ports.
					   Change that situation.
					*/

					/* XXX could be optimized to not drop
					   the one we want.
					*/
						
					_session.engine().disconnect (*_outputs.port(n));
				}
			}
		}

		/* second pass: connect all requested ports where necessary */

		for (uint32_t n = 0; n < limit; ++n) {

			const Bundle::PortList& pl = c.channel_ports (n);
				
			for (Bundle::PortList::const_iterator i = pl.begin(); i != pl.end(); ++i) {
					
				if (!_outputs.port(n)->connected_to ((*i))) {
						
					if (_session.engine().connect (_outputs.port(n)->name(), *i)) {
						return -1;
					}
				}
			}
		}

		_output_bundle = &c;

		output_bundle_configuration_connection = c.ConfigurationChanged.connect
			(mem_fun (*this, &IO::output_bundle_configuration_changed));
		output_bundle_connection_connection = c.PortsChanged.connect
			(mem_fun (*this, &IO::output_bundle_connection_changed));
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
IO::input_bundle_connection_changed (int ignored)
{
	use_input_bundle (*_input_bundle, this);
}

void
IO::input_bundle_configuration_changed ()
{
	use_input_bundle (*_input_bundle, this);
}

void
IO::output_bundle_connection_changed (int ignored)
{
	use_output_bundle (*_output_bundle, this);
}

void
IO::output_bundle_configuration_changed ()
{
	use_output_bundle (*_output_bundle, this);
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
IO::setup_peak_meters()
{
	ChanCount max_streams = std::max(_inputs.count(), _outputs.count());
	_meter->configure_io(max_streams, max_streams);
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
    
    Meter(); /* EMIT SIGNAL */
}

void
IO::meter ()
{
	// FIXME: Ugly.  Meter should manage the lock, if it's necessary
	
	Glib::Mutex::Lock lm (io_lock); // READER: meter thread.
	_meter->meter();
}

void
IO::clear_automation ()
{
	Automatable::clear_automation (); // clears gain automation
	_panner->clear_automation ();
}

void
IO::set_parameter_automation_state (ParamID param, AutoState state)
{
	// XXX: would be nice to get rid of this special hack

	if (param.type() == GainAutomation) {

		bool changed = false;

		{ 
			Glib::Mutex::Lock lm (_automation_lock);

			ARDOUR::AutomationList& gain_auto = gain_automation();

			if (state != gain_auto.automation_state()) {
				changed = true;
				last_automation_snapshot = 0;
				gain_auto.set_automation_state (state);

				if (state != Off) {
					// FIXME: shouldn't this use Curve?
					set_gain (gain_auto.eval (_session.transport_frame()), this);
				}
			}
		}

		if (changed) {
			_session.set_dirty ();
		}

	} else {
		Automatable::set_parameter_automation_state(param, state);
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
	
	ARDOUR::AutomationList& gain_auto = gain_automation();

	if (_session.transport_stopped() && src != 0 && src != this && gain_auto.automation_write()) {
		gain_auto.add (_session.transport_frame(), val);
		
	}

	_session.set_dirty();
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
IO::automation_snapshot (nframes_t now)
{
	if (last_automation_snapshot > now || (now - last_automation_snapshot) > _automation_interval) {

		ARDOUR::AutomationList& gain_auto = gain_automation();

		if (gain_auto.automation_write()) {
			gain_auto.rt_add (now, gain());
		}
		
		_panner->snapshot (now);

		last_automation_snapshot = now;
	}
}

void
IO::transport_stopped (nframes_t frame)
{
	ARDOUR::AutomationList& gain_auto = gain_automation();

	gain_auto.reposition_for_rt_add (frame);

	if (gain_auto.automation_state() != Off) {
		
		/* the src=0 condition is a special signal to not propagate 
		   automation gain changes into the mix group when locating.
		*/

		// FIXME: shouldn't this use Curve?
		set_gain (gain_auto.eval (frame), 0);
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
		PortSet::iterator i = _inputs.begin();

		snprintf (buf, jack_port_name_size(), _("%s/in %u"), _name.c_str(), n);

		for ( ; i != _inputs.end(); ++i) {
			if (i->short_name() == buf) {
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
		PortSet::iterator i = _outputs.begin();

		snprintf (buf, jack_port_name_size(), _("%s/out %u"), _name.c_str(), n);

		for ( ; i != _outputs.end(); ++i) {
			if (i->short_name() == buf) {
				break;
			}
		}

		if (i == _outputs.end()) {
			break;
		}
	}
	
	return n;
}

AudioPort*
IO::audio_input(uint32_t n) const
{
	return dynamic_cast<AudioPort*>(input(n));
}

AudioPort*
IO::audio_output(uint32_t n) const
{
	return dynamic_cast<AudioPort*>(output(n));
}

MidiPort*
IO::midi_input(uint32_t n) const
{
	return dynamic_cast<MidiPort*>(input(n));
}

MidiPort*
IO::midi_output(uint32_t n) const
{
	return dynamic_cast<MidiPort*>(output(n));
}

void
IO::set_phase_invert (bool yn, void *src)
{
	if (_phase_invert != yn) {
		_phase_invert = yn;
		//  phase_invert_changed (src); /* EMIT SIGNAL */
	}
}

void
IO::set_denormal_protection (bool yn, void *src)
{
	if (_denormal_protection != yn) {
		_denormal_protection = yn;
		//  denormal_protection_changed (src); /* EMIT SIGNAL */
	}
}


