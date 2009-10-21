/*
    Copyright (C) 2004 Paul Davis

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

#define __STDC_FORMAT_MACROS 1
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

#include <glibmm.h>

#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/xml++.h"
#include "pbd/enumwriter.h"

#include "evoral/Curve.hpp"

#include "ardour/session.h"
#include "ardour/panner.h"
#include "ardour/utils.h"
#include "ardour/audio_buffer.h"

#include "ardour/runtime_functions.h"
#include "ardour/buffer_set.h"
#include "ardour/audio_buffer.h"

#include "i18n.h"

#include "pbd/mathfix.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

float Panner::current_automation_version_number = 1.0;

string EqualPowerStereoPanner::name = "Equal Power Stereo";
string Multi2dPanner::name = "Multiple (2D)";

/* this is a default mapper of  control values to a pan position
   others can be imagined.
*/

static pan_t direct_control_to_pan (double fract) {
	return fract;
}


//static double direct_pan_to_control (pan_t val) {
//	return val;
//}

StreamPanner::StreamPanner (Panner& p, Evoral::Parameter param)
	: parent (p)
{
	assert(param.type() != NullAutomation);

	_muted = false;
	_mono = false;

	_control = boost::dynamic_pointer_cast<AutomationControl>( parent.control( param, true ) );

	x = 0.5;
	y = 0.5;
	z = 0.5;
}

StreamPanner::~StreamPanner ()
{
}

void
StreamPanner::set_mono (bool yn)
{
	if (yn != _mono) {
		_mono = yn;
		StateChanged ();
	}
}

void
Panner::PanControllable::set_value (float val)
{
	panner.streampanner(parameter().id()).set_position (direct_control_to_pan (val));
	AutomationControl::set_value(val);
}

float
Panner::PanControllable::get_value (void) const
{
	return AutomationControl::get_value();
}

void
StreamPanner::set_muted (bool yn)
{
	if (yn != _muted) {
		_muted = yn;
		StateChanged ();
	}
}

void
StreamPanner::set_position (float xpos, bool link_call)
{
	if (!link_call && parent.linked()) {
		parent.set_position (xpos, *this);
	}

	if (x != xpos) {
		x = xpos;
		update ();
		Changed ();
		_control->Changed ();
	}
}

void
StreamPanner::set_position (float xpos, float ypos, bool link_call)
{
	if (!link_call && parent.linked()) {
		parent.set_position (xpos, ypos, *this);
	}

	if (x != xpos || y != ypos) {

		x = xpos;
		y = ypos;
		update ();
		Changed ();
	}
}

void
StreamPanner::set_position (float xpos, float ypos, float zpos, bool link_call)
{
	if (!link_call && parent.linked()) {
		parent.set_position (xpos, ypos, zpos, *this);
	}

	if (x != xpos || y != ypos || z != zpos) {
		x = xpos;
		y = ypos;
		z = zpos;
		update ();
		Changed ();
	}
}

int
StreamPanner::set_state (const XMLNode& node, int /*version*/)
{
	const XMLProperty* prop;
	XMLNodeConstIterator iter;

	if ((prop = node.property (X_("muted")))) {
		set_muted (string_is_affirmative (prop->value()));
	}

	return 0;
}

void
StreamPanner::add_state (XMLNode& node)
{
	node.add_property (X_("muted"), (muted() ? "yes" : "no"));
}

void
StreamPanner::distribute (AudioBuffer& src, BufferSet& obufs, gain_t gain_coeff, nframes_t nframes)
{
	if (_mono) {
		/* we're in mono mode, so just pan the input to all outputs equally */
		int const N = parent.nouts ();
		for (int i = 0; i < N; ++i) {
			mix_buffers_with_gain (obufs.get_audio(i).data(), src.data(), nframes, gain_coeff);
		}
	} else {
		/* normal mode, call the `real' distribute method */
		do_distribute (src, obufs, gain_coeff, nframes);
	}
}

void
StreamPanner::distribute_automated (AudioBuffer& src, BufferSet& obufs,
				    nframes_t start, nframes_t end, nframes_t nframes, pan_t** buffers)
{
	if (_mono) {
		/* we're in mono mode, so just pan the input to all outputs equally */
		int const N = parent.nouts ();
		for (int i = 0; i < N; ++i) {
			mix_buffers_with_gain (obufs.get_audio(i).data(), src.data(), nframes, 1.0);
		}
	} else {
		/* normal mode, call the `real' distribute method */
		do_distribute_automated (src, obufs, start, end, nframes, buffers);
	}
	
}


/*---------------------------------------------------------------------- */

BaseStereoPanner::BaseStereoPanner (Panner& p, Evoral::Parameter param)
	: StreamPanner (p, param)
{
}

BaseStereoPanner::~BaseStereoPanner ()
{
}

