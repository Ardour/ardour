/*
    Copyright (C) 1998-2009 Paul Davis 

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

#include <glibmm/threads.h>

#include "pbd/libpbd_visibility.h"
#include "pbd/receiver.h"
#include "pbd/ringbufferNPT.h"
#include "pbd/signals.h"
#include "pbd/base_ui.h"

class Touchable;

template<typename RequestObject>
class AbstractUI : public BaseUI
{
  public:
	AbstractUI (const std::string& name);
	virtual ~AbstractUI() {}

	void register_thread (std::string, pthread_t, std::string, uint32_t num_requests);
	void call_slot (EventLoop::InvalidationRecord*, const boost::function<void()>&);
        Glib::Threads::Mutex& slot_invalidation_mutex() { return request_buffer_map_lock; }

	Glib::Threads::Mutex request_buffer_map_lock;

  protected:
	struct RequestBuffer : public PBD::RingBufferNPT<RequestObject> {
                bool dead;
                AbstractUI<RequestObject>& ui;
                RequestBuffer (uint32_t size, AbstractUI<RequestObject>& uir) 
                        : PBD::RingBufferNPT<RequestObject> (size)
                        , dead (false) 
                        , ui (uir) {}
        };
	typedef typename RequestBuffer::rw_vector RequestBufferVector;
	typedef typename std::map<pthread_t,RequestBuffer*>::iterator RequestBufferMapIterator;
	typedef std::map<pthread_t,RequestBuffer*> RequestBufferMap;

	RequestBufferMap request_buffers;
        static Glib::Threads::Private<RequestBuffer> per_thread_request_buffer;
	
	Glib::Threads::Mutex               request_list_lock;
	std::list<RequestObject*> request_list;
	
	RequestObject* get_request (RequestType);
	void handle_ui_requests ();
	void send_request (RequestObject *);

	virtual void do_request (RequestObject *) = 0;
	PBD::ScopedConnection new_thread_connection;
};

#endif /* __pbd_abstract_ui_h__ */


