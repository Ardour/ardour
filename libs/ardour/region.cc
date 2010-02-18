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


#include <glibmm/thread.h>
#include "pbd/xml++.h"
#include "pbd/stacktrace.h"
#include "pbd/enumwriter.h"

#include "ardour/debug.h"
#include "ardour/region.h"
#include "ardour/playlist.h"
#include "ardour/session.h"
#include "ardour/source.h"
#include "ardour/tempo.h"
#include "ardour/region_factory.h"
#include "ardour/filter.h"
#include "ardour/profile.h"
#include "ardour/utils.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PropertyChange Region::FadeChanged       = PBD::new_change ();
PropertyChange Region::SyncOffsetChanged = PBD::new_change ();
PropertyChange Region::MuteChanged       = PBD::new_change ();
PropertyChange Region::OpacityChanged    = PBD::new_change ();
PropertyChange Region::LockChanged       = PBD::new_change ();
PropertyChange Region::LayerChanged      = PBD::new_change ();
PropertyChange Region::HiddenChanged     = PBD::new_change ();

namespace ARDOUR { 
	namespace Properties {
		PBD::PropertyDescriptor<bool> muted;
		PBD::PropertyDescriptor<bool> opaque;
		PBD::PropertyDescriptor<bool> locked;
		PBD::PropertyDescriptor<bool> automatic;
		PBD::PropertyDescriptor<bool> whole_file;
		PBD::PropertyDescriptor<bool> import;
		PBD::PropertyDescriptor<bool> external;
		PBD::PropertyDescriptor<bool> sync_marked;
		PBD::PropertyDescriptor<bool> left_of_split;
		PBD::PropertyDescriptor<bool> right_of_split;
		PBD::PropertyDescriptor<bool> hidden;
		PBD::PropertyDescriptor<bool> position_locked;
		PBD::PropertyDescriptor<framepos_t> start;
		PBD::PropertyDescriptor<framecnt_t> length;
		PBD::PropertyDescriptor<framepos_t> position;
		PBD::PropertyDescriptor<framecnt_t> sync_position;
		PBD::PropertyDescriptor<layer_t> layer;
		PBD::PropertyDescriptor<framepos_t> ancestral_start;
		PBD::PropertyDescriptor<framecnt_t> ancestral_length;
		PBD::PropertyDescriptor<float> stretch;
		PBD::PropertyDescriptor<float> shift;
	}
}
	
PBD::Signal1<void,boost::shared_ptr<ARDOUR::Region> > Region::RegionPropertyChanged;

void
Region::make_property_quarks ()
{
	Properties::muted.id = g_quark_from_static_string (X_("muted"));
	Properties::opaque.id = g_quark_from_static_string (X_("opaque"));
	Properties::locked.id = g_quark_from_static_string (X_("locked"));
	Properties::automatic.id = g_quark_from_static_string (X_("automatic"));
	Properties::whole_file.id = g_quark_from_static_string (X_("whole-file"));
	Properties::import.id = g_quark_from_static_string (X_("import"));
	Properties::external.id = g_quark_from_static_string (X_("external"));
	Properties::sync_marked.id = g_quark_from_static_string (X_("sync-marked"));
	Properties::left_of_split.id = g_quark_from_static_string (X_("left-of-split"));
	Properties::right_of_split.id = g_quark_from_static_string (X_("right-of-split"));
	Properties::hidden.id = g_quark_from_static_string (X_("hidden"));
	Properties::position_locked.id = g_quark_from_static_string (X_("position-locked"));
	Properties::start.id = g_quark_from_static_string (X_("start"));
	Properties::length.id = g_quark_from_static_string (X_("length"));
	Properties::position.id = g_quark_from_static_string (X_("position"));
	Properties::sync_position.id = g_quark_from_static_string (X_("sync-position"));
	Properties::layer.id = g_quark_from_static_string (X_("layer"));
	Properties::ancestral_start.id = g_quark_from_static_string (X_("ancestral-start"));
	Properties::ancestral_length.id = g_quark_from_static_string (X_("ancestral-length"));
	Properties::stretch.id = g_quark_from_static_string (X_("stretch"));
	Properties::shift.id = g_quark_from_static_string (X_("shift"));
}

