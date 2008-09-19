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
#include <sstream>

#include <sigc++/bind.h>
#include <sigc++/class_slot.h>

#include <glibmm/thread.h>
#include <pbd/xml++.h>
#include <pbd/stacktrace.h>
#include <pbd/enumwriter.h>

#include <ardour/region.h>
#include <ardour/playlist.h>
#include <ardour/session.h>
#include <ardour/source.h>
#include <ardour/tempo.h>
#include <ardour/region_factory.h>
#include <ardour/filter.h>

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

sigc::signal<void,boost::shared_ptr<ARDOUR::Region> > Region::RegionPropertyChanged;

/* derived-from-derived constructor (no sources in constructor) */
Region::Region (Session& s, nframes_t start, nframes_t length, const string& name, DataType type, layer_t layer, Region::Flag flags)
	: Automatable(s, name)
	, _type(type)
	, _flags(flags)
	, _start(start) 
	, _length(length) 
	, _position(0) 
	, _last_position(0) 
	, _positional_lock_style(AudioTime)
	, _sync_position(_start)
	, _layer(layer)
	, _first_edit(EditChangesNothing)
	, _frozen(0)
	, _stretch(1.0)
	, _read_data_count(0)
	, _pending_changed(Change (0))
	, _last_layer_op(0)
{
	/* no sources at this point */
}

/** Basic Region constructor (single source) */
Region::Region (boost::shared_ptr<Source> src, nframes_t start, nframes_t length, const string& name, DataType type, layer_t layer, Region::Flag flags)
	: Automatable(src->session(), name)
	, _type(type)
	, _flags(flags)
	, _start(start) 
	, _length(length) 
	, _position(0) 
	, _last_position(0) 
	, _positional_lock_style(AudioTime)
	, _sync_position(_start)
	, _layer(layer)
	, _first_edit(EditChangesNothing)
	, _frozen(0)
	, _ancestral_start (start)
	, _ancestral_length (length)
	, _stretch (1.0)
	, _shift (0.0)
	, _valid_transients(false)
	, _read_data_count(0)
	, _pending_changed(Change (0))
	, _last_layer_op(0)

{
	_sources.push_back (src);
	_master_sources.push_back (src);

	src->GoingAway.connect (bind (mem_fun (*this, &Region::source_deleted), src));

	assert(_sources.size() > 0);
	_positional_lock_style = AudioTime;
}

/** Basic Region constructor (many sources) */
Region::Region (const SourceList& srcs, nframes_t start, nframes_t length, const string& name, DataType type, layer_t layer, Region::Flag flags)
	: Automatable(srcs.front()->session(), name)
	, _type(type)
	, _flags(flags)
	, _start(start) 
	, _length(length) 
	, _position(0) 
	, _last_position(0) 
	, _positional_lock_style(AudioTime)
	, _sync_position(_start)
	, _layer(layer)
	, _first_edit(EditChangesNothing)
	, _frozen(0)
	, _stretch(1.0)
	, _read_data_count(0)
	, _pending_changed(Change (0))
	, _last_layer_op(0)
{
	
	set<boost::shared_ptr<Source> > unique_srcs;

	for (SourceList::const_iterator i=srcs.begin(); i != srcs.end(); ++i) {
		_sources.push_back (*i);
		(*i)->GoingAway.connect (bind (mem_fun (*this, &Region::source_deleted), (*i)));
		unique_srcs.insert (*i);
	}

	for (SourceList::const_iterator i = srcs.begin(); i != srcs.end(); ++i) {
		_master_sources.push_back (*i);
		if (unique_srcs.find (*i) == unique_srcs.end()) {
			(*i)->GoingAway.connect (bind (mem_fun (*this, &Region::source_deleted), (*i)));
		}
	}
	
	assert(_sources.size() > 0);
}

