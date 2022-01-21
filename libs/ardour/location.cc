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
using namespace Temporal;

PBD::Signal0<void> Location::scene_changed;
PBD::Signal1<void,Location*> Location::name_changed;
PBD::Signal1<void,Location*> Location::end_changed;
PBD::Signal1<void,Location*> Location::start_changed;
PBD::Signal1<void,Location*> Location::flags_changed;
PBD::Signal1<void,Location*> Location::lock_changed;
PBD::Signal1<void,Location*> Location::changed;
PBD::Signal1<void,Location*> Location::cue_change;

Location::Location (Session& s)
	: SessionHandleRef (s)
	, _flags (Flags (0))
	, _locked (false)
	, _timestamp (time (0))
	, _cue (0)
{
}

/** Construct a new Location, giving it the position lock style determined by glue-new-markers-to-bars-and-beats */
Location::Location (Session& s, timepos_t const & start, timepos_t const & end, const std::string &name, Flags bits, int32_t cue_id)
	: SessionHandleRef (s)
	, _name (name)
	, _start (start)
	, _end (end)
	, _flags (bits)
	, _locked (false)
	, _timestamp (time (0))
	, _cue (cue_id)
{

	/* it would be nice if the caller could ensure that the start and end
	   values simply use the correct domain, but that would involve
	   enforcing/checking that at every place where we create a
	   Location. So instead we centralize this here.

	   NUTEMPO: it might make sense to switch time domains when <something>
	   happens, but it's not clear what the <something> might be? Maybe
	   changing some setting of the tempo map.
	*/

	if (s.config.get_glue_new_markers_to_bars_and_beats()) {
		set_position_time_domain (Temporal::BeatTime);
	} else {
		set_position_time_domain (Temporal::AudioTime);
	}
}

Location::Location (const Location& other)
	: SessionHandleRef (other._session)
	, StatefulDestructible()
	, _name (other._name)
	, _start (other._start)
	, _end (other._end)
	, _flags (other._flags)
	, _timestamp (time (0))
	, _cue (other._cue)
{
	/* copy is not locked even if original was */

	_locked = false;

	/* scene change is NOT COPIED */
}

Location::Location (Session& s, const XMLNode& node)
	: SessionHandleRef (s)
	, _flags (Flags (0))
	, _timestamp (time (0))
{
	//_start.set_time_domain (AudioTime);
	//_end.set_time_domain (AudioTime);

	/* Note: _position_time_domain is initialised above in case set_state
	 * doesn't set it
	 */

	if (set_state (node, Stateful::loading_state_version)) {
		throw failed_constructor ();
	}
}

bool
Location::operator== (const Location& other)
{
	if (_name != other._name ||
	    _start != other._start ||
	    _end != other._end ||
	    _flags != other._flags) {
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
	_end = other._end;
	_flags = other._flags;

	/* XXX need to copy scene change */

	/* copy is not locked even if original was */

	_locked = false;

	/* "changed" not emitted on purpose */

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
 */
int
Location::set_start (Temporal::timepos_t const & s, bool force)
{

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

		assert (s.is_zero() || s.is_positive());

		if (is_cue_marker()) {
			cue_change (this);
		}

		return 0;
	} else if (!force) {
		/* range locations must exceed a minimum duration */
		if (s.distance (_end) < Config->get_range_location_minimum()) {
			return -1;
		}
	}

	if (s != _start) {

		Temporal::timepos_t const old = _start;

		_start = s;
		start_changed (this); /* EMIT SIGNAL */
		StartChanged (); /* EMIT SIGNAL */

		if (is_session_range ()) {
			Session::StartTimeChanged (old.samples()); /* emit signal */
			AudioFileSource::set_header_position_offset (s.samples());
		}
	}

	assert (_start.is_positive() || _start.is_zero());

	return 0;
}

/** set end position.
 *  @param s new end.
 *  @param force true to force setting, even if the given new end is before the current start.
 */
int
Location::set_end (Temporal::timepos_t const & e, bool force)
{
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
			//start_changed (this); /* emit signal */
			//startchanged (); /* emit signal */
			end_changed (this); /* emit signal */
			EndChanged (); /* emit signal */
		}

		assert (_start >= 0);
		assert (_end >= 0);

		return 0;
	} else if (!force) {
		/* range locations must exceed a minimum duration */
		if (_start.distance (e) < Config->get_range_location_minimum()) {
			return -1;
		}
	}

	if (e != _end) {

		timepos_t const old = _end;

		_end = e;
		end_changed(this); /* EMIT SIGNAL */
		EndChanged(); /* EMIT SIGNAL */

		if (is_session_range()) {
			Session::EndTimeChanged (old.samples()); /* EMIT SIGNAL */
		}
	}

	assert (_end.is_positive() || _end.is_zero());

	return 0;
}

