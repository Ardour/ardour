/*
 * Copyright (C) 2000-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2015-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <iostream>
#include <cmath>
#include <climits>
#include <algorithm>
#include <sstream>

#include <glibmm/threads.h>

#include "pbd/types_convert.h"
#include "pbd/xml++.h"

#include "ardour/audioregion.h"
#include "ardour/debug.h"
#include "ardour/filter.h"
#include "ardour/lua_api.h"
#include "ardour/playlist.h"
#include "ardour/playlist_source.h"
#include "ardour/profile.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/region_fx_plugin.h"
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
		PBD::PropertyDescriptor<timepos_t> start;
		PBD::PropertyDescriptor<timecnt_t> length;
		PBD::PropertyDescriptor<double> beat;
		PBD::PropertyDescriptor<timepos_t> sync_position;
		PBD::PropertyDescriptor<layer_t> layer;
		PBD::PropertyDescriptor<timepos_t> ancestral_start;
		PBD::PropertyDescriptor<timecnt_t> ancestral_length;
		PBD::PropertyDescriptor<float> stretch;
		PBD::PropertyDescriptor<float> shift;
		PBD::PropertyDescriptor<uint64_t> layering_index;
		PBD::PropertyDescriptor<std::string> tags;
		PBD::PropertyDescriptor<uint64_t> reg_group;
		PBD::PropertyDescriptor<bool> contents;
		PBD::PropertyDescriptor<bool> region_fx;

/* these properties are used as a convenience for announcing changes to state, but aren't stored as properties */
		PBD::PropertyDescriptor<Temporal::TimeDomain> time_domain;

	}
}

PBD::Signal2<void,std::shared_ptr<ARDOUR::RegionList>,const PropertyChange&> Region::RegionsPropertyChanged;

/* these static values are used by Region Groups to assign a group-id across the scope of an operation that might span many function calls */
uint64_t Region::_retained_group_id = 0;
uint64_t Region::_retained_take_cnt = 0;
uint64_t Region::_next_group_id     = 0;

std::map<uint64_t, uint64_t> Region::_operation_rgroup_map;
Glib::Threads::Mutex         Region::_operation_rgroup_mutex;

/* access the group-id for an operation on a region, honoring the existing region's group status */
uint64_t
Region::get_region_operation_group_id (uint64_t old_region_group, RegionOperationFlag flags) {
	/* if the region was ungrouped, stay ungrouped */
	if ((old_region_group == NoGroup) || (old_region_group == Explicit)) {
		return old_region_group;
	}

	/* separate and preserve the Explicit flag: */
	bool expl = (old_region_group & Explicit) == Explicit;

	/* remove all flags */
	old_region_group = (old_region_group >> 4) << 4;

	/* apply flags to create a key, which will be used to recognize regions that belong in the same group */
	uint64_t region_group_key = old_region_group | flags;

	/* since the GUI is single-threaded, and it's hard/impossible to edit
	 * during a rec-stop, this 'should' never be contentious
	 */
	Glib::Threads::Mutex::Lock lm (_operation_rgroup_mutex);

	/* if a region group has not been assigned for this key, assign one */
	if (_operation_rgroup_map.find (region_group_key) == _operation_rgroup_map.end ()) {
		_operation_rgroup_map[region_group_key] = _next_group_id++;
	}

	return ((_operation_rgroup_map[region_group_key] << 4) | expl);
}