/** Create a new Region from part of an existing one */
Region::Region (boost::shared_ptr<const Region> other, nframes_t offset, nframes_t length, const string& name, layer_t layer, Flag flags)
	: Automatable(other->session(), name)
	, _type(other->data_type())
	, _flags(Flag(flags & ~(Locked|PositionLocked|WholeFile|Hidden)))
	, _start(other->_start + offset) 
	, _length(length) 
	, _position(0) 
	, _last_position(0) 
	, _positional_lock_style(other->_positional_lock_style)
	, _sync_position(_start)
	, _layer(layer)
	, _first_edit(EditChangesNothing)
	, _frozen(0)
	, _ancestral_start (other->_ancestral_start + offset)
	, _ancestral_length (length)
	, _stretch (1.0)
	, _shift (0.0)
	, _valid_transients(false)
	, _read_data_count(0)
	, _pending_changed(Change (0))
	, _last_layer_op(0)
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
	: Automatable(other->session(), other->name())
	, _type(other->data_type())
	, _flags(Flag(other->_flags & ~(Locked|PositionLocked)))
	, _start(other->_start) 
	, _length(other->_length) 
	, _position(other->_position) 
	, _last_position(other->_last_position) 
	, _positional_lock_style(other->_positional_lock_style)
	, _sync_position(other->_sync_position)
	, _layer(other->_layer)
	, _first_edit(EditChangesID)
	, _frozen(0)
	, _ancestral_start (_start)
	, _ancestral_length (_length)
	, _stretch (1.0)
	, _shift (0.0)
	, _valid_transients(false)
	, _read_data_count(0)
	, _pending_changed(Change(0))
	, _last_layer_op(other->_last_layer_op)
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