int
BaseStereoPanner::load (istream& in, string path, uint32_t& linecnt)
{
	char line[128];
	LocaleGuard lg (X_("POSIX"));

	_control->list()->clear ();

	while (in.getline (line, sizeof (line), '\n')) {
		nframes_t when;
		double value;

		++linecnt;

		if (strcmp (line, "end") == 0) {
			break;
		}

		if (sscanf (line, "%" PRIu32 " %lf", &when, &value) != 2) {
			warning << string_compose(_("badly formatted pan automation event record at line %1 of %2 (ignored) [%3]"), linecnt, path, line) << endmsg;
			continue;
		}

		_control->list()->fast_simple_add (when, value);
	}

	/* now that we are done loading */

	((AutomationList*)_control->list().get())->StateChanged ();

	return 0;
}

void
BaseStereoPanner::do_distribute (AudioBuffer& srcbuf, BufferSet& obufs, gain_t gain_coeff, nframes_t nframes)
{
	assert(obufs.count().n_audio() == 2);

	pan_t delta;
	Sample* dst;
	pan_t pan;

	if (_muted) {
		return;
	}

	Sample* const src = srcbuf.data();

	/* LEFT */

	dst = obufs.get_audio(0).data();

	if (fabsf ((delta = (left - desired_left))) > 0.002) { // about 1 degree of arc

		/* we've moving the pan by an appreciable amount, so we must
		   interpolate over 64 frames or nframes, whichever is smaller */

		nframes_t const limit = min ((nframes_t)64, nframes);
		nframes_t n;

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

			/* mark that we wrote into the buffer */

			// obufs[0] = 0;
		}
	}

	/* RIGHT */

	dst = obufs.get_audio(1).data();

	if (fabsf ((delta = (right - desired_right))) > 0.002) { // about 1 degree of arc

		/* we're moving the pan by an appreciable amount, so we must
		   interpolate over 64 frames or nframes, whichever is smaller */

		nframes_t const limit = min ((nframes_t)64, nframes);
		nframes_t n;

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

/*---------------------------------------------------------------------- */

EqualPowerStereoPanner::EqualPowerStereoPanner (Panner& p, Evoral::Parameter param)
	: BaseStereoPanner (p, param)
{
	update ();

	left = desired_left;
	right = desired_right;
	left_interp = left;
	right_interp = right;
}

EqualPowerStereoPanner::~EqualPowerStereoPanner ()
{
}

void
EqualPowerStereoPanner::update ()
{
	/* it would be very nice to split this out into a virtual function
	   that can be accessed from BaseStereoPanner and used in do_distribute_automated().

	   but the place where its used in do_distribute_automated() is a tight inner loop,
	   and making "nframes" virtual function calls to compute values is an absurd
	   overhead.
	*/

	/* x == 0 => hard left
	   x == 1 => hard right
	*/

	float panR = x;
	float panL = 1 - panR;

	const float pan_law_attenuation = -3.0f;
	const float scale = 2.0f - 4.0f * powf (10.0f,pan_law_attenuation/20.0f);

	desired_left = panL * (scale * panL + 1.0f - scale);
	desired_right = panR * (scale * panR + 1.0f - scale);

	effective_x = x;
	//_control->set_value(x);
}

void
EqualPowerStereoPanner::do_distribute_automated (AudioBuffer& srcbuf, BufferSet& obufs,
						 nframes_t start, nframes_t end, nframes_t nframes,
						 pan_t** buffers)
{
	assert(obufs.count().n_audio() == 2);

	Sample* dst;
	pan_t* pbuf;
	Sample* const src = srcbuf.data();

	/* fetch positional data */

	if (!_control->list()->curve().rt_safe_get_vector (start, end, buffers[0], nframes)) {
		/* fallback */
		if (!_muted) {
			do_distribute (srcbuf, obufs, 1.0, nframes);
		}
		return;
	}

	/* store effective pan position. do this even if we are muted */

	if (nframes > 0) {
		effective_x = buffers[0][nframes-1];
	}

	if (_muted) {
		return;
	}

	/* apply pan law to convert positional data into pan coefficients for
	   each buffer (output)
	*/

	const float pan_law_attenuation = -3.0f;
	const float scale = 2.0f - 4.0f * powf (10.0f,pan_law_attenuation/20.0f);

	for (nframes_t n = 0; n < nframes; ++n) {

		float panR = buffers[0][n];
		float panL = 1 - panR;

		buffers[0][n] = panL * (scale * panL + 1.0f - scale);
		buffers[1][n] = panR * (scale * panR + 1.0f - scale);
	}

	/* LEFT */

	dst = obufs.get_audio(0).data();
	pbuf = buffers[0];

	for (nframes_t n = 0; n < nframes; ++n) {
		dst[n] += src[n] * pbuf[n];
	}

	/* XXX it would be nice to mark the buffer as written to */

	/* RIGHT */

	dst = obufs.get_audio(1).data();
	pbuf = buffers[1];

	for (nframes_t n = 0; n < nframes; ++n) {
		dst[n] += src[n] * pbuf[n];
	}

	/* XXX it would be nice to mark the buffer as written to */
}

StreamPanner*
EqualPowerStereoPanner::factory (Panner& parent, Evoral::Parameter param)
{
	return new EqualPowerStereoPanner (parent, param);
}

XMLNode&
EqualPowerStereoPanner::get_state (void)
{
	return state (true);
}

XMLNode&
EqualPowerStereoPanner::state (bool /*full_state*/)
{
	XMLNode* root = new XMLNode ("StreamPanner");
	char buf[64];
	LocaleGuard lg (X_("POSIX"));

	snprintf (buf, sizeof (buf), "%.12g", x);
	root->add_property (X_("x"), buf);
	root->add_property (X_("type"), EqualPowerStereoPanner::name);

	// XXX: dont save automation here... its part of the automatable panner now.

	StreamPanner::add_state (*root);

	root->add_child_nocopy (_control->get_state ());

	return *root;
}

int
EqualPowerStereoPanner::set_state (const XMLNode& node, int version)
{
	const XMLProperty* prop;
	float pos;
	LocaleGuard lg (X_("POSIX"));

	if ((prop = node.property (X_("x")))) {
		pos = atof (prop->value().c_str());
		set_position (pos, true);
	}

	StreamPanner::set_state (node, version);

	for (XMLNodeConstIterator iter = node.children().begin(); iter != node.children().end(); ++iter) {

		if ((*iter)->name() == X_("Controllable")) {
			if ((prop = (*iter)->property("name")) != 0 && prop->value() == "panner") {
				_control->set_state (**iter, version);
			}

		} else if ((*iter)->name() == X_("Automation")) {

			_control->alist()->set_state (*((*iter)->children().front()), version);

			if (_control->alist()->automation_state() != Off) {
				set_position (_control->list()->eval (parent.session().transport_frame()));
			}
		}
	}

	return 0;
}

/*----------------------------------------------------------------------*/

Multi2dPanner::Multi2dPanner (Panner& p, Evoral::Parameter param)
	: StreamPanner (p, param)
{
	update ();
}

Multi2dPanner::~Multi2dPanner ()
{
}

void
Multi2dPanner::update ()
{
	static const float BIAS = FLT_MIN;
	uint32_t i;
	uint32_t nouts = parent.outputs.size();
	float dsq[nouts];
	float f, fr;
	vector<pan_t> pans;

	f = 0.0f;

	for (i = 0; i < nouts; i++) {
		dsq[i] = ((x - parent.outputs[i].x) * (x - parent.outputs[i].x) + (y - parent.outputs[i].y) * (y - parent.outputs[i].y) + BIAS);
		if (dsq[i] < 0.0) {
			dsq[i] = 0.0;
		}
		f += dsq[i] * dsq[i];
	}
#ifdef __APPLE__
	// terrible hack to support OSX < 10.3.9 builds
	fr = (float) (1.0 / sqrt((double)f));
#else
	fr = 1.0 / sqrtf(f);
#endif
	for (i = 0; i < nouts; ++i) {
		parent.outputs[i].desired_pan = 1.0f - (dsq[i] * fr);
	}

	effective_x = x;
}

void
Multi2dPanner::do_distribute (AudioBuffer& srcbuf, BufferSet& obufs, gain_t gain_coeff, nframes_t nframes)
{
	Sample* dst;
	pan_t pan;
	vector<Panner::Output>::iterator o;
	uint32_t n;

	if (_muted) {
		return;
	}

	Sample* const src = srcbuf.data();


	for (n = 0, o = parent.outputs.begin(); o != parent.outputs.end(); ++o, ++n) {

		dst = obufs.get_audio(n).data();

#ifdef CAN_INTERP
		if (fabsf ((delta = (left_interp - desired_left))) > 0.002) { // about 1 degree of arc

			/* interpolate over 64 frames or nframes, whichever is smaller */

			nframes_t limit = min ((nframes_t)64, nframes);
			nframes_t n;

			delta = -(delta / (float) (limit));

			for (n = 0; n < limit; n++) {
				left_interp = left_interp + delta;
				left = left_interp + 0.9 * (left - left_interp);
				dst[n] += src[n] * left * gain_coeff;
			}

			pan = left * gain_coeff;
			mix_buffers_with_gain(dst+n,src+n,nframes-n,pan);

		} else {

#else
			pan = (*o).desired_pan;

			if ((pan *= gain_coeff) != 1.0f) {

				if (pan != 0.0f) {
					mix_buffers_with_gain(dst,src,nframes,pan);
				}
			} else {
					mix_buffers_no_gain(dst,src,nframes);
			}
#endif
#ifdef CAN_INTERP
		}
#endif
	}

	return;
}

void
Multi2dPanner::do_distribute_automated (AudioBuffer& /*src*/, BufferSet& /*obufs*/,
					nframes_t /*start*/, nframes_t /*end*/, nframes_t /*nframes*/,
					pan_t** /*buffers*/)
{
	if (_muted) {
		return;
	}

	/* what ? */

	return;
}

StreamPanner*
Multi2dPanner::factory (Panner& p, Evoral::Parameter param)
{
	return new Multi2dPanner (p, param);
}

int
Multi2dPanner::load (istream& /*in*/, string /*path*/, uint32_t& /*linecnt*/)
{
	return 0;
}

XMLNode&
Multi2dPanner::get_state (void)
{
	return state (true);
}

XMLNode&
Multi2dPanner::state (bool /*full_state*/)
{
	XMLNode* root = new XMLNode ("StreamPanner");
	char buf[64];
	LocaleGuard lg (X_("POSIX"));

	snprintf (buf, sizeof (buf), "%.12g", x);
	root->add_property (X_("x"), buf);
	snprintf (buf, sizeof (buf), "%.12g", y);
	root->add_property (X_("y"), buf);
	root->add_property (X_("type"), Multi2dPanner::name);

	/* XXX no meaningful automation yet */

	return *root;
}

int
Multi2dPanner::set_state (const XMLNode& node, int /*version*/)
{
	const XMLProperty* prop;
	float newx,newy;
	LocaleGuard lg (X_("POSIX"));

	newx = -1;
	newy = -1;

	if ((prop = node.property (X_("x")))) {
		newx = atof (prop->value().c_str());
	}

	if ((prop = node.property (X_("y")))) {
		newy = atof (prop->value().c_str());
	}

	if (x < 0 || y < 0) {
		error << _("badly-formed positional data for Multi2dPanner - ignored")
		      << endmsg;
		return -1;
	}

	set_position (newx, newy);
	return 0;
}

/*---------------------------------------------------------------------- */

Panner::Panner (string name, Session& s)
	: SessionObject (s, name)
	, AutomatableControls (s)
{
	//set_name_old_auto (name);
	set_name (name);

	_linked = false;
	_link_direction = SameDirection;
	_bypassed = false;
}

Panner::~Panner ()
{
}

void
Panner::set_linked (bool yn)
{
	if (yn != _linked) {
		_linked = yn;
		_session.set_dirty ();
		LinkStateChanged (); /* EMIT SIGNAL */
	}
}

void
Panner::set_link_direction (LinkDirection ld)
{
	if (ld != _link_direction) {
		_link_direction = ld;
		_session.set_dirty ();
		LinkStateChanged (); /* EMIT SIGNAL */
	}
}


void
Panner::set_bypassed (bool yn)
{
	if (yn != _bypassed) {
		_bypassed = yn;
		StateChanged ();
	}
}


void
Panner::reset_to_default ()
{
	vector<float> positions;

	switch (outputs.size()) {
	case 0:
	case 1:
		return;
	}

	if (outputs.size() == 2) {
		switch (_streampanners.size()) {
		case 1:
			_streampanners.front()->set_position (0.5);
			_streampanners.front()->pan_control()->list()->reset_default (0.5);
			return;
			break;
		case 2:
			_streampanners.front()->set_position (0.0);
			_streampanners.front()->pan_control()->list()->reset_default (0.0);
			_streampanners.back()->set_position (1.0);
			_streampanners.back()->pan_control()->list()->reset_default (1.0);
			return;
		default:
			break;
		}
	}

	vector<Output>::iterator o;
	vector<StreamPanner*>::iterator p;

	for (o = outputs.begin(), p = _streampanners.begin(); o != outputs.end() && p != _streampanners.end(); ++o, ++p) {
		(*p)->set_position ((*o).x, (*o).y);
	}
}

void
Panner::reset_streampanner (uint32_t which)
{
	if (which >= _streampanners.size() || which >= outputs.size()) {
		return;
	}

	switch (outputs.size()) {
	case 0:
	case 1:
		return;

	case 2:
		switch (_streampanners.size()) {
		case 1:
			/* stereo out, 1 stream, default = middle */
			_streampanners.front()->set_position (0.5);
			_streampanners.front()->pan_control()->list()->reset_default (0.5);
			break;
		case 2:
			/* stereo out, 2 streams, default = hard left/right */
			if (which == 0) {
				_streampanners.front()->set_position (0.0);
				_streampanners.front()->pan_control()->list()->reset_default (0.0);
			} else {
				_streampanners.back()->set_position (1.0);
				_streampanners.back()->pan_control()->list()->reset_default (1.0);
			}
			break;
		}
		return;

	default:
		_streampanners[which]->set_position (outputs[which].x, outputs[which].y);
	}
}

/**
 *    Reset the panner with a given number of outs and panners (and hence inputs)
 *
 *    \param nouts Number of outputs.
 *    \param npans Number of panners.
 */
void
Panner::reset (uint32_t nouts, uint32_t npans)
{
	uint32_t n;
	bool changed = false;
	bool do_not_and_did_not_need_panning = ((nouts < 2) && (outputs.size() < 2));

	//cout << "Reset panner for " << nouts << " " << npans << "\n";

	/* if new and old config don't need panning, or if
	   the config hasn't changed, we're done.
	*/

	if (do_not_and_did_not_need_panning ||
	    ((nouts == outputs.size()) && (npans == _streampanners.size()))) {
		return;
	}

	n = _streampanners.size();
	clear_panners ();

	if (n != npans) {
		changed = true;
	}

	n = outputs.size();
	outputs.clear ();

	if (n != nouts) {
		changed = true;
	}

	if (nouts < 2) {
		/* no need for panning with less than 2 outputs */
		goto send_changed;
	}

	switch (nouts) {
	case 0:
		/* XXX: this can never happen */
		break;

	case 1:
		/* XXX: this can never happen */
		fatal << _("programming error:")
		      << X_("Panner::reset() called with a single output")
		      << endmsg;
		/*NOTREACHED*/
		break;

	case 2: // line
		outputs.push_back (Output (0, 0));
		outputs.push_back (Output (1.0, 0));

		for (n = 0; n < npans; ++n) {
			_streampanners.push_back (new EqualPowerStereoPanner (*this, Evoral::Parameter(PanAutomation, 0, n)));
		}
		break;

	case 3: // triangle
		outputs.push_back (Output  (0.5, 0));
		outputs.push_back (Output  (0, 1.0));
		outputs.push_back (Output  (1.0, 1.0));

		for (n = 0; n < npans; ++n) {
			_streampanners.push_back (new Multi2dPanner (*this, Evoral::Parameter(PanAutomation, 0, n)));
		}

		break;

	case 4: // square
		outputs.push_back (Output  (0, 0));
		outputs.push_back (Output  (1.0, 0));
		outputs.push_back (Output  (1.0, 1.0));
		outputs.push_back (Output  (0, 1.0));

		for (n = 0; n < npans; ++n) {
			_streampanners.push_back (new Multi2dPanner (*this, Evoral::Parameter(PanAutomation, 0, n)));
		}

		break;

	case 5: //square+offcenter center
		outputs.push_back (Output  (0, 0));
		outputs.push_back (Output  (1.0, 0));
		outputs.push_back (Output  (1.0, 1.0));
		outputs.push_back (Output  (0, 1.0));
		outputs.push_back (Output  (0.5, 0.75));

		for (n = 0; n < npans; ++n) {
			_streampanners.push_back (new Multi2dPanner (*this, Evoral::Parameter(PanAutomation, 0, n)));
		}

		break;

	default:
		/* XXX horrible placement. FIXME */
		for (n = 0; n < nouts; ++n) {
			outputs.push_back (Output (0.1 * n, 0.1 * n));
		}

		for (n = 0; n < npans; ++n) {
			_streampanners.push_back (new Multi2dPanner (*this, Evoral::Parameter(PanAutomation, 0, n)));
		}

		break;
	}

	for (std::vector<StreamPanner*>::iterator x = _streampanners.begin(); x != _streampanners.end(); ++x) {
		(*x)->update ();
	}

	/* force hard left/right panning in a common case: 2in/2out
	*/

	if (npans == 2 && outputs.size() == 2) {

		/* Do this only if we changed configuration, or our configuration
		   appears to be the default set up (center).
		*/

		float left;
		float right;

		_streampanners.front()->get_position (left);
		_streampanners.back()->get_position (right);

		if (changed || ((left == 0.5) && (right == 0.5))) {

			_streampanners.front()->set_position (0.0);
			_streampanners.front()->pan_control()->list()->reset_default (0.0);

			_streampanners.back()->set_position (1.0);
			_streampanners.back()->pan_control()->list()->reset_default (1.0);

			changed = true;
		}
	}

  send_changed:
	if (changed) {
		Changed (); /* EMIT SIGNAL */
	}

	return;
}

void
Panner::remove (uint32_t which)
{
	vector<StreamPanner*>::iterator i;
	for (i = _streampanners.begin(); i != _streampanners.end() && which; ++i, --which) {}

	if (i != _streampanners.end()) {
		delete *i;
		_streampanners.erase (i);
	}
}


/** Remove all our StreamPanners */
void
Panner::clear_panners ()
{
	for (vector<StreamPanner*>::iterator i = _streampanners.begin(); i != _streampanners.end(); ++i) {
		delete *i;
	}

	_streampanners.clear ();
}

void
Panner::set_automation_style (AutoStyle style)
{
	for (vector<StreamPanner*>::iterator i = _streampanners.begin(); i != _streampanners.end(); ++i) {
		((AutomationList*)(*i)->pan_control()->list().get())->set_automation_style (style);
	}
	_session.set_dirty ();
}

void
Panner::set_automation_state (AutoState state)
{
	for (vector<StreamPanner*>::iterator i = _streampanners.begin(); i != _streampanners.end(); ++i) {
		((AutomationList*)(*i)->pan_control()->list().get())->set_automation_state (state);
	}
	_session.set_dirty ();
}

AutoState
Panner::automation_state () const
{
	boost::shared_ptr<AutomationList> l;
	if (!empty()) {
		boost::shared_ptr<AutomationControl> control = _streampanners.front()->pan_control();
		if (control) {
			l = boost::dynamic_pointer_cast<AutomationList>(control->list());
		}
	}

	return l ? l->automation_state() : Off;
}

AutoStyle
Panner::automation_style () const
{
	boost::shared_ptr<AutomationList> l;
	if (!empty()) {
		boost::shared_ptr<AutomationControl> control = _streampanners.front()->pan_control();
		if (control) {
			l = boost::dynamic_pointer_cast<AutomationList>(control->list());
		}
	}

	return l ? l->automation_style() : Absolute;
}

struct PanPlugins {
    string name;
    uint32_t nouts;
    StreamPanner* (*factory)(Panner&, Evoral::Parameter);
};

PanPlugins pan_plugins[] = {
	{ EqualPowerStereoPanner::name, 2, EqualPowerStereoPanner::factory },
	{ Multi2dPanner::name, 3, Multi2dPanner::factory },
	{ string (""), 0, 0 }
};

XMLNode&
Panner::get_state (void)
{
	return state (true);
}

XMLNode&
Panner::state (bool full)
{
	XMLNode* node = new XMLNode ("Panner");

	char buf[32];

	node->add_property (X_("linked"), (_linked ? "yes" : "no"));
	node->add_property (X_("link_direction"), enum_2_string (_link_direction));
	node->add_property (X_("bypassed"), (bypassed() ? "yes" : "no"));

	for (vector<Panner::Output>::iterator o = outputs.begin(); o != outputs.end(); ++o) {
		XMLNode* onode = new XMLNode (X_("Output"));
		snprintf (buf, sizeof (buf), "%.12g", (*o).x);
		onode->add_property (X_("x"), buf);
		snprintf (buf, sizeof (buf), "%.12g", (*o).y);
		onode->add_property (X_("y"), buf);
		node->add_child_nocopy (*onode);
	}

	for (vector<StreamPanner*>::const_iterator i = _streampanners.begin(); i != _streampanners.end(); ++i) {
		node->add_child_nocopy ((*i)->state (full));
	}


	return *node;
}

int
Panner::set_state (const XMLNode& node, int version)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	const XMLProperty *prop;
	uint32_t i;
	uint32_t num_panners = 0;
	StreamPanner* sp;
	LocaleGuard lg (X_("POSIX"));

	clear_panners ();

	ChanCount ins = ChanCount::ZERO;
	ChanCount outs = ChanCount::ZERO;

	// XXX: this might not be necessary anymore
	outputs.clear ();

	if ((prop = node.property (X_("linked"))) != 0) {
		set_linked (string_is_affirmative (prop->value()));
	}


	if ((prop = node.property (X_("bypassed"))) != 0) {
		set_bypassed (string_is_affirmative (prop->value()));
	}

	if ((prop = node.property (X_("link_direction"))) != 0) {
		LinkDirection ld; /* here to provide type information */
		set_link_direction (LinkDirection (string_2_enum (prop->value(), ld)));
	}

	nlist = node.children();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == X_("Output")) {

			float x, y;

			prop = (*niter)->property (X_("x"));
			sscanf (prop->value().c_str(), "%g", &x);

			prop = (*niter)->property (X_("y"));
			sscanf (prop->value().c_str(), "%g", &y);

			outputs.push_back (Output (x, y));
		}
	}

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		if ((*niter)->name() == X_("StreamPanner")) {

			if ((prop = (*niter)->property (X_("type")))) {

				for (i = 0; pan_plugins[i].factory; ++i) {
					if (prop->value() == pan_plugins[i].name) {


						/* note that we assume that all the stream panners
						   are of the same type. pretty good
						   assumption, but it's still an assumption.
						*/

						sp = pan_plugins[i].factory (*this, Evoral::Parameter(PanAutomation, 0, num_panners));
						num_panners++;

						if (sp->set_state (**niter, version) == 0) {
							_streampanners.push_back (sp);
						}

						break;
					}
				}


				if (!pan_plugins[i].factory) {
					error << string_compose (_("Unknown panner plugin \"%1\" found in pan state - ignored"),
							  prop->value())
					      << endmsg;
				}

			} else {
				error << _("panner plugin node has no type information!")
				      << endmsg;
				return -1;
			}

		}
	}

	reset (outputs.size (), num_panners);
	/* don't try to do old-school automation loading if it wasn't marked as existing */

	if ((prop = node.property (X_("automation")))) {

		/* automation path is relative */

		automation_path = Glib::build_filename(_session.automation_dir(), prop->value ());
	}

	return 0;
}

