
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
#include "ardour/vbap.h"

#include "i18n.h"

#include "pbd/mathfix.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

float Panner::current_automation_version_number = 1.0;

string EqualPowerStereoPanner::name = "Equal Power Stereo";

/* this is a default mapper of control values to a pan position
   others can be imagined.
*/

static double direct_control_to_stereo_pan (double fract)
{
	return BaseStereoPanner::lr_fract_to_azimuth (fract);
}

StreamPanner::StreamPanner (Panner& p, Evoral::Parameter param)
	: parent (p)
{
	assert (param.type() != NullAutomation);

	_muted = false;
	_mono = false;

	/* get our AutomationControl from our parent Panner, creating it if required */
	_control = boost::dynamic_pointer_cast<AutomationControl> (parent.control (param, true));
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
Panner::PanControllable::set_value (double val)
{
	panner.streampanner (parameter().id()).set_position (AngularVector (direct_control_to_stereo_pan (val), 0.0));
	AutomationControl::set_value(val);
}

double
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
StreamPanner::set_position (const AngularVector& av, bool link_call)
{
	if (!link_call && parent.linked()) {
		parent.set_position (av, *this);
	}

	if (_angles != av) {
		_angles = av;
		update ();
		Changed ();
		_control->Changed ();
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

	if ((prop = node.property (X_("mono")))) {
		set_mono (string_is_affirmative (prop->value()));
	}

	return 0;
}

void
StreamPanner::add_state (XMLNode& node)
{
	node.add_property (X_("muted"), (muted() ? "yes" : "no"));
	node.add_property (X_("mono"), (_mono ? "yes" : "no"));
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
	, left (0.5)
	, right (0.5)
	, left_interp (left)
	, right_interp (right)
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

	/* x == 0 => hard left = 180.0 degrees
	   x == 1 => hard right = 0.0 degrees
	*/

	double _x = BaseStereoPanner::azimuth_to_lr_fract (_angles.azi);

	float const panR = _x;
	float const panL = 1 - panR;

	float const pan_law_attenuation = -3.0f;
	float const scale = 2.0f - 4.0f * powf (10.0f,pan_law_attenuation/20.0f);

	desired_left = panL * (scale * panL + 1.0f - scale);
	desired_right = panR * (scale * panR + 1.0f - scale);

	_effective_angles = _angles;
	//_control->set_value(x);
}

void
EqualPowerStereoPanner::do_distribute_automated (AudioBuffer& srcbuf, BufferSet& obufs,
						 nframes_t start, nframes_t end, nframes_t nframes,
						 pan_t** buffers)
{
	assert (obufs.count().n_audio() == 2);

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
		_effective_angles.azi = BaseStereoPanner::lr_fract_to_azimuth (buffers[0][nframes-1]);
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

		float const panR = buffers[0][n];
		float const panL = 1 - panR;

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
EqualPowerStereoPanner::factory (Panner& parent, Evoral::Parameter param, Speakers& /* ignored */)
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

	snprintf (buf, sizeof (buf), "%.12g", _angles.azi);
	root->add_property (X_("azimuth"), buf);
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
	LocaleGuard lg (X_("POSIX"));

	if ((prop = node.property (X_("azimuth")))) {
		AngularVector a (atof (prop->value().c_str()), 0.0);
		set_position (a, true);
	} else if ((prop = node.property (X_("x")))) {
		/* old school cartesian positioning */
		AngularVector a;
		a.azi = BaseStereoPanner::lr_fract_to_azimuth (atof (prop->value().c_str()));
		set_position (a, true);
	}

	StreamPanner::set_state (node, version);

	for (XMLNodeConstIterator iter = node.children().begin(); iter != node.children().end(); ++iter) {

		if ((*iter)->name() == Controllable::xml_node_name) {
			if ((prop = (*iter)->property("name")) != 0 && prop->value() == "panner") {
				_control->set_state (**iter, version);
			}

		} else if ((*iter)->name() == X_("Automation")) {

			_control->alist()->set_state (*((*iter)->children().front()), version);

			if (_control->alist()->automation_state() != Off) {
				double degrees = BaseStereoPanner::lr_fract_to_azimuth (_control->list()->eval (parent.session().transport_frame()));
				set_position (AngularVector (degrees, 0.0));
			}
		}
	}

	return 0;
}

Panner::Panner (string name, Session& s)
	: SessionObject (s, name)
	, Automatable (s)
{
	//set_name_old_auto (name);
	set_name (name);

	_linked = false;
	_link_direction = SameDirection;
	_bypassed = false;
	_mono = false;
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
		AngularVector a;
		switch (_streampanners.size()) {
		case 1:
			a.azi = 90.0; /* "front" or "top", in degrees */
			_streampanners.front()->set_position (a);
			_streampanners.front()->pan_control()->list()->reset_default (0.5);
			return;
			break;
		case 2:
			a.azi = 180.0; /* "left", in degrees */
			_streampanners.front()->set_position (a);
			_streampanners.front()->pan_control()->list()->reset_default (0.0);
			a.azi = 0.0; /* "right", in degrees */
			_streampanners.back()->set_position (a);
			_streampanners.back()->pan_control()->list()->reset_default (1.0);
			return;
		default:
			break;
		}
	}

	vector<Output>::iterator o;
	vector<StreamPanner*>::iterator p;

	for (o = outputs.begin(), p = _streampanners.begin(); o != outputs.end() && p != _streampanners.end(); ++o, ++p) {
		(*p)->set_position ((*o).position);
	}
}

void
Panner::reset_streampanner (uint32_t which)
{
	AngularVector a;

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
			a.azi = 90.0; /* "front" or "top", in degrees */
			_streampanners.front()->set_position (a);
			_streampanners.front()->pan_control()->list()->reset_default (0.5);
			break;
		case 2:
			/* stereo out, 2 streams, default = hard left/right */
			if (which == 0) {
				a.azi = 180.0; /* "left", in degrees */
				_streampanners.front()->set_position (a);
				_streampanners.front()->pan_control()->list()->reset_default (0.0);
			} else {
				a.azi = 0.0; /* "right", in degrees */
				_streampanners.back()->set_position (a);
				_streampanners.back()->pan_control()->list()->reset_default (1.0);
			}
			break;
		}
		return;

	default:
		_streampanners[which]->set_position (outputs[which].position);
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
		if (changed) {
			Changed (); /* EMIT SIGNAL */
		}
		return;
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
		outputs.push_back (Output (AngularVector (180.0, 0.0)));
		outputs.push_back (Output (AngularVector (0.0, 0,0)));
		for (n = 0; n < npans; ++n) {
			_streampanners.push_back (new EqualPowerStereoPanner (*this, Evoral::Parameter(PanAutomation, 0, n)));
		}
		break;

	default:
		setup_speakers (nouts);
		for (n = 0; n < npans; ++n) {
			_streampanners.push_back (new VBAPanner (*this, Evoral::Parameter(PanAutomation, 0, n), _session.get_speakers()));
		}
		break;
	}

	for (std::vector<StreamPanner*>::iterator x = _streampanners.begin(); x != _streampanners.end(); ++x) {
		(*x)->update ();
	}

	/* must emit Changed here, otherwise the changes to the pan_control below raise further
	   signals which the GUI is not prepared for until it has seen the Changed here.
	*/
	
	if (changed) {
		Changed (); /* EMIT SIGNAL */
	}

	/* force hard left/right panning in a common case: 2in/2out
	*/

	if (npans == 2 && outputs.size() == 2) {

		/* Do this only if we changed configuration, or our configuration
		   appears to be the default set up (zero degrees)
		*/

		AngularVector left;
		AngularVector right;

		left = _streampanners.front()->get_position ();
		right = _streampanners.back()->get_position ();

		if (changed || ((left.azi == 0.0) && (right.azi == 0.0))) {

			_streampanners.front()->set_position (AngularVector (180.0, 0.0));
			_streampanners.front()->pan_control()->list()->reset_default (0.0);

			_streampanners.back()->set_position (AngularVector (0.0, 0.0));
			_streampanners.back()->pan_control()->list()->reset_default (1.0);
		}

	} else if (npans > 1 && outputs.size() > 2) {

		/* 2d panning: spread signals equally around a circle */

		double degree_step = 360.0 / nouts;
		double deg;

		/* even number of signals? make sure the top two are either side of "top".
		   otherwise, just start at the "top" (90.0 degrees) and rotate around
		*/

		if (npans % 2) {
			deg = 90.0 - degree_step;
		} else {
			deg = 90.0;
		}

		for (std::vector<StreamPanner*>::iterator x = _streampanners.begin(); x != _streampanners.end(); ++x) {
			(*x)->set_position (AngularVector (deg, 0.0));
			deg += degree_step;
		}
	}
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
    StreamPanner* (*factory)(Panner&, Evoral::Parameter, Speakers&);
};

