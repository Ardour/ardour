/*
    Copyright (C) 2000 Paul Davis 

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

#include <algorithm>
#include <set>
#include <cstdio> /* for sprintf */
#include <unistd.h>
#include <cerrno>
#include <ctime>
#include <sigc++/bind.h>

#include "pbd/stl_delete.h"
#include "pbd/xml++.h"
#include "pbd/enumwriter.h"

#include "ardour/location.h"
#include "ardour/session.h"
#include "ardour/audiofilesource.h"

#include "i18n.h"

#define SUFFIX_MAX 32

using namespace std;
using namespace ARDOUR;
using namespace sigc;
using namespace PBD;

Location::Location (const Location& other)
	: _name (other._name),
	  _start (other._start),
	  _end (other._end),
	  _flags (other._flags)
{
	/* start and end flags can never be copied, because there can only ever be one of each */

	_flags = Flags (_flags & ~IsStart);
	_flags = Flags (_flags & ~IsEnd);

	/* copy is not locked even if original was */

	_locked = false;
}

Location::Location (const XMLNode& node)
{
	if (set_state (node)) {
		throw failed_constructor ();
	}
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

	/* copy is not locked even if original was */

	_locked = false;

	/* "changed" not emitted on purpose */
	
	return this;
}

int
Location::set_start (nframes_t s)
{
	if (_locked) {
		return -1;
	}

	if (is_mark()) {
		if (_start != s) {

			_start = s;
			_end = s;

			start_changed(this); /* EMIT SIGNAL */
			end_changed(this); /* EMIT SIGNAL */

			if ( is_start() ) {

				Session::StartTimeChanged (); /* EMIT SIGNAL */
				AudioFileSource::set_header_position_offset ( s );
			}

			if ( is_end() ) {
				Session::EndTimeChanged (); /* EMIT SIGNAL */
			}
		}
		return 0;
	}

	if (((is_auto_punch() || is_auto_loop()) && s >= _end) || s > _end) {
		return -1;
	}

	if (s != _start) {
		_start = s; 
		start_changed(this); /* EMIT SIGNAL */
	}

	return 0;
}

int
Location::set_end (nframes_t e)
{
	if (_locked) {
		return -1;
	}

	if (is_mark()) {
		if (_start != e) {
			_start = e;
			_end = e;
			start_changed(this); /* EMIT SIGNAL */
			end_changed(this); /* EMIT SIGNAL */

			if ( is_start() ) {
				Session::StartTimeChanged (); /* EMIT SIGNAL */
			}

			if ( is_end() ) {
				Session::EndTimeChanged (); /* EMIT SIGNAL */
			}

		}
		return 0;
	}

	if (((is_auto_punch() || is_auto_loop()) && e <= _start) || e < _start) {
		return -1;
	}

	if (e != _end) {
		_end = e; 
		 end_changed(this); /* EMIT SIGNAL */
	}
	return 0;
}

int
Location::set (nframes_t start, nframes_t end)
{
	if (_locked) {
		return -1;
	}

	if (is_mark() && start != end) {
		return -1;
	} else if (((is_auto_punch() || is_auto_loop()) && start >= end) || (start > end)) {
		return -1;
	}
	
	if (_start != start) {
		_start = start;
		start_changed(this); /* EMIT SIGNAL */
	}

	if (_end != end) {
		_end = end;
		end_changed(this); /* EMIT SIGNAL */
	}
	return 0;
}

int
Location::move_to (nframes_t pos) 
{
	if (_locked) {
		return -1;
	}

	if (_start != pos) {
		_start = pos;
		_end = _start + length();
		
		changed (this); /* EMIT SIGNAL */
	}
	
	return 0;
}

void
Location::set_hidden (bool yn, void *src)
{
	if (set_flag_internal (yn, IsHidden)) {
		 FlagsChanged (this, src); /* EMIT SIGNAL */
	}
}

void
Location::set_cd (bool yn, void *src)
{
	// XXX this really needs to be session start
	// but its not available here - leave to GUI

	if (_start == 0) {
		error << _("You cannot put a CD marker at this position") << endmsg;
		return;
	}

	if (set_flag_internal (yn, IsCDMarker)) {
		 FlagsChanged (this, src); /* EMIT SIGNAL */
	}
}

