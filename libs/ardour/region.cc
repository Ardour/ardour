/*
    Copyright (C) 2000-2003 Paul Davis 

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

#include <iostream>
#include <cmath>
#include <climits>
#include <algorithm>

#include <sigc++/bind.h>
#include <sigc++/class_slot.h>

#include <glibmm/thread.h>
#include <pbd/xml++.h>
#include <pbd/stacktrace.h>

#include <ardour/region.h>
#include <ardour/playlist.h>
#include <ardour/session.h>
#include <ardour/region_factory.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

Change Region::FadeChanged       = ARDOUR::new_change ();
Change Region::SyncOffsetChanged = ARDOUR::new_change ();
Change Region::MuteChanged       = ARDOUR::new_change ();
Change Region::OpacityChanged    = ARDOUR::new_change ();
Change Region::LockChanged       = ARDOUR::new_change ();
Change Region::LayerChanged      = ARDOUR::new_change ();
Change Region::HiddenChanged     = ARDOUR::new_change ();

Region::Region (nframes_t start, nframes_t length, const string& name, layer_t layer, Region::Flag flags)
{
	/* basic Region constructor */

	_flags = flags;
	_read_data_count = 0;
	_frozen = 0;
	pending_changed = Change (0);

	_name = name;
	_start = start; 
	_sync_position = _start;
	_length = length; 
	_position = 0; 
	_layer = layer;
	_read_data_count = 0;
	_first_edit = EditChangesNothing;
	_last_layer_op = 0;
}

Region::Region (boost::shared_ptr<const Region> other, nframes_t offset, nframes_t length, const string& name, layer_t layer, Flag flags)
{
	/* create a new Region from part of an existing one */

	_frozen = 0;
	pending_changed = Change (0);
	_read_data_count = 0;

	_start = other->_start + offset; 
	if (other->_sync_position < offset) {
		_sync_position = other->_sync_position;
	} else {
		_sync_position = _start;
	}
	_length = length; 
	_name = name;
	_position = 0; 
	_layer = layer; 
	_flags = Flag (flags & ~(Locked|WholeFile|Hidden));
	_first_edit = EditChangesNothing;
	_last_layer_op = 0;
}

Region::Region (boost::shared_ptr<const Region> other)
{
	/* Pure copy constructor */

	_frozen = 0;
	pending_changed = Change (0);
	_read_data_count = 0;

	_first_edit = EditChangesID;
	other->_first_edit = EditChangesName;

	if (other->_extra_xml) {
		_extra_xml = new XMLNode (*other->_extra_xml);
	} else {
		_extra_xml = 0;
	}

	_start = other->_start;
	_sync_position = other->_sync_position;
	_length = other->_length; 
	_name = other->_name;
	_position = other->_position; 
	_layer = other->_layer; 
	_flags = Flag (other->_flags & ~Locked);
	_last_layer_op = other->_last_layer_op;
}

Region::Region (const XMLNode& node)
{
	_frozen = 0;
	pending_changed = Change (0);
	_read_data_count = 0;
	_start = 0; 
	_sync_position = _start;
	_length = 0;
	_name = X_("error: XML did not reset this");
	_position = 0; 
	_layer = 0;
	_flags = Flag (0);
	_first_edit = EditChangesNothing;

	if (set_state (node)) {
		throw failed_constructor();
	}
}

Region::~Region ()
{
	/* derived classes must call notify_callbacks() and then emit GoingAway */
}

void
Region::set_playlist (boost::weak_ptr<Playlist> pl)
{
	_playlist = pl;
}

void
Region::set_name (string str)
{
	if (_name != str) {
		_name = str; 
		send_change (NameChanged);
	}
}

void
Region::set_length (nframes_t len, void *src)
{
	if (_flags & Locked) {
		return;
	}

	if (_length != len && len != 0) {

		/* check that the current _position wouldn't make the new 
		   length impossible.
		*/

		if (max_frames - len < _position) {
			return;
		}

		if (!verify_length (len)) {
			return;
		}
		
		_length = len;

		_flags = Region::Flag (_flags & ~WholeFile);

		first_edit ();
		maybe_uncopy ();

		if (!_frozen) {
			recompute_at_end ();
		}

		send_change (LengthChanged);
	}
}