int
Location::set (Temporal::timepos_t const & s, Temporal::timepos_t const & e)
{
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
			start_change = true;
			end_change = true;
		}

		assert (_start >= 0);
		assert (_end >= 0);

	} else {

		/* range locations must exceed a minimum duration */
		if (s.distance (e) < Config->get_range_location_minimum()) {
			return -1;
		}

		if (s != _start) {

			Temporal::timepos_t const old = _start;
			_start = s;
			start_change = true;

			if (is_session_range ()) {
				Session::StartTimeChanged (old.samples()); /* EMIT SIGNAL */
				AudioFileSource::set_header_position_offset (s.samples());
			}
		}


		if (e != _end) {

			Temporal::timepos_t const old = _end;
			_end = e;
			end_change = true;

			if (is_session_range()) {
				Session::EndTimeChanged (old.samples()); /* EMIT SIGNAL */
			}
		}

		assert (e.is_positive() || e.is_zero());
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

	if (is_cue_marker()) {
		cue_change (this);
	}

	return 0;
}

int
Location::move_to (Temporal::timepos_t const & pos)
{
	if (_locked) {
		return -1;
	}

	if (_start != pos) {
		const timecnt_t len = _start.distance (_end);
		_start = pos;
		_end = pos + len;

		changed (this); /* EMIT SIGNAL */
		Changed (); /* EMIT SIGNAL */

		if (is_cue_marker()) {
			cue_change (this);
		}
	}

	assert (_start >= 0);
	assert (_end >= 0);

	return 0;
}

void
Location::set_hidden (bool yn, void*)
{
	/* do not allow session range markers to be hidden */
	if (is_session_range()) {
		return;
	}

	if (set_flag_internal (yn, IsHidden)) {
		flags_changed (this); /* EMIT SIGNAL */
		FlagsChanged ();
	}
}

void
Location::set_cd (bool yn, void*)
{
	if (set_flag_internal (yn, IsCDMarker)) {
		flags_changed (this); /* EMIT SIGNAL */
		FlagsChanged ();
	}
}

void
Location::set_cue_id (int32_t cue_id)
{
	if (!is_cue_marker()) {
		return;
	}
	if (_cue != cue_id) {
		_cue = cue_id;
		cue_change (this); /* EMIT SIGNAL */
		CueChanged  (); /* EMIT SIGNAL */
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
	if (is_range_marker() && length().is_positive()) {
		if (set_flag_internal (yn, IsSkip)) {
			flags_changed (this);
			FlagsChanged ();
		}
	}
}

void
Location::set_skipping (bool yn)
{
	if (is_range_marker() && is_skip() && length().is_positive()) {
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
	node->set_property ("flags", _flags);
	node->set_property ("locked", _locked);
	node->set_property ("timestamp", _timestamp);
	node->set_property ("cue", _cue);
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
	node.get_property ("cue", _cue);

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

	XMLNode* scene_child = find_named_node (node, SceneChange::xml_node_name);

	if (scene_child) {
		_scene_change = SceneChange::factory (*scene_child, version);
	}

	changed (this); /* EMIT SIGNAL */
	Changed (); /* EMIT SIGNAL */

	assert (_start.is_positive() || _start.is_zero());
	assert (_end.is_positive() || _end.is_zero());

	return 0;
}

void
Location::set_position_time_domain (TimeDomain domain)
{
	if (_start.time_domain() == domain) {
		return;
	}

	_start.set_time_domain (domain);
	_end.set_time_domain (domain);

	TimeDomainChanged (); /* EMIT SIGNAL */
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
	Glib::Threads::RWLock::WriterLock lm (_lock);
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
		Glib::Threads::RWLock::ReaderLock lm (_lock);
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

bool
Locations::clear ()
{
	bool deleted = false;

	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		for (LocationList::iterator i = locations.begin(); i != locations.end(); ) {

			LocationList::iterator tmp = i;
			++tmp;

			if (!(*i)->is_session_range()) {
				delete *i;
				locations.erase (i);
				deleted = true;
			}

			i = tmp;
		}

		current_location = 0;
	}
	if (deleted) {
		changed (); /* EMIT SIGNAL */
		current_changed (0); /* EMIT SIGNAL */
	}

	return deleted;
}

bool
Locations::clear_markers ()
{
	bool deleted = false;

	{
		Glib::Threads::RWLock::WriterLock lm (_lock);
		LocationList::iterator tmp;

		for (LocationList::iterator i = locations.begin(); i != locations.end(); ) {
			tmp = i;
			++tmp;

			if ((*i)->is_mark() && !(*i)->is_session_range()) {
				delete *i;
				locations.erase (i);
				deleted = true;
			}

			i = tmp;
		}
	}

	if (deleted) {
		changed (); /* EMIT SIGNAL */
	}

	return deleted;
}

bool
Locations::clear_xrun_markers ()
{
	bool deleted = false;

	{
		Glib::Threads::RWLock::WriterLock lm (_lock);
		LocationList::iterator tmp;

		for (LocationList::iterator i = locations.begin(); i != locations.end(); ) {
			tmp = i;
			++tmp;

			if ((*i)->is_xrun()) {
				delete *i;
				locations.erase (i);
				deleted = true;
			}

			i = tmp;
		}
	}

	if (deleted) {
		changed (); /* EMIT SIGNAL */
	}

	return deleted;
}

bool
Locations::clear_ranges ()
{
	bool deleted = false;

	{
		Glib::Threads::RWLock::WriterLock lm (_lock);
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
				deleted = true;
			}

			i = tmp;
		}

		current_location = 0;
	}

	if (deleted) {
		changed (); /* EMIT SIGNAL */
		current_changed (0); /* EMIT SIGNAL */
	}

	return deleted;
}

