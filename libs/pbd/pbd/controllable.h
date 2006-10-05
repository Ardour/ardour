#ifndef __pbd_controllable_h__
#define __pbd_controllable_h__

#include <string>

#include <sigc++/trackable.h>
#include <sigc++/signal.h>

#include <pbd/stateful.h>
#include <pbd/id.h>

class XMLNode;

namespace PBD {

class Controllable : public virtual sigc::trackable, public Stateful {
  public:
	Controllable (std::string name);
	virtual ~Controllable() { GoingAway (this); }

	virtual void set_value (float) = 0;
	virtual float get_value (void) const = 0;

	virtual bool can_send_feedback() const { return true; }

	sigc::signal<void> LearningFinished;

	static sigc::signal<void,Controllable*> GoingAway;

	static sigc::signal<bool,PBD::Controllable*> StartLearning;
	static sigc::signal<void,PBD::Controllable*> StopLearning;

	sigc::signal<void> Changed;

	int set_state (const XMLNode&);
	XMLNode& get_state ();

	std::string name() const { return _name; }

  private:
	std::string _name;
};

}

#endif /* __pbd_controllable_h__ */
