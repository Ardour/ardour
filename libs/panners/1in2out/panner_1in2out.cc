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
#include "ardour/buffer_set.h"
#include "ardour/audio_buffer.h"
#include "ardour/pannable.h"
#include "ardour/profile.h"

#include "i18n.h"
#include "panner_1in2out.h"

#include "pbd/mathfix.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

#ifdef ENABLE_PANNING_DELAY
static PanPluginDescriptor _descriptor = {
        "Mono to Stereo Panner with Delay",
        "http://ardour.org/plugin/panner_1in2out_delay",
        "http://ardour.org/plugin/panner_1in2out#ui",
        1, 2, 
        1000,
        Panner1in2out::factory
};
#else /* !defined(ENABLE_PANNING_DELAY) */
static PanPluginDescriptor _descriptor = {
        "Mono to Stereo Panner",
        "http://ardour.org/plugin/panner_1in2out",
        "http://ardour.org/plugin/panner_1in2out#ui",
        1, 2, 
        10000,
        Panner1in2out::factory
};
#endif /* !defined(ENABLE_PANNING_DELAY) */

extern "C" ARDOURPANNER_API PanPluginDescriptor*  panner_descriptor () { return &_descriptor; }

Panner1in2out::Panner1in2out (boost::shared_ptr<Pannable> p)
	: Panner (p)
	, left_dist_buf (p->session())
	, right_dist_buf (p->session())
{
        if (!Profile->get_trx () ) {
            if (!_pannable->has_state ()) {
                _pannable->pan_azimuth_control->set_value (0.5);
            }
        }
        
        update ();

        left = desired_left;
        right = desired_right;

        _pannable->pan_azimuth_control->Changed.connect_same_thread (*this, boost::bind (&Panner1in2out::update, this));
}

Panner1in2out::~Panner1in2out ()
{
}

void
Panner1in2out::update ()
{
        float panR, panL;

        panR = position();
        panL = 1.0f - panR;

        desired_left = panL * (_pan_law_scale * panL + 1.0f - _pan_law_scale);
        desired_right = panR * (_pan_law_scale * panR + 1.0f - _pan_law_scale);
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

pair<double, double>
Panner1in2out::position_range () const
{
	return make_pair (0, 1);
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

	Sample* dst;

	Sample* const src = srcbuf.data();
        
	/* LEFT OUTPUT */

	dst = obufs.get_audio(0).data();

	left_dist_buf.update_session_config();
	left_dist_buf.set_pan_position(1.0 - position());

	left_dist_buf.mix_buffers(dst, src, nframes, left * gain_coeff, desired_left * gain_coeff);

	left = desired_left;

	/* XXX it would be nice to mark the buffer as written to, depending on gain (see pan_distribution_buffer.cc) */

	/* RIGHT OUTPUT */

	dst = obufs.get_audio(1).data();

	right_dist_buf.update_session_config();
	right_dist_buf.set_pan_position(position());

	right_dist_buf.mix_buffers(dst, src, nframes, right * gain_coeff, desired_right * gain_coeff);

	right = desired_right;

	/* XXX it would be nice to mark the buffer as written to, depending on gain (see pan_distribution_buffer.cc) */
}

void
Panner1in2out::distribute_one_automated (AudioBuffer& srcbuf, BufferSet& obufs,
                                         framepos_t start, framepos_t end, pframes_t nframes,
                                         pan_t** buffers, uint32_t which)
{
	assert (obufs.count().n_audio() == 2);

	Sample* dst;
	Sample* const src = srcbuf.data();
        pan_t* const position = buffers[0];

	/* fetch positional data */

	if (!_pannable->pan_azimuth_control->list()->curve().rt_safe_get_vector (start, end, position, nframes)) {
		/* fallback */
                distribute_one (srcbuf, obufs, 1.0, nframes, which);
		return;
	}

	/* LEFT OUTPUT */

	dst = obufs.get_audio(0).data();

	left_dist_buf.update_session_config();

	for (pframes_t n = 0; n < nframes; ++n) {
                const float panL = 1.0f - position[n];

		left_dist_buf.set_pan_position(panL);
		dst[n] += left_dist_buf.process(src[n] * panL * (_pan_law_scale * panL + 1.0f - _pan_law_scale));
	}

	/* XXX it would be nice to mark the buffer as written to */

	/* RIGHT OUTPUT */

	dst = obufs.get_audio(1).data();

	right_dist_buf.update_session_config();

	for (pframes_t n = 0; n < nframes; ++n) {
                const float panR = position[n];
                
		right_dist_buf.set_pan_position(panR);
		dst[n] += right_dist_buf.process(src[n] * panR * (_pan_law_scale * panR + 1.0f - _pan_law_scale));
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
	root.add_property (X_("uri"), _descriptor.panner_uri);
	/* this is needed to allow new sessions to load with old Ardour: */
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
                return _("unused");
        }
}

void
Panner1in2out::reset ()
{
	set_position (0.5);
	update ();
}
