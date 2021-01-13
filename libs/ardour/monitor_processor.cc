/*
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2010-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015 Ben Loftis <ben@harrisonconsoles.com>
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
#include "pbd/xml++.h"

#include "ardour/amp.h"
#include "ardour/debug.h"
#include "ardour/audio_buffer.h"
#include "ardour/monitor_processor.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

/* specialize for bool because of set_value() semantics */

namespace ARDOUR {
	template<> void MPControl<bool>::set_value (double v, PBD::Controllable::GroupControlDisposition gcd) {
                bool newval = fabs (v) >= 0.5;
                if (newval != _value) {
                        _value = newval;
                        Changed (true, gcd); /* EMIT SIGNAL */
                }
        }
}

MonitorProcessor::MonitorProcessor (Session& s)
	: Processor (s, X_("MonitorOut"), Temporal::AudioTime)
	, solo_cnt (0)
	, _monitor_active (false)

	, _dim_all_ptr (new MPControl<bool> (false, _("monitor dim"), Controllable::Toggle))
	, _cut_all_ptr (new MPControl<bool> (false, _("monitor cut"), Controllable::Toggle))
	, _mono_ptr (new MPControl<bool> (false, _("monitor mono"), Controllable::Toggle))
	, _dim_level_ptr (new MPControl<volatile gain_t>
		/* default is -12dB, range is -20dB to 0dB */
		(dB_to_coefficient(-12.0), _("monitor dim level"), Controllable::Flag (0),
		dB_to_coefficient(-20.0), dB_to_coefficient (0.0)))
	, _solo_boost_level_ptr (new MPControl<volatile gain_t>
	/* default is 0dB, range is 0dB to +20dB */
			(dB_to_coefficient(0.0), _("monitor solo boost level"), Controllable::Flag (0),
			 dB_to_coefficient(0.0), dB_to_coefficient(10.0)))
	, _dim_all_control (_dim_all_ptr)
	, _cut_all_control (_cut_all_ptr)
	, _mono_control (_mono_ptr)
	, _dim_level_control (_dim_level_ptr)
	, _solo_boost_level_control (_solo_boost_level_ptr)

	, _dim_all (*_dim_all_ptr)
	, _cut_all (*_cut_all_ptr)
	, _mono (*_mono_ptr)
	, _dim_level (*_dim_level_ptr)
	, _solo_boost_level (*_solo_boost_level_ptr)

{
}

MonitorProcessor::~MonitorProcessor ()
{
	allocate_channels (0);

	/* special case for MPControl */
	_dim_all_control->DropReferences (); /* EMIT SIGNAL */
	_cut_all_control->DropReferences (); /* EMIT SIGNAL */
	_mono_control->DropReferences (); /* EMIT SIGNAL */
	_dim_level_control->DropReferences (); /* EMIT SIGNAL */
	_solo_boost_level_control->DropReferences (); /* EMIT SIGNAL */
}

void
MonitorProcessor::allocate_channels (uint32_t size)
{
	while (_channels.size() > size) {
		if (_channels.back()->soloed) {
			if (solo_cnt > 0) {
				--solo_cnt;
			}
		}
		ChannelRecord* cr = _channels.back();
		_channels.pop_back();
		delete cr;
	}

	uint32_t n = _channels.size() + 1;

	while (_channels.size() < size) {
		_channels.push_back (new ChannelRecord (n));
	}
}

