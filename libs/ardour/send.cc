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
#include "pbd/boost_debug.h"

#include "ardour/amp.h"
#include "ardour/send.h"
#include "ardour/session.h"
#include "ardour/port.h"
#include "ardour/audio_port.h"
#include "ardour/buffer_set.h"
#include "ardour/meter.h"
#include "ardour/panner.h"
#include "ardour/io.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

string
Send::name_and_id_new_send (Session& s, Role r, uint32_t& bitslot)
{
	if (r == Role (0)) {
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
		/*NOTREACHED*/
		return string();
	}
	
}

Send::Send (Session& s, boost::shared_ptr<Pannable> p, boost::shared_ptr<MuteMaster> mm, Role r)
	: Delivery (s, p, mm, name_and_id_new_send (s, r, _bitslot), r)
	, _metering (false)
{
	if (_role == Listen) {
		/* we don't need to do this but it keeps things looking clean
		   in a debugger. _bitslot is not used by listen sends.
		*/
		_bitslot = 0;
	}

	boost_debug_shared_ptr_mark_interesting (this, "send");

	_amp.reset (new Amp (_session));
	_meter.reset (new PeakMeter (_session));

	add_control (_amp->gain_control ());
}

Send::Send (Session& s, const std::string& name, uint32_t bslot, boost::shared_ptr<Pannable> p, boost::shared_ptr<MuteMaster> mm, Role r)
	: Delivery (s, p, mm, name, r)
	, _metering (false)
	, _bitslot (bslot)
{
	if (_role == Listen) {
		/* we don't need to do this but it keeps things looking clean
		   in a debugger. _bitslot is not used by listen sends.
		*/
		_bitslot = 0;
	}

	boost_debug_shared_ptr_mark_interesting (this, "send");

	_amp.reset (new Amp (_session));
	_meter.reset (new PeakMeter (_session));

	add_control (_amp->gain_control ());
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
Send::run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, pframes_t nframes, bool)
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

	// Can't automate gain for sends or returns yet because we need different buffers
	// so that we don't overwrite the main automation data for the route amp
	// _amp->setup_gain_automation (start_frame, end_frame, nframes);
	_amp->run (sendbufs, start_frame, end_frame, nframes, true);

	/* deliver to outputs */

	Delivery::run (sendbufs, start_frame, end_frame, nframes, true);

	/* consider metering */

	if (_metering) {
		if (_amp->gain_control()->get_value() == 0) {
			_meter->reset();
		} else {
			_meter->run (*_output_buffers, start_frame, end_frame, nframes, true);
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
	char buf[32];

	node.add_property ("type", "send");
	snprintf (buf, sizeof (buf), "%" PRIu32, _bitslot);

	if (_role != Listen) {
		node.add_property ("bitslot", buf);
	}

	node.add_child_nocopy (_amp->state (full));

	return node;
}

int
Send::set_state (const XMLNode& node, int version)
{
	if (version < 3000) {
		return set_state_2X (node, version);
	}

	const XMLProperty* prop;

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
				sscanf (prop->value().c_str(), "%" PRIu32, &_bitslot);
				_session.mark_aux_send_id (_bitslot);
			} else if (_role == Delivery::Send) {
				_session.unmark_send_id (_bitslot);
				sscanf (prop->value().c_str(), "%" PRIu32, &_bitslot);
				_session.mark_send_id (_bitslot);
			} else {
				// bitslot doesn't matter but make it zero anyway
				_bitslot = 0;
			}
		}
	}
	
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
Send::can_support_io_configuration (const ChanCount& in, ChanCount& out) const
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
	if (!_amp->configure_io (in, out) || !_meter->configure_io (in, out)) {
		return false;
	}

	if (!Processor::configure_io (in, out)) {
		return false;
	}

	reset_panner ();

	return true;
}

/** Set up the XML description of a send so that we will not
 *  reset its name or bitslot during ::set_state()
 *  @param state XML send state.
 *  @param session Session.
 */
void
Send::make_unique (XMLNode &state)
{
	state.add_property ("ignore-bitslot", "1");
	state.add_property ("ignore-name", "1");
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
Send::value_as_string (boost::shared_ptr<AutomationControl> ac) const
{
	return _amp->value_as_string (ac);
}

	
