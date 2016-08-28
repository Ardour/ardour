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

*/

#include <iostream>
#include <algorithm>

#include "pbd/xml++.h"

#include "ardour/amp.h"
#include "ardour/boost_debug.h"
#include "ardour/buffer_set.h"
#include "ardour/debug.h"
#include "ardour/gain_control.h"
#include "ardour/io.h"
#include "ardour/meter.h"
#include "ardour/panner_shell.h"
#include "ardour/send.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

namespace ARDOUR {
class AutomationControl;
class MuteMaster;
class Pannable;
}

using namespace ARDOUR;
using namespace PBD;
using namespace std;

string
Send::name_and_id_new_send (Session& s, Role r, uint32_t& bitslot, bool ignore_bitslot)
{
	if (ignore_bitslot) {
		/* this happens during initial construction of sends from XML,
		   before they get ::set_state() called. lets not worry about
		   it.
		*/
		bitslot = 0;
		return string ();
	}

	switch (r) {
	case Delivery::Aux:
		return string_compose (_("aux %1"), (bitslot = s.next_aux_send_id ()) + 1);
	case Delivery::Listen:
		return _("listen"); // no ports, no need for numbering
	case Delivery::Send:
		return string_compose (_("send %1"), (bitslot = s.next_send_id ()) + 1);
	default:
		fatal << string_compose (_("programming error: send created using role %1"), enum_2_string (r)) << endmsg;
		abort(); /*NOTREACHED*/
		return string();
	}

}

Send::Send (Session& s, boost::shared_ptr<Pannable> p, boost::shared_ptr<MuteMaster> mm, Role r, bool ignore_bitslot)
	: Delivery (s, p, mm, name_and_id_new_send (s, r, _bitslot, ignore_bitslot), r)
	, _metering (false)
	, _delay_in (0)
	, _delay_out (0)
	, _remove_on_disconnect (false)
{
	if (_role == Listen) {
		/* we don't need to do this but it keeps things looking clean
		   in a debugger. _bitslot is not used by listen sends.
		*/
		_bitslot = 0;
	}

	//boost_debug_shared_ptr_mark_interesting (this, "send");

	boost::shared_ptr<AutomationList> gl (new AutomationList (Evoral::Parameter (GainAutomation)));
	_gain_control = boost::shared_ptr<GainControl> (new GainControl (_session, Evoral::Parameter(GainAutomation), gl));
	add_control (_gain_control);

	_amp.reset (new Amp (_session, _("Fader"), _gain_control, true));
	_meter.reset (new PeakMeter (_session, name()));

	_delayline.reset (new DelayLine (_session, name()));

	if (panner_shell()) {
		panner_shell()->Changed.connect_same_thread (*this, boost::bind (&Send::panshell_changed, this));
	}
	if (_output) {
		_output->changed.connect_same_thread (*this, boost::bind (&Send::snd_output_changed, this, _1, _2));
	}
}

Send::~Send ()
{
        _session.unmark_send_id (_bitslot);
}

void
Send::activate ()
{
	_amp->activate ();
	_meter->activate ();

	Processor::activate ();
}

void
Send::deactivate ()
{
	_amp->deactivate ();
	_meter->deactivate ();
	_meter->reset ();

	Processor::deactivate ();
}

void
Send::set_delay_in(framecnt_t delay)
{
	if (!_delayline) return;
	if (_delay_in == delay) {
		return;
	}
	_delay_in = delay;

	DEBUG_TRACE (DEBUG::LatencyCompensation,
			string_compose ("Send::set_delay_in(%1) + %2 = %3\n",
				delay, _delay_out, _delay_out + _delay_in));
	_delayline.get()->set_delay(_delay_out + _delay_in);
}

void
Send::set_delay_out(framecnt_t delay)
{
	if (!_delayline) return;
	if (_delay_out == delay) {
		return;
	}
	_delay_out = delay;
	DEBUG_TRACE (DEBUG::LatencyCompensation,
			string_compose ("Send::set_delay_out(%1) + %2 = %3\n",
				delay, _delay_in, _delay_out + _delay_in));
	_delayline.get()->set_delay(_delay_out + _delay_in);
}

void
Send::run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, double speed, pframes_t nframes, bool)
{
	if (_output->n_ports() == ChanCount::ZERO) {
		_meter->reset ();
		_active = _pending_active;
		return;
	}

	if (!_active && !_pending_active) {
		_meter->reset ();
		_output->silence (nframes);
		_active = _pending_active;
		return;
	}

	// we have to copy the input, because deliver_output() may alter the buffers
	// in-place, which a send must never do.

	BufferSet& sendbufs = _session.get_mix_buffers (bufs.count());
	sendbufs.read_from (bufs, nframes);
	assert(sendbufs.count() == bufs.count());

	/* gain control */

	_amp->set_gain_automation_buffer (_session.send_gain_automation_buffer ());
	_amp->setup_gain_automation (start_frame, end_frame, nframes);
	_amp->run (sendbufs, start_frame, end_frame, speed, nframes, true);

	_delayline->run (sendbufs, start_frame, end_frame, speed, nframes, true);

	/* deliver to outputs */

	Delivery::run (sendbufs, start_frame, end_frame, speed, nframes, true);

	/* consider metering */

	if (_metering) {
		if (_amp->gain_control()->get_value() == 0) {
			_meter->reset();
		} else {
			_meter->run (*_output_buffers, start_frame, end_frame, speed, nframes, true);
		}
	}

	/* _active was set to _pending_active by Delivery::run() */
}