bool
Panner::touching () const
{
	for (vector<StreamPanner*>::const_iterator i = _streampanners.begin(); i != _streampanners.end(); ++i) {
		if (((AutomationList*)(*i)->pan_control()->list().get())->touching ()) {
			return true;
		}
	}

	return false;
}

void
Panner::set_position (float xpos, StreamPanner& orig)
{
	float xnow;
	float xdelta ;
	float xnew;

	orig.get_position (xnow);
	xdelta = xpos - xnow;

	if (_link_direction == SameDirection) {

		for (vector<StreamPanner*>::iterator i = _streampanners.begin(); i != _streampanners.end(); ++i) {
			if (*i == &orig) {
				(*i)->set_position (xpos, true);
			} else {
				(*i)->get_position (xnow);
				xnew = min (1.0f, xnow + xdelta);
				xnew = max (0.0f, xnew);
				(*i)->set_position (xnew, true);
			}
		}

	} else {

		for (vector<StreamPanner*>::iterator i = _streampanners.begin(); i != _streampanners.end(); ++i) {
			if (*i == &orig) {
				(*i)->set_position (xpos, true);
			} else {
				(*i)->get_position (xnow);
				xnew = min (1.0f, xnow - xdelta);
				xnew = max (0.0f, xnew);
				(*i)->set_position (xnew, true);
			}
		}
	}
}

