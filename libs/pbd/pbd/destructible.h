#ifndef __pbd_destructible_h__
#define __pbd_destructible_h__

#include <sigc++/signal.h>

namespace PBD {

/* be very very careful using this class. it does not inherit from sigc::trackable and thus
   should only be used in multiple-inheritance situations involving another type
   that does inherit from sigc::trackable (or sigc::trackable itself)
*/

class ThingWithGoingAway {
  public:
	virtual ~ThingWithGoingAway () {}
	sigc::signal<void> GoingAway;
};

class Destructible : public sigc::trackable, public ThingWithGoingAway {
  public:
	virtual ~Destructible () {}
	void drop_references () const { GoingAway(); }

};

}

#endif /* __pbd_destructible_h__ */