void
Locations::add (Location *loc, bool make_current)
{
	assert (loc);

	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		/* Do not allow multiple cue markers in the same location */

		if (loc->is_cue_marker()) {
			for (LocationList::iterator i = locations.begin(); i != locations.end(); ++i) {
				if ((*i)->is_cue_marker() && (*i)->start() == loc->start()) {
					locations.erase (i);
					break;
				}
			}
		}

		locations.push_back (loc);

		if (make_current) {
			current_location = loc;
		}
	}

	added (loc); /* EMIT SIGNAL */

	if (loc->name().empty()) {
		string new_name;

		if (loc->is_cue_marker()) {
			next_available_name (new_name, _("cue"));
		} else if (loc->is_mark()) {
			next_available_name (new_name, _("mark"));
		} else {
			next_available_name (new_name, _("range"));
		}

		loc->set_name (new_name);
	}

	if (make_current) {
		current_changed (current_location); /* EMIT SIGNAL */
	}

	if (loc->is_session_range()) {
		Session::StartTimeChanged (0);
		Session::EndTimeChanged (1);
	}

	if (loc->is_cue_marker()) {
		Location::cue_change (loc);
	}
}

Location*
Locations::add_range (timepos_t const & start, timepos_t const &  end)
{
	string name;
	next_available_name(name, _("range"));

	Location* loc = new Location(_session, start, end, name, Location::IsRangeMarker);
	add(loc, false);

	return loc;
}