int
MonitorProcessor::set_state (const XMLNode& node, int version)
{
	int ret = Processor::set_state (node, version);

	if (ret != 0) {
		return ret;
	}

	std::string type_name;
	if (!node.get_property (X_("type"), type_name)) {
		error << string_compose (X_("programming error: %1"), X_("MonitorProcessor XML settings have no type information"))
			<< endmsg;
		return -1;
	}

	if (type_name != X_("monitor")) {
		error << string_compose (X_("programming error: %1"), X_("MonitorProcessor given unknown XML settings"))
			<< endmsg;
		return -1;
	}

	uint32_t channels = 0;
	if (!node.get_property (X_("channels"), channels)) {
		error << string_compose (X_("programming error: %1"), X_("MonitorProcessor XML settings are missing a channel cnt"))
			<< endmsg;
		return -1;
	}

	allocate_channels (channels);

	// need to check that these conversions are working as expected
	gain_t val;
	if (node.get_property (X_("dim-level"), val)) {
		_dim_level = val;
	}

	if (node.get_property (X_("solo-boost-level"), val)) {
		_solo_boost_level = val;
	}

	bool bool_val;
	if (node.get_property (X_("cut-all"), bool_val)) {
		_cut_all = bool_val;
	}

	if (node.get_property (X_("dim-all"), bool_val)) {
		_dim_all = bool_val;
	}

	if (node.get_property (X_("mono"), bool_val)) {
		_mono = bool_val;
	}

	for (XMLNodeList::const_iterator i = node.children().begin(); i != node.children().end(); ++i) {

		if ((*i)->name() == X_("Channel")) {

			uint32_t chn;
			if (!(*i)->get_property (X_("id"), chn)) {
				error << string_compose (X_("programming error: %1"), X_("MonitorProcessor XML settings are missing an ID"))
					<< endmsg;
				return -1;
			}

			if (chn >= _channels.size()) {
				error << string_compose (X_("programming error: %1"), X_("MonitorProcessor XML settings has an illegal channel count"))
					<< endmsg;
				return -1;
			}
			ChannelRecord& cr (*_channels[chn]);

			bool gain_coeff_zero;
			if ((*i)->get_property ("cut", gain_coeff_zero)) {
				if (gain_coeff_zero) {
					cr.cut = GAIN_COEFF_ZERO;
				} else {
					cr.cut = GAIN_COEFF_UNITY;
				}
			}

			bool dim;
			if ((*i)->get_property ("dim", dim)) {
				cr.dim = dim;
			}

			bool invert_polarity;
			if ((*i)->get_property ("invert", invert_polarity)) {
				if (invert_polarity) {
					cr.polarity = -1.0f;
				} else {
					cr.polarity = 1.0f;
				}
			}

			bool soloed;
			if ((*i)->get_property ("solo", soloed)) {
				cr.soloed = soloed;
			}
		}
	}

	/* reset solo cnt */

	solo_cnt = 0;

	for (vector<ChannelRecord*>::const_iterator x = _channels.begin(); x != _channels.end(); ++x) {
		if ((*x)->soloed) {
			solo_cnt++;
		}
	}

	update_monitor_state ();
	return 0;
}

XMLNode&
MonitorProcessor::state ()
{
	XMLNode& node(Processor::state ());

	/* this replaces any existing "type" property */

	node.set_property (X_("type"), X_("monitor"));

	node.set_property (X_ ("dim-level"), (float)_dim_level.val ());
	node.set_property (X_ ("solo-boost-level"), (float)_solo_boost_level.val ());

	node.set_property (X_("cut-all"), _cut_all.val());
	node.set_property (X_("dim-all"), _dim_all.val());
	node.set_property (X_("mono"), _mono.val());

	node.set_property (X_("channels"), (uint32_t)_channels.size ());

	XMLNode* chn_node;
	uint32_t chn = 0;

	for (vector<ChannelRecord*>::const_iterator x = _channels.begin (); x != _channels.end ();
			++x, ++chn) {
		chn_node = new XMLNode (X_("Channel"));

		chn_node->set_property ("id", chn);

		// implicitly cast these to bool
		chn_node->set_property (X_("cut"), (*x)->cut != GAIN_COEFF_UNITY);
		chn_node->set_property (X_("invert"), (*x)->polarity != GAIN_COEFF_UNITY);
		chn_node->set_property (X_("dim"), (*x)->dim == true);
		chn_node->set_property (X_("solo"), (*x)->soloed == true);

		node.add_child_nocopy (*chn_node);
	}

	return node;
}

