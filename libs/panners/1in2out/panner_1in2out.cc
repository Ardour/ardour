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

#include "ardour/session.h"
#include "ardour/panner.h"
#include "ardour/utils.h"
#include "ardour/audio_buffer.h"

#include "ardour/debug.h"
#include "ardour/runtime_functions.h"
#include "ardour/buffer_set.h"
#include "ardour/audio_buffer.h"
#include "ardour/pannable.h"

#include "i18n.h"
#include "panner_1in2out.h"

#include "pbd/mathfix.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

static PanPluginDescriptor _descriptor = {
        "Mono to Stereo Panner",
        1, 2, 
        Panner1in2out::factory
};

extern "C" { PanPluginDescriptor* panner_descriptor () { return &_descriptor; } }

Panner1in2out::Panner1in2out (boost::shared_ptr<Pannable> p)
	: Panner (p)
{
        if (!_pannable->has_state()) {
                _pannable->pan_azimuth_control->set_value (0.5);
        }
        
        update ();

        left = desired_left;
        right = desired_right;
        left_interp = left;
        right_interp = right;

        _pannable->pan_azimuth_control->Changed.connect_same_thread (*this, boost::bind (&Panner1in2out::update, this));
}

Panner1in2out::~Panner1in2out ()
{
}

void
Panner1in2out::update ()
{
        float panR, panL;
        float const pan_law_attenuation = -3.0f;
        float const scale = 2.0f - 4.0f * powf (10.0f,pan_law_attenuation/20.0f);

        panR = _pannable->pan_azimuth_control->get_value();
        panL = 1 - panR;

        desired_left = panL * (scale * panL + 1.0f - scale);
        desired_right = panR * (scale * panR + 1.0f - scale);
}

void
Panner1in2out::set_position (double p)
{
        if (clamp_position (p)) {
                _pannable->pan_azimuth_control->set_value (p);
        }
}

bool
Panner1in2out::clamp_position (double& p)
{
        /* any position between 0.0 and 1.0 is legal */
        DEBUG_TRACE (DEBUG::Panning, string_compose ("want to move panner to %1 - always allowed in 0.0-1.0 range\n", p));
        p = max (min (p, 1.0), 0.0);
        return true;
}

double 
Panner1in2out::position () const
{
        return _pannable->pan_azimuth_control->get_value ();
}

void
Panner1in2out::distribute_one (AudioBuffer& srcbuf, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes, uint32_t /* not used */)
{
	assert (obufs.count().n_audio() == 2);

	pan_t delta;
	Sample* dst;
	pan_t pan;

	Sample* const src = srcbuf.data();
        
	/* LEFT OUTPUT */

	dst = obufs.get_audio(0).data();

	if (fabsf ((delta = (left - desired_left))) > 0.002) { // about 1 degree of arc

		/* we've moving the pan by an appreciable amount, so we must
		   interpolate over 64 frames or nframes, whichever is smaller */

		pframes_t const limit = min ((pframes_t) 64, nframes);
		pframes_t n;

		delta = -(delta / (float) (limit));

		for (n = 0; n < limit; n++) {
			left_interp = left_interp + delta;
			left = left_interp + 0.9 * (left - left_interp);
			dst[n] += src[n] * left * gain_coeff;
		}

		/* then pan the rest of the buffer; no need for interpolation for this bit */

		pan = left * gain_coeff;

		mix_buffers_with_gain (dst+n,src+n,nframes-n,pan);

	} else {

		left = desired_left;
		left_interp = left;

		if ((pan = (left * gain_coeff)) != 1.0f) {

			if (pan != 0.0f) {

				/* pan is 1 but also not 0, so we must do it "properly" */

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

	if (fabsf ((delta = (right - desired_right))) > 0.002) { // about 1 degree of arc

		/* we're moving the pan by an appreciable amount, so we must
		   interpolate over 64 frames or nframes, whichever is smaller */

		pframes_t const limit = min ((pframes_t) 64, nframes);
		pframes_t n;

		delta = -(delta / (float) (limit));

		for (n = 0; n < limit; n++) {
			right_interp = right_interp + delta;
			right = right_interp + 0.9 * (right - right_interp);
			dst[n] += src[n] * right * gain_coeff;
		}

		/* then pan the rest of the buffer, no need for interpolation for this bit */

		pan = right * gain_coeff;

		mix_buffers_with_gain(dst+n,src+n,nframes-n,pan);

		/* XXX it would be nice to mark the buffer as written to */

	} else {

		right = desired_right;
		right_interp = right;

		if ((pan = (right * gain_coeff)) != 1.0f) {

			if (pan != 0.0f) {

				/* pan is not 1 but also not 0, so we must do it "properly" */
				
				mix_buffers_with_gain(dst,src,nframes,pan);

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
Panner1in2out::distribute_one_automated (AudioBuffer& srcbuf, BufferSet& obufs,
                                         framepos_t start, framepos_t end, pframes_t nframes,
                                         pan_t** buffers, uint32_t which)
{
	assert (obufs.count().n_audio() == 2);

	Sample* dst;
	pan_t* pbuf;
	Sample* const src = srcbuf.data();
        pan_t* const position = buffers[0];

	/* fetch positional data */

	if (!_pannable->pan_azimuth_control->list()->curve().rt_safe_get_vector (start, end, position, nframes)) {
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

                float panR = position[n];
                const float panL = 1 - panR;

                /* note that are overwriting buffers, but its OK
                   because we're finished with their old contents
                   (position automation data) and are
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
Panner1in2out::factory (boost::shared_ptr<Pannable> p, boost::shared_ptr<Speakers> /* ignored */)
{
	return new Panner1in2out (p);
}

XMLNode&
Panner1in2out::get_state ()
{
	XMLNode& root (Panner::get_state ());
	root.add_property (X_("type"), _descriptor.name);
	return root;
}


std::set<Evoral::Parameter> 
Panner1in2out::what_can_be_automated() const
{
        set<Evoral::Parameter> s;
        s.insert (Evoral::Parameter (PanAzimuthAutomation));
        return s;
}

string
Panner1in2out::describe_parameter (Evoral::Parameter p)
{
        switch (p.type()) {
        case PanAzimuthAutomation:
                return _("L/R");
        default:
                return _pannable->describe_parameter (p);
        }
}

string 
Panner1in2out::value_as_string (boost::shared_ptr<AutomationControl> ac) const
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
                
        default:
                return _pannable->value_as_string (ac);
        }
}

void
Panner1in2out::reset ()
{
	set_position (0.5);
	update ();
}