void
Region::register_properties ()
{
	_xml_node_name = X_("Region");

	add_property (_muted);
	add_property (_opaque);
	add_property (_locked);
	add_property (_automatic);
	add_property (_whole_file);
	add_property (_import);
	add_property (_external);
	add_property (_sync_marked);
	add_property (_left_of_split);
	add_property (_right_of_split);
	add_property (_hidden);
	add_property (_position_locked);
	add_property (_start);
	add_property (_length);
	add_property (_position);
	add_property (_sync_position);
	add_property (_layer);
	add_property (_ancestral_start);
	add_property (_ancestral_length);
	add_property (_stretch);
	add_property (_shift);
}

#define REGION_DEFAULT_STATE(s,l) \
	_muted (Properties::muted, MuteChanged, false)	     \
	, _opaque (Properties::opaque, OpacityChanged, true) \
	, _locked (Properties::locked, LockChanged, false) \
	, _automatic (Properties::automatic, PropertyChange (0), false) \
	, _whole_file (Properties::whole_file, PropertyChange (0), false) \
	, _import (Properties::import, PropertyChange (0), false) \
	, _external (Properties::external, PropertyChange (0), false) \
	, _sync_marked (Properties::sync_marked, SyncOffsetChanged, false) \
	, _left_of_split (Properties::left_of_split, PropertyChange (0), false) \
	, _right_of_split (Properties::right_of_split, PropertyChange (0), false) \
	, _hidden (Properties::hidden, HiddenChanged, false) \
	, _position_locked (Properties::position_locked, PropertyChange (0), false) \
	, _start (Properties::start, StartChanged, (s))	\
	, _length (Properties::length, LengthChanged, (l))	\
	, _position (Properties::position, PositionChanged, 0) \
	, _sync_position (Properties::sync_position, SyncOffsetChanged, (s)) \
	, _layer (Properties::layer, LayerChanged, 0)	\
	, _ancestral_start (Properties::ancestral_start, PropertyChange (0), (s)) \
	, _ancestral_length (Properties::ancestral_length, PropertyChange (0), (l)) \
	, _stretch (Properties::stretch, PropertyChange (0), 1.0) \
	, _shift (Properties::shift, PropertyChange (0), 1.0)

#define REGION_COPY_STATE(other) \
	  _muted (other->_muted) \
	, _opaque (other->_opaque) \
	, _locked (other->_locked) \
	, _automatic (other->_automatic) \
	, _whole_file (other->_whole_file) \
	, _import (other->_import) \
	, _external (other->_external) \
	, _sync_marked (other->_sync_marked) \
	, _left_of_split (other->_left_of_split) \
	, _right_of_split (other->_right_of_split) \
	, _hidden (other->_hidden) \
	, _position_locked (other->_position_locked) \
	, _start(other->_start) \
	, _length(other->_length) \
	, _position(other->_position) \
	, _sync_position(other->_sync_position) \
        , _layer (other->_layer) \
	, _ancestral_start (other->_ancestral_start) \
	, _ancestral_length (other->_ancestral_length) \
	, _stretch (other->_stretch) \
	, _shift (other->_shift)

/* derived-from-derived constructor (no sources in constructor) */
Region::Region (Session& s, framepos_t start, framecnt_t length, const string& name, DataType type)
	: SessionObject(s, name)
	, _type(type)
	, _no_property_changes (true)
	, REGION_DEFAULT_STATE(start,length)
	, _last_length (length)
	, _last_position (0)
	, _positional_lock_style(AudioTime)
	, _first_edit (EditChangesNothing)
	, _frozen(0)
	, _read_data_count(0)
	, _pending_changed(PropertyChange (0))
	, _last_layer_op(0)
	, _pending_explicit_relayer (false)
{
	register_properties ();

	/* no sources at this point */
}

/** Basic Region constructor (single source) */
Region::Region (boost::shared_ptr<Source> src)
	: SessionObject(src->session(), "toBeRenamed")
	, _type (src->type())
	, _no_property_changes (true)
	, REGION_DEFAULT_STATE(0,0)
	, _last_length (0)
	, _last_position (0)
        , _positional_lock_style (_type == DataType::AUDIO ? AudioTime : MusicTime)
	, _first_edit (EditChangesNothing)
	, _frozen(0)
	, _valid_transients(false)
	, _read_data_count(0)
	, _pending_changed(PropertyChange (0))
	, _last_layer_op(0)
	, _pending_explicit_relayer (false)
{
	register_properties ();

	_sources.push_back (src);
	_master_sources.push_back (src);

	src->DropReferences.connect_same_thread (*this, boost::bind (&Region::source_deleted, this, boost::weak_ptr<Source>(src)));
	
	assert (_sources.size() > 0);
	assert (_type == src->type());
}

