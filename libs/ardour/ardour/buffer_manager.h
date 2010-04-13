#ifndef __libardour_buffer_manager__ 
#define __libardour_buffer_manager__

#include <stdint.h>

#include "pbd/ringbufferNPT.h"

#include "ardour/chan_count.h"

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
        typedef RingBufferNPT<ThreadBuffers*> ThreadBufferFIFO;
        static ThreadBufferFIFO* thread_buffers;
};

}

#endif /* __libardour_buffer_manager__ */
