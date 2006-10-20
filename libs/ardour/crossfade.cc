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

    $Id$
*/

#include <sigc++/bind.h>

#include <ardour/types.h>
#include <ardour/crossfade.h>
#include <ardour/crossfade_compare.h>
#include <ardour/audioregion.h>
#include <ardour/playlist.h>
#include <ardour/utils.h>
#include <ardour/session.h>

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

nframes_t Crossfade::_short_xfade_length = 0;
Change Crossfade::ActiveChanged = new_change();
Change Crossfade::FollowOverlapChanged = new_change();

/* XXX if and when we ever implement parallel processing of the process()
   callback, these will need to be handled on a per-thread basis.
*/

Sample* Crossfade::crossfade_buffer_out = 0;
Sample* Crossfade::crossfade_buffer_in = 0;

void
Crossfade::set_buffer_size (nframes_t sz)
{
	if (crossfade_buffer_out) {
		delete [] crossfade_buffer_out;
		crossfade_buffer_out = 0;
	}

	if (crossfade_buffer_in) {
		delete [] crossfade_buffer_in;
		crossfade_buffer_in = 0;
	}

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
		      nframes_t length,
		      nframes_t position,
		      AnchorPoint ap)
	: _fade_in (0.0, 2.0, 1.0), // linear (gain coefficient) => -inf..+6dB
	  _fade_out (0.0, 2.0, 1.0) // linear (gain coefficient) => -inf..+6dB
{
	_in = in;
	_out = out;
	_length = length;
	_position = position;
	_anchor_point = ap;

	switch (Config->get_xfade_model()) {
	case ShortCrossfade:
		_follow_overlap = false;
		break;
	default:
		_follow_overlap = true;
	}

	_active = Config->get_xfades_active ();
	_fixed = true;
		
	initialize ();
}

Crossfade::Crossfade (boost::shared_ptr<AudioRegion> a, boost::shared_ptr<AudioRegion> b, CrossfadeModel model, bool act)
	: _fade_in (0.0, 2.0, 1.0), // linear (gain coefficient) => -inf..+6dB
	  _fade_out (0.0, 2.0, 1.0) // linear (gain coefficient) => -inf..+6dB
{
	_in_update = false;
	_fixed = false;

	if (compute (a, b, model)) {
		throw failed_constructor();
	}

	_active = act;

	initialize ();

}

