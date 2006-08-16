#include <unistd.h>

#include <pbd/abstract_ui.h>
#include <pbd/pthread_utils.h>
#include <pbd/failed_constructor.h>

#include "i18n.h"

using namespace std;

template <typename RequestObject>
AbstractUI<RequestObject>::AbstractUI (string name, bool with_signal_pipes)
	: BaseUI (name, with_signal_pipes)
{
	if (pthread_key_create (&thread_request_buffer_key, 0)) {
		cerr << _("cannot create thread request buffer key") << endl;
		throw failed_constructor();
	}

	PBD::ThreadCreated.connect (mem_fun (*this, &AbstractUI<RequestObject>::register_thread));
	PBD::ThreadCreatedWithRequestSize.connect (mem_fun (*this, &AbstractUI<RequestObject>::register_thread_with_request_count));
}

template <typename RequestObject> void
AbstractUI<RequestObject>::register_thread (pthread_t thread_id, string name)
{
	register_thread_with_request_count (thread_id, name, 256);
}

template <typename RequestObject> void
AbstractUI<RequestObject>::register_thread_with_request_count (pthread_t thread_id, string thread_name, uint32_t num_requests)
{
	RequestBuffer* b = new RequestBuffer (num_requests);

	{
        Glib::Mutex::Lock lm (request_buffer_map_lock);
		request_buffers[thread_id] = b;
	}

	pthread_setspecific (thread_request_buffer_key, b);
}

template <typename RequestObject> RequestObject*
AbstractUI<RequestObject>::get_request (RequestType rt)
{
	RequestBuffer* rbuf = static_cast<RequestBuffer*>(pthread_getspecific (thread_request_buffer_key));
	
	if (rbuf == 0) {
		/* Cannot happen, but if it does we can't use the error reporting mechanism */
		cerr << _("programming error: ")
		     << string_compose ("no %1-UI request buffer found for thread %2", name(), pthread_name())
		     << endl;
		abort ();
	}
	
	RequestBufferVector vec;
	
	rbuf->get_write_vector (&vec);

	if (vec.len[0] == 0) {
		if (vec.len[1] == 0) {
			cerr << string_compose ("no space in %1-UI request buffer for thread %2", name(), pthread_name())
			     << endl;
			return 0;
		} else {
			vec.buf[1]->type = rt;
			return vec.buf[1];
		}
	} else {
		vec.buf[0]->type = rt;
		return vec.buf[0];
	}
}

template <typename RequestObject> void
AbstractUI<RequestObject>::handle_ui_requests ()
{
	RequestBufferMapIterator i;

	request_buffer_map_lock.lock ();

	for (i = request_buffers.begin(); i != request_buffers.end(); ++i) {

		RequestBufferVector vec;

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
				/* request_factory/copy constructor does a deep
				   copy of the Request object,
				   unlike Ringbuffer::read()
				*/

				RequestObject req (*vec.buf[0]);
				i->second->increment_read_ptr (1);
				request_buffer_map_lock.unlock ();
				do_request (&req);
				request_buffer_map_lock.lock ();
			} 
		}
	}

	request_buffer_map_lock.unlock ();
}

template <typename RequestObject> void
AbstractUI<RequestObject>::send_request (RequestObject *req)
{
	if (base_instance() == 0) {
		return; /* XXX is this the right thing to do ? */
	}
	
	if (caller_is_ui_thread()) {
		// cerr << "GUI thread sent request " << req << " type = " << req->type << endl;
		do_request (req);
	} else {	
		RequestBuffer* rbuf = static_cast<RequestBuffer*> (pthread_getspecific (thread_request_buffer_key));

		if (rbuf == 0) {
			/* can't use the error system to report this, because this
			   thread isn't registered!
			*/
			cerr << _("programming error: ")
			     << string_compose ("AbstractUI::send_request() called from %1 (%2), but no request buffer exists for that thread", name(), pthread_name())
			     << endl;
			abort ();
		}
		
		// cerr << "thread " << pthread_self() << " sent request " << req << " type = " << req->type << endl;

		rbuf->increment_write_ptr (1);

		if (signal_pipe[1] >= 0) {
			const char c = 0;
			write (signal_pipe[1], &c, 1);
		}
	}
}

