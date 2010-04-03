#include <unistd.h>
#include <iostream>

#include "pbd/stacktrace.h"
#include "pbd/abstract_ui.h"
#include "pbd/pthread_utils.h"
#include "pbd/failed_constructor.h"

#include "i18n.h"

using namespace std;

static void do_not_delete_the_request_buffer (void*) { }

template<typename R>
Glib::StaticPrivate<typename AbstractUI<R>::RequestBuffer> AbstractUI<R>::per_thread_request_buffer;

template <typename RequestObject>
AbstractUI<RequestObject>::AbstractUI (const string& name)
	: BaseUI (name)
{
	void (AbstractUI<RequestObject>::*pmf)(string,pthread_t,string,uint32_t) = &AbstractUI<RequestObject>::register_thread;

	/* better to make this connect a handler that runs in the UI event loop but the syntax seems hard, and 
	   register_thread() is thread safe anyway.
	*/

	PBD::ThreadCreatedWithRequestSize.connect_same_thread (new_thread_connection, boost::bind (pmf, this, _1, _2, _3, _4));
}

template <typename RequestObject> void
AbstractUI<RequestObject>::register_thread (string target_gui, pthread_t thread_id, string /*thread name*/, uint32_t num_requests)
{
	if (target_gui != name()) {
		return;
	}

	RequestBuffer* b = new RequestBuffer (num_requests);

	{
		Glib::Mutex::Lock lm (request_buffer_map_lock);
		request_buffers[thread_id] = b;
	}

	per_thread_request_buffer.set (b, do_not_delete_the_request_buffer);
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
                vec.buf[0]->valid = true;
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
                                if (vec.buf[0]->valid) {
                                        request_buffer_map_lock.unlock ();
                                        do_request (vec.buf[0]);
                                        request_buffer_map_lock.lock ();
                                        if (vec.buf[0]->invalidation) {
                                                vec.buf[0]->invalidation->requests.remove (vec.buf[0]);
                                        }
                                        i->second->increment_read_ptr (1);
                                }
			} 
		}
	}

	request_buffer_map_lock.unlock ();

	/* and now, the generic request buffer. same rules as above apply */

	Glib::Mutex::Lock lm (request_list_lock);

	while (!request_list.empty()) {
		RequestObject* req = request_list.front ();
		request_list.pop_front ();

                /* We need to use this lock, because its the one
                   returned by slot_invalidation_mutex() and protects
                   against request invalidation.
                */

                request_buffer_map_lock.lock ();
                if (!req->valid) {
                        delete req;
                        request_buffer_map_lock.unlock ();
                        continue;
                }

                /* we're about to execute this request, so its
                   too late for any invalidation. mark
                   the request as "done" before we start.
                */

                if (req->invalidation) {
                        req->invalidation->requests.remove (req);
                }

                request_buffer_map_lock.unlock ();

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
AbstractUI<RequestObject>::call_slot (InvalidationRecord* invalidation, const boost::function<void()>& f)
{
	if (caller_is_self()) {
		f ();
		return;
	}

	RequestObject *req = get_request (BaseUI::CallSlot);
	
	if (req == 0) {
		return;
	}

	req->the_slot = f;
        req->invalidation = invalidation;

        if (invalidation) {
                invalidation->requests.push_back (req);
                invalidation->event_loop = this;
        }

	send_request (req);
}	