XMLNode&
Send::get_state(void)
{
	return state (true);
}

XMLNode&
Send::state (bool full)
{
	XMLNode& node = Delivery::state(full);

	node.set_property ("type", "send");

	if (_role != Listen) {
		node.set_property ("bitslot", _bitslot);
	}

	node.set_property ("selfdestruct", _remove_on_disconnect);

	node.add_child_nocopy (_amp->state (full));

	return node;
}

int
Send::set_state (const XMLNode& node, int version)
{
	if (version < 3000) {
		return set_state_2X (node, version);
	}

	XMLProperty const * prop;

	Delivery::set_state (node, version);

	if (node.property ("ignore-bitslot") == 0) {

		/* don't try to reset bitslot if there is a node for it already: this can cause
		   issues with the session's accounting of send ID's
		*/

		if ((prop = node.property ("bitslot")) == 0) {
			if (_role == Delivery::Aux) {
				_bitslot = _session.next_aux_send_id ();
			} else if (_role == Delivery::Send) {
				_bitslot = _session.next_send_id ();
			} else {
				// bitslot doesn't matter but make it zero anyway
				_bitslot = 0;
			}
		} else {
			if (_role == Delivery::Aux) {
				_session.unmark_aux_send_id (_bitslot);
				_bitslot = string_to<uint32_t>(prop->value());
				_session.mark_aux_send_id (_bitslot);
			} else if (_role == Delivery::Send) {
				_session.unmark_send_id (_bitslot);
				_bitslot = string_to<uint32_t>(prop->value());
				_session.mark_send_id (_bitslot);
			} else {
				// bitslot doesn't matter but make it zero anyway
				_bitslot = 0;
			}
		}
	}

	node.get_property (X_("selfdestruct"), _remove_on_disconnect);

	XMLNodeList nlist = node.children();
	for (XMLNodeIterator i = nlist.begin(); i != nlist.end(); ++i) {
		if ((*i)->name() == X_("Processor")) {
			_amp->set_state (**i, version);
		}
	}

	return 0;
}

int
Send::set_state_2X (const XMLNode& node, int /* version */)
{
	/* use the IO's name for the name of the send */
	XMLNodeList const & children = node.children ();

	XMLNodeList::const_iterator i = children.begin();
	while (i != children.end() && (*i)->name() != X_("Redirect")) {
		++i;
	}

	if (i == children.end()) {
		return -1;
	}

	XMLNodeList const & grand_children = (*i)->children ();
	XMLNodeList::const_iterator j = grand_children.begin ();
	while (j != grand_children.end() && (*j)->name() != X_("IO")) {
		++j;
	}

	if (j == grand_children.end()) {
		return -1;
	}

	XMLProperty const * prop = (*j)->property (X_("name"));
	if (!prop) {
		return -1;
	}

	set_name (prop->value ());

	return 0;
}

bool
Send::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	/* sends have no impact at all on the channel configuration of the
	   streams passing through the route. so, out == in.
	*/

	out = in;
	return true;
}

/** Caller must hold process lock */
bool
Send::configure_io (ChanCount in, ChanCount out)
{
	if (!_amp->configure_io (in, out)) {
		return false;
	}

	if (!Processor::configure_io (in, out)) {
		return false;
	}

	if (!_meter->configure_io (ChanCount (DataType::AUDIO, pan_outs()), ChanCount (DataType::AUDIO, pan_outs()))) {
		return false;
	}

	if (_delayline && !_delayline->configure_io(in, out)) {
		cerr << "send delayline config failed\n";
		return false;
	}

	reset_panner ();

	return true;
}

void
Send::panshell_changed ()
{
	_meter->configure_io (ChanCount (DataType::AUDIO, pan_outs()), ChanCount (DataType::AUDIO, pan_outs()));
}

bool
Send::set_name (const string& new_name)
{
	string unique_name;

	if (_role == Delivery::Send) {
		char buf[32];

		/* rip any existing numeric part of the name, and append the bitslot
		 */

		string::size_type last_letter = new_name.find_last_not_of ("0123456789");

		if (last_letter != string::npos) {
			unique_name = new_name.substr (0, last_letter + 1);
		} else {
			unique_name = new_name;
		}

		snprintf (buf, sizeof (buf), "%u", (_bitslot + 1));
		unique_name += buf;

	} else {
		unique_name = new_name;
	}

	return Delivery::set_name (unique_name);
}

bool
Send::display_to_user () const
{
	/* we ignore Deliver::_display_to_user */

	if (_role == Listen) {
                /* don't make the monitor/control/listen send visible */
		return false;
	}

	return true;
}

string
Send::value_as_string (boost::shared_ptr<const AutomationControl> ac) const
{
	return _amp->value_as_string (ac);
}

void
Send::snd_output_changed (IOChange change, void* /*src*/)
{
	if (change.type & IOChange::ConnectionsChanged) {
		if (!_output->connected() && _remove_on_disconnect) {
			_remove_on_disconnect = false;
			SelfDestruct (); /* EMIT SIGNAL */
		}
	}
}