Crossfade::Crossfade (const Playlist& playlist, XMLNode& node)
	:  _fade_in (0.0, 2.0, 1.0), // linear (gain coefficient) => -inf..+6dB
	   _fade_out (0.0, 2.0, 1.0) // linear (gain coefficient) => -inf..+6dB
{
	boost::shared_ptr<Region> r;
	XMLProperty* prop;
	LocaleGuard lg (X_("POSIX"));

	/* we have to find the in/out regions before we can do anything else */

	if ((prop = node.property ("in")) == 0) {
		error << _("Crossfade: no \"in\" region in state") << endmsg;
		throw failed_constructor();
	}
	
	PBD::ID id (prop->value());

	if ((r = playlist.find_region (id)) == 0) {
		error << string_compose (_("Crossfade: no \"in\" region %1 found in playlist %2"), id, playlist.name())
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

	if ((r = playlist.find_region (id2)) == 0) {
		error << string_compose (_("Crossfade: no \"out\" region %1 found in playlist %2"), id2, playlist.name())
		      << endmsg;
		throw failed_constructor();
	}
	
	if ((_out = boost::dynamic_pointer_cast<AudioRegion> (r)) == 0) {
		throw failed_constructor();
	}

	_length = 0;
	initialize();
	
	if (set_state (node)) {
		throw failed_constructor();
	}
}

Crossfade::Crossfade (const Crossfade &orig, boost::shared_ptr<AudioRegion> newin, boost::shared_ptr<AudioRegion> newout)
	: _fade_in(orig._fade_in),
	  _fade_out(orig._fade_out)
{
	_active           = orig._active;
	_in_update        = orig._in_update;
	_length           = orig._length;
	_position         = orig._position;
	_anchor_point     = orig._anchor_point;
	_follow_overlap   = orig._follow_overlap;
	_fixed            = orig._fixed;
	
	_in = newin;
	_out = newout;

	// copied from Crossfade::initialize()
	_in_update = false;
	
	_out->suspend_fade_out ();
	_in->suspend_fade_in ();

	overlap_type = _in->coverage (_out->position(), _out->last_frame());

	// Let's make sure the fade isn't too long
	set_length(_length);
}


Crossfade::~Crossfade ()
{
	Invalidated (this);
}

void
Crossfade::initialize ()
{
	_in_update = false;
	
	_out->suspend_fade_out ();
	_in->suspend_fade_in ();

	_fade_out.freeze ();
	_fade_out.clear ();
	_fade_out.add (0.0, 1.0);
	_fade_out.add ((_length * 0.1), 0.99);
	_fade_out.add ((_length * 0.2), 0.97);
	_fade_out.add ((_length * 0.8), 0.03);
	_fade_out.add ((_length * 0.9), 0.01);
	_fade_out.add (_length, 0.0);
	_fade_out.thaw ();
	
	_fade_in.freeze ();
	_fade_in.clear ();
	_fade_in.add (0.0, 0.0);
	_fade_in.add ((_length * 0.1),  0.01);
	_fade_in.add ((_length * 0.2),  0.03);
	_fade_in.add ((_length * 0.8),  0.97);
	_fade_in.add ((_length * 0.9),  0.99);
	_fade_in.add (_length, 1.0);
	_fade_in.thaw ();

	_in->StateChanged.connect (sigc::mem_fun (*this, &Crossfade::member_changed));
	_out->StateChanged.connect (sigc::mem_fun (*this, &Crossfade::member_changed));

	overlap_type = _in->coverage (_out->position(), _out->last_frame());
}	

int
Crossfade::compute (boost::shared_ptr<AudioRegion> a, boost::shared_ptr<AudioRegion> b, CrossfadeModel model)
{
	boost::shared_ptr<AudioRegion> top;
	boost::shared_ptr<AudioRegion> bottom;
	nframes_t short_xfade_length;

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

			_length = min (short_xfade_length, top->length());
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
			_length = min (short_xfade_length, top->length());
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
			_position = bottom->first_frame();
			_anchor_point = StartOfIn;

			if (model == FullCrossfade) {
				_length = _out->first_frame() + _out->length() - _in->first_frame();
				/* leave active alone */
				_follow_overlap = true;
			} else {
				_length = min (short_xfade_length, top->length());
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
				_length = _out->first_frame() + _out->length() - _in->first_frame();
				/* leave active alone */
				_follow_overlap = true;
			} else {
				_length = min (short_xfade_length, top->length());
				_active = true;
				_follow_overlap = false;
				
			}
			
			break;
		}
	}
	
	return 0;
}

nframes_t 
Crossfade::read_at (Sample *buf, Sample *mixdown_buffer, 
		    float *gain_buffer, nframes_t start, nframes_t cnt, uint32_t chan_n,
		    nframes_t read_frames, nframes_t skip_frames)
{
	nframes_t offset;
	nframes_t to_write;

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
		to_write = min (_length, cnt);

	} else {
		
		to_write = min (_length - (start - _position), cnt);
		
	}

	offset = start - _position;

	_out->read_at (crossfade_buffer_out, mixdown_buffer, gain_buffer, start, to_write, chan_n, read_frames, skip_frames);
	_in->read_at (crossfade_buffer_in, mixdown_buffer, gain_buffer, start, to_write, chan_n, read_frames, skip_frames);

	float* fiv = new float[to_write];
	float* fov = new float[to_write];

	_fade_in.get_vector (offset, offset+to_write, fiv, to_write);
	_fade_out.get_vector (offset, offset+to_write, fov, to_write);

	/* note: although we have not explicitly taken into account the return values
	   from _out->read_at() or _in->read_at(), the length() function does this
	   implicitly. why? because it computes a value based on the in+out regions'
	   position and length, and so we know precisely how much data they could return. 
	*/

	for (nframes_t n = 0; n < to_write; ++n) {
		buf[n] = (crossfade_buffer_out[n] * fov[n]) + (crossfade_buffer_in[n] * fiv[n]);
	}

	delete [] fov;
	delete [] fiv;

	return to_write;
}	

