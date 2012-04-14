/*
    Copyright (C) 2003-2006 Paul Davis

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

#include "ardour/debug.h"
#include "ardour/types.h"
#include "ardour/crossfade.h"
#include "ardour/audioregion.h"
#include "ardour/playlist.h"
#include "ardour/utils.h"
#include "ardour/session.h"
#include "ardour/source.h"
#include "ardour/region_factory.h"

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

framecnt_t Crossfade::_short_xfade_length = 0;

/* XXX if and when we ever implement parallel processing of the process()
   callback, these will need to be handled on a per-thread basis.
*/

Sample* Crossfade::crossfade_buffer_out = 0;
Sample* Crossfade::crossfade_buffer_in = 0;


#define CROSSFADE_DEFAULT_PROPERTIES \
	_active (Properties::active, _session.config.get_xfades_active ()) \
	, _follow_overlap (Properties::follow_overlap, false)


namespace ARDOUR {
	namespace Properties {
		PropertyDescriptor<bool> follow_overlap;
	}
}

void
Crossfade::make_property_quarks ()
{
	Properties::follow_overlap.property_id = g_quark_from_static_string (X_("follow-overlap"));
        DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for follow-overlap = %1\n", 	Properties::follow_overlap.property_id));
}

void
Crossfade::set_buffer_size (framecnt_t sz)
{
	delete [] crossfade_buffer_out;
	crossfade_buffer_out = 0;

	delete [] crossfade_buffer_in;
	crossfade_buffer_in = 0;

	if (sz) {
		crossfade_buffer_out = new Sample[sz];
		crossfade_buffer_in = new Sample[sz];
	}
}

bool
Crossfade::operator== (const Crossfade& other)
{
	return (_in == other._in) && (_out == other._out);
}

Crossfade::Crossfade (boost::shared_ptr<AudioRegion> in, boost::shared_ptr<AudioRegion> out,
		      framecnt_t length,
		      AnchorPoint ap)
	: AudioRegion (in->session(), 0, length, in->name() + string ("<>") + out->name())
	, CROSSFADE_DEFAULT_PROPERTIES
	, _fade_in (Evoral::Parameter(FadeInAutomation)) // linear (gain coefficient) => -inf..+6dB
	, _fade_out (Evoral::Parameter(FadeOutAutomation)) // linear (gain coefficient) => -inf..+6dB

{
	register_properties ();

	_in = in;
	_out = out;
	_anchor_point = ap;
	_fixed = true;
        _follow_overlap = false;

	initialize ();
}

Crossfade::Crossfade (boost::shared_ptr<AudioRegion> a, boost::shared_ptr<AudioRegion> b, CrossfadeModel model, bool act)
	: AudioRegion (a->session(), 0, 0, a->name() + string ("<>") + b->name())
	, CROSSFADE_DEFAULT_PROPERTIES
	, _fade_in (Evoral::Parameter(FadeInAutomation)) // linear (gain coefficient) => -inf..+6dB
	, _fade_out (Evoral::Parameter(FadeOutAutomation)) // linear (gain coefficient) => -inf..+6dB
{
	register_properties ();
	
	_in_update = false;
	_fixed = false;
	_follow_overlap = false;

	if (compute (a, b, model)) {
		throw failed_constructor();
	}

	_active = act;

	initialize ();
}

Crossfade::Crossfade (const Playlist& playlist, XMLNode const & node)
	: AudioRegion (playlist.session(), 0, 0, "unnamed crossfade")
	, CROSSFADE_DEFAULT_PROPERTIES
	, _fade_in (Evoral::Parameter(FadeInAutomation)) // linear (gain coefficient) => -inf..+6dB
	, _fade_out (Evoral::Parameter(FadeOutAutomation)) // linear (gain coefficient) => -inf..+6dB

