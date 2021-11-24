/*
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
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

#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/types_convert.h"

#include "ardour/amp.h"
#include "ardour/audio_buffer.h"
#include "ardour/audioengine.h"
#include "ardour/delayline.h"
#include "ardour/internal_return.h"
#include "ardour/internal_send.h"
#include "ardour/meter.h"
#include "ardour/panner_shell.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/solo_isolate_control.h"

#include "pbd/i18n.h"

namespace ARDOUR {
	class MuteMaster;
	class Pannable;
}

using namespace PBD;
using namespace ARDOUR;
using namespace std;

PBD::Signal1<void, pframes_t> InternalSend::CycleStart;

InternalSend::InternalSend (Session&                      s,
                            boost::shared_ptr<Pannable>   p,
                            boost::shared_ptr<MuteMaster> mm,
                            boost::shared_ptr<Route>      sendfrom,
                            boost::shared_ptr<Route>      sendto,
                            Delivery::Role                role,
                            bool                          ignore_bitslot)
	: Send (s, p, mm, role, ignore_bitslot)
	, _send_from (sendfrom)
	, _allow_feedback (false)
{
	if (sendto) {
		if (use_target (sendto)) {
			throw failed_constructor ();
		}
	}

	init_gain ();

	_send_from->DropReferences.connect_same_thread (source_connection, boost::bind (&InternalSend::send_from_going_away, this));
	CycleStart.connect_same_thread (*this, boost::bind (&InternalSend::cycle_start, this, _1));
}

InternalSend::~InternalSend ()
{
	propagate_solo ();
	if (_send_to) {
		_send_to->remove_send_from_internal_return (this);
	}
}

void
InternalSend::propagate_solo ()
{
	if (_session.inital_connect_or_deletion_in_progress ()) {
		return;
	}
	if (!_send_to || !_send_from) {
		return;
	}

	/* cache state before modification */
	bool from_soloed            = _send_from->soloed();
	bool to_soloed              = _send_to->soloed();
	bool from_soloed_downstream = _send_from->solo_control()->soloed_by_others_downstream();
	bool to_soloed_upstream     = _send_to->solo_control()->soloed_by_others_upstream();
	bool to_isolated_upstream   = _send_to->solo_isolate_control()->solo_isolated_by_upstream();

	if (from_soloed && (to_soloed_upstream | to_isolated_upstream)) {
		if (to_soloed_upstream) {
			_send_to->solo_control()->mod_solo_by_others_upstream (-1);
		}
		if (to_isolated_upstream) {
			_send_to->solo_isolate_control()->mod_solo_isolated_by_upstream (-1);
		}
		/* propagate further downstream alike Route::input_change_handler() */
		boost::shared_ptr<RouteList> routes = _session.get_routes ();
		for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
			if ((*i) == _send_to || (*i)->is_master() || (*i)->is_monitor() || (*i)->is_auditioner()) {
				continue;
			}
			bool does_feed = _send_to->feeds (*i);
			if (does_feed && to_soloed_upstream) {
				(*i)->solo_control()->mod_solo_by_others_upstream (-1);
			}
			if (does_feed && to_isolated_upstream) {
				(*i)->solo_isolate_control()->mod_solo_isolated_by_upstream (-1);
			}
		}
	}
	if (to_soloed && from_soloed_downstream) {
		_send_from->solo_control()->mod_solo_by_others_downstream (-1);

		/* propagate further upstream alike Route::output_change_handler() */
		boost::shared_ptr<RouteList> routes = _session.get_routes ();
		for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
			if (*i == _send_from || !(*i)->can_solo()) {
				continue;
			}
			if ((*i)->feeds (_send_from)) {
				(*i)->solo_control()->mod_solo_by_others_downstream (-1);
			}
		}
	}
}

void
InternalSend::init_gain ()
{
	if (_role == Listen) {
		/* send to monitor bus is always at unity */
		_gain_control->set_value (GAIN_COEFF_UNITY, PBD::Controllable::NoGroup);
	} else {
		/* aux sends start at -inf dB */
		_gain_control->set_value (GAIN_COEFF_ZERO, PBD::Controllable::NoGroup);
	}
}