OverlapType 
Crossfade::coverage (nframes_t start, nframes_t end) const
{
	nframes_t my_end = _position + _length;

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
		StateChanged (ActiveChanged);
	}
}

bool
Crossfade::refresh ()
{
	/* crossfades must be between non-muted regions */
	
	if (_out->muted() || _in->muted()) {
		Invalidated (this);
		return false;
	}

	/* overlap type must be Start, End or External */

	OverlapType ot;
	
	ot = _in->coverage (_out->first_frame(), _out->last_frame());
	
	switch (ot) {
	case OverlapNone:
	case OverlapInternal:
		Invalidated (this);
		return false;
		
	default:
		break;
	}
		
	/* overlap type must not have altered */
	
	if (ot != overlap_type) {
		Invalidated (this);
		return false;
	} 

	/* time to update */

	return update (false);
}

bool
Crossfade::update (bool force)
{
	nframes_t newlen;

	if (_follow_overlap) {
		newlen = _out->first_frame() + _out->length() - _in->first_frame();
	} else {
		newlen = _length;
	}

	if (newlen == 0) {
		Invalidated (this);
		return false;
	}

	_in_update = true;

	if (force || (_follow_overlap && newlen != _length) || (_length > newlen)) {

		double factor =  newlen / (double) _length;
		
		_fade_out.x_scale (factor);
		_fade_in.x_scale (factor);
		
		_length = newlen;

	} 

	switch (_anchor_point) {
	case StartOfIn:
		if (_position != _in->first_frame()) {
			_position = _in->first_frame();
		}
		break;

	case EndOfIn:
		if (_position != _in->last_frame() - _length) {
			_position = _in->last_frame() - _length;
		}
		break;

	case EndOfOut:
		if (_position != _out->last_frame() - _length) {
			_position = _out->last_frame() - _length;
		}
	}

	/* UI's may need to know that the overlap changed even 
	   though the xfade length did not.
	*/
	
	StateChanged (BoundsChanged); /* EMIT SIGNAL */

	_in_update = false;

	return true;
}

void
Crossfade::member_changed (Change what_changed)
{
	Change what_we_care_about = Change (Region::MuteChanged|
					    Region::LayerChanged|
					    BoundsChanged);

	if (what_changed & what_we_care_about) {
		refresh ();
	}
}

XMLNode&
Crossfade::get_state () 
{
	XMLNode* node = new XMLNode (X_("Crossfade"));
	XMLNode* child;
	char buf[64];
	LocaleGuard lg (X_("POSIX"));

	_out->id().print (buf, sizeof (buf));
	node->add_property ("out", buf);
	_in->id().print (buf, sizeof (buf));
	node->add_property ("in", buf);
	node->add_property ("active", (_active ? "yes" : "no"));
	node->add_property ("follow-overlap", (_follow_overlap ? "yes" : "no"));
	node->add_property ("fixed", (_fixed ? "yes" : "no"));
	snprintf (buf, sizeof(buf), "%" PRIu32, _length);
	node->add_property ("length", buf);
	snprintf (buf, sizeof(buf), "%" PRIu32, (uint32_t) _anchor_point);
	node->add_property ("anchor-point", buf);
	snprintf (buf, sizeof(buf), "%" PRIu32, (uint32_t) _position);
	node->add_property ("position", buf);

	child = node->add_child ("FadeIn");

	for (AutomationList::iterator ii = _fade_in.begin(); ii != _fade_in.end(); ++ii) {
		XMLNode* pnode;

		pnode = new XMLNode ("point");

		snprintf (buf, sizeof (buf), "%" PRIu32, (nframes_t) floor ((*ii)->when));
		pnode->add_property ("x", buf);
		snprintf (buf, sizeof (buf), "%.12g", (*ii)->value);
		pnode->add_property ("y", buf);
		child->add_child_nocopy (*pnode);
	}

	child = node->add_child ("FadeOut");

	for (AutomationList::iterator ii = _fade_out.begin(); ii != _fade_out.end(); ++ii) {
		XMLNode* pnode;

		pnode = new XMLNode ("point");

		snprintf (buf, sizeof (buf), "%" PRIu32, (nframes_t) floor ((*ii)->when));
		pnode->add_property ("x", buf);
		snprintf (buf, sizeof (buf), "%.12g", (*ii)->value);
		pnode->add_property ("y", buf);
		child->add_child_nocopy (*pnode);
	}

	return *node;
}

