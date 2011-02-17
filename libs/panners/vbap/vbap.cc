#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <iostream>
#include <string>

#include "pbd/cartesian.h"

#include "ardour/pannable.h"
#include "ardour/speakers.h"
#include "ardour/audio_buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/pan_controllable.h"

#include "vbap.h"
#include "vbap_speakers.h"

using namespace PBD;
using namespace ARDOUR;
using namespace std;

static PanPluginDescriptor _descriptor = {
        "VBAP 2D panner",
        -1, -1,
        VBAPanner::factory
};

extern "C" { PanPluginDescriptor* panner_descriptor () { return &_descriptor; } }

VBAPanner::Signal::Signal (Session& session, VBAPanner& p, uint32_t n)
{
        gains[0] = gains[1] = gains[2] = 0;
        desired_gains[0] = desired_gains[1] = desired_gains[2] = 0;
        outputs[0] = outputs[1] = outputs[2] = -1;
        desired_outputs[0] = desired_outputs[1] = desired_outputs[2] = -1;
};

VBAPanner::VBAPanner (boost::shared_ptr<Pannable> p, boost::shared_ptr<Speakers> s)
	: Panner (p)
	, _speakers (new VBAPSpeakers (s))
{
        _pannable->pan_azimuth_control->Changed.connect_same_thread (*this, boost::bind (&VBAPanner::update, this));
        _pannable->pan_width_control->Changed.connect_same_thread (*this, boost::bind (&VBAPanner::update, this));

        update ();
}

VBAPanner::~VBAPanner ()
{
        clear_signals ();
}

void
VBAPanner::clear_signals ()
{
        for (vector<Signal*>::iterator i = _signals.begin(); i != _signals.end(); ++i) {
                delete *i;
        }
        _signals.clear ();
}

void
VBAPanner::configure_io (ChanCount in, ChanCount /* ignored - we use Speakers */)
{
        uint32_t n = in.n_audio();

        clear_signals ();

        for (uint32_t i = 0; i < n; ++i) {
                _signals.push_back (new Signal (_pannable->session(), *this, i));
        }

        update ();
}

void
VBAPanner::update ()
{
        /* recompute signal directions based on panner azimuth and width (diffusion) parameters)
         */

        /* panner azimuth control is [0 .. 1.0] which we interpret as [0 .. 360] degrees
         */

        double center = _pannable->pan_azimuth_control->get_value() * 360.0;

        /* panner width control is [-1.0 .. 1.0]; we ignore sign, and map to [0 .. 360] degrees
           so that a width of 1 corresponds to a signal equally present from all directions, 
           and a width of zero corresponds to a point source from the "center" (above)
        */

        double w = fabs (_pannable->pan_width_control->get_value()) * 360.0;

        double min_dir = center - w;
        min_dir = max (min (min_dir, 360.0), 0.0);

        double max_dir = center + w;
        max_dir = max (min (max_dir, 360.0), 0.0);

        double degree_step_per_signal = (max_dir - min_dir) / _signals.size();
        double signal_direction = min_dir;

        for (vector<Signal*>::iterator s = _signals.begin(); s != _signals.end(); ++s) {

                Signal* signal = *s;

                signal->direction = AngularVector (signal_direction, 0.0);

                compute_gains (signal->desired_gains, signal->desired_outputs, signal->direction.azi, signal->direction.ele);
                        cerr << " @ " << signal->direction.azi << " /= " << signal->direction.ele
                             << " Outputs: "
                             << signal->desired_outputs[0] + 1 << ' '
                             << signal->desired_outputs[1] + 1 << ' '
                             << " Gains "
                             << signal->desired_gains[0] << ' '
                             << signal->desired_gains[1] << ' '
                             << endl;

                signal_direction += degree_step_per_signal;
        }
}

void 
VBAPanner::compute_gains (double gains[3], int speaker_ids[3], int azi, int ele) 
{
	/* calculates gain factors using loudspeaker setup and given direction */
	double cartdir[3];
	double power;
	int i,j,k;
	double small_g;
	double big_sm_g, gtmp[3];

	azi_ele_to_cart (azi,ele, cartdir[0], cartdir[1], cartdir[2]);  
	big_sm_g = -100000.0;

	gains[0] = gains[1] = gains[2] = 0;
	speaker_ids[0] = speaker_ids[1] = speaker_ids[2] = 0;

	for (i = 0; i < _speakers->n_tuples(); i++) {

		small_g = 10000000.0;

		for (j = 0; j < _speakers->dimension(); j++) {

			gtmp[j] = 0.0;

			for (k = 0; k < _speakers->dimension(); k++) {
				gtmp[j] += cartdir[k] * _speakers->matrix(i)[j*_speakers->dimension()+k]; 
			}

			if (gtmp[j] < small_g) {
				small_g = gtmp[j];
			}
		}

		if (small_g > big_sm_g) {

			big_sm_g = small_g;

			gains[0] = gtmp[0]; 
			gains[1] = gtmp[1]; 

			speaker_ids[0] = _speakers->speaker_for_tuple (i, 0);
			speaker_ids[1] = _speakers->speaker_for_tuple (i, 1);
                        
			if (_speakers->dimension() == 3) {
				gains[2] = gtmp[2];
				speaker_ids[2] = _speakers->speaker_for_tuple (i, 2);
			} else {
				gains[2] = 0.0;
				speaker_ids[2] = -1;
			}
		}
	}
        
	power = sqrt (gains[0]*gains[0] + gains[1]*gains[1] + gains[2]*gains[2]);

	if (power > 0) {
		gains[0] /= power; 
		gains[1] /= power;
		gains[2] /= power;
	}
}

