/*
 * Copyright (C) 2023 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2023 Robin Gareus <robin@gareus.org>
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

#include "pbd/unwind.h"

#include "ardour/surround_send.h"
#include "ardour/amp.h"
#include "ardour/audioengine.h"
#include "ardour/buffer.h"
#include "ardour/delayline.h"
#include "ardour/gain_control.h"
#include "ardour/internal_send.h"
#include "ardour/surround_pannable.h"
#include "ardour/route.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

SurroundSend::SurroundSend (Session& s, std::shared_ptr<MuteMaster> mm)
	: Processor (s, _("Surround"), Temporal::TimeDomainProvider (Temporal::AudioTime))
	, _surround_id (s.next_surround_send_id ())
	, _current_gain (GAIN_COEFF_ZERO)
	, _has_state (false)
	, _ignore_enable_change (false)
	, _mute_master (mm)

{
	_send_delay.reset (new DelayLine (_session, "Send-" + name ()));
	_thru_delay.reset (new DelayLine (_session, "Thru-" + name ()));

	std::shared_ptr<AutomationList> gl (new AutomationList (Evoral::Parameter (SurroundSendLevel), *this));
	_gain_control = std::shared_ptr<GainControl> (new GainControl (_session, Evoral::Parameter (SurroundSendLevel), gl));
	_amp.reset (new Amp (_session, _("Surround"), _gain_control, false));
	_amp->activate ();

	_gain_control->set_flag (PBD::Controllable::InlineControl);
	//_gain_control->set_value (GAIN_COEFF_ZERO, PBD::Controllable::NoGroup);

	add_control (_gain_control);

	_send_enable_control = std::shared_ptr<AutomationControl> (new AutomationControl (_session, BusSendEnable, ParameterDescriptor(BusSendEnable)));
	_send_enable_control->Changed.connect_same_thread (*this, boost::bind (&SurroundSend::send_enable_changed, this));
	_send_enable_control->clear_flag (PBD::Controllable::RealTime);

	ActiveChanged.connect_same_thread (*this, boost::bind (&SurroundSend::proc_active_changed, this));

	InternalSend::CycleStart.connect_same_thread (*this, boost::bind (&SurroundSend::cycle_start, this, _1));
}

SurroundSend::~SurroundSend ()
{
	_send_enable_control->drop_references ();
}

std::shared_ptr<SurroundPannable>
SurroundSend::pannable (size_t chn) const
{
	return _pannable[chn];
}

std::shared_ptr<SurroundPannable> const&
SurroundSend::pan_param (size_t chn, timepos_t& s, timepos_t& e) const
{
	s = _cycle_start;
	e = _cycle_end;
	return _pannable[chn];
}


gain_t
SurroundSend::target_gain () const
{
	return _mute_master->mute_gain_at (MuteMaster::SurroundSend);
}

void
SurroundSend::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool)
{
	automation_run (start_sample, nframes);

	if (!check_active ()) {
		_mixbufs.silence (nframes, 0);
		return;
	}

	/* Copy inputs to mixbufs, since (a) we may need to adjust gain (b) the
	 * contents need to be available for the Surround return (later)
	 */

	BufferSet::iterator o = _mixbufs.begin (DataType::AUDIO);
	BufferSet::iterator i = bufs.begin (DataType::AUDIO);

	for (; i != bufs.end (DataType::AUDIO) && o != _mixbufs.end (DataType::AUDIO); ++i, ++o) {
		o->read_from (*i, nframes);
	}

	/* main gain control: * mute & bypass/enable */
	gain_t tgain = target_gain ();

	if (tgain != _current_gain) {
		/* target gain has changed, fade in/out */
		_current_gain = Amp::apply_gain (_mixbufs, _session.nominal_sample_rate (), nframes, _current_gain, tgain);
	} else if (tgain == GAIN_COEFF_ZERO) {
		/* we were quiet last time, and we're still supposed to be quiet. */
		Amp::apply_simple_gain (_mixbufs, nframes, GAIN_COEFF_ZERO);
		return;
	} else if (tgain != GAIN_COEFF_UNITY) {
		/* target gain has not changed, but is not zero or unity */
		Amp::apply_simple_gain (_mixbufs, nframes, tgain);
	}

	/* apply fader gain automation */
	_amp->set_gain_automation_buffer (_session.send_gain_automation_buffer ());
	_amp->setup_gain_automation (start_sample, end_sample, nframes);
	_amp->run (_mixbufs, start_sample, end_sample, speed, nframes, true);

	_send_delay->run (_mixbufs, start_sample, end_sample, speed, nframes, true);

	for (uint32_t chn = 0; chn < n_pannables (); ++ chn) {
		_pannable[chn]->automation_run (start_sample, nframes);
	}

	_cycle_start = timepos_t (start_sample);
	_cycle_end   = timepos_t (end_sample);

	_thru_delay->run (bufs, start_sample, end_sample, speed, nframes, true);
}

