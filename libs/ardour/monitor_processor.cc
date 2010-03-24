#include "pbd/convert.h"
#include "pbd/error.h"
#include "pbd/xml++.h"

#include "ardour/amp.h"
#include "ardour/dB.h"
#include "ardour/debug.h"
#include "ardour/audio_buffer.h"
#include "ardour/monitor_processor.h"
#include "ardour/session.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

MonitorProcessor::MonitorProcessor (Session& s)
        : Processor (s, X_("MonitorOut"))
{
        solo_cnt = 0;
        _cut_all = false;
        _dim_all = false;
        _dim_level = 0.2;
        _solo_boost_level = 1.0;
}

void
MonitorProcessor::allocate_channels (uint32_t size)
{
        while (_channels.size() > size) {
                if (_channels.back().soloed) {
                        if (solo_cnt > 0) {
                                --solo_cnt;
                        }
                }
                _channels.pop_back();
        }

        while (_channels.size() < size) {
                _channels.push_back (ChannelRecord());
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
                double val = atof (prop->value());
                _dim_level = val;
        }

        if ((prop = node.property (X_("solo-boost-level"))) != 0) {
                double val = atof (prop->value());
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
                        ChannelRecord& cr (_channels[chn]);

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

        for (vector<ChannelRecord>::const_iterator x = _channels.begin(); x != _channels.end(); ++x) {
                if (x->soloed) {
                        solo_cnt++;
                }
        }
        
        return 0;
}

XMLNode&
MonitorProcessor::state (bool full)
{
        XMLNode& node (Processor::state (full));
        char buf[64];

	/* this replaces any existing "type" property */

	node.add_property (X_("type"), X_("monitor"));
        
        snprintf (buf, sizeof(buf), "%.12g", _dim_level);
        node.add_property (X_("dim-level"), buf);

        snprintf (buf, sizeof(buf), "%.12g", _solo_boost_level);
        node.add_property (X_("solo-boost-level"), buf);

        node.add_property (X_("cut-all"), (_cut_all ? "yes" : "no"));
        node.add_property (X_("dim-all"), (_dim_all ? "yes" : "no"));
        node.add_property (X_("mono"), (_mono ? "yes" : "no"));
        
        uint32_t limit = _channels.size();

        snprintf (buf, sizeof (buf), "%u", limit);
        node.add_property (X_("channels"), buf);

        XMLNode* chn_node;
        uint32_t chn = 0;

        for (vector<ChannelRecord>::const_iterator x = _channels.begin(); x != _channels.end(); ++x, ++chn) {
                chn_node = new XMLNode (X_("Channel"));

                snprintf (buf, sizeof (buf), "%u", chn);
                chn_node->add_property ("id", buf);

                chn_node->add_property (X_("cut"), x->cut == 1.0 ? "no" : "yes");
                chn_node->add_property (X_("invert"), x->polarity == 1.0 ? "no" : "yes");
                chn_node->add_property (X_("dim"), x->dim ? "yes" : "no");
                chn_node->add_property (X_("solo"), x->soloed ? "yes" : "no");
                
                node.add_child_nocopy (*chn_node);
        }

        return node;
}

void
MonitorProcessor::run (BufferSet& bufs, sframes_t /*start_frame*/, sframes_t /*end_frame*/, nframes_t nframes, bool /*result_required*/)
{
        uint32_t chn = 0;
        gain_t target_gain;
        gain_t dim_level_this_time = _dim_level;
        gain_t global_cut = (_cut_all ? 0.0f : 1.0f);
        gain_t global_dim = (_dim_all ? dim_level_this_time : 1.0f);
        gain_t solo_boost;

        if (_session.listening() || _session.soloing()) {
                solo_boost = _solo_boost_level;
        } else {
                solo_boost = 1.0;
        }

        for (BufferSet::audio_iterator b = bufs.audio_begin(); b != bufs.audio_end(); ++b) {

                /* don't double-scale by both track dim and global dim coefficients */

                gain_t dim_level = (global_dim == 1.0 ? (_channels[chn].dim ? dim_level_this_time : 1.0) : 1.0);

                if (_channels[chn].soloed) {
                        target_gain = _channels[chn].polarity * _channels[chn].cut * dim_level * global_cut * global_dim * solo_boost;
                } else {
                        if (solo_cnt == 0) {
                                target_gain = _channels[chn].polarity * _channels[chn].cut * dim_level * global_cut * global_dim * solo_boost;
                        } else {
                                target_gain = 0.0;
                        }
                }

                DEBUG_TRACE (DEBUG::Monitor, 
                             string_compose("channel %1 sb %2 gc %3 gd %4 cd %5 dl %6 cp %7 cc %8 cs %9 sc %10 TG %11\n", 
                                            chn, 
                                            solo_boost,
                                            global_cut,
                                            global_dim,
                                            _channels[chn].dim,
                                            dim_level,
                                            _channels[chn].polarity,
                                            _channels[chn].cut,
                                            _channels[chn].soloed,
                                            solo_cnt,
                                            target_gain));
                
                if (target_gain != _channels[chn].current_gain || target_gain != 1.0f) {

                        Amp::apply_gain (*b, nframes, _channels[chn].current_gain, target_gain);
                        _channels[chn].current_gain = target_gain;
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

                for (nframes_t n = 0; n < nframes; ++n) {
                        buf[n] *= scale;
                }

                /* add every other channel into the first channel's buffer */

                ++b;
                for (; b != bufs.audio_end(); ++b) {
                        AudioBuffer& ob (*b);
                        Sample* obuf = ob.data ();
                        for (nframes_t n = 0; n < nframes; ++n) {
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
MonitorProcessor::can_support_io_configuration (const ChanCount& in, ChanCount& out) const
{
        return in == out;
}

void
MonitorProcessor::set_polarity (uint32_t chn, bool invert)
{
        if (invert) {
                _channels[chn].polarity = -1.0f;
        } else {
                _channels[chn].polarity = 1.0f;
        }
}       

void
MonitorProcessor::set_dim (uint32_t chn, bool yn)
{
        _channels[chn].dim = yn;
}

void
MonitorProcessor::set_cut (uint32_t chn, bool yn)
{
        if (yn) {
                _channels[chn].cut = 0.0f;
        } else {
                _channels[chn].cut = 1.0f;
        }
}

void
MonitorProcessor::set_solo (uint32_t chn, bool solo)
{
        if (solo != _channels[chn].soloed) {
                _channels[chn].soloed = solo;
                
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

void
MonitorProcessor::set_dim_level (gain_t val)
{
        _dim_level = val;
}

void
MonitorProcessor::set_solo_boost_level (gain_t val)
{
        _solo_boost_level = val;
}

bool 
MonitorProcessor::soloed (uint32_t chn) const
{
        return _channels[chn].soloed;
}


bool 
MonitorProcessor::inverted (uint32_t chn) const
{
        return _channels[chn].polarity < 0.0f;
}


bool 
MonitorProcessor::cut (uint32_t chn) const
{
        return _channels[chn].cut == 0.0f;
}

bool 
MonitorProcessor::dimmed (uint32_t chn) const
{
        return _channels[chn].dim;
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
