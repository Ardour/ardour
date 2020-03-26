/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006 Hans Fugal <hans@fugal.net>
 * Copyright (C) 2007-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2009 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 GZharun <grygoriiz@wavesglobal.com>
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

#include <algorithm>
#include <set>
#include <cstdio> /* for sprintf */
#include <unistd.h>
#include <cerrno>
#include <ctime>
#include <list>

#include "pbd/types_convert.h"
#include "pbd/stl_delete.h"
#include "pbd/xml++.h"
#include "pbd/enumwriter.h"

#include "ardour/location.h"
#include "ardour/midi_scene_change.h"
#include "ardour/session.h"
#include "ardour/audiofilesource.h"
#include "ardour/tempo.h"
#include "ardour/types_convert.h"

#include "pbd/i18n.h"

namespace PBD {
	DEFINE_ENUM_CONVERT(ARDOUR::Location::Flags);
}

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PBD::Signal0<void> Location::scene_changed;
PBD::Signal1<void,Location*> Location::name_changed;
PBD::Signal1<void,Location*> Location::end_changed;
PBD::Signal1<void,Location*> Location::start_changed;
PBD::Signal1<void,Location*> Location::flags_changed;
PBD::Signal1<void,Location*> Location::lock_changed;
PBD::Signal1<void,Location*> Location::position_lock_style_changed;
PBD::Signal1<void,Location*> Location::changed;

Location::Location (Session& s)
	: SessionHandleRef (s)
	, _start (0)
	, _start_beat (0.0)
	, _end (0)
	, _end_beat (0.0)
	, _flags (Flags (0))
	, _locked (false)
	, _position_lock_style (AudioTime)
	, _timestamp (time (0))
{
	assert (_start >= 0);
	assert (_end >= 0);
}

/** Construct a new Location, giving it the position lock style determined by glue-new-markers-to-bars-and-beats */
Location::Location (Session& s, samplepos_t sample_start, samplepos_t sample_end, const std::string &name, Flags bits, const uint32_t sub_num)
	: SessionHandleRef (s)
	, _name (name)
	, _start (sample_start)
	, _end (sample_end)
	, _flags (bits)
	, _locked (false)
	, _position_lock_style (s.config.get_glue_new_markers_to_bars_and_beats() ? MusicTime : AudioTime)
	, _timestamp (time (0))
{
	recompute_beat_from_samples (sub_num);

	assert (_start >= 0);
	assert (_end >= 0);
}

Location::Location (const Location& other)
	: SessionHandleRef (other._session)
	, StatefulDestructible()
	, _name (other._name)
	, _start (other._start)
	, _start_beat (other._start_beat)
	, _end (other._end)
	, _end_beat (other._end_beat)
	, _flags (other._flags)
	, _position_lock_style (other._position_lock_style)
	, _timestamp (time (0))
{
	/* copy is not locked even if original was */

	_locked = false;

	assert (_start >= 0);
	assert (_end >= 0);

	/* scene change is NOT COPIED */
}

Location::Location (Session& s, const XMLNode& node)
	: SessionHandleRef (s)
	, _flags (Flags (0))
	, _position_lock_style (AudioTime)
	, _timestamp (time (0))
{
	/* Note: _position_lock_style is initialised above in case set_state doesn't set it
	   (for 2.X session file compatibility).
	*/

	if (set_state (node, Stateful::loading_state_version)) {
		throw failed_constructor ();
	}

	assert (_start >= 0);
	assert (_end >= 0);
}

bool
Location::operator== (const Location& other)
{
	if (_name != other._name ||
	    _start != other._start ||
	    _end != other._end ||
	    _start_beat != other._start_beat ||
	    _end_beat != other._end_beat ||
	    _flags != other._flags ||
	    _position_lock_style != other._position_lock_style) {
		return false;
	}
	return true;
}

Location*
Location::operator= (const Location& other)
{
	if (this == &other) {
		return this;
	}

	_name = other._name;
	_start = other._start;
	_start_beat = other._start_beat;
	_end = other._end;
	_end_beat = other._end_beat;
	_flags = other._flags;
	_position_lock_style = other._position_lock_style;

	/* XXX need to copy scene change */

	/* copy is not locked even if original was */

	_locked = false;

	/* "changed" not emitted on purpose */

	assert (_start >= 0);
	assert (_end >= 0);

	return this;
}