void
Panner::set_position (float xpos, float ypos, StreamPanner& orig)
{
	float xnow, ynow;
	float xdelta, ydelta;
	float xnew, ynew;

	orig.get_position (xnow, ynow);
	xdelta = xpos - xnow;
	ydelta = ypos - ynow;

	if (_link_direction == SameDirection) {

		for (vector<StreamPanner*>::iterator i = _streampanners.begin(); i != _streampanners.end(); ++i) {
			if (*i == &orig) {
				(*i)->set_position (xpos, ypos, true);
			} else {
				(*i)->get_position (xnow, ynow);

				xnew = min (1.0f, xnow + xdelta);
				xnew = max (0.0f, xnew);

				ynew = min (1.0f, ynow + ydelta);
				ynew = max (0.0f, ynew);

				(*i)->set_position (xnew, ynew, true);
			}
		}

	} else {

		for (vector<StreamPanner*>::iterator i = _streampanners.begin(); i != _streampanners.end(); ++i) {
			if (*i == &orig) {
				(*i)->set_position (xpos, ypos, true);
			} else {
				(*i)->get_position (xnow, ynow);

				xnew = min (1.0f, xnow - xdelta);
				xnew = max (0.0f, xnew);

				ynew = min (1.0f, ynow - ydelta);
				ynew = max (0.0f, ynew);

				(*i)->set_position (xnew, ynew, true);
			}
		}
	}
}

