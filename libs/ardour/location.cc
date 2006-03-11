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

    $Id$
*/

#include <algorithm>
#include <set>
#include <cstdio> /* for sprintf */
#include <unistd.h>
#include <cerrno>
#include <ctime>
#include <sigc++/bind.h>

#include <pbd/stl_delete.h>
#include <pbd/xml++.h>

#include <ardour/location.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace sigc;

Location::Location (const Location& other)
	: _name (other._name),
	  _start (other._start),
	  _end (other._end),
	  _flags (other._flags)
{
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

	/* "changed" not emitted on purpose */
	
	return this;
}

int
Location::set_start (jack_nframes_t s)
{
	if (is_mark()) {
		if (_start != s) {
			_start = s;
			_end = s;
			start_changed(this); /* EMIT SIGNAL */
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
Location::set_end (jack_nframes_t e)
{
	if (is_mark()) {
		if (_start != e) {
			_start = e;
			_end = e;
			end_changed(this); /* EMIT SIGNAL */
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
Location::set (jack_nframes_t start, jack_nframes_t end)
{
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
			_flags |= flag;
			return true;
		}
	} else {
		if (_flags & flag) {
			_flags &= ~flag;
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
	char buf[32];

	typedef map<string, string>::const_iterator CI;
	for(CI m = cd_info.begin(); m != cd_info.end(); ++m){
		node->add_child_nocopy(cd_info_node(m->first, m->second));
	}

	node->add_property ("name", name());
	snprintf (buf, sizeof (buf), "%u", start());
	node->add_property ("start", buf);
	snprintf (buf, sizeof (buf), "%u", end());
	node->add_property ("end", buf);
	snprintf (buf, sizeof (buf), "%" PRIu32, (uint32_t) _flags);
	node->add_property ("flags", buf);

	return *node;
}

int
Location::set_state (const XMLNode& node)
{
	XMLPropertyList plist;
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

	plist = node.properties();
		
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
		
	_flags = 0;
		
	if ((prop = node.property ("flags")) == 0) {
		  error << _("XML node for Location has no flags information") << endmsg; 
		  return -1;
	}
		
	_flags = Flags (atoi (prop->value().c_str()));

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
	save_state (_("initial"));
}

Locations::~Locations () 
{
	std::set<Location*> all_locations;
	
	for (StateMap::iterator siter = states.begin(); siter != states.end(); ++siter) {

		State* lstate = dynamic_cast<State*> (*siter);

		for (LocationList::iterator liter = lstate->locations.begin(); liter != lstate->locations.end(); ++liter) {
			all_locations.insert (*liter);
		}

		for (LocationList::iterator siter = lstate->states.begin(); siter != lstate->states.end(); ++siter) {
			all_locations.insert (*siter);
		}
	}

	set_delete (&all_locations);
}

int
Locations::set_current (Location *loc, bool want_lock)

{
	int ret;

	if (want_lock) {
		LockMonitor lm (lock, __LINE__, __FILE__);
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
		LockMonitor lm (lock, __LINE__, __FILE__);
		LocationList::iterator tmp;
		for (LocationList::iterator i = locations.begin(); i != locations.end(); ) {
			tmp = i;
			++tmp;
			if (!(*i)->is_end()) {
				locations.erase (i);
			}
			i = tmp;
		}

		locations.clear ();
		current_location = 0;
	}

	save_state (_("clear"));
	
	changed (); /* EMIT SIGNAL */
	current_changed (0); /* EMIT SIGNAL */
}	

void
Locations::clear_markers ()
{
	{
		LockMonitor lm (lock, __LINE__, __FILE__);
		LocationList::iterator tmp;

		for (LocationList::iterator i = locations.begin(); i != locations.end(); ) {
			tmp = i;
			++tmp;

			if ((*i)->is_mark() && !(*i)->is_end()) {
				locations.erase (i);
			}

			i = tmp;
		}
	}

	save_state (_("clear markers"));
	
	changed (); /* EMIT SIGNAL */
}	

void
Locations::clear_ranges ()
{
	{
		LockMonitor lm (lock, __LINE__, __FILE__);
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

	save_state (_("clear ranges"));

	changed (); /* EMIT SIGNAL */
	current_changed (0); /* EMIT SIGNAL */
}	

void
Locations::add (Location *loc, bool make_current)
{
	{
		LockMonitor lm (lock, __LINE__, __FILE__);
		locations.push_back (loc);

		if (make_current) {
			current_location = loc;
		}
	}
	
	save_state (_("add"));

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

	if (loc->is_end()) {
		return;
	}

	{
		LockMonitor lm (lock, __LINE__, __FILE__);

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
		save_state (_("remove"));

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
	save_state (X_("location changed"));
	changed (); /* EMIT SIGNAL */
}

XMLNode&
Locations::get_state ()
{
	XMLNode *node = new XMLNode ("Locations");
	LocationList::iterator iter;
	LockMonitor lm (lock, __LINE__, __FILE__);
       
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
	
	{
		LockMonitor lm (lock, __LINE__, __FILE__);

		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
			Location *loc = new Location;
			
			if (loc->set_state (**niter)) {
				delete loc;
			} else {
				locations.push_back (loc);
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
Locations::first_location_before (jack_nframes_t frame)
{
	LocationList locs;

	{
		LockMonitor lm (lock, __LINE__, __FILE__);
		locs = locations;
	}

	LocationStartLaterComparison cmp;
	locs.sort (cmp);

	/* locs is now sorted latest..earliest */
	
	for (LocationList::iterator i = locs.begin(); i != locs.end(); ++i) {
		if (!(*i)->is_hidden() && (*i)->start() < frame) {
			return (*i);
		}
	}

	return 0;
}

Location *
Locations::first_location_after (jack_nframes_t frame)
{
	LocationList locs;

	{
		LockMonitor lm (lock, __LINE__, __FILE__);
		locs = locations;
	}

	LocationStartEarlierComparison cmp;
	locs.sort (cmp);

	/* locs is now sorted earliest..latest */
	
	for (LocationList::iterator i = locs.begin(); i != locs.end(); ++i) {
		if (!(*i)->is_hidden() && (*i)->start() > frame) {
			return (*i);
		}
	}

	return 0;
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

StateManager::State*
Locations::state_factory (std::string why) const
{
	State* state = new State (why);

	state->locations = locations;
	
	for (LocationList::const_iterator i = locations.begin(); i != locations.end(); ++i) {
		state->states.push_back (new Location (**i));
	}

	return state;
}

Change
Locations::restore_state (StateManager::State& state) 
{
	{
		LockMonitor lm (lock, __LINE__, __FILE__);
		State* lstate = dynamic_cast<State*> (&state);

		locations = lstate->locations;
		LocationList& states = lstate->states;
		LocationList::iterator l, s;

		for (l = locations.begin(), s = states.begin(); s != states.end(); ++s, ++l) {
			(*l) = (*s);
		}
	}

	return Change (0);
}

UndoAction
Locations::get_memento () const
{
  return sigc::bind (mem_fun (*(const_cast<Locations*> (this)), &StateManager::use_state), _current_state_id);
}

uint32_t
Locations::num_range_markers () const
{
	uint32_t cnt = 0;
	LockMonitor lm (lock, __LINE__, __FILE__);
	for (LocationList::const_iterator i = locations.begin(); i != locations.end(); ++i) {
		if ((*i)->is_range_marker()) {
			++cnt;
		}
	}
	return cnt;
}