void
SurroundSend::set_delay_in (samplecnt_t delay)
{
	if (_delay_in == delay) {
		return;
	}
	_delay_in = delay;
	update_delaylines (false);
}

void
SurroundSend::set_delay_out (samplecnt_t delay, size_t /*bus*/)
{
	if (_delay_out == delay) {
		return;
	}
	_delay_out = delay;
	update_delaylines (true);
}

void
SurroundSend::update_delaylines (bool rt_ok)
{
	if (!rt_ok && AudioEngine::instance ()->running () && AudioEngine::instance ()->in_process_thread ()) {
		if (_delay_out > _delay_in) {
			if (_send_delay->delay () != 0 || _thru_delay->delay () != _delay_out - _delay_in) {
				QueueUpdate (); /* EMIT SIGNAL */
			}
		} else {
			if (_thru_delay->delay () != 0 || _send_delay->delay () != _delay_in - _delay_out) {
				QueueUpdate (); /* EMIT SIGNAL */
			}
		}
		return;
	}

	bool changed;
	if (_delay_out > _delay_in) {
		changed = _thru_delay->set_delay (_delay_out - _delay_in);
		_send_delay->set_delay (0);
	} else {
		changed = _thru_delay->set_delay (0);
		_send_delay->set_delay (_delay_in - _delay_out);
	}

	if (changed && !AudioEngine::instance ()->in_process_thread ()) {
		ChangedLatency (); /* EMIT SIGNAL */
	}
}

samplecnt_t
SurroundSend::signal_latency () const
{
	if (!_pending_active) {
		return 0;
	}
	if (_delay_out > _delay_in) {
		return _delay_out - _delay_in;
	}
	return 0;
}

bool
SurroundSend::display_to_user() const
{
#ifdef MIXBUS
	return false;
#endif
	return true;
}

uint32_t
SurroundSend::n_pannables () const
{
	/* do not use _pannable.size(),
	 * if we would do so, state of removed pannables would be saved.
	 */
#ifdef MIXBUS
	return std::min<uint32_t> (2, _configured_input.n_audio ());
#endif
	return _configured_input.n_audio ();
}

void
SurroundSend::add_pannable ()
{
	std::shared_ptr<SurroundPannable> p = std::shared_ptr<SurroundPannable> (new SurroundPannable (_session, _pannable.size (), Temporal::TimeDomainProvider (Temporal::AudioTime)));

	add_control (p->pan_pos_x);
	add_control (p->pan_pos_y);
	add_control (p->pan_pos_z);
	add_control (p->pan_size);
	add_control (p->pan_snap);
	add_control (p->binaural_render_mode);

	for (uint32_t i = 0; i < _pannable.size (); ++i) {
		_pannable[i]->sync_auto_state_with (p);
		p->sync_auto_state_with (_pannable[i]);
	}

	_pannable.push_back (p);

	_change_connections.drop_connections ();
	for (auto const& c: _controls) {
		std::shared_ptr<AutomationControl> ac = std::dynamic_pointer_cast<AutomationControl>(c.second);
		ac->Changed.connect_same_thread (_change_connections, [this](bool, PBD::Controllable::GroupControlDisposition) { PanChanged (); /* EMIT SIGNAL*/});
	}
}