/** Set location name
 */

void
Location::set_name (const std::string& str)
{
	_name = str;

	name_changed (this); /* EMIT SIGNAL */
	NameChanged  (); /* EMIT SIGNAL */
}

/** Set start position.
 *  @param s New start.
 *  @param force true to force setting, even if the given new start is after the current end.
 *  @param allow_beat_recompute True to recompute BEAT start time from the new given start time.
 */
int
Location::set_start (samplepos_t s, bool force, bool allow_beat_recompute, const uint32_t sub_num)
{
	if (s < 0) {
		return -1;
	}

	if (_locked) {
		return -1;
	}

	if (!force) {
		if (((is_auto_punch() || is_auto_loop()) && s >= _end) || (!is_mark() && s > _end)) {
			return -1;
		}
	}

	if (is_mark()) {
		if (_start != s) {
			_start = s;
			_end = s;
			if (allow_beat_recompute) {
				recompute_beat_from_samples (sub_num);
			}

			start_changed (this); /* EMIT SIGNAL */
			StartChanged (); /* EMIT SIGNAL */
			//end_changed (this); /* EMIT SIGNAL */
			//EndChanged (); /* EMIT SIGNAL */
		}

		/* moving the start (position) of a marker with a scene change
		   requires an update in the Scene Changer.
		*/

		if (_scene_change) {
			scene_changed (); /* EMIT SIGNAL */
		}

		assert (_start >= 0);
		assert (_end >= 0);

		return 0;
	} else if (!force) {
		/* range locations must exceed a minimum duration */
		if (_end - s < Config->get_range_location_minimum()) {
			return -1;
		}
	}

	if (s != _start) {

		samplepos_t const old = _start;

		_start = s;
		if (allow_beat_recompute) {
			recompute_beat_from_samples (sub_num);
		}
		start_changed (this); /* EMIT SIGNAL */
		StartChanged (); /* EMIT SIGNAL */

		if (is_session_range ()) {
			Session::StartTimeChanged (old); /* EMIT SIGNAL */
			AudioFileSource::set_header_position_offset (s);
		}
	}

	assert (_start >= 0);

	return 0;
}

/** Set end position.
 *  @param s New end.
 *  @param force true to force setting, even if the given new end is before the current start.
 *  @param allow_beat_recompute True to recompute BEAT end time from the new given end time.
 */
int
Location::set_end (samplepos_t e, bool force, bool allow_beat_recompute, const uint32_t sub_num)
{
	if (e < 0) {
		return -1;
	}

	if (_locked) {
		return -1;
	}

	if (!force) {
		if (((is_auto_punch() || is_auto_loop()) && e <= _start) || e < _start) {
			return -1;
		}
	}

	if (is_mark()) {
		if (_start != e) {
			_start = e;
			_end = e;
			if (allow_beat_recompute) {
				recompute_beat_from_samples (sub_num);
			}
			//start_changed (this); /* EMIT SIGNAL */
			//StartChanged (); /* EMIT SIGNAL */
			end_changed (this); /* EMIT SIGNAL */
			EndChanged (); /* EMIT SIGNAL */
		}

		assert (_start >= 0);
		assert (_end >= 0);

		return 0;
	} else if (!force) {
		/* range locations must exceed a minimum duration */
		if (e - _start < Config->get_range_location_minimum()) {
			return -1;
		}
	}

	if (e != _end) {

		samplepos_t const old = _end;

		_end = e;
		if (allow_beat_recompute) {
			recompute_beat_from_samples (sub_num);
		}

		end_changed(this); /* EMIT SIGNAL */
		EndChanged(); /* EMIT SIGNAL */

		if (is_session_range()) {
			Session::EndTimeChanged (old); /* EMIT SIGNAL */
		}
	}

	assert (_end >= 0);

	return 0;
}

