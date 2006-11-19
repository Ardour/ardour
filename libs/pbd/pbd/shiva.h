#ifndef __pbd_shiva_h__
#define __pbd_shiva_h__

#include <sigc++/sigc++.h>

namespace PBD {

/* named after the Hindu god Shiva, The Destroyer */

template<typename ObjectWithGoingAway, typename ObjectToBeDestroyed>
class Shiva {
  public:
	Shiva (ObjectWithGoingAway& emitter, ObjectToBeDestroyed& receiver) {

		/* if the emitter goes away, destroy the receiver */

		_connection = emitter.GoingAway.connect 
			(sigc::bind (sigc::mem_fun 
				     (*this, &Shiva<ObjectWithGoingAway,ObjectToBeDestroyed>::destroy),
				     &receiver));
	}

	~Shiva() { 
		forget ();
	}

  private:
	sigc::connection _connection;

	void destroy (ObjectToBeDestroyed* obj) {
		delete obj;
		forget ();
	}

	void forget () {
		_connection.disconnect ();
	}
			
};

template<typename ObjectWithGoingAway, typename ObjectToBeDestroyed>
class ProxyShiva {
  public:
	ProxyShiva (ObjectWithGoingAway& emitter, ObjectToBeDestroyed& receiver, void (*callback)(ObjectToBeDestroyed*, ObjectWithGoingAway*)) {

		/* if the emitter goes away, destroy the receiver */

		_callback = callback;
		_callback_argument1 = &receiver;
		_callback_argument2 = &emitter;

		_connection = emitter.GoingAway.connect 
			(sigc::bind (sigc::mem_fun 
				     (*this, &ProxyShiva<ObjectWithGoingAway,ObjectToBeDestroyed>::destroy),
				     &receiver));
	}

	~ProxyShiva() { 
		forget ();
	}

  private:
	sigc::connection _connection;
	void (*_callback) (ObjectToBeDestroyed*, ObjectWithGoingAway*);
	ObjectToBeDestroyed* _callback_argument1;
	ObjectWithGoingAway* _callback_argument2;

	void destroy (ObjectToBeDestroyed* obj) {
		/* callback must destroy obj if appropriate, not done here */
		_callback (obj, _callback_argument2);
		forget ();
	}

	void forget () {
		_connection.disconnect ();
	}
};

template<typename ObjectWithGoingAway, typename ObjectToBeDestroyed>
class PairedShiva {
  public:
	PairedShiva (ObjectWithGoingAway& emitter, ObjectToBeDestroyed& receiver) {

		/* if the emitter goes away, destroy the receiver */

		_connection1 = emitter.GoingAway.connect 
			(sigc::bind (sigc::mem_fun 
				     (*this, &PairedShiva<ObjectWithGoingAway,ObjectToBeDestroyed>::destroy),
				     &receiver));

		/* if the receiver goes away, forget all this nonsense */

		_connection2 = receiver.GoingAway.connect 
			(sigc::mem_fun (*this, &PairedShiva<ObjectWithGoingAway,ObjectToBeDestroyed>::forget));
	}

	~PairedShiva() { 
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
