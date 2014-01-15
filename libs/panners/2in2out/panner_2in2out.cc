/*
    Copyright (C) 2004-2011 Paul Davis

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

#include <inttypes.h>

#include <cmath>
#include <cerrno>
#include <fstream>
#include <cstdlib>
#include <string>
#include <cstdio>
#include <locale.h>
#include <unistd.h>
#include <float.h>
#include <iomanip>

#include <glibmm.h>

#include "pbd/cartesian.h"
#include "pbd/convert.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/xml++.h"
#include "pbd/enumwriter.h"

#include "evoral/Curve.hpp"

#include "ardour/audio_buffer.h"
#include "ardour/audio_buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/pan_controllable.h"
#include "ardour/pannable.h"
#include "ardour/runtime_functions.h"
#include "ardour/session.h"
#include "ardour/utils.h"
#include "ardour/mix.h"

#include "panner_2in2out.h"

#include "i18n.h"

#include "pbd/mathfix.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

static PanPluginDescriptor _descriptor = {
        "Equal Power Stereo",
        "http://ardour.org/plugin/panner_2in2out",
        "http://ardour.org/plugin/panner_2in2out#ui",
        2, 2,
        10000,
        Panner2in2out::factory
};

extern "C" { PanPluginDescriptor* panner_descriptor () { return &_descriptor; } }

Panner2in2out::Panner2in2out (boost::shared_ptr<Pannable> p)
	: Panner (p)
{
        if (!_pannable->has_state()) {
                _pannable->pan_azimuth_control->set_value (0.5);
                _pannable->pan_width_control->set_value (1.0);
        }

        double const w = width();
        double const wrange = min (position(), (1 - position())) * 2;
        if (fabs(w) > wrange) {
                set_width(w > 0 ? wrange : -wrange);
        }

        
        update ();
        
        /* LEFT SIGNAL */
        left_interp[0] = left[0] = desired_left[0];
        right_interp[0] = right[0] = desired_right[0]; 
        
        /* RIGHT SIGNAL */
        left_interp[1] = left[1] = desired_left[1];
        right_interp[1] = right[1] = desired_right[1];
        
        _pannable->pan_azimuth_control->Changed.connect_same_thread (*this, boost::bind (&Panner2in2out::update, this));
        _pannable->pan_width_control->Changed.connect_same_thread (*this, boost::bind (&Panner2in2out::update, this));
}

Panner2in2out::~Panner2in2out ()
{
}

double 
Panner2in2out::position () const
{
        return _pannable->pan_azimuth_control->get_value();
}

double 
Panner2in2out::width () const
{
        return _pannable->pan_width_control->get_value();
}

void
Panner2in2out::set_position (double p)
{
        if (clamp_position (p)) {
                _pannable->pan_azimuth_control->set_value (p);
        }
}

void
Panner2in2out::set_width (double p)
{
        if (clamp_width (p)) {
                _pannable->pan_width_control->set_value (p);
        }
}

void
Panner2in2out::thaw ()
{
	Panner::thaw ();
	if (_frozen == 0) {
		update ();
	}
}

void
Panner2in2out::update ()
{
	if (_frozen) {
		return;
	}

        /* it would be very nice to split this out into a virtual function
           that can be accessed from BaseStereoPanner and used in do_distribute_automated().
           
           but the place where its used in do_distribute_automated() is a tight inner loop,
           and making "nframes" virtual function calls to compute values is an absurd
           overhead.
        */
        
        /* x == 0 => hard left = 180.0 degrees
           x == 1 => hard right = 0.0 degrees
        */
        
        float pos[2];
        double width = this->width ();
        const double direction_as_lr_fract = position ();

        if (width < 0.0) {
                width = -width;
                pos[0] = direction_as_lr_fract + (width/2.0); // left signal lr_fract
                pos[1] = direction_as_lr_fract - (width/2.0); // right signal lr_fract
        } else {
                pos[1] = direction_as_lr_fract + (width/2.0); // right signal lr_fract
                pos[0] = direction_as_lr_fract - (width/2.0); // left signal lr_fract
        }
        
        /* compute target gain coefficients for both input signals */
        
        float const pan_law_attenuation = -3.0f;
        float const scale = 2.0f - 4.0f * powf (10.0f,pan_law_attenuation/20.0f);
        float panR;
        float panL;
        
        /* left signal */
        
        panR = pos[0];
        panL = 1 - panR;
        desired_left[0] = panL * (scale * panL + 1.0f - scale);
        desired_right[0] = panR * (scale * panR + 1.0f - scale);
        
        /* right signal */
        
        panR = pos[1];
        panL = 1 - panR;
        desired_left[1] = panL * (scale * panL + 1.0f - scale);
        desired_right[1] = panR * (scale * panR + 1.0f - scale);
}

bool
Panner2in2out::clamp_position (double& p)
{
        double w = width ();
        return clamp_stereo_pan (p, w);
}