{
	register_properties ();
	
	boost::shared_ptr<Region> r;
	XMLProperty const * prop;
	LocaleGuard lg (X_("POSIX"));

	/* we have to find the in/out regions before we can do anything else */

	if ((prop = node.property ("in")) == 0) {
		error << _("Crossfade: no \"in\" region in state") << endmsg;
		throw failed_constructor();
	}

	PBD::ID id (prop->value());

	r = playlist.find_region (id);

	if (!r) {
		/* the `in' region is not in a playlist, which probably means that this crossfade
		   is in the undo record, so we have to find the region in the global region map.
		*/
		r = RegionFactory::region_by_id (id);
	}

	if (!r) {
		error << string_compose (_("Crossfade: no \"in\" region %1 found in playlist %2 nor in region map"), id, playlist.name())
		      << endmsg;
		throw failed_constructor();
	}

	if ((_in = boost::dynamic_pointer_cast<AudioRegion> (r)) == 0) {
		throw failed_constructor();
	}

	if ((prop = node.property ("out")) == 0) {
		error << _("Crossfade: no \"out\" region in state") << endmsg;
		throw failed_constructor();
	}

	PBD::ID id2 (prop->value());

	r = playlist.find_region (id2);

	if (!r) {
		r = RegionFactory::region_by_id (id2);
	}

	if (!r) {
		error << string_compose (_("Crossfade: no \"out\" region %1 found in playlist %2 nor in region map"), id2, playlist.name())
		      << endmsg;
		throw failed_constructor();
	}

	if ((_out = boost::dynamic_pointer_cast<AudioRegion> (r)) == 0) {
		throw failed_constructor();
	}

	_length = 0;
	initialize();
	_active = true;

	if (set_state (node, Stateful::loading_state_version)) {
		throw failed_constructor();
	}
}

Crossfade::Crossfade (boost::shared_ptr<Crossfade> orig, boost::shared_ptr<AudioRegion> newin, boost::shared_ptr<AudioRegion> newout)
	: AudioRegion (boost::dynamic_pointer_cast<const AudioRegion> (orig), 0)
	, CROSSFADE_DEFAULT_PROPERTIES
	, _fade_in (orig->_fade_in)
	, _fade_out (orig->_fade_out)
{
	register_properties ();
	
	_active           = orig->_active;
	_in_update        = orig->_in_update;
	_anchor_point     = orig->_anchor_point;
	_follow_overlap   = orig->_follow_overlap;
	_fixed            = orig->_fixed;
	_position         = orig->_position;

	_in = newin;
	_out = newout;

	// copied from Crossfade::initialize()
	_in_update = false;

	_out->suspend_fade_out ();
	_in->suspend_fade_in ();

	overlap_type = _in->coverage (_out->position(), _out->last_frame());
	layer_relation = (int32_t) (_in->layer() - _out->layer());

	// Let's make sure the fade isn't too long
	set_xfade_length(_length);
}


Crossfade::~Crossfade ()
{
}

void
Crossfade::register_properties ()
{
	add_property (_active);
	add_property (_follow_overlap);
}

void
Crossfade::initialize ()
{
	/* merge source lists from regions */

	_sources = _in->sources();
	_sources.insert (_sources.end(), _out->sources().begin(), _out->sources().end());

        for (SourceList::iterator i = _sources.begin(); i != _sources.end(); ++i) {
                (*i)->inc_use_count ();
        }

	_master_sources = _in->master_sources();
	_master_sources.insert(_master_sources.end(), _out->master_sources().begin(), _out->master_sources().end());

        for (SourceList::iterator i = _master_sources.begin(); i != _master_sources.end(); ++i) {
                (*i)->inc_use_count ();
        }

	_in_update = false;

	_out->suspend_fade_out ();
	_in->suspend_fade_in ();

	_fade_out.freeze ();
	_fade_out.clear ();

#define EQUAL_POWER_MINUS_3DB
#ifdef  EQUAL_POWER_MINUS_3DB

	_fade_out.add ((_length * 0.000000), 1.000000);
	_fade_out.add ((_length * 0.166667), 0.948859);
	_fade_out.add ((_length * 0.333333), 0.851507);
	_fade_out.add ((_length * 0.500000), 0.707946);
	_fade_out.add ((_length * 0.666667), 0.518174);
	_fade_out.add ((_length * 0.833333), 0.282192);
	_fade_out.add ((_length * 1.000000), 0.000000);

#else // EQUAL_POWER_MINUS_6DB

	_fade_out.add ((_length * 0.000000), 1.000000);
	_fade_out.add ((_length * 0.166667), 0.833033);
	_fade_out.add ((_length * 0.333333), 0.666186);
	_fade_out.add ((_length * 0.500000), 0.499459);
	_fade_out.add ((_length * 0.666667), 0.332853);
	_fade_out.add ((_length * 0.833333), 0.166366);
	_fade_out.add ((_length * 1.000000), 0.000000);
#endif

	_fade_out.thaw ();

	_fade_in.freeze ();
	_fade_in.clear ();

#define EQUAL_POWER_MINUS_3DB
#ifdef  EQUAL_POWER_MINUS_3DB

	_fade_in.add ((_length * 0.000000), 0.000000);
	_fade_in.add ((_length * 0.166667), 0.282192);
	_fade_in.add ((_length * 0.333333), 0.518174);
	_fade_in.add ((_length * 0.500000), 0.707946);
	_fade_in.add ((_length * 0.666667), 0.851507);
	_fade_in.add ((_length * 0.833333), 0.948859);
	_fade_in.add ((_length * 1.000000), 1.000000);

#else // EQUAL_POWER_MINUS_SIX_DB

	_fade_in.add ((_length * 0.000000), 0.000000);
	_fade_in.add ((_length * 0.166667), 0.166366);
	_fade_in.add ((_length * 0.333333), 0.332853);
	_fade_in.add ((_length * 0.500000), 0.499459);
	_fade_in.add ((_length * 0.666667), 0.666186);
	_fade_in.add ((_length * 0.833333), 0.833033);
	_fade_in.add ((_length * 1.000000), 1.000000);

#endif

	_fade_in.thaw ();

	overlap_type = _in->coverage (_out->position(), _out->last_frame());
	layer_relation = (int32_t) (_in->layer() - _out->layer());
}

