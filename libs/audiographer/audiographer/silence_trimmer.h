#ifndef AUDIOGRAPHER_SILENCE_TRIMMER_H
#define AUDIOGRAPHER_SILENCE_TRIMMER_H

#include "listed_source.h"
#include "sink.h"
#include "exception.h"
#include "utils.h"

#include <cstring>

namespace AudioGrapher {

template<typename T>
class SilenceTrimmer : public ListedSource<T>, public Sink<T>
{
  public:

	SilenceTrimmer()
	{
	reset ();
	}

	void reset()
	{
		in_beginning = true;
		in_end = false;
		trim_beginning = false;
		trim_end = false;
		silence_frames = 0;
		max_output_frames = 0;
		add_to_beginning = 0;
		add_to_end = 0;
	}
	
	void add_silence_to_beginning (nframes_t frames_per_channel)
	{
		if (!in_beginning) {
			throw Exception(*this, "Tried to add silence to beginning after already outputting data");
		}
		add_to_beginning = frames_per_channel;
	}
	
	void add_silence_to_end (nframes_t frames_per_channel)
	{
		if (in_end) {
			throw Exception(*this, "Tried to add silence to end after already reaching end");
		}
		add_to_end = frames_per_channel;
	}
	
	void set_trim_beginning (bool yn)
	{
		if (!in_beginning) {
			throw Exception(*this, "Tried to set beginning trim after already outputting data");
		}
		trim_beginning = yn;
	}
	
	void set_trim_end (bool yn)
	{
		if (in_end) {
			throw Exception(*this, "Tried to set end trim after already reaching end");
		}
		trim_end = yn;
	}
	
	void limit_output_size (nframes_t max_frames)
	{
		max_output_frames = max_frames;
	}

	void process (ProcessContext<T> const & c)
	{
		if (in_end) { throw Exception(*this, "process() after reacing end of input"); }
		in_end = c.has_flag (ProcessContext<T>::EndOfInput);
		
		nframes_t frame_index = 0;
		
		if (in_beginning) {
			
			bool has_data = true;
			
			// only check silence if doing either of these
			// This will set both has_data and frame_index
			if (add_to_beginning || trim_beginning) {
				has_data = find_first_non_zero_sample (c, frame_index);
			}
			
			// Added silence if there is silence to add
			if (add_to_beginning) {
				ConstProcessContext<T> c_copy (c);
				if (has_data) { // There will be more output, so remove flag
					c_copy().remove_flag (ProcessContext<T>::EndOfInput);
				}
				add_to_beginning *= c.channels();
				output_silence_frames (c_copy, add_to_beginning);
			}
			
			// If we are not trimming the beginning, output everything
			// Then has_data = true and frame_index = 0
			// Otherwise these reflect the silence state
			if (has_data) {
				in_beginning = false;
				ConstProcessContext<T> c_out (c, &c.data()[frame_index], c.frames() - frame_index);
				ListedSource<T>::output (c_out);
			}
			
		} else if (trim_end) { // Only check zero samples if trimming end
			
			if (find_first_non_zero_sample (c, frame_index)) {
				// context contains non-zero data
				output_silence_frames (c, silence_frames); // flush intermediate silence
				ListedSource<T>::output (c); // output rest of data
			} else { // whole context is zero
				silence_frames += c.frames();
			}
			
		} else { // no need to do anything special
			
			ListedSource<T>::output (c);
		}
		
		// Finally if in end, add silence to end
		if (in_end && add_to_end) {
			add_to_end *= c.channels();
			output_silence_frames (c, add_to_end, true);
		}
	}

	using Sink<T>::process;

  private:

	bool find_first_non_zero_sample (ProcessContext<T> const & c, nframes_t & result_frame)
	{
		for (nframes_t i = 0; i < c.frames(); ++i) {
			if (c.data()[i] != static_cast<T>(0.0)) {
				result_frame = i;
				// Round down to nearest interleaved "frame" beginning
				result_frame -= result_frame % c.channels();
				return true;
			}
		}
		return false;
	}
	
	void output_silence_frames (ProcessContext<T> const & c, nframes_t & total_frames, bool adding_to_end = false)
	{
		nframes_t silence_buffer_size = Utils::get_zero_buffer_size<T>();
		if (silence_buffer_size == 0) { throw Exception (*this, "Utils::init_zeros has not been called!"); }
		
		bool end_of_input = c.has_flag (ProcessContext<T>::EndOfInput);
		c.remove_flag (ProcessContext<T>::EndOfInput);
		
		while (total_frames > 0) {
			nframes_t frames = std::min (silence_buffer_size, total_frames);
			if (max_output_frames) {
				frames = std::min (frames, max_output_frames);
			}
			frames -= frames % c.channels();
			
			total_frames -= frames;
			ConstProcessContext<T> c_out (c, Utils::get_zeros<T>(frames), frames);
			
			// boolean commentation :)
			bool const no_more_silence_will_be_added = adding_to_end || (add_to_end == 0);
			bool const is_last_frame_output_in_this_function = (total_frames == 0);
			if (end_of_input && no_more_silence_will_be_added && is_last_frame_output_in_this_function) {
				c_out().set_flag (ProcessContext<T>::EndOfInput);
			}
			ListedSource<T>::output (c_out);
		}
	}


	bool      in_beginning;
	bool      in_end;
	
	bool      trim_beginning;
	bool      trim_end;
	
	nframes_t silence_frames;
	nframes_t max_output_frames;
	
	nframes_t add_to_beginning;
	nframes_t add_to_end;
};

} // namespace

#endif // AUDIOGRAPHER_SILENCE_TRIMMER_H