void
Locations::remove (Location *loc)
{
	bool was_removed = false;
	bool was_current = false;
	bool was_loop    = false;
	LocationList::iterator i;

	if (!loc) {
		return;
	}

	if (loc->is_session_range()) {
		return;
	}

	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		for (i = locations.begin(); i != locations.end(); ++i) {
			if ((*i) != loc) {
				continue;
			}
			was_loop = (*i)->is_auto_loop();
			if ((*i)->is_auto_punch()) {
				/* needs to happen before deleting:
				 * disconnect signals, clear events */
				lm.release ();
				_session.set_auto_punch_location (0);
				lm.acquire ();
			}
			delete *i;
			locations.erase (i);
			was_removed = true;
			if (current_location == loc) {
				current_location = 0;
				was_current = true;
			}
			break;
		}
	}

	if (was_removed) {

		if (was_loop) {
			if (_session.get_play_loop()) {
				_session.request_play_loop (false, false);
			}
			_session.auto_loop_location_changed (0);
		}

		removed (loc); /* EMIT SIGNAL */

		if (loc->is_cue_marker()) {
			Location::cue_change (loc);
		}

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
	Glib::Threads::RWLock::ReaderLock lm (_lock);

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

	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		current_location = 0;

		Location* session_range_location = 0;
		if (version < 3000) {
			session_range_location = new Location (_session, timepos_t (Temporal::AudioTime), timepos_t (Temporal::AudioTime), _("session"), Location::IsSessionRange);
			new_locations.push_back (session_range_location);
		}

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


typedef std::pair<timepos_t,Location*> LocationPair;

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

timepos_t
Locations::first_mark_before (timepos_t const & pos, bool include_special_ranges)
{
	vector<LocationPair> locs;
	{
		Glib::Threads::RWLock::ReaderLock lm (_lock);

		for (LocationList::iterator i = locations.begin(); i != locations.end(); ++i) {
			locs.push_back (make_pair ((*i)->start(), (*i)));
			if (!(*i)->is_mark()) {
				locs.push_back (make_pair ((*i)->end(), (*i)));
			}
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
		if ((*i).first < pos) {
			return (*i).first;
		}
	}

	return timepos_t::max (pos.time_domain());
}

Location*
Locations::mark_at (timepos_t const & pos, timecnt_t const & slop) const
{
	Location* closest = 0;
	timecnt_t mindelta = timecnt_t::max (pos.time_domain());
	timecnt_t delta;

	/* locations are not necessarily stored in linear time order so we have
	 * to iterate across all of them to find the one closest to a give point.
	 */

	Glib::Threads::RWLock::ReaderLock lm (_lock);
	for (LocationList::const_iterator i = locations.begin(); i != locations.end(); ++i) {

		if ((*i)->is_mark()) {
			if (pos > (*i)->start()) {
				delta = (*i)->start().distance (pos);
			} else {
				delta = pos.distance ((*i)->start());
			}

			if (slop.is_zero() && delta.is_zero()) {
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

timepos_t
Locations::first_mark_after (timepos_t const & pos, bool include_special_ranges)
{
	vector<LocationPair> locs;

	{
		Glib::Threads::RWLock::ReaderLock lm (_lock);

		for (LocationList::iterator i = locations.begin(); i != locations.end(); ++i) {
			locs.push_back (make_pair ((*i)->start(), (*i)));
			if (!(*i)->is_mark()) {
				locs.push_back (make_pair ((*i)->end(), (*i)));
			}
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
		if ((*i).first > pos) {
			return (*i).first;
		}
	}

	return timepos_t::max (pos.time_domain());
}

/** Look for the `marks' (either locations which are marks, or start/end points of range markers) either
 *  side of a sample.  Note that if sample is exactly on a `mark', that mark will not be considered for returning
 *  as before/after.
 *  @param pos position to be used
 *  @param before Filled in with the position of the last `mark' before `pos' (or max_timepos if none exists)
 *  @param after Filled in with the position of the next `mark' after `pos' (or max_timepos if none exists)
 */
void
Locations::marks_either_side (timepos_t const & pos, timepos_t& before, timepos_t& after) const
{
	before = after = timepos_t::max (pos.time_domain());

	LocationList locs;

	{
		Glib::Threads::RWLock::ReaderLock lm (_lock);
		locs = locations;
	}

	/* Get a list of positions; don't store any that are exactly on our requested position */

	std::list<timepos_t> positions;

	for (LocationList::const_iterator i = locs.begin(); i != locs.end(); ++i) {
		if (((*i)->is_auto_loop() || (*i)->is_auto_punch()) || (*i)->is_xrun() || (*i)->is_cue_marker()) {
			continue;
		}

		if (!(*i)->is_hidden()) {
			if ((*i)->is_mark ()) {
				if ((*i)->start() != pos) {
					positions.push_back ((*i)->start ());
				}
			} else {
				if ((*i)->start() != pos) {
					positions.push_back ((*i)->start ());
				}
				if ((*i)->end() != pos) {
					positions.push_back ((*i)->end ());
				}
			}
		}
	}

	if (positions.empty ()) {
		return;
	}

	positions.sort ();

	std::list<timepos_t>::iterator i = positions.begin ();

	while (i != positions.end () && *i < pos) {
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
	Glib::Threads::RWLock::ReaderLock lm (_lock);
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
	Glib::Threads::RWLock::ReaderLock lm (_lock);
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
	Glib::Threads::RWLock::ReaderLock lm (_lock);
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
	Location* sr = 0;
	Glib::Threads::RWLock::ReaderLock lm (_lock);
	for (LocationList::const_iterator i = locations.begin(); i != locations.end(); ++i) {
		if ((*i)->is_clock_origin()) {
			return const_cast<Location*> (*i);
		}
		if ((*i)->is_session_range()) {
			sr = const_cast<Location*> (*i);
		}
	}
	/* fall back to session_range_location () */
	return sr;
}

uint32_t
Locations::num_range_markers () const
{
	uint32_t cnt = 0;
	Glib::Threads::RWLock::ReaderLock lm (_lock);
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
	Glib::Threads::RWLock::ReaderLock lm (_lock);
	for (LocationList::const_iterator i  = locations.begin(); i != locations.end(); ++i) {
		if (id == (*i)->id()) {
			return const_cast<Location*> (*i);
		}
	}
	return 0;
}

void
Locations::find_all_between (timepos_t const & start, timepos_t const & end, LocationList& ll, Location::Flags flags)
{
	Glib::Threads::RWLock::ReaderLock lm (_lock);
	for (LocationList::const_iterator i = locations.begin(); i != locations.end(); ++i) {
		if ((flags == 0 || (*i)->matches (flags)) &&
		    ((*i)->start() >= start && (*i)->end() < end)) {
			ll.push_back (*i);
		}
	}
}

Location *
Locations::range_starts_at (timepos_t const & pos, timecnt_t const & slop, bool incl) const
{
	Location *closest = 0;
	timecnt_t mindelta = timecnt_t (pos.time_domain());

	Glib::Threads::RWLock::ReaderLock lm (_lock);
	for (LocationList::const_iterator i = locations.begin(); i != locations.end(); ++i) {
		if (!(*i)->is_range_marker()) {
			continue;
		}

		if (incl && (pos < (*i)->start() || pos > (*i)->end())) {
			continue;
		}

		timecnt_t delta = (*i)->start().distance (pos).abs ();

		if (delta.is_zero()) {
			return *i;
		}

		if (delta > slop) {
			continue;
		}

		if (delta < mindelta) {
			closest = *i;
			mindelta = delta;
		}
	}

	return closest;
}

void
Locations::ripple (timepos_t const & at, timecnt_t const & distance, bool include_locked, bool notify)
{
	LocationList copy;

	{
		Glib::Threads::RWLock::WriterLock lm (_lock);
		copy = locations;
	}

	for (LocationList::iterator i = copy.begin(); i != copy.end(); ++i) {

		/* keep session range markers covering entire region if
		   a ripple "extends" the session.
		*/
		if (distance.is_positive() && (*i)->is_session_range()) {

			/* Don't move start unless it occurs after the ripple point.
			 */
			if ((*i)->start() >= at) {
				(*i)->set ((*i)->start() + distance, (*i)->end() + distance);
			} else {
				(*i)->set_end ((*i)->end() + distance);
			}
			continue;
		}

		bool locked = (*i)->locked();

		if (locked) {
			if (!include_locked) {
				continue;
			}
		} else {
			(*i)->unlock ();
		}

		if ((*i)->start() >= at) {
			(*i)->set_start ((*i)->start().earlier (distance));

			if (!(*i)->is_mark()) {
				(*i)->set_end ((*i)->end().earlier (distance));
			}
		}

		if (locked) {
			(*i)->lock();
		}
	}

	if (notify) {
		changed(); /* EMIT SIGNAL */
	}
}

void
Locations::clear_cue_markers (samplepos_t start, samplepos_t end)
{
	TempoMap::SharedPtr tmap (TempoMap::use());
	Temporal::Beats sb;
	Temporal::Beats eb;
	bool have_beats = false;
	vector<Location*> r;


	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		for (LocationList::iterator i = locations.begin(); i != locations.end(); ) {

			if ((*i)->is_cue_marker()) {
				Location* l (*i);

				if (l->start().time_domain() == AudioTime) {
					samplepos_t when = l->start().samples();
					if (when >= start && when < end) {
						i = locations.erase (i);
						r.push_back (l);
						continue;
					}
				} else {
					if (!have_beats) {
						sb = tmap->quarters_at (timepos_t (start));
						eb = tmap->quarters_at (timepos_t (end));
						have_beats = true;
					}

					Temporal::Beats when = l->start().beats();
					if (when >= sb && when < eb) {
						r.push_back (l);
						i = locations.erase (i);
						continue;
					}
				}
			}

			++i;
		}
	} /* end lock scope */

	for (auto & l : r) {
		removed (l); /* EMIT SIGNAL */
		delete l;
	}
}