int
Location::set (samplepos_t s, samplepos_t e, bool allow_beat_recompute, const uint32_t sub_num)
{
	if (s < 0 || e < 0) {
		return -1;
	}

	/* check validity */
	if (((is_auto_punch() || is_auto_loop()) && s >= e) || (!is_mark() && s > e)) {
		return -1;
	}

	bool start_change = false;
	bool end_change = false;

	if (is_mark()) {

		if (_start != s) {
			_start = s;
			_end = s;

			if (allow_beat_recompute) {
				recompute_beat_from_samples (sub_num);
			}

			start_change = true;
			end_change = true;
		}

		assert (_start >= 0);
		assert (_end >= 0);

	} else {

		/* range locations must exceed a minimum duration */
		if (e - s < Config->get_range_location_minimum()) {
			return -1;
		}

		if (s != _start) {

			samplepos_t const old = _start;
			_start = s;

			if (allow_beat_recompute) {
				recompute_beat_from_samples (sub_num);
			}

			start_change = true;

			if (is_session_range ()) {
				Session::StartTimeChanged (old); /* EMIT SIGNAL */
				AudioFileSource::set_header_position_offset (s);
			}
		}


		if (e != _end) {

			samplepos_t const old = _end;
			_end = e;

			if (allow_beat_recompute) {
				recompute_beat_from_samples (sub_num);
			}

			end_change = true;

			if (is_session_range()) {
				Session::EndTimeChanged (old); /* EMIT SIGNAL */
			}
		}

		assert (_end >= 0);
	}

	if (start_change && end_change) {
		changed (this);
		Changed ();
	} else if (start_change) {
		start_changed(this); /* EMIT SIGNAL */
		StartChanged(); /* EMIT SIGNAL */
	} else if (end_change) {
		end_changed(this); /* EMIT SIGNAL */
		EndChanged(); /* EMIT SIGNAL */
	}

	return 0;
}

int
Location::move_to (samplepos_t pos, const uint32_t sub_num)
{
	if (pos < 0) {
		return -1;
	}

	if (_locked) {
		return -1;
	}

	if (_start != pos) {
		_start = pos;
		_end = _start + length();
		recompute_beat_from_samples (sub_num);

		changed (this); /* EMIT SIGNAL */
		Changed (); /* EMIT SIGNAL */
	}

	assert (_start >= 0);
	assert (_end >= 0);

	return 0;
}

void
Location::set_hidden (bool yn, void*)
{
	if (set_flag_internal (yn, IsHidden)) {
		flags_changed (this); /* EMIT SIGNAL */
		FlagsChanged ();
	}
}

void
Location::set_cd (bool yn, void*)
{
	// XXX this really needs to be session start
	// but its not available here - leave to GUI

	if (yn && _start == 0) {
		error << _("You cannot put a CD marker at this position") << endmsg;
		return;
	}

	if (set_flag_internal (yn, IsCDMarker)) {
		flags_changed (this); /* EMIT SIGNAL */
		FlagsChanged ();
	}
}

void
Location::set_is_range_marker (bool yn, void*)
{
	if (set_flag_internal (yn, IsRangeMarker)) {
		flags_changed (this);
		FlagsChanged (); /* EMIT SIGNAL */
	}
}

void
Location::set_is_clock_origin (bool yn, void*)
{
	if (set_flag_internal (yn, IsClockOrigin)) {
		flags_changed (this);
		FlagsChanged (); /* EMIT SIGNAL */
	}
}

void
Location::set_skip (bool yn)
{
	if (is_range_marker() && length() > 0) {
		if (set_flag_internal (yn, IsSkip)) {
			flags_changed (this);
			FlagsChanged ();
		}
	}
}

void
Location::set_skipping (bool yn)
{
	if (is_range_marker() && is_skip() && length() > 0) {
		if (set_flag_internal (yn, IsSkipping)) {
			flags_changed (this);
			FlagsChanged ();
		}
	}
}

void
Location::set_auto_punch (bool yn, void*)
{
	if (is_mark() || _start == _end) {
		return;
	}

	if (set_flag_internal (yn, IsAutoPunch)) {
		flags_changed (this); /* EMIT SIGNAL */
		FlagsChanged (); /* EMIT SIGNAL */
	}
}

void
Location::set_auto_loop (bool yn, void*)
{
	if (is_mark() || _start == _end) {
		return;
	}

	if (set_flag_internal (yn, IsAutoLoop)) {
		flags_changed (this); /* EMIT SIGNAL */
		FlagsChanged (); /* EMIT SIGNAL */
	}
}

bool
Location::set_flag_internal (bool yn, Flags flag)
{
	if (yn) {
		if (!(_flags & flag)) {
			_flags = Flags (_flags | flag);
			return true;
		}
	} else {
		if (_flags & flag) {
			_flags = Flags (_flags & ~flag);
			return true;
		}
	}
	return false;
}