/** Basic Region constructor (many sources) */
Region::Region (const SourceList& srcs)
	: SessionObject(srcs.front()->session(), "toBeRenamed")
	, _type (srcs.front()->type())
	, _no_property_changes (true)
	, REGION_DEFAULT_STATE(0,0)
	, _last_length (0)
	, _last_position (0)
        , _positional_lock_style (_type == DataType::AUDIO ? AudioTime : MusicTime)
	, _first_edit (EditChangesNothing)
	, _frozen (0)
	, _valid_transients(false)
	, _read_data_count(0)
	, _pending_changed(PropertyChange (0))
	, _last_layer_op (0)
	, _pending_explicit_relayer (false)
{
	register_properties ();

	_type = srcs.front()->type();

	use_sources (srcs);

	assert(_sources.size() > 0);
	assert (_type == srcs.front()->type());
}

/** Create a new Region from part of an existing one, starting at one of two places:

    if @param offset_relative is true, then the start within @param other is given by @param offset
    (i.e. relative to the start of @param other's sources, the start is @param offset + @param other.start()

    if @param offset_relative is false, then the start within the source is given @param offset.
*/
Region::Region (boost::shared_ptr<const Region> other, frameoffset_t offset, bool offset_relative)
	: SessionObject(other->session(), "toBeRenamed")
	, _type (other->data_type())
	, _no_property_changes (true)
	, REGION_COPY_STATE (other)
	, _last_length (other->_last_length)
	, _last_position(other->_last_position) \
	, _positional_lock_style(other->_positional_lock_style) \
	, _first_edit (EditChangesNothing)
	, _frozen (0)
	, _valid_transients(false)
	, _read_data_count(0)
	, _pending_changed(PropertyChange (0))
	, _last_layer_op (0)
	, _pending_explicit_relayer (false)

{
	register_properties ();

	/* override state that may have been incorrectly inherited from the other region
	 */

	_position = 0;
	_locked = false;
	_whole_file = false;
	_hidden = false;

	use_sources (other->_sources);

	if (!offset_relative) {

		/* not sure why we do this, but its a hangover from ardour before
		   property lists. this would be nice to remove.
		*/

		_positional_lock_style = other->_positional_lock_style;
		_first_edit = other->_first_edit;

		if (offset == 0) {

			_start = 0;

			/* sync pos is relative to start of file. our start-in-file is now zero,
			   so set our sync position to whatever the the difference between
			   _start and _sync_pos was in the other region.
			   
			   result is that our new sync pos points to the same point in our source(s)
			   as the sync in the other region did in its source(s).
			   
			   since we start at zero in our source(s), it is not possible to use a sync point that
			   is before the start. reset it to _start if that was true in the other region.
			*/
			
			if (other->sync_marked()) {
				if (other->_start < other->_sync_position) {
					/* sync pos was after the start point of the other region */
					_sync_position = other->_sync_position - other->_start;
				} else {
					/* sync pos was before the start point of the other region. not possible here. */
					_sync_marked = false;
					_sync_position = _start;
				}
			} else {
				_sync_marked = false;
				_sync_position = _start;
			}
		} else {
			/* XXX do something else ! */
			fatal << string_compose (_("programming error: %1"), X_("Region+offset constructor used with illegal combination of offset+relative"))
			      << endmsg;
			/*NOTREACHED*/
		}

	} else {

		_start = other->_start + offset;
		
		/* if the other region had a distinct sync point
		   set, then continue to use it as best we can.
		   otherwise, reset sync point back to start.
		*/
		
		if (other->sync_marked()) {
			if (other->_sync_position < _start) {
				_sync_marked = false;
				_sync_position = _start;
		} else {
				_sync_position = other->_sync_position;
			}
		} else {
			_sync_marked = false;
			_sync_position = _start;
		}
	}

	if (Profile->get_sae()) {
		/* reset sync point to start if its ended up
		   outside region bounds.
		*/

		if (_sync_position < _start || _sync_position >= _start + _length) {
			_sync_marked = false;
			_sync_position = _start;
		}
	}

	assert (_type == other->data_type());
}

