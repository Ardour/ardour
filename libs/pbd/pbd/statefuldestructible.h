#ifndef __pbd_stateful_destructible_h__
#define __pbd_stateful_destructible_h__

#include <pbd/stateful.h>
#include <pbd/destructible.h>

namespace PBD {

class StatefulDestructible : public Stateful, public Destructible 
{
};

/* be very very careful using this class. it does not inherit from sigc::trackable and thus
   should only be used in multiple-inheritance situations involving another type
   that does inherit from sigc::trackable (or sigc::trackable itself)
*/

class StatefulThingWithGoingAway : public Stateful, public ThingWithGoingAway
{
};

}


#endif /* __pbd_stateful_destructible_h__ */