bool
SurroundSend::configure_io (ChanCount in, ChanCount out)
{
	bool     changed = false;
	uint32_t n_audio = in.n_audio ();

#ifdef MIXBUS
	n_audio = std::min<uint32_t> (2, n_audio);
#endif

	if (_configured) {
		changed = n_audio != n_pannables ();
	}

	while (_pannable.size () < n_audio) {
		add_pannable ();
	}

	if (changed) {
		for (uint32_t i = 0; i < n_audio; ++i) {
			_pannable[i]->foreach_pan_control ([](std::shared_ptr<AutomationControl> ac) { ac->clear_flag (PBD::Controllable::HiddenControl); });
		}
		for (uint32_t i = n_audio; i < _pannable.size (); ++i) {
			_pannable[i]->foreach_pan_control ([](std::shared_ptr<AutomationControl> ac) { ac->set_flag (PBD::Controllable::HiddenControl); });
		}
	}

#ifdef MIXBUS
	/* Link visibility - currently only for Mixbus which has a custom UI, and at most stereo */
	for (uint32_t i = 0; i < _pannable.size (); ++i) {
		_pannable[i]->foreach_pan_control ([](std::shared_ptr<AutomationControl> ac) { ac->clear_visually_linked_control (); });
	}
	/* first link local controls */
	for (uint32_t i = 0; i < n_audio; ++i) {
		_pannable[i]->setup_visual_links ();
	}
	for (uint32_t i = 0; i < n_audio; ++i) {
		for (uint32_t j = 0; j < n_audio; ++j) {
			if (i == j) {
				continue;
			}
			_pannable[i]->sync_visual_link_to (_pannable[j]);
		}
	}
#endif

	if (!_configured && !_has_state) {
		switch (n_audio) {
			case 2:
				_pannable[0]->pan_pos_x->set_value (0.0, PBD::Controllable::NoGroup);
				_pannable[1]->pan_pos_x->set_value (1.0, PBD::Controllable::NoGroup);
				break;
			case 3:
				_pannable[0]->pan_pos_x->set_value (0.0, PBD::Controllable::NoGroup);
				_pannable[1]->pan_pos_x->set_value (1.0, PBD::Controllable::NoGroup);
				_pannable[2]->pan_pos_x->set_value (0.5, PBD::Controllable::NoGroup);
				break;
			case 5:
				_pannable[0]->pan_pos_x->set_value (0.0, PBD::Controllable::NoGroup);
				_pannable[1]->pan_pos_x->set_value (1.0, PBD::Controllable::NoGroup);
				_pannable[2]->pan_pos_x->set_value (0.5, PBD::Controllable::NoGroup);
				_pannable[3]->pan_pos_x->set_value (0.0, PBD::Controllable::NoGroup);
				_pannable[4]->pan_pos_x->set_value (1.0, PBD::Controllable::NoGroup);
				_pannable[3]->pan_pos_y->set_value (1.0, PBD::Controllable::NoGroup);
				_pannable[4]->pan_pos_y->set_value (1.0, PBD::Controllable::NoGroup);
				break;
			default:
				break;
		}
	}

	ChanCount ca (DataType::AUDIO, n_audio);
	_amp->configure_io (ca, ca);

	if (!_send_delay->configure_io (ca, ca)) {
		return false;
	}
	if (!_thru_delay->configure_io (in, out)) {
		return false;
	}

	if (_configured && changed) {
		/* We cannot emit `processors_changed` while holing the `process lock` */
		dynamic_cast<Route*> (_owner)->queue_surround_processors_changed (); /* EMIT SIGNAL */
	}

	Processor::configure_io (in, out); /* may EMIT SIGNAL ConfigurationChanged */

	set_block_size (_session.get_block_size ());

	if (changed) {
		NPannablesChanged (); /* EMIT SIGNAL */
	}
	return true;
}