framecnt_t
Crossfade::read_raw_internal (Sample* buf, framepos_t start, framecnt_t cnt, int channel) const
{
	Sample* mixdown = new Sample[cnt];
	float* gain = new float[cnt];
	framecnt_t ret;

	ret = read_at (buf, mixdown, gain, start, cnt, channel);

	delete [] mixdown;
	delete [] gain;

	return ret;
}

framecnt_t
Crossfade::read_at (Sample *buf, Sample *mixdown_buffer,
		    float *gain_buffer, framepos_t start, framecnt_t cnt, uint32_t chan_n) const
{
	frameoffset_t offset;
	framecnt_t to_write;

	if (!_active) {
		return 0;
	}

	if (start < _position) {

		/* handle an initial section of the read area that we do not
		   cover.
		*/

		offset = _position - start;

		if (offset < cnt) {
			cnt -= offset;
		} else {
			return 0;
		}

		start = _position;
		buf += offset;
		to_write = min (_length.val(), cnt);

	} else {

		to_write = min ((_length - (start - _position)), cnt);

	}

	offset = start - _position;

	/* Prevent data from piling up inthe crossfade buffers when reading a transparent region */
	if (!(_out->opaque())) {
		memset (crossfade_buffer_out, 0, sizeof (Sample) * to_write);
	} else if (!(_in->opaque())) {
		memset (crossfade_buffer_in, 0, sizeof (Sample) * to_write);
	}

	_out->read_at (crossfade_buffer_out, mixdown_buffer, gain_buffer, start, to_write, chan_n);
	_in->read_at (crossfade_buffer_in, mixdown_buffer, gain_buffer, start, to_write, chan_n);

	float* fiv = new float[to_write];
	float* fov = new float[to_write];

	_fade_in.curve().get_vector (offset, offset+to_write, fiv, to_write);
	_fade_out.curve().get_vector (offset, offset+to_write, fov, to_write);

	/* note: although we have not explicitly taken into account the return values
	   from _out->read_at() or _in->read_at(), the length() function does this
	   implicitly. why? because it computes a value based on the in+out regions'
	   position and length, and so we know precisely how much data they could return.
	*/

	for (framecnt_t n = 0; n < to_write; ++n) {
		buf[n] = (crossfade_buffer_out[n] * fov[n]) + (crossfade_buffer_in[n] * fiv[n]);
	}

	delete [] fov;
	delete [] fiv;

	return to_write;
}

OverlapType
Crossfade::coverage (framepos_t start, framepos_t end) const
{
	framepos_t my_end = _position + _length;

	if ((start >= _position) && (end <= my_end)) {
		return OverlapInternal;
	}
	if ((end >= _position) && (end <= my_end)) {
		return OverlapStart;
	}
	if ((start >= _position) && (start <= my_end)) {
		return OverlapEnd;
	}
	if ((_position >= start) && (_position <= end) && (my_end <= end)) {
		return OverlapExternal;
	}
	return OverlapNone;
}

void
Crossfade::set_active (bool yn)
{
	if (_active != yn) {
		_active = yn;
		PropertyChanged (PropertyChange (Properties::active));
	}
}