PanPlugins pan_plugins[] = {
	{ EqualPowerStereoPanner::name, 2, EqualPowerStereoPanner::factory },
	{ VBAPanner::name, 3, VBAPanner::factory },
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
		snprintf (buf, sizeof (buf), "%.12g", (*o).position.azi);
		onode->add_property (X_("azimuth"), buf);
		snprintf (buf, sizeof (buf), "%.12g", (*o).position.ele);
		onode->add_property (X_("elevation"), buf);
		node->add_child_nocopy (*onode);
	}

	for (vector<StreamPanner*>::const_iterator i = _streampanners.begin(); i != _streampanners.end(); ++i) {
		node->add_child_nocopy ((*i)->state (full));
	}

	node->add_child_nocopy (get_automation_xml_state ());

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

			AngularVector a;

			if ((prop = (*niter)->property (X_("azimuth")))) {
				sscanf (prop->value().c_str(), "%lg", &a.azi);
			} else if ((prop = (*niter)->property (X_("x")))) {
				/* old school cartesian */
				a.azi = BaseStereoPanner::lr_fract_to_azimuth (atof (prop->value().c_str()));
			}

			if ((prop = (*niter)->property (X_("elevation")))) {
				sscanf (prop->value().c_str(), "%lg", &a.ele);
			}

			outputs.push_back (Output (a));
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

						sp = pan_plugins[i].factory (*this, Evoral::Parameter(PanAutomation, 0, num_panners), _session.get_speakers ());
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

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == X_("Automation")) {
			set_automation_xml_state (**niter, Evoral::Parameter (PanAutomation));
		}
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
Panner::set_position (const AngularVector& a, StreamPanner& orig)
{
	AngularVector delta;
	AngularVector new_position;

	delta = orig.get_position() - a;

	if (_link_direction == SameDirection) {

		for (vector<StreamPanner*>::iterator i = _streampanners.begin(); i != _streampanners.end(); ++i) {
			if (*i == &orig) {
				(*i)->set_position (a, true);
			} else {
				new_position = (*i)->get_position() + delta;
				(*i)->set_position (new_position, true);
			}
		}

	} else {

		for (vector<StreamPanner*>::iterator i = _streampanners.begin(); i != _streampanners.end(); ++i) {
			if (*i == &orig) {
				(*i)->set_position (a, true);
			} else {
				new_position = (*i)->get_position() - delta;
				(*i)->set_position (new_position, true);
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
Panner::run (BufferSet& inbufs, BufferSet& outbufs, framepos_t start_frame, framepos_t end_frame, nframes_t nframes)
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

		if (fabsf(_session.transport_speed()) > 1.5f && Config->get_quieten_at_speed ()) {
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

string
Panner::value_as_string (double v)
{
	if (Panner::equivalent (v, 0.5)) {
		return _("C");
	} else if (equivalent (v, 0)) {
		return _("L");
	} else if (equivalent (v, 1)) {
		return _("R");
	} else if (v < 0.5) {
		stringstream s;
		s << fixed << setprecision (0) << _("L") << ((0.5 - v) * 200) << "%";
		return s.str();
	} else {
		stringstream s;
		s << fixed << setprecision (0) << _("R") << ((v -0.5) * 200) << "%";
		return s.str ();
	}

	return "";
}

void
Panner::setup_speakers (uint32_t nouts)
{
	switch (nouts) {
	case 3:
		/* top, bottom kind-of-left & bottom kind-of-right */
		outputs.push_back (AngularVector (90.0, 0.0));
		outputs.push_back (AngularVector (215.0, 0,0));
		outputs.push_back (AngularVector (335.0, 0,0));
		break;
	case 4:
		/* clockwise from top left */
		outputs.push_back (AngularVector (135.0, 0.0));
		outputs.push_back (AngularVector (45.0, 0.0));
		outputs.push_back (AngularVector (335.0, 0.0));
		outputs.push_back (AngularVector (215.0, 0.0));
		break;

	default: 
	{
		double degree_step = 360.0 / nouts;
		double deg;
		uint32_t n;

		/* even number of speakers? make sure the top two are either side of "top".
		   otherwise, just start at the "top" (90.0 degrees) and rotate around
		*/

		if (nouts % 2) {
			deg = 90.0 - degree_step;
		} else {
			deg = 90.0;
		}
		for (n = 0; n < nouts; ++n, deg += degree_step) {
			outputs.push_back (Output (AngularVector (deg, 0.0)));
		}
	}
	}

	Speakers& speakers (_session.get_speakers());
                        
	speakers.clear_speakers ();

	for (vector<Output>::iterator o = outputs.begin(); o != outputs.end(); ++o) {
		speakers.add_speaker ((*o).position);
	}
}
