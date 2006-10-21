#ifndef __pbd_shiva_h__
#define __pbd_shiva_h__

#include <sigc++/sigc++.h>

namespace PBD {

template<typename ObjectWithGoingAway, typename ObjectToBeDestroyed>

/* named after the Hindu god Shiva, The Destroyer */

class Shiva {
  public:
	Shiva (ObjectWithGoingAway& emitter, ObjectToBeDestroyed& receiver) {

		/* if the emitter goes away, destroy the receiver */

		_connection1 = emitter.GoingAway.connect 
			(sigc::bind (sigc::mem_fun 
				     (*this, &Shiva<ObjectWithGoingAway,ObjectToBeDestroyed>::destroy),
				     &receiver));

		/* if the receiver goes away, forget all this nonsense */

		_connection2 = receiver.GoingAway.connect 
			(sigc::mem_fun (*this, &Shiva<ObjectWithGoingAway,ObjectToBeDestroyed>::forget));
	}

	~Shiva() { 
		forget ();
	}

  private:
	sigc::connection _connection1;
	sigc::connection _connection2;

	void destroy (ObjectToBeDestroyed* obj) {
		delete obj;
		forget ();
	}

	void forget () {
		_connection1.disconnect ();
		_connection2.disconnect ();
	}
			
};

}

#endif /* __pbd_shiva_h__ */