void
Location::set_mark (bool yn)
{
	/* This function is private, and so does not emit signals */

	if (_start != _end) {
		return;
	}

	set_flag_internal (yn, IsMark);
}


XMLNode&
Location::cd_info_node(const string & name, const string & value)
{
	XMLNode* root = new XMLNode("CD-Info");

	root->set_property("name", name);
	root->set_property("value", value);

	return *root;
}


XMLNode&
Location::get_state ()
{
	XMLNode *node = new XMLNode ("Location");

	typedef map<string, string>::const_iterator CI;

	for(CI m = cd_info.begin(); m != cd_info.end(); ++m){
		node->add_child_nocopy(cd_info_node(m->first, m->second));
	}

	node->set_property ("id", id ());
	node->set_property ("name", name());
	node->set_property ("start", start());
	node->set_property ("end", end());
	if (position_lock_style() == MusicTime) {
		node->set_property ("start-beat", _start_beat);
		node->set_property ("end-beat", _end_beat);
	}
	node->set_property ("flags", _flags);
	node->set_property ("locked", _locked);
	node->set_property ("position-lock-style", _position_lock_style);
	node->set_property ("timestamp", _timestamp);
	if (_scene_change) {
		node->add_child_nocopy (_scene_change->get_state());
	}

	return *node;
}

int
Location::set_state (const XMLNode& node, int version)
{
	XMLNodeList cd_list = node.children();
	XMLNodeConstIterator cd_iter;
	XMLNode *cd_node;

	string cd_name;
	string cd_value;

	if (node.name() != "Location") {
		error << _("incorrect XML node passed to Location::set_state") << endmsg;
		return -1;
	}

	if (!set_id (node)) {
		warning << _("XML node for Location has no ID information") << endmsg;
	}

	std::string str;
	if (!node.get_property ("name", str)) {
		error << _("XML node for Location has no name information") << endmsg;
		return -1;
	}

	set_name (str);

	/* can't use set_start() here, because _end
	   may make the value of _start illegal.
	*/

	if (!node.get_property ("start", _start)) {
		error << _("XML node for Location has no start information") << endmsg;
		return -1;
	}

	if (!node.get_property ("end", _end)) {
		error << _("XML node for Location has no end information") << endmsg;
		return -1;
	}

	node.get_property ("timestamp", _timestamp);

	Flags old_flags (_flags);

	if (!node.get_property ("flags", _flags)) {
		error << _("XML node for Location has no flags information") << endmsg;
		return -1;
	}

	if (old_flags != _flags) {
		FlagsChanged ();
	}

	if (!node.get_property ("locked", _locked)) {
		_locked = false;
	}

	for (cd_iter = cd_list.begin(); cd_iter != cd_list.end(); ++cd_iter) {

		cd_node = *cd_iter;

		if (cd_node->name() != "CD-Info") {
			continue;
		}

		if (!cd_node->get_property ("name", cd_name)) {
			throw failed_constructor ();
		}

		if (!cd_node->get_property ("value", cd_value)) {
			throw failed_constructor ();
		}

		cd_info[cd_name] = cd_value;
	}

	node.get_property ("position-lock-style", _position_lock_style);

	XMLNode* scene_child = find_named_node (node, SceneChange::xml_node_name);

	if (scene_child) {
		_scene_change = SceneChange::factory (*scene_child, version);
	}

	if (position_lock_style() == AudioTime) {
		recompute_beat_from_samples (0);
	} else{
		/* music */
		if (!node.get_property ("start-beat", _start_beat) ||
		    !node.get_property ("end-beat", _end_beat)) {
			recompute_beat_from_samples (0);
		}
	}


	changed (this); /* EMIT SIGNAL */
	Changed (); /* EMIT SIGNAL */

	assert (_start >= 0);
	assert (_end >= 0);

	return 0;
}

void
Location::set_position_lock_style (PositionLockStyle ps)
{
	if (_position_lock_style == ps) {
		return;
	}

	_position_lock_style = ps;

	if (ps == MusicTime) {
		recompute_beat_from_samples (0);
	}

	position_lock_style_changed (this); /* EMIT SIGNAL */
	PositionLockStyleChanged (); /* EMIT SIGNAL */
}

void
Location::recompute_beat_from_samples (const uint32_t sub_num)
{
	_start_beat = _session.tempo_map().exact_beat_at_sample (_start, sub_num);
	_end_beat = _session.tempo_map().exact_beat_at_sample (_end, sub_num);
}

