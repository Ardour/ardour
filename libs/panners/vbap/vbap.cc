/*
    Copyright (C) 2012 Paul Davis 

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

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <iostream>
#include <string>

#include "pbd/cartesian.h"
#include "pbd/compose.h"

#include "ardour/amp.h"
#include "ardour/audio_buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/pan_controllable.h"
#include "ardour/pannable.h"
#include "ardour/speakers.h"

#include "vbap.h"
#include "vbap_speakers.h"

#include "i18n.h"

using namespace PBD;
using namespace ARDOUR;
using namespace std;

static PanPluginDescriptor _descriptor = {
        "VBAP 2D panner",
        "http://ardour.org/plugin/panner_vbap",
        "http://ardour.org/plugin/panner_vbap#ui",
        -1, -1,
        1000,
        VBAPanner::factory
};

extern "C" { PanPluginDescriptor* panner_descriptor () { return &_descriptor; } }

VBAPanner::Signal::Signal (Session&, VBAPanner&, uint32_t, uint32_t n_speakers)
{
        resize_gains (n_speakers);

        desired_gains[0] = desired_gains[1] = desired_gains[2] = 0;
        outputs[0] = outputs[1] = outputs[2] = -1;
        desired_outputs[0] = desired_outputs[1] = desired_outputs[2] = -1;
}

void
VBAPanner::Signal::Signal::resize_gains (uint32_t n)
{
        gains.assign (n, 0.0);
}        

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
                Signal* s = new Signal (_pannable->session(), *this, i, _speakers->n_speakers());
                _signals.push_back (s);
                
        }

        update ();
}

void
VBAPanner::update ()
{
        /* recompute signal directions based on panner azimuth and, if relevant, width (diffusion) parameters)
         */

        /* panner azimuth control is [0 .. 1.0] which we interpret as [0 .. 360] degrees
         */
        double center = _pannable->pan_azimuth_control->get_value() * 360.0;

        if (_signals.size() > 1) {

                /* panner width control is [-1.0 .. 1.0]; we ignore sign, and map to [0 .. 360] degrees
                   so that a width of 1 corresponds to a signal equally present from all directions, 
                   and a width of zero corresponds to a point source from the "center" (above) point
                   on the perimeter of the speaker array.
                */

                double w = fabs (_pannable->pan_width_control->get_value()) * 360.0;
                
                double min_dir = center - (w/2.0);
                if (min_dir < 0) {
                        min_dir = 360.0 + min_dir; // its already negative
                }
                min_dir = max (min (min_dir, 360.0), 0.0);
                
                double max_dir = center + (w/2.0);
                if (max_dir > 360.0) {
                        max_dir = max_dir - 360.0;
                }
                max_dir = max (min (max_dir, 360.0), 0.0);
                
                if (max_dir < min_dir) {
                        swap (max_dir, min_dir);
                }

                double degree_step_per_signal = (max_dir - min_dir) / (_signals.size() - 1);
                double signal_direction = min_dir;

                if (w >= 0.0) {

                        /* positive width - normal order of signal spread */

                        for (vector<Signal*>::iterator s = _signals.begin(); s != _signals.end(); ++s) {
                        
                                Signal* signal = *s;
                                
                                signal->direction = AngularVector (signal_direction, 0.0);
                                compute_gains (signal->desired_gains, signal->desired_outputs, signal->direction.azi, signal->direction.ele);
                                signal_direction += degree_step_per_signal;
                        }
                } else {

                        /* inverted width - reverse order of signal spread */

                        for (vector<Signal*>::reverse_iterator s = _signals.rbegin(); s != _signals.rend(); ++s) {
                        
                                Signal* signal = *s;
                                
                                signal->direction = AngularVector (signal_direction, 0.0);
                                compute_gains (signal->desired_gains, signal->desired_outputs, signal->direction.azi, signal->direction.ele);
                                signal_direction += degree_step_per_signal;
                        }
                }

        } else if (_signals.size() == 1) {

                /* width has no role to play if there is only 1 signal: VBAP does not do "diffusion" of a single channel */

                Signal* s = _signals.front();
                s->direction = AngularVector (center, 0);
                compute_gains (s->desired_gains, s->desired_outputs, s->direction.azi, s->direction.ele);
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

	spherical_to_cartesian (azi, ele, 1.0, cartdir[0], cartdir[1], cartdir[2]);  
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

                memcpy (signal->outputs, signal->desired_outputs, sizeof (signal->outputs));
        }
}