void
Panner::set_position (float xpos, float ypos, float zpos, StreamPanner& orig)
{
	float xnow, ynow, znow;
	float xdelta, ydelta, zdelta;
	float xnew, ynew, znew;

	orig.get_position (xnow, ynow, znow);
	xdelta = xpos - xnow;
	ydelta = ypos - ynow;
	zdelta = zpos - znow;

	if (_link_direction == SameDirection) {

		for (vector<StreamPanner*>::iterator i = _streampanners.begin(); i != _streampanners.end(); ++i) {
			if (*i == &orig) {
				(*i)->set_position (xpos, ypos, zpos, true);
			} else {
				(*i)->get_position (xnow, ynow, znow);

				xnew = min (1.0f, xnow + xdelta);
				xnew = max (0.0f, xnew);

				ynew = min (1.0f, ynow + ydelta);
				ynew = max (0.0f, ynew);

				znew = min (1.0f, znow + zdelta);
				znew = max (0.0f, znew);

				(*i)->set_position (xnew, ynew, znew, true);
			}
		}

	} else {

		for (vector<StreamPanner*>::iterator i = _streampanners.begin(); i != _streampanners.end(); ++i) {
			if (*i == &orig) {
				(*i)->set_position (xpos, ypos, true);
			} else {
				(*i)->get_position (xnow, ynow, znow);

				xnew = min (1.0f, xnow - xdelta);
				xnew = max (0.0f, xnew);

				ynew = min (1.0f, ynow - ydelta);
				ynew = max (0.0f, ynew);

				znew = min (1.0f, znow + zdelta);
				znew = max (0.0f, znew);

				(*i)->set_position (xnew, ynew, znew, true);
			}
		}
	}
}