void
SurroundSend::ensure_mixbufs ()
{
	_mixbufs.ensure_buffers (DataType::AUDIO, n_pannables (), _session.get_block_size ());
}

int
SurroundSend::set_block_size (pframes_t)
{
	ensure_mixbufs ();
	return 0;
}

void
SurroundSend::cycle_start (pframes_t /*nframes*/)
{
	for (BufferSet::audio_iterator b = _mixbufs.audio_begin (); b != _mixbufs.audio_end (); ++b) {
		b->prepare ();
	}
}

std::string
SurroundSend::describe_parameter (Evoral::Parameter param)
{
	if (param.id() >=  n_pannables ()) {
		return X_("hidden");
	}

	if (n_pannables () < 2) {
		/* Use default names */
		return Automatable::describe_parameter (param);
	}

	std::string prefix;
	if (n_pannables () == 2) {
		prefix = string_compose ("[%1]", param.id() == 0 ? S_("Panner|L") : S_("Panner|R"));
	} else {
		prefix = string_compose ("[%1]", 1 + param.id());
	}

	if (param.type() == PanSurroundX) {
		return string_compose("%1 %2", prefix, _("Left/Right"));
	} else if (param.type() == PanSurroundY) {
		return string_compose("%1 %2", prefix, _("Front/Back"));
	} else if (param.type() == PanSurroundZ) {
		return string_compose("%1 %2", prefix, _("Elevation"));
	} else if (param.type() == PanSurroundSize) {
		return string_compose("%1 %2", prefix, _("Object Size"));
	} else if (param.type() == PanSurroundSnap) {
		return string_compose("%1 %2", prefix, _("Snap to Speaker"));
	} else if (param.type() == BinauralRenderMode) {
		return string_compose("%1 %2", prefix, _("Binaural Render mode"));
	}

	return Automatable::describe_parameter (param);
}

void
SurroundSend::send_enable_changed ()
{
	if (_ignore_enable_change) {
		return;
	}
	PBD::Unwinder<bool> uw (_ignore_enable_change, true);
	if (_send_enable_control->get_value () > 0) {
		activate ();
	} else {
		deactivate ();
	}
}

void
SurroundSend::proc_active_changed ()
{
	if (_ignore_enable_change) {
		return;
	}
	PBD::Unwinder<bool> uw (_ignore_enable_change, true);
	_send_enable_control->set_value (_pending_active ? 1 : 0, PBD::Controllable::UseGroup);
}

int
SurroundSend::set_state (const XMLNode& node, int version)
{
	XMLNode* gainnode = node.child (PBD::Controllable::xml_node_name.c_str());
	_gain_control->set_state (*gainnode, version);

	uint32_t npan;
	if (!node.get_property("n-pannables", npan))  {
		return -1;
	}

	while (_pannable.size () < npan) {
		add_pannable ();
	}

	XMLNodeList pans = node.children (X_("SurroundPannable"));
	for (auto const& c: pans) {
		uint32_t chn;
		if (!c->get_property("channel", chn)) {
			continue;
		}
		_pannable[chn]->set_state (*c, version);
	}

	_has_state = true;

	return Processor::set_state (node, version);
}

XMLNode&
SurroundSend::state () const
{
	XMLNode& node (Processor::state ());
	node.set_property ("type", "sursend");
	node.set_property ("n-pannables", n_pannables ());

	node.add_child_nocopy (_gain_control->get_state());
	for (uint32_t chn = 0; chn < n_pannables (); ++ chn) {
		node.add_child_nocopy (_pannable[chn]->get_state ());
	}
	return node;
}
