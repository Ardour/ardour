/*
    Copyright (C) 2009 Paul Davis

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

#include "pbd/error.h"
#include "pbd/failed_constructor.h"

#include "ardour/amp.h"
#include "ardour/audio_buffer.h"
#include "ardour/internal_return.h"
#include "ardour/internal_send.h"
#include "ardour/meter.h"
#include "ardour/panner_shell.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"

#include "i18n.h"

namespace ARDOUR { class MuteMaster; class Pannable; }

using namespace PBD;
using namespace ARDOUR;
using namespace std;

PBD::Signal1<void, pframes_t> InternalSend::CycleStart;

InternalSend::InternalSend (Session& s, boost::shared_ptr<Pannable> p, boost::shared_ptr<MuteMaster> mm, boost::shared_ptr<Route> sendto, Delivery::Role role)
	: Send (s, p, mm, role)
{
        if (sendto) {
                if (use_target (sendto)) {
                        throw failed_constructor();
                }
        }

	init_gain ();

	CycleStart.connect_same_thread (*this, boost::bind (&InternalSend::cycle_start, this, _1));
}

InternalSend::~InternalSend ()
{
	if (_send_to) {
		_send_to->remove_send_from_internal_return (this);
	}
}

void
InternalSend::init_gain ()
{
	if (_role == Listen) {
		/* send to monitor bus is always at unity */
		_amp->set_gain (1.0, this);
	} else {
		/* aux sends start at -inf dB */
		_amp->set_gain (0, this);
	}
}

int
InternalSend::use_target (boost::shared_ptr<Route> sendto)
{
	if (_send_to) {
		_send_to->remove_send_from_internal_return (this);
	}

        _send_to = sendto;

        _send_to->add_send_to_internal_return (this);

	mixbufs.ensure_buffers (_send_to->internal_return()->input_streams(), _session.get_block_size());
	mixbufs.set_count (_send_to->internal_return()->input_streams());

	reset_panner ();

        set_name (sendto->name());
        _send_to_id = _send_to->id();

        target_connections.drop_connections ();

        _send_to->DropReferences.connect_same_thread (target_connections, boost::bind (&InternalSend::send_to_going_away, this));
        _send_to->PropertyChanged.connect_same_thread (target_connections, boost::bind (&InternalSend::send_to_property_changed, this, _1));;

        return 0;
}

void
InternalSend::send_to_going_away ()
{
        target_connections.drop_connections ();
	_send_to.reset ();
	_send_to_id = "0";
}

void
InternalSend::run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, pframes_t nframes, bool)
{
	if ((!_active && !_pending_active) || !_send_to) {
		_meter->reset ();
		return;
	}

	// we have to copy the input, because we may alter the buffers with the amp
	// in-place, which a send must never do.

	if (_panshell && !_panshell->bypassed()) {
		_panshell->run (bufs, mixbufs, start_frame, end_frame, nframes);
	} else {
		if (role() == Listen) {
			/* We're going to the monitor bus, so discard MIDI data */
			
			uint32_t const bufs_audio = bufs.count().get (DataType::AUDIO);
			uint32_t const mixbufs_audio = mixbufs.count().get (DataType::AUDIO);
			
			assert (mixbufs.available().get (DataType::AUDIO) >= bufs_audio);

			/* Copy bufs into mixbufs, going round bufs more than once if necessary
			   to ensure that every mixbuf gets some data.
			*/

			uint32_t j = 0;
			for (uint32_t i = 0; i < mixbufs_audio; ++i) {
				mixbufs.get_audio(i).read_from (bufs.get_audio(j), nframes);
				++j;

				if (j == bufs_audio) {
					j = 0;
				}
			}

		} else {
			assert (mixbufs.available() >= bufs.count());
			mixbufs.read_from (bufs, nframes);
		}
	}

	/* gain control */

	gain_t tgain = target_gain ();

	if (tgain != _current_gain) {

		/* target gain has changed */

		Amp::apply_gain (mixbufs, nframes, _current_gain, tgain);
		_current_gain = tgain;

	} else if (tgain == 0.0) {

		/* we were quiet last time, and we're still supposed to be quiet.
		*/

		_meter->reset ();
		Amp::apply_simple_gain (mixbufs, nframes, 0.0);
		goto out;

	} else if (tgain != 1.0) {

		/* target gain has not changed, but is not zero or unity */
		Amp::apply_simple_gain (mixbufs, nframes, tgain);
	}

	_amp->set_gain_automation_buffer (_session.send_gain_automation_buffer ());
	_amp->setup_gain_automation (start_frame, end_frame, nframes);
	_amp->run (mixbufs, start_frame, end_frame, nframes, true);

	/* consider metering */

	if (_metering) {
		if (_amp->gain_control()->get_value() == 0) {
			_meter->reset();
		} else {
			_meter->run (mixbufs, start_frame, end_frame, nframes, true);
		}
	}

	/* target will pick up our output when it is ready */

  out:
	_active = _pending_active;
}