void
Region::maybe_uncopy ()
{
}

void
Region::first_edit ()
{
	boost::shared_ptr<Playlist> pl (playlist());

	if (_first_edit != EditChangesNothing && pl) {

		_name = pl->session().new_region_name (_name);
		_first_edit = EditChangesNothing;

		send_change (NameChanged);
		RegionFactory::CheckNewRegion (shared_from_this());
	}
}

bool
Region::at_natural_position () const
{
	boost::shared_ptr<Playlist> pl (playlist());

	if (!pl) {
		return false;
	}
	
	boost::shared_ptr<Region> whole_file_region = get_parent();

	if (whole_file_region) {
		if (_position == whole_file_region->position() + _start) {
			return true;
		}
	}

	return false;
}

void
Region::move_to_natural_position (void *src)
{
	boost::shared_ptr<Playlist> pl (playlist());

	if (!pl) {
		return;
	}
	
	boost::shared_ptr<Region> whole_file_region = get_parent();

	if (whole_file_region) {
		set_position (whole_file_region->position() + _start, src);
	}
}
	
void
Region::special_set_position (nframes_t pos)
{
	/* this is used when creating a whole file region as 
	   a way to store its "natural" or "captured" position.
	*/

	_position = pos;
}

void
Region::set_position (nframes_t pos, void *src)
{
	if (_flags & Locked) {
		return;
	}

	if (_position != pos) {
		_position = pos;

		/* check that the new _position wouldn't make the current
		   length impossible - if so, change the length. 

		   XXX is this the right thing to do?
		*/

		if (max_frames - _length < _position) {
			_length = max_frames - _position;
		}
	}

	/* do this even if the position is the same. this helps out
	   a GUI that has moved its representation already.
	*/

	send_change (PositionChanged);
}

void
Region::set_position_on_top (nframes_t pos, void *src)
{
	if (_flags & Locked) {
		return;
	}

	if (_position != pos) {
		_position = pos;
	}

	boost::shared_ptr<Playlist> pl (playlist());

	if (pl) {
		pl->raise_region_to_top (shared_from_this ());
	}

	/* do this even if the position is the same. this helps out
	   a GUI that has moved its representation already.
	*/
	
	send_change (PositionChanged);
}

void
Region::nudge_position (long n, void *src)
{
	if (_flags & Locked) {
		return;
	}

	if (n == 0) {
		return;
	}
	
	if (n > 0) {
		if (_position > max_frames - n) {
			_position = max_frames;
		} else {
			_position += n;
		}
	} else {
		if (_position < (nframes_t) -n) {
			_position = 0;
		} else {
			_position += n;
		}
	}

	send_change (PositionChanged);
}

void
Region::set_start (nframes_t pos, void *src)
{
	if (_flags & Locked) {
		return;
	}
	/* This just sets the start, nothing else. It effectively shifts
	   the contents of the Region within the overall extent of the Source,
	   without changing the Region's position or length
	*/

	if (_start != pos) {

		if (!verify_start (pos)) {
			return;
		}

		_start = pos;
		_flags = Region::Flag (_flags & ~WholeFile);
		first_edit ();

		send_change (StartChanged);
	}
}

void
Region::trim_start (nframes_t new_position, void *src)
{
	if (_flags & Locked) {
		return;
	}
	nframes_t new_start;
	int32_t start_shift;
	
	if (new_position > _position) {
		start_shift = new_position - _position;
	} else {
		start_shift = -(_position - new_position);
	}

	if (start_shift > 0) {

		if (_start > max_frames - start_shift) {
			new_start = max_frames;
		} else {
			new_start = _start + start_shift;
		}

		if (!verify_start (new_start)) {
			return;
		}

	} else if (start_shift < 0) {

		if (_start < (nframes_t) -start_shift) {
			new_start = 0;
		} else {
			new_start = _start + start_shift;
		}
	} else {
		return;
	}

	if (new_start == _start) {
		return;
	}
	
	_start = new_start;
	_flags = Region::Flag (_flags & ~WholeFile);
	first_edit ();

	send_change (StartChanged);
}

