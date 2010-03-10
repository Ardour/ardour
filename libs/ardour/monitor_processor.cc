#include "pbd/xml++.h"

#include "ardour/amp.h"
#include "ardour/dB.h"
#include "ardour/monitor_processor.h"
#include "ardour/session.h"

#include "i18n.h"

using namespace ARDOUR;
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

MonitorProcessor::MonitorProcessor (Session& s, const XMLNode& node)
        : Processor (s, node)
{
        set_state (node, Stateful::loading_state_version);
}

int
MonitorProcessor::set_state (const XMLNode& node, int version)
{
        return Processor::set_state (node, version);
}

XMLNode&
MonitorProcessor::state (bool full)
{
        XMLNode& node (Processor::state (full));

	/* this replaces any existing "type" property */

	node.add_property (X_("type"), X_("monitor"));

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

                gain_t dim_level = (global_dim == 1.0 ? (_dim[chn] ? dim_level_this_time : 1.0) : 1.0);

                if (_soloed[chn]) {
                        target_gain = _polarity[chn] * _cut[chn] * dim_level * global_cut * global_dim * solo_boost;
                } else {
                        if (solo_cnt == 0) {
                                target_gain = _polarity[chn] * _cut[chn] * dim_level * global_cut * global_dim * solo_boost;
                        } else {
                                target_gain = 0.0;
                        }
                }

                if (target_gain != current_gain[chn] || target_gain != 1.0f) {

                        Amp::apply_gain (*b, nframes, current_gain[chn], target_gain);
                        current_gain[chn] = target_gain;
                }

                ++chn;
        }
}

bool
MonitorProcessor::configure_io (ChanCount in, ChanCount out)
{
        uint32_t needed = in.n_audio();

        while (current_gain.size() > needed) {
                current_gain.pop_back ();
                _dim.pop_back ();
                _cut.pop_back ();
                _polarity.pop_back ();

                if (_soloed.back()) {
                        if (solo_cnt > 0) {
                                --solo_cnt;
                        }
                }

                _soloed.pop_back ();
        }
        
        while (current_gain.size() < needed) {
                current_gain.push_back (1.0);
                _dim.push_back (false);
                _cut.push_back (1.0);
                _polarity.push_back (1.0);
                _soloed.push_back (false);
        }

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
                _polarity[chn] = -1.0f;
        } else {
                _polarity[chn] = 1.0f;
        }
}       

void
MonitorProcessor::set_dim (uint32_t chn, bool yn)
{
        _dim[chn] = yn;
}

void
MonitorProcessor::set_cut (uint32_t chn, bool yn)
{
        if (yn) {
                _cut[chn] = 0.0f;
        } else {
                _cut[chn] = 1.0f;
        }
}

void
MonitorProcessor::set_solo (uint32_t chn, bool solo)
{
        _soloed[chn] = solo;

        if (solo) {
                solo_cnt++;
        } else {
                if (solo_cnt > 0) {
                        solo_cnt--;
                }
        }
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
        return _soloed[chn];
}


bool 
MonitorProcessor::inverted (uint32_t chn) const
{
        return _polarity[chn] < 0.0f;
}


bool 
MonitorProcessor::cut (uint32_t chn) const
{
        return _cut[chn] == 0.0f;
}

bool 
MonitorProcessor::dimmed (uint32_t chn) const
{
        return _dim[chn];
}

