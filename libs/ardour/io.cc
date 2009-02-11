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

#include <glibmm.h>
#include <glibmm/thread.h>

#include <pbd/xml++.h>
#include <pbd/replace_all.h>
#include <pbd/unknown_type.h>

#include <ardour/audioengine.h>
#include <ardour/io.h>
#include <ardour/route.h>
#include <ardour/port.h>
#include <ardour/audio_port.h>
#include <ardour/midi_port.h>
#include <ardour/session.h>
#include <ardour/cycle_timer.h>
#include <ardour/panner.h>
#include <ardour/buffer_set.h>
#include <ardour/meter.h>
#include <ardour/amp.h>
#include <ardour/user_bundle.h>

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

const string                 IO::state_node_name = "IO";
bool                         IO::connecting_legal = false;
bool                         IO::ports_legal = false;
bool                         IO::panners_legal = false;
sigc::signal<void>           IO::Meter;
sigc::signal<int>            IO::ConnectingLegal;
sigc::signal<int>            IO::PortsLegal;
sigc::signal<int>            IO::PannersLegal;
sigc::signal<void,ChanCount> IO::PortCountChanged;
sigc::signal<int>            IO::PortsCreated;

Glib::StaticMutex IO::m_meter_signal_lock = GLIBMM_STATIC_MUTEX_INIT;

/* this is a default mapper of [0 .. 1.0] control values to a gain coefficient.
   others can be imagined. 
*/

#if 0
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
#endif

/** @param default_type The type of port that will be created by ensure_io
 * and friends if no type is explicitly requested (to avoid breakage).
 */
IO::IO (Session& s, const string& name,
	int input_min, int input_max, int output_min, int output_max,
	DataType default_type)
	: SessionObject(s, name),
	  AutomatableControls (s),
  	  _output_buffers (new BufferSet()),
	  _active(true),
	  _default_type (default_type),
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
	pending_state_node = 0;
	no_panner_reset = false;
	_phase_invert = false;
	deferred_state = 0;

	boost::shared_ptr<AutomationList> gl(
			new AutomationList(Evoral::Parameter(GainAutomation)));

	_gain_control = boost::shared_ptr<GainControl>( new GainControl( X_("gaincontrol"), this, Evoral::Parameter(GainAutomation), gl ));

	add_control(_gain_control);

	apply_gain_automation = false;
	
	{
		// IO::Meter is emitted from another thread so the
		// Meter signal must be protected.
		Glib::Mutex::Lock guard (m_meter_signal_lock);
		m_meter_connection = Meter.connect (mem_fun (*this, &IO::meter));
	}
	
	_session.add_controllable (_gain_control);

	setup_bundles_for_inputs_and_outputs ();
}

IO::IO (Session& s, const XMLNode& node, DataType dt)
	: SessionObject(s, "unnamed io"),
	  AutomatableControls (s),
  	  _output_buffers (new BufferSet()),
	  _active(true),
	  _default_type (dt)
{
	_meter = new PeakMeter (_session);
	_panner = 0;
	deferred_state = 0;
	no_panner_reset = false;
	_desired_gain = 1.0;
	_gain = 1.0;

	apply_gain_automation = false;
	
	boost::shared_ptr<AutomationList> gl(
			new AutomationList(Evoral::Parameter(GainAutomation)));

	_gain_control = boost::shared_ptr<GainControl>(
			new GainControl( X_("gaincontrol"), this, Evoral::Parameter(GainAutomation), gl));

	add_control(_gain_control);

	set_state (node);

	{
		// IO::Meter is emitted from another thread so the
		// Meter signal must be protected.
		Glib::Mutex::Lock guard (m_meter_signal_lock);
		m_meter_connection = Meter.connect (mem_fun (*this, &IO::meter));
	}
	
	_session.add_controllable (_gain_control);

	setup_bundles_for_inputs_and_outputs ();
}

IO::~IO ()
{
	Glib::Mutex::Lock guard (m_meter_signal_lock);
	Glib::Mutex::Lock lm (io_lock);

	BLOCK_PROCESS_CALLBACK ();

	for (PortSet::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
		_session.engine().unregister_port (*i);
	}

	for (PortSet::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
		_session.engine().unregister_port (*i);
	}

	m_meter_connection.disconnect();

	delete _meter;
	delete _panner;
}

void
IO::silence (nframes_t nframes, nframes_t offset)
{
	/* io_lock, not taken: function must be called from Session::process() calltree */

	for (PortSet::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
		i->get_buffer(nframes,offset).silence (nframes, offset);
	}
}

/** Deliver bufs to the IO's output ports
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

		if (dg != _gain || dg != 1.0) {
			Amp::run_in_place(bufs, nframes, _gain, dg, _phase_invert);
			_gain = dg;
		}
	}
	
	/* do this so that any processing that comes after deliver_outputs()
	   can use the output buffers.
	*/

	output_buffers().attach_buffers (_outputs, nframes, offset);

	// Use the panner to distribute audio to output port buffers

	if (0 && _panner && _panner->npanners() && !_panner->bypassed()) {

		/* blech .. we shouldn't be creating and tearing this down every process()
		   cycle. XXX fix me to not waste cycles and do memory allocation etc.
		*/
		
		_panner->run_out_of_place(bufs, output_buffers(), start_frame, end_frame, nframes, offset);

	} else {

		/* do a 1:1 copy of data to output ports */

		if (bufs.count().n_audio() > 0 && _outputs.count().n_audio () > 0) {
			copy_to_outputs (bufs, DataType::AUDIO, nframes, offset);
		}
		if (bufs.count().n_midi() > 0 && _outputs.count().n_midi () > 0) {
			copy_to_outputs (bufs, DataType::MIDI, nframes, offset);
		}
	}
}