bool
Crossfade::refresh ()
{
	/* crossfades must be between non-muted regions */

	if (_out->muted() || _in->muted()) {
		Invalidated (shared_from_this ());
		return false;
	}

	/* Top layer shouldn't be transparent */

	if (!((layer_relation > 0 ? _in : _out)->opaque())) {
		Invalidated (shared_from_this());
		return false;
	}

        /* regions must cannot be identically sized and placed */

        if (_in->position() == _out->position() && _in->length() == _out->length()) {
		Invalidated (shared_from_this());
                return false;
        }

	/* layer ordering cannot change */

	int32_t new_layer_relation = (int32_t) (_in->layer() - _out->layer());

	if (new_layer_relation * layer_relation < 0) { // different sign, layers rotated
		Invalidated (shared_from_this ());
		return false;
	}

	OverlapType ot = _in->coverage (_out->first_frame(), _out->last_frame());

	if (ot == OverlapNone) {
		Invalidated (shared_from_this ());
		return false;
	}

	bool send_signal;

	if (ot != overlap_type) {

		if (_follow_overlap) {

			try {
				compute (_in, _out, _session.config.get_xfade_model());
			}

			catch (NoCrossfadeHere& err) {
				Invalidated (shared_from_this ());
				return false;
			}

			send_signal = true;

		} else {
			Invalidated (shared_from_this ());
			return false;
		}

	} else {

		send_signal = update ();
	}

	if (send_signal) {
		PropertyChange bounds;
		bounds.add (Properties::start);
		bounds.add (Properties::position);
		bounds.add (Properties::length);
		PropertyChanged (bounds); /* EMIT SIGNAL */
	}

	_in_update = false;

	return true;
}

bool
Crossfade::update ()
{
	framecnt_t newlen;

	if (_follow_overlap) {
		newlen = _out->first_frame() + _out->length() - _in->first_frame();
	} else {
		newlen = _length;
	}

	if (newlen == 0) {
		Invalidated (shared_from_this ());
		return false;
	}

	_in_update = true;

	if ((_follow_overlap && newlen != _length) || (_length > newlen)) {

		double factor =  newlen / (double) _length;

		_fade_out.x_scale (factor);
		_fade_in.x_scale (factor);

		_length = newlen;
	}

	switch (_anchor_point) {
	case StartOfIn:
		_position = _in->first_frame();
		break;

	case EndOfIn:
		_position = _in->last_frame() - _length;
		break;

	case EndOfOut:
		_position = _out->last_frame() - _length;
	}

	return true;
}

int
Crossfade::compute (boost::shared_ptr<AudioRegion> a, boost::shared_ptr<AudioRegion> b, CrossfadeModel model)
{
	boost::shared_ptr<AudioRegion> top;
	boost::shared_ptr<AudioRegion> bottom;
	framecnt_t short_xfade_length;

	short_xfade_length = _short_xfade_length;

	if (a->layer() < b->layer()) {
		top = b;
		bottom = a;
	} else {
		top = a;
		bottom = b;
	}

	/* first check for matching ends */

	if (top->first_frame() == bottom->first_frame()) {

		/* Both regions start at the same point */

		if (top->last_frame() < bottom->last_frame()) {

			/* top ends before bottom, so put an xfade
			   in at the end of top.
			*/

			/* [-------- top ---------- ]
                         * {====== bottom =====================}
			 */

			_in = bottom;
			_out = top;

			if (top->last_frame() < short_xfade_length) {
				_position = 0;
			} else {
				_position = top->last_frame() - short_xfade_length;
			}
			
			set_xfade_length (min (short_xfade_length, top->length()));
			_follow_overlap = false;
			_anchor_point = EndOfIn;
			_active = true;
			_fixed = true;

		} else {
			/* top ends after (or same time) as bottom - no xfade
			 */

			/* [-------- top ------------------------ ]
                         * {====== bottom =====================}
			 */

			throw NoCrossfadeHere();
		}

	} else if (top->last_frame() == bottom->last_frame()) {

		/* Both regions end at the same point */

		if (top->first_frame() > bottom->first_frame()) {

			/* top starts after bottom, put an xfade in at the
			   start of top
			*/

			/*            [-------- top ---------- ]
                         * {====== bottom =====================}
			 */

			_in = top;
			_out = bottom;
			_position = top->first_frame();
			set_xfade_length (min (short_xfade_length, top->length()));
			_follow_overlap = false;
			_anchor_point = StartOfIn;
			_active = true;
			_fixed = true;

		} else {
			/* top starts before bottom - no xfade
			 */

			/* [-------- top ------------------------ ]
                         *    {====== bottom =====================}
			 */

			throw NoCrossfadeHere();
		}

	} else {

		/* OK, time to do more regular overlapping */

		OverlapType ot = top->coverage (bottom->first_frame(), bottom->last_frame());

		switch (ot) {
		case OverlapNone:
			/* should be NOTREACHED as a precondition of creating
			   a new crossfade, but we need to handle it here.
			*/
			throw NoCrossfadeHere();
			break;

		case OverlapInternal:
		case OverlapExternal:
			/* should be NOTREACHED because of tests above */
			throw NoCrossfadeHere();
			break;

		case OverlapEnd: /* top covers start of bottom but ends within it */

			/* [---- top ------------------------]
			 *                { ==== bottom ============ }
			 */

			_in = bottom;
			_out = top;
			_anchor_point = EndOfOut;

			if (model == FullCrossfade) {
				_position = bottom->first_frame(); // "{"
				set_xfade_length (_out->first_frame() + _out->length() - _in->first_frame());
				/* leave active alone */
				_follow_overlap = true;
			} else {
				set_xfade_length (min (short_xfade_length, top->length()));
				_position = top->last_frame() - _length;  // "]" - length
				_active = true;
				_follow_overlap = false;

			}
			break;

		case OverlapStart:   /* top starts within bottom but covers bottom's end */

			/*                   { ==== top ============ }
			 *   [---- bottom -------------------]
			 */

			_in = top;
			_out = bottom;
			_position = top->first_frame();
			_anchor_point = StartOfIn;

			if (model == FullCrossfade) {
				set_xfade_length (_out->first_frame() + _out->length() - _in->first_frame());
				/* leave active alone */
				_follow_overlap = true;
			} else {
				set_xfade_length (min (short_xfade_length, top->length()));
				_active = true;
				_follow_overlap = false;

			}

			break;
		}
	}

	return 0;
}