int
Crossfade::set_state (const XMLNode& node)
{
	XMLNodeConstIterator i;
	XMLNodeList children;
	XMLNode* fi;
	XMLNode* fo;
	const XMLProperty* prop;
	LocaleGuard lg (X_("POSIX"));
	Change what_changed = Change (0);
	nframes_t val;

	if ((prop = node.property ("position")) != 0) {
		sscanf (prop->value().c_str(), "%" PRIu32, &val);
		if (val != _position) {
			_position = val;
			what_changed = Change (what_changed | PositionChanged);
		}
	} else {
		warning << _("old-style crossfade information - no position information") << endmsg;
		_position = _in->first_frame();
	}

	if ((prop = node.property ("active")) != 0) {
		bool x = (prop->value() == "yes");
		if (x != _active) {
			_active = x;
			what_changed = Change (what_changed | ActiveChanged);
		}
	} else {
		_active = true;
	}

	if ((prop = node.property ("follow-overlap")) != 0) {
		_follow_overlap = (prop->value() == "yes");
	} else {
		_follow_overlap = false;
	}

	if ((prop = node.property ("fixed")) != 0) {
		_fixed = (prop->value() == "yes");
	} else {
		_fixed = false;
	}

	if ((prop = node.property ("anchor-point")) != 0) {
		_anchor_point = AnchorPoint (atoi ((prop->value().c_str())));
	} else {
		_anchor_point = StartOfIn;
	}

	if ((prop = node.property ("length")) != 0) {

		sscanf (prop->value().c_str(), "%" PRIu32, &val);
		if (val != _length) {
			_length = atol (prop->value().c_str());
			what_changed = Change (what_changed | LengthChanged);
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
			nframes_t x;
			float y;
			
			prop = (*i)->property ("x");
			sscanf (prop->value().c_str(), "%" PRIu32, &x);
			
			prop = (*i)->property ("y");
			sscanf (prop->value().c_str(), "%f", &y);

			_fade_in.add (x, y);
		}
	}

	_fade_in.thaw ();
	
        /* fade out */
	
	_fade_in.freeze ();
	_fade_out.clear ();

	children = fo->children();
	
	for (i = children.begin(); i != children.end(); ++i) {
		if ((*i)->name() == "point") {
			nframes_t x;
			float y;
			XMLProperty* prop;

			prop = (*i)->property ("x");
			sscanf (prop->value().c_str(), "%" PRIu32, &x);

			prop = (*i)->property ("y");
			sscanf (prop->value().c_str(), "%f", &y);
			
			_fade_out.add (x, y);
		}
	}

	_fade_out.thaw ();

	StateChanged (what_changed); /* EMIT SIGNAL */

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
		set_length (_short_xfade_length);
	} else {
		set_length (_out->first_frame() + _out->length() - _in->first_frame());
	}

	StateChanged (FollowOverlapChanged);
}

nframes_t
Crossfade::set_length (nframes_t len)
{
	nframes_t limit;

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

	StateChanged (LengthChanged);

	return len;
}

nframes_t
Crossfade::overlap_length () const
{
	if (_fixed) {
		return _length;
	}
	return _out->first_frame() + _out->length() - _in->first_frame();
}

void
Crossfade::set_short_xfade_length (nframes_t n)
{
	_short_xfade_length = n;
}

void
Crossfade::invalidate ()
{
	Invalidated (this); /* EMIT SIGNAL */
}
