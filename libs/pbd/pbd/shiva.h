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

#ifndef __pbd_shiva_h__
#define __pbd_shiva_h__

#include <sigc++/sigc++.h>

namespace PBD {

/* named after the Hindu god Shiva, The Destroyer */

template<typename ObjectWithGoingAway, typename ObjectToBeDestroyed>
class Shiva : public sigc::trackable 
{
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
class ProxyShiva : public sigc::trackable 
{
  public:
	ProxyShiva (ObjectWithGoingAway& emitter, ObjectToBeDestroyed& receiver, void (*callback)(ObjectToBeDestroyed*, ObjectWithGoingAway*)) {
		
		/* if the emitter goes away, destroy the receiver */

		_callback = callback;
		_callback_argument = &emitter;

		_connection = emitter.GoingAway.connect 
			(sigc::bind (sigc::mem_fun 
				     (*this, &ProxyShiva<ObjectWithGoingAway,ObjectToBeDestroyed>::destroy),
				     &receiver));
	}

	~ProxyShiva () {
		forget ();
	}

  private:
	sigc::connection _connection;
	void (*_callback) (ObjectToBeDestroyed*, ObjectWithGoingAway*);
	ObjectWithGoingAway* _callback_argument;

	void destroy (ObjectToBeDestroyed* obj) {
		/* callback must destroy obj if appropriate, not done here */
		_callback (obj, _callback_argument);
		forget ();
	}

	void forget () {
		_connection.disconnect ();
	}
};

template<typename ObjectWithGoingAway, typename ObjectToBeDestroyed>
class PairedShiva : public sigc::trackable 
{
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