void
Panner::distribute_no_automation (BufferSet& inbufs, BufferSet& outbufs, nframes_t nframes, gain_t gain_coeff)
{
	if (outbufs.count().n_audio() == 0) {
		// Don't want to lose audio...
		assert(inbufs.count().n_audio() == 0);
		return;
	}

	// We shouldn't be called in the first place...
	assert(!bypassed());
	assert(!empty());


	if (outbufs.count().n_audio() == 1) {

		AudioBuffer& dst = outbufs.get_audio(0);

		if (gain_coeff == 0.0f) {

			/* only one output, and gain was zero, so make it silent */

			dst.silence (nframes);

		} else if (gain_coeff == 1.0f){

			/* mix all buffers into the output */

			// copy the first
			dst.read_from(inbufs.get_audio(0), nframes);

			// accumulate starting with the second
			if (inbufs.count().n_audio() > 0) {
				BufferSet::audio_iterator i = inbufs.audio_begin();
				for (++i; i != inbufs.audio_end(); ++i) {
					dst.merge_from(*i, nframes);
				}
			}

		} else {

			/* mix all buffers into the output, scaling them all by the gain */

			// copy the first
			dst.read_from(inbufs.get_audio(0), nframes);

			// accumulate (with gain) starting with the second
			if (inbufs.count().n_audio() > 0) {
				BufferSet::audio_iterator i = inbufs.audio_begin();
				for (++i; i != inbufs.audio_end(); ++i) {
					dst.accumulate_with_gain_from(*i, nframes, gain_coeff);
				}
			}

		}

		return;
	}

	/* the terrible silence ... */
	for (BufferSet::audio_iterator i = outbufs.audio_begin(); i != outbufs.audio_end(); ++i) {
		i->silence(nframes);
	}

	BufferSet::audio_iterator i = inbufs.audio_begin();

	for (vector<StreamPanner*>::iterator pan = _streampanners.begin(); pan != _streampanners.end() && i != inbufs.audio_end(); ++pan, ++i) {
		(*pan)->distribute (*i, outbufs, gain_coeff, nframes);
	}
}