XMLNode&
Crossfade::get_state ()
{
	XMLNode* node = new XMLNode (X_("Crossfade"));
	XMLNode* child;
	char buf[64];
	LocaleGuard lg (X_("POSIX"));

	id().print (buf, sizeof (buf));
	node->add_property ("id", buf);
	_out->id().print (buf, sizeof (buf));
	node->add_property ("out", buf);
	_in->id().print (buf, sizeof (buf));
	node->add_property ("in", buf);
	node->add_property ("active", (_active ? "yes" : "no"));
	node->add_property ("follow-overlap", (_follow_overlap ? "yes" : "no"));
	node->add_property ("fixed", (_fixed ? "yes" : "no"));
	snprintf (buf, sizeof(buf), "%" PRId64, _length.val());
	node->add_property ("length", buf);
	snprintf (buf, sizeof(buf), "%" PRIu32, (uint32_t) _anchor_point);
	node->add_property ("anchor-point", buf);
	snprintf (buf, sizeof(buf), "%" PRId64, _position.val());
	node->add_property ("position", buf);

	child = node->add_child ("FadeIn");

	for (AutomationList::iterator ii = _fade_in.begin(); ii != _fade_in.end(); ++ii) {
		XMLNode* pnode;

		pnode = new XMLNode ("point");

		snprintf (buf, sizeof (buf), "%" PRId64, (framepos_t) floor ((*ii)->when));
		pnode->add_property ("x", buf);
		snprintf (buf, sizeof (buf), "%.12g", (*ii)->value);
		pnode->add_property ("y", buf);
		child->add_child_nocopy (*pnode);
	}

	child = node->add_child ("FadeOut");

	for (AutomationList::iterator ii = _fade_out.begin(); ii != _fade_out.end(); ++ii) {
		XMLNode* pnode;

		pnode = new XMLNode ("point");

		snprintf (buf, sizeof (buf), "%" PRId64, (framepos_t) floor ((*ii)->when));
		pnode->add_property ("x", buf);
		snprintf (buf, sizeof (buf), "%.12g", (*ii)->value);
		pnode->add_property ("y", buf);
		child->add_child_nocopy (*pnode);
	}

	return *node;
}

