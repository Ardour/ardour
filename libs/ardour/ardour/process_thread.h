#ifndef __libardour_process_thread__
#define __libardour_process_thread__

#include <glibmm/threads.h>

#include "ardour/chan_count.h"
#include "ardour/types.h"

namespace ARDOUR {

class ThreadBuffers;
class BufferSet;

class ProcessThread
{
public:
	ProcessThread ();
	~ProcessThread ();

	static void init();

	void get_buffers ();
	void drop_buffers ();

	/* these MUST be called by a process thread's thread, nothing else
	 */

	static BufferSet& get_silent_buffers (ChanCount count = ChanCount::ZERO);
	static BufferSet& get_scratch_buffers (ChanCount count = ChanCount::ZERO);
	static BufferSet& get_mix_buffers (ChanCount count = ChanCount::ZERO);
	static gain_t* gain_automation_buffer ();
	static gain_t* send_gain_automation_buffer ();
	static pan_t** pan_automation_buffer ();

protected:
	void session_going_away ();

private:
    static Glib::Threads::Private<ThreadBuffers> _private_thread_buffers;
};

} // namespace

#endif /* __libardour_process_thread__ */