/** Create a copy of @param other but with different sources. Used by filters */
Region::Region (boost::shared_ptr<const Region> other, const SourceList& srcs)
	: SessionObject (other->session(), other->name())
	, _type (srcs.front()->type())
	, _no_property_changes (true)
	, REGION_COPY_STATE (other)
	, _last_length (other->_last_length)
	, _last_position (other->_last_position)
        , _positional_lock_style (other->_positional_lock_style)
	, _first_edit (EditChangesID)
	, _frozen (0)
	, _valid_transients (false)
	, _read_data_count (0)
	, _pending_changed (PropertyChange(0))
	, _last_layer_op (other->_last_layer_op)
	, _pending_explicit_relayer (false)
{
	register_properties ();

	_locked = false;
	_position_locked = false;

	other->_first_edit = EditChangesName;

	if (other->_extra_xml) {
		_extra_xml = new XMLNode (*other->_extra_xml);
	} else {
		_extra_xml = 0;
	}

	use_sources (srcs);
	assert(_sources.size() > 0);
}

/** Simple "copy" constructor */
Region::Region (boost::shared_ptr<const Region> other)
	: SessionObject(other->session(), other->name())
	, _type(other->data_type())
	, _no_property_changes (true)
	, REGION_COPY_STATE (other)
	, _last_length (other->_last_length)
	, _last_position (other->_last_position)
        , _positional_lock_style (other->_positional_lock_style)
	, _first_edit (EditChangesID)
	, _frozen(0)
	, _valid_transients(false)
	, _read_data_count(0)
	, _pending_changed(PropertyChange(0))
	, _last_layer_op(other->_last_layer_op)
	, _pending_explicit_relayer (false)
{
	register_properties ();

	_locked = false;
	_position_locked = false;

	other->_first_edit = EditChangesName;

	if (other->_extra_xml) {
		_extra_xml = new XMLNode (*other->_extra_xml);
	} else {
		_extra_xml = 0;
	}

	use_sources (other->_sources);
	assert(_sources.size() > 0);
}

Region::Region (const SourceList& srcs, const XMLNode& node)
	: SessionObject(srcs.front()->session(), X_("error: XML did not reset this"))
	, _type (srcs.front()->type())
	, REGION_DEFAULT_STATE(0,0)
	, _last_length (0)
	, _last_position (0)
        , _positional_lock_style (_type == DataType::AUDIO ? AudioTime : MusicTime)
	, _first_edit (EditChangesNothing)
	, _frozen(0)
	, _valid_transients(false)
	, _read_data_count(0)
	, _pending_changed(PropertyChange(0))
	, _last_layer_op(0)
	, _pending_explicit_relayer (false)
{
	const XMLProperty* prop;

	register_properties ();

	if ((prop = node.property (X_("id")))) {
		_id = prop->value();
	}

	use_sources (srcs);

	if (set_state (node, Stateful::loading_state_version)) {
		throw failed_constructor();
	}

	assert(_type != DataType::NIL);
	assert(_sources.size() > 0);
	assert (_type == srcs.front()->type());

}

Region::Region (boost::shared_ptr<Source> src, const XMLNode& node)
	: SessionObject(src->session(), X_("error: XML did not reset this"))
	, _type (src->type())
	, REGION_DEFAULT_STATE(0,0)
	, _last_length (0)
	, _last_position (0)
        , _positional_lock_style (_type == DataType::AUDIO ? AudioTime : MusicTime)
	, _first_edit (EditChangesNothing)
	, _frozen (0)
	, _read_data_count (0)
	, _pending_changed (PropertyChange(0))
	, _last_layer_op (0)
	, _pending_explicit_relayer (false)
{
	const XMLProperty *prop;

	register_properties ();

	_sources.push_back (src);

	if ((prop = node.property (X_("id")))) {
		_id = prop->value();
	}

	if (set_state (node, Stateful::loading_state_version)) {
		throw failed_constructor();
	}

	assert (_type != DataType::NIL);
	assert (_sources.size() > 0);
	assert (_type == src->type());
}

Region::~Region ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("Region %1 destructor @ %2\n", _name, this));
}