void
Location::recompute_samples_from_beat ()
{
	if (_position_lock_style != MusicTime) {
		return;
	}

	TempoMap& map (_session.tempo_map());
	set (map.sample_at_beat (_start_beat), map.sample_at_beat (_end_beat), false);
}

void
Location::lock ()
{
	_locked = true;
	lock_changed (this);
	LockChanged ();
}

void
Location::unlock ()
{
	_locked = false;
	lock_changed (this);
	LockChanged ();
}

void
Location::set_scene_change (boost::shared_ptr<SceneChange>  sc)
{
        if (_scene_change != sc) {
                _scene_change = sc;
                _session.set_dirty ();

                scene_changed (); /* EMIT SIGNAL */
                SceneChangeChanged (); /* EMIT SIGNAL */
        }
}

/*---------------------------------------------------------------------- */

Locations::Locations (Session& s)
	: SessionHandleRef (s)
{
	current_location = 0;
}

Locations::~Locations ()
{
	for (LocationList::iterator i = locations.begin(); i != locations.end(); ) {
		LocationList::iterator tmp = i;
		++tmp;
		delete *i;
		i = tmp;
	}
}

int
Locations::set_current (Location *loc, bool want_lock)
{
	int ret;

	if (want_lock) {
		Glib::Threads::Mutex::Lock lm (lock);
		ret = set_current_unlocked (loc);
	} else {
		ret = set_current_unlocked (loc);
	}

	if (ret == 0) {
		current_changed (current_location); /* EMIT SIGNAL */
	}
	return ret;
}

void
Locations::set_clock_origin (Location* loc, void *src)
{
	LocationList::iterator i;
	for (i = locations.begin(); i != locations.end(); ++i) {
		if ((*i)->is_clock_origin ()) {
			(*i)->set_is_clock_origin (false, src);
		}
		if (*i == loc) {
			(*i)->set_is_clock_origin (true, src);
		}
	}
}

int
Locations::next_available_name(string& result,string base)
{
	LocationList::iterator i;
	string::size_type l;
	int suffix;
	char buf[32];
	std::map<uint32_t,bool> taken;
	uint32_t n;

	result = base;
	l = base.length();

	if (!base.empty()) {

		/* find all existing names that match "base", and store
		   the numeric part of them (if any) in the map "taken"
		*/

		for (i = locations.begin(); i != locations.end(); ++i) {

			const string& temp ((*i)->name());

			if (!temp.find (base,0)) {
				/* grab what comes after the "base" as if it was
				   a number, and assuming that works OK,
				   store it in "taken" so that we know it
				   has been used.
				*/
                                if ((suffix = atoi (temp.substr(l))) != 0) {
					taken.insert (make_pair (suffix,true));
				}
			}
		}
	}

	/* Now search for an un-used suffix to add to "base". This
	   will find "holes" in the numbering sequence when a location
	   was deleted.

	   This must start at 1, both for human-numbering reasons
	   and also because the call to atoi() above would return
	   zero if there is no recognizable numeric suffix, causing
	   "base 0" not to be inserted into the "taken" map.
	*/

	n = 1;

	while (n < UINT32_MAX) {
		if (taken.find (n) == taken.end()) {
			snprintf (buf, sizeof(buf), "%d", n);
			result += buf;
			return 1;
		}
		++n;
	}

	return 0;
}

int
Locations::set_current_unlocked (Location *loc)
{
	if (find (locations.begin(), locations.end(), loc) == locations.end()) {
		error << _("Locations: attempt to use unknown location as selected location") << endmsg;
		return -1;
	}

	current_location = loc;
	return 0;
}

void
Locations::clear ()
{
	{
		Glib::Threads::Mutex::Lock lm (lock);

		for (LocationList::iterator i = locations.begin(); i != locations.end(); ) {

			LocationList::iterator tmp = i;
			++tmp;

			if (!(*i)->is_session_range()) {
				delete *i;
				locations.erase (i);
			}

			i = tmp;
		}

		current_location = 0;
	}

	changed (); /* EMIT SIGNAL */
	current_changed (0); /* EMIT SIGNAL */
}

