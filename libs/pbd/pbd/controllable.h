/*
    Copyright (C) 2000-2007 Paul Davis 

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

#ifndef __pbd_controllable_h__
#define __pbd_controllable_h__

#include <string>
#include <set>

#include <sigc++/trackable.h>
#include <sigc++/signal.h>

#include <pbd/statefuldestructible.h>

class XMLNode;

namespace PBD {

class Controllable : public PBD::StatefulDestructible {
  public:
	Controllable (std::string name);
	virtual ~Controllable() { Destroyed (this); }

	virtual void set_value (float) = 0;
	virtual float get_value (void) const = 0;

	virtual bool can_send_feedback() const { return true; }

	sigc::signal<void> LearningFinished;

	static sigc::signal<bool,PBD::Controllable*> StartLearning;
	static sigc::signal<void,PBD::Controllable*> StopLearning;

	static sigc::signal<void,Controllable*> Destroyed;

	sigc::signal<void> Changed;

	int set_state (const XMLNode&);
	XMLNode& get_state ();

	std::string name() const { return _name; }

	static Controllable* by_id (const PBD::ID&);
	static Controllable* by_name (const std::string&);

  private:
	std::string _name;

	void add ();
	void remove ();

	typedef std::set<PBD::Controllable*> Controllables;
	static Glib::Mutex* registry_lock;
	static Controllables registry;
};

/* a utility class for the occasions when you need but do not have
   a Controllable
*/

class IgnorableControllable : public Controllable 
{
  public: 
    IgnorableControllable () : PBD::Controllable ("ignoreMe") {}
    ~IgnorableControllable () {}
    
    void set_value (float v){}
    float get_value () const { return 0.0; }
    bool can_send_feedback () const { return false; }
};

}

#endif /* __pbd_controllable_h__ */