void
VBAPanner::distribute (BufferSet& inbufs, BufferSet& obufs, gain_t gain_coefficient, pframes_t nframes)
{
        uint32_t n;
        vector<Signal*>::iterator s;

        assert (inbufs.count().n_audio() == _signals.size());

        for (s = _signals.begin(), n = 0; s != _signals.end(); ++s, ++n) {

                Signal* signal (*s);

                distribute_one (inbufs.get_audio (n), obufs, gain_coefficient, nframes, n);

                memcpy (signal->gains, signal->desired_gains, sizeof (signal->gains));
                memcpy (signal->outputs, signal->desired_outputs, sizeof (signal->outputs));
        }
}

void
VBAPanner::distribute_one (AudioBuffer& srcbuf, BufferSet& obufs, gain_t gain_coefficient, pframes_t nframes, uint32_t which)
{
	Sample* const src = srcbuf.data();
	Sample* dst;
	pan_t pan;
	uint32_t n_audio = obufs.count().n_audio();
	bool todo[n_audio];
        Signal* signal (_signals[which]);

	for (uint32_t o = 0; o < n_audio; ++o) {
		todo[o] = true;
	}
        
	/* VBAP may distribute the signal across up to 3 speakers depending on
	   the configuration of the speakers.
	*/

	for (int o = 0; o < 3; ++o) {
		if (signal->desired_outputs[o] != -1) {
                        
			pframes_t n = 0;

			/* XXX TODO: interpolate across changes in gain and/or outputs
			 */

			dst = obufs.get_audio (signal->desired_outputs[o]).data();

			pan = gain_coefficient * signal->desired_gains[o];
			mix_buffers_with_gain (dst+n,src+n,nframes-n,pan);

			todo[o] = false;
		}
	}
        
	for (uint32_t o = 0; o < n_audio; ++o) {
		if (todo[o]) {
			/* VBAP decided not to deliver any audio to this output, so we write silence */
			dst = obufs.get_audio(o).data();
			memset (dst, 0, sizeof (Sample) * nframes);
		}
	}
        
}

void 
VBAPanner::distribute_one_automated (AudioBuffer& src, BufferSet& obufs,
                                     framepos_t start, framepos_t end, pframes_t nframes, pan_t** buffers, uint32_t which)
{
}

XMLNode&
VBAPanner::get_state ()
{
	return state (true);
}

XMLNode&
VBAPanner::state (bool full_state)
{
        XMLNode& node (Panner::get_state());
	node.add_property (X_("type"), _descriptor.name);
	return node;
}

int
VBAPanner::set_state (const XMLNode& node, int /*version*/)
{
	return 0;
}

Panner*
VBAPanner::factory (boost::shared_ptr<Pannable> p, boost::shared_ptr<Speakers> s)
{
	return new VBAPanner (p, s);
}

ChanCount
VBAPanner::in() const
{
        return ChanCount (DataType::AUDIO, _signals.size());
}

ChanCount
VBAPanner::out() const
{
        return ChanCount (DataType::AUDIO, _speakers->n_speakers());
}

std::set<Evoral::Parameter> 
VBAPanner::what_can_be_automated() const
{
        set<Evoral::Parameter> s;
        s.insert (Evoral::Parameter (PanAzimuthAutomation));
        s.insert (Evoral::Parameter (PanWidthAutomation));
        return s;
}
        
string
VBAPanner::describe_parameter (Evoral::Parameter p)
{
        switch (p.type()) {
        case PanAzimuthAutomation:
                return _("Direction");
        case PanWidthAutomation:
                return _("Diffusion");
        default:
                return _pannable->describe_parameter (p);
        }
}

string 
VBAPanner::value_as_string (boost::shared_ptr<AutomationControl> ac) const
{
        /* DO NOT USE LocaleGuard HERE */
        double val = ac->get_value();

        switch (ac->parameter().type()) {
        case PanAzimuthAutomation: /* direction */
                return string_compose (_("%1"), val * 360.0);
                
        case PanWidthAutomation: /* diffusion */
                return string_compose (_("%1%%"), (int) floor (100.0 * fabs(val)));
                
        default:
                return _pannable->value_as_string (ac);
        }
}

AngularVector
VBAPanner::signal_position (uint32_t n) const
{
        if (n < _signals.size()) {
                return _signals[n]->direction;
        }

        return AngularVector();
}
