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

    $Id$
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
#include <ardour/source.h>
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

/** Basic Region constructor (single source) */
Region::Region (boost::shared_ptr<Source> src, jack_nframes_t start, jack_nframes_t length, const string& name, DataType type, layer_t layer, Region::Flag flags)
	: _name(name)
	, _type(type)
	, _flags(flags)
	, _start(start) 
	, _length(length) 
	, _position(0) 
	, _sync_position(_start)
	, _layer(layer)
	, _first_edit(EditChangesNothing)
	, _frozen(0)
	, _read_data_count(0)
	, _pending_changed(Change (0))
	, _last_layer_op(0)
	, _playlist(0)
{
	_sources.push_back (src);
	_master_sources.push_back (src);
	src->GoingAway.connect (bind (mem_fun (*this, &Region::source_deleted), src));

	assert(_sources.size() > 0);
}

/** Basic Region constructor (many sources) */
Region::Region (SourceList& srcs, jack_nframes_t start, jack_nframes_t length, const string& name, DataType type, layer_t layer, Region::Flag flags)
	: _name(name)
	, _type(type)
	, _flags(flags)
	, _start(start) 
	, _length(length) 
	, _position(0) 
	, _sync_position(_start)
	, _layer(layer)
	, _first_edit(EditChangesNothing)
	, _frozen(0)
	, _read_data_count(0)
	, _pending_changed(Change (0))
	, _last_layer_op(0)
	, _playlist(0)
{
	
	set<boost::shared_ptr<Source> > unique_srcs;

	for (SourceList::iterator i=srcs.begin(); i != srcs.end(); ++i) {
		_sources.push_back (*i);
		(*i)->GoingAway.connect (bind (mem_fun (*this, &Region::source_deleted), (*i)));
		unique_srcs.insert (*i);
	}

	for (SourceList::iterator i = srcs.begin(); i != srcs.end(); ++i) {
		_master_sources.push_back (*i);
		if (unique_srcs.find (*i) == unique_srcs.end()) {
			(*i)->GoingAway.connect (bind (mem_fun (*this, &Region::source_deleted), (*i)));
		}
	}
	
	assert(_sources.size() > 0);
}

/** Create a new Region from part of an existing one */
Region::Region (boost::shared_ptr<const Region> other, nframes_t offset, nframes_t length, const string& name, layer_t layer, Flag flags)
	: _name(name)
	, _type(other->data_type())
	, _flags(Flag(flags & ~(Locked|WholeFile|Hidden)))
	, _start(other->_start + offset) 
	, _length(length) 
	, _position(0) 
	, _sync_position(_start)
	, _layer(layer)
	, _first_edit(EditChangesNothing)
	, _frozen(0)
	, _read_data_count(0)
	, _pending_changed(Change (0))
	, _last_layer_op(0)
	, _playlist(0)
{
	if (other->_sync_position < offset)
		_sync_position = other->_sync_position;

	set<boost::shared_ptr<Source> > unique_srcs;

	for (SourceList::const_iterator i= other->_sources.begin(); i != other->_sources.end(); ++i) {
		_sources.push_back (*i);
		(*i)->GoingAway.connect (bind (mem_fun (*this, &Region::source_deleted), (*i)));
		unique_srcs.insert (*i);
	}
	
	if (other->_sync_position < offset) {
		_sync_position = other->_sync_position;
	}

	for (SourceList::const_iterator i = other->_master_sources.begin(); i != other->_master_sources.end(); ++i) {
		if (unique_srcs.find (*i) == unique_srcs.end()) {
			(*i)->GoingAway.connect (bind (mem_fun (*this, &Region::source_deleted), (*i)));
		}
		_master_sources.push_back (*i);
	}
	
	assert(_sources.size() > 0);
}

