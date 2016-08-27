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
#include "pbd/enum_convert.h"

#include "ardour/amp.h"
#include "ardour/audioengine.h"
#include "ardour/buffer_set.h"
#include "ardour/debug.h"
#include "ardour/delivery.h"
#include "ardour/io.h"
#include "ardour/mute_master.h"
#include "ardour/pannable.h"
#include "ardour/panner_shell.h"
#include "ardour/port.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

namespace PBD {
	DEFINE_ENUM_CONVERT(ARDOUR::Delivery::Role);
}

namespace ARDOUR { class Panner; }

using namespace std;
using namespace PBD;
using namespace ARDOUR;

PBD::Signal0<void>            Delivery::PannersLegal;
bool                          Delivery::panners_legal = false;

/* deliver to an existing IO object */

Delivery::Delivery (Session& s, boost::shared_ptr<IO> io, boost::shared_ptr<Pannable> pannable,
                    boost::shared_ptr<MuteMaster> mm, const string& name, Role r)
	: IOProcessor(s, boost::shared_ptr<IO>(), (role_requires_output_ports (r) ? io : boost::shared_ptr<IO>()), name)
	, _role (r)
	, _output_buffers (new BufferSet())
	, _current_gain (GAIN_COEFF_UNITY)
	, _no_outs_cuz_we_no_monitor (false)
	, _mute_master (mm)
	, _no_panner_reset (false)
{
	if (pannable) {
		bool is_send = false;
		if (r & (Delivery::Send|Delivery::Aux)) is_send = true;
		_panshell = boost::shared_ptr<PannerShell>(new PannerShell (_name, _session, pannable, is_send));
	}

	_display_to_user = false;

	if (_output) {
		_output->changed.connect_same_thread (*this, boost::bind (&Delivery::output_changed, this, _1, _2));
	}
}

/* deliver to a new IO object */

Delivery::Delivery (Session& s, boost::shared_ptr<Pannable> pannable, boost::shared_ptr<MuteMaster> mm, const string& name, Role r)
	: IOProcessor(s, false, (role_requires_output_ports (r) ? true : false), name, "", DataType::AUDIO, (r == Send))
	, _role (r)
	, _output_buffers (new BufferSet())
	, _current_gain (GAIN_COEFF_UNITY)
	, _no_outs_cuz_we_no_monitor (false)
	, _mute_master (mm)
	, _no_panner_reset (false)
{
	if (pannable) {
		bool is_send = false;
		if (r & (Delivery::Send|Delivery::Aux)) is_send = true;
		_panshell = boost::shared_ptr<PannerShell>(new PannerShell (_name, _session, pannable, is_send));
	}

	_display_to_user = false;

	if (_output) {
		_output->changed.connect_same_thread (*this, boost::bind (&Delivery::output_changed, this, _1, _2));
	}
}


Delivery::~Delivery()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("delivery %1 destructor\n", _name));

	/* this object should vanish from any signal callback lists
	   that it is on before we get any further. The full qualification
	   of the method name is not necessary, but is here to make it
	   clear that this call is about signals, not data flow connections.
	*/

	ScopedConnectionList::drop_connections ();

	delete _output_buffers;
}

std::string
Delivery::display_name () const
{
	switch (_role) {
	case Main:
		return _("main outs");
		break;
	case Listen:
		return _("listen");
		break;
	case Send:
	case Insert:
	default:
		return name();
	}
}

bool
Delivery::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	if (_role == Main) {

		/* the out buffers will be set to point to the port output buffers
		   of our output object.
		*/

		if (_output) {
			if (_output->n_ports() != ChanCount::ZERO) {
				/* increase number of output ports if the processor chain requires it */
				out = ChanCount::max (_output->n_ports(), in);
				return true;
			} else {
				/* not configured yet - we will passthru */
				out = in;
				return true;
			}
		} else {
			fatal << "programming error: this should never be reached" << endmsg;
			abort(); /*NOTREACHED*/
		}


	} else if (_role == Insert) {

		/* the output buffers will be filled with data from the *input* ports
		   of this Insert.
		*/

		if (_input) {
			if (_input->n_ports() != ChanCount::ZERO) {
				out = _input->n_ports();
				return true;
			} else {
				/* not configured yet - we will passthru */
				out = in;
				return true;
			}
		} else {
			fatal << "programming error: this should never be reached" << endmsg;
			abort(); /*NOTREACHED*/
		}

	} else {
		fatal << "programming error: this should never be reached" << endmsg;
	}

	return false;
}

