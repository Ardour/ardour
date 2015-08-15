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

#include <glibmm/threads.h>
#include "pbd/xml++.h"

#include "ardour/debug.h"
#include "ardour/filter.h"
#include "ardour/playlist.h"
#include "ardour/playlist_source.h"
#include "ardour/profile.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/source.h"
#include "ardour/tempo.h"
#include "ardour/transient_detector.h"
#include "ardour/types_convert.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

namespace ARDOUR {
	class Progress;
	namespace Properties {
		PBD::PropertyDescriptor<bool> muted;
		PBD::PropertyDescriptor<bool> opaque;
		PBD::PropertyDescriptor<bool> locked;
		PBD::PropertyDescriptor<bool> video_locked;
		PBD::PropertyDescriptor<bool> automatic;
		PBD::PropertyDescriptor<bool> whole_file;
		PBD::PropertyDescriptor<bool> import;
		PBD::PropertyDescriptor<bool> external;
		PBD::PropertyDescriptor<bool> sync_marked;
		PBD::PropertyDescriptor<bool> left_of_split;
		PBD::PropertyDescriptor<bool> right_of_split;
		PBD::PropertyDescriptor<bool> hidden;
		PBD::PropertyDescriptor<bool> position_locked;
		PBD::PropertyDescriptor<bool> valid_transients;
		PBD::PropertyDescriptor<framepos_t> start;
		PBD::PropertyDescriptor<framecnt_t> length;
		PBD::PropertyDescriptor<framepos_t> position;
		PBD::PropertyDescriptor<double> beat;
		PBD::PropertyDescriptor<framecnt_t> sync_position;
		PBD::PropertyDescriptor<layer_t> layer;
		PBD::PropertyDescriptor<framepos_t> ancestral_start;
		PBD::PropertyDescriptor<framecnt_t> ancestral_length;
		PBD::PropertyDescriptor<float> stretch;
		PBD::PropertyDescriptor<float> shift;
		PBD::PropertyDescriptor<PositionLockStyle> position_lock_style;
		PBD::PropertyDescriptor<uint64_t> layering_index;
	}
}

PBD::Signal2<void,boost::shared_ptr<ARDOUR::Region>,const PropertyChange&> Region::RegionPropertyChanged;

