#ifndef AUDIOGRAPHER_SILENCE_TRIMMER_H
#define AUDIOGRAPHER_SILENCE_TRIMMER_H

#include "audiographer/visibility.h"
#include "audiographer/debug_utils.h"
#include "audiographer/flag_debuggable.h"
#include "audiographer/sink.h"
#include "audiographer/exception.h"
#include "audiographer/utils/listed_source.h"

#include <cstring>

namespace AudioGrapher {

template<typename T> struct SilenceTester;

// this needs to be implemented for every datatype T
// currently Ardour always uses Sample aka float
template <>
struct SilenceTester<float> {
	public:
	SilenceTester (const float dB) {
		threshold = dB > -318.8f ? pow (10.0f, dB * 0.05f) : 0.0f;
	}
	bool is_silent (const float d) {
		return fabsf (d) <= threshold;
	}
	private:
	float threshold;
};


/// Removes and adds silent frames to beginning and/or end of stream
template<typename T = DefaultSampleType>
class /*LIBAUDIOGRAPHER_API*/ SilenceTrimmer
  : public ListedSource<T>
  , public Sink<T>
  , public FlagDebuggable<>
  , public Throwing<>
{
  public:

	/// Constructor, \see reset() \n Not RT safe
	SilenceTrimmer(framecnt_t silence_buffer_size_ = 1024, float thresh_dB = -INFINITY)
	  : silence_buffer_size (0)
	  , silence_buffer (0)
	  , tester (thresh_dB)
	{
		reset (silence_buffer_size_);
		add_supported_flag (ProcessContext<T>::EndOfInput);
	}

	~SilenceTrimmer()
	{
		delete [] silence_buffer;
	}

	/** Reset state \n Not RT safe
	  * Allocates a buffer the size of \a silence_buffer_size_
	  * This also defines the maximum length of output process context
	  * which can be output during long intermediate silence.
	  */
	void reset (framecnt_t silence_buffer_size_ = 1024)
	{
		if (throw_level (ThrowObject) && silence_buffer_size_ == 0) {
			throw Exception (*this,
			  "Silence trimmer constructor and reset() must be called with a non-zero parameter!");
		}

		if (silence_buffer_size != silence_buffer_size_) {
			silence_buffer_size = silence_buffer_size_;
			delete [] silence_buffer;
			silence_buffer = new T[silence_buffer_size];
			TypeUtils<T>::zero_fill (silence_buffer, silence_buffer_size);
		}

		processed_data = false;
		processing_finished = false;
		trim_beginning = false;
		trim_end = false;
		silence_frames = 0;
		max_output_frames = 0;
		add_to_beginning = 0;
		add_to_end = 0;
	}

	/** Tells that \a frames_per_channel frames of silence per channel should be added to beginning
	  * Needs to be called before starting processing.
	  * \n RT safe
	  */
	void add_silence_to_beginning (framecnt_t frames_per_channel)
	{
		if (throw_level (ThrowObject) && processed_data) {
			throw Exception(*this, "Tried to add silence to beginning after processing started");
		}
		add_to_beginning = frames_per_channel;
	}

	/** Tells that \a frames_per_channel frames of silence per channel should be added to end
	  * Needs to be called before end is reached.
	  * \n RT safe
	  */
	void add_silence_to_end (framecnt_t frames_per_channel)
	{
		if (throw_level (ThrowObject) && processed_data) {
			throw Exception(*this, "Tried to add silence to end after processing started");
		}
		add_to_end = frames_per_channel;
	}

	/** Tells whether ot nor silence should be trimmed from the beginning
	  * Has to be called before starting processing.
	  * \n RT safe
	  */
	void set_trim_beginning (bool yn)
	{
		if (throw_level (ThrowObject) && processed_data) {
			throw Exception(*this, "Tried to set beginning trim after processing started");
		}
		trim_beginning = yn;
	}

	/** Tells whether ot nor silence should be trimmed from the end
	  * Has to be called before the is reached.
	  * \n RT safe
	  */
	void set_trim_end (bool yn)
	{
		if (throw_level (ThrowObject) && processed_data) {
			throw Exception(*this, "Tried to set end trim after processing started");
		}
		trim_end = yn;
	}

	/** Process stream according to current settings.
	  * Note that some calls will not produce any output,
	  * while others may produce many. \see reset()
	  * \n RT safe
	  */
	void process (ProcessContext<T> const & c)
	{
		if (debug_level (DebugVerbose)) {
			debug_stream () << DebugUtils::demangled_name (*this) <<
				"::process()" << std::endl;
		}

		check_flags (*this, c);

		if (throw_level (ThrowStrict) && processing_finished) {
			throw Exception(*this, "process() after reaching end of input");
		}

		// delay end of input propagation until output/processing is complete
		processing_finished = c.has_flag (ProcessContext<T>::EndOfInput);
		c.remove_flag (ProcessContext<T>::EndOfInput);

		/* TODO this needs a general overhaul.
		 *
		 * - decouple "required silence duration" from buffer-size.
		 * - add hold-times for in/out
		 * - optional high pass filter (for DC offset)
		 * -> allocate a buffer "hold time" worth of samples.
		 * check if all samples in buffer are above/below threshold,
		 *
		 * https://github.com/x42/silan/blob/master/src/main.c#L130
		 * may lend itself for some inspiration.
		 */

		framecnt_t output_start_index = 0;
		framecnt_t output_sample_count = c.frames();

		if (!processed_data) {
			if (trim_beginning) {
				framecnt_t first_non_silent_frame_index = 0;
				if (find_first_non_silent_frame (c, first_non_silent_frame_index)) {
					// output from start of non-silent data until end of buffer
					// output_sample_count may also be altered in trim end
					output_start_index = first_non_silent_frame_index;
					output_sample_count = c.frames() - first_non_silent_frame_index;
					processed_data = true;
				} else {
					// keep entering this block until non-silence is found to trim
					processed_data = false;
				}
			} else {
				processed_data = true;
			}

			// This block won't be called again so add silence to beginning
			if (processed_data && add_to_beginning) {
				add_to_beginning *= c.channels ();
				output_silence_frames (c, add_to_beginning);
			}
		}

		if (processed_data) {
			if (trim_end) {
				framecnt_t first_non_silent_frame_index = 0;
				if (find_first_non_silent_frame (c, first_non_silent_frame_index)) {
					// context buffer contains non-silent data, flush any intermediate silence
					output_silence_frames (c, silence_frames);

					framecnt_t silent_frame_index = 0;
					find_last_silent_frame_reverse (c, silent_frame_index);

					// Count of samples at end of block that are "silent", may be zero.
					framecnt_t silent_end_samples = c.frames () - silent_frame_index;
					framecnt_t samples_before_silence = c.frames() - silent_end_samples;

					assert (samples_before_silence + silent_end_samples == c.frames ());

					// output_start_index may be non-zero if start trim occurred above
					output_sample_count = samples_before_silence - output_start_index;

					// keep track of any silent samples not output
					silence_frames = silent_end_samples;

				} else {
					// whole context buffer is silent output nothing
					silence_frames += c.frames ();
					output_sample_count = 0;
				}
			}

			// now output data if any
			ConstProcessContext<T> c_out (c, &c.data()[output_start_index], output_sample_count);
			ListedSource<T>::output (c_out);
		}

		// Finally, if in last process call, add silence to end
		if (processing_finished && processed_data && add_to_end) {
			add_to_end *= c.channels();
			output_silence_frames (c, add_to_end);
		}

		if (processing_finished) {
			// reset flag removed previous to processing above
			c.set_flag (ProcessContext<T>::EndOfInput);

			// Finally mark write complete by writing nothing with EndOfInput set
			// whether or not any data has been written
			ConstProcessContext<T> c_out(c, silence_buffer, 0);
			c_out().set_flag (ProcessContext<T>::EndOfInput);
			ListedSource<T>::output (c_out);
		}

	}

	using Sink<T>::process;

private:

	bool find_first_non_silent_frame (ProcessContext<T> const & c, framecnt_t & result_frame)
	{
		for (framecnt_t i = 0; i < c.frames(); ++i) {
			if (!tester.is_silent (c.data()[i])) {
				result_frame = i;
				// Round down to nearest interleaved "frame" beginning
				result_frame -= result_frame % c.channels();
				return true;
			}
		}
		return false;
	}

	/**
	 * Reverse find the last silent frame index. If the last sample in the
	 * buffer is non-silent the index will be one past the end of the buffer and
	 * equal to c.frames(). e.g silent_end_samples = c.frames() - result_frame
	 *
	 * @return true if result_frame index is valid, false if there were only
	 * silent samples in the context buffer
	 */
	bool find_last_silent_frame_reverse (ProcessContext<T> const & c, framecnt_t & result_frame)
	{
		framecnt_t last_sample_index = c.frames() - 1;

		for (framecnt_t i = last_sample_index; i >= 0; --i) {
			if (!tester.is_silent (c.data()[i])) {
				result_frame = i;
				// Round down to nearest interleaved "frame" beginning
				result_frame -= result_frame % c.channels();
				// Round up to return the "last" silent interleaved frame
				result_frame += c.channels();
				return true;
			}
		}
		return false;
	}

	void output_silence_frames (ProcessContext<T> const & c, framecnt_t & total_frames)
	{
		assert (!c.has_flag (ProcessContext<T>::EndOfInput));

		while (total_frames > 0) {
			framecnt_t frames = std::min (silence_buffer_size, total_frames);
			if (max_output_frames) {
				frames = std::min (frames, max_output_frames);
			}
			frames -= frames % c.channels();

			total_frames -= frames;
			ConstProcessContext<T> c_out (c, silence_buffer, frames);
			ListedSource<T>::output (c_out);
		}
	}

	bool       processed_data;
	bool       processing_finished;

	bool       trim_beginning;
	bool       trim_end;

	framecnt_t silence_frames;
	framecnt_t max_output_frames;

	framecnt_t add_to_beginning;
	framecnt_t add_to_end;

	framecnt_t silence_buffer_size;
	T *        silence_buffer;

	SilenceTester<T> tester;
};

} // namespace

#endif // AUDIOGRAPHER_SILENCE_TRIMMER_H
