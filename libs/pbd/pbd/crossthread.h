#ifndef __pbd__crossthread_h__
#define __pbd__crossthread_h__

#include <pbd/abstract_ui.h>
#include <sigc++/sigc++.h>
#include <pthread.h>

template<class RequestType> 
void 
call_slot_from_thread_or_dispatch_it (pthread_t thread_id, AbstractUI<RequestType>& ui, sigc::slot<void> theSlot)
{
	/* when called, this function will determine whether the calling thread
	   is the same as thread specified by the first argument. if it is,
	   the we execute the slot. if not, we ask the interface given by the second
	   argument to call the slot.
	*/

	if (pthread_self() == thread_id) {
		theSlot ();
	} else {
		ui.call_slot (theSlot);
	}
}

template<class RequestType> 
sigc::slot<void>
crossthread_safe (pthread_t thread_id, AbstractUI<RequestType>& ui, sigc::slot<void> theSlot)
{
	/* this function returns a slot that will ensure that theSlot is either
	   called by the specified thread or passed to the interface via 
	   AbstractUI::call_slot().
	*/
	   
	return sigc::bind (sigc::ptr_fun (call_slot_from_thread_or_dispatch_it<RequestType>), 
			   thread_id, ui, theSlot);
}

#endif /* __pbd__crossthread_h__ */