bool
Panner2in2out::clamp_width (double& w)
{
        double p = position ();
        return clamp_stereo_pan (p, w);
}

pair<double, double>
Panner2in2out::position_range () const
{
	return make_pair (0.5 - (1 - width()) / 2, 0.5 + (1 - width()) / 2);
}

pair<double, double>
Panner2in2out::width_range () const
{
	double const w = min (position(), (1 - position())) * 2;
	return make_pair (-w, w);
}

bool
Panner2in2out::clamp_stereo_pan (double& direction_as_lr_fract, double& width)
{
        double r_pos;
        double l_pos;

        width = max (min (width, 1.0), -1.0);
        direction_as_lr_fract = max (min (direction_as_lr_fract, 1.0), 0.0);

        r_pos = direction_as_lr_fract + (width/2.0);
        l_pos = direction_as_lr_fract - (width/2.0);

        if (width < 0.0) {
                swap (r_pos, l_pos);
        }

        /* if the new left position is less than or equal to zero (hard left) and the left panner
           is already there, we're not moving the left signal. 
        */
        
        if (l_pos < 0.0) {
                return false;
        }

        /* if the new right position is less than or equal to 1.0 (hard right) and the right panner
           is already there, we're not moving the right signal. 
        */
        
        if (r_pos > 1.0) {
                return false;
                
        }

        return true;
}

void
Panner2in2out::distribute_one (AudioBuffer& srcbuf, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes, uint32_t which)
{
	assert (obufs.count().n_audio() == 2);

	pan_t delta;
	Sample* dst;
	pan_t pan;

	Sample* const src = srcbuf.data();
        
	/* LEFT OUTPUT */

	dst = obufs.get_audio(0).data();

	if (fabsf ((delta = (left[which] - desired_left[which]))) > 0.002) { // about 1 degree of arc

		/* we've moving the pan by an appreciable amount, so we must
		   interpolate over 64 frames or nframes, whichever is smaller */

		pframes_t const limit = min ((pframes_t) 64, nframes);
		pframes_t n;

		delta = -(delta / (float) (limit));

		for (n = 0; n < limit; n++) {
			left_interp[which] = left_interp[which] + delta;
			left[which] = left_interp[which] + 0.9 * (left[which] - left_interp[which]);
			dst[n] += src[n] * left[which] * gain_coeff;
		}

		/* then pan the rest of the buffer; no need for interpolation for this bit */

		pan = left[which] * gain_coeff;

		mix_buffers_with_gain (dst+n,src+n,nframes-n,pan);

	} else {

		left[which] = desired_left[which];
		left_interp[which] = left[which];

		if ((pan = (left[which] * gain_coeff)) != 1.0f) {

			if (pan != 0.0f) {

				/* pan is 1 but also not 0, so we must do it "properly" */
				
				//obufs.get_audio(1).read_from (srcbuf, nframes);
				mix_buffers_with_gain(dst,src,nframes,pan);

				/* mark that we wrote into the buffer */

				// obufs[0] = 0;

			}

		} else {

			/* pan is 1 so we can just copy the input samples straight in */

			mix_buffers_no_gain(dst,src,nframes);
                        
			/* XXX it would be nice to mark that we wrote into the buffer */
		}
	}

	/* RIGHT OUTPUT */

	dst = obufs.get_audio(1).data();

	if (fabsf ((delta = (right[which] - desired_right[which]))) > 0.002) { // about 1 degree of arc

		/* we're moving the pan by an appreciable amount, so we must
		   interpolate over 64 frames or nframes, whichever is smaller */

		pframes_t const limit = min ((pframes_t) 64, nframes);
		pframes_t n;

		delta = -(delta / (float) (limit));

		for (n = 0; n < limit; n++) {
			right_interp[which] = right_interp[which] + delta;
			right[which] = right_interp[which] + 0.9 * (right[which] - right_interp[which]);
			dst[n] += src[n] * right[which] * gain_coeff;
		}

		/* then pan the rest of the buffer, no need for interpolation for this bit */

		pan = right[which] * gain_coeff;

		mix_buffers_with_gain(dst+n,src+n,nframes-n,pan);

		/* XXX it would be nice to mark the buffer as written to */

	} else {

		right[which] = desired_right[which];
		right_interp[which] = right[which];

		if ((pan = (right[which] * gain_coeff)) != 1.0f) {

			if (pan != 0.0f) {

				/* pan is not 1 but also not 0, so we must do it "properly" */
				
				mix_buffers_with_gain(dst,src,nframes,pan);
				// obufs.get_audio(1).read_from (srcbuf, nframes);
				
				/* XXX it would be nice to mark the buffer as written to */
			}

		} else {

			/* pan is 1 so we can just copy the input samples straight in */
			
			mix_buffers_no_gain(dst,src,nframes);

			/* XXX it would be nice to mark the buffer as written to */
		}
	}
}

