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

/* We have a special case in libpbd of a template class that gets instantiated
 * as the base class of several classes in other libraries. It is not possible
 * to use LIBFOO_API to mark this visible, because the FOO in each case is 
 * different. So we define this generic visible/export/hidden/import pair
 * of macros to try to deal with this special case. These should NEVER be
 * used anywhere except AbstractUI<T> (or similar cases if they arise.
 *
 * Note the assumption here that other libs are being built as DLLs if this one is.
 */

#ifdef ABSTRACT_UI_EXPORTS
#define ABSTRACT_UI_API LIBPBD_DLL_EXPORT
#else
#define ABSTRACT_UI_API LIBPBD_DLL_IMPORT
#endif 


class Touchable;

template<typename RequestObject>
class ABSTRACT_UI_API AbstractUI : public BaseUI
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

#if defined(__MINGW32__) && !defined (__MINGW64__)

	struct pthread_cmp
	{
		bool operator() (const ptw32_handle_t& thread1, const ptw32_handle_t& thread2)
		{
			return thread1.p < thread2.p;
		}
	};
	typedef typename std::map<pthread_t,RequestBuffer*, pthread_cmp>::iterator RequestBufferMapIterator;
	typedef std::map<pthread_t,RequestBuffer*, pthread_cmp> RequestBufferMap;
#else
	typedef typename std::map<pthread_t,RequestBuffer*>::iterator RequestBufferMapIterator;
	typedef std::map<pthread_t,RequestBuffer*> RequestBufferMap;
#endif

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


