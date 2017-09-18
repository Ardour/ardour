#ifndef AUDIOGRAPHER_INTERLEAVER_H
#define AUDIOGRAPHER_INTERLEAVER_H

#include "audiographer/visibility.h"
#include "audiographer/types.h"
#include "audiographer/sink.h"
#include "audiographer/exception.h"
#include "audiographer/throwing.h"
#include "audiographer/utils/listed_source.h"

#include <vector>
#include <cmath>

namespace AudioGrapher
{

/// Interleaves many streams of non-interleaved data into one interleaved stream
template<typename T = DefaultSampleType>
class /*LIBAUDIOGRAPHER_API*/ Interleaver
  : public ListedSource<T>
  , public Throwing<>
{
  public:

	/// Constructs an interleaver \n RT safe
	Interleaver()
	  : channels (0)
	  , max_samples (0)
	  , buffer (0)
	{}

	~Interleaver() { reset(); }

	/// Inits the interleaver. Must be called before using. \n Not RT safe
	void init (unsigned int num_channels, samplecnt_t max_samples_per_channel)
	{
		reset();
		channels = num_channels;
		max_samples = max_samples_per_channel;

		buffer = new T[channels * max_samples];

		for (unsigned int i = 0; i < channels; ++i) {
			inputs.push_back (InputPtr (new Input (*this, i)));
		}
	}

	/** Returns the input indexed by \a channel \n RT safe
	  * \n The \a process function of returned Sinks are also RT Safe
	  */
	typename Source<T>::SinkPtr input (unsigned int channel)
	{
		if (throw_level (ThrowObject) && channel >= channels) {
			throw Exception (*this, "Channel out of range");
		}

		return boost::static_pointer_cast<Sink<T> > (inputs[channel]);
	}

  private:

	class Input : public Sink<T>
	{
	  public:
		Input (Interleaver & parent, unsigned int channel)
		  : samples_written (0), parent (parent), channel (channel) {}

		void process (ProcessContext<T> const & c)
		{
			if (parent.throw_level (ThrowProcess) && c.channels() > 1) {
				throw Exception (*this, "Data input has more than on channel");
			}
			if (parent.throw_level (ThrowStrict) && samples_written) {
				throw Exception (*this, "Input channels out of sync");
			}
			samples_written = c.samples();
			parent.write_channel (c, channel);
		}

		using Sink<T>::process;

		samplecnt_t samples() { return samples_written; }
		void reset() { samples_written = 0; }

	  private:
		samplecnt_t samples_written;
		Interleaver & parent;
		unsigned int channel;
	};

	void reset ()
	{
		inputs.clear();
		delete [] buffer;
		buffer = 0;
		channels = 0;
		max_samples = 0;
	}

	void reset_channels ()
	{
		for (unsigned int i = 0; i < channels; ++i) {
			inputs[i]->reset();
		}

	}

	void write_channel (ProcessContext<T> const & c, unsigned int channel)
	{
		if (throw_level (ThrowProcess) && c.samples() > max_samples) {
			reset_channels();
			throw Exception (*this, "Too many samples given to an input");
		}

		for (unsigned int i = 0; i < c.samples(); ++i) {
			buffer[channel + (channels * i)] = c.data()[i];
		}

		samplecnt_t const ready_samples = ready_to_output();
		if (ready_samples) {
			ProcessContext<T> c_out (c, buffer, ready_samples, channels);
			ListedSource<T>::output (c_out);
			reset_channels ();
		}
	}

	samplecnt_t ready_to_output()
	{
		samplecnt_t ready_samples = inputs[0]->samples();
		if (!ready_samples) { return 0; }

		for (unsigned int i = 1; i < channels; ++i) {
			samplecnt_t const samples = inputs[i]->samples();
			if (!samples) { return 0; }
			if (throw_level (ThrowProcess) && samples != ready_samples) {
				init (channels, max_samples);
				throw Exception (*this, "Samples count out of sync");
			}
		}
		return ready_samples * channels;
	}

	typedef boost::shared_ptr<Input> InputPtr;
	std::vector<InputPtr> inputs;

	unsigned int channels;
	samplecnt_t max_samples;
	T * buffer;
};

} // namespace

#endif // AUDIOGRAPHER_INTERLEAVER_H
