#ifndef __pbd_stateful_destructible_h__
#define __pbd_stateful_destructible_h__

#include <pbd/stateful.h>
#include <pbd/destructible.h>

namespace PBD {
class StatefulDestructible : public Stateful, public Destructible 
{
};
}

#endif /* __pbd_stateful_destructible_h__ */