void
IO::copy_to_outputs (BufferSet& bufs, DataType type, nframes_t nframes, nframes_t offset)
{
	// Copy any buffers 1:1 to outputs
	
	PortSet::iterator o = _outputs.begin(type);
	BufferSet::iterator i = bufs.begin(type);
	BufferSet::iterator prev = i;
	
	while (i != bufs.end(type) && o != _outputs.end (type)) {
		
		Buffer& port_buffer (o->get_buffer (nframes, offset));
		port_buffer.read_from (*i, nframes, offset);

		prev = i;
		++i;
		++o;
	}
	
	/* extra outputs get a copy of the last buffer */
	
	while (o != _outputs.end(type)) {
		Buffer& port_buffer (o->get_buffer (nframes, offset));
		port_buffer.read_from(*prev, nframes, offset);
		++o;
	}
}

void
IO::collect_input (BufferSet& outs, nframes_t nframes, nframes_t offset)
{
	assert(outs.available() >= n_inputs());
	
	if (n_inputs() == ChanCount::ZERO)
		return;

	outs.set_count(n_inputs());
	
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		
		BufferSet::iterator o = outs.begin(*t);
		PortSet::iterator e = _inputs.end (*t);
		for (PortSet::iterator i = _inputs.begin(*t); i != e; ++i, ++o) {
			Buffer& b (i->get_buffer (nframes,offset));
			o->read_from (b, nframes, offset);
		}

	}
}

void
IO::just_meter_input (nframes_t start_frame, nframes_t end_frame, 
		      nframes_t nframes, nframes_t offset)
{
	BufferSet& bufs = _session.get_scratch_buffers (n_inputs());

	collect_input (bufs, nframes, offset);

	_meter->run_in_place(bufs, start_frame, end_frame, nframes, offset);
}


void
IO::check_bundles_connected_to_inputs ()
{
	check_bundles (_bundles_connected_to_inputs, inputs());
}

void
IO::check_bundles_connected_to_outputs ()
{
	check_bundles (_bundles_connected_to_outputs, outputs());
}