/** Caller must hold process lock */
bool
Delivery::configure_io (ChanCount in, ChanCount out)
{
#ifndef NDEBUG
	bool r = AudioEngine::instance()->process_lock().trylock();
	assert (!r && "trylock inside Delivery::configure_io");
#endif

	/* check configuration by comparison with our I/O port configuration, if appropriate.
	   see ::can_support_io_configuration() for comments
	*/

	if (_role == Main) {

		if (_output) {
			if (_output->n_ports() != out) {
				if (_output->n_ports() != ChanCount::ZERO) {
					_output->ensure_io (out, false, this);
				} else {
					/* I/O not yet configured */
				}
			}
		}

	} else if (_role == Insert) {

		if (_input) {
			if (_input->n_ports() != in) {
				if (_input->n_ports() != ChanCount::ZERO) {
					fatal << _name << " programming error: configure_io called with " << in << " and " << out << " with " << _input->n_ports() << " input ports" << endmsg;
					abort(); /*NOTREACHED*/
				} else {
					/* I/O not yet configured */
				}
			}
		}

	}

	if (!Processor::configure_io (in, out)) {
		return false;
	}

	reset_panner ();

	return true;
}

void
Delivery::run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, double /*speed*/, pframes_t nframes, bool result_required)
{
	assert (_output);

	PortSet& ports (_output->ports());
	gain_t tgain;

	if (ports.num_ports () == 0) {
		goto out;
	}

	if (!_active && !_pending_active) {
		_output->silence (nframes);
		goto out;
	}

	/* this setup is not just for our purposes, but for anything that comes after us in the
	 * processing pathway that wants to use this->output_buffers() for some reason.
	 */

	// TODO delayline -- latency-compensation
	output_buffers().get_backend_port_addresses (ports, nframes);

	// this Delivery processor is not a derived type, and thus we assume
	// we really can modify the buffers passed in (it is almost certainly
	// the main output stage of a Route). Contrast with Send::run()
	// which cannot do this.

	tgain = target_gain ();

	if (tgain != _current_gain) {
		/* target gain has changed */

		_current_gain = Amp::apply_gain (bufs, _session.nominal_frame_rate(), nframes, _current_gain, tgain);

	} else if (tgain < GAIN_COEFF_SMALL) {

		/* we were quiet last time, and we're still supposed to be quiet.
			 Silence the outputs, and make sure the buffers are quiet too,
			 */

		_output->silence (nframes);
		if (result_required) {
			bufs.set_count (output_buffers().count ());
			Amp::apply_simple_gain (bufs, nframes, GAIN_COEFF_ZERO);
		}
		goto out;

	} else if (tgain != GAIN_COEFF_UNITY) {

		/* target gain has not changed, but is not unity */
		Amp::apply_simple_gain (bufs, nframes, tgain);
	}

	// Speed quietning

	if (fabs (_session.transport_speed()) > 1.5 && Config->get_quieten_at_speed ()) {
		Amp::apply_simple_gain (bufs, nframes, speed_quietning, false);
	}

	// Panning

	if (_panshell && !_panshell->bypassed() && _panshell->panner()) {

		// Use the panner to distribute audio to output port buffers

		_panshell->run (bufs, output_buffers(), start_frame, end_frame, nframes);

		// non-audio data will not have been delivered by the panner

		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			if (*t != DataType::AUDIO && bufs.count().get(*t) > 0) {
				_output->copy_to_outputs (bufs, *t, nframes, Port::port_offset());
			}
		}

	} else {

		/* Do a 1:1 copy of data to output ports

		   Audio is handled separately because we use 0 for the offset,
		   since the port offset is only used for timestamped events
		   (i.e. MIDI).
		*/

		if (bufs.count().n_audio() > 0) {
			_output->copy_to_outputs (bufs, DataType::AUDIO, nframes, 0);
		}

		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			if (*t != DataType::AUDIO && bufs.count().get(*t) > 0) {
				_output->copy_to_outputs (bufs, *t, nframes, Port::port_offset());
			}
		}
	}

	if (result_required) {
		/* "bufs" are internal, meaning they should never reflect
		   split-cycle offsets. So shift events back in time from where
		   they were for the external buffers associated with Ports.
		*/

		const BufferSet& outs (output_buffers());
		bufs.set_count (output_buffers().count ());

		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {

			uint32_t n = 0;
			for (BufferSet::iterator b = bufs.begin (*t); b != bufs.end (*t); ++b) {
				if (outs.count ().get (*t) <= n) {
					continue;
				}
				b->read_from (outs.get (*t, n++), nframes, (*t == DataType::AUDIO ? 0 : -Port::port_offset()));
			}
		}
	}

out:
	_active = _pending_active;
}

XMLNode&
Delivery::state (bool full_state)
{
	XMLNode& node (IOProcessor::state (full_state));

	if (_role & Main) {
		node.set_property("type", "main-outs");
	} else if (_role & Listen) {
		node.set_property("type", "listen");
	} else {
		node.set_property("type", "delivery");
	}

	node.set_property("role", _role);

	if (_panshell) {
		node.add_child_nocopy (_panshell->get_state ());
		if (_panshell->pannable()) {
			node.add_child_nocopy (_panshell->pannable()->get_state ());
		}
	}

	return node;
}