void
Region::make_property_quarks ()
{
	Properties::muted.property_id = g_quark_from_static_string (X_("muted"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for muted = %1\n",	Properties::muted.property_id));
	Properties::opaque.property_id = g_quark_from_static_string (X_("opaque"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for opaque = %1\n",	Properties::opaque.property_id));
	Properties::locked.property_id = g_quark_from_static_string (X_("locked"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for locked = %1\n",	Properties::locked.property_id));
	Properties::video_locked.property_id = g_quark_from_static_string (X_("video-locked"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for video-locked = %1\n",	Properties::video_locked.property_id));
	Properties::automatic.property_id = g_quark_from_static_string (X_("automatic"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for automatic = %1\n",	Properties::automatic.property_id));
	Properties::whole_file.property_id = g_quark_from_static_string (X_("whole-file"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for whole-file = %1\n",	Properties::whole_file.property_id));
	Properties::import.property_id = g_quark_from_static_string (X_("import"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for import = %1\n",	Properties::import.property_id));
	Properties::external.property_id = g_quark_from_static_string (X_("external"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for external = %1\n",	Properties::external.property_id));
	Properties::sync_marked.property_id = g_quark_from_static_string (X_("sync-marked"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for sync-marked = %1\n",		Properties::sync_marked.property_id));
	Properties::left_of_split.property_id = g_quark_from_static_string (X_("left-of-split"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for left-of-split = %1\n",	Properties::left_of_split.property_id));
	Properties::right_of_split.property_id = g_quark_from_static_string (X_("right-of-split"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for right-of-split = %1\n",	Properties::right_of_split.property_id));
	Properties::hidden.property_id = g_quark_from_static_string (X_("hidden"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for hidden = %1\n",	Properties::hidden.property_id));
	Properties::position_locked.property_id = g_quark_from_static_string (X_("position-locked"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for position-locked = %1\n",	Properties::position_locked.property_id));
	Properties::valid_transients.property_id = g_quark_from_static_string (X_("valid-transients"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for valid-transients = %1\n",	Properties::valid_transients.property_id));
	Properties::start.property_id = g_quark_from_static_string (X_("start"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for start = %1\n",	Properties::start.property_id));
	Properties::length.property_id = g_quark_from_static_string (X_("length"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for length = %1\n",	Properties::length.property_id));
	Properties::position.property_id = g_quark_from_static_string (X_("position"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for position = %1\n",	Properties::position.property_id));
	Properties::beat.property_id = g_quark_from_static_string (X_("beat"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for beat = %1\n",	Properties::beat.property_id));
	Properties::sync_position.property_id = g_quark_from_static_string (X_("sync-position"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for sync-position = %1\n",	Properties::sync_position.property_id));
	Properties::layer.property_id = g_quark_from_static_string (X_("layer"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for layer = %1\n",	Properties::layer.property_id));
	Properties::ancestral_start.property_id = g_quark_from_static_string (X_("ancestral-start"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for ancestral-start = %1\n",	Properties::ancestral_start.property_id));
	Properties::ancestral_length.property_id = g_quark_from_static_string (X_("ancestral-length"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for ancestral-length = %1\n",	Properties::ancestral_length.property_id));
	Properties::stretch.property_id = g_quark_from_static_string (X_("stretch"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for stretch = %1\n",	Properties::stretch.property_id));
	Properties::shift.property_id = g_quark_from_static_string (X_("shift"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for shift = %1\n",	Properties::shift.property_id));
	Properties::position_lock_style.property_id = g_quark_from_static_string (X_("positional-lock-style"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for position_lock_style = %1\n",		Properties::position_lock_style.property_id));
	Properties::layering_index.property_id = g_quark_from_static_string (X_("layering-index"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for layering_index = %1\n",	Properties::layering_index.property_id));
}

void
Region::register_properties ()
{
	_xml_node_name = X_("Region");

	add_property (_muted);
	add_property (_opaque);
	add_property (_locked);
	add_property (_video_locked);
	add_property (_automatic);
	add_property (_whole_file);
	add_property (_import);
	add_property (_external);
	add_property (_sync_marked);
	add_property (_left_of_split);
	add_property (_right_of_split);
	add_property (_hidden);
	add_property (_position_locked);
	add_property (_valid_transients);
	add_property (_start);
	add_property (_length);
	add_property (_position);
	add_property (_beat);
	add_property (_sync_position);
	add_property (_ancestral_start);
	add_property (_ancestral_length);
	add_property (_stretch);
	add_property (_shift);
	add_property (_position_lock_style);
	add_property (_layering_index);
}

#define REGION_DEFAULT_STATE(s,l) \
	_sync_marked (Properties::sync_marked, false) \
	, _left_of_split (Properties::left_of_split, false) \
	, _right_of_split (Properties::right_of_split, false) \
	, _valid_transients (Properties::valid_transients, false) \
	, _start (Properties::start, (s))	\
	, _length (Properties::length, (l))	\
	, _position (Properties::position, 0) \
	, _beat (Properties::beat, 0.0) \
	, _sync_position (Properties::sync_position, (s)) \
	, _quarter_note (0.0) \
	, _transient_user_start (0) \
	, _transient_analysis_start (0) \
	, _transient_analysis_end (0) \
	, _muted (Properties::muted, false) \
	, _opaque (Properties::opaque, true) \
	, _locked (Properties::locked, false) \
  , _video_locked (Properties::video_locked, false) \
	, _automatic (Properties::automatic, false) \
	, _whole_file (Properties::whole_file, false) \
	, _import (Properties::import, false) \
	, _external (Properties::external, false) \
	, _hidden (Properties::hidden, false) \
	, _position_locked (Properties::position_locked, false) \
	, _ancestral_start (Properties::ancestral_start, (s)) \
	, _ancestral_length (Properties::ancestral_length, (l)) \
	, _stretch (Properties::stretch, 1.0) \
	, _shift (Properties::shift, 1.0) \
	, _position_lock_style (Properties::position_lock_style, _type == DataType::AUDIO ? AudioTime : MusicTime) \
	, _layering_index (Properties::layering_index, 0)

#define REGION_COPY_STATE(other) \
	  _sync_marked (Properties::sync_marked, other->_sync_marked) \
	, _left_of_split (Properties::left_of_split, other->_left_of_split) \
	, _right_of_split (Properties::right_of_split, other->_right_of_split) \
	, _valid_transients (Properties::valid_transients, other->_valid_transients) \
	, _start(Properties::start, other->_start)		\
	, _length(Properties::length, other->_length)		\
	, _position(Properties::position, other->_position)	\
	, _beat (Properties::beat, other->_beat)                \
	, _sync_position(Properties::sync_position, other->_sync_position) \
	, _quarter_note (other->_quarter_note)                                \
	, _user_transients (other->_user_transients) \
	, _transient_user_start (other->_transient_user_start) \
	, _transients (other->_transients) \
	, _transient_analysis_start (other->_transient_analysis_start) \
	, _transient_analysis_end (other->_transient_analysis_end) \
	, _muted (Properties::muted, other->_muted)	        \
	, _opaque (Properties::opaque, other->_opaque)		\
	, _locked (Properties::locked, other->_locked)		\
  , _video_locked (Properties::video_locked, other->_video_locked) \
	, _automatic (Properties::automatic, other->_automatic)	\
	, _whole_file (Properties::whole_file, other->_whole_file) \
	, _import (Properties::import, other->_import)		\
	, _external (Properties::external, other->_external)	\
	, _hidden (Properties::hidden, other->_hidden)		\
	, _position_locked (Properties::position_locked, other->_position_locked) \
	, _ancestral_start (Properties::ancestral_start, other->_ancestral_start) \
	, _ancestral_length (Properties::ancestral_length, other->_ancestral_length) \
	, _stretch (Properties::stretch, other->_stretch)	\
	, _shift (Properties::shift, other->_shift)		\
	, _position_lock_style (Properties::position_lock_style, other->_position_lock_style) \
	, _layering_index (Properties::layering_index, other->_layering_index)

/* derived-from-derived constructor (no sources in constructor) */
Region::Region (Session& s, framepos_t start, framecnt_t length, const string& name, DataType type)
	: SessionObject(s, name)
	, _type(type)
	, REGION_DEFAULT_STATE(start,length)
	, _last_length (length)
	, _last_position (0)
	, _first_edit (EditChangesNothing)
	, _layer (0)
{
	register_properties ();

	/* no sources at this point */
}

/** Basic Region constructor (many sources) */
Region::Region (const SourceList& srcs)
	: SessionObject(srcs.front()->session(), "toBeRenamed")
	, _type (srcs.front()->type())
	, REGION_DEFAULT_STATE(0,0)
	, _last_length (0)
	, _last_position (0)
	, _first_edit (EditChangesNothing)
	, _layer (0)
{
	register_properties ();

	_type = srcs.front()->type();

	use_sources (srcs);

	assert(_sources.size() > 0);
	assert (_type == srcs.front()->type());
}

/** Create a new Region from an existing one */
Region::Region (boost::shared_ptr<const Region> other)
	: SessionObject(other->session(), other->name())
	, _type (other->data_type())
	, REGION_COPY_STATE (other)
	, _last_length (other->_last_length)
	, _last_position(other->_last_position) \
	, _first_edit (EditChangesNothing)
	, _layer (other->_layer)
{
	register_properties ();

	/* override state that may have been incorrectly inherited from the other region
	 */

	_position = other->_position;
	_locked = false;
	_whole_file = false;
	_hidden = false;

	use_sources (other->_sources);
	set_master_sources (other->_master_sources);

	_position_lock_style = other->_position_lock_style;
	_first_edit = other->_first_edit;

	_start = other->_start;
	_beat = other->_beat;
	_quarter_note = other->_quarter_note;

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

	assert (_type == other->data_type());
}

/** Create a new Region from part of an existing one.

    the start within \a other is given by \a offset
    (i.e. relative to the start of \a other's sources, the start is \a offset + \a other.start()
*/
Region::Region (boost::shared_ptr<const Region> other, MusicFrame offset)
	: SessionObject(other->session(), other->name())
	, _type (other->data_type())
	, REGION_COPY_STATE (other)
	, _last_length (other->_last_length)
	, _last_position(other->_last_position) \
	, _first_edit (EditChangesNothing)
	, _layer (other->_layer)
{
	register_properties ();

	/* override state that may have been incorrectly inherited from the other region
	 */

	_locked = false;
	_whole_file = false;
	_hidden = false;

	use_sources (other->_sources);
	set_master_sources (other->_master_sources);

	_position = other->_position + offset.frame;
	_start = other->_start + offset.frame;

	/* prevent offset of 0 from altering musical position */
	if (offset.frame != 0) {
		const double offset_qn = _session.tempo_map().exact_qn_at_frame (other->_position + offset.frame, offset.division)
			- other->_quarter_note;

		_quarter_note = other->_quarter_note + offset_qn;
		_beat = _session.tempo_map().beat_at_quarter_note (_quarter_note);
	} else {
		_quarter_note = _session.tempo_map().quarter_note_at_beat (_beat);
	}

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

	assert (_type == other->data_type());
}

/** Create a copy of @param other but with different sources. Used by filters */
Region::Region (boost::shared_ptr<const Region> other, const SourceList& srcs)
	: SessionObject (other->session(), other->name())
	, _type (srcs.front()->type())
	, REGION_COPY_STATE (other)
	, _last_length (other->_last_length)
	, _last_position (other->_last_position)
	, _first_edit (EditChangesID)
	, _layer (other->_layer)
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

Region::~Region ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("Region %1 destructor @ %2\n", _name, this));
        drop_sources ();
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

		send_change (Properties::name);
	}

	return true;
}

void
Region::set_length (framecnt_t len, const int32_t sub_num)
{
	//cerr << "Region::set_length() len = " << len << endl;
	if (locked()) {
		return;
	}

	if (_length != len && len != 0) {

		/* check that the current _position wouldn't make the new
		   length impossible.
		*/

		if (max_framepos - len < _position) {
			return;
		}

		if (!verify_length (len)) {
			return;
		}


		set_length_internal (len, sub_num);
		_whole_file = false;
		first_edit ();
		maybe_uncopy ();
		maybe_invalidate_transients ();

		if (!property_changes_suspended()) {
			recompute_at_end ();
		}

		send_change (Properties::length);
	}
}

void
Region::set_length_internal (framecnt_t len, const int32_t sub_num)
{
	_last_length = _length;
	_length = len;
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

		_name = RegionFactory::new_region_name (_name);
		_first_edit = EditChangesNothing;

		send_change (Properties::name);

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
Region::move_to_natural_position ()
{
	boost::shared_ptr<Playlist> pl (playlist());

	if (!pl) {
		return;
	}

	boost::shared_ptr<Region> whole_file_region = get_parent();

	if (whole_file_region) {
		set_position (whole_file_region->position() + _start);
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
	if (_position_lock_style != ps) {

		boost::shared_ptr<Playlist> pl (playlist());

		_position_lock_style = ps;

		send_change (Properties::position_lock_style);
	}
}

void
Region::update_after_tempo_map_change (bool send)
{
	boost::shared_ptr<Playlist> pl (playlist());

	if (!pl) {
		return;
	}

	if (_position_lock_style == AudioTime) {
		/* don't signal as the actual position has not chnged */
		recompute_position_from_lock_style (0);
		return;
	}

	/* prevent movement before 0 */
	const framepos_t pos = max ((framepos_t) 0, _session.tempo_map().frame_at_beat (_beat));
	/* we have _beat. update frame position non-musically */
	set_position_internal (pos, false, 0);

	/* do this even if the position is the same. this helps out
	   a GUI that has moved its representation already.
	*/

	if (send) {
		send_change (Properties::position);
	}
}

void
Region::set_position (framepos_t pos, int32_t sub_num)
{
	if (!can_move()) {
		return;
	}

	/* do this even if the position is the same. this helps out
	   a GUI that has moved its representation already.
	*/
	PropertyChange p_and_l;

	p_and_l.add (Properties::position);

	if (position_lock_style() == AudioTime) {
		set_position_internal (pos, true, sub_num);
	} else {
		if (!_session.loading()) {
			_beat = _session.tempo_map().exact_beat_at_frame (pos, sub_num);
			_quarter_note = _session.tempo_map().quarter_note_at_beat (_beat);
		}

		set_position_internal (pos, false, sub_num);
	}

	if (position_lock_style() == MusicTime) {
		p_and_l.add (Properties::length);
	}

	send_change (p_and_l);

}

void
Region::set_position_internal (framepos_t pos, bool allow_bbt_recompute, const int32_t sub_num)
{
	/* We emit a change of Properties::position even if the position hasn't changed
	   (see Region::set_position), so we must always set this up so that
	   e.g. Playlist::notify_region_moved doesn't use an out-of-date last_position.
	*/
	_last_position = _position;

	if (_position != pos) {
		_position = pos;

		if (allow_bbt_recompute) {
			recompute_position_from_lock_style (sub_num);
		} else {
			/* MusicTime dictates that we glue to ardour beats. the pulse may have changed.*/
			_quarter_note = _session.tempo_map().quarter_note_at_beat (_beat);
		}

		/* check that the new _position wouldn't make the current
		   length impossible - if so, change the length.

		   XXX is this the right thing to do?
		*/
		if (max_framepos - _length < _position) {
			_last_length = _length;
			_length = max_framepos - _position;
		}
	}
}

void
Region::set_position_music (double qn)
{
	if (!can_move()) {
		return;
	}

	/* do this even if the position is the same. this helps out
	   a GUI that has moved its representation already.
	*/
	PropertyChange p_and_l;

	p_and_l.add (Properties::position);

	if (!_session.loading()) {
		_beat = _session.tempo_map().beat_at_quarter_note (qn);
	}

	/* will set frame accordingly */
	set_position_music_internal (qn);

	if (position_lock_style() == MusicTime) {
		p_and_l.add (Properties::length);
	}

	send_change (p_and_l);
}

void
Region::set_position_music_internal (double qn)
{
	/* We emit a change of Properties::position even if the position hasn't changed
	   (see Region::set_position), so we must always set this up so that
	   e.g. Playlist::notify_region_moved doesn't use an out-of-date last_position.
	*/
	_last_position = _position;

	if (_quarter_note != qn) {
		_position = _session.tempo_map().frame_at_quarter_note (qn);
		_quarter_note = qn;

		/* check that the new _position wouldn't make the current
		   length impossible - if so, change the length.

		   XXX is this the right thing to do?
		*/
		if (max_framepos - _length < _position) {
			_last_length = _length;
			_length = max_framepos - _position;
		}
	}
}

/** A gui may need to create a region, then place it in an initial
 *  position determined by the user.
 *  When this takes place within one gui operation, we have to reset
 *  _last_position to prevent an implied move.
 */
void
Region::set_initial_position (framepos_t pos)
{
	if (!can_move()) {
		return;
	}

	if (_position != pos) {
		_position = pos;

		/* check that the new _position wouldn't make the current
		   length impossible - if so, change the length.

		   XXX is this the right thing to do?
		*/

		if (max_framepos - _length < _position) {
			_last_length = _length;
			_length = max_framepos - _position;
		}

		recompute_position_from_lock_style (0);
		/* ensure that this move doesn't cause a range move */
		_last_position = _position;
	}


	/* do this even if the position is the same. this helps out
	   a GUI that has moved its representation already.
	*/
	send_change (Properties::position);
}

void
Region::recompute_position_from_lock_style (const int32_t sub_num)
{
	_beat = _session.tempo_map().exact_beat_at_frame (_position, sub_num);
	_quarter_note = _session.tempo_map().exact_qn_at_frame (_position, sub_num);
}

void
Region::nudge_position (frameoffset_t n)
{
	if (locked() || video_locked()) {
		return;
	}

	if (n == 0) {
		return;
	}

	framepos_t new_position = _position;

	if (n > 0) {
		if (_position > max_framepos - n) {
			new_position = max_framepos;
		} else {
			new_position += n;
		}
	} else {
		if (_position < -n) {
			new_position = 0;
		} else {
			new_position += n;
		}
	}
	/* assumes non-musical nudge */
	set_position_internal (new_position, true, 0);

	send_change (Properties::position);
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
Region::set_start (framepos_t pos)
{
	if (locked() || position_locked() || video_locked()) {
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

		set_start_internal (pos);
		_whole_file = false;
		first_edit ();
		maybe_invalidate_transients ();

		send_change (Properties::start);
	}
}

void
Region::move_start (frameoffset_t distance, const int32_t sub_num)
{
	if (locked() || position_locked() || video_locked()) {
		return;
	}

	framepos_t new_start;

	if (distance > 0) {

		if (_start > max_framepos - distance) {
			new_start = max_framepos; // makes no sense
		} else {
			new_start = _start + distance;
		}

		if (!verify_start (new_start)) {
			return;
		}

	} else if (distance < 0) {

		if (_start < -distance) {
			new_start = 0;
		} else {
			new_start = _start + distance;
		}

	} else {
		return;
	}

	if (new_start == _start) {
		return;
	}

	set_start_internal (new_start, sub_num);

	_whole_file = false;
	first_edit ();

	send_change (Properties::start);
}

void
Region::trim_front (framepos_t new_position, const int32_t sub_num)
{
	modify_front (new_position, false, sub_num);
}

void
Region::cut_front (framepos_t new_position, const int32_t sub_num)
{
	modify_front (new_position, true, sub_num);
}

void
Region::cut_end (framepos_t new_endpoint, const int32_t sub_num)
{
	modify_end (new_endpoint, true, sub_num);
}

void
Region::modify_front (framepos_t new_position, bool reset_fade, const int32_t sub_num)
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

		framecnt_t newlen = 0;

		if (!can_trim_start_before_source_start ()) {
			/* can't trim it back past where source position zero is located */
			new_position = max (new_position, source_zero);
		}

		if (new_position > _position) {
			newlen = _length - (new_position - _position);
		} else {
			newlen = _length + (_position - new_position);
		}

		trim_to_internal (new_position, newlen, sub_num);

		if (reset_fade) {
			_right_of_split = true;
		}

		if (!property_changes_suspended()) {
			recompute_at_start ();
		}

		maybe_invalidate_transients ();
	}
}

void
Region::modify_end (framepos_t new_endpoint, bool reset_fade, const int32_t sub_num)
{
	if (locked()) {
		return;
	}

	if (new_endpoint > _position) {
		trim_to_internal (_position, new_endpoint - _position, sub_num);
		if (reset_fade) {
			_left_of_split = true;
		}
		if (!property_changes_suspended()) {
			recompute_at_end ();
		}
	}
}

/** @param new_endpoint New region end point, such that, for example,
 *  a region at 0 of length 10 has an endpoint of 9.
 */

void
Region::trim_end (framepos_t new_endpoint, const int32_t sub_num)
{
	modify_end (new_endpoint, false, sub_num);
}

void
Region::trim_to (framepos_t position, framecnt_t length, const int32_t sub_num)
{
	if (locked()) {
		return;
	}

	trim_to_internal (position, length, sub_num);

	if (!property_changes_suspended()) {
		recompute_at_start ();
		recompute_at_end ();
	}
}

void
Region::trim_to_internal (framepos_t position, framecnt_t length, const int32_t sub_num)
{
	framepos_t new_start;

	if (locked()) {
		return;
	}

	frameoffset_t const start_shift = position - _position;

	if (start_shift > 0) {

		if (_start > max_framepos - start_shift) {
			new_start = max_framepos;
		} else {
			new_start = _start + start_shift;
		}

	} else if (start_shift < 0) {

		if (_start < -start_shift && !can_trim_start_before_source_start ()) {
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

	PropertyChange what_changed;

	if (_start != new_start) {
		set_start_internal (new_start, sub_num);
		what_changed.add (Properties::start);
	}


	/* Set position before length, otherwise for MIDI regions this bad thing happens:
	 * 1. we call set_length_internal; length in beats is computed using the region's current
	 *    (soon-to-be old) position
	 * 2. we call set_position_internal; position is set and length in frames re-computed using
	 *    length in beats from (1) but at the new position, which is wrong if the region
	 *    straddles a tempo/meter change.
	 */

	if (_position != position) {
		if (!property_changes_suspended()) {
			_last_position = _position;
		}
		set_position_internal (position, true, sub_num);
		what_changed.add (Properties::position);
	}

	if (_length != length) {
		if (!property_changes_suspended()) {
			_last_length = _length;
		}
		set_length_internal (length, sub_num);
		what_changed.add (Properties::length);
	}

	_whole_file = false;

	PropertyChange start_and_length;

	start_and_length.add (Properties::start);
	start_and_length.add (Properties::length);

	if (what_changed.contains (start_and_length)) {
		first_edit ();
	}

	if (!what_changed.empty()) {
		send_change (what_changed);
	}
}

void
Region::set_hidden (bool yn)
{
	if (hidden() != yn) {
		_hidden = yn;
		send_change (Properties::hidden);
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
		send_change (Properties::muted);
	}
}

void
Region::set_opaque (bool yn)
{
	if (opaque() != yn) {
		_opaque = yn;
		send_change (Properties::opaque);
	}
}

void
Region::set_locked (bool yn)
{
	if (locked() != yn) {
		_locked = yn;
		send_change (Properties::locked);
	}
}

void
Region::set_video_locked (bool yn)
{
	if (video_locked() != yn) {
		_video_locked = yn;
		send_change (Properties::video_locked);
	}
}

void
Region::set_position_locked (bool yn)
{
	if (position_locked() != yn) {
		_position_locked = yn;
		send_change (Properties::locked);
	}
}

/** Set the region's sync point.
 *  @param absolute_pos Session time.
 */
void
Region::set_sync_position (framepos_t absolute_pos)
{
	/* position within our file */
	framepos_t const file_pos = _start + (absolute_pos - _position);

	if (file_pos != _sync_position) {
		_sync_marked = true;
		_sync_position = file_pos;
		if (!property_changes_suspended()) {
			maybe_uncopy ();
		}

		send_change (Properties::sync_position);
	}
}

void
Region::clear_sync_position ()
{
	if (sync_marked()) {
		_sync_marked = false;
		if (!property_changes_suspended()) {
			maybe_uncopy ();
		}

		send_change (Properties::sync_position);
	}
}

/* @return the sync point relative the first frame of the region */
frameoffset_t
Region::sync_offset (int& dir) const
{
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
		if (max_framepos - pos > offset) {
			pos += offset;
		}
	}

	return pos;
}

/** @return Sync position in session time */
framepos_t
Region::sync_position() const
{
	if (sync_marked()) {
		return _position - _start + _sync_position;
	} else {
		/* if sync has not been marked, use the start of the region */
		return _position;
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
	_layer = l;
}

XMLNode&
Region::state ()
{
	XMLNode *node = new XMLNode ("Region");
	char buf2[64];
	LocaleGuard lg;
	const char* fe = NULL;

	/* custom version of 'add_properties (*node);'
	 * skip values that have have dedicated save functions
	 * in AudioRegion::state()
	 */
	for (OwnedPropertyList::iterator i = _properties->begin(); i != _properties->end(); ++i) {
		if (!strcmp(i->second->property_name(), (const char*)"Envelope")) continue;
		if (!strcmp(i->second->property_name(), (const char*)"FadeIn")) continue;
		if (!strcmp(i->second->property_name(), (const char*)"FadeOut")) continue;
		if (!strcmp(i->second->property_name(), (const char*)"InverseFadeIn")) continue;
		if (!strcmp(i->second->property_name(), (const char*)"InverseFadeOut")) continue;
		i->second->get_value (*node);
	}

	node->add_property ("id", id ().to_s ());
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

	for (uint32_t n=0; n < _sources.size(); ++n) {
		snprintf (buf2, sizeof(buf2), "source-%d", n);
		node->add_property (buf2, _sources[n]->id().to_s ());
	}

	for (uint32_t n=0; n < _master_sources.size(); ++n) {
		snprintf (buf2, sizeof(buf2), "master-source-%d", n);
		node->add_property (buf2, _master_sources[n]->id ().to_s ());
	}

	/* Only store nested sources for the whole-file region that acts
	   as the parent/root of all regions using it.
	*/

	if (_whole_file && max_source_level() > 0) {

		XMLNode* nested_node = new XMLNode (X_("NestedSource"));

		/* region is compound - get its playlist and
		   store that before we list the region that
		   needs it ...
		*/

		for (SourceList::const_iterator s = _sources.begin(); s != _sources.end(); ++s) {
			nested_node->add_child_nocopy ((*s)->get_state ());
		}

		if (nested_node) {
			node->add_child_nocopy (*nested_node);
		}
	}

	if (_extra_xml) {
		node->add_child_copy (*_extra_xml);
	}

	return *node;
}

XMLNode&
Region::get_state ()
{
	return state ();
}

int
Region::set_state (const XMLNode& node, int version)
{
	PropertyChange what_changed;
	return _set_state (node, version, what_changed, true);
}

int
Region::_set_state (const XMLNode& node, int /*version*/, PropertyChange& what_changed, bool send)
{
	XMLProperty const * prop;
	Timecode::BBT_Time bbt_time;

	Stateful::save_extra_xml (node);

	what_changed = set_values (node);

	set_id (node);

	if (_position_lock_style == MusicTime) {
		if ((prop = node.property ("bbt-position")) != 0) {
			if (sscanf (prop->value().c_str(), "%d|%d|%d",
				    &bbt_time.bars,
				    &bbt_time.beats,
				    &bbt_time.ticks) != 3) {
				_position_lock_style = AudioTime;
				_beat = _session.tempo_map().beat_at_frame (_position);
			} else {
				_beat = _session.tempo_map().beat_at_bbt (bbt_time);
			}
			/* no position property change for legacy Property, so we do this here */
			_quarter_note = _session.tempo_map().quarter_note_at_beat (_beat);
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

	if (send) {
		send_change (what_changed);
	}

	/* Quick fix for 2.x sessions when region is muted */
	if ((prop = node.property (X_("flags")))) {
		if (string::npos != prop->value().find("Muted")){
			set_muted (true);
		}
	}

	// saved property is invalid, region-transients are not saved
	if (_user_transients.size() == 0){
		_valid_transients = false;
	}

	return 0;
}

void
Region::suspend_property_changes ()
{
	Stateful::suspend_property_changes ();
	_last_length = _length;
	_last_position = _position;
}

void
Region::mid_thaw (const PropertyChange& what_changed)
{
	if (what_changed.contains (Properties::length)) {
		if (what_changed.contains (Properties::position)) {
			recompute_at_start ();
		}
		recompute_at_end ();
	}
}

void
Region::send_change (const PropertyChange& what_changed)
{
	if (what_changed.empty()) {
		return;
	}

	Stateful::send_change (what_changed);

	if (!Stateful::property_changes_suspended()) {

		/* Try and send a shared_pointer unless this is part of the constructor.
		   If so, do nothing.
		*/

		try {
			boost::shared_ptr<Region> rptr = shared_from_this();
			RegionPropertyChanged (rptr, what_changed);
		} catch (...) {
			/* no shared_ptr available, relax; */
		}
	}
}

bool
Region::overlap_equivalent (boost::shared_ptr<const Region> other) const
{
	return coverage (other->first_frame(), other->last_frame()) != Evoral::OverlapNone;
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
	drop_sources ();

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
	for (SourceList::const_iterator i = _master_sources.begin (); i != _master_sources.end(); ++i) {
		(*i)->dec_use_count ();
	}

	_master_sources = srcs;
	assert (_sources.size() == _master_sources.size());

	for (SourceList::const_iterator i = _master_sources.begin (); i != _master_sources.end(); ++i) {
		(*i)->inc_use_count ();
	}
}

bool
Region::source_equivalent (boost::shared_ptr<const Region> other) const
{
	if (!other)
		return false;

	if ((_sources.size() != other->_sources.size()) ||
	    (_master_sources.size() != other->_master_sources.size())) {
		return false;
	}

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
Region::any_source_equivalent (boost::shared_ptr<const Region> other) const
{
	if (!other) {
		return false;
	}

	SourceList::const_iterator i;
	SourceList::const_iterator io;

	for (i = _sources.begin(), io = other->_sources.begin(); i != _sources.end() && io != other->_sources.end(); ++i, ++io) {
		if ((*i)->id() == (*io)->id()) {
			return true;
		}
	}

	return false;
}

std::string
Region::source_string () const
{
	//string res = itos(_sources.size());

	stringstream res;
	res << _sources.size() << ":";

	SourceList::const_iterator i;

	for (i = _sources.begin(); i != _sources.end(); ++i) {
		res << (*i)->id() << ":";
	}

	for (i = _master_sources.begin(); i != _master_sources.end(); ++i) {
		res << (*i)->id() << ":";
	}

	return res.str();
}

void
Region::deep_sources (std::set<boost::shared_ptr<Source> > & sources) const
{
	for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {

		boost::shared_ptr<PlaylistSource> ps = boost::dynamic_pointer_cast<PlaylistSource> (*i);

		if (ps) {
			if (sources.find (ps) == sources.end()) {
				/* (Playlist)Source not currently in
				   accumulating set, so recurse.
				*/
				ps->playlist()->deep_sources (sources);
			}
		}

		/* add this source */
		sources.insert (*i);
	}

	for (SourceList::const_iterator i = _master_sources.begin(); i != _master_sources.end(); ++i) {

		boost::shared_ptr<PlaylistSource> ps = boost::dynamic_pointer_cast<PlaylistSource> (*i);

		if (ps) {
			if (sources.find (ps) == sources.end()) {
				/* (Playlist)Source not currently in
				   accumulating set, so recurse.
				*/
				ps->playlist()->deep_sources (sources);
			}
		}

		/* add this source */
		sources.insert (*i);
	}
}

bool
Region::uses_source (boost::shared_ptr<const Source> source, bool shallow) const
{
	for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {
		if (*i == source) {
			return true;
		}

		if (!shallow) {
			boost::shared_ptr<PlaylistSource> ps = boost::dynamic_pointer_cast<PlaylistSource> (*i);

			if (ps) {
				if (ps->playlist()->uses_source (source)) {
					return true;
				}
			}
		}
	}

	for (SourceList::const_iterator i = _master_sources.begin(); i != _master_sources.end(); ++i) {
		if (*i == source) {
			return true;
		}

		if (!shallow) {
			boost::shared_ptr<PlaylistSource> ps = boost::dynamic_pointer_cast<PlaylistSource> (*i);

			if (ps) {
				if (ps->playlist()->uses_source (source)) {
					return true;
				}
			}
		}
	}

	return false;
}


framecnt_t
Region::source_length(uint32_t n) const
{
	assert (n < _sources.size());
	return _sources[n]->length (_position - _start);
}

bool
Region::verify_length (framecnt_t& len)
{
	if (source() && (source()->destructive() || source()->length_mutable())) {
		return true;
	}

	framecnt_t maxlen = 0;

	for (uint32_t n = 0; n < _sources.size(); ++n) {
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

	for (uint32_t n = 0; n < _sources.size(); ++n) {
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

	for (uint32_t n = 0; n < _sources.size(); ++n) {
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

	for (uint32_t n = 0; n < _sources.size(); ++n) {
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
Region::apply (Filter& filter, Progress* progress)
{
	return filter.run (shared_from_this(), progress);
}


void
Region::maybe_invalidate_transients ()
{
	bool changed = !_onsets.empty();
	_onsets.clear ();

	if (_valid_transients || changed) {
		send_change (PropertyChange (Properties::valid_transients));
		return;
	}
}

void
Region::transients (AnalysisFeatureList& afl)
{
	int cnt = afl.empty() ? 0 : 1;

	Region::merge_features (afl, _onsets, _position);
	Region::merge_features (afl, _user_transients, _position + _transient_user_start - _start);
	if (!_onsets.empty ()) {
		++cnt;
	}
	if (!_user_transients.empty ()) {
		++cnt;
	}
	if (cnt > 1 ) {
		afl.sort ();
		// remove exact duplicates
		TransientDetector::cleanup_transients (afl, _session.frame_rate(), 0);
	}
}

bool
Region::has_transients () const
{
	if (!_user_transients.empty ()) {
		assert (_valid_transients);
		return true;
	}
	if (!_onsets.empty ()) {
		return true;
	}
	return false;
}

void
Region::merge_features (AnalysisFeatureList& result, const AnalysisFeatureList& src, const frameoffset_t off) const
{
	for (AnalysisFeatureList::const_iterator x = src.begin(); x != src.end(); ++x) {
		const frameoffset_t p = (*x) + off;
		if (p < first_frame() || p > last_frame()) {
			continue;
		}
		result.push_back (p);
	}
}

void
Region::drop_sources ()
{
	for (SourceList::const_iterator i = _sources.begin (); i != _sources.end(); ++i) {
		(*i)->dec_use_count ();
	}

	_sources.clear ();

	for (SourceList::const_iterator i = _master_sources.begin (); i != _master_sources.end(); ++i) {
		(*i)->dec_use_count ();
	}

	_master_sources.clear ();
}

void
Region::use_sources (SourceList const & s)
{
	set<boost::shared_ptr<Source> > unique_srcs;

	for (SourceList::const_iterator i = s.begin (); i != s.end(); ++i) {

		_sources.push_back (*i);
		(*i)->inc_use_count ();
		_master_sources.push_back (*i);
		(*i)->inc_use_count ();

		/* connect only once to DropReferences, even if sources are replicated
		 */

		if (unique_srcs.find (*i) == unique_srcs.end ()) {
			unique_srcs.insert (*i);
			(*i)->DropReferences.connect_same_thread (*this, boost::bind (&Region::source_deleted, this, boost::weak_ptr<Source>(*i)));
		}
	}
}

Trimmable::CanTrim
Region::can_trim () const
{
	CanTrim ct = CanTrim (0);

	if (locked()) {
		return ct;
	}

	/* if not locked, we can always move the front later, and the end earlier
	 */

	ct = CanTrim (ct | FrontTrimLater | EndTrimEarlier);

	if (start() != 0 || can_trim_start_before_source_start ()) {
		ct = CanTrim (ct | FrontTrimEarlier);
	}

	if (!_sources.empty()) {
		if ((start() + length()) < _sources.front()->length (0)) {
			ct = CanTrim (ct | EndTrimLater);
		}
	}

	return ct;
}

uint32_t
Region::max_source_level () const
{
	uint32_t lvl = 0;

	for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {
		lvl = max (lvl, (*i)->level());
	}

	return lvl;
}

bool
Region::is_compound () const
{
	return max_source_level() > 0;
}

void
Region::post_set (const PropertyChange& pc)
{
	_quarter_note = _session.tempo_map().quarter_note_at_beat (_beat);
}

void
Region::set_start_internal (framecnt_t s, const int32_t sub_num)
{
	_start = s;
}

framepos_t
Region::earliest_possible_position () const
{
	if (_start > _position) {
		return 0;
	} else {
		return _position - _start;
	}
}

framecnt_t
Region::latest_possible_frame () const
{
	framecnt_t minlen = max_framecnt;

	for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {
		/* non-audio regions have a length that may vary based on their
		 * position, so we have to pass it in the call.
		 */
		minlen = min (minlen, (*i)->length (_position));
	}

	/* the latest possible last frame is determined by the current
	 * position, plus the shortest source extent past _start.
	 */

	return _position + (minlen - _start) - 1;
}