void
Location::set_is_end (bool yn, void *src)
{
	if (set_flag_internal (yn, IsEnd)) {
		 FlagsChanged (this, src); /* EMIT SIGNAL */
	}
}

void
Location::set_is_start (bool yn, void *src)
{
	if (set_flag_internal (yn, IsStart)) {
		 FlagsChanged (this, src); /* EMIT SIGNAL */
	}
}

void
Location::set_is_range_marker (bool yn, void *src)
{
       if (set_flag_internal (yn, IsRangeMarker)) {
                FlagsChanged (this, src); /* EMIT SIGNAL */
       }
}

void
Location::set_auto_punch (bool yn, void *src) 
{
	if (is_mark() || _start == _end) {
		return;
	}

	if (set_flag_internal (yn, IsAutoPunch)) {
		 FlagsChanged (this, src); /* EMIT SIGNAL */
	}
}

void
Location::set_auto_loop (bool yn, void *src) 
{
	if (is_mark() || _start == _end) {
		return;
	}

	if (set_flag_internal (yn, IsAutoLoop)) {
		 FlagsChanged (this, src); /* EMIT SIGNAL */
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

	root->add_property("name", name);
	root->add_property("value", value);
	
	return *root;
}

 
XMLNode&
Location::get_state (void)
{
	XMLNode *node = new XMLNode ("Location");
	char buf[64];

	typedef map<string, string>::const_iterator CI;

	for(CI m = cd_info.begin(); m != cd_info.end(); ++m){
		node->add_child_nocopy(cd_info_node(m->first, m->second));
	}

	id().print (buf, sizeof (buf));
	node->add_property("id", buf);
	node->add_property ("name", name());
	snprintf (buf, sizeof (buf), "%u", start());
	node->add_property ("start", buf);
	snprintf (buf, sizeof (buf), "%u", end());
	node->add_property ("end", buf);
	node->add_property ("flags", enum_2_string (_flags));
	node->add_property ("locked", (_locked ? "yes" : "no"));

	return *node;
}

int
Location::set_state (const XMLNode& node)
{
	const XMLProperty *prop;

	XMLNodeList cd_list = node.children();
	XMLNodeConstIterator cd_iter;
	XMLNode *cd_node;
	
	string cd_name;
	string cd_value;

	if (node.name() != "Location") {
		error << _("incorrect XML node passed to Location::set_state") << endmsg;
		return -1;
	}

	if ((prop = node.property ("id")) == 0) {
		warning << _("XML node for Location has no ID information") << endmsg;
	} else {
		_id = prop->value ();
	}

	if ((prop = node.property ("name")) == 0) {
		error << _("XML node for Location has no name information") << endmsg;
		return -1;
	}
		
	set_name (prop->value());
		
	if ((prop = node.property ("start")) == 0) {
		error << _("XML node for Location has no start information") << endmsg; 
		return -1;
	}
		
		/* can't use set_start() here, because _end
		   may make the value of _start illegal.
		*/
		
	_start = atoi (prop->value().c_str());
		
	if ((prop = node.property ("end")) == 0) {
		  error << _("XML node for Location has no end information") << endmsg; 
		  return -1;
	}
		
	_end = atoi (prop->value().c_str());
		
	if ((prop = node.property ("flags")) == 0) {
		  error << _("XML node for Location has no flags information") << endmsg; 
		  return -1;
	}
		
	_flags = Flags (string_2_enum (prop->value(), _flags));

	if ((prop = node.property ("locked")) != 0) {
		_locked = (prop->value() == "yes");
	} else {
		_locked = false;
	}

	for (cd_iter = cd_list.begin(); cd_iter != cd_list.end(); ++cd_iter) {
		  
		  cd_node = *cd_iter;
		  
		  if (cd_node->name() != "CD-Info") {
		    continue;
		  }
		  
		  if ((prop = cd_node->property ("name")) != 0) {
		    cd_name = prop->value();
		  } else {
		    throw failed_constructor ();
		  }
		  
		  if ((prop = cd_node->property ("value")) != 0) {
		    cd_value = prop->value();
		  } else {
		    throw failed_constructor ();
		  }
		  
		  
		  cd_info[cd_name] = cd_value;
		  
	}

	changed(this); /* EMIT SIGNAL */
		
	return 0;
}

/*---------------------------------------------------------------------- */

Locations::Locations ()

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
		Glib::Mutex::Lock lm (lock);
		ret = set_current_unlocked (loc);
	} else {
		ret = set_current_unlocked (loc);
	}

	if (ret == 0) {
		 current_changed (current_location); /* EMIT SIGNAL */
	}
	return ret;
}

