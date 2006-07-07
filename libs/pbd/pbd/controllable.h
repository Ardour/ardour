#ifndef __pbd_controllable_h__
#define __pbd_controllable_h__

#include <sigc++/trackable.h>
#include <sigc++/signal.h>

#include <pbd/stateful.h>
#include <pbd/id.h>

class XMLNode;

namespace PBD {

class Controllable : public virtual sigc::trackable, public Stateful {
  public:
	Controllable ();
	virtual ~Controllable() { GoingAway (this); }

	virtual void set_value (float) = 0;
	virtual float get_value (void) const = 0;

	virtual bool can_send_feedback() const { return true; }

	static sigc::signal<void,Controllable*> Created;
	static sigc::signal<void,Controllable*> GoingAway;


	static sigc::signal<bool,PBD::Controllable*> StartLearning;
	static sigc::signal<void,PBD::Controllable*> StopLearning;

	sigc::signal<void> Changed;

	const PBD::ID& id() const { return _id; }

	int set_state (const XMLNode&) { return 0; }
	XMLNode& get_state ();

  private:
	PBD::ID _id;
};

}

#endif /* __pbd_controllable_h__ */