void
MonitorProcessor::run (BufferSet& bufs, samplepos_t /*start_sample*/, samplepos_t /*end_sample*/, double /*speed*/, pframes_t nframes, bool /*result_required*/)
{
	uint32_t chn = 0;
	gain_t target_gain;
	gain_t dim_level_this_time = _dim_level;
	gain_t global_cut = (_cut_all ? GAIN_COEFF_ZERO : GAIN_COEFF_UNITY);
	gain_t global_dim = (_dim_all ? dim_level_this_time : GAIN_COEFF_UNITY);
	gain_t solo_boost;

	if (_session.listening() || _session.soloing()) {
		solo_boost = _solo_boost_level;
	} else {
		solo_boost = GAIN_COEFF_UNITY;
	}

	for (BufferSet::audio_iterator b = bufs.audio_begin(); b != bufs.audio_end(); ++b) {

		/* don't double-scale by both track dim and global dim coefficients */

		gain_t dim_level = (global_dim == GAIN_COEFF_UNITY ? (_channels[chn]->dim ? dim_level_this_time : GAIN_COEFF_UNITY) : GAIN_COEFF_UNITY);

		if (_channels[chn]->soloed) {
			target_gain = _channels[chn]->polarity * _channels[chn]->cut * dim_level * global_cut * global_dim * solo_boost;
		} else {
			if (solo_cnt == 0) {
				target_gain = _channels[chn]->polarity * _channels[chn]->cut * dim_level * global_cut * global_dim * solo_boost;
			} else {
				target_gain = GAIN_COEFF_ZERO;
			}
		}

		if (target_gain != _channels[chn]->current_gain || target_gain != GAIN_COEFF_UNITY) {

			_channels[chn]->current_gain = Amp::apply_gain (*b, _session.nominal_sample_rate(), nframes, _channels[chn]->current_gain, target_gain);
		}

		++chn;
	}

	if (_mono) {
		DEBUG_TRACE (DEBUG::Monitor, "mono-izing\n");

		/* chn is now the number of channels, use as a scaling factor when mixing
		*/
		gain_t scale = 1.f / (float)chn;
		BufferSet::audio_iterator b = bufs.audio_begin();
		AudioBuffer& ab (*b);
		Sample* buf = ab.data();

		/* scale the first channel */

		for (pframes_t n = 0; n < nframes; ++n) {
			buf[n] *= scale;
		}

		/* add every other channel into the first channel's buffer */

		++b;
		for (; b != bufs.audio_end(); ++b) {
			AudioBuffer& ob (*b);
			Sample* obuf = ob.data ();
			for (pframes_t n = 0; n < nframes; ++n) {
				buf[n] += obuf[n] * scale;
			}
		}

		/* copy the first channel to every other channel's buffer */

		b = bufs.audio_begin();
		++b;
		for (; b != bufs.audio_end(); ++b) {
			AudioBuffer& ob (*b);
			Sample* obuf = ob.data ();
			memcpy (obuf, buf, sizeof (Sample) * nframes);
		}
	}
}

bool
MonitorProcessor::configure_io (ChanCount in, ChanCount out)
{
	allocate_channels (in.n_audio());
	return Processor::configure_io (in, out);
}

bool
MonitorProcessor::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	out = in;
	return true;
}

void
MonitorProcessor::set_polarity (uint32_t chn, bool invert)
{
	if (invert) {
		_channels[chn]->polarity = -1.0f;
	} else {
		_channels[chn]->polarity = 1.0f;
	}
	update_monitor_state ();
}

void
MonitorProcessor::set_dim (uint32_t chn, bool yn)
{
	_channels[chn]->dim = yn;
	update_monitor_state ();
}

void
MonitorProcessor::set_cut (uint32_t chn, bool yn)
{
	if (yn) {
		_channels[chn]->cut = GAIN_COEFF_ZERO;
	} else {
		_channels[chn]->cut = GAIN_COEFF_UNITY;
	}
	update_monitor_state ();
}

void
MonitorProcessor::set_solo (uint32_t chn, bool solo)
{
	if (solo != _channels[chn]->soloed) {
		_channels[chn]->soloed = solo;

		if (solo) {
			solo_cnt++;
		} else {
			if (solo_cnt > 0) {
				solo_cnt--;
			}
		}
	}
	update_monitor_state ();
}