int
Locations::next_available_name(string& result,string base)
{
	LocationList::iterator i;
	Location* location;
	string temp;
	string::size_type l;
	int suffix;
	char buf[32];
	bool available[SUFFIX_MAX+1];

	result = base;
	for (int k=1; k<SUFFIX_MAX; k++) {
		available[k] = true;
	}
	l = base.length();
	for (i = locations.begin(); i != locations.end(); ++i) {
		location =* i;
		temp = location->name();
		if (l && !temp.find(base,0)) {
			suffix = atoi(temp.substr(l,3).c_str());
			if (suffix) available[suffix] = false;
		}
	}
	for (int k=1; k<=SUFFIX_MAX; k++) {
		if (available[k]) { 
			snprintf (buf, 31, "%d", k);
			result += buf;
			return 1;
		}
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
		Glib::Mutex::Lock lm (lock);

		for (LocationList::iterator i = locations.begin(); i != locations.end(); ) {

			LocationList::iterator tmp = i;
			++tmp;

			if (!(*i)->is_end() && !(*i)->is_start()) {
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
		Glib::Mutex::Lock lm (lock);
		LocationList::iterator tmp;

		for (LocationList::iterator i = locations.begin(); i != locations.end(); ) {
			tmp = i;
			++tmp;

			if ((*i)->is_mark() && !(*i)->is_end() && !(*i)->is_start()) {
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
		Glib::Mutex::Lock lm (lock);
		LocationList::iterator tmp;
		
		for (LocationList::iterator i = locations.begin(); i != locations.end(); ) {

			tmp = i;
			++tmp;

			if (!(*i)->is_mark()) {
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
Locations::add (Location *loc, bool make_current)
{
	{
		Glib::Mutex::Lock lm (lock);
		locations.push_back (loc);

		if (make_current) {
			current_location = loc;
		}
	}
	
	added (loc); /* EMIT SIGNAL */

	if (make_current) {
		 current_changed (current_location); /* EMIT SIGNAL */
	} 
}

void
Locations::remove (Location *loc)

{
	bool was_removed = false;
	bool was_current = false;
	LocationList::iterator i;

	if (loc->is_end() || loc->is_start()) {
		return;
	}

	{
		Glib::Mutex::Lock lm (lock);

		for (i = locations.begin(); i != locations.end(); ++i) {
			if ((*i) == loc) {
				locations.erase (i);
				was_removed = true;
				if (current_location == loc) {
					current_location = 0;
					was_current = true;
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

		changed (); /* EMIT_SIGNAL */
	}
}

void
Locations::location_changed (Location* loc)
{
	changed (); /* EMIT SIGNAL */
}

XMLNode&
Locations::get_state ()
{
	XMLNode *node = new XMLNode ("Locations");
	LocationList::iterator iter;
	Glib::Mutex::Lock lm (lock);
       
	for (iter  = locations.begin(); iter != locations.end(); ++iter) {
		node->add_child_nocopy ((*iter)->get_state ());
	}

	return *node;
}	

int
Locations::set_state (const XMLNode& node)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;

	if (node.name() != "Locations") {
		error << _("incorrect XML mode passed to Locations::set_state") << endmsg;
		return -1;
	}
	
	nlist = node.children();

	locations.clear ();
	current_location = 0;

	{
		Glib::Mutex::Lock lm (lock);

		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
			
			try {

				Location *loc = new Location (**niter);
				locations.push_back (loc);
			}

			catch (failed_constructor& err) {
				error << _("could not load location from session file - ignored") << endmsg;
			}
		}
		
		if (locations.size()) {

			current_location = locations.front();
		} else {
			current_location = 0;
		}
	}

	changed (); /* EMIT SIGNAL */
	 
	return 0;
}	

struct LocationStartEarlierComparison 
{
    bool operator() (Location *a, Location *b) {
	return a->start() < b->start();
    }
};

struct LocationStartLaterComparison 
{
    bool operator() (Location *a, Location *b) {
	return a->start() > b->start();
    }
};

Location *
Locations::first_location_before (nframes_t frame, bool include_special_ranges)
{
	LocationList locs;

	{
		Glib::Mutex::Lock lm (lock);
		locs = locations;
	}

	LocationStartLaterComparison cmp;
	locs.sort (cmp);

	/* locs is now sorted latest..earliest */
	
	for (LocationList::iterator i = locs.begin(); i != locs.end(); ++i) {
		if (!include_special_ranges && ((*i)->is_auto_loop() || (*i)->is_auto_punch())) {
			continue;
		}
		if (!(*i)->is_hidden() && (*i)->start() < frame) {
			return (*i);
		}
	}

	return 0;
}

Location *
Locations::first_location_after (nframes_t frame, bool include_special_ranges)
{
	LocationList locs;

	{
		Glib::Mutex::Lock lm (lock);
		locs = locations;
	}

	LocationStartEarlierComparison cmp;
	locs.sort (cmp);

	/* locs is now sorted earliest..latest */
	
	for (LocationList::iterator i = locs.begin(); i != locs.end(); ++i) {
		if (!include_special_ranges && ((*i)->is_auto_loop() || (*i)->is_auto_punch())) {
			continue;
		}
		if (!(*i)->is_hidden() && (*i)->start() > frame) {
			return (*i);
		}
	}

	return 0;
}

nframes_t
Locations::first_mark_before (nframes_t frame, bool include_special_ranges)
{
	LocationList locs;

	{
        Glib::Mutex::Lock lm (lock);
		locs = locations;
	}

	LocationStartLaterComparison cmp;
	locs.sort (cmp);

	/* locs is now sorted latest..earliest */
	
	for (LocationList::iterator i = locs.begin(); i != locs.end(); ++i) {
		if (!include_special_ranges && ((*i)->is_auto_loop() || (*i)->is_auto_punch())) {
			continue;
		}
		if (!(*i)->is_hidden()) {
			if ((*i)->is_mark()) {
				/* MARK: start == end */
				if ((*i)->start() < frame) {
					return (*i)->start();
				}
			} else {
				/* RANGE: start != end, compare start and end */
				if ((*i)->end() < frame) {
					return (*i)->end();
				}
				if ((*i)->start () < frame) {
					return (*i)->start();
				}
			}
		}
	}

	return 0;
}

nframes_t
Locations::first_mark_after (nframes_t frame, bool include_special_ranges)
{
	LocationList locs;

	{
        Glib::Mutex::Lock lm (lock);
		locs = locations;
	}

	LocationStartEarlierComparison cmp;
	locs.sort (cmp);

	/* locs is now sorted earliest..latest */
	
	for (LocationList::iterator i = locs.begin(); i != locs.end(); ++i) {
		if (!include_special_ranges && ((*i)->is_auto_loop() || (*i)->is_auto_punch())) {
			continue;
		}
		if (!(*i)->is_hidden()) {
			if ((*i)->is_mark()) {
				/* MARK, start == end so just compare start */
				if ((*i)->start() > frame) {
					return (*i)->start();
				}
			} else {
				/* RANGE, start != end, compare start and end */
				if ((*i)->start() > frame ) {
					return (*i)->start ();
				}
				if ((*i)->end() > frame) {
					return (*i)->end ();
				}
			}
		}
	}

	return max_frames;
}

Location*
Locations::end_location () const
{
	for (LocationList::const_iterator i = locations.begin(); i != locations.end(); ++i) {
		if ((*i)->is_end()) {
			return const_cast<Location*> (*i);
		}
	}
	return 0;
}	

Location*
Locations::start_location () const
{
	for (LocationList::const_iterator i = locations.begin(); i != locations.end(); ++i) {
		if ((*i)->is_start()) {
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

uint32_t
Locations::num_range_markers () const
{
	uint32_t cnt = 0;
	Glib::Mutex::Lock lm (lock);
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
    for (it  = locations.begin(); it != locations.end(); it++)
        if (id == (*it)->id())
            return *it;

    return 0;
}

void
Locations::find_all_between (nframes64_t start, nframes64_t end, LocationList& ll, Location::Flags flags)
{
	Glib::Mutex::Lock lm (lock);

	for (LocationList::const_iterator i = locations.begin(); i != locations.end(); ++i) {
		if ((flags == 0 || (*i)->matches (flags)) && 
		    ((*i)->start() >= start && (*i)->end() < end)) {
			ll.push_back (*i);
		}
	}
}