void
Panner::run (BufferSet& inbufs, BufferSet& outbufs, sframes_t start_frame, sframes_t end_frame, nframes_t nframes)
{
	if (outbufs.count().n_audio() == 0) {
		// Failing to deliver audio we were asked to deliver is a bug
		assert(inbufs.count().n_audio() == 0);
		return;
	}

	// We shouldn't be called in the first place...
	assert(!bypassed());
	assert(!empty());

	// If we shouldn't play automation defer to distribute_no_automation
	if (!(automation_state() & Play || ((automation_state() & Touch) && !touching()))) {

		// Speed quietning
		gain_t gain_coeff = 1.0;

		if (fabsf(_session.transport_speed()) > 1.5f) {
			gain_coeff = speed_quietning;
		}

		distribute_no_automation (inbufs, outbufs, nframes, gain_coeff);
		return;
	}

	// Otherwise.. let the automation flow, baby

	if (outbufs.count().n_audio() == 1) {

		AudioBuffer& dst = outbufs.get_audio(0);

		// FIXME: apply gain automation?

		// copy the first
		dst.read_from(inbufs.get_audio(0), nframes);

		// accumulate starting with the second
		BufferSet::audio_iterator i = inbufs.audio_begin();
		for (++i; i != inbufs.audio_end(); ++i) {
			dst.merge_from(*i, nframes);
		}

		return;
	}

	// More than 1 output, we should have 1 panner for each input
	//assert(_streampanners.size() == inbufs.count().n_audio());

	/* the terrible silence ... */
	for (BufferSet::audio_iterator i = outbufs.audio_begin(); i != outbufs.audio_end(); ++i) {
		i->silence(nframes);
	}

	BufferSet::audio_iterator i = inbufs.audio_begin();
	for (vector<StreamPanner*>::iterator pan = _streampanners.begin(); pan != _streampanners.end(); ++pan, ++i) {
		(*pan)->distribute_automated (*i, outbufs, start_frame, end_frame, nframes, _session.pan_automation_buffer());
	}
}

