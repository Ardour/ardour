/*
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Len Ovens <len@ovenwerks.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <iostream>
#include <algorithm>

#include "pbd/xml++.h"

#include "ardour/amp.h"
#include "ardour/boost_debug.h"
#include "ardour/buffer_set.h"
#include "ardour/debug.h"
#include "ardour/delayline.h"
#include "ardour/event_type_map.h"
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

PBD::Signal0<void> LatentSend::ChangedLatency;

LatentSend::LatentSend ()
		: _delay_in (0)
		, _delay_out (0)
{
}

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
		return string_compose (_("aux %1"), (bitslot = s.next_aux_send_id ()));
	case Delivery::Listen:
		bitslot = 0; /* unused */
		return _("listen"); // no ports, no need for numbering
	case Delivery::Send:
		return string_compose (_("send %1"), (bitslot = s.next_send_id ()));
	case Delivery::Foldback:
		return string_compose (_("foldback %1"), (bitslot = s.next_aux_send_id ()));
	default:
		fatal << string_compose (_("programming error: send created using role %1"), enum_2_string (r)) << endmsg;
		abort(); /*NOTREACHED*/
		return string();
	}

}

Send::Send (Session& s, boost::shared_ptr<Pannable> p, boost::shared_ptr<MuteMaster> mm, Role r, bool ignore_bitslot)
	: Delivery (s, p, mm, name_and_id_new_send (s, r, _bitslot, ignore_bitslot), r)
	, _metering (false)
	, _remove_on_disconnect (false)
{
	//boost_debug_shared_ptr_mark_interesting (this, "send");

#warning NUTEMPO question what time domain should this use?
	boost::shared_ptr<AutomationList> gl (new AutomationList (Evoral::Parameter (BusSendLevel), Temporal::AudioTime));
	_gain_control = boost::shared_ptr<GainControl> (new GainControl (_session, Evoral::Parameter(BusSendLevel), gl));
	_gain_control->set_flag (Controllable::InlineControl);
	add_control (_gain_control);

	_amp.reset (new Amp (_session, _("Fader"), _gain_control, true));
	_meter.reset (new PeakMeter (_session, name()));

	_send_delay.reset (new DelayLine (_session, "Send-" + name()));
	_thru_delay.reset (new DelayLine (_session, "Thru-" + name()));

	if (panner_shell()) {
		panner_shell()->Changed.connect_same_thread (*this, boost::bind (&Send::panshell_changed, this));
		panner_shell()->PannableChanged.connect_same_thread (*this, boost::bind (&Send::pannable_changed, this));
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

samplecnt_t
Send::signal_latency () const
{
	if (!_pending_active) {
		 return 0;
	}
	if (_delay_out > _delay_in) {
		return _delay_out - _delay_in;
	}
	return 0;
}

void
Send::update_delaylines ()
{
	if (_role == Listen) {
		/* Don't align monitor-listen (just yet).
		 * They're present on each route, may change positions
		 * and could potentially signficiantly increase worst-case
		 * Latency: In PFL mode all tracks/busses would additionally be
		 * aligned at PFL position.
		 *
		 * We should only align active monitor-sends when at least one is active.
		 */
		return;
	}

	bool changed;
	if (_delay_out > _delay_in) {
		changed = _thru_delay->set_delay(_delay_out - _delay_in);
		_send_delay->set_delay(0);
	} else {
		changed = _thru_delay->set_delay(0);
		_send_delay->set_delay(_delay_in - _delay_out);
	}

	if (changed) {
		// TODO -- ideally postpone for effective no-op changes
		// (in case both  _delay_out and _delay_in are changed by the
		// same amount in a single latency-update cycle).
		ChangedLatency (); /* EMIT SIGNAL */
	}
}

void
Send::set_delay_in (samplecnt_t delay)
{
	if (_delay_in == delay) {
		return;
	}
	_delay_in = delay;

	DEBUG_TRACE (DEBUG::LatencyDelayLine,
			string_compose ("Send::set_delay_in %1: (%2) - %3 = %4\n",
				name (), _delay_in, _delay_out, _delay_in - _delay_out));

	update_delaylines ();
}

void
Send::set_delay_out (samplecnt_t delay, size_t /*bus*/)
{
	if (_delay_out == delay) {
		return;
	}
	_delay_out = delay;
	DEBUG_TRACE (DEBUG::LatencyDelayLine,
			string_compose ("Send::set_delay_out %1: %2 - (%3) = %4\n",
				name (), _delay_in, _delay_out, _delay_in - _delay_out));

	update_delaylines ();
}

void
Send::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool)
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
	_amp->setup_gain_automation (start_sample, end_sample, nframes);
	_amp->run (sendbufs, start_sample, end_sample, speed, nframes, true);

	_send_delay->run (sendbufs, start_sample, end_sample, speed, nframes, true);

	/* deliver to outputs */

	Delivery::run (sendbufs, start_sample, end_sample, speed, nframes, true);

	/* consider metering */

	if (_metering) {
		if (_amp->gain_control()->get_value() == 0) {
			_meter->reset();
		} else {
			_meter->run (*_output_buffers, start_sample, end_sample, speed, nframes, true);
		}
	}

	_thru_delay->run (bufs, start_sample, end_sample, speed, nframes, true);

	/* _active was set to _pending_active by Delivery::run() */
}