Region::Region (const SourceList& srcs, const XMLNode& node)
	: Automatable(srcs.front()->session(), X_("error: XML did not reset this"))
	, _type(DataType::NIL) // to be loaded from XML
	, _flags(Flag(0))
	, _start(0) 
	, _length(0) 
	, _position(0) 
	, _last_position(0) 
	, _positional_lock_style(AudioTime)
	, _sync_position(_start)
	, _layer(0)
	, _first_edit(EditChangesNothing)
	, _frozen(0)
	, _stretch(1.0)
	, _read_data_count(0)
	, _pending_changed(Change(0))
	, _last_layer_op(0)
{
	set<boost::shared_ptr<Source> > unique_srcs;

	for (SourceList::const_iterator i=srcs.begin(); i != srcs.end(); ++i) {
		_sources.push_back (*i);
		(*i)->GoingAway.connect (bind (mem_fun (*this, &Region::source_deleted), (*i)));
		unique_srcs.insert (*i);
	}

	for (SourceList::const_iterator i = srcs.begin(); i != srcs.end(); ++i) {
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
	: Automatable(src->session(), X_("error: XML did not reset this"))
	, _type(DataType::NIL)
	, _flags(Flag(0))
	, _start(0) 
	, _length(0) 
	, _position(0) 
	, _last_position(0) 
	, _positional_lock_style(AudioTime)
	, _sync_position(_start)
	, _layer(0)
	, _first_edit(EditChangesNothing)
	, _frozen(0)
	, _stretch(1.0)
	, _read_data_count(0)
	, _pending_changed(Change(0))
	, _last_layer_op(0)
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
	boost::shared_ptr<Playlist> pl (playlist());

	if (pl) {
		for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {
			(*i)->remove_playlist (pl);
		}
		for (SourceList::const_iterator i = _master_sources.begin(); i != _master_sources.end(); ++i) {
			(*i)->remove_playlist (pl);
		}
	}
	
	notify_callbacks ();
	GoingAway (); /* EMIT SIGNAL */
}

void
Region::set_playlist (boost::weak_ptr<Playlist> wpl)
{
	boost::shared_ptr<Playlist> old_playlist = (_playlist.lock());

	boost::shared_ptr<Playlist> pl (wpl.lock());

	if (old_playlist == pl) {
		return;
	}

	_playlist = pl;

	if (pl) {
		if (old_playlist) {
			for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {
				(*i)->remove_playlist (_playlist);	
				(*i)->add_playlist (pl);
			}
			for (SourceList::const_iterator i = _master_sources.begin(); i != _master_sources.end(); ++i) {
				(*i)->remove_playlist (_playlist);	
				(*i)->add_playlist (pl);
			}
		} else {
			for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {
				(*i)->add_playlist (pl);
			}
			for (SourceList::const_iterator i = _master_sources.begin(); i != _master_sources.end(); ++i) {
				(*i)->add_playlist (pl);
			}
		}
	} else {
		if (old_playlist) {
			for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {
				(*i)->remove_playlist (old_playlist);
			}
			for (SourceList::const_iterator i = _master_sources.begin(); i != _master_sources.end(); ++i) {
				(*i)->remove_playlist (old_playlist);
			}
		}
	}
}

bool
Region::set_name (const std::string& str)
{
	if (_name != str) {
		SessionObject::set_name(str); // EMIT SIGNAL NameChanged()
		assert(_name == str); 
		send_change (ARDOUR::NameChanged);
	}

	return true;
}

void
Region::set_length (nframes_t len, void *src)
{
	//cerr << "Region::set_length() len = " << len << endl;
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
		

		_last_length = _length;
		_length = len;

		_flags = Region::Flag (_flags & ~WholeFile);

		first_edit ();
		maybe_uncopy ();
		invalidate_transients ();

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

		send_change (ARDOUR::NameChanged);
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

	_position = _position;
	_position = pos;
}

void
Region::set_position_lock_style (PositionLockStyle ps)
{
	boost::shared_ptr<Playlist> pl (playlist());

	if (!pl) {
		return;
	}

	_positional_lock_style = ps;

	if (_positional_lock_style == MusicTime) {
		pl->session().tempo_map().bbt_time (_position, _bbt_time);
	}
	
}

void
Region::update_position_after_tempo_map_change ()
{
	boost::shared_ptr<Playlist> pl (playlist());
	
	if (!pl || _positional_lock_style != MusicTime) {
		return;
	}

	TempoMap& map (pl->session().tempo_map());
	nframes_t pos = map.frame_time (_bbt_time);
	set_position_internal (pos, false);
}

void
Region::set_position (nframes_t pos, void *src)
{
	if (!can_move()) {
		return;
	}

	set_position_internal (pos, true);
}

void
Region::set_position_internal (nframes_t pos, bool allow_bbt_recompute)
{
	if (_position != pos) {
		_last_position = _position;
		_position = pos;

		/* check that the new _position wouldn't make the current
		   length impossible - if so, change the length. 

		   XXX is this the right thing to do?
		*/

		if (max_frames - _length < _position) {
			_last_length = _length;
			_length = max_frames - _position;
		}

		if (allow_bbt_recompute) {
			recompute_position_from_lock_style ();
		}

		invalidate_transients ();
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
		_last_position = _position;
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
Region::recompute_position_from_lock_style ()
{
	if (_positional_lock_style == MusicTime) {
		boost::shared_ptr<Playlist> pl (playlist());
		if (pl) {
			pl->session().tempo_map().bbt_time (_position, _bbt_time);
		}
	}
}
		
void
Region::nudge_position (nframes64_t n, void *src)
{
	if (_flags & Locked) {
		return;
	}

	if (n == 0) {
		return;
	}
	
	_last_position = _position;

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
Region::set_ancestral_data (nframes64_t s, nframes64_t l, float st, float sh)
{
	_ancestral_length = l;
	_ancestral_start = s;
	_stretch = st;
	_shift = sh;
}

void
Region::set_start (nframes_t pos, void *src)
{
	if (_flags & (Locked|PositionLocked)) {
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
		invalidate_transients ();

		send_change (StartChanged);
	}
}

void
Region::trim_start (nframes_t new_position, void *src)
{
	if (_flags & (Locked|PositionLocked)) {
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
		if (!_frozen) {
			_last_length = _length;
		}
		_length = length;
		what_changed = Change (what_changed|LengthChanged);
	}
	if (_position != position) {
		if (!_frozen) {
			_last_position = _position;
		}
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
Region::set_position_locked (bool yn)
{
	if (position_locked() != yn) {
		if (yn) {
			_flags = Flag (_flags|PositionLocked);
		} else {
			_flags = Flag (_flags & ~PositionLocked);
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

	// cerr << "adjusting pos = " << pos << " to sync at " << _sync_position << " offset = " << offset << " with dir = " << sync_dir << endl;
	
	if (sync_dir > 0) {
		if (pos > offset) {
			pos -= offset;
		} else {
			pos = 0;
		}
	} else {
		if (max_frames - pos > offset) {
			pos += offset;
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
	const char* fe = NULL;

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
	snprintf (buf, sizeof (buf), "%" PRIi64, _ancestral_start);
	node->add_property ("ancestral-start", buf);
	snprintf (buf, sizeof (buf), "%" PRIi64, _ancestral_length);
	node->add_property ("ancestral-length", buf);
	snprintf (buf, sizeof (buf), "%.12g", _stretch);
	node->add_property ("stretch", buf);
	snprintf (buf, sizeof (buf), "%.12g", _shift);
	node->add_property ("shift", buf);
	
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
		fe = X_("nothing");
		break;
	}

	node->add_property ("first_edit", fe);

	/* note: flags are stored by derived classes */

	snprintf (buf, sizeof (buf), "%d", (int) _layer);
	node->add_property ("layer", buf);
	snprintf (buf, sizeof (buf), "%" PRIu32, _sync_position);
	node->add_property ("sync-position", buf);

	if (_positional_lock_style != AudioTime) {
		node->add_property ("positional-lock-style", enum_2_string (_positional_lock_style));
		stringstream str;
		str << _bbt_time;
		node->add_property ("bbt-position", str.str());
	}

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
			_last_length = _length;
			_length = val;
		}
	} else {
		_last_length = _length;
		_length = 1;
	}

	if ((prop = node.property ("position")) != 0) {
		sscanf (prop->value().c_str(), "%" PRIu32, &val);
		if (val != _position) {
			what_changed = Change (what_changed|PositionChanged);
			_last_position = _position;
			_position = val;
		}
	} else {
		_last_position = _position;
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

	if ((prop = node.property ("positional-lock-style")) != 0) {
		_positional_lock_style = PositionLockStyle (string_2_enum (prop->value(), _positional_lock_style));

		if (_positional_lock_style == MusicTime) {
			if ((prop = node.property ("bbt-position")) == 0) {
				/* missing BBT info, revert to audio time locking */
				_positional_lock_style = AudioTime;
			} else {
				if (sscanf (prop->value().c_str(), "%d|%d|%d", 
					    &_bbt_time.bars,
					    &_bbt_time.beats,
					    &_bbt_time.ticks) != 3) {
					_positional_lock_style = AudioTime;
				}
			}
		}
			
	} else {
		_positional_lock_style = AudioTime;
	}

	/* XXX FIRST EDIT !!! */
	
	/* these 3 properties never change as a result of any editing */

	if ((prop = node.property ("ancestral-start")) != 0) {
		_ancestral_start = atoi (prop->value());
	} else {
		_ancestral_start = _start;
	}

	if ((prop = node.property ("ancestral-length")) != 0) {
		_ancestral_length = atoi (prop->value());
	} else {
		_ancestral_length = _length;
	}

	if ((prop = node.property ("stretch")) != 0) {
		_stretch = atof (prop->value());
	} else {
		_stretch = 1.0;
	}

	if ((prop = node.property ("shift")) != 0) {
		_shift = atof (prop->value());
	} else {
		_shift = 1.0;
	}

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
	_last_length = _length;
	_last_position = _position;
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
	
	if (!(_flags & DoNotSaveState)) {
		
		/* Try and send a shared_pointer unless this is part of the constructor.
		   If so, do nothing.
		*/
		
		try {
			boost::shared_ptr<Region> rptr = shared_from_this();
			RegionPropertyChanged (rptr);
		} catch (...) {
			/* no shared_ptr available, relax; */
		}
	}
	
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
	_sources.clear ();
	drop_references ();
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

void
Region::set_master_sources (const SourceList& srcs)
{
	_master_sources = srcs;
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
Region::verify_length (nframes_t len)
{
	if (source() && (source()->destructive() || source()->length_mutable())) {
		return true;
	}

	nframes_t maxlen = 0;

	for (uint32_t n=0; n < _sources.size(); ++n) {
		maxlen = max (maxlen, _sources[n]->length() - _start);
	}
	
	len = min (len, maxlen);
	
	return true;
}

bool
Region::verify_start_and_length (nframes_t new_start, nframes_t& new_length)
{
	if (source() && (source()->destructive() || source()->length_mutable())) {
		return true;
	}

	nframes_t maxlen = 0;

	for (uint32_t n=0; n < _sources.size(); ++n) {
		maxlen = max (maxlen, _sources[n]->length() - new_start);
	}

	new_length = min (new_length, maxlen);

	return true;
}

bool
Region::verify_start (nframes_t pos)
{
	if (source() && (source()->destructive() || source()->length_mutable())) {
		return true;
	}

	for (uint32_t n=0; n < _sources.size(); ++n) {
		if (pos > _sources[n]->length() - _length) {
			return false;
		}
	}
	return true;
}

bool
Region::verify_start_mutable (nframes_t& new_start)
{
	if (source() && (source()->destructive() || source()->length_mutable())) {
		return true;
	}

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
	boost::shared_ptr<Playlist> pl (playlist());

	if (pl) {
		boost::shared_ptr<Region> r;
		boost::shared_ptr<Region const> grrr2 = boost::dynamic_pointer_cast<Region const> (shared_from_this());
		
		if (grrr2 && (r = pl->session().find_whole_file_parent (grrr2))) {
			return boost::static_pointer_cast<Region> (r);
		}
	}
	
	return boost::shared_ptr<Region>();
}

int
Region::apply (Filter& filter)
{
	return filter.run (shared_from_this());
}


void
Region::invalidate_transients ()
{
	_valid_transients = false;
	_transients.clear ();
}

