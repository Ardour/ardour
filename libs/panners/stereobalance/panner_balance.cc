/*
    Copyright (C) 2004-2011 Paul Davis
    adopted from 2in2out panner by Robin Gareus <robin@gareus.org>

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

#include "panner_balance.h"

#include "i18n.h"

#include "pbd/mathfix.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

static PanPluginDescriptor _descriptor = {
	"Stereo Balance",
	"http://ardour.org/plugin/panner_balance",
	"http://ardour.org/plugin/panner_balance#ui",
	2, 2,
	Pannerbalance::factory
};

extern "C" { PanPluginDescriptor* panner_descriptor () { return &_descriptor; } }

Pannerbalance::Pannerbalance (boost::shared_ptr<Pannable> p)
	: Panner (p)
{
	if (!_pannable->has_state()) {
		_pannable->pan_azimuth_control->set_value (0.5);
	}

	update ();

	/* LEFT SIGNAL */
	pos_interp[0] = pos[0] = desired_pos[0];
	/* RIGHT SIGNAL */
	pos_interp[1] = pos[1] = desired_pos[1];

	_pannable->pan_azimuth_control->Changed.connect_same_thread (*this, boost::bind (&Pannerbalance::update, this));
}

Pannerbalance::~Pannerbalance ()
{
}

double
Pannerbalance::position () const
{
	return _pannable->pan_azimuth_control->get_value();
}

	void
Pannerbalance::set_position (double p)
{
	if (clamp_position (p)) {
		_pannable->pan_azimuth_control->set_value (p);
	}
}

	void
Pannerbalance::thaw ()
{
	Panner::thaw ();
	if (_frozen == 0) {
		update ();
	}
}

void
Pannerbalance::update ()
{
	if (_frozen) {
		return;
	}

	float const pos = _pannable->pan_azimuth_control->get_value();

	if (pos == .5) {
		desired_pos[0] = 1.0;
		desired_pos[1] = 1.0;
	} else if (pos > .5) {
		desired_pos[0] = 2 - 2. * pos;
		desired_pos[1] = 1.0;
	} else {
		desired_pos[0] = 1.0;
		desired_pos[1] = 2. * pos;
	}
}

bool
Pannerbalance::clamp_position (double& p)
{
	p = max (min (p, 1.0), 0.0);
	return true;
}

pair<double, double>
Pannerbalance::position_range () const
{
	return make_pair (0, 1);
}

void
Pannerbalance::distribute_one (AudioBuffer& srcbuf, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes, uint32_t which)
{
	assert (obufs.count().n_audio() == 2);

	pan_t delta;
	Sample* dst;
	pan_t pan;

	Sample* const src = srcbuf.data();

	dst = obufs.get_audio(which).data();

	if (fabsf ((delta = (pos[which] - desired_pos[which]))) > 0.002) { // about 1 degree of arc

		/* we've moving the pan by an appreciable amount, so we must
			 interpolate over 64 frames or nframes, whichever is smaller */

		pframes_t const limit = min ((pframes_t) 64, nframes);
		pframes_t n;

		delta = -(delta / (float) (limit));

		for (n = 0; n < limit; n++) {
			pos_interp[which] = pos_interp[which] + delta;
			pos[which] = pos_interp[which] + 0.9 * (pos[which] - pos_interp[which]);
			dst[n] += src[n] * pos[which] * gain_coeff;
		}

		/* then pan the rest of the buffer; no need for interpolation for this bit */

		pan = pos[which] * gain_coeff;

		mix_buffers_with_gain (dst+n,src+n,nframes-n,pan);

	} else {

		pos[which] = desired_pos[which];
		pos_interp[which] = pos[which];

		if ((pan = (pos[which] * gain_coeff)) != 1.0f) {

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
		}
	}
}

void
Pannerbalance::distribute_one_automated (AudioBuffer& srcbuf, BufferSet& obufs,
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

	for (pframes_t n = 0; n < nframes; ++n) {

		float const pos = position[n];

		if (which == 0) { // Left
			if (pos > .5) {
				buffers[which][n] = 2 - 2. * pos;
			} else {
				buffers[which][n] = 1.0;
			}
		} else { // Right
			if (pos < .5) {
				buffers[which][n] = 2. * pos;
			} else {
				buffers[which][n] = 1.0;
			}
		}
	}

	dst = obufs.get_audio(which).data();
	pbuf = buffers[which];

	for (pframes_t n = 0; n < nframes; ++n) {
		dst[n] += src[n] * pbuf[n];
	}

	/* XXX it would be nice to mark the buffer as written to */
}

Panner*
Pannerbalance::factory (boost::shared_ptr<Pannable> p, boost::shared_ptr<Speakers> /* ignored */)
{
	return new Pannerbalance (p);
}

	XMLNode&
Pannerbalance::get_state ()
{
	XMLNode& root (Panner::get_state ());
	root.add_property (X_("uri"), _descriptor.panner_uri);
	/* this is needed to allow new sessions to load with old Ardour: */
	root.add_property (X_("type"), _descriptor.name);
	return root;
}

std::set<Evoral::Parameter>
Pannerbalance::what_can_be_automated() const
{
	set<Evoral::Parameter> s;
	s.insert (Evoral::Parameter (PanAzimuthAutomation));
	return s;
}

string
Pannerbalance::describe_parameter (Evoral::Parameter p)
{
	switch (p.type()) {
		case PanAzimuthAutomation:
			return _("L/R");
		default:
			return _pannable->describe_parameter (p);
	}
}

string
Pannerbalance::value_as_string (boost::shared_ptr<AutomationControl> ac) const
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
Pannerbalance::reset ()
{
	set_position (0.5);
	update ();
}