int
InternalSend::use_target (boost::shared_ptr<Route> sendto, bool update_name)
{
	if (_send_to) {
		propagate_solo ();
		_send_to->remove_send_from_internal_return (this);
	}

	_send_to = sendto;

	_send_to->add_send_to_internal_return (this);

	ensure_mixbufs ();
	mixbufs.set_count (_send_to->internal_return ()->input_streams ());

	_meter->configure_io (_send_to->internal_return ()->input_streams (), _send_to->internal_return ()->input_streams ());

	_send_delay->configure_io (_send_to->internal_return ()->input_streams (), _send_to->internal_return ()->input_streams ());

	reset_panner ();

	if (update_name) {
		set_name (sendto->name ());
	}
	_send_to_id = _send_to->id ();

	target_connections.drop_connections ();

	_send_to->DropReferences.connect_same_thread (target_connections, boost::bind (&InternalSend::send_to_going_away, this));
	_send_to->PropertyChanged.connect_same_thread (target_connections, boost::bind (&InternalSend::send_to_property_changed, this, _1));
	_send_to->io_changed.connect_same_thread (target_connections, boost::bind (&InternalSend::target_io_changed, this));

	return 0;
}

void
InternalSend::target_io_changed ()
{
	assert (_send_to);
	ensure_mixbufs ();
	mixbufs.set_count (_send_to->internal_return ()->input_streams ());
	reset_panner ();
}

void
InternalSend::send_from_going_away ()
{
	/* notify route while source-route is still available,
	 * signal emission in the d'tor is too late */
	propagate_solo ();
	_send_from.reset ();
}

void
InternalSend::send_to_going_away ()
{
	target_connections.drop_connections ();
	_send_to.reset ();
	_send_to_id = "0";
}

void
InternalSend::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool)
{
	if (!check_active() || !_send_to) {
		_meter->reset ();
		return;
	}

	/* we have to copy the input, because we may alter the buffers with the amp
	 * in-place, which a send must never do.
	 */

	if (_panshell && !_panshell->bypassed () && role () != Listen) {
		if (mixbufs.count ().n_audio () > 0) {
			_panshell->run (bufs, mixbufs, start_sample, end_sample, nframes);
		}

		/* non-audio data will not have been copied by the panner, do it now
		 * if there are more buffers available than send buffers, ignore them,
		 * if there are less, copy the last as IO::copy_to_output does. */

		for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
			if (*t != DataType::AUDIO) {
				BufferSet::iterator o = mixbufs.begin (*t);
				BufferSet::iterator i = bufs.begin (*t);

				while (i != bufs.end (*t) && o != mixbufs.end (*t)) {
					o->read_from (*i, nframes);
					++i;
					++o;
				}
				while (o != mixbufs.end (*t)) {
					o->silence (nframes, 0);
					++o;
				}
			}
		}
	} else if (role () == Listen) {
		/* We're going to the monitor bus, so discard MIDI data */

		uint32_t const bufs_audio    = bufs.count ().get (DataType::AUDIO);
		uint32_t const mixbufs_audio = mixbufs.count ().get (DataType::AUDIO);

		/* monitor-section has same number of channels as master-bus (on creation).
		 *
		 * There is no clear answer what should happen when trying to PFL or AFL
		 * a track that has more channels (bufs_audio from source-track is
		 * larger than mixbufs).
		 *
		 * There are two options:
		 *  1: discard additional channels    (current)
		 * OR
		 *  2: require the monitor-section to have at least as many channels
		 * as the largest count of any route
		 */
		//assert (mixbufs.available().get (DataType::AUDIO) >= bufs_audio);

		/* Copy bufs into mixbufs, going round bufs more than once if necessary
		 * to ensure that every mixbuf gets some data.
		 */

		uint32_t j = 0;
		uint32_t i = 0;
		for (i = 0; i < mixbufs_audio && j < bufs_audio; ++i) {
			mixbufs.get_audio (i).read_from (bufs.get_audio (j), nframes);
			++j;

			if (j == bufs_audio) {
				j = 0;
			}
		}
		/* in case or MIDI track with 0 audio channels */
		for (; i < mixbufs_audio; ++i) {
			mixbufs.get_audio (i).silence (nframes);
		}

	} else {
		/* no panner or panner is bypassed */
		assert (mixbufs.available () >= bufs.count ());
		/* BufferSet::read_from() changes the channel-conut,
		 * so we manually copy bufs -> mixbufs
		 */
		for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
			/* iterate over outputs */
			BufferSet::iterator i = bufs.begin (*t);
			for (BufferSet::iterator o = mixbufs.begin (*t); o != mixbufs.end (*t); ++o) {
				if (i == bufs.end (*t)) {
					o->silence (nframes, 0);
				} else {
					o->read_from (*i, nframes);
					++i;
				}
			}
		}
	}

	/* main gain control: * mute & bypass/enable */
	gain_t tgain = target_gain ();

	if (tgain != _current_gain) {
		/* target gain has changed, fade in/out */
		_current_gain = Amp::apply_gain (mixbufs, _session.nominal_sample_rate (), nframes, _current_gain, tgain);
	} else if (tgain == GAIN_COEFF_ZERO) {
		/* we were quiet last time, and we're still supposed to be quiet. */
		_meter->reset ();
		Amp::apply_simple_gain (mixbufs, nframes, GAIN_COEFF_ZERO);
		return;
	} else if (tgain != GAIN_COEFF_UNITY) {
		/* target gain has not changed, but is not zero or unity */
		Amp::apply_simple_gain (mixbufs, nframes, tgain);
	}

	/* apply fader gain automation */
	_amp->set_gain_automation_buffer (_session.send_gain_automation_buffer ());
	_amp->setup_gain_automation (start_sample, end_sample, nframes);
	_amp->run (mixbufs, start_sample, end_sample, speed, nframes, true);

	_send_delay->run (mixbufs, start_sample, end_sample, speed, nframes, true);

	/* consider metering */
	if (_metering) {
		if (_amp->gain_control ()->get_value () == GAIN_COEFF_ZERO) {
			_meter->reset ();
		} else {
			_meter->run (mixbufs, start_sample, end_sample, speed, nframes, true);
		}
	}

	_thru_delay->run (bufs, start_sample, end_sample, speed, nframes, true);

	/* target will pick up our output when it is ready */
}