/** Pure copy constructor */
Region::Region (boost::shared_ptr<const Region> other)
	: _name(other->_name)
	, _type(other->data_type())
	, _flags(Flag(other->_flags & ~Locked))
	, _start(other->_start) 
	, _length(other->_length) 
	, _position(other->_position) 
	, _sync_position(other->_sync_position)
	, _layer(other->_layer)
	, _first_edit(EditChangesID)
	, _frozen(0)
	, _read_data_count(0)
	, _pending_changed(Change(0))
	, _last_layer_op(other->_last_layer_op)
	, _playlist(0)
{
	other->_first_edit = EditChangesName;

	if (other->_extra_xml) {
		_extra_xml = new XMLNode (*other->_extra_xml);
	} else {
		_extra_xml = 0;
	}

	set<boost::shared_ptr<Source> > unique_srcs;

	for (SourceList::const_iterator i = other->_sources.begin(); i != other->_sources.end(); ++i) {
		_sources.push_back (*i);
		(*i)->GoingAway.connect (bind (mem_fun (*this, &Region::source_deleted), (*i)));
		unique_srcs.insert (*i);
	}

	for (SourceList::const_iterator i = other->_master_sources.begin(); i != other->_master_sources.end(); ++i) {
		_master_sources.push_back (*i);
		if (unique_srcs.find (*i) == unique_srcs.end()) {
			(*i)->GoingAway.connect (bind (mem_fun (*this, &Region::source_deleted), (*i)));
		}
	}
	
	assert(_sources.size() > 0);
}

Region::Region (SourceList& srcs, const XMLNode& node)
	: _name(X_("error: XML did not reset this"))
	, _type(DataType::NIL) // to be loaded from XML
	, _flags(Flag(0))
	, _start(0) 
	, _length(0) 
	, _position(0) 
	, _sync_position(_start)
	, _layer(0)
	, _first_edit(EditChangesNothing)
	, _frozen(0)
	, _read_data_count(0)
	, _pending_changed(Change(0))
	, _last_layer_op(0)
	, _playlist(0)

{

	set<boost::shared_ptr<Source> > unique_srcs;

	for (SourceList::iterator i=srcs.begin(); i != srcs.end(); ++i) {
		_sources.push_back (*i);
		(*i)->GoingAway.connect (bind (mem_fun (*this, &Region::source_deleted), (*i)));
		unique_srcs.insert (*i);
	}

	for (SourceList::iterator i = srcs.begin(); i != srcs.end(); ++i) {
		_master_sources.push_back (*i);
		if (unique_srcs.find (*i) == unique_srcs.end()) {
			(*i)->GoingAway.connect (bind (mem_fun (*this, &Region::source_deleted), (*i)));
		}
	}

	if (set_state (node)) {
		throw failed_constructor();
	}

	assert(_type != DataType::NIL);
	assert(_sources.size() > 0);
}

Region::Region (boost::shared_ptr<Source> src, const XMLNode& node)
	: _name(X_("error: XML did not reset this"))
	, _type(DataType::NIL)
	, _flags(Flag(0))
	, _start(0) 
	, _length(0) 
	, _position(0) 
	, _sync_position(_start)
	, _layer(0)
	, _first_edit(EditChangesNothing)
	, _frozen(0)
	, _read_data_count(0)
	, _pending_changed(Change(0))
	, _last_layer_op(0)
	, _playlist(0)
{
	_sources.push_back (src);


	if (set_state (node)) {
		throw failed_constructor();
	}
	
	assert(_type != DataType::NIL);
	assert(_sources.size() > 0);
}

Region::~Region ()
{
	if (_playlist) {
		for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {
			(*i)->remove_playlist (_playlist);
		}
	}
	
	notify_callbacks ();
	GoingAway (); /* EMIT SIGNAL */
}

void
Region::set_playlist (Playlist* pl)
{
	if (pl == _playlist) {
		return;
	}

	Playlist* old_playlist = _playlist;

	if (pl) {
		if (old_playlist) {
			for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {
				(*i)->remove_playlist (old_playlist);	
				(*i)->add_playlist (_playlist);
			}
		} else {
			for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {
				(*i)->add_playlist (_playlist);
			}
		}
	} else {
		if (old_playlist) {
			for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {
				(*i)->remove_playlist (old_playlist);
			}
		}
	}
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
	if (_first_edit != EditChangesNothing && _playlist) {

		_name = _playlist->session().new_region_name (_name);
		_first_edit = EditChangesNothing;

		send_change (NameChanged);
		RegionFactory::CheckNewRegion (shared_from_this());
	}
}

