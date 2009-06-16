/*
    Copyright (C) 2009 Paul Davis 
    
    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
    
    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.
    
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <cmath>
#include <algorithm>

#include "pbd/enumwriter.h"
#include "pbd/convert.h"

#include "ardour/delivery.h"
#include "ardour/audio_buffer.h"
#include "ardour/amp.h"
#include "ardour/buffer_set.h"
#include "ardour/configuration.h"
#include "ardour/io.h"
#include "ardour/meter.h"
#include "ardour/mute_master.h"
#include "ardour/panner.h"
#include "ardour/port.h"
#include "ardour/session.h"

#include "i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

sigc::signal<void,nframes_t> Delivery::CycleStart;
sigc::signal<int>            Delivery::PannersLegal;
bool                         Delivery::panners_legal = false;

/* deliver to an existing IO object */

Delivery::Delivery (Session& s, boost::shared_ptr<IO> io, boost::shared_ptr<MuteMaster> mm, const string& name, Role r)
	: IOProcessor(s, boost::shared_ptr<IO>(), (r == Listen ? boost::shared_ptr<IO>() : io), name)
	, _role (r)
	, _output_buffers (new BufferSet())
	, _solo_level (0)
	, _solo_isolated (false)
	, _mute_master (mm)
{
	_output_offset = 0;
	_current_gain = 1.0;
	_panner = boost::shared_ptr<Panner>(new Panner (_name, _session));
	
	if (_output) {
		_output->changed.connect (mem_fun (*this, &Delivery::output_changed));
	}
}

/* deliver to a new IO object */

Delivery::Delivery (Session& s, boost::shared_ptr<MuteMaster> mm, const string& name, Role r)
	: IOProcessor(s, false, (r == Listen ? false : true), name)
	, _role (r)
	, _output_buffers (new BufferSet())
	, _solo_level (0)
	, _solo_isolated (false)
	, _mute_master (mm)
{
	_output_offset = 0;
	_current_gain = 1.0;
	_panner = boost::shared_ptr<Panner>(new Panner (_name, _session));

	if (_output) {
		_output->changed.connect (mem_fun (*this, &Delivery::output_changed));
	}
}

/* deliver to a new IO object, reconstruct from XML */

Delivery::Delivery (Session& s, boost::shared_ptr<MuteMaster> mm, const XMLNode& node)
	: IOProcessor (s, false, true, "reset")
	, _role (Role (0))
	, _output_buffers (new BufferSet())
	, _solo_level (0)
	, _solo_isolated (false)
	, _mute_master (mm)
{
	_output_offset = 0;
	_current_gain = 1.0;
	_panner = boost::shared_ptr<Panner>(new Panner (_name, _session));

	if (set_state (node)) {
		throw failed_constructor ();
	}

	if (_output) {
		_output->changed.connect (mem_fun (*this, &Delivery::output_changed));
	}
}

/* deliver to an existing IO object, reconstruct from XML */

Delivery::Delivery (Session& s, boost::shared_ptr<IO> out, boost::shared_ptr<MuteMaster> mm, const XMLNode& node)
	: IOProcessor (s, boost::shared_ptr<IO>(), out, "reset")
	, _role (Role (0))
	, _output_buffers (new BufferSet())
	, _solo_level (0)
	, _solo_isolated (false)
	, _mute_master (mm)
{
	_output_offset = 0;
	_current_gain = 1.0;
	_panner = boost::shared_ptr<Panner>(new Panner (_name, _session));

	if (set_state (node)) {
		throw failed_constructor ();
	}

	if (_output) {
		_output->changed.connect (mem_fun (*this, &Delivery::output_changed));
	}
}

void
Delivery::cycle_start (nframes_t nframes)
{
	_output_offset = 0;
	_no_outs_cuz_we_no_monitor = false;
}

void
Delivery::increment_output_offset (nframes_t n)
{
	_output_offset += n;
}

bool
Delivery::visible () const
{
	if (_role & Main) {
		return false;
	}

	return true;
}

bool
Delivery::can_support_io_configuration (const ChanCount& in, ChanCount& out) const
{
	out = in;
	return true;
}

bool
Delivery::configure_io (ChanCount in, ChanCount out)
{
	if (out != in) { // always 1:1
		return false;
	}

	reset_panner ();
	
	return Processor::configure_io (in, out);
}

void
Delivery::run (BufferSet& bufs, sframes_t start_frame, sframes_t end_frame, nframes_t nframes)
{
	if (_output->n_ports ().get (_output->default_type()) == 0) {
		return;
	}

	/* this setup is not just for our purposes, but for anything that comes after us in the 
	   processing pathway that wants to use this->output_buffers() for some reason.
	*/

	PortSet& ports (_output->ports());
	output_buffers().attach_buffers (ports, nframes, _output_offset);

	// this Delivery processor is not a derived type, and thus we assume
	// we really can modify the buffers passed in (it is almost certainly
	// the main output stage of a Route). Contrast with Send::run()
	// which cannot do this.

	gain_t tgain = target_gain ();
	
	if (tgain != _current_gain) {
		
		/* target gain has changed */

		Amp::apply_gain (bufs, nframes, _current_gain, tgain);
		_current_gain = tgain;

	} else if (tgain == 0.0) {

		/* we were quiet last time, and we're still supposed to be quiet.
		   Silence the outputs, and make sure the buffers are quiet too,
		*/

		_output->silence (nframes);
		Amp::apply_simple_gain (bufs, nframes, 0.0);
		
		return;

	} else if (tgain != 1.0) {

		/* target gain has not changed, but is not unity */
		Amp::apply_simple_gain (bufs, nframes, tgain);
	}

	// Attach output buffers to port buffers

	if (_panner && _panner->npanners() && !_panner->bypassed()) {

		// Use the panner to distribute audio to output port buffers

		_panner->run (bufs, output_buffers(), start_frame, end_frame, nframes);

	} else {
		// Do a 1:1 copy of data to output ports

		if (bufs.count().n_audio() > 0 && ports.count().n_audio () > 0) {
			_output->copy_to_outputs (bufs, DataType::AUDIO, nframes, _output_offset);
		}

		if (bufs.count().n_midi() > 0 && ports.count().n_midi () > 0) {
			_output->copy_to_outputs (bufs, DataType::MIDI, nframes, _output_offset);
		}
	}
}