void
Region::trim_front (nframes_t new_position, void *src)
{
	if (_flags & Locked) {
		return;
	}

	nframes_t end = last_frame();
	nframes_t source_zero;

	if (_position > _start) {
		source_zero = _position - _start;
	} else {
		source_zero = 0; // its actually negative, but this will work for us
	}

	if (new_position < end) { /* can't trim it zero or negative length */
		
		nframes_t newlen;

		/* can't trim it back passed where source position zero is located */
		
		new_position = max (new_position, source_zero);
		
		
		if (new_position > _position) {
			newlen = _length - (new_position - _position);
		} else {
			newlen = _length + (_position - new_position);
		}
		
		trim_to_internal (new_position, newlen, src);
		if (!_frozen) {
			recompute_at_start ();
		}
	}
}

void
Region::trim_end (nframes_t new_endpoint, void *src)
{
	if (_flags & Locked) {
		return;
	}

	if (new_endpoint > _position) {
		trim_to_internal (_position, new_endpoint - _position, this);
		if (!_frozen) {
			recompute_at_end ();
		}
	}
}

void
Region::trim_to (nframes_t position, nframes_t length, void *src)
{
	if (_flags & Locked) {
		return;
	}

	trim_to_internal (position, length, src);

	if (!_frozen) {
		recompute_at_start ();
		recompute_at_end ();
	}
}

void
Region::trim_to_internal (nframes_t position, nframes_t length, void *src)
{
	int32_t start_shift;
	nframes_t new_start;

	if (_flags & Locked) {
		return;
	}

	if (position > _position) {
		start_shift = position - _position;
	} else {
		start_shift = -(_position - position);
	}

	if (start_shift > 0) {

		if (_start > max_frames - start_shift) {
			new_start = max_frames;
		} else {
			new_start = _start + start_shift;
		}


	} else if (start_shift < 0) {

		if (_start < (nframes_t) -start_shift) {
			new_start = 0;
		} else {
			new_start = _start + start_shift;
		}
	} else {
		new_start = _start;
	}

	if (!verify_start_and_length (new_start, length)) {
		return;
	}

	Change what_changed = Change (0);

	if (_start != new_start) {
		_start = new_start;
		what_changed = Change (what_changed|StartChanged);
	}
	if (_length != length) {
		_length = length;
		what_changed = Change (what_changed|LengthChanged);
	}
	if (_position != position) {
		_position = position;
		what_changed = Change (what_changed|PositionChanged);
	}
	
	_flags = Region::Flag (_flags & ~WholeFile);

	if (what_changed & (StartChanged|LengthChanged)) {
		first_edit ();
	} 

	if (what_changed) {
		send_change (what_changed);
	}
}	

void
Region::set_hidden (bool yn)
{
	if (hidden() != yn) {

		if (yn) {
			_flags = Flag (_flags|Hidden);
		} else {
			_flags = Flag (_flags & ~Hidden);
		}

		send_change (HiddenChanged);
	}
}

void
Region::set_muted (bool yn)
{
	if (muted() != yn) {

		if (yn) {
			_flags = Flag (_flags|Muted);
		} else {
			_flags = Flag (_flags & ~Muted);
		}

		send_change (MuteChanged);
	}
}

void
Region::set_opaque (bool yn)
{
	if (opaque() != yn) {
		if (yn) {
			_flags = Flag (_flags|Opaque);
		} else {
			_flags = Flag (_flags & ~Opaque);
		}
			send_change (OpacityChanged);
	}
}

void
Region::set_locked (bool yn)
{
	if (locked() != yn) {
		if (yn) {
			_flags = Flag (_flags|Locked);
		} else {
			_flags = Flag (_flags & ~Locked);
		}
		send_change (LockChanged);
	}
}

