#ifndef AUDIOGRAPHER_UTILS_H
#define AUDIOGRAPHER_UTILS_H

#include "types.h"
#include "exception.h"

#include <cstring>

namespace AudioGrapher
{

class Utils
{
  public:
	
	static void free_resources();
	
	/// Initialize zero buffer, if buffer is != 0, it will be used as the zero buffer
	template <typename T>
	static void init_zeros (nframes_t frames, T const * buffer = 0)
	{
		if (frames == 0) {
			throw Exception (Utils(), "init_zeros must be called with an argument greater than zero.");
		}
		unsigned long n_zeros = frames * sizeof (T);
		if (n_zeros <= num_zeros) { return; }
		delete [] zeros;
		if (buffer) {
			zeros = reinterpret_cast<char const *>(buffer);
		} else {
			zeros = new char[n_zeros];
			memset (const_cast<char *>(zeros), 0, n_zeros);
		}
		num_zeros = n_zeros;
	}
	  
	template <typename T>
	static T const * get_zeros (nframes_t frames)
	{
		if (frames * sizeof (T) > num_zeros) {
			throw Exception (Utils(), "init_zeros has not been called with a large enough frame count");
		}
		return reinterpret_cast<T const *> (zeros);
	}
	
	template <typename T>
	static nframes_t get_zero_buffer_size ()
	{
		return num_zeros / sizeof (T);
	}

  private:
	static char const *  zeros;
	static unsigned long num_zeros;
};

} // namespace

#endif // AUDIOGRAPHER_ROUTINES_H