void
Locations::clear_markers ()
{
	{
		Glib::Threads::Mutex::Lock lm (lock);
		LocationList::iterator tmp;

		for (LocationList::iterator i = locations.begin(); i != locations.end(); ) {
			tmp = i;
			++tmp;

			if ((*i)->is_mark() && !(*i)->is_session_range()) {
				delete *i;
				locations.erase (i);
			}

			i = tmp;
		}
	}

	changed (); /* EMIT SIGNAL */
}

void
Locations::clear_ranges ()
{
	{
		Glib::Threads::Mutex::Lock lm (lock);
		LocationList::iterator tmp;

		for (LocationList::iterator i = locations.begin(); i != locations.end(); ) {

			tmp = i;
			++tmp;

			/* We do not remove these ranges as part of this
			 * operation
			 */

			if ((*i)->is_auto_punch() ||
			    (*i)->is_auto_loop() ||
			    (*i)->is_session_range()) {
				i = tmp;
				continue;
			}

			if (!(*i)->is_mark()) {
				delete *i;
				locations.erase (i);

			}

			i = tmp;
		}

		current_location = 0;
	}

	changed ();
	current_changed (0); /* EMIT SIGNAL */
}

void
Locations::add (Location *loc, bool make_current)
{
	assert (loc);

	{
		Glib::Threads::Mutex::Lock lm (lock);
		locations.push_back (loc);

		if (make_current) {
			current_location = loc;
		}
	}

	added (loc); /* EMIT SIGNAL */

	if (make_current) {
		current_changed (current_location); /* EMIT SIGNAL */
	}

	if (loc->is_session_range()) {
		Session::StartTimeChanged (0);
		Session::EndTimeChanged (1);
	}
}

void
Locations::remove (Location *loc)
{
	bool was_removed = false;
	bool was_current = false;
	LocationList::iterator i;

	if (!loc) {
		return;
	}

	if (loc->is_session_range()) {
		return;
	}

	{
		Glib::Threads::Mutex::Lock lm (lock);

		for (i = locations.begin(); i != locations.end(); ++i) {
			if ((*i) == loc) {
				bool was_loop = (*i)->is_auto_loop();
				if ((*i)->is_auto_punch()) {
					/* needs to happen before deleting:
					 * disconnect signals, clear events */
					_session.set_auto_punch_location (0);
				}
				delete *i;
				locations.erase (i);
				was_removed = true;
				if (current_location == loc) {
					current_location = 0;
					was_current = true;
				}
				if (was_loop) {
					if (_session.get_play_loop()) {
						_session.request_play_loop (false, false);
					}
					_session.auto_loop_location_changed (0);
				}
				break;
			}
		}
	}

	if (was_removed) {

		removed (loc); /* EMIT SIGNAL */

		if (was_current) {
			current_changed (0); /* EMIT SIGNAL */
		}
	}
}

XMLNode&
Locations::get_state ()
{
	XMLNode *node = new XMLNode ("Locations");
	LocationList::iterator iter;
	Glib::Threads::Mutex::Lock lm (lock);

	for (iter = locations.begin(); iter != locations.end(); ++iter) {
		node->add_child_nocopy ((*iter)->get_state ());
	}

	return *node;
}