bool
Region::at_natural_position () const
{
	if (!_playlist) {
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
	if (!_playlist) {
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

	_playlist->raise_region_to_top (shared_from_this ());

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
	if (_playlist == 0) {
		return;
	}

	_playlist->raise_region (shared_from_this ());
}

void
Region::lower ()
{
	if (_playlist == 0) {
		return;
	}

	_playlist->lower_region (shared_from_this ());
}

void
Region::raise_to_top ()
{

	if (_playlist == 0) {
		return;
	}

	_playlist->raise_region_to_top (shared_from_this());
}

void
Region::lower_to_bottom ()
{
	if (_playlist == 0) {
		return;
	}

	_playlist->lower_region_to_bottom (shared_from_this());
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
	char* fe;

	_id.print (buf, sizeof (buf));
	node->add_property ("id", buf);
	node->add_property ("name", _name);
	node->add_property ("type", _type.to_string());
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
	}

	node->add_property ("first_edit", fe);

	/* note: flags are stored by derived classes */

	snprintf (buf, sizeof (buf), "%d", (int) _layer);
	node->add_property ("layer", buf);
	snprintf (buf, sizeof (buf), "%u", _sync_position);
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
	
	if ((prop = node.property ("type")) == 0) {
		_type = DataType::AUDIO;
	} else {
		_type = DataType(prop->value());
	}

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
		Glib::Mutex::Lock lm (_lock);

		if (_frozen && --_frozen > 0) {
			return;
		}

		if (_pending_changed) {
			what_changed = _pending_changed;
			_pending_changed = Change (0);
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
		Glib::Mutex::Lock lm (_lock);
		if (_frozen) {
			_pending_changed = Change (_pending_changed|what_changed);
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

void
Region::source_deleted (boost::shared_ptr<Source>)
{
	delete this;
}

vector<string>
Region::master_source_names ()
{
	SourceList::iterator i;

	vector<string> names;
	for (i = _master_sources.begin(); i != _master_sources.end(); ++i) {
		names.push_back((*i)->name());
	}

	return names;
}

bool
Region::source_equivalent (boost::shared_ptr<const Region> other) const
{
	if (!other)
		return false;

	SourceList::const_iterator i;
	SourceList::const_iterator io;

	for (i = _sources.begin(), io = other->_sources.begin(); i != _sources.end() && io != other->_sources.end(); ++i, ++io) {
		if ((*i)->id() != (*io)->id()) {
			return false;
		}
	}

	for (i = _master_sources.begin(), io = other->_master_sources.begin(); i != _master_sources.end() && io != other->_master_sources.end(); ++i, ++io) {
		if ((*i)->id() != (*io)->id()) {
			return false;
		}
	}

	return true;
}

bool
Region::verify_length (jack_nframes_t len)
{
	for (uint32_t n=0; n < _sources.size(); ++n) {
		if (_start > _sources[n]->length() - len) {
			return false;
		}
	}
	return true;
}

bool
Region::verify_start_and_length (jack_nframes_t new_start, jack_nframes_t new_length)
{
	for (uint32_t n=0; n < _sources.size(); ++n) {
		if (new_length > _sources[n]->length() - new_start) {
			return false;
		}
	}
	return true;
}
bool
Region::verify_start (jack_nframes_t pos)
{
	for (uint32_t n=0; n < _sources.size(); ++n) {
		if (pos > _sources[n]->length() - _length) {
			return false;
		}
	}
	return true;
}

bool
Region::verify_start_mutable (jack_nframes_t& new_start)
{
	for (uint32_t n=0; n < _sources.size(); ++n) {
		if (new_start > _sources[n]->length() - _length) {
			new_start = _sources[n]->length() - _length;
		}
	}
	return true;
}

boost::shared_ptr<Region>
Region::get_parent() const
{
	if (_playlist) {
		boost::shared_ptr<Region> r;
		boost::shared_ptr<Region const> grrr2 = boost::dynamic_pointer_cast<Region const> (shared_from_this());
		
		if (grrr2 && (r = _playlist->session().find_whole_file_parent (grrr2))) {
			return boost::static_pointer_cast<Region> (r);
		}
	}
	
	return boost::shared_ptr<Region>();
}

