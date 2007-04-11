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

}

#endif /* __pbd_controllable_h__ */