void
Panner2in2out::distribute_one_automated (AudioBuffer& srcbuf, BufferSet& obufs,
                                         framepos_t start, framepos_t end, pframes_t nframes,
                                         pan_t** buffers, uint32_t which)
{
	assert (obufs.count().n_audio() == 2);

	Sample* dst;
	pan_t* pbuf;
	Sample* const src = srcbuf.data();
        pan_t* const position = buffers[0];
        pan_t* const width = buffers[1];

	/* fetch positional data */

	if (!_pannable->pan_azimuth_control->list()->curve().rt_safe_get_vector (start, end, position, nframes)) {
		/* fallback */
                distribute_one (srcbuf, obufs, 1.0, nframes, which);
		return;
	}

	if (!_pannable->pan_width_control->list()->curve().rt_safe_get_vector (start, end, width, nframes)) {
		/* fallback */
                distribute_one (srcbuf, obufs, 1.0, nframes, which);
		return;
	}

	/* apply pan law to convert positional data into pan coefficients for
	   each buffer (output)
	*/

	const float pan_law_attenuation = -3.0f;
	const float scale = 2.0f - 4.0f * powf (10.0f,pan_law_attenuation/20.0f);

	for (pframes_t n = 0; n < nframes; ++n) {

                float panR;

                if (which == 0) { 
                        // panning left signal
                        panR = position[n] - (width[n]/2.0f); // center - width/2
                } else {
                        // panning right signal
                        panR = position[n] + (width[n]/2.0f); // center - width/2
                }

                const float panL = 1 - panR;

                /* note that are overwriting buffers, but its OK
                   because we're finished with their old contents
                   (position/width automation data) and are
                   replacing it with panning/gain coefficients 
                   that we need to actually process the data.
                */
                
                buffers[0][n] = panL * (scale * panL + 1.0f - scale);
                buffers[1][n] = panR * (scale * panR + 1.0f - scale);
        }

	/* LEFT OUTPUT */

	dst = obufs.get_audio(0).data();
	pbuf = buffers[0];

	for (pframes_t n = 0; n < nframes; ++n) {
		dst[n] += src[n] * pbuf[n];
	}

	/* XXX it would be nice to mark the buffer as written to */

	/* RIGHT OUTPUT */

	dst = obufs.get_audio(1).data();
	pbuf = buffers[1];

	for (pframes_t n = 0; n < nframes; ++n) {
		dst[n] += src[n] * pbuf[n];
	}

	/* XXX it would be nice to mark the buffer as written to */
}

Panner*
Panner2in2out::factory (boost::shared_ptr<Pannable> p, boost::shared_ptr<Speakers> /* ignored */)
{
	return new Panner2in2out (p);
}

XMLNode&
Panner2in2out::get_state ()
{
	XMLNode& root (Panner::get_state ());
	root.add_property (X_("uri"), _descriptor.panner_uri);
	/* this is needed to allow new sessions to load with old Ardour: */
	root.add_property (X_("type"), _descriptor.name);
	return root;
}

std::set<Evoral::Parameter> 
Panner2in2out::what_can_be_automated() const
{
        set<Evoral::Parameter> s;
        s.insert (Evoral::Parameter (PanAzimuthAutomation));
        s.insert (Evoral::Parameter (PanWidthAutomation));
        return s;
}

string
Panner2in2out::describe_parameter (Evoral::Parameter p)
{
        switch (p.type()) {
        case PanAzimuthAutomation:
                return _("L/R");
        case PanWidthAutomation:
                return _("Width");
        default:
                return _pannable->describe_parameter (p);
        }
}

string 
Panner2in2out::value_as_string (boost::shared_ptr<AutomationControl> ac) const
{
        /* DO NOT USE LocaleGuard HERE */
        double val = ac->get_value();

        switch (ac->parameter().type()) {
        case PanAzimuthAutomation:
                /* We show the position of the center of the image relative to the left & right.
                   This is expressed as a pair of percentage values that ranges from (100,0) 
                   (hard left) through (50,50) (hard center) to (0,100) (hard right).
                   
                   This is pretty wierd, but its the way audio engineers expect it. Just remember that
                   the center of the USA isn't Kansas, its (50LA, 50NY) and it will all make sense.
                
		   This is designed to be as narrow as possible. Dedicated
		   panner GUIs can do their own version of this if they need
		   something less compact.
                */
                
                return string_compose (_("L%1R%2"), (int) rint (100.0 * (1.0 - val)),
                                       (int) rint (100.0 * val));

        case PanWidthAutomation:
                return string_compose (_("Width: %1%%"), (int) floor (100.0 * val));
                
        default:
                return _("unused");
        }
}

void
Panner2in2out::reset ()
{
	set_position (0.5);
	set_width (1);
	update ();
}
