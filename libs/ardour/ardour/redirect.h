/*
    Copyright (C) 2001 Paul Davis 

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

#ifndef __ardour_redirect_h__
#define __ardour_redirect_h__

#include <string>
#include <vector>
#include <set>
#include <map>
#include <sigc++/signal.h>

#include <pbd/lockmonitor.h>
#include <pbd/undo.h>

#include <ardour/ardour.h>
#include <ardour/io.h>
#include <ardour/automation_event.h>

using std::map;
using std::set;
using std::string;
using std::vector;

class XMLNode;

namespace ARDOUR {

class Session;

struct RedirectState : public StateManager::State {
    RedirectState (string why) 
	    : StateManager::State (why) {}
    ~RedirectState () {}

    bool active;
};

class Redirect : public IO
{
  public:
	static const string state_node_name;

	Redirect (Session&, const string& name, Placement,
		  int input_min = -1, int input_max = -1, int output_min = -1, int output_max = -1);
	Redirect (const Redirect&);
	virtual ~Redirect ();

	static Redirect *clone (const Redirect&);

	bool active () const { return _active; }
	void set_active (bool yn, void *src);

	virtual uint32_t output_streams() const { return n_outputs(); }
	virtual uint32_t input_streams () const { return n_inputs(); }
	virtual uint32_t natural_output_streams() const { return n_outputs(); }
	virtual uint32_t natural_input_streams () const { return n_inputs(); }

	uint32_t sort_key() const { return _sort_key; }
	void set_sort_key (uint32_t key);

	Placement placement() const { return _placement; }
	void set_placement (Placement, void *src);

	virtual void run (vector<Sample *>& ibufs, uint32_t nbufs, jack_nframes_t nframes, jack_nframes_t offset) = 0;
	virtual void activate () = 0;
	virtual void deactivate () = 0;
	virtual jack_nframes_t latency() { return 0; }

	virtual void set_block_size (jack_nframes_t nframes) {}

	sigc::signal<void,Redirect*,void*> active_changed;
	sigc::signal<void,Redirect*,void*> placement_changed;
	sigc::signal<void,Redirect*,bool>  AutomationPlaybackChanged;
	sigc::signal<void,Redirect*,uint32_t> AutomationChanged;
	sigc::signal<void,Redirect*> GoingAway;

	static sigc::signal<void,Redirect*> RedirectCreated;
	
	XMLNode& state (bool full);
	XMLNode& get_state (void);
	int set_state (const XMLNode&);

	StateManager::State* state_factory (string why) const;
	Change restore_state (StateManager::State&);

	void *get_gui () const { return _gui; }
	void  set_gui (void *p) { _gui = p; }

	virtual string describe_parameter (uint32_t which);
	virtual float default_parameter_value (uint32_t which) {
		return 1.0f;
	}

	int load_automation (string path);
	int save_automation (string path);

	void what_has_automation (set<uint32_t>&) const;
	void what_has_visible_automation (set<uint32_t>&) const;
	const set<uint32_t>& what_can_be_automated () const { return can_automate_list; }

	void mark_automation_visible (uint32_t, bool);
	
	AutomationList& automation_list (uint32_t);
	bool find_next_event (jack_nframes_t, jack_nframes_t, ControlEvent&) const;

	virtual void transport_stopped (jack_nframes_t frame) {};
	
  protected:
	void set_placement (const string&, void *src);

	/* children may use this stuff as they see fit */

	map<uint32_t,AutomationList*> parameter_automation;
	set<uint32_t> visible_parameter_automation;

	mutable PBD::NonBlockingLock _automation_lock;

	void can_automate (uint32_t);
	set<uint32_t> can_automate_list;

	void store_state (RedirectState&) const;

	virtual void automation_list_creation_callback (uint32_t, AutomationList&) {}

  private:
	bool _active;
	Placement _placement;
	uint32_t _sort_key;
	void* _gui;  /* generic, we don't know or care what this is */
};

}; /* namespace ARDOUR */

#endif /* __ardour_redirect_h__ */
