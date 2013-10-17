#ifndef AUDIOGRAPHER_THREADER_H
#define AUDIOGRAPHER_THREADER_H

#include <glibmm/threadpool.h>
#include <glibmm/timeval.h>
#include <sigc++/slot.h>
#include <boost/format.hpp>

#include <glib.h>
#include <vector>
#include <algorithm>

#include "audiographer/visibility.h"
#include "audiographer/source.h"
#include "audiographer/sink.h"
#include "audiographer/exception.h"

namespace AudioGrapher
{

/// Class that stores exceptions thrown from different threads
class LIBAUDIOGRAPHER_API ThreaderException : public Exception
{
  public:
	template<typename T>
	ThreaderException (T const & thrower, std::exception const & e)
		: Exception (thrower,
			boost::str ( boost::format
			("\n\t- Dynamic type: %1%\n\t- what(): %2%")
			% DebugUtils::demangled_name (e) % e.what() ))
	{ }
};

/// Class for distributing processing across several threads
template <typename T = DefaultSampleType>
class LIBAUDIOGRAPHER_API Threader : public Source<T>, public Sink<T>
{
  private:
	typedef std::vector<typename Source<T>::SinkPtr> OutputVec;

  public:
	
	/** Constructor
	  * \n RT safe
	  * \param thread_pool a thread pool from which all tasks are scheduled
	  * \param wait_timeout_milliseconds maximum time allowed for threads to use in processing
	  */
	Threader (Glib::ThreadPool & thread_pool, long wait_timeout_milliseconds = 500)
	  : thread_pool (thread_pool)
	  , readers (0)
	  , wait_timeout (wait_timeout_milliseconds)
	{ }
	
	virtual ~Threader () {}
	
	/// Adds output \n RT safe
	void add_output (typename Source<T>::SinkPtr output) { outputs.push_back (output); }
	
	/// Clears outputs \n RT safe
	void clear_outputs () { outputs.clear (); }
	
	/// Removes a specific output \n RT safe
	void remove_output (typename Source<T>::SinkPtr output) {
		typename OutputVec::iterator new_end = std::remove(outputs.begin(), outputs.end(), output);
		outputs.erase (new_end, outputs.end());
	}
	
	/// Processes context concurrently by scheduling each output separately to the given thread pool
	void process (ProcessContext<T> const & c)
	{
		wait_mutex.lock();
		
		exception.reset();
		
		unsigned int outs = outputs.size();
		g_atomic_int_add (&readers, outs);
		for (unsigned int i = 0; i < outs; ++i) {
			thread_pool.push (sigc::bind (sigc::mem_fun (this, &Threader::process_output), c, i));
		}
		
		wait();
	}
	
	using Sink<T>::process;
	
  private:

	void wait()
	{
		while (g_atomic_int_get (&readers) != 0) {
			gint64 end_time = g_get_monotonic_time () + (wait_timeout * G_TIME_SPAN_MILLISECOND);
			wait_cond.wait_until(wait_mutex, end_time);
		}

		wait_mutex.unlock();
		
		if (exception) {
			throw *exception;
		}
	}
	
	void process_output(ProcessContext<T> const & c, unsigned int output)
	{
		try {
			outputs[output]->process (c);
		} catch (std::exception const & e) {
			// Only first exception will be passed on
			exception_mutex.lock();
			if(!exception) { exception.reset (new ThreaderException (*this, e)); }
			exception_mutex.unlock();
		}
		
		if (g_atomic_int_dec_and_test (&readers)) {
			wait_cond.signal();
		}
	}

	OutputVec outputs;

	Glib::ThreadPool & thread_pool;
        Glib::Threads::Mutex wait_mutex;
        Glib::Threads::Cond  wait_cond;
	gint        readers;
	long        wait_timeout;
	
        Glib::Threads::Mutex exception_mutex;
	boost::shared_ptr<ThreaderException> exception;

};

} // namespace

#endif //AUDIOGRAPHER_THREADER_H
