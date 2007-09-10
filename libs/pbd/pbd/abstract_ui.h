/*
    Copyright (C) 1998-99 Paul Barton-Davis 

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

#ifndef __pbd_abstract_ui_h__
#define __pbd_abstract_ui_h__

#include <map>
#include <string>
#include <pthread.h>

#include <sigc++/sigc++.h>

#include <glibmm/thread.h>

#include <pbd/receiver.h>
#include <pbd/ringbufferNPT.h>
#include <pbd/base_ui.h>

class Touchable;

template <class RequestObject>
class AbstractUI : public BaseUI
{
  public:
	AbstractUI (std::string name, bool with_signal_pipe);
	virtual ~AbstractUI() {}

	virtual bool caller_is_ui_thread() = 0;

	void call_slot (sigc::slot<void> el_slot) {
		RequestObject *req = get_request (BaseUI::CallSlot);
		
		if (req == 0) {
			return;
		}
		
		req->slot = el_slot;
		send_request (req);
	}	

	void register_thread (pthread_t, std::string);
	void register_thread_with_request_count (pthread_t, std::string, uint32_t num_requests);
	void unregister_thread (pthread_t);

  protected:
	typedef RingBufferNPT<RequestObject> RequestBuffer;
	typedef typename RequestBuffer::rw_vector RequestBufferVector;
	typedef typename std::map<pthread_t,RequestBuffer*>::iterator RequestBufferMapIterator;

    Glib::Mutex request_buffer_map_lock;
	typedef std::map<pthread_t,RequestBuffer*> RequestBufferMap;
	RequestBufferMap request_buffers;
	pthread_key_t thread_request_buffer_key;
	RequestObject* get_request (RequestType);
	void handle_ui_requests ();
	void send_request (RequestObject *);

	virtual void do_request (RequestObject *) = 0;
};

#endif /* __pbd_abstract_ui_h__ */