int
Delivery::set_state (const XMLNode& node, int version)
{
	if (IOProcessor::set_state (node, version)) {
		return -1;
	}

	if (node.get_property ("role", _role)) {
		// std::cerr << this << ' ' << _name << " set role to " << enum_2_string (_role) << std::endl;
	} else {
		// std::cerr << this << ' ' << _name << " NO ROLE INFO\n";
	}

	XMLNode* pan_node = node.child (X_("PannerShell"));

	if (pan_node && _panshell) {
		_panshell->set_state (*pan_node, version);
	}

	reset_panner ();

	XMLNode* pannnode = node.child (X_("Pannable"));
	if (_panshell && _panshell->panner() && pannnode) {
		_panshell->pannable()->set_state (*pannnode, version);
	}

	return 0;
}

void
Delivery::unpan ()
{
	/* caller must hold process lock */

	_panshell.reset ();
}

uint32_t
Delivery::pan_outs () const
{
	if (_output) {
		return _output->n_ports().n_audio();
	}

	return _configured_output.n_audio();
}

void
Delivery::reset_panner ()
{
	if (panners_legal) {
		if (!_no_panner_reset) {

			if (_panshell && _role != Insert && _role != Listen) {
				_panshell->configure_io (ChanCount (DataType::AUDIO, pans_required()), ChanCount (DataType::AUDIO, pan_outs()));
			}
		}

	} else {
		panner_legal_c.disconnect ();
		PannersLegal.connect_same_thread (panner_legal_c, boost::bind (&Delivery::panners_became_legal, this));
	}
}

void
Delivery::panners_became_legal ()
{
	if (_panshell && _role != Insert) {
		_panshell->configure_io (ChanCount (DataType::AUDIO, pans_required()), ChanCount (DataType::AUDIO, pan_outs()));
	}

	panner_legal_c.disconnect ();
}

void
Delivery::defer_pan_reset ()
{
	_no_panner_reset = true;
}

void
Delivery::allow_pan_reset ()
{
	_no_panner_reset = false;
	reset_panner ();
}


int
Delivery::disable_panners ()
{
	panners_legal = false;
	return 0;
}

void
Delivery::reset_panners ()
{
	panners_legal = true;
	PannersLegal ();
}

void
Delivery::flush_buffers (framecnt_t nframes)
{
	/* io_lock, not taken: function must be called from Session::process() calltree */

	if (!_output) {
		return;
	}

	PortSet& ports (_output->ports());

	for (PortSet::iterator i = ports.begin(); i != ports.end(); ++i) {
		i->flush_buffers (nframes);
	}
}

void
Delivery::transport_stopped (framepos_t now)
{
        Processor::transport_stopped (now);

	if (_panshell) {
		_panshell->pannable()->transport_stopped (now);
	}

        if (_output) {
                PortSet& ports (_output->ports());

                for (PortSet::iterator i = ports.begin(); i != ports.end(); ++i) {
                        i->transport_stopped ();
                }
        }
}

void
Delivery::realtime_locate ()
{
	if (_output) {
                PortSet& ports (_output->ports());

                for (PortSet::iterator i = ports.begin(); i != ports.end(); ++i) {
                        i->realtime_locate ();
                }
        }
}

gain_t
Delivery::target_gain ()
{
	/* if we've been requested to deactivate, our target gain is zero */

	if (!_pending_active) {
		return GAIN_COEFF_ZERO;
	}

	/* if we've been told not to output because its a monitoring situation and
	   we're not monitoring, then be quiet.
	*/

	if (_no_outs_cuz_we_no_monitor) {
		return GAIN_COEFF_ZERO;
	}

        MuteMaster::MutePoint mp = MuteMaster::Main; // stupid gcc uninit warning

        switch (_role) {
        case Main:
                mp = MuteMaster::Main;
                break;
        case Listen:
                mp = MuteMaster::Listen;
                break;
        case Send:
        case Insert:
        case Aux:
		if (_pre_fader) {
			mp = MuteMaster::PreFader;
		} else {
			mp = MuteMaster::PostFader;
		}
                break;
        }

        gain_t desired_gain = _mute_master->mute_gain_at (mp);

        if (_role == Listen && _session.monitor_out() && !_session.listening()) {

                /* nobody is soloed, and this delivery is a listen-send to the
                   control/monitor/listen bus, we should be silent since
                   it gets its signal from the master out.
                */

                desired_gain = GAIN_COEFF_ZERO;

        }

	return desired_gain;
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

	if (ret && _panshell) {
		ret = _panshell->set_name (name);
	}

	return ret;
}

bool ignore_output_change = false;

void
Delivery::output_changed (IOChange change, void* /*src*/)
{
	if (change.type & IOChange::ConfigurationChanged) {
		reset_panner ();
		_output_buffers->attach_buffers (_output->ports ());
	}
}

boost::shared_ptr<Panner>
Delivery::panner () const
{
	if (_panshell) {
		return _panshell->panner();
	} else {
		return boost::shared_ptr<Panner>();
	}
}