void
MonitorProcessor::set_mono (bool yn)
{
	_mono = yn;
	update_monitor_state ();
}

void
MonitorProcessor::set_cut_all (bool yn)
{
	_cut_all = yn;
	update_monitor_state ();
}

void
MonitorProcessor::set_dim_all (bool yn)
{
	_dim_all = yn;
	update_monitor_state ();
}

bool
MonitorProcessor::display_to_user () const
{
	return false;
}

bool
MonitorProcessor::soloed (uint32_t chn) const
{
	return _channels[chn]->soloed;
}

bool
MonitorProcessor::inverted (uint32_t chn) const
{
	return _channels[chn]->polarity < 0.0f;
}

bool
MonitorProcessor::cut (uint32_t chn) const
{
	return _channels[chn]->cut == GAIN_COEFF_ZERO;
}

bool
MonitorProcessor::dimmed (uint32_t chn) const
{
	return _channels[chn]->dim;
}

bool
MonitorProcessor::mono () const
{
	return _mono;
}

bool
MonitorProcessor::dim_all () const
{
	return _dim_all;
}

bool
MonitorProcessor::cut_all () const
{
	return _cut_all;
}

void
MonitorProcessor::update_monitor_state ()
{
	bool en = false;

	if (_cut_all || _dim_all || _mono) {
		en = true;
	}

	const uint32_t nchans = _channels.size();
	for (uint32_t i = 0; i < nchans && !en; ++i) {
		if (cut (i) || dimmed (i) || soloed (i) || inverted (i)) {
			en = true;
			break;
		}
	}

	if (_monitor_active != en) {
		_monitor_active = en;
		_session.MonitorChanged();
	}
}

boost::shared_ptr<Controllable>
MonitorProcessor::channel_cut_control (uint32_t chn) const
{
	if (chn < _channels.size()) {
		return _channels[chn]->cut_control;
	}
	return boost::shared_ptr<Controllable>();
}

boost::shared_ptr<Controllable>
MonitorProcessor::channel_dim_control (uint32_t chn) const
{
	if (chn < _channels.size()) {
		return _channels[chn]->dim_control;
	}
	return boost::shared_ptr<Controllable>();
}

boost::shared_ptr<Controllable>
MonitorProcessor::channel_polarity_control (uint32_t chn) const
{
	if (chn < _channels.size()) {
		return _channels[chn]->polarity_control;
	}
	return boost::shared_ptr<Controllable>();
}

boost::shared_ptr<Controllable>
MonitorProcessor::channel_solo_control (uint32_t chn) const
{
	if (chn < _channels.size()) {
		return _channels[chn]->soloed_control;
	}
	return boost::shared_ptr<Controllable>();
}

MonitorProcessor::ChannelRecord::ChannelRecord (uint32_t chn)
	: current_gain (GAIN_COEFF_UNITY)
	, cut_ptr (new MPControl<gain_t> (1.0, string_compose (_("cut control %1"), chn), PBD::Controllable::GainLike))
	, dim_ptr (new MPControl<bool> (false, string_compose (_("dim control"), chn), PBD::Controllable::Toggle))
	, polarity_ptr (new MPControl<gain_t> (1.0, string_compose (_("polarity control"), chn), PBD::Controllable::Toggle, -1, 1))
	, soloed_ptr (new MPControl<bool> (false, string_compose (_("solo control"), chn), PBD::Controllable::Toggle))

	, cut_control (cut_ptr)
	, dim_control (dim_ptr)
	, polarity_control (polarity_ptr)
	, soloed_control (soloed_ptr)

	, cut (*cut_ptr)
	, dim (*dim_ptr)
	, polarity (*polarity_ptr)
	, soloed (*soloed_ptr)
{
}

MonitorProcessor::ChannelRecord::~ChannelRecord ()
{
	/* special case for MPControl */
	cut_control->DropReferences(); /* EMIT SIGNAL */
	dim_control->DropReferences(); /* EMIT SIGNAL */
	polarity_control->DropReferences(); /* EMIT SIGNAL */
	soloed_control->DropReferences(); /* EMIT SIGNAL */
}
