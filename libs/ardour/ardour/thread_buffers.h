#ifndef __libardour_thread_buffers__
#define __libardour_thread_buffers__

#include <glibmm/thread.h>

#include "ardour/chan_count.h"
#include "ardour/types.h"

namespace ARDOUR {

class BufferSet;

class ThreadBuffers {
public:
	ThreadBuffers ();
	~ThreadBuffers ();

	void ensure_buffers (ChanCount howmany = ChanCount::ZERO);

	BufferSet* silent_buffers;
	BufferSet* scratch_buffers;
	BufferSet* mix_buffers;
	gain_t*    gain_automation_buffer;
	gain_t*    send_gain_automation_buffer;
	pan_t**    pan_automation_buffer;
	uint32_t   npan_buffers;

private:
	void allocate_pan_automation_buffers (framecnt_t nframes, uint32_t howmany, bool force);
};

} // namespace

#endif /* __libardour_thread_buffers__ */