void
Region::set_sync_position (nframes_t absolute_pos)
{
	nframes_t file_pos;

	file_pos = _start + (absolute_pos - _position);

	if (file_pos != _sync_position) {
		
		_sync_position = file_pos;
		_flags = Flag (_flags|SyncMarked);

		if (!_frozen) {
			maybe_uncopy ();
		}
		send_change (SyncOffsetChanged);
	}
}

void
Region::clear_sync_position ()
{
	if (_flags & SyncMarked) {
		_flags = Flag (_flags & ~SyncMarked);

		if (!_frozen) {
			maybe_uncopy ();
		}
		send_change (SyncOffsetChanged);
	}
}

nframes_t
Region::sync_offset (int& dir) const
{
	/* returns the sync point relative the first frame of the region */

	if (_flags & SyncMarked) {
		if (_sync_position > _start) {
			dir = 1;
			return _sync_position - _start; 
		} else {
			dir = -1;
			return _start - _sync_position;
		}
	} else {
		dir = 0;
		return 0;
	}
}

nframes_t 
Region::adjust_to_sync (nframes_t pos)
{
	int sync_dir;
	nframes_t offset = sync_offset (sync_dir);
	
	if (sync_dir > 0) {
		if (max_frames - pos > offset) {
			pos += offset;
		}
	} else {
		if (pos > offset) {
			pos -= offset;
		} else {
			pos = 0;
		}
	}

	return pos;
}

nframes_t
Region::sync_position() const
{
	if (_flags & SyncMarked) {
		return _sync_position; 
	} else {
		return _start;
	}
}


void
Region::raise ()
{
	boost::shared_ptr<Playlist> pl (playlist());
	if (pl) {
		pl->raise_region (shared_from_this ());
	}
}

void
Region::lower ()
{
	boost::shared_ptr<Playlist> pl (playlist());
	if (pl) {
		pl->lower_region (shared_from_this ());
	}
}

void
Region::raise_to_top ()
{
	boost::shared_ptr<Playlist> pl (playlist());
	if (pl) {
		pl->raise_region_to_top (shared_from_this());
	}
}

void
Region::lower_to_bottom ()
{
	boost::shared_ptr<Playlist> pl (playlist());
	if (pl) {
		pl->lower_region_to_bottom (shared_from_this());
	}
}

void
Region::set_layer (layer_t l)
{
	if (_layer != l) {
		_layer = l;
		
		send_change (LayerChanged);
	}
}

XMLNode&
Region::state (bool full_state)
{
	XMLNode *node = new XMLNode ("Region");
	char buf[64];
	char* fe = NULL;

	_id.print (buf, sizeof (buf));
	node->add_property ("id", buf);
	node->add_property ("name", _name);
	snprintf (buf, sizeof (buf), "%u", _start);
	node->add_property ("start", buf);
	snprintf (buf, sizeof (buf), "%u", _length);
	node->add_property ("length", buf);
	snprintf (buf, sizeof (buf), "%u", _position);
	node->add_property ("position", buf);
	
	switch (_first_edit) {
	case EditChangesNothing:
		fe = X_("nothing");
		break;
	case EditChangesName:
		fe = X_("name");
		break;
	case EditChangesID:
		fe = X_("id");
		break;
	default: /* should be unreachable but makes g++ happy */
		cerr << "Odd region property found\n";
		fe = X_("nothing");
		break;
	}

	node->add_property ("first_edit", fe);

	/* note: flags are stored by derived classes */

	snprintf (buf, sizeof (buf), "%d", (int) _layer);
	node->add_property ("layer", buf);
	snprintf (buf, sizeof (buf), "%" PRIu32, _sync_position);
	node->add_property ("sync-position", buf);

	return *node;
}

XMLNode&
Region::get_state ()
{
	return state (true);
}