/* old school automation handling */

/*
void
Panner::set_name (string str)
{
	automation_path = Glib::build_filename(_session.automation_dir(),
		_session.snap_name() + "-pan-" + legalize_for_path (str) + ".automation");
}
*/

int
Panner::load ()
{
	char line[128];
	uint32_t linecnt = 0;
	float version;
	vector<StreamPanner*>::iterator sp;
	LocaleGuard lg (X_("POSIX"));

	if (automation_path.length() == 0) {
		return 0;
	}

	if (access (automation_path.c_str(), F_OK)) {
		return 0;
	}

	ifstream in (automation_path.c_str());

	if (!in) {
		error << string_compose (_("cannot open pan automation file %1 (%2)"),
				  automation_path, strerror (errno))
		      << endmsg;
		return -1;
	}

	sp = _streampanners.begin();

	while (in.getline (line, sizeof(line), '\n')) {

		if (++linecnt == 1) {
			if (memcmp (line, X_("version"), 7) == 0) {
				if (sscanf (line, "version %f", &version) != 1) {
					error << string_compose(_("badly formed version number in pan automation event file \"%1\""), automation_path) << endmsg;
					return -1;
				}
			} else {
				error << string_compose(_("no version information in pan automation event file \"%1\" (first line = %2)"),
						 automation_path, line) << endmsg;
				return -1;
			}

			continue;
		}

		if (strlen (line) == 0 || line[0] == '#') {
			continue;
		}

		if (strcmp (line, "begin") == 0) {

			if (sp == _streampanners.end()) {
				error << string_compose (_("too many panner states found in pan automation file %1"),
						  automation_path)
				      << endmsg;
				return -1;
			}

			if ((*sp)->load (in, automation_path, linecnt)) {
				return -1;
			}

			++sp;
		}
	}

	return 0;
}

void
Panner::set_mono (bool yn)
{
	if (yn != _mono) {
		_mono = yn;
		StateChanged ();
	}

	for (vector<StreamPanner*>::iterator i = _streampanners.begin(); i != _streampanners.end(); ++i) {
		(*i)->set_mono (yn);
	}
}