XMLNode&
Delivery::state (bool full_state)
{
	XMLNode& node (IOProcessor::state (full_state));

	if (_role & Main) {
		node.add_property("type", "main-outs");
	} else if (_role & Listen) {
		node.add_property("type", "listen");
	} else {
		node.add_property("type", "delivery");
	}

	node.add_property("role", enum_2_string(_role));
	node.add_child_nocopy (_panner->state (full_state));

	return node;
}

int
Delivery::set_state (const XMLNode& node)
{
	const XMLProperty* prop;

	if (IOProcessor::set_state (node)) {
		return -1;
	}
	
	if ((prop = node.property ("role")) != 0) {
		_role = Role (string_2_enum (prop->value(), _role));
	}

	if ((prop = node.property ("solo_level")) != 0) {
		_solo_level = 0; // needed for the reset to work
		mod_solo_level (atoi (prop->value()));
	}

	if ((prop = node.property ("solo-isolated")) != 0) {
		set_solo_isolated (prop->value() == "yes");
	}

	XMLNode* pan_node = node.child (X_("Panner"));
	
	if (pan_node) {
		_panner->set_state (*pan_node);
	} 

	reset_panner ();

	return 0;
}

void
Delivery::reset_panner ()
{
	if (panners_legal) {
		if (!no_panner_reset) {

			uint32_t ntargets;
			
			if (_output) {
				ntargets = _output->n_ports().n_audio();
			} else {
				ntargets = _configured_output.n_audio();
			}

			_panner->reset (ntargets, pans_required());
		}
	} else {
		panner_legal_c.disconnect ();
		panner_legal_c = PannersLegal.connect (mem_fun (*this, &Delivery::panners_became_legal));
	}
}

int
Delivery::panners_became_legal ()
{
	uint32_t ntargets;

	if (_output) {
		ntargets = _output->n_ports().n_audio();
	} else {
		ntargets = _configured_output.n_audio();
	}

	_panner->reset (ntargets, pans_required());
	_panner->load (); // automation
	panner_legal_c.disconnect ();
	return 0;
}

void
Delivery::defer_pan_reset ()
{
	no_panner_reset = true;
}

void
Delivery::allow_pan_reset ()
{
	no_panner_reset = false;
	reset_panner ();
}


int
Delivery::disable_panners (void)
{
	panners_legal = false;
	return 0;
}

int
Delivery::reset_panners ()
{
	panners_legal = true;
	return PannersLegal ();
}


void
Delivery::start_pan_touch (uint32_t which)
{
	if (which < _panner->npanners()) {
		_panner->pan_control(which)->start_touch();
	}
}

void
Delivery::end_pan_touch (uint32_t which)
{
	if (which < _panner->npanners()) {
		_panner->pan_control(which)->stop_touch();
	}

}

void
Delivery::transport_stopped (sframes_t frame)
{
	_panner->transport_stopped (frame);
}

void
Delivery::flush (nframes_t nframes)
{
	/* io_lock, not taken: function must be called from Session::process() calltree */
	
	PortSet& ports (_output->ports());

	for (PortSet::iterator i = ports.begin(); i != ports.end(); ++i) {
		(*i).flush_buffers (nframes, _output_offset);
	}
}

gain_t
Delivery::target_gain ()
{
	/* if we've been told not to output because its a monitoring situation and
	   we're not monitoring, then be quiet.
	*/

	if (_no_outs_cuz_we_no_monitor) {
		return 0.0;
	}

	gain_t desired_gain;
	MuteMaster::MutePoint mp;

	switch (_role) {
	case Main:
		mp = MuteMaster::Main;
		break;
	case Listen:
		mp = MuteMaster::Listen;
		break;
	case Send:
	case Insert:
		if (_placement == PreFader) {
			mp = MuteMaster::PreFader;
		} else {
			mp = MuteMaster::PostFader;
		}
		break;
	}

	if (_solo_level) {
		desired_gain = 1.0;
	} else {
		if (_solo_isolated) {

			desired_gain = _mute_master->mute_gain_at (mp);

		} else if (_session.soloing()) {

			desired_gain = min (Config->get_solo_mute_gain(), _mute_master->mute_gain_at (mp));

		} else {
			desired_gain = _mute_master->mute_gain_at (mp);
		}
	}

	return desired_gain;
}

void
Delivery::mod_solo_level (int32_t delta)
{
	if (delta < 0) {
		if (_solo_level >= (uint32_t) delta) {
			_solo_level += delta;
		} else {
			_solo_level = 0;
		}
	} else {
		_solo_level += delta;
	}
}

void
Delivery::set_solo_isolated (bool yn)
{
	_solo_isolated = yn;
}

void
Delivery::no_outs_cuz_we_no_monitor (bool yn)
{
	_no_outs_cuz_we_no_monitor = yn;
}

bool
Delivery::set_name (const std::string& name)
{
	bool ret = IOProcessor::set_name (name);

	if (ret) {
		ret = _panner->set_name (name);
	}

	return ret;
}

void
Delivery::output_changed (IOChange change, void* src)
{
	if (change & ARDOUR::ConfigurationChanged) {
		reset_panner ();
	}
}