void
VBAPanner::distribute_one (AudioBuffer& srcbuf, BufferSet& obufs, gain_t gain_coefficient, pframes_t nframes, uint32_t which)
{
	Sample* const src = srcbuf.data();
        Signal* signal (_signals[which]);

	/* VBAP may distribute the signal across up to 3 speakers depending on
	   the configuration of the speakers.

           But the set of speakers in use "this time" may be different from
           the set of speakers "the last time". So we have up to 6 speakers
           involved, and we have to interpolate so that those no longer
           in use are rapidly faded to silence and those newly in use
           are rapidly faded to their correct level. This prevents clicks
           as we change the set of speakers used to put the signal in
           a given position.

           However, the speakers are represented by output buffers, and other
           speakers may write to the same buffers, so we cannot use
           anything here that will simply assign new (sample) values
           to the output buffers - everything must be done via mixing
           functions and not assignment/copying.
	*/

        vector<double>::size_type sz = signal->gains.size();

        assert (sz == obufs.count().n_audio());

        int8_t outputs[sz]; // on the stack, no malloc
        
        /* set initial state of each output "record"
         */

        for (uint32_t o = 0; o < sz; ++o) {
                outputs[o] = 0;
        }

        /* for all outputs used this time and last time,
           change the output record to show what has
           happened.
        */


        for (int o = 0; o < 3; ++o) {
                if (signal->outputs[o] != -1) {
                        /* used last time */
                        outputs[signal->outputs[o]] |= 1;
                } 

                if (signal->desired_outputs[o] != -1) {
                        /* used this time */
                        outputs[signal->desired_outputs[o]] |= 1<<1;
                } 
        }

        /* at this point, we can test a speaker's status:

           (outputs[o] & 1)      <= in use before
           (outputs[o] & 2)      <= in use this time
           (outputs[o] & 3) == 3 <= in use both times
            outputs[o] == 0      <= not in use either time
           
        */

	for (int o = 0; o < 3; ++o) {
                pan_t pan;
                int output = signal->desired_outputs[o];

		if (output == -1) {
                        continue;
                }

                pan = gain_coefficient * signal->desired_gains[o];

                if (pan == 0.0 && signal->gains[output] == 0.0) {
                        
                        /* nothing deing delivered to this output */

                        signal->gains[output] = 0.0;
                        
                } else if (fabs (pan - signal->gains[output]) > 0.00001) {
                        
                        /* signal to this output but the gain coefficient has changed, so 
                           interpolate between them.
                        */

                        AudioBuffer& buf (obufs.get_audio (output));
                        buf.accumulate_with_ramped_gain_from (srcbuf.data(), nframes, signal->gains[output], pan, 0);
                        signal->gains[output] = pan;

                } else {
                        
                        /* signal to this output, same gain as before so just copy with gain
                         */
                           
                        mix_buffers_with_gain (obufs.get_audio (output).data(),src,nframes,pan);
                        signal->gains[output] = pan;
                }
	}

        /* clean up the outputs that were used last time but not this time
         */

        for (uint32_t o = 0; o < sz; ++o) {
                if (outputs[o] == 1) {
                        /* take signal and deliver with a rapid fade out
                         */
                        AudioBuffer& buf (obufs.get_audio (o));
                        buf.accumulate_with_ramped_gain_from (srcbuf.data(), nframes, signal->gains[o], 0.0, 0);
                        signal->gains[o] = 0.0;
                }
        }

        /* note that the output buffers were all silenced at some point
           so anything we didn't write to with this signal (or any others)
           is just as it should be.
        */
}

void 
VBAPanner::distribute_one_automated (AudioBuffer& /*src*/, BufferSet& /*obufs*/,
                                     framepos_t /*start*/, framepos_t /*end*/, 
				     pframes_t /*nframes*/, pan_t** /*buffers*/, uint32_t /*which*/)
{
	/* XXX to be implemented */
}

XMLNode&
VBAPanner::get_state ()
{
	XMLNode& node (Panner::get_state());
	node.add_property (X_("uri"), _descriptor.panner_uri);
	/* this is needed to allow new sessions to load with old Ardour: */
	node.add_property (X_("type"), _descriptor.name);
	return node;
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
        if (_signals.size() > 1) {
                s.insert (Evoral::Parameter (PanWidthAutomation));
        }
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
                return string_compose (_("%1"), int (rint (val * 360.0)));
                
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

boost::shared_ptr<Speakers>
VBAPanner::get_speakers () const 
{
        return _speakers->parent();
}

void
VBAPanner::set_position (double p)
{
        if (p < 0.0) {
                p = 1.0 + p;
        }

        if (p > 1.0) {
                p = fmod (p, 1.0);
        } 

        _pannable->pan_azimuth_control->set_value (p);
}

void
VBAPanner::set_width (double w)
{
        _pannable->pan_width_control->set_value (min (1.0, max (-1.0, w)));
}

void
VBAPanner::reset ()
{
	set_position (0);
	set_width (1);

	update ();
}