XMLNode&
Send::state ()
{
	XMLNode& node = Delivery::state ();

	node.set_property ("type", "send");

	if (_role != Listen) {
		node.set_property ("bitslot", _bitslot);
	}

	node.set_property ("selfdestruct", _remove_on_disconnect);

	node.add_child_nocopy (_gain_control->get_state());

	return node;
}

int
Send::set_state (const XMLNode& node, int version)
{
	if (version < 3000) {
		return set_state_2X (node, version);
	}

	XMLNode* gain_node;

	if ((gain_node = node.child (Controllable::xml_node_name.c_str ())) != 0) {
		_gain_control->set_state (*gain_node, version);
	}

	if (version <= 6000) {
		XMLNode const* nn = &node;

#ifdef MIXBUS
		/* This was also broken in mixbus 6.0 */
		if (version <= 6000)
#else
		/* version 5: Gain Control was owned by the Amp */
		if (version < 6000)
#endif
		{
			XMLNode* processor = node.child ("Processor");
			if (processor) {
				nn = processor;
				if ((gain_node = nn->child (Controllable::xml_node_name.c_str ())) != 0) {
					_gain_control->set_state (*gain_node, version);
					_gain_control->set_flags (Controllable::InlineControl);
				}
			}
		}

		/* convert GainAutomation to BusSendLevel
		 *
		 * (early Ardour 6.0-pre0 and Mixbus 6.0 used "BusSendLevel"
		 *  control with GainAutomation, so we check version <= 6000.
		 *  New A6 sessions do not have a GainAutomation parameter,
		 *  so this is safe.)
		 *
		 * Normally this is restored via
		 * Delivery::set_state() -> Processor::set_state()
		 * -> Automatable::set_automation_xml_state()
		 */
		XMLNodeList nlist;
		XMLNode* automation = nn->child ("Automation");
		if (automation) {
			nlist = automation->children();
		} else if (0 != (automation = node.child ("Automation"))) {
			nlist = automation->children();
		}
		for (XMLNodeIterator i = nlist.begin(); i != nlist.end(); ++i) {
			if ((*i)->name() != "AutomationList") {
				continue;
			}
			XMLProperty const* id_prop = (*i)->property("automation-id");
			if (!id_prop) {
				continue;
			}
			Evoral::Parameter param = EventTypeMap::instance().from_symbol (id_prop->value());
			if (param.type() != GainAutomation) {
				continue;
			}
			XMLNode xn (**i);
			xn.set_property ("automation-id", EventTypeMap::instance().to_symbol(Evoral::Parameter (BusSendLevel)));
			_gain_control->alist()->set_state (xn, version);
			break;
		}
	}

	Delivery::set_state (node, version);

	if (node.property ("ignore-bitslot") == 0) {
		XMLProperty const* prop;

		/* don't try to reset bitslot if there is a node for it already: this can cause
		   issues with the session's accounting of send ID's
		*/

		if ((prop = node.property ("bitslot")) == 0) {
			if (_role == Delivery::Aux || _role == Delivery::Foldback) {
				_bitslot = _session.next_aux_send_id ();
			} else if (_role == Delivery::Send) {
				_bitslot = _session.next_send_id ();
			} else {
				// bitslot doesn't matter but make it zero anyway
				_bitslot = 0;
			}
		} else {
			if (_role == Delivery::Aux || _role == Delivery::Foldback) {
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

	_send_delay->set_name ("Send-" + name());
	_thru_delay->set_name ("Thru-" + name());

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
Send::has_panner () const
{
	/* see InternalSend::run() and Delivery::run */
	if (_panshell && role () != Listen && _panshell->panner()) {
		return true; // !_panshell->bypassed ()
	}
	return false;
}

bool
Send::panner_linked_to_route () const
{
	return _panshell ? _panshell->is_linked_to_route() : false;
}

void
Send::set_panner_linked_to_route (bool onoff) {
	if (_panshell) {
		_panshell->set_linked_to_route (onoff);
	}
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

	if (!_thru_delay->configure_io (in, out)) {
		return false;
	}

	if (!_send_delay->configure_io (ChanCount (DataType::AUDIO, pan_outs()), ChanCount (DataType::AUDIO, pan_outs()))) {
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

void
Send::pannable_changed ()
{
	PropertyChanged (PBD::PropertyChange ()); /* EMIT SIGNAL */
}

bool
Send::set_name (const string& new_name)
{
	string unique_name;

	if (_role == Delivery::Send) {
		unique_name = validate_name (new_name, string_compose (_("send %1"), _bitslot));

		if (unique_name.empty ()) {
			return false;
		}
	} else {
		unique_name = new_name;
	}

	return Delivery::set_name (unique_name);
}

bool
Send::display_to_user () const
{
	/* we ignore Deliver::_display_to_user */

	if (_role == Listen || _role == Foldback) {
		/* don't make the monitor/control/listen send visible */
		return false;
	}

	return true;
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
