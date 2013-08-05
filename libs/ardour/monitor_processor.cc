/*
    Copyright (C) 2010 Paul Davis

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

#include "pbd/convert.h"
#include "pbd/error.h"
#include "pbd/locale_guard.h"
#include "pbd/xml++.h"

#include "ardour/amp.h"
#include "ardour/debug.h"
#include "ardour/audio_buffer.h"
#include "ardour/monitor_processor.h"
#include "ardour/session.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

/* specialize for bool because of set_value() semantics */

namespace ARDOUR {
        template<> void MPControl<bool>::set_value (double v) {
                bool newval = fabs (v) >= 0.5;
                if (newval != _value) {
                        _value = newval;
                        Changed(); /* EMIT SIGNAL */
                }
        }
}

MonitorProcessor::MonitorProcessor (Session& s)
        : Processor (s, X_("MonitorOut"))
        , solo_cnt (0)

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

        const XMLProperty* prop;

        if ((prop = node.property (X_("type"))) == 0) {
                error << string_compose (X_("programming error: %1"), X_("MonitorProcessor XML settings have no type information"))
                      << endmsg;
                return -1;
        }

        if (prop->value() != X_("monitor")) {
                error << string_compose (X_("programming error: %1"), X_("MonitorProcessor given unknown XML settings"))
                      << endmsg;
                return -1;
        }

        if ((prop = node.property (X_("channels"))) == 0) {
                error << string_compose (X_("programming error: %1"), X_("MonitorProcessor XML settings are missing a channel cnt"))
                      << endmsg;
                return -1;
        }

        allocate_channels (atoi (prop->value()));

        if ((prop = node.property (X_("dim-level"))) != 0) {
                gain_t val = atof (prop->value());
                _dim_level = val;
        }

        if ((prop = node.property (X_("solo-boost-level"))) != 0) {
                gain_t val = atof (prop->value());
                _solo_boost_level = val;
        }

        if ((prop = node.property (X_("cut-all"))) != 0) {
                bool val = string_is_affirmative (prop->value());
                _cut_all = val;
        }
        if ((prop = node.property (X_("dim-all"))) != 0) {
                bool val = string_is_affirmative (prop->value());
                _dim_all = val;
        }
        if ((prop = node.property (X_("mono"))) != 0) {
                bool val = string_is_affirmative (prop->value());
                _mono = val;
        }

        for (XMLNodeList::const_iterator i = node.children().begin(); i != node.children().end(); ++i) {

                if ((*i)->name() == X_("Channel")) {
                        if ((prop = (*i)->property (X_("id"))) == 0) {
                                error << string_compose (X_("programming error: %1"), X_("MonitorProcessor XML settings are missing an ID"))
                                      << endmsg;
                                return -1;
                        }

                        uint32_t chn;

                        if (sscanf (prop->value().c_str(), "%u", &chn) != 1) {
                                error << string_compose (X_("programming error: %1"), X_("MonitorProcessor XML settings has an unreadable channel ID"))
                                      << endmsg;
                                return -1;
                        }

                        if (chn >= _channels.size()) {
                                error << string_compose (X_("programming error: %1"), X_("MonitorProcessor XML settings has an illegal channel count"))
                                      << endmsg;
                                return -1;
                        }
                        ChannelRecord& cr (*_channels[chn]);

                        if ((prop = (*i)->property ("cut")) != 0) {
                                if (string_is_affirmative (prop->value())){
                                        cr.cut = 0.0f;
                                } else {
                                        cr.cut = 1.0f;
                                }
                        }

                        if ((prop = (*i)->property ("dim")) != 0) {
                                bool val = string_is_affirmative (prop->value());
                                cr.dim = val;
                        }

                        if ((prop = (*i)->property ("invert")) != 0) {
                                if (string_is_affirmative (prop->value())) {
                                        cr.polarity = -1.0f;
                                } else {
                                        cr.polarity = 1.0f;
                                }
                        }

                        if ((prop = (*i)->property ("solo")) != 0) {
                                bool val = string_is_affirmative (prop->value());
                                cr.soloed = val;
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

        return 0;
}

XMLNode&
MonitorProcessor::state (bool full)
{
	LocaleGuard lg (X_("POSIX"));
        XMLNode& node (Processor::state (full));
        char buf[64];

	/* this replaces any existing "type" property */

	node.add_property (X_("type"), X_("monitor"));

        snprintf (buf, sizeof(buf), "%.12g", _dim_level.val());
        node.add_property (X_("dim-level"), buf);

        snprintf (buf, sizeof(buf), "%.12g", _solo_boost_level.val());
        node.add_property (X_("solo-boost-level"), buf);

        node.add_property (X_("cut-all"), (_cut_all ? "yes" : "no"));
        node.add_property (X_("dim-all"), (_dim_all ? "yes" : "no"));
        node.add_property (X_("mono"), (_mono ? "yes" : "no"));

        uint32_t limit = _channels.size();

        snprintf (buf, sizeof (buf), "%u", limit);
        node.add_property (X_("channels"), buf);

        XMLNode* chn_node;
        uint32_t chn = 0;

        for (vector<ChannelRecord*>::const_iterator x = _channels.begin(); x != _channels.end(); ++x, ++chn) {
                chn_node = new XMLNode (X_("Channel"));

                snprintf (buf, sizeof (buf), "%u", chn);
                chn_node->add_property ("id", buf);

                chn_node->add_property (X_("cut"), (*x)->cut == 1.0f ? "no" : "yes");
                chn_node->add_property (X_("invert"), (*x)->polarity == 1.0f ? "no" : "yes");
                chn_node->add_property (X_("dim"), (*x)->dim ? "yes" : "no");
                chn_node->add_property (X_("solo"), (*x)->soloed ? "yes" : "no");

                node.add_child_nocopy (*chn_node);
        }

        return node;
}

void
MonitorProcessor::run (BufferSet& bufs, framepos_t /*start_frame*/, framepos_t /*end_frame*/, pframes_t nframes, bool /*result_required*/)
{
        uint32_t chn = 0;
        gain_t target_gain;
        gain_t dim_level_this_time = _dim_level;
        gain_t global_cut = (_cut_all ? 0.0f : 1.0f);
        gain_t global_dim = (_dim_all ? dim_level_this_time : 1.0);
        gain_t solo_boost;

        if (_session.listening() || _session.soloing()) {
                solo_boost = _solo_boost_level;
        } else {
                solo_boost = 1.0;
        }

        for (BufferSet::audio_iterator b = bufs.audio_begin(); b != bufs.audio_end(); ++b) {

                /* don't double-scale by both track dim and global dim coefficients */

                gain_t dim_level = (global_dim == 1.0 ? (_channels[chn]->dim ? dim_level_this_time : 1.0) : 1.0);
		
                if (_channels[chn]->soloed) {
                        target_gain = _channels[chn]->polarity * _channels[chn]->cut * dim_level * global_cut * global_dim * solo_boost;
                } else {
                        if (solo_cnt == 0) {
                                target_gain = _channels[chn]->polarity * _channels[chn]->cut * dim_level * global_cut * global_dim * solo_boost;
                        } else {
                                target_gain = 0.0;
                        }
                }

                if (target_gain != _channels[chn]->current_gain || target_gain != 1.0f) {

                        Amp::apply_gain (*b, nframes, _channels[chn]->current_gain, target_gain);
                        _channels[chn]->current_gain = target_gain;
                }

                ++chn;
        }

        if (_mono) {
                DEBUG_TRACE (DEBUG::Monitor, "mono-izing\n");

                /* chn is now the number of channels, use as a scaling factor when mixing
                 */
                gain_t scale = 1.0/chn;
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
}

void
MonitorProcessor::set_dim (uint32_t chn, bool yn)
{
        _channels[chn]->dim = yn;
}

void
MonitorProcessor::set_cut (uint32_t chn, bool yn)
{
        if (yn) {
                _channels[chn]->cut = 0.0f;
        } else {
                _channels[chn]->cut = 1.0f;
        }
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
}

void
MonitorProcessor::set_mono (bool yn)
{
        _mono = yn;
}

void
MonitorProcessor::set_cut_all (bool yn)
{
        _cut_all = yn;
}

void
MonitorProcessor::set_dim_all (bool yn)
{
        _dim_all = yn;
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
        return _channels[chn]->cut == 0.0f;
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
	: current_gain (1.0)
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