int
Locations::set_state (const XMLNode& node, int version)
{
	if (node.name() != "Locations") {
		error << _("incorrect XML mode passed to Locations::set_state") << endmsg;
		return -1;
	}

	XMLNodeList nlist = node.children();

	/* build up a new locations list in here */
	LocationList new_locations;

	current_location = 0;

	Location* session_range_location = 0;
	if (version < 3000) {
		session_range_location = new Location (_session, 0, 0, _("session"), Location::IsSessionRange, 0);
		new_locations.push_back (session_range_location);
	}

	{
		Glib::Threads::Mutex::Lock lm (lock);

		XMLNodeConstIterator niter;
		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

			try {

				XMLProperty const * prop_id = (*niter)->property ("id");
				assert (prop_id);
				PBD::ID id (prop_id->value ());

				LocationList::const_iterator i = locations.begin();
				while (i != locations.end () && (*i)->id() != id) {
					++i;
				}

				Location* loc;
				if (i != locations.end()) {
					/* we can re-use an old Location object */
					loc = *i;

					// changed locations will be updated by Locations::changed signal
					loc->set_state (**niter, version);
				} else {
					loc = new Location (_session, **niter);
				}

				bool add = true;

				if (version < 3000) {
					/* look for old-style IsStart / IsEnd properties in this location;
					   if they are present, update the session_range_location accordingly
					*/
					XMLProperty const * prop = (*niter)->property ("flags");
					if (prop) {
						string v = prop->value ();
						while (1) {
							string::size_type const c = v.find_first_of (',');
							string const s = v.substr (0, c);
							if (s == X_("IsStart")) {
								session_range_location->set_start (loc->start(), true);
								add = false;
							} else if (s == X_("IsEnd")) {
								session_range_location->set_end (loc->start(), true);
								add = false;
							}

							if (c == string::npos) {
								break;
							}

							v = v.substr (c + 1);
						}
					}
				}

				if (add) {
					new_locations.push_back (loc);
				}
			}

			catch (failed_constructor& err) {
				error << _("could not load location from session file - ignored") << endmsg;
			}
		}

		/* We may have some unused locations in the old list. */
		for (LocationList::iterator i = locations.begin(); i != locations.end(); ) {
			LocationList::iterator tmp = i;
			++tmp;

			LocationList::iterator n = new_locations.begin();
			bool found = false;

			while (n != new_locations.end ()) {
				if ((*i)->id() == (*n)->id()) {
					found = true;
					break;
				}
				++n;
			}

			if (!found) {
				delete *i;
				locations.erase (i);
			}

			i = tmp;
		}

		locations = new_locations;

		if (locations.size()) {
			current_location = locations.front();
		} else {
			current_location = 0;
		}
	}

	changed (); /* EMIT SIGNAL */

	return 0;
}


typedef std::pair<samplepos_t,Location*> LocationPair;

struct LocationStartEarlierComparison
{
	bool operator() (LocationPair a, LocationPair b) {
		return a.first < b.first;
	}
};

struct LocationStartLaterComparison
{
	bool operator() (LocationPair a, LocationPair b) {
		return a.first > b.first;
	}
};

samplepos_t
Locations::first_mark_before (samplepos_t sample, bool include_special_ranges)
{
	Glib::Threads::Mutex::Lock lm (lock);
	vector<LocationPair> locs;

	for (LocationList::iterator i = locations.begin(); i != locations.end(); ++i) {
		locs.push_back (make_pair ((*i)->start(), (*i)));
		if (!(*i)->is_mark()) {
			locs.push_back (make_pair ((*i)->end(), (*i)));
		}
	}

	LocationStartLaterComparison cmp;
	sort (locs.begin(), locs.end(), cmp);

	/* locs is sorted in ascending order */

	for (vector<LocationPair>::iterator i = locs.begin(); i != locs.end(); ++i) {
		if ((*i).second->is_hidden()) {
			continue;
		}
		if (!include_special_ranges && ((*i).second->is_auto_loop() || (*i).second->is_auto_punch())) {
			continue;
		}
		if ((*i).first < sample) {
			return (*i).first;
		}
	}

	return -1;
}

Location*
Locations::mark_at (samplepos_t pos, samplecnt_t slop) const
{
	Glib::Threads::Mutex::Lock lm (lock);
	Location* closest = 0;
	sampleoffset_t mindelta = max_samplepos;
	sampleoffset_t delta;

	/* locations are not necessarily stored in linear time order so we have
	 * to iterate across all of them to find the one closest to a give point.
	 */

	for (LocationList::const_iterator i = locations.begin(); i != locations.end(); ++i) {

		if ((*i)->is_mark()) {
			if (pos > (*i)->start()) {
				delta = pos - (*i)->start();
			} else {
				delta = (*i)->start() - pos;
			}

			if (slop == 0 && delta == 0) {
				/* special case: no slop, and direct hit for position */
				return *i;
			}

			if (delta <= slop) {
				if (delta < mindelta) {
					closest = *i;
					mindelta = delta;
				}
			}
		}
	}

	return closest;
}

samplepos_t
Locations::first_mark_after (samplepos_t sample, bool include_special_ranges)
{
	Glib::Threads::Mutex::Lock lm (lock);
	vector<LocationPair> locs;

	for (LocationList::iterator i = locations.begin(); i != locations.end(); ++i) {
		locs.push_back (make_pair ((*i)->start(), (*i)));
		if (!(*i)->is_mark()) {
			locs.push_back (make_pair ((*i)->end(), (*i)));
		}
	}

	LocationStartEarlierComparison cmp;
	sort (locs.begin(), locs.end(), cmp);

	/* locs is sorted in reverse order */

	for (vector<LocationPair>::iterator i = locs.begin(); i != locs.end(); ++i) {
		if ((*i).second->is_hidden()) {
			continue;
		}
		if (!include_special_ranges && ((*i).second->is_auto_loop() || (*i).second->is_auto_punch())) {
			continue;
		}
		if ((*i).first > sample) {
			return (*i).first;
		}
	}

	return -1;
}

