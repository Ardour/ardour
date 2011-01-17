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
#include "ardour/panner_1in2out.h"
#include "ardour/utils.h"
#include "ardour/audio_buffer.h"

#include "ardour/runtime_functions.h"
#include "ardour/buffer_set.h"
#include "ardour/audio_buffer.h"
#include "ardour/vbap.h"

#include "i18n.h"

#include "pbd/mathfix.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

static PanPluginDescriptor _descriptor = {
        "Mono to Stereo Panner",
        1, 1, 2, 2,
        Panner1in2out::factory
};

extern "C" { PanPluginDescriptor* panner_descriptor () { return &_descriptor; }

Panner1in2out::Panner1in2out (PannerShell& p)
	: Panner (p)
        , _position (new PanControllable (parent.session(), _("position"), this, Evoral::Parameter(PanAzimuthAutomation, 0, 0)))
	, left (0.5)
	, right (0.5)
	, left_interp (left)
	, right_interp (right)
{
        desired_left = left;
        desired_right = right;
}

Panner1in2out::~Panner1in2out ()
{
}

void
Panner1in2out::set_position (double p)
{
        _desired_right = p;
        _desired_left = 1 - p;
}

void
Panner1in2out::do_distribute_one (AudioBuffer& srcbuf, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes, uint32_t /* not used */)
{
	assert (obufs.count().n_audio() == 2);

	pan_t delta;
	Sample* dst;
	pan_t pan;

	if (_muted) {
		return;
	}

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
			left = left_interp[which] + 0.9 * (left[which] - left_interp[which]);
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

				/* XXX it would be nice to mark the buffer as written to */
			}

		} else {

			/* pan is 1 so we can just copy the input samples straight in */
			
			mix_buffers_no_gain(dst,src,nframes);

			/* XXX it would be nice to mark the buffer as written to */
		}
	}

}

string
Panner1in2out::describe_parameter (Evoral::Parameter param)
{
        switch (param.type()) {
        case PanWidthAutomation:
                return "Pan:width";
        case PanAzimuthAutomation:
                return "Pan:position";
        case PanElevationAutomation: 
                error << X_("stereo panner should not have elevation control") << endmsg;
                return "Pan:elevation";
        } 
        
        return Automatable::describe_parameter (param);
}