int
Crossfade::set_state (const XMLNode& node, int /*version*/)
{
	XMLNodeConstIterator i;
	XMLNodeList children;
	XMLNode* fi;
	XMLNode* fo;
	const XMLProperty* prop;
	LocaleGuard lg (X_("POSIX"));
	PropertyChange what_changed;
	framepos_t val;

	set_id (node);

	if ((prop = node.property ("position")) != 0) {
		sscanf (prop->value().c_str(), "%" PRId64, &val);
		if (val != _position) {
			_position = val;
			what_changed.add (Properties::position);
		}
	} else {
		warning << _("old-style crossfade information - no position information") << endmsg;
		_position = _in->first_frame();
	}

	if ((prop = node.property ("active")) != 0) {
		bool x = string_is_affirmative (prop->value());
		if (x != _active) {
			_active = x;
			what_changed.add (Properties::active);
		}
	} else {
		_active = true;
	}

	if ((prop = node.property ("follow-overlap")) != 0) {
		_follow_overlap = string_is_affirmative (prop->value());
	} else {
		_follow_overlap = false;
	}

	if ((prop = node.property ("fixed")) != 0) {
		_fixed = string_is_affirmative (prop->value());
	} else {
		_fixed = false;
	}

	if ((prop = node.property ("anchor-point")) != 0) {
		_anchor_point = AnchorPoint (atoi ((prop->value().c_str())));
	} else {
		_anchor_point = StartOfIn;
	}

	if ((prop = node.property ("length")) != 0) {

		sscanf (prop->value().c_str(), "%" PRId64, &val);
		if (val != _length) {
			_length = val;
			what_changed.add (Properties::length);
		}

	} else {

		/* XXX this branch is legacy code from before
		   the point where we stored xfade lengths.
		*/

		if ((_length = overlap_length()) == 0) {
			throw failed_constructor();
		}
	}

	if ((fi = find_named_node (node, "FadeIn")) == 0) {
		return -1;
	}

	if ((fo = find_named_node (node, "FadeOut")) == 0) {
		return -1;
	}

	/* fade in */

	_fade_in.freeze ();
	_fade_in.clear ();

	children = fi->children();

	for (i = children.begin(); i != children.end(); ++i) {
		if ((*i)->name() == "point") {
			framepos_t x;
			float y;

			prop = (*i)->property ("x");
			sscanf (prop->value().c_str(), "%" PRId64, &x);

			prop = (*i)->property ("y");
			sscanf (prop->value().c_str(), "%f", &y);

			_fade_in.add (x, y);
		}
	}

        if (_fade_in.size() < 2) {
                /* fade state somehow saved with no points */
                return -1;
        }

        _fade_in.front()->value = 0.0;
        _fade_in.back()->value = 1.0;

	_fade_in.thaw ();

        /* fade out */

	_fade_out.freeze ();
	_fade_out.clear ();

	children = fo->children();

	for (i = children.begin(); i != children.end(); ++i) {
		if ((*i)->name() == "point") {
			framepos_t x;
			float y;
			XMLProperty* prop;

			prop = (*i)->property ("x");
			sscanf (prop->value().c_str(), "%" PRId64, &x);

			prop = (*i)->property ("y");
			sscanf (prop->value().c_str(), "%f", &y);

			_fade_out.add (x, y);
		}
	}

        if (_fade_out.size() < 2) {
                /* fade state somehow saved with no points */
                return -1;
        }

        _fade_out.front()->value = 1.0;
        _fade_out.back()->value = 0.0;

	_fade_out.thaw ();

	PropertyChanged (what_changed); /* EMIT SIGNAL */
	FadesChanged (); /* EMIT SIGNAL */

	return 0;
}

bool
Crossfade::can_follow_overlap () const
{
	return !_fixed;
}

void
Crossfade::set_follow_overlap (bool yn)
{
	if (yn == _follow_overlap || _fixed) {
		return;
	}

	_follow_overlap = yn;

	if (!yn) {
		set_xfade_length (_short_xfade_length);
	} else {
		set_xfade_length (_out->first_frame() + _out->length() - _in->first_frame());
	}

	PropertyChanged (PropertyChange (Properties::follow_overlap));
}

framecnt_t
Crossfade::set_xfade_length (framecnt_t len)
{
	framecnt_t limit = 0;

	switch (_anchor_point) {
	case StartOfIn:
		limit = _in->length();
		break;

	case EndOfIn:
		limit = _in->length();
		break;

	case EndOfOut:
		limit = _out->length();
		break;

	}

	len = min (limit, len);

	double factor = len / (double) _length;

	_in_update = true;
	_fade_out.x_scale (factor);
	_fade_in.x_scale (factor);
	_in_update = false;

	_length = len;

	PropertyChanged (PropertyChange (Properties::length));

	return len;
}

framecnt_t
Crossfade::overlap_length () const
{
	if (_fixed) {
		return _length;
	}
	return _out->first_frame() + _out->length() - _in->first_frame();
}

void
Crossfade::set_short_xfade_length (framecnt_t n)
{
	_short_xfade_length = n;
}
