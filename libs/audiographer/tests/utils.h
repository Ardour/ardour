#ifndef AUDIOGRAPHER_TESTS_UTILS_H
#define AUDIOGRAPHER_TESTS_UTILS_H

// Includes we want almost always

#include <cppunit/extensions/HelperMacros.h>
#include <boost/shared_ptr.hpp>

// includes used in this file

#include "audiographer/sink.h"
#include "audiographer/exception.h"

#include <vector>
#include <cstring>
#include <cstdlib>
#include <ctime>

using AudioGrapher::nframes_t;

struct TestUtils
{
	template<typename T>
	static bool array_equals (T const * a, T const * b, nframes_t frames)
	{
		for (nframes_t i = 0; i < frames; ++i) {
			if (a[i] != b[i]) {
				return false;
			}
		}
		return true;
	}

	template<typename T>
	static bool array_filled (T const * array, nframes_t frames)
	{
		for (nframes_t i = 0; i < frames; ++i) {
			if (array[i] == static_cast<T> (0.0)) {
				return false;
			}
		}
		return true;
	}

	/// Generate random data, all samples guaranteed not to be 0.0, 1.0 or -1.0
	static float * init_random_data (nframes_t frames, float range = 1.0)
	{
		unsigned int const granularity = 4096;
		float * data = new float[frames];
		srand (std::time (NULL));
		
		for (nframes_t i = 0; i < frames; ++i) {
			do {
				int biased_int = (rand() % granularity) - (granularity / 2);
				data[i] = (range * biased_int) / granularity;
			} while (data[i] == 0.0 || data[i] == 1.0 || data[i] == -1.0);
		}
		return data;
	}
};

template<typename T>
class VectorSink : public AudioGrapher::Sink<T>
{
  public:
	virtual void process (AudioGrapher::ProcessContext<T> const & c)
	{
		data.resize (c.frames());
		memcpy (&data[0], c.data(), c.frames() * sizeof(T));
	}

	void process (AudioGrapher::ProcessContext<T> & c) { AudioGrapher::Sink<T>::process (c); }
	using AudioGrapher::Sink<T>::process;

	std::vector<T> const & get_data() const { return data; }
	T const * get_array() const { return &data[0]; }
	void reset() { data.clear(); }

  protected:
	std::vector<T> data;

};

template<typename T>
class AppendingVectorSink : public VectorSink<T>
{
  public:
	void process (AudioGrapher::ProcessContext<T> const & c)
	{
		std::vector<T> & data (VectorSink<T>::data);
		data.resize (total_frames + c.frames());
		memcpy (&data[total_frames], c.data(), c.frames() * sizeof(T));
		total_frames += c.frames();
	}
	using AudioGrapher::Sink<T>::process;

	void reset ()
	{
		total_frames = 0;
		VectorSink<T>::reset();
	}

  private:
	nframes_t total_frames;
};


template<typename T>
class ThrowingSink : public AudioGrapher::Sink<T>
{
  public:
	void process (AudioGrapher::ProcessContext<T> const &)
	{
		throw AudioGrapher::Exception(*this, "ThrowingSink threw!");
	}
	using AudioGrapher::Sink<T>::process;
};

#endif // AUDIOGRAPHER_TESTS_UTILS_H