void
InternalSend::ensure_mixbufs ()
{
	for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
		size_t size = (*t == DataType::MIDI) ? _session.engine ().raw_buffer_size (*t) : _session.get_block_size ();
		mixbufs.ensure_buffers (*t, _send_to->internal_return ()->input_streams ().get (*t), size);
	}
}

int
InternalSend::set_block_size (pframes_t)
{
	if (_send_to) {
		ensure_mixbufs ();
	}

	return 0;
}

void
InternalSend::set_allow_feedback (bool yn)
{
	if (is_foldback ()) {
		return;
	}
	_allow_feedback = yn;
	_send_from->processors_changed (RouteProcessorChange ()); /* EMIT SIGNAL */
}

bool
InternalSend::feeds (boost::shared_ptr<Route> other) const
{
	if (_role == Listen || !_allow_feedback) {
		return _send_to == other;
	}
	return false;
}

XMLNode&
InternalSend::state ()
{
	XMLNode& node (Send::state ());

	/* this replaces any existing "type" property */

	node.set_property ("type", "intsend");

	if (_send_to) {
		node.set_property ("target", _send_to->id ());
	}
	node.set_property ("allow-feedback", _allow_feedback);

	return node;
}

int
InternalSend::set_state (const XMLNode& node, int version)
{
	init_gain ();

	/* Allow Delivery::set_state() to restore pannable state when
	 * copy/pasting Aux sends.
	 *
	 * At this point in time there is no target-bus. So when
	 * Delivery::set_state() calls reset_panner(), the pannable
	 * is dropped, before the panner state can be restored.
	 */
	defer_pan_reset ();

	Send::set_state (node, version);

	if (node.get_property ("target", _send_to_id)) {
		/* if we're loading a session, the target route may not have been
		 * create yet. make sure we defer till we are sure that it should
		 * exist.
		 */

		if (_session.loading()) {
			Session::AfterConnect.connect_same_thread (connect_c, boost::bind (&InternalSend::after_connect, this));
		} else {
			after_connect ();
		}
	}

	allow_pan_reset ();

	if (!is_foldback ()) {
		node.get_property (X_("allow-feedback"), _allow_feedback);
	} else {
		_allow_feedback = false;
	}

	return 0;
}

int
InternalSend::after_connect ()
{
	connect_c.disconnect ();

	if (_send_to_id == "0") {
		/* it vanished before we could connect */
		return 0;
	}

	boost::shared_ptr<Route> sendto;

	if ((sendto = _session.route_by_id (_send_to_id)) == 0) {
		error << string_compose (_("%1 - cannot find any track/bus with the ID %2 to connect to"), display_name (), _send_to_id) << endmsg;
		cerr << string_compose (_("%1 - cannot find any track/bus with the ID %2 to connect to"), display_name (), _send_to_id) << endl;
		return -1;
	}

	return use_target (sendto, false);
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
	 * sending to, if anything.
	 */

	if (_send_to) {
		return _send_to->internal_return ()->input_streams ().n_audio ();
	}

	/* zero is more accurate, but 1 is probably safer as a way to
	 * say "don't pan"
	 */
	return 1;
}

bool
InternalSend::configure_io (ChanCount in, ChanCount out)
{
	bool ret = Send::configure_io (in, out);
	set_block_size (_session.engine ().samples_per_cycle ());
	return ret;
}

bool
InternalSend::set_name (const string& str)
{
	/* rules for external sends don't apply to us */
	return Delivery::set_name (str);
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
	for (BufferSet::audio_iterator b = mixbufs.audio_begin (); b != mixbufs.audio_end (); ++b) {
		b->prepare ();
	}
}