void
IO::check_bundles (std::vector<UserBundleInfo>& list, const PortSet& ports)
{
	std::vector<UserBundleInfo> new_list;
	
	for (std::vector<UserBundleInfo>::iterator i = list.begin(); i != list.end(); ++i) {

		uint32_t const N = i->bundle->nchannels ();

		if (ports.num_ports (default_type()) < N) {
			continue;
		}

		bool ok = true;

		for (uint32_t j = 0; j < N; ++j) {
			/* Every port on bundle channel j must be connected to our input j */
			Bundle::PortList const pl = i->bundle->channel_ports (j);
			for (uint32_t k = 0; k < pl.size(); ++k) {
				if (ports.port(j)->connected_to (pl[k]) == false) {
					ok = false;
					break;
				}
			}

			if (ok == false) {
				break;
			}
		}

		if (ok) {
			new_list.push_back (*i);
		} else {
			i->changed.disconnect ();
		}
	}

	list = new_list;
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
			
			if (our_port->disconnect (other_port)) {
				error << string_compose(_("IO: cannot disconnect input port %1 from %2"), our_port->name(), other_port) << endmsg;
				return -1;
			}

			check_bundles_connected_to_inputs ();
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

			if (our_port->connect (other_port)) {
				return -1;
			}
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
			
			if (our_port->disconnect (other_port)) {
				error << string_compose(_("IO: cannot disconnect output port %1 from %2"), our_port->name(), other_port) << endmsg;
				return -1;
			}

			check_bundles_connected_to_outputs ();
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
			
			if (our_port->connect (other_port)) {
				return -1;
			}
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
				check_bundles_connected_to_outputs ();
				
				setup_peak_meters ();
				reset_panner ();
			}
		}

		PortCountChanged (n_outputs()); /* EMIT SIGNAL */
	}

	if (change == ConfigurationChanged) {
		setup_bundle_for_outputs ();
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
			
			string portname = build_legal_port_name (type, false);
			
			if ((our_port = _session.engine().register_output_port (type, portname)) == 0) {
				error << string_compose(_("IO: cannot register output port %1"), portname) << endmsg;
				return -1;
			}
			
			_outputs.add (our_port);
			setup_peak_meters ();
			reset_panner ();
		}

		PortCountChanged (n_outputs()); /* EMIT SIGNAL */
	}

	if (destination.length()) {
		if (our_port->connect (destination)) {
			return -1;
		}
	}
	
	// pan_changed (src); /* EMIT SIGNAL */
	output_changed (ConfigurationChanged, src); /* EMIT SIGNAL */
	setup_bundle_for_outputs ();
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
				check_bundles_connected_to_inputs ();
				
				setup_peak_meters ();
				reset_panner ();
			}
		}
		
		PortCountChanged (n_inputs ()); /* EMIT SIGNAL */
	}

	if (change == ConfigurationChanged) {
		setup_bundle_for_inputs ();
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
 * @param type Data type of port.  The appropriate port type, and @ref Port will be created.
 * @param destination Name of input port to connect new port to.
 * @param src Source for emitted ConfigurationChanged signal.
 */
int
IO::add_input_port (string source, void* src, DataType type)
{
	Port* our_port;
	
	if (type == DataType::NIL)
		type = _default_type;

	{
		BLOCK_PROCESS_CALLBACK ();
		
		{ 
			Glib::Mutex::Lock lm (io_lock);

			if (_input_maximum.get(type) >= 0 && n_inputs().get (type) >= _input_maximum.get (type)) {
				return -1;
			}

			/* Create a new input port */
			
			string portname = build_legal_port_name (type, true);

			if ((our_port = _session.engine().register_input_port (type, portname)) == 0) {
				error << string_compose(_("IO: cannot register input port %1"), portname) << endmsg;
				return -1;
			}

			_inputs.add (our_port);
			setup_peak_meters ();
			reset_panner ();
		}

		PortCountChanged (n_inputs()); /* EMIT SIGNAL */
	}

	if (source.length()) {

		if (our_port->connect (source)) {
			return -1;
		}
	} 

	// pan_changed (src); /* EMIT SIGNAL */
	input_changed (ConfigurationChanged, src); /* EMIT SIGNAL */
	setup_bundle_for_inputs ();
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
				i->disconnect_all ();
			}

			check_bundles_connected_to_inputs ();
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
				i->disconnect_all ();
			}

			check_bundles_connected_to_outputs ();
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

			string portname = build_legal_port_name (*t, true);

			try {

				if ((input_port = _session.engine().register_input_port (*t, portname)) == 0) {
					error << string_compose(_("IO: cannot register input port %1"), portname) << endmsg;
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
		check_bundles_connected_to_inputs ();
		setup_peak_meters ();
		reset_panner ();
		PortCountChanged (n_inputs()); /* EMIT SIGNAL */
		_session.set_dirty ();
	}
	
	if (clear) {
		/* disconnect all existing ports so that we get a fresh start */
		for (PortSet::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
			i->disconnect_all ();
		}
	}

	return changed;
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

				string portname = build_legal_port_name (*t, true);

				try {
					if ((port = _session.engine().register_input_port (*t, portname)) == 0) {
						error << string_compose(_("IO: cannot register input port %1"), portname) << endmsg;
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

				string portname = build_legal_port_name (*t, false);
				
				try { 
					if ((port = _session.engine().register_output_port (*t, portname)) == 0) {
						error << string_compose(_("IO: cannot register output port %1"), portname) << endmsg;
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
				i->disconnect_all ();
			}
			
			for (PortSet::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
				i->disconnect_all ();
			}
		}
		
		if (in_changed || out_changed) {
			setup_peak_meters ();
			reset_panner ();
		}
	}

	if (out_changed) {
		check_bundles_connected_to_outputs ();
		output_changed (ConfigurationChanged, src); /* EMIT SIGNAL */
		setup_bundle_for_outputs ();
	}
	
	if (in_changed) {
		check_bundles_connected_to_inputs ();
		input_changed (ConfigurationChanged, src); /* EMIT SIGNAL */
		setup_bundle_for_inputs ();
	}

	if (in_changed || out_changed) {
		PortCountChanged (max (n_outputs(), n_inputs())); /* EMIT SIGNAL */
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
		setup_bundle_for_inputs ();
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

			string portname = build_legal_port_name (*t, false);

			if ((output_port = _session.engine().register_output_port (*t, portname)) == 0) {
				error << string_compose(_("IO: cannot register output port %1"), portname) << endmsg;
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
		check_bundles_connected_to_outputs ();
		PortCountChanged (n_outputs()); /* EMIT SIGNAL */
		_session.set_dirty ();
	}
	
	if (clear) {
		/* disconnect all existing ports so that we get a fresh start */
		for (PortSet::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
			i->disconnect_all ();
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
		 setup_bundle_for_outputs ();
	}

	return 0;
}

gain_t
IO::effective_gain () const
{
	if (_gain_control->automation_playback()) {
		return _gain_control->get_value();
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
	vector<string>::iterator ci;
	int n;
	LocaleGuard lg (X_("POSIX"));
	Glib::Mutex::Lock lm (io_lock);

	node->add_property("name", _name);
	id().print (buf, sizeof (buf));
	node->add_property("id", buf);

	for (
	  std::vector<UserBundleInfo>::iterator i = _bundles_connected_to_inputs.begin();
	  i != _bundles_connected_to_inputs.end();
	  ++i
	  )
	{
		XMLNode* n = new XMLNode ("InputBundle");
		n->add_property ("name", i->bundle->name ());
		node->add_child_nocopy (*n);
	}

	for (
	  std::vector<UserBundleInfo>::iterator i = _bundles_connected_to_outputs.begin();
	  i != _bundles_connected_to_outputs.end();
	  ++i
	  )
	{
		XMLNode* n = new XMLNode ("OutputBundle");
		n->add_property ("name", i->bundle->name ());
		node->add_child_nocopy (*n);
	}
	
	str = "";

	for (PortSet::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
			
		vector<string> connections;

		if (i->get_connections (connections)) {

			str += '{';
			
			for (n = 0, ci = connections.begin(); ci != connections.end(); ++ci, ++n) {
				if (n) {
					str += ',';
				}
				
				/* if its a connection to our own port,
				   return only the port name, not the
				   whole thing. this allows connections
				   to be re-established even when our
				   client name is different.
				*/
				
				str += _session.engine().make_port_name_relative (*ci);
			}	
			
			str += '}';

		} else {
			str += "{}";
		}
	}
	
	node->add_property ("inputs", str);

	str = "";
	
	for (PortSet::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
		
		vector<string> connections;

		if (i->get_connections (connections)) {
			
			str += '{';
			
			for (n = 0, ci = connections.begin(); ci != connections.end(); ++ci, ++n) {
				if (n) {
					str += ',';
				}
				
				str += _session.engine().make_port_name_relative (*ci);
			}
			
			str += '}';

		} else {
			str += "{}";
		}
	}
	
	node->add_property ("outputs", str);

	node->add_child_nocopy (_panner->state (full_state));
	node->add_child_nocopy (_gain_control->get_state ());

	snprintf (buf, sizeof(buf), "%2.12f", gain());
	node->add_property ("gain", buf);

	/* To make backwards compatibility a bit easier, write ChanCount::INFINITE to the session file
	   as -1.
	*/

	int const in_max = _input_maximum == ChanCount::INFINITE ? -1 : _input_maximum.get(_default_type);
	int const out_max = _output_maximum == ChanCount::INFINITE ? -1 : _output_maximum.get(_default_type);

	snprintf (buf, sizeof(buf)-1, "%d,%d,%d,%d", _input_minimum.get(_default_type), in_max, _output_minimum.get(_default_type), out_max);

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

	int in_min = -1;
	int in_max = -1;
	int out_min = -1;
	int out_max = -1;

	if ((prop = node.property ("iolimits")) != 0) {
		sscanf (prop->value().c_str(), "%d,%d,%d,%d",
			&in_min, &in_max, &out_min, &out_max);

		/* Correct for the difference between the way we write things to session files and the
		   way things are described by ChanCount; see comments in io.h about what the different
		   ChanCount values mean. */

		if (in_min < 0) {
			_input_minimum = ChanCount::ZERO;
		} else {
			_input_minimum = ChanCount (_default_type, in_min);
		}

		if (in_max < 0) {
			_input_maximum = ChanCount::INFINITE;
		} else {
			_input_maximum = ChanCount (_default_type, in_max);
		}

		if (out_min < 0) {
			_output_minimum = ChanCount::ZERO;
		} else {
			_output_minimum = ChanCount (_default_type, out_min);
		}
		
		if (out_max < 0) {
			_output_maximum = ChanCount::INFINITE;
		} else {
			_output_maximum = ChanCount (_default_type, out_max);
		}
	}
	
	if ((prop = node.property ("gain")) != 0) {
		set_gain (atof (prop->value().c_str()), this);
		_gain = _desired_gain;
	}

	if ((prop = node.property ("automation-state")) != 0 || (prop = node.property ("automation-style")) != 0) {
		/* old school automation handling */
	}

	for (iter = node.children().begin(); iter != node.children().end(); ++iter) {

		// Old school Panner.
		if ((*iter)->name() == "Panner") {
			if (_panner == 0) {
				_panner = new Panner (_name, _session);
			}
			_panner->set_state (**iter);
		}

		if ((*iter)->name() == "Processor") {
			if ((*iter)->property ("type") && ((*iter)->property ("type")->value() == "panner" ) ) {
				if (_panner == 0) {
					_panner = new Panner (_name, _session);
				}
				_panner->set_state (**iter);
			}
		}

		if ((*iter)->name() == X_("Automation")) {

			set_automation_state (*(*iter), Evoral::Parameter(GainAutomation));
		}

		if ((*iter)->name() == X_("controllable")) {
			if ((prop = (*iter)->property("name")) != 0 && prop->value() == "gaincontrol") {
				_gain_control->set_state (**iter);
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

	if( !_panner )
	    _panner = new Panner( _name, _session );
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
IO::load_automation (string path)
{
	string fullpath;
	ifstream in;
	char line[128];
	uint32_t linecnt = 0;
	float version;
	LocaleGuard lg (X_("POSIX"));

	fullpath = Glib::build_filename(_session.automation_dir(), path);

	in.open (fullpath.c_str());

	if (!in) {
		fullpath = Glib::build_filename(_session.automation_dir(), _session.snap_name() + '-' + path);

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
			_gain_control->list()->fast_simple_add (when, value);
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

boost::shared_ptr<Bundle>
IO::find_possible_bundle (const string &desired_name, const string &default_name, const string &bundle_type_name) 
{
	static const string digits = "0123456789";

	boost::shared_ptr<Bundle> c = _session.bundle_by_name (desired_name);

	if (!c) {
		int bundle_number, mask;
		string possible_name;
		bool stereo = false;
		string::size_type last_non_digit_pos;

		error << string_compose(_("Unknown bundle \"%1\" listed for %2 of %3"), desired_name, bundle_type_name, _name)
		      << endmsg;

		// find numeric suffix of desired name
		bundle_number = 0;
		
		last_non_digit_pos = desired_name.find_last_not_of(digits);

		if (last_non_digit_pos != string::npos) {
			stringstream s;
			s << desired_name.substr(last_non_digit_pos);
			s >> bundle_number;
		}
	
		// see if it's a stereo connection e.g. "in 3+4"

		if (last_non_digit_pos > 1 && desired_name[last_non_digit_pos] == '+') {
			int left_bundle_number = 0;
			string::size_type left_last_non_digit_pos;

			left_last_non_digit_pos = desired_name.find_last_not_of(digits, last_non_digit_pos-1);

			if (left_last_non_digit_pos != string::npos) {
				stringstream s;
				s << desired_name.substr(left_last_non_digit_pos, last_non_digit_pos-1);
				s >> left_bundle_number;

				if (left_bundle_number > 0 && left_bundle_number + 1 == bundle_number) {
					bundle_number--;
					stereo = true;
				}
			}
		}

		// make 0-based
		if (bundle_number)
			bundle_number--;

		// find highest set bit
		mask = 1;
		while ((mask <= bundle_number) && (mask <<= 1));
		
		// "wrap" bundle number into largest possible power of 2 
		// that works...

		while (mask) {

			if (bundle_number & mask) {
				bundle_number &= ~mask;
				
				stringstream s;
				s << default_name << " " << bundle_number + 1;

				if (stereo) {
					s << "+" << bundle_number + 2;
				}
				
				possible_name = s.str();

				if ((c = _session.bundle_by_name (possible_name)) != 0) {
					break;
				}
			}
			mask >>= 1;
		}
		if (c) {
			info << string_compose (_("Bundle %1 was not available - \"%2\" used instead"), desired_name, possible_name)
			     << endmsg;
		} else {
			error << string_compose(_("No %1 bundles available as a replacement"), bundle_type_name)
			      << endmsg;
		}

	}

	return c;

}

int
IO::create_ports (const XMLNode& node)
{
	XMLProperty const * prop;
	uint32_t num_inputs = 0;
	uint32_t num_outputs = 0;

	if ((prop = node.property ("input-connection")) != 0) {

		boost::shared_ptr<Bundle> c = find_possible_bundle (prop->value(), _("in"), _("input"));
		
		if (!c) {
			return -1;
		} 

		num_inputs = c->nchannels();

	} else if ((prop = node.property ("inputs")) != 0) {

		num_inputs = count (prop->value().begin(), prop->value().end(), '{');
	}
	
	if ((prop = node.property ("output-connection")) != 0) {

		boost::shared_ptr<Bundle> c = find_possible_bundle(prop->value(), _("out"), _("output"));

		if (!c) {
			return -1;
		} 

		num_outputs = c->nchannels ();
		
	} else if ((prop = node.property ("outputs")) != 0) {

		num_outputs = count (prop->value().begin(), prop->value().end(), '{');
	}

	no_panner_reset = true;

	if (ensure_io (ChanCount (_default_type, num_inputs),
		       ChanCount (_default_type, num_outputs),
		       true, this)) {
		
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
		boost::shared_ptr<Bundle> c = find_possible_bundle (prop->value(), _("in"), _("input"));
		
		if (!c) {
			return -1;
		} 

		connect_input_ports_to_bundle (c, this);

	} else if ((prop = node.property ("inputs")) != 0) {
		if (set_inputs (prop->value())) {
			error << string_compose(_("improper input channel list in XML node (%1)"), prop->value()) << endmsg;
			return -1;
		}
	}

	if ((prop = node.property ("output-connection")) != 0) {
		boost::shared_ptr<Bundle> c = find_possible_bundle (prop->value(), _("out"), _("output"));
		
		if (!c) {
			return -1;
		} 
		
		connect_output_ports_to_bundle (c, this);
		
	} else if ((prop = node.property ("outputs")) != 0) {
		if (set_outputs (prop->value())) {
			error << string_compose(_("improper output channel list in XML node (%1)"), prop->value()) << endmsg;
			return -1;
		}
	}

	for (XMLNodeConstIterator i = node.children().begin(); i != node.children().end(); ++i) {

		if ((*i)->name() == "InputBundle") {
			XMLProperty const * prop = (*i)->property ("name");
			if (prop) {
				boost::shared_ptr<Bundle> b = find_possible_bundle (prop->value(), _("in"), _("input"));
				if (b) {
					connect_input_ports_to_bundle (b, this);
				}
			}
			
		} else if ((*i)->name() == "OutputBundle") {
			XMLProperty const * prop = (*i)->property ("name");
			if (prop) {
				boost::shared_ptr<Bundle> b = find_possible_bundle (prop->value(), _("out"), _("output"));
				if (b) {
					connect_output_ports_to_bundle (b, this);
				} 
			}
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
IO::set_name (const string& requested_name)
{
	if (requested_name == _name) {
		return true;
	}
	
	string name;
	Route *rt;
	if ( (rt = dynamic_cast<Route *>(this))) {
		name = Route::ensure_track_or_route_name(requested_name, _session);
	} else {
		name = requested_name;
	}


	/* replace all colons in the name. i wish we didn't have to do this */

	if (replace_all (name, ":", "-")) {
		warning << _("you cannot use colons to name objects with I/O connections") << endmsg;
	}

	for (PortSet::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
		string current_name = i->name();
		current_name.replace (current_name.find (_name), _name.length(), name);
		i->set_name (current_name);
	}

	for (PortSet::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
		string current_name = i->name();
		current_name.replace (current_name.find (_name), _name.length(), name);
		i->set_name (current_name);
	}

	bool const r = SessionObject::set_name(name);

	setup_bundles_for_inputs_and_outputs ();

	return r;
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
		if ((latency = i->total_latency ()) > max_latency) {
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
		if ((latency = i->total_latency ()) > max_latency) {
			max_latency = latency;
		} 
	}

	return max_latency;
}

int
IO::connect_input_ports_to_bundle (boost::shared_ptr<Bundle> c, void* src)
{
	{
		BLOCK_PROCESS_CALLBACK ();
		Glib::Mutex::Lock lm2 (io_lock);

		c->connect (_bundle_for_inputs, _session.engine());

		/* If this is a UserBundle, make a note of what we've done */

		boost::shared_ptr<UserBundle> ub = boost::dynamic_pointer_cast<UserBundle> (c);
		if (ub) {

			/* See if we already know about this one */
			std::vector<UserBundleInfo>::iterator i = _bundles_connected_to_inputs.begin();
			while (i != _bundles_connected_to_inputs.end() && i->bundle != ub) {
				++i;
			}

			if (i == _bundles_connected_to_inputs.end()) {
				/* We don't, so make a note */
				_bundles_connected_to_inputs.push_back (UserBundleInfo (this, ub));
			}
		}
	}

	input_changed (IOChange (ConfigurationChanged|ConnectionsChanged), src); /* EMIT SIGNAL */
	return 0;
}

int
IO::disconnect_input_ports_from_bundle (boost::shared_ptr<Bundle> c, void* src)
{
	{
		BLOCK_PROCESS_CALLBACK ();
		Glib::Mutex::Lock lm2 (io_lock);

		c->disconnect (_bundle_for_inputs, _session.engine());
			
		/* If this is a UserBundle, make a note of what we've done */

		boost::shared_ptr<UserBundle> ub = boost::dynamic_pointer_cast<UserBundle> (c);
		if (ub) {

			std::vector<UserBundleInfo>::iterator i = _bundles_connected_to_inputs.begin();
			while (i != _bundles_connected_to_inputs.end() && i->bundle != ub) {
				++i;
			}

			if (i != _bundles_connected_to_inputs.end()) {
				_bundles_connected_to_inputs.erase (i);
			}
		}
	}

	input_changed (IOChange (ConfigurationChanged|ConnectionsChanged), src); /* EMIT SIGNAL */
	return 0;
}

int
IO::connect_output_ports_to_bundle (boost::shared_ptr<Bundle> c, void* src)
{
	{
		BLOCK_PROCESS_CALLBACK ();
		Glib::Mutex::Lock lm2 (io_lock);

		c->connect (_bundle_for_outputs, _session.engine());

		/* If this is a UserBundle, make a note of what we've done */

		boost::shared_ptr<UserBundle> ub = boost::dynamic_pointer_cast<UserBundle> (c);
		if (ub) {

			/* See if we already know about this one */
			std::vector<UserBundleInfo>::iterator i = _bundles_connected_to_outputs.begin();
			while (i != _bundles_connected_to_outputs.end() && i->bundle != ub) {
				++i;
			}

			if (i == _bundles_connected_to_outputs.end()) {
				/* We don't, so make a note */
				_bundles_connected_to_outputs.push_back (UserBundleInfo (this, ub));
			}
		}
	}

	output_changed (IOChange (ConnectionsChanged|ConfigurationChanged), src); /* EMIT SIGNAL */

	return 0;
}

int
IO::disconnect_output_ports_from_bundle (boost::shared_ptr<Bundle> c, void* src)
{
	{
		BLOCK_PROCESS_CALLBACK ();
		Glib::Mutex::Lock lm2 (io_lock);

		c->disconnect (_bundle_for_outputs, _session.engine());
			
		/* If this is a UserBundle, make a note of what we've done */

		boost::shared_ptr<UserBundle> ub = boost::dynamic_pointer_cast<UserBundle> (c);
		if (ub) {

			std::vector<UserBundleInfo>::iterator i = _bundles_connected_to_outputs.begin();
			while (i != _bundles_connected_to_outputs.end() && i->bundle != ub) {
				++i;
			}

			if (i != _bundles_connected_to_outputs.end()) {
				_bundles_connected_to_outputs.erase (i);
			}
		}
	}

	output_changed (IOChange (ConfigurationChanged|ConnectionsChanged), src); /* EMIT SIGNAL */
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
IO::bundle_changed (Bundle::Change c)
{
	//XXX
//	connect_input_ports_to_bundle (_input_bundle, this);
}

void
IO::GainControl::set_value (float val)
{
	// max gain at about +6dB (10.0 ^ ( 6 dB * 0.05))
	if (val > 1.99526231f)
		val = 1.99526231f;

	_io->set_gain (val, this);
	
	AutomationControl::set_value(val);
}

float
IO::GainControl::get_value (void) const
{
	return AutomationControl::get_value();
}

void
IO::setup_peak_meters()
{
	ChanCount max_streams = std::max (_inputs.count(), _outputs.count());
	_meter->configure_io (max_streams, max_streams);
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
	data().clear (); // clears gain automation
	_panner->data().clear();
}

void
IO::set_parameter_automation_state (Evoral::Parameter param, AutoState state)
{
	// XXX: would be nice to get rid of this special hack

	if (param.type() == GainAutomation) {

		bool changed = false;

		{ 
			Glib::Mutex::Lock lm (control_lock());

			boost::shared_ptr<AutomationList> gain_auto
				= boost::dynamic_pointer_cast<AutomationList>(_gain_control->list());

			if (state != gain_auto->automation_state()) {
				changed = true;
				_last_automation_snapshot = 0;
				gain_auto->set_automation_state (state);

				if (state != Off) {
					// FIXME: shouldn't this use Curve?
					set_gain (gain_auto->eval (_session.transport_frame()), this);
				}
			}
		}

		if (changed) {
			_session.set_dirty ();
		}

	} else {
		AutomatableControls::set_parameter_automation_state(param, state);
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
	if (val > 1.99526231f) {
		val = 1.99526231f;
	}

	cerr << "set desired gain to " << val << " when curgain = " << _gain_control->get_value () << endl;

	if (src != _gain_control.get()) {
		_gain_control->set_value(val);
		// bit twisty, this will come back and call us again
		// (this keeps control in sync with reality)
		return;
	}

	{
		Glib::Mutex::Lock dm (declick_lock);
		_desired_gain = val;
	}

	if (_session.transport_stopped()) {
		// _gain = val;
	}
	
	/*
	if (_session.transport_stopped() && src != 0 && src != this && _gain_control->automation_write()) {
		_gain_control->list()->add (_session.transport_frame(), val);
		
	}
	*/

	_session.set_dirty();
}

void
IO::start_pan_touch (uint32_t which)
{
	if (which < _panner->npanners()) {
		(*_panner).pan_control(which)->start_touch();
	}
}

void
IO::end_pan_touch (uint32_t which)
{
	if (which < _panner->npanners()) {
		(*_panner).pan_control(which)->stop_touch();
	}

}

void
IO::automation_snapshot (nframes_t now, bool force)
{
	AutomatableControls::automation_snapshot (now, force);
	// XXX: This seems to be wrong. 
	// drobilla: shouldnt automation_snapshot for panner be called
	//           "automagically" because its an Automatable now ?
	//
	//           we could dump this whole method then. <3

	if (_last_automation_snapshot > now || (now - _last_automation_snapshot) > _automation_interval) {
		_panner->automation_snapshot (now, force);
	}
	
	_panner->automation_snapshot (now, force);
	_last_automation_snapshot = now;
}

void
IO::transport_stopped (nframes_t frame)
{
	_gain_control->list()->reposition_for_rt_add (frame);

	if (_gain_control->automation_state() != Off) {
		
		/* the src=0 condition is a special signal to not propagate 
		   automation gain changes into the mix group when locating.
		*/

		// FIXME: shouldn't this use Curve?
		set_gain (_gain_control->list()->eval (frame), 0);
	}

	_panner->transport_stopped (frame);
}

string
IO::build_legal_port_name (DataType type, bool in)
{
	const int name_size = jack_port_name_size();
	int limit;
	string suffix;
	int maxports;

	if (type == DataType::AUDIO) {
		suffix = _("audio");
	} else if (type == DataType::MIDI) {
		suffix = _("midi");
	} else {
		throw unknown_type();
	}
	
	if (in) {
		suffix += _("_in");
		maxports = _input_maximum.get(type);
	} else {
		suffix += _("_out");
		maxports = _output_maximum.get(type);
	}
	
	if (maxports == 1) {
		// allow space for the slash + the suffix
		limit = name_size - _session.engine().client_name().length() - (suffix.length() + 1);
		char buf[name_size+1];
		snprintf (buf, name_size+1, ("%.*s/%s"), limit, _name.c_str(), suffix.c_str());
		return string (buf);
	} 
	
	// allow up to 4 digits for the output port number, plus the slash, suffix and extra space

	limit = name_size - _session.engine().client_name().length() - (suffix.length() + 5);

	char buf1[name_size+1];
	char buf2[name_size+1];
	
	snprintf (buf1, name_size+1, ("%.*s/%s"), limit, _name.c_str(), suffix.c_str());
	
	int port_number;
	
	if (in) {
		port_number = find_input_port_hole (buf1);
	} else {
		port_number = find_output_port_hole (buf1);
	}
	
	snprintf (buf2, name_size+1, "%s %d", buf1, port_number);
	
	return string (buf2);
}

int32_t
IO::find_input_port_hole (const char* base)
{
	/* CALLER MUST HOLD IO LOCK */

	uint32_t n;

	if (_inputs.empty()) {
		return 1;
	}

	/* we only allow up to 4 characters for the port number
	 */

	for (n = 1; n < 9999; ++n) {
		char buf[jack_port_name_size()];
		PortSet::iterator i = _inputs.begin();

		snprintf (buf, jack_port_name_size(), _("%s %u"), base, n);

		for ( ; i != _inputs.end(); ++i) {
			if (i->name() == buf) {
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
IO::find_output_port_hole (const char* base)
{
	/* CALLER MUST HOLD IO LOCK */

	uint32_t n;

	if (_outputs.empty()) {
		return 1;
	}

	/* we only allow up to 4 characters for the port number
	 */

	for (n = 1; n < 9999; ++n) {
		char buf[jack_port_name_size()];
		PortSet::iterator i = _outputs.begin();

		snprintf (buf, jack_port_name_size(), _("%s %u"), base, n);

		for ( ; i != _outputs.end(); ++i) {
			if (i->name() == buf) {
				break;
			}
		}

		if (i == _outputs.end()) {
			break;
		}
	}
	
	return n;
}

void
IO::set_active (bool yn)
{
	_active = yn; 
	 active_changed(); /* EMIT SIGNAL */
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

void
IO::update_port_total_latencies ()
{
	/* io_lock, not taken: function must be called from Session::process() calltree */

	for (PortSet::iterator i = _inputs.begin(); i != _inputs.end(); ++i) {
		_session.engine().update_total_latency (*i);
	}

	for (PortSet::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {
		_session.engine().update_total_latency (*i);
	}
}


/**
 *  Setup bundles that describe our inputs and outputs. Also creates bundles if necessary.
 */

void
IO::setup_bundles_for_inputs_and_outputs ()
{
	setup_bundle_for_inputs ();
	setup_bundle_for_outputs ();
}


void
IO::setup_bundle_for_inputs ()
{
        char buf[32];

	if (!_bundle_for_inputs) {
		_bundle_for_inputs.reset (new Bundle (true));
	}

	_bundle_for_inputs->suspend_signals ();
	
	_bundle_for_inputs->remove_channels ();

        snprintf(buf, sizeof (buf), _("%s in"), _name.c_str());
        _bundle_for_inputs->set_name (buf);
	uint32_t const ni = inputs().num_ports();
	for (uint32_t i = 0; i < ni; ++i) {
		_bundle_for_inputs->add_channel (bundle_channel_name (i, ni));
		_bundle_for_inputs->set_port (i, _session.engine().make_port_name_non_relative (inputs().port(i)->name()));
	}

	_bundle_for_inputs->resume_signals ();
}


void
IO::setup_bundle_for_outputs ()
{
        char buf[32];

	if (!_bundle_for_outputs) {
		_bundle_for_outputs.reset (new Bundle (false));
	}

	_bundle_for_outputs->suspend_signals ();

	_bundle_for_outputs->remove_channels ();

        snprintf(buf, sizeof (buf), _("%s out"), _name.c_str());
        _bundle_for_outputs->set_name (buf);
	uint32_t const no = outputs().num_ports();
	for (uint32_t i = 0; i < no; ++i) {
		_bundle_for_outputs->add_channel (bundle_channel_name (i, no));
		_bundle_for_outputs->set_port (i, _session.engine().make_port_name_non_relative (outputs().port(i)->name()));
	}

	_bundle_for_outputs->resume_signals ();
}


/** @return Bundles connected to our inputs */
BundleList
IO::bundles_connected_to_inputs ()
{
	BundleList bundles;
	
	/* User bundles */
	for (std::vector<UserBundleInfo>::iterator i = _bundles_connected_to_inputs.begin(); i != _bundles_connected_to_inputs.end(); ++i) {
		bundles.push_back (i->bundle);
	}

	/* Normal bundles */
	boost::shared_ptr<ARDOUR::BundleList> b = _session.bundles ();
	for (ARDOUR::BundleList::iterator i = b->begin(); i != b->end(); ++i) {
		if ((*i)->ports_are_outputs() == false || (*i)->nchannels() != n_inputs().n_total()) {
			continue;
		}

		for (uint32_t j = 0; j < n_inputs().n_total(); ++j) {

			Bundle::PortList const& pl = (*i)->channel_ports (j);
			if (!pl.empty() && input(j)->connected_to (pl[0])) {
				bundles.push_back (*i);
			}

		}
	}

	return bundles;
}


/* @return Bundles connected to our outputs */
BundleList
IO::bundles_connected_to_outputs ()
{
	BundleList bundles;

	/* User bundles */
	for (std::vector<UserBundleInfo>::iterator i = _bundles_connected_to_outputs.begin(); i != _bundles_connected_to_outputs.end(); ++i) {
		bundles.push_back (i->bundle);
	}

	/* Auto bundles */
	boost::shared_ptr<ARDOUR::BundleList> b = _session.bundles ();
	for (ARDOUR::BundleList::iterator i = b->begin(); i != b->end(); ++i) {
		if ((*i)->ports_are_inputs() == false || (*i)->nchannels() != n_outputs().n_total()) {
			continue;
		}

		for (uint32_t j = 0; j < n_outputs().n_total(); ++j) {

			Bundle::PortList const& pl = (*i)->channel_ports (j);

			if (!pl.empty() && output(j)->connected_to (pl[0])) {
				bundles.push_back (*i);
			}
		}
	}

	return bundles;	
}


IO::UserBundleInfo::UserBundleInfo (IO* io, boost::shared_ptr<UserBundle> b)
{
	bundle = b;
	changed = b->Changed.connect (
		sigc::mem_fun (*io, &IO::bundle_changed)
		);
}

void
IO::prepare_inputs (nframes_t nframes, nframes_t offset)
{
	/* io_lock, not taken: function must be called from Session::process() calltree */
}

void
IO::flush_outputs (nframes_t nframes, nframes_t offset)
{
	/* io_lock, not taken: function must be called from Session::process() calltree */
	for (PortSet::iterator i = _outputs.begin(); i != _outputs.end(); ++i) {

		/* Only run cycle_start() on output ports, because 
		   inputs must be done in the correct processing order,
		   which requires interleaving with route processing.
		*/

		(*i).flush_buffers (nframes, offset);
	}
		
}

std::string
IO::bundle_channel_name (uint32_t c, uint32_t n) const
{
	char buf[32];
	
	switch (n) {
	case 1:
		return _("mono");
	case 2:
		return c == 0 ? _("L") : _("R");
	default:
		snprintf (buf, sizeof(buf), _("%d"), (c + 1));
		return buf;
	}

	return "";
}