/** Look for the `marks' (either locations which are marks, or start/end points of range markers) either
 *  side of a sample.  Note that if sample is exactly on a `mark', that mark will not be considered for returning
 *  as before/after.
 *  @param sample Frame to look for.
 *  @param before Filled in with the position of the last `mark' before `sample' (or max_samplepos if none exists)
 *  @param after Filled in with the position of the next `mark' after `sample' (or max_samplepos if none exists)
 */
void
Locations::marks_either_side (samplepos_t const sample, samplepos_t& before, samplepos_t& after) const
{
	before = after = max_samplepos;

	LocationList locs;

	{
		Glib::Threads::Mutex::Lock lm (lock);
		locs = locations;
	}

	/* Get a list of positions; don't store any that are exactly on our requested position */

	std::list<samplepos_t> positions;

	for (LocationList::const_iterator i = locs.begin(); i != locs.end(); ++i) {
		if (((*i)->is_auto_loop() || (*i)->is_auto_punch())) {
			continue;
		}

		if (!(*i)->is_hidden()) {
			if ((*i)->is_mark ()) {
				if ((*i)->start() != sample) {
					positions.push_back ((*i)->start ());
				}
			} else {
				if ((*i)->start() != sample) {
					positions.push_back ((*i)->start ());
				}
				if ((*i)->end() != sample) {
					positions.push_back ((*i)->end ());
				}
			}
		}
	}

	if (positions.empty ()) {
		return;
	}

	positions.sort ();

	std::list<samplepos_t>::iterator i = positions.begin ();
	while (i != positions.end () && *i < sample) {
		++i;
	}

	if (i == positions.end ()) {
		/* run out of marks */
		before = positions.back ();
		return;
	}

	after = *i;

	if (i == positions.begin ()) {
		/* none before */
		return;
	}

	--i;
	before = *i;
}

Location*
Locations::session_range_location () const
{
	for (LocationList::const_iterator i = locations.begin(); i != locations.end(); ++i) {
		if ((*i)->is_session_range()) {
			return const_cast<Location*> (*i);
		}
	}
	return 0;
}

Location*
Locations::auto_loop_location () const
{
	for (LocationList::const_iterator i = locations.begin(); i != locations.end(); ++i) {
		if ((*i)->is_auto_loop()) {
			return const_cast<Location*> (*i);
		}
	}
	return 0;
}

Location*
Locations::auto_punch_location () const
{
	for (LocationList::const_iterator i = locations.begin(); i != locations.end(); ++i) {
		if ((*i)->is_auto_punch()) {
			return const_cast<Location*> (*i);
		}
	}
	return 0;
}

Location*
Locations::clock_origin_location () const
{
	for (LocationList::const_iterator i = locations.begin(); i != locations.end(); ++i) {
		if ((*i)->is_clock_origin()) {
			return const_cast<Location*> (*i);
		}
	}
	return session_range_location ();
}

uint32_t
Locations::num_range_markers () const
{
	uint32_t cnt = 0;
	Glib::Threads::Mutex::Lock lm (lock);
	for (LocationList::const_iterator i = locations.begin(); i != locations.end(); ++i) {
		if ((*i)->is_range_marker()) {
			++cnt;
		}
	}
	return cnt;
}

Location *
Locations::get_location_by_id(PBD::ID id)
{
	LocationList::iterator it;
	for (it  = locations.begin(); it != locations.end(); ++it)
		if (id == (*it)->id())
			return *it;

	return 0;
}

void
Locations::find_all_between (samplepos_t start, samplepos_t end, LocationList& ll, Location::Flags flags)
{
	Glib::Threads::Mutex::Lock lm (lock);

	for (LocationList::const_iterator i = locations.begin(); i != locations.end(); ++i) {
		if ((flags == 0 || (*i)->matches (flags)) &&
		    ((*i)->start() >= start && (*i)->end() < end)) {
			ll.push_back (*i);
		}
	}
}
