#include <unistd.h>

#include "pbd/stacktrace.h"
#include "pbd/abstract_ui.h"
#include "pbd/pthread_utils.h"
#include "pbd/failed_constructor.h"

#include "i18n.h"

using namespace std;

template <typename RequestObject>
AbstractUI<RequestObject>::AbstractUI (const string& name)
	: BaseUI (name)
{
	PBD::ThreadCreatedWithRequestSize.connect (mem_fun (*this, &AbstractUI<RequestObject>::register_thread));
}

template <typename RequestObject> void
AbstractUI<RequestObject>::register_thread (string target_gui, pthread_t thread_id, string /*thread_name*/, uint32_t num_requests)
{
	if (target_gui != name()) {
		return;
	}

	RequestBuffer* b = new RequestBuffer (num_requests);

	{
		Glib::Mutex::Lock lm (request_buffer_map_lock);
		request_buffers[thread_id] = b;
	}

	per_thread_request_buffer.set (b);
}

template <typename RequestObject> RequestObject*
AbstractUI<RequestObject>::get_request (RequestType rt)
{
	RequestBuffer* rbuf = per_thread_request_buffer.get ();
	RequestBufferVector vec;

	if (rbuf != 0) {
		/* we have a per-thread FIFO, use it */

		rbuf->get_write_vector (&vec);

		if (vec.len[0] == 0) {
			return 0;
		}

		vec.buf[0]->type = rt;
		return vec.buf[0];
	}

	RequestObject* req = new RequestObject;
	req->type = rt;
	return req;
}

template <typename RequestObject> void
AbstractUI<RequestObject>::handle_ui_requests ()
{
	RequestBufferMapIterator i;
	RequestBufferVector vec;

	/* per-thread buffers first */

	request_buffer_map_lock.lock ();

	for (i = request_buffers.begin(); i != request_buffers.end(); ++i) {

		while (true) {

			/* we must process requests 1 by 1 because
			   the request may run a recursive main
			   event loop that will itself call
			   handle_ui_requests. when we return
			   from the request handler, we cannot
			   expect that the state of queued requests
			   is even remotely consistent with
			   the condition before we called it.
			*/

			i->second->get_read_vector (&vec);

			if (vec.len[0] == 0) {
				break;
			} else {
				request_buffer_map_lock.unlock ();
				do_request (vec.buf[0]);
				request_buffer_map_lock.lock ();
				i->second->increment_read_ptr (1);
			} 
		}
	}

	request_buffer_map_lock.unlock ();

	/* and now, the generic request buffer. same rules as above apply */

	Glib::Mutex::Lock lm (request_list_lock);

	while (!request_list.empty()) {
		RequestObject* req = request_list.front ();
		request_list.pop_front ();
		lm.release ();

		do_request (req);

		delete req;

		lm.acquire();
	}
}

template <typename RequestObject> void
AbstractUI<RequestObject>::send_request (RequestObject *req)
{
	if (base_instance() == 0) {
		return; /* XXX is this the right thing to do ? */
	}

	if (caller_is_self ()) {
		do_request (req);
	} else {	
		RequestBuffer* rbuf = per_thread_request_buffer.get ();

		if (rbuf != 0) {
			rbuf->increment_write_ptr (1);
		} else {
			/* no per-thread buffer, so just use a list with a lock so that it remains
			   single-reader/single-writer semantics
			*/
			Glib::Mutex::Lock lm (request_list_lock);
			request_list.push_back (req);
		}

		request_channel.wakeup ();
	}
}

template<typename RequestObject> void
AbstractUI<RequestObject>::call_slot (sigc::slot<void> elSlot)
{
	if (caller_is_self()) {
		elSlot ();
		return;
	}

	RequestObject *req = get_request (BaseUI::CallSlot);
	
	if (req == 0) {
		return;
	}

	req->the_slot = elSlot;
	send_request (req);
}	

