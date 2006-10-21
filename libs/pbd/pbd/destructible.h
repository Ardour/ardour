#ifndef __pbd_destructible_h__
#define __pbd_destructible_h__

#include <sigc++/signal.h>

namespace PBD {

class Destructible : public virtual sigc::trackable {
  public:
	Destructible() {}
	virtual ~Destructible () {}

	sigc::signal<void> GoingAway;

	void drop_references () const { GoingAway(); }
};

}

#endif /* __pbd_destructible_h__ */