void
Region::make_property_quarks ()
{
	Properties::muted.property_id = g_quark_from_static_string (X_("muted"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for muted = %1\n", Properties::muted.property_id));
	Properties::opaque.property_id = g_quark_from_static_string (X_("opaque"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for opaque = %1\n", Properties::opaque.property_id));
	Properties::locked.property_id = g_quark_from_static_string (X_("locked"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for locked = %1\n", Properties::locked.property_id));
	Properties::video_locked.property_id = g_quark_from_static_string (X_("video-locked"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for video-locked = %1\n", Properties::video_locked.property_id));
	Properties::automatic.property_id = g_quark_from_static_string (X_("automatic"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for automatic = %1\n", Properties::automatic.property_id));
	Properties::whole_file.property_id = g_quark_from_static_string (X_("whole-file"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for whole-file = %1\n", Properties::whole_file.property_id));
	Properties::import.property_id = g_quark_from_static_string (X_("import"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for import = %1\n", Properties::import.property_id));
	Properties::external.property_id = g_quark_from_static_string (X_("external"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for external = %1\n", Properties::external.property_id));
	Properties::sync_marked.property_id = g_quark_from_static_string (X_("sync-marked"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for sync-marked = %1\n", Properties::sync_marked.property_id));
	Properties::left_of_split.property_id = g_quark_from_static_string (X_("left-of-split"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for left-of-split = %1\n", Properties::left_of_split.property_id));
	Properties::right_of_split.property_id = g_quark_from_static_string (X_("right-of-split"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for right-of-split = %1\n", Properties::right_of_split.property_id));
	Properties::hidden.property_id = g_quark_from_static_string (X_("hidden"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for hidden = %1\n", Properties::hidden.property_id));
	Properties::position_locked.property_id = g_quark_from_static_string (X_("position-locked"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for position-locked = %1\n", Properties::position_locked.property_id));
	Properties::valid_transients.property_id = g_quark_from_static_string (X_("valid-transients"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for valid-transients = %1\n", Properties::valid_transients.property_id));
	Properties::start.property_id = g_quark_from_static_string (X_("start"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for start = %1\n", Properties::start.property_id));
	Properties::length.property_id = g_quark_from_static_string (X_("length"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for length = %1\n", Properties::length.property_id));
	Properties::beat.property_id = g_quark_from_static_string (X_("beat"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for beat = %1\n", Properties::beat.property_id));
	Properties::sync_position.property_id = g_quark_from_static_string (X_("sync-position"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for sync-position = %1\n", Properties::sync_position.property_id));
	Properties::layer.property_id = g_quark_from_static_string (X_("layer"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for layer = %1\n", Properties::layer.property_id));
	Properties::ancestral_start.property_id = g_quark_from_static_string (X_("ancestral-start"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for ancestral-start = %1\n", Properties::ancestral_start.property_id));
	Properties::ancestral_length.property_id = g_quark_from_static_string (X_("ancestral-length"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for ancestral-length = %1\n", Properties::ancestral_length.property_id));
	Properties::stretch.property_id = g_quark_from_static_string (X_("stretch"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for stretch = %1\n", Properties::stretch.property_id));
	Properties::shift.property_id = g_quark_from_static_string (X_("shift"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for shift = %1\n", Properties::shift.property_id));
	Properties::layering_index.property_id = g_quark_from_static_string (X_("layering-index"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for layering_index = %1\n",	Properties::layering_index.property_id));
	Properties::tags.property_id = g_quark_from_static_string (X_("tags"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for tags = %1\n",	Properties::tags.property_id));
	Properties::contents.property_id = g_quark_from_static_string (X_("contents"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for contents = %1\n",	Properties::contents.property_id));
	Properties::region_fx.property_id = g_quark_from_static_string (X_("region-fx"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for region-fx = %1\n",	Properties::region_fx.property_id));
	Properties::time_domain.property_id = g_quark_from_static_string (X_("time_domain"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for time_domain = %1\n",	Properties::time_domain.property_id));
	Properties::reg_group.property_id = g_quark_from_static_string (X_("rgroup"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for region_group = %1\n", Properties::reg_group.property_id));
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
	add_property (_sync_position);
	add_property (_ancestral_start);
	add_property (_ancestral_length);
	add_property (_stretch);
	add_property (_shift);
	add_property (_layering_index);
	add_property (_tags);
	add_property (_reg_group);
	add_property (_contents);
}

#define REGION_DEFAULT_STATE(s,l) \
	_sync_marked (Properties::sync_marked, false) \
	, _left_of_split (Properties::left_of_split, false) \
	, _right_of_split (Properties::right_of_split, false) \
	, _valid_transients (Properties::valid_transients, false) \
	, _start (Properties::start, (s)) \
	, _length (Properties::length, (l)) \
	, _sync_position (Properties::sync_position, (s)) \
	, _transient_user_start (0) \
	, _transient_analysis_start (0) \
	, _transient_analysis_end (0) \
	, _soloSelected (false) \
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
	, _layering_index (Properties::layering_index, 0) \
	, _tags (Properties::tags, "") \
	, _reg_group (Properties::reg_group, 0) \
	, _contents (Properties::contents, false)

#define REGION_COPY_STATE(other) \
	  _sync_marked (Properties::sync_marked, other->_sync_marked) \
	, _left_of_split (Properties::left_of_split, other->_left_of_split) \
	, _right_of_split (Properties::right_of_split, other->_right_of_split) \
	, _valid_transients (Properties::valid_transients, other->_valid_transients) \
	, _start(Properties::start, other->_start) \
	, _length(Properties::length, other->_length) \
	, _sync_position(Properties::sync_position, other->_sync_position) \
	, _user_transients (other->_user_transients) \
	, _transient_user_start (other->_transient_user_start) \
	, _transients (other->_transients) \
	, _transient_analysis_start (other->_transient_analysis_start) \
	, _transient_analysis_end (other->_transient_analysis_end) \
	, _soloSelected (false) \
	, _muted (Properties::muted, other->_muted) \
	, _opaque (Properties::opaque, other->_opaque) \
	, _locked (Properties::locked, other->_locked) \
	, _video_locked (Properties::video_locked, other->_video_locked) \
	, _automatic (Properties::automatic, other->_automatic) \
	, _whole_file (Properties::whole_file, other->_whole_file) \
	, _import (Properties::import, other->_import) \
	, _external (Properties::external, other->_external) \
	, _hidden (Properties::hidden, other->_hidden) \
	, _position_locked (Properties::position_locked, other->_position_locked) \
	, _ancestral_start (Properties::ancestral_start, other->_ancestral_start) \
	, _ancestral_length (Properties::ancestral_length, other->_ancestral_length) \
	, _stretch (Properties::stretch, other->_stretch) \
	, _shift (Properties::shift, other->_shift) \
	, _layering_index (Properties::layering_index, other->_layering_index) \
	, _tags (Properties::tags, other->_tags) \
	, _reg_group (Properties::reg_group, other->_reg_group) \
	, _contents (Properties::contents, other->_contents)

/* derived-from-derived constructor (no sources in constructor) */
Region::Region (Session& s, timepos_t const & start, timecnt_t const & length, const string& name, DataType type)
	: SessionObject(s, name)
	, _type (type)
	, _fx_latency (0)
	, REGION_DEFAULT_STATE (start,length)
	, _last_length (length)
	, _first_edit (EditChangesNothing)
	, _layer (0)
	, _changemap (0)
{
	register_properties ();

	/* no sources at this point */
}

/** Basic Region constructor (many sources) */
Region::Region (const SourceList& srcs)
	: SessionObject(srcs.front()->session(), "toBeRenamed")
	, _type (srcs.front()->type())
	, _fx_latency (0)
	, REGION_DEFAULT_STATE(_type == DataType::MIDI ? timepos_t (Temporal::Beats()) : timepos_t::from_superclock (0),
	                       _type == DataType::MIDI ? timecnt_t (Temporal::Beats()) : timecnt_t::from_superclock (0))
	, _last_length (_type == DataType::MIDI ? timecnt_t (Temporal::Beats()) : timecnt_t::from_superclock (0))
	, _first_edit (EditChangesNothing)
	, _layer (0)
	, _changemap (0)
{
	register_properties ();

	_type = srcs.front()->type();

	use_sources (srcs);

	assert(_sources.size() > 0);
	assert (_type == srcs.front()->type());
}

/** Create a new Region from an existing one */
Region::Region (std::shared_ptr<const Region> other)
	: SessionObject(other->session(), other->name())
	, _type (other->data_type())
	, _fx_latency (0)
	, REGION_COPY_STATE (other)
	, _last_length (other->_last_length)
	, _first_edit (EditChangesNothing)
	, _layer (other->_layer)
	, _changemap (other->_changemap)
{
	register_properties ();

	/* override state that may have been incorrectly inherited from the other region
	 */

	_locked = false;
	_whole_file = false;
	_hidden = false;

	use_sources (other->_sources);
	set_master_sources (other->_master_sources);

	_first_edit = other->_first_edit;

	_start = other->_start;

	/* sync pos is relative to start of file. our start-in-file is now zero,
	 * so set our sync position to whatever the the difference between
	 * _start and _sync_pos was in the other region.
	 *
	 * result is that our new sync pos points to the same point in our source(s)
	 * as the sync in the other region did in its source(s).
	 *
	 * since we start at zero in our source(s), it is not possible to use a sync point that
	 * is before the start. reset it to _start if that was true in the other region.
	 */

	if (other->sync_marked()) {
		if (other->_start < other->_sync_position) {
			/* sync pos was after the start point of the other region */
			_sync_position = timepos_t (other->start().distance (other->_sync_position));
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
 *
 * the start within \a other is given by \a offset
 * (i.e. relative to the start of \a other's sources, the start is \a offset + \a other.start()
 */
Region::Region (std::shared_ptr<const Region> other, timecnt_t const & offset)
	: SessionObject(other->session(), other->name())
	, _type (other->data_type())
	, _fx_latency (0)
	, REGION_COPY_STATE (other)
	, _last_length (other->_last_length)
	, _first_edit (EditChangesNothing)
	, _layer (other->_layer)
	, _changemap (other->_changemap)
{
	register_properties ();

	/* override state that may have been incorrectly inherited from the other region
	 */

	_locked = false;
	_whole_file = false;
	_hidden = false;

	use_sources (other->_sources);
	set_master_sources (other->_master_sources);

	_length = timecnt_t (_length.val().distance(), other->position() + offset);
	_start = other->_start.val() + offset;

	/* if the other region had a distinct sync point
	 * set, then continue to use it as best we can.
	 * otherwise, reset sync point back to start.
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

/** Create a copy of @p other but with different sources. Used by filters */
Region::Region (std::shared_ptr<const Region> other, const SourceList& srcs)
	: SessionObject (other->session(), other->name())
	, _type (srcs.front()->type())
	, _fx_latency (0)
	, REGION_COPY_STATE (other)
	, _last_length (other->_last_length)
	, _first_edit (EditChangesID)
	, _layer (other->_layer)
	, _changemap (other->_changemap)
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
Region::set_playlist (std::weak_ptr<Playlist> wpl)
{
	_playlist = wpl.lock();
}

bool
Region::set_name (const std::string& str)
{
	if (_name == str) {
		return true;
	}

	SessionObject::set_name (str); // EMIT SIGNAL NameChanged()
	assert (_name == str);
	send_change (Properties::name);
	return true;
}

void
Region::set_selected_for_solo(bool yn)
{
	if (_soloSelected != yn) {

		std::shared_ptr<Playlist> pl (playlist());
		if (pl){
			if (yn) {
				pl->AddToSoloSelectedList(this);
			} else {
				pl->RemoveFromSoloSelectedList(this);
			}
		}

		_soloSelected = yn;
	}
}

void
Region::set_length (timecnt_t const & len)
{
	if (locked()) {
		return;
	}
	if (_length == len) {
		return;
	}

	set_length_unchecked (len);
}

void
Region::set_length_unchecked (timecnt_t const & len)
{
	if (len.is_zero ()) {
		return;
	}

	/* check that the current _position wouldn't make the new
	 * length impossible.
	 */

	if (timepos_t::max (len.time_domain()).earlier (len) < position()) {
		return;
	}

	timecnt_t l = len;

	if (!verify_length (l)) {
		return;
	}

	set_length_internal (l);
	_whole_file = false;
	first_edit ();
	maybe_uncopy ();
	maybe_invalidate_transients ();

	if (!property_changes_suspended()) {
		recompute_at_end ();
	}

	send_change (Properties::length);
}

void
Region::set_length_internal (timecnt_t const & len)
{
	/* maintain position value of both _last_length and _length.
	 *
	 * This is very important: set_length() can only be used to the length
	 * component of _length, and set_position() can only be used to set the
	 * position component.
	 */

	_last_length = timecnt_t (_length.val().distance(), _last_length.position());

	std::shared_ptr<Playlist> pl (playlist());

	if (pl) {
		Temporal::TimeDomain td (pl->time_domain());

		/* Note: timecnt_t::time_domain() returns the domain for the
		 * length component, *not* the position.
		 */

		if (td != len.time_domain()) {
			timecnt_t l = _length.val();
			l.set_time_domain (td);
			_length = l;
			return;
		}
	}
	/* either no playlist or time domain for distance is not changing */

	_length = timecnt_t (len.distance(), _length.val().position());
}

void
Region::maybe_uncopy ()
{
	/* this does nothing but marked a semantic moment once upon a time */
}

void
Region::first_edit ()
{
	std::shared_ptr<Playlist> pl (playlist());

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
	std::shared_ptr<Playlist> pl (playlist());

	if (!pl) {
		return false;
	}

	std::shared_ptr<Region> whole_file_region = get_parent();

	if (whole_file_region) {
		if (position() == whole_file_region->position() + _start) {
			return true;
		}
	}

	return false;
}

void
Region::move_to_natural_position ()
{
	std::shared_ptr<Playlist> pl (playlist());

	if (!pl) {
		return;
	}

	std::shared_ptr<Region> whole_file_region = get_parent();

	if (whole_file_region) {
		set_position (whole_file_region->position() + _start);
	}
}

void
Region::special_set_position (timepos_t const & pos)
{
	/* this is used when creating a whole file region as
	 * a way to store its "natural" or "captured" position.
	 */

	_length = timecnt_t (_length.val().distance(), pos);
}

void
Region::set_position_time_domain (Temporal::TimeDomain td)
{
	if (position_time_domain() == td) {
		return;
	}

	/* _length is a property so we cannot directly call
	 * ::set_time_domain() on it. Create a temporary timecnt_t,
	 * change it's time domain, and then assign to _length.
	 *
	 * The region's duration (distance) time-domain must not change (!)
	 *
	 * An Audio region's duration must always be given in samples,
	 * and a MIDI region's duration in beats.
	 * (Beat granularity is coarser than samples. If an Audio-region's duration
	 * is specified in beats, the disk-reader can try to read more samples than
	 * are present in the source. This causes various follow up bugs.
	 *
	 * This can change in the future:
	 * - When events inside a MIDI region can use Audio-time, a MIDI region's duration must also be specified in in audio-time.
	 * - When Ardour support time-strech of Audio regions at disk-reader level, Audio regions can be stretched to match music-time.
	 */
	timepos_t p (position ());
	p.set_time_domain (td);

	timecnt_t t (length ().distance (), p);
	_length = t;

	/* for a while, we allowed the time domain of _length to not match the
	 * region time domain. This ought to be prevented as of August 7th 2023
	 * or earlier, but let's not abort() out if asked to load sessions
	 * where this happened. Note that for reasons described in the previous
	 * comment, this could still cause issues during reading from disk.
	 */

	if  (_length.val().time_domain () != time_domain ()) {
		_length.non_const_val().set_time_domain (time_domain());
#ifndef NDEBUG
		std::cerr << "Fixed up a" << (time_domain() == Temporal::AudioTime ? "n audio" : " music") << "-timed region called " << name() << std::endl;
#endif
	}

	assert (_length.val().time_domain () == time_domain ());

	send_change (Properties::time_domain);
}

void
Region::recompute_position_from_time_domain ()
{
	/* XXX currently do nothing, but if we wanted to reduce lazy evaluation
	 * of timepos_t non-canonical values, we could possibly do it here.
	 */
}

void
Region::update_after_tempo_map_change (bool send)
{
	std::shared_ptr<Playlist> pl (playlist());

	if (!pl) {
		return;
	}

	/* a region using AudioTime is never going to move after a tempo map
	 * change
	 */

	if (_length.val().time_domain() == Temporal::AudioTime && position_time_domain () == Temporal::AudioTime) {
		return;
	}

	if (!send) {
		return;
	}

	PropertyChange what_changed;

	/* any or none of these may have changed due to a tempo map change,
	   but we have no way to establish which have changed and which have
	   not. So we have to mention all 3 to be certain that listeners pay
	   attention. We can't verify because we have no cache of our old
	   start/length/position values in the audio domain, so we can't
	   compare the new values in the audio domain. The beat domain values
	   haven't changed (just the tempo map that connects beat and audio
	   time)
	*/

	what_changed.add (Properties::start);
	what_changed.add (Properties::length);

	/* do this even if the position is the same. this helps out
	 * a GUI that has moved its representation already.
	 */

	send_change (what_changed);
}

void
Region::set_position (timepos_t const & pos)
{
	if (!can_move()) {
		return;
	}
	set_position_unchecked (pos);
}

void
Region::set_position_unchecked (timepos_t const & pos)
{
	set_position_internal (pos);

	/* do this even if the position is the same. this helps out
	 * a GUI that has moved its representation already.
	 */
	send_change (Properties::length);

}

void
Region::set_position_internal (timepos_t const & pos)
{
	if (position() == pos) {
		return;
	}

	/* We emit a change of Properties::length even if the position hasn't changed
	 * (see Region::set_position), so we must always set this up so that
	 * e.g. Playlist::notify_region_moved doesn't use an out-of-date last_position.
	 *
	 * maintain length value of both _last_length and _length.
	 *
	 * This is very important: set_length() can only be used to the length
	 * component of _length, and set_position() can only be used to set the
	 * position component.
	 */

	_last_length.set_position (position());

	std::shared_ptr<Playlist> pl (playlist());

	if (pl) {
		Temporal::TimeDomain td (pl->time_domain());

		/* Note: timecnt_t::time_domain() returns the domain for the
		 * length component, *not* the position.
		 */

		if (td != _length.val().position().time_domain()) {
			timecnt_t l = _length.val();
			l.set_position (pos);
			l.set_time_domain (td);
			_length = l;
		} else {
			/* time domain of position not changing */
			_length = timecnt_t (_length.val().distance(), pos);
		}

	} else {
		/* no playlist, so time domain is free to change */
		_length = timecnt_t (_length.val().distance(), pos);
	}


	/* check that the new _position wouldn't make the current
	 * length impossible - if so, change the length.
	 *
	 * XXX is this the right thing to do?
	 */
	if (timepos_t::max (_length.val().time_domain()).earlier (_length) < position()) {
		_last_length = _length;
		_length = position().distance (timepos_t::max (position().time_domain()));
	}
}

/** A gui may need to create a region, then place it in an initial
 * position determined by the user.
 * When this takes place within one gui operation, we have to reset
 * _last_position to prevent an implied move.
 */
void
Region::set_initial_position (timepos_t const & pos)
{
	if (!can_move()) {
		return;
	}

	if (position() != pos) {

		_length = timecnt_t (_length.val().distance(), pos);

		/* check that the new _position wouldn't make the current
		 * length impossible - if so, change the length.
		 *
		 * XXX is this the right thing to do?
		 */

		if (timepos_t::max (_length.val().time_domain()).earlier (_length) < position()) {
			_last_length = _length;
			_length = position().distance (timepos_t::max (position().time_domain()));
		}

		recompute_position_from_time_domain ();
		/* ensure that this move doesn't cause a range move */
		_last_length.set_position (position());
	}


	/* do this even if the position is the same. this helps out
	 * a GUI that has moved its representation already.
	 */
	send_change (Properties::length);
}

void
Region::nudge_position (timecnt_t const & n)
{
	if (locked() || video_locked()) {
		return;
	}

	if (n.is_zero()) {
		return;
	}

	timepos_t new_position = position();

	if (n.is_positive()) {
		if (position() > timepos_t::max (n.time_domain()).earlier (n)) {
			new_position = timepos_t::max (n.time_domain());
		} else {
			new_position += n;
		}
	} else {
		if (position() < -n) {
			new_position = timepos_t (position().time_domain());
		} else {
			new_position += n;
		}
	}

	/* assumes non-musical nudge */
	set_position_internal (new_position);

	send_change (Properties::length);
}

void
Region::set_ancestral_data (timepos_t const & s, timecnt_t const & l, float st, float sh)
{
	_ancestral_length = l;
	_ancestral_start = s;
	_stretch = st;
	_shift = sh;
}

void
Region::set_start (timepos_t const & pos)
{
	if (locked() || position_locked() || video_locked()) {
		return;
	}
	/* This just sets the start, nothing else. It effectively shifts
	 * the contents of the Region within the overall extent of the Source,
	 * without changing the Region's position or length
	 */

	if (_start != pos) {

		timepos_t p = pos;

		if (!verify_start (p)) {
			return;
		}

		set_start_internal (p);
		_whole_file = false;
		first_edit ();
		maybe_invalidate_transients ();

		send_change (Properties::start);
	}
}

void
Region::move_start (timecnt_t const & distance)
{
	if (locked() || position_locked() || video_locked()) {
		return;
	}

	timepos_t new_start (_start);
	timepos_t current_start (_start);

	if (distance.is_positive()) {

		if (current_start > timepos_t::max (current_start.time_domain()).earlier (distance)) {
			new_start = timecnt_t::max(current_start.time_domain()); // makes no sense
		} else {
			new_start = current_start + distance;
		}

		if (!verify_start (new_start)) {
			return;
		}

	} else {

		if (current_start < -distance) {
			new_start = timecnt_t (current_start.time_domain());
		} else {
			new_start = current_start + distance;
		}
	}

	if (new_start == _start) {
		return;
	}

	set_start_internal (new_start);

	_whole_file = false;
	first_edit ();

	send_change (Properties::start);
}

void
Region::trim_front (timepos_t const & new_position)
{
	if (locked()) {
		return;
	}
	modify_front_unchecked (new_position, false);
}

void
Region::cut_front (timepos_t const & new_position)
{
	if (locked()) {
		return;
	}
	modify_front_unchecked (new_position, true);
}

void
Region::cut_end (timepos_t const & new_endpoint)
{
	if (locked()) {
		return;
	}
	modify_end_unchecked (new_endpoint, true);
}


void
Region::modify_front_unchecked (timepos_t const & npos, bool reset_fade)
{
	timepos_t last = nt_last();
	timepos_t source_zero;
	timepos_t new_position (npos);

	new_position.set_time_domain (time_domain());

	if (position() > start()) {
		source_zero = source_position ();
	} else {
		source_zero = timepos_t (source_position().time_domain()); // its actually negative, but this will work for us
	}

	if (new_position < last) { /* can't trim it zero or negative length */

		timecnt_t newlen (_length);
		timepos_t np = new_position;

		if (!can_trim_start_before_source_start ()) {
			/* can't trim it back past where source position zero is located */
			np = max (np, source_zero);
		}

		if (np > position()) {
			newlen = length() - (position().distance (np));
		} else {
			newlen = length() + (np.distance (position()));
		}

		trim_to_internal (np, newlen);

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
Region::modify_end_unchecked (timepos_t const & new_endpoint, bool reset_fade)
{
	if (new_endpoint > position()) {
		trim_to_internal (position(), position().distance (new_endpoint));
		if (reset_fade) {
			_left_of_split = true;
		}
		if (!property_changes_suspended()) {
			recompute_at_end ();
		}
	}
}

/** @param new_endpoint New region end point, such that, for example,
 * a region at 0 of length 10 has an endpoint of 9.
 */
void
Region::trim_end (timepos_t const & new_endpoint)
{
	if (locked()) {
		return;
	}
	modify_end_unchecked (new_endpoint, false);
}

void
Region::trim_to (timepos_t const & position, timecnt_t const & length)
{
	if (locked()) {
		return;
	}

	trim_to_internal (position, length);

	if (!property_changes_suspended()) {
		recompute_at_start ();
		recompute_at_end ();
	}
}

void
Region::trim_to_internal (timepos_t const & npos, timecnt_t const & nlen)
{
	timepos_t pos (npos);
	pos.set_time_domain (time_domain());
	timecnt_t len (nlen);
	len.set_time_domain (time_domain());

	timepos_t new_start (time_domain());
	timecnt_t const start_shift = position().distance (pos);

	if (start_shift.is_positive()) {

		if (start() > timecnt_t::max() - start_shift) {
			new_start = timepos_t::max (start().time_domain());
		} else {
			new_start = start() + start_shift;
		}

	} else if (start_shift.is_negative()) {

		if (start() < -start_shift && !can_trim_start_before_source_start ()) {
			new_start = timecnt_t (start().time_domain());
		} else {
			new_start = start() + start_shift;
		}

	} else {
		new_start = start();
	}

	timepos_t ns = new_start;
	timecnt_t nl = len;

	if (!verify_start_and_length (ns, nl)) {
		return;
	}

	PropertyChange what_changed;

	if (start() != ns) {
		set_start_internal (ns);
		what_changed.add (Properties::start);
	}

	/* Set position before length, otherwise for MIDI regions this bad thing happens:
	 * 1. we call set_length_internal; len in beats is computed using the region's current
	 *    (soon-to-be old) position
	 * 2. we call set_position_internal; position is set and length in samples re-computed using
	 *    length in beats from (1) but at the new position, which is wrong if the region
	 *    straddles a tempo/meter change.
	 */

	if (position() != pos) {
		if (!property_changes_suspended()) {
			_last_length.set_position (position());
		}
		set_position_internal (pos);
		what_changed.add (Properties::length);
	}

	if (length() != nl) {
		if (!property_changes_suspended()) {
			_last_length = _length;
		}
		set_length_internal (nl);
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
Region::set_sync_position (timepos_t const & absolute_pos)
{
	/* position within our file */
	const timepos_t file_pos = start() + position().distance (absolute_pos);

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

/* @return the sync point relative the position of the region */
timecnt_t
Region::sync_offset (int& dir) const
{
	if (sync_marked()) {
		if (_sync_position > _start) {
			dir = 1;
			return start().distance (_sync_position);
		} else {
			dir = -1;
			return sync_position().distance (start());
		}
	} else {
		dir = 0;
		return timecnt_t::zero (start().time_domain());
	}
}

timepos_t
Region::adjust_to_sync (timepos_t const & pos) const
{
	int sync_dir;
	timepos_t p = pos;
	timecnt_t offset = sync_offset (sync_dir);

	// cerr << "adjusting pos = " << pos << " to sync at " << _sync_position << " offset = " << offset << " with dir = " << sync_dir << endl;

	if (sync_dir > 0) {
		if (pos > offset) {
			p.shift_earlier (offset);
		} else {
			p = timepos_t (p.time_domain());
		}
	} else {
		if (timepos_t::max (p.time_domain()).earlier (timecnt_t (p, p)) > offset) {
			p += offset;
		}
	}

	return p;
}

/** @return Sync position in session time */
timepos_t
Region::sync_position() const
{
	if (sync_marked()) {
		return source_position() + _sync_position;
	} else {
		/* if sync has not been marked, use the start of the region */
		return position();
	}
}

void
Region::raise ()
{
	std::shared_ptr<Playlist> pl (playlist());
	if (pl) {
		pl->raise_region (shared_from_this ());
	}
}

void
Region::lower ()
{
	std::shared_ptr<Playlist> pl (playlist());
	if (pl) {
		pl->lower_region (shared_from_this ());
	}
}


void
Region::raise_to_top ()
{
	std::shared_ptr<Playlist> pl (playlist());
	if (pl) {
		pl->raise_region_to_top (shared_from_this());
	}
}

void
Region::lower_to_bottom ()
{
	std::shared_ptr<Playlist> pl (playlist());
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
Region::state () const
{
	XMLNode *node = new XMLNode ("Region");
	char buf2[64];

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

	node->set_property ("id", id ());
	node->set_property ("type", _type);

	std::string fe;

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

	node->set_property ("first-edit", fe);

	/* note: flags are stored by derived classes */

	for (uint32_t n=0; n < _sources.size(); ++n) {
		snprintf (buf2, sizeof(buf2), "source-%d", n);
		node->set_property (buf2, _sources[n]->id());
	}

	for (uint32_t n=0; n < _master_sources.size(); ++n) {
		snprintf (buf2, sizeof(buf2), "master-source-%d", n);
		node->set_property (buf2, _master_sources[n]->id ());
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

	{
		Glib::Threads::RWLock::ReaderLock lm (_fx_lock);
		for (auto const & p : _plugins) {
			node->add_child_nocopy (p->get_state ());
		}
	}

	return *node;
}

XMLNode&
Region::get_state () const
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
Region::_set_state (const XMLNode& node, int version, PropertyChange& what_changed, bool send)
{
	Temporal::BBT_Time bbt_time;

	Stateful::save_extra_xml (node);

	what_changed = set_values (node);

	if (version < 7000) {

		/* Older versions saved position and length as separate XML
		 * node properties.
		 */

		samplepos_t p;
		samplepos_t l;

		if (node.get_property (X_("position"), p) && node.get_property (X_("length"), l)) {
			_length = timecnt_t (l, timepos_t (p));
		}

		std::string lock_style;
		if (node.get_property (X_("positional-lock-style"), lock_style)) {
			if (lock_style == "MusicTime") {
				double beat, start_beats, length_beats;
				if (node.get_property (X_("beat"), beat) && node.get_property (X_("length-beats"), length_beats)) {
					_length = timecnt_t (Temporal::Beats::from_double (length_beats), timepos_t (Temporal::Beats::from_double (beat)));
				}
				if (node.get_property (X_("start-beats"), start_beats)) {
					_start = timepos_t (Temporal::Beats::from_double (start_beats));
				}
			}
		}
	}

	/* Regions derived from "Destructive/Tape" mode tracks in earlier
	 * versions will have their length set to an extremely large value
	 * (essentially the maximum possible length of a file). Detect this
	 * here and reset to the actual source length (using the first source
	 * as a proxy for all of them). For "previously destructive" sources,
	 * this will correspond to the full extent of the data actually written
	 * to the file (though this may include blank space if discontiguous
	 * punches/capture passes were carried out.
	 */

	if (!_sources.empty() && _type == DataType::AUDIO) {
		/* both region and source length must be audio time for this to
		   actually be a case of a destructive track/region. And also
		   for the operator>() in the 3rd conditional clause to be
		   legal, since these values are timepos_t IS-A int62_t and
		   that requires the same "flagged" status (i.e. domain) to be
		   match.
		*/
		if ((length().time_domain() == Temporal::AudioTime) && (_sources.front()->length().time_domain() == Temporal::AudioTime) && (length().distance() > _sources.front()->length())) {
			std::cerr << "Region " << _name << " has length " << _length.val().str() << " which is longer than its (first?) source's length of " << _sources.front()->length().str() << std::endl;
			throw failed_constructor();
			// _length = timecnt_t (start().distance (_sources.front()->length()), _length.val().position());
		}
	}

	set_id (node);

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
	std::string flags;
	if (node.get_property (X_("flags"), flags)) {
		if (string::npos != flags.find("Muted")){
			set_muted (true);
		}
	}

	// saved property is invalid, region-transients are not saved
	if (_user_transients.size() == 0){
		_valid_transients = false;
	}

	{
		Glib::Threads::RWLock::WriterLock lm (_fx_lock);
		bool changed = !_plugins.empty ();

		_plugins.clear ();

		for (auto const& child : node.children ()) {
			if (child->name() == X_("RegionFXPlugin")) {
				std::shared_ptr<RegionFxPlugin> rfx (new RegionFxPlugin (_session, time_domain ()));
				rfx->set_state (*child, version);
				if (!_add_plugin (rfx, std::shared_ptr<RegionFxPlugin>(), true)) {
					continue;
				}
				_plugins.push_back (rfx);
				changed = true;
			}
		}
		if (changed) {
			fx_latency_changed (true);
			send_change (PropertyChange (Properties::region_fx)); // trigger DiskReader overwrite
			RegionFxChanged (); /* EMIT SIGNAL */
		}
	}

	return 0;
}

PropertyList
Region::derive_properties (bool with_times, bool with_envelope) const
{
	PropertyList plist (properties ());
	plist.remove (Properties::automatic);
	plist.remove (Properties::sync_marked);
	plist.remove (Properties::left_of_split);
	plist.remove (Properties::valid_transients);
	plist.remove (Properties::whole_file);
	if (!with_envelope) {
		plist.remove (Properties::envelope);
	}
	if (!with_times) {
		plist.remove (Properties::start);
		plist.remove (Properties::length);
	}
	return plist;
}

void
Region::suspend_property_changes ()
{
	Stateful::suspend_property_changes ();
	_last_length = _length;
}

void
Region::mid_thaw (const PropertyChange& what_changed)
{
	if (what_changed.contains (Properties::length)) {
		if (length().position() != last_position()) {
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
		 * If so, do nothing.
		 */

		try {
			std::shared_ptr<Region> rptr = shared_from_this();
			if (_changemap) {
				(*_changemap)[what_changed].push_back (rptr);
			} else {
				std::shared_ptr<RegionList> rl (new RegionList);
				rl->push_back (rptr);
				RegionsPropertyChanged (rl, what_changed);
			}
		} catch (...) {
			/* no shared_ptr available, relax; */
		}
	}
}

bool
Region::overlap_equivalent (std::shared_ptr<const Region> other) const
{
	return coverage (other->position(), other->nt_last()) != Temporal::OverlapNone;
}

bool
Region::enclosed_equivalent (std::shared_ptr<const Region> other) const
{
	return ((position() >= other->position() && end() <= other->end()) ||
	        (position() <= other->position() && end() >= other->end()));
}

bool
Region::layer_and_time_equivalent (std::shared_ptr<const Region> other) const
{
	return _layer == other->_layer &&
		position() == other->position() &&
		_length == other->_length;
}

bool
Region::exact_equivalent (std::shared_ptr<const Region> other) const
{
	return _start == other->_start &&
		position() == other->position() &&
		_length == other->_length;
}

bool
Region::size_equivalent (std::shared_ptr<const Region> other) const
{
	return _start == other->_start &&
		_length == other->_length;
}

void
Region::source_deleted (std::weak_ptr<Source>)
{
	if (_source_deleted.fetch_add (1)) {
		return;
	}
	drop_sources ();

	if (!_session.deletion_in_progress()) {
		/* this is a very special case: at least one of the region's
		 * sources has been deleted, so invalidate all references to
		 * ourselves. We run the risk that this will actually result
		 * in this object being deleted (as refcnt goes to zero)
		 * while emitting DropReferences.
		 */
		try {
			std::shared_ptr<Region> me (shared_from_this ());
			drop_references ();
		} catch (...) {
			/* relax */
		}
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
	Glib::Threads::Mutex::Lock lx (_source_list_lock);
	for (SourceList::const_iterator i = _master_sources.begin (); i != _master_sources.end(); ++i) {
		(*i)->dec_use_count ();
	}

	_master_sources = srcs;
	assert (_sources.size() == _master_sources.size());

	for (SourceList::const_iterator i = _master_sources.begin (); i != _master_sources.end(); ++i) {
		(*i)->inc_use_count ();
	}
	subscribe_to_source_drop ();
}

bool
Region::source_equivalent (std::shared_ptr<const Region> other) const
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
Region::any_source_equivalent (std::shared_ptr<const Region> other) const
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
Region::deep_sources (std::set<std::shared_ptr<Source> > & sources) const
{
	for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {

		std::shared_ptr<PlaylistSource> ps = std::dynamic_pointer_cast<PlaylistSource> (*i);

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

		std::shared_ptr<PlaylistSource> ps = std::dynamic_pointer_cast<PlaylistSource> (*i);

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
Region::uses_source (std::shared_ptr<const Source> source, bool shallow) const
{
	for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {
		if (*i == source) {
			return true;
		}

		if (!shallow) {
			std::shared_ptr<PlaylistSource> ps = std::dynamic_pointer_cast<PlaylistSource> (*i);

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
			std::shared_ptr<PlaylistSource> ps = std::dynamic_pointer_cast<PlaylistSource> (*i);

			if (ps) {
				if (ps->playlist()->uses_source (source)) {
					return true;
				}
			}
		}
	}

	return false;
}


timepos_t
Region::source_length (uint32_t n) const
{
	assert (n < _sources.size());
	return _sources[n]->length ();
}

bool
Region::verify_length (timecnt_t& len)
{
	if (source() && source()->length_mutable()) {
		return true;
	}

	timecnt_t maxlen;

	for (uint32_t n = 0; n < _sources.size(); ++n) {
		/* this is computing the distance between _start and the end of the source */
		timecnt_t max_possible_length = _start.val().distance (source_length(n));
		maxlen = max (maxlen, max_possible_length);
	}

	len = timecnt_t (min (len, maxlen), len.position());

	return true;
}

bool
Region::verify_start_and_length (timepos_t const & new_start, timecnt_t& new_length)
{
	if (source() && source()->length_mutable()) {
		return true;
	}

	timecnt_t maxlen;

	for (uint32_t n = 0; n < _sources.size(); ++n) {
		maxlen = max (maxlen, new_start.distance (source_length(n)));
	}

	new_length = min (new_length, maxlen);

	return true;
}

bool
Region::verify_start (timepos_t const & pos)
{
	if (source() && source()->length_mutable()) {
		return true;
	}

	for (uint32_t n = 0; n < _sources.size(); ++n) {
		/* _start can't be before the start of the region as defined by its length */
		if (pos > source_length(n).earlier (_length)) {
			return false;
		}
	}
	return true;
}

std::shared_ptr<Region>
Region::get_parent() const
{
	std::shared_ptr<Playlist> pl (playlist());

	if (pl) {
		std::shared_ptr<Region> r;
		std::shared_ptr<Region const> grrr2 = std::dynamic_pointer_cast<Region const> (shared_from_this());

		if (grrr2 && (r = _session.find_whole_file_parent (grrr2))) {
			return std::static_pointer_cast<Region> (r);
		}
	}

	return std::shared_ptr<Region>();
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

	Region::merge_features (afl, _onsets, position_sample());
	Region::merge_features (afl, _user_transients, position_sample() + _transient_user_start - start_sample());
	if (!_onsets.empty ()) {
		++cnt;
	}
	if (!_user_transients.empty ()) {
		++cnt;
	}
	if (cnt > 1) {
		afl.sort ();
		// remove exact duplicates
		TransientDetector::cleanup_transients (afl, _session.sample_rate(), 0);
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
Region::merge_features (AnalysisFeatureList& result, const AnalysisFeatureList& src, const sampleoffset_t off) const
{
	for (AnalysisFeatureList::const_iterator x = src.begin(); x != src.end(); ++x) {
		const sampleoffset_t p = (*x) + off;
		if (p < first_sample() || p > last_sample()) {
			continue;
		}
		result.push_back (p);
	}
}

void
Region::captured_xruns (XrunPositions& xruns, bool abs) const
{
	bool was_empty = xruns.empty ();
	for (SourceList::const_iterator i = _sources.begin (); i != _sources.end(); ++i) {
		XrunPositions const& x = (*i)->captured_xruns ();
		const samplepos_t ss = start_sample();
		const samplecnt_t ll = length_samples();
		for (XrunPositions::const_iterator p = x.begin (); p != x.end (); ++p) {
			if (abs) {
				xruns.push_back (*p);
			} else if (*p >= ss && *p < ss + ll) {
				xruns.push_back (*p - ss);
			}
		}
	}
	if (_sources.size () > 1 || !was_empty) {
		sort (xruns.begin (), xruns.end ());
		xruns.erase (unique (xruns.begin (), xruns.end ()), xruns.end ());
	}
}

void
Region::get_cue_markers (CueMarkers& cues, bool abs) const
{
	for (SourceList::const_iterator s = _sources.begin (); s != _sources.end(); ++s) {
		CueMarkers const& x = (*s)->cue_markers ();
		for (CueMarkers::const_iterator p = x.begin (); p != x.end (); ++p) {
			if (p->position() >= start() && p->position() < start() + length()) {
				if (abs) {
					cues.insert (*p);
				} else {
					cues.insert (CueMarker (p->text(), timepos_t (start().distance (p->position()))));
				}
			}
		}
	}
}

void
Region::move_cue_marker (CueMarker const & cm, timepos_t const & region_relative_position)
{
	for (SourceList::const_iterator s = _sources.begin (); s != _sources.end(); ++s) {
		(*s)->move_cue_marker (cm, region_relative_position + start());
	}
}

void
Region::rename_cue_marker (CueMarker& cm, std::string const & str)
{
	for (SourceList::const_iterator s = _sources.begin (); s != _sources.end(); ++s) {
		(*s)->rename_cue_marker (cm, str);
	}
}

void
Region::drop_sources ()
{
	Glib::Threads::Mutex::Lock lx (_source_list_lock);
	for (SourceList::const_iterator i = _sources.begin (); i != _sources.end(); ++i) {
		(*i)->dec_use_count ();
	}

	_sources.clear ();

	for (SourceList::const_iterator i = _master_sources.begin (); i != _master_sources.end(); ++i) {
		(*i)->dec_use_count ();
	}

	_master_sources.clear ();
	_source_deleted_connections.drop_connections ();
}

void
Region::use_sources (SourceList const & s)
{
	Glib::Threads::Mutex::Lock lx (_source_list_lock);
	for (SourceList::const_iterator i = s.begin (); i != s.end(); ++i) {
		_sources.push_back (*i);
		(*i)->inc_use_count ();
		_master_sources.push_back (*i);
		(*i)->inc_use_count ();
	}
	subscribe_to_source_drop ();
}

void
Region::subscribe_to_source_drop ()
{
	_source_deleted.store (0);
	_source_deleted_connections.drop_connections ();
	set<std::shared_ptr<Source> > unique_srcs;
	for (auto const& i : _sources) {
		if (unique_srcs.find (i) == unique_srcs.end ()) {
			unique_srcs.insert (i);
			i->DropReferences.connect_same_thread (_source_deleted_connections, boost::bind (&Region::source_deleted, this, std::weak_ptr<Source>(i)));
		}
	}
	for (auto const& i : _master_sources) {
		if (unique_srcs.find (i) == unique_srcs.end ()) {
			unique_srcs.insert (i);
			i->DropReferences.connect_same_thread (_source_deleted_connections, boost::bind (&Region::source_deleted, this, std::weak_ptr<Source>(i)));
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
		if ((start() + length()) < _sources.front()->length ()) {
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
Region::set_start_internal (timepos_t const & s)
{
	_start = s;
}

timepos_t
Region::earliest_possible_position () const
{
	if (start() > timecnt_t (position(), timepos_t())) {
		return timepos_t::from_superclock (0);
	} else {
		return source_position();
	}
}

samplecnt_t
Region::latest_possible_sample () const
{
	timecnt_t minlen = timecnt_t::max (Temporal::AudioTime);

	for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {
		/* non-audio regions have a length that may vary based on their
		 * position, so we have to pass it in the call.
		 */
		minlen = min (minlen, timecnt_t ((*i)->length (), (*i)->natural_position()));
	}

	/* the latest possible last sample is determined by the current
	 * position, plus the shortest source extent past _start.
	 */

	return (position() + minlen).samples() - 1;
}

Temporal::TimeDomain
Region::position_time_domain() const
{
	return position().time_domain();
}

timepos_t
Region::end() const
{
	/* one day we might want to enforce _position, _start and _length (or
	   some combination thereof) all being in the same time domain.
	*/
	return _length.val().end();
}

timepos_t
Region::source_position () const
{
	/* this is the position of the start of the source, in absolute time */
	return position().earlier (_start.val());
}

Temporal::Beats
Region::region_distance_to_region_beats (timecnt_t const & region_relative_offset) const
{
	return timecnt_t (region_relative_offset, position()).beats ();
}

Temporal::Beats
Region::source_beats_to_absolute_beats (Temporal::Beats beats) const
{
	/* since the return type must be beats, force source_position() to
	   beats before adding, rather than after.
	*/
	return source_position().beats() + beats;
}

Temporal::Beats
Region::absolute_time_to_region_beats(timepos_t const & b) const
{
	 return (position().distance (b)).beats () + start().beats();
}

Temporal::timepos_t
Region::absolute_time_to_region_time (timepos_t const & t) const
{
	return start() + position().distance (t);
}

Temporal::timepos_t
Region::region_beats_to_absolute_time (Temporal::Beats beats) const
{
	return position() + timepos_t (beats);
}

Temporal::timepos_t
Region::source_beats_to_absolute_time (Temporal::Beats beats) const
{
	/* return the time corresponding to `beats' relative to the start of
	   the source. The start of the source is an implied position given by
	   region->position - region->start aka ::source_position()
	*/
	return source_position() + timepos_t (beats);
}

/** Calculate  (time - source_position) in Beats
 *
 * Measure the distance between the absolute time and the position of
 * the source start, in beats. The result is positive if time is later
 * than source position.
 *
 * @param p is an absolute time
 * @returns time offset from p to the region's source position as the origin in Beat units
 */
Temporal::Beats
Region::absolute_time_to_source_beats(timepos_t const& p) const
{
	return source_position().distance (p).beats();
}

/** Calculate (pos - source-position)
 *
 * @param p is an absolute time
 * @returns time offset from p to the region's source position as the origin.
 */
timecnt_t
Region::source_relative_position (timepos_t const & p) const
{
	return source_position().distance (p);
}

/** Calculate (p - region-position)
 *
 * @param p is an absolute time
 * @returns the time offset using the region (timeline) position as origin
 */
timecnt_t
Region::region_relative_position (timepos_t const & p) const
{
	return position().distance (p);
}

Temporal::TimeDomain
Region::time_domain() const
{
	std::shared_ptr<Playlist> pl (_playlist.lock());

	if (pl) {
		return pl->time_domain ();
	}

	switch (_type) {
	case DataType::AUDIO:
		return Temporal::AudioTime;
	default:
		break;
	}

	return Temporal::BeatTime;
}

void
Region::start_domain_bounce (Temporal::DomainBounceInfo& cmd)
{
	if (locked()) {
		return;
	}

	/* recall that the _length member is a timecnt_t, and so holds both
	 * position *and* length.
	 */

	if (_length.val().time_domain() != cmd.from) {
		return;
	}

	timecnt_t& l (_length.non_const_val());

	timecnt_t  saved (l);
	saved.set_time_domain (cmd.to);

	cmd.counts.insert (std::make_pair (&l, saved));
}

void
Region::finish_domain_bounce (Temporal::DomainBounceInfo& cmd)
{
	clear_changes ();

	Temporal::TimeDomainCntChanges::iterator tc = cmd.counts.find (&_length.non_const_val());
	if (tc == cmd.counts.end()) {
		/* must have already been in the correct time domain */
		return;
	}

	/* switch domains back (but with modified TempoMap, presumably */
	tc->second.set_time_domain (cmd.from);
	_length = tc->second;

	send_change (Properties::length);
}

bool
Region::load_plugin (ARDOUR::PluginType type, std::string const& name)
{
	PluginInfoPtr pip = LuaAPI::new_plugin_info (name, type);
	if (!pip) {
		return false;
	}
	PluginPtr p (pip->load (_session));
	if (!p) {
		return false;
	}
	std::shared_ptr<RegionFxPlugin> rfx (new RegionFxPlugin (_session, time_domain (), p));
	return add_plugin (rfx);
}

bool
Region::add_plugin (std::shared_ptr<RegionFxPlugin> rfx, std::shared_ptr<RegionFxPlugin> pos)
{
	return _add_plugin (rfx, pos, false);
}

void
Region::reorder_plugins (RegionFxList const& new_order)
{
	Glib::Threads::RWLock::WriterLock lm (_fx_lock);

	RegionFxList                 as_it_will_be;
	RegionFxList::iterator       oiter = _plugins.begin ();
	RegionFxList::const_iterator niter = new_order.begin ();

	while (niter != new_order.end ()) {
		if (oiter == _plugins.end ()) {
			as_it_will_be.insert (as_it_will_be.end (), niter, new_order.end ());
			while (niter != new_order.end ()) {
				++niter;
			}
			break;
		}
		if (find (new_order.begin (), new_order.end (), *oiter) != new_order.end ()) {
			as_it_will_be.push_back (*niter);
			++niter;
		}
		oiter = _plugins.erase (oiter);
	}
	_plugins.insert (oiter, as_it_will_be.begin (), as_it_will_be.end ());
}

void
Region::fx_latency_changed (bool)
{
	uint32_t l = 0;
	for (auto const& rfx : _plugins) {
		l += rfx->effective_latency ();
	}
	if (l == _fx_latency) {
		return;
	}
	_fx_latency = l;
}
