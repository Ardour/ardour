#ifndef __libardour_buffer_manager__
#define __libardour_buffer_manager__

#include <stdint.h>

#include "pbd/ringbufferNPT.h"

#include "ardour/chan_count.h"
#include <list>
#include <glibmm/threads.h>

namespace ARDOUR {

class ThreadBuffers;

class BufferManager
{
public:
	static void init (uint32_t);

	static ThreadBuffers* get_thread_buffers ();
	static void           put_thread_buffers (ThreadBuffers*);

	static void ensure_buffers (ChanCount howmany = ChanCount::ZERO);

private:
        static Glib::Threads::Mutex rb_mutex;

	typedef PBD::RingBufferNPT<ThreadBuffers*> ThreadBufferFIFO;
	typedef std::list<ThreadBuffers*> ThreadBufferList;

	static ThreadBufferFIFO* thread_buffers;
	static ThreadBufferList* thread_buffers_list;
};

}

#endif /* __libardour_buffer_manager__ */