int
Region::set_live_state (const XMLNode& node, Change& what_changed, bool send)
{
	const XMLNodeList& nlist = node.children();
	const XMLProperty *prop;
	nframes_t val;

	/* this is responsible for setting those aspects of Region state 
	   that are mutable after construction.
	*/

	if ((prop = node.property ("name")) == 0) {
		error << _("XMLNode describing a Region is incomplete (no name)") << endmsg;
		return -1;
	}

	_name = prop->value();

	if ((prop = node.property ("start")) != 0) {
		sscanf (prop->value().c_str(), "%" PRIu32, &val);
		if (val != _start) {
			what_changed = Change (what_changed|StartChanged);	
			_start = val;
		}
	} else {
		_start = 0;
	}

	if ((prop = node.property ("length")) != 0) {
		sscanf (prop->value().c_str(), "%" PRIu32, &val);
		if (val != _length) {
			what_changed = Change (what_changed|LengthChanged);
			_length = val;
		}
	} else {
		_length = 1;
	}

	if ((prop = node.property ("position")) != 0) {
		sscanf (prop->value().c_str(), "%" PRIu32, &val);
		if (val != _position) {
			what_changed = Change (what_changed|PositionChanged);
			_position = val;
		}
	} else {
		_position = 0;
	}

	if ((prop = node.property ("layer")) != 0) {
		layer_t x;
		x = (layer_t) atoi (prop->value().c_str());
		if (x != _layer) {
			what_changed = Change (what_changed|LayerChanged);
			_layer = x;
		}
	} else {
		_layer = 0;
	}

	if ((prop = node.property ("sync-position")) != 0) {
		sscanf (prop->value().c_str(), "%" PRIu32, &val);
		if (val != _sync_position) {
			what_changed = Change (what_changed|SyncOffsetChanged);
			_sync_position = val;
		}
	} else {
		_sync_position = _start;
	}

	/* XXX FIRST EDIT !!! */
	
	/* note: derived classes set flags */

	if (_extra_xml) {
		delete _extra_xml;
		_extra_xml = 0;
	}

	for (XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); ++niter) {
		
		XMLNode *child;
		
		child = (*niter);
		
		if (child->name () == "extra") {
			_extra_xml = new XMLNode (*child);
			break;
		}
	}

	if (send) {
		send_change (what_changed);
	}

	return 0;
}

int
Region::set_state (const XMLNode& node)
{
	const XMLProperty *prop;
	Change what_changed = Change (0);

	/* ID is not allowed to change, ever */

	if ((prop = node.property ("id")) == 0) {
		error << _("Session: XMLNode describing a Region is incomplete (no id)") << endmsg;
		return -1;
	}

	_id = prop->value();
	
	_first_edit = EditChangesNothing;
	
	set_live_state (node, what_changed, true);

	return 0;
}

void
Region::freeze ()
{
	_frozen++;
}

void
Region::thaw (const string& why)
{
	Change what_changed = Change (0);

	{
		Glib::Mutex::Lock lm (lock);

		if (_frozen && --_frozen > 0) {
			return;
		}

		if (pending_changed) {
			what_changed = pending_changed;
			pending_changed = Change (0);
		}
	}

	if (what_changed == Change (0)) {
		return;
	}

	if (what_changed & LengthChanged) {
		if (what_changed & PositionChanged) {
			recompute_at_start ();
		} 
		recompute_at_end ();
	}
		
	StateChanged (what_changed);
}

void
Region::send_change (Change what_changed)
{
	{
		Glib::Mutex::Lock lm (lock);
		if (_frozen) {
			pending_changed = Change (pending_changed|what_changed);
			return;
		} 
	}

	StateChanged (what_changed);
}

void
Region::set_last_layer_op (uint64_t when)
{
	_last_layer_op = when;
}

bool
Region::overlap_equivalent (boost::shared_ptr<const Region> other) const
{
	return coverage (other->first_frame(), other->last_frame()) != OverlapNone;
}

bool
Region::equivalent (boost::shared_ptr<const Region> other) const
{
	return _start == other->_start &&
		_position == other->_position &&
		_length == other->_length;
}

bool
Region::size_equivalent (boost::shared_ptr<const Region> other) const
{
	return _start == other->_start &&
		_length == other->_length;
}

bool
Region::region_list_equivalent (boost::shared_ptr<const Region> other) const
{
	return size_equivalent (other) && source_equivalent (other) && _name == other->_name;
}