int
InternalSend::set_block_size (pframes_t nframes)
{
	if (_send_to) {
		mixbufs.ensure_buffers (_send_to->internal_return()->input_streams(), nframes);
	}

        return 0;
}

bool
InternalSend::feeds (boost::shared_ptr<Route> other) const
{
	return _send_to == other;
}

XMLNode&
InternalSend::state (bool full)
{
	XMLNode& node (Send::state (full));

	/* this replaces any existing "type" property */

	node.add_property ("type", "intsend");

	if (_send_to) {
		node.add_property ("target", _send_to->id().to_s());
	}

	return node;
}

XMLNode&
InternalSend::get_state()
{
	return state (true);
}

int
InternalSend::set_state (const XMLNode& node, int version)
{
	const XMLProperty* prop;

	init_gain ();

	Send::set_state (node, version);

	if ((prop = node.property ("target")) != 0) {

		_send_to_id = prop->value();

		/* if we're loading a session, the target route may not have been
		   create yet. make sure we defer till we are sure that it should
		   exist.
		*/

		if (!IO::connecting_legal) {
			IO::ConnectingLegal.connect_same_thread (connect_c, boost::bind (&InternalSend::connect_when_legal, this));
		} else {
			connect_when_legal ();
		}
	}

	return 0;
}

int
InternalSend::connect_when_legal ()
{
	connect_c.disconnect ();

	if (_send_to_id == "0") {
		/* it vanished before we could connect */
		return 0;
	}

        boost::shared_ptr<Route> sendto;

	if ((sendto = _session.route_by_id (_send_to_id)) == 0) {
		error << string_compose (_("%1 - cannot find any track/bus with the ID %2 to connect to"), display_name(), _send_to_id) << endmsg;
		cerr << string_compose (_("%1 - cannot find any track/bus with the ID %2 to connect to"), display_name(), _send_to_id) << endl;
		return -1;
	}

        return use_target (sendto);
}

bool
InternalSend::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	out = in;
	return true;
}

uint32_t
InternalSend::pan_outs () const
{
	/* the number of targets for our panner is determined by what we are
	   sending to, if anything.
	*/

	if (_send_to) {
		return _send_to->internal_return()->input_streams().n_audio();
	}

	return 1; /* zero is more accurate, but 1 is probably safer as a way to
		   * say "don't pan"
		   */
}

bool
InternalSend::configure_io (ChanCount in, ChanCount out)
{
	bool ret = Send::configure_io (in, out);
	set_block_size (_session.engine().frames_per_cycle());
	return ret;
}

bool
InternalSend::set_name (const string& str)
{
	/* rules for external sends don't apply to us */
	return IOProcessor::set_name (str);
}

string
InternalSend::display_name () const
{
	if (_role == Aux) {
		return string_compose (X_("%1"), _name);
	} else {
		return _name;
	}
}

bool
InternalSend::visible () const
{
	if (_role == Aux) {
		return true;
	}

	return false;
}

void
InternalSend::send_to_property_changed (const PropertyChange& what_changed)
{
	if (what_changed.contains (Properties::name)) {
		set_name (_send_to->name ());
	}
}

void
InternalSend::set_can_pan (bool yn)
{
	if (_panshell) {
		_panshell->set_bypassed (!yn);
	}
}

void
InternalSend::cycle_start (pframes_t /*nframes*/)
{
	for (BufferSet::audio_iterator b = mixbufs.audio_begin(); b != mixbufs.audio_end(); ++b) {
		b->prepare ();
	}
}	