void
Region::set_playlist (boost::weak_ptr<Playlist> wpl)
{
	_playlist = wpl.lock();
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
Region::set_length (framecnt_t len, void */*src*/)
{
	//cerr << "Region::set_length() len = " << len << endl;
	if (locked()) {
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
		_whole_file = false;
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
	/* this does nothing but marked a semantic moment once upon a time */
}

void
Region::first_edit ()
{
	boost::shared_ptr<Playlist> pl (playlist());

	if (_first_edit != EditChangesNothing && pl) {

		_name = _session.new_region_name (_name);
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
Region::special_set_position (framepos_t pos)
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
		_session.tempo_map().bbt_time (_position, _bbt_time);
	}

}

void
Region::update_position_after_tempo_map_change ()
{
	boost::shared_ptr<Playlist> pl (playlist());

	if (!pl || _positional_lock_style != MusicTime) {
		return;
	}

	TempoMap& map (_session.tempo_map());
	framepos_t pos = map.frame_time (_bbt_time);
	set_position_internal (pos, false);
}

void
Region::set_position (framepos_t pos, void* /*src*/)
{
	if (!can_move()) {
		return;
	}

	set_position_internal (pos, true);
}

void
Region::set_position_internal (framepos_t pos, bool allow_bbt_recompute)
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
Region::set_position_on_top (framepos_t pos, void* /*src*/)
{
	if (locked()) {
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
		_session.tempo_map().bbt_time (_position, _bbt_time);
	}
}

void
Region::nudge_position (frameoffset_t n, void* /*src*/)
{
	if (locked()) {
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
		if (_position < -n) {
			_position = 0;
		} else {
			_position += n;
		}
	}

	send_change (PositionChanged);
}

void
Region::set_ancestral_data (framepos_t s, framecnt_t l, float st, float sh)
{
	_ancestral_length = l;
	_ancestral_start = s;
	_stretch = st;
	_shift = sh;
}

void
Region::set_start (framepos_t pos, void* /*src*/)
{
	if (locked() || position_locked()) {
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
		_whole_file = false;
		first_edit ();
		invalidate_transients ();

		send_change (StartChanged);
	}
}

void
Region::trim_start (framepos_t new_position, void */*src*/)
{
	if (locked() || position_locked()) {
		return;
	}
	framepos_t new_start;
	frameoffset_t start_shift;

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

		if (_start < -start_shift) {
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
	_whole_file = false;
	first_edit ();

	send_change (StartChanged);
}

void
Region::trim_front (framepos_t new_position, void *src)
{
	if (locked()) {
		return;
	}

	framepos_t end = last_frame();
	framepos_t source_zero;

	if (_position > _start) {
		source_zero = _position - _start;
	} else {
		source_zero = 0; // its actually negative, but this will work for us
	}

	if (new_position < end) { /* can't trim it zero or negative length */

		framecnt_t newlen;

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

/** @param new_endpoint New region end point, such that, for example,
 *  a region at 0 of length 10 has an endpoint of 9.
 */

void
Region::trim_end (framepos_t new_endpoint, void */*src*/)
{
	if (locked()) {
		return;
	}

	if (new_endpoint > _position) {
		trim_to_internal (_position, new_endpoint - _position + 1, this);
		if (!_frozen) {
			recompute_at_end ();
		}
	}
}

void
Region::trim_to (framepos_t position, framecnt_t length, void *src)
{
	if (locked()) {
		return;
	}

	trim_to_internal (position, length, src);

	if (!_frozen) {
		recompute_at_start ();
		recompute_at_end ();
	}
}

void
Region::trim_to_internal (framepos_t position, framecnt_t length, void */*src*/)
{
	frameoffset_t start_shift;
	framepos_t new_start;

	if (locked()) {
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

		if (_start < -start_shift) {
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

	PropertyChange what_changed = PropertyChange (0);

	if (_start != new_start) {
		_start = new_start;
		what_changed = PropertyChange (what_changed|StartChanged);
	}
	if (_length != length) {
		if (!_frozen) {
			_last_length = _length;
		}
		_length = length;
		what_changed = PropertyChange (what_changed|LengthChanged);
	}
	if (_position != position) {
		if (!_frozen) {
			_last_position = _position;
		}
		_position = position;
		what_changed = PropertyChange (what_changed|PositionChanged);
	}

	_whole_file = false;

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
		_hidden = yn;
		send_change (HiddenChanged);
	}
}

void
Region::set_whole_file (bool yn)
{
	_whole_file = yn;
	/* no change signal */
}

void
Region::set_automatic (bool yn)
{
	_automatic = yn;
	/* no change signal */
}

void
Region::set_muted (bool yn)
{
	if (muted() != yn) {
		_muted = yn;
		send_change (MuteChanged);
	}
}

void
Region::set_opaque (bool yn)
{
	if (opaque() != yn) {
		_opaque = yn;
		send_change (OpacityChanged);
	}
}

void
Region::set_locked (bool yn)
{
	if (locked() != yn) {
		_locked = yn;
		send_change (LockChanged);
	}
}

void
Region::set_position_locked (bool yn)
{
	if (position_locked() != yn) {
		_position_locked = yn;
		send_change (LockChanged);
	}
}

void
Region::set_sync_position (framepos_t absolute_pos)
{
	framepos_t const file_pos = _start + (absolute_pos - _position);

	if (file_pos != _sync_position) {
		_sync_marked = true;
		_sync_position = file_pos;
		if (!_frozen) {
			maybe_uncopy ();
		}
		send_change (SyncOffsetChanged);
	}
}

void
Region::clear_sync_position ()
{
	if (sync_marked()) {
		_sync_marked = false;
		if (!_frozen) {
			maybe_uncopy ();
		}
		send_change (SyncOffsetChanged);
	}
}

framepos_t
Region::sync_offset (int& dir) const
{
	/* returns the sync point relative the first frame of the region */

	if (sync_marked()) {
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

framepos_t
Region::adjust_to_sync (framepos_t pos) const
{
	int sync_dir;
	frameoffset_t offset = sync_offset (sync_dir);

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

framepos_t
Region::sync_position() const
{
	if (sync_marked()) {
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
Region::state (bool /*full_state*/)
{
	XMLNode *node = new XMLNode ("Region");
	char buf[64];
	const char* fe = NULL;

	add_properties (*node);

	_id.print (buf, sizeof (buf));
	node->add_property ("id", buf);
	node->add_property ("type", _type.to_string());

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

	node->add_property ("first-edit", fe);

	/* note: flags are stored by derived classes */

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
Region::set_state (const XMLNode& node, int version)
{
	PropertyChange what_changed = PropertyChange (0);
	return _set_state (node, version, what_changed, true);
}

int
Region::_set_state (const XMLNode& node, int version, PropertyChange& what_changed, bool send)
{
	const XMLProperty* prop;

	what_changed = set_properties (node);

	if ((prop = node.property (X_("id")))) {
		_id = prop->value();
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

	}

	/* fix problems with old sessions corrupted by impossible
	   values for _stretch or _shift
	*/
	if (_stretch == 0.0f) {
		_stretch = 1.0f;
	}
	
	if (_shift == 0.0f) {
		_shift = 1.0f;
	}

	const XMLNodeList& nlist = node.children();

	for (XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); ++niter) {

		XMLNode *child;

		child = (*niter);

		if (child->name () == "Extra") {
			delete _extra_xml;
			_extra_xml = new XMLNode (*child);
			break;
		}
	}

	if (send) {
		cerr << _name << ": final change to be sent: " << hex << what_changed << dec << endl;
		send_change (what_changed);
	}

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
Region::thaw ()
{
	PropertyChange what_changed = PropertyChange (0);

	{
		Glib::Mutex::Lock lm (_lock);

		if (_frozen && --_frozen > 0) {
			return;
		}

		if (_pending_changed) {
			what_changed = _pending_changed;
			_pending_changed = PropertyChange (0);
		}
	}

	if (what_changed == PropertyChange (0)) {
		return;
	}

	if (what_changed & LengthChanged) {
		if (what_changed & PositionChanged) {
			recompute_at_start ();
		}
		recompute_at_end ();
	}

	send_change (what_changed);
}

void
Region::send_change (PropertyChange what_changed)
{

	{
		Glib::Mutex::Lock lm (_lock);
		if (_frozen) {
			_pending_changed = PropertyChange (_pending_changed|what_changed);
			return;
		}
	}

	cerr << _name << " actually sends " << hex << what_changed << dec << " @" << get_microseconds() << endl;
	StateChanged (what_changed);
	cerr << _name << " done with " << hex << what_changed << dec << " @" << get_microseconds() << endl;

	if (!_no_property_changes) {
		
		/* Try and send a shared_pointer unless this is part of the constructor.
		   If so, do nothing.
		*/

		try {
			boost::shared_ptr<Region> rptr = shared_from_this();
			cerr << _name << " actually sends prop change " << hex << what_changed << dec <<  " @ " << get_microseconds() << endl;
			RegionPropertyChanged (rptr);
			cerr << _name << " done with prop change  @ " << get_microseconds() << endl;

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
Region::source_deleted (boost::weak_ptr<Source>)
{
	_sources.clear ();

	if (!_session.deletion_in_progress()) {
		/* this is a very special case: at least one of the region's
		   sources has bee deleted, so invalidate all references to
		   ourselves. Do NOT do this during session deletion, because
		   then we run the risk that this will actually result
		   in this object being deleted (as refcnt goes to zero)
		   while emitting DropReferences.
		*/

		drop_references ();
	}
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
	assert (_sources.size() == _master_sources.size());
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
Region::uses_source (boost::shared_ptr<const Source> source) const
{
	for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {
		if (*i == source) {
			return true;
		}
	}
	return false;
}

sframes_t
Region::source_length(uint32_t n) const
{
	return _sources[n]->length(_position - _start);
}

bool
Region::verify_length (framecnt_t len)
{
	if (source() && (source()->destructive() || source()->length_mutable())) {
		return true;
	}

	framecnt_t maxlen = 0;

	for (uint32_t n=0; n < _sources.size(); ++n) {
		maxlen = max (maxlen, source_length(n) - _start);
	}

	len = min (len, maxlen);

	return true;
}

bool
Region::verify_start_and_length (framepos_t new_start, framecnt_t& new_length)
{
	if (source() && (source()->destructive() || source()->length_mutable())) {
		return true;
	}

	framecnt_t maxlen = 0;

	for (uint32_t n=0; n < _sources.size(); ++n) {
		maxlen = max (maxlen, source_length(n) - new_start);
	}

	new_length = min (new_length, maxlen);

	return true;
}

bool
Region::verify_start (framepos_t pos)
{
	if (source() && (source()->destructive() || source()->length_mutable())) {
		return true;
	}

	for (uint32_t n=0; n < _sources.size(); ++n) {
		if (pos > source_length(n) - _length) {
			return false;
		}
	}
	return true;
}

bool
Region::verify_start_mutable (framepos_t& new_start)
{
	if (source() && (source()->destructive() || source()->length_mutable())) {
		return true;
	}

	for (uint32_t n=0; n < _sources.size(); ++n) {
		if (new_start > source_length(n) - _length) {
			new_start = source_length(n) - _length;
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

		if (grrr2 && (r = _session.find_whole_file_parent (grrr2))) {
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


void
Region::use_sources (SourceList const & s)
{
	set<boost::shared_ptr<Source> > unique_srcs;

	for (SourceList::const_iterator i = s.begin (); i != s.end(); ++i) {
		_sources.push_back (*i);
		(*i)->DropReferences.connect_same_thread (*this, boost::bind (&Region::source_deleted, this, boost::weak_ptr<Source>(*i)));
		unique_srcs.insert (*i);
	}

	for (SourceList::const_iterator i = s.begin (); i != s.end(); ++i) {
		_master_sources.push_back (*i);
		if (unique_srcs.find (*i) == unique_srcs.end()) {
			(*i)->DropReferences.connect_same_thread (*this, boost::bind (&Region::source_deleted, this, boost::weak_ptr<Source>(*i)));
		}
	}
}


PropertyChange
Region::set_property (const PropertyBase& prop)
{
	PropertyChange c = PropertyChange (0);

	DEBUG_TRACE (DEBUG::Properties,  string_compose ("region %1 set property %2\n", _name.val(), prop.property_name()));

	if (prop == Properties::muted.id) {
		bool val = dynamic_cast<const PropertyTemplate<bool>*>(&prop)->val();
		if (val != _muted) {
			DEBUG_TRACE (DEBUG::Properties, string_compose ("region %1 muted changed from %2 to %3",
									_name.val(), _muted.val(), val));
			_muted = val;
			c = MuteChanged;
		}
	} else if (prop == Properties::opaque.id) {
		bool val = dynamic_cast<const PropertyTemplate<bool>*>(&prop)->val();
		if (val != _opaque) {
			DEBUG_TRACE (DEBUG::Properties, string_compose ("region %1 opaque changed from %2 to %3",
									_name.val(), _opaque.val(), val));
			_opaque = val;
			c = OpacityChanged;
		}
	} else if (prop == Properties::locked.id) {
		bool val = dynamic_cast<const PropertyTemplate<bool>*>(&prop)->val();
		if (val != _locked) {
			DEBUG_TRACE (DEBUG::Properties, string_compose ("region %1 locked changed from %2 to %3",
									_name.val(), _locked.val(), val));
			_locked = val;
			c = LockChanged;
		}
	} else if (prop == Properties::automatic.id) {
		_automatic = dynamic_cast<const PropertyTemplate<bool>*>(&prop)->val();
	} else if (prop == Properties::whole_file.id) {
		_whole_file = dynamic_cast<const PropertyTemplate<bool>*>(&prop)->val();
	} else if (prop == Properties::import.id) {
		_import = dynamic_cast<const PropertyTemplate<bool>*>(&prop)->val();
	} else if (prop == Properties::external.id) {
		_external = dynamic_cast<const PropertyTemplate<bool>*>(&prop)->val();
	} else if (prop == Properties::sync_marked.id) {
		_sync_marked = dynamic_cast<const PropertyTemplate<bool>*>(&prop)->val();
	} else if (prop == Properties::left_of_split.id) {
		_left_of_split = dynamic_cast<const PropertyTemplate<bool>*>(&prop)->val();
	} else if (prop == Properties::right_of_split.id) {
		_right_of_split = dynamic_cast<const PropertyTemplate<bool>*>(&prop)->val();
	} else if (prop == Properties::hidden.id) {
		bool val = dynamic_cast<const PropertyTemplate<bool>*>(&prop)->val();
		if (val != _hidden) {
			_hidden = val;
			c = HiddenChanged;
		}
	} else if (prop == Properties::position_locked.id) {
		_position_locked = dynamic_cast<const PropertyTemplate<bool>*>(&prop)->val();
	} else if (prop == Properties::start.id) {
		_start = dynamic_cast<const PropertyTemplate<framepos_t>*>(&prop)->val();
	} else if (prop == Properties::length.id) {
		const PropertyTemplate<framecnt_t>* pt1 = dynamic_cast<const PropertyTemplate<framecnt_t>* >(&prop);
		const PropertyTemplate<int>* pt2 = dynamic_cast<const PropertyTemplate<int>* >(&prop);
		
		cerr << "Cast to frmecnt = " << pt1 << " to int = " << pt2 << endl;

		framecnt_t val = dynamic_cast<const PropertyTemplate<framecnt_t>* > (&prop)->val();
		if (val != _length) {
			DEBUG_TRACE (DEBUG::Properties, string_compose ("region %1 length changed from %2 to %3",
									_name.val(), _length.val(), val));
			_length = val;
			c = LengthChanged;
		} else {
			DEBUG_TRACE (DEBUG::Properties, string_compose ("length %1 matches %2\n", _length.val(), val));
		}

	} else if (prop == Properties::position.id) {
		framepos_t val = dynamic_cast<const PropertyTemplate<framepos_t>*>(&prop)->val();
		if (val != _position) {
			DEBUG_TRACE (DEBUG::Properties, string_compose ("region %1 position changed from %2 to %3",
									_name.val(), _position.val(), val));
			_position = val;
			c = PositionChanged;
		}
	} else if (prop == Properties::sync_position.id) {
		framepos_t val = dynamic_cast<const PropertyTemplate<framepos_t>*>(&prop)->val();
		if (val != _sync_position) {
			DEBUG_TRACE (DEBUG::Properties, string_compose ("region %1 syncpos changed from %2 to %3",
									_name.val(), _sync_position, val));
			_sync_position = val;
			c = SyncOffsetChanged;
		}
	} else if (prop == Properties::layer.id) {
		layer_t val = dynamic_cast<const PropertyTemplate<layer_t>*>(&prop)->val();
		if (val != _layer) {
			DEBUG_TRACE (DEBUG::Properties, string_compose ("region %1 syncpos changed from %2 to %3",
									_name.val(), _sync_position, val));
			_layer = val;
			c = LayerChanged;
		}
	} else if (prop == Properties::ancestral_start.id) {
		_ancestral_start = dynamic_cast<const PropertyTemplate<framepos_t>*>(&prop)->val();
	} else if (prop == Properties::ancestral_length.id) {
		_ancestral_length = dynamic_cast<const PropertyTemplate<framecnt_t>*>(&prop)->val();
	} else if (prop == Properties::stretch.id) {
		_stretch = dynamic_cast<const PropertyTemplate<float>*>(&prop)->val();
	} else if (prop == Properties::shift.id) {
		_shift = dynamic_cast<const PropertyTemplate<float>*>(&prop)->val();
	} else {
		return SessionObject::set_property (prop);
	}
	
	return c;
}
