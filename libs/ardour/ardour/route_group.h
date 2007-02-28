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

#ifndef __ardour_route_group_h__
#define __ardour_route_group_h__

#include <list>
#include <set>
#include <string>
#include <stdint.h>
#include <sigc++/signal.h>
#include <pbd/stateful.h> 
#include <ardour/types.h>

using std::string;
using std::list;

namespace ARDOUR {

class Route;
class Track;
class AudioTrack;
class Session;

class RouteGroup : public Stateful, public sigc::trackable {
  public:
    enum Flag {
	    Relative = 0x1,
	    Active = 0x2,
	    Hidden = 0x4
    };

    RouteGroup (Session& s, const string &n, Flag f = Flag(0));

    const string& name() { return _name; }
    void set_name (std::string str);

    bool is_active () const { return _flags & Active; }
    bool is_relative () const { return _flags & Relative; }
    bool is_hidden () const { return _flags & Hidden; }
    bool empty() const {return routes.empty();}

    gain_t get_max_factor(gain_t factor);
    gain_t get_min_factor(gain_t factor);
    
    int size() { return routes.size();}
    ARDOUR::Route * first () const { return *routes.begin();}

    void set_active (bool yn, void *src);
    void set_relative (bool yn, void *src);
    void set_hidden (bool yn, void *src);

    int add (Route *);

    int remove (Route *);

    void apply (void (Route::*func)(void *), void *src) {
	    for (list<Route *>::iterator i = routes.begin(); i != routes.end(); i++) {
		    ((*i)->*func)(src);
	    }
    }

    template<class T> void apply (void (Route::*func)(T, void *), T val, void *src) {
	    for (list<Route *>::iterator i = routes.begin(); i != routes.end(); i++) {
		    ((*i)->*func)(val, src);
	    }
    }

    template<class T> void foreach_route (T *obj, void (T::*func)(Route&)) {
	    for (list<Route *>::iterator i = routes.begin(); i != routes.end(); i++) {
		    (obj->*func)(**i);
	    }
    }

    /* to use these, #include <ardour/route_group_specialized.h> */

    template<class T> void apply (void (Track::*func)(T, void *), T val, void *src);

    /* fills at_set with all members of the group that are AudioTracks */

    void audio_track_group (std::set<AudioTrack*>& at_set);

    void clear () {
	    routes.clear ();
	    changed();
    }

    const list<Route*>& route_list() { return routes; }
    
    sigc::signal<void> changed;
    sigc::signal<void,void*> FlagsChanged;

    XMLNode& get_state (void);

    int set_state (const XMLNode&);

 private:
    Session& _session;
    list<Route *> routes;
    string _name;
    Flag _flags;

    void remove_when_going_away (Route*);
};

} /* namespace */

#endif /* __ardour_route_group_h__ */
