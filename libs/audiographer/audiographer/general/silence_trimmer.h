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
	SilenceTrimmer(framecnt_t silence_buffer_size_ = 1024)
	  : silence_buffer_size (0)
	  , silence_buffer (0)
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
		
		in_beginning = true;
		in_end = false;
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
		if (throw_level (ThrowObject) && !in_beginning) {
			throw Exception(*this, "Tried to add silence to beginning after already outputting data");
		}
		add_to_beginning = frames_per_channel;
	}
	
	/** Tells that \a frames_per_channel frames of silence per channel should be added to end
	  * Needs to be called before end is reached.
	  * \n RT safe
	  */
	void add_silence_to_end (framecnt_t frames_per_channel)
	{
		if (throw_level (ThrowObject) && in_end) {
			throw Exception(*this, "Tried to add silence to end after already reaching end");
		}
		add_to_end = frames_per_channel;
	}
	
	/** Tells whether ot nor silence should be trimmed from the beginning
	  * Has to be called before starting processing.
	  * \n RT safe
	  */
	void set_trim_beginning (bool yn)
	{
		if (throw_level (ThrowObject) && !in_beginning) {
			throw Exception(*this, "Tried to set beginning trim after already outputting data");
		}
		trim_beginning = yn;
	}
	
	/** Tells whether ot nor silence should be trimmed from the end
	  * Has to be called before the is reached.
	  * \n RT safe
	  */
	void set_trim_end (bool yn)
	{
		if (throw_level (ThrowObject) && in_end) {
			throw Exception(*this, "Tried to set end trim after already reaching end");
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
		
		if (throw_level (ThrowStrict) && in_end) {
			throw Exception(*this, "process() after reacing end of input");
		}
		in_end = c.has_flag (ProcessContext<T>::EndOfInput);

		// If adding to end, delay end of input propagation
		if (add_to_end) { c.remove_flag(ProcessContext<T>::EndOfInput); }
		
		framecnt_t frame_index = 0;
		
		if (in_beginning) {
			
			bool has_data = true;
			
			// only check silence if doing either of these
			// This will set both has_data and frame_index
			if (add_to_beginning || trim_beginning) {
				has_data = find_first_non_zero_sample (c, frame_index);
			}
			
			// Added silence if there is silence to add
			if (add_to_beginning) {
				
				if (debug_level (DebugVerbose)) {
					debug_stream () << DebugUtils::demangled_name (*this) <<
						" adding to beginning" << std::endl;
				}
				
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
				
				if (debug_level (DebugVerbose)) {
					debug_stream () << DebugUtils::demangled_name (*this) <<
						" outputting whole frame to beginning" << std::endl;
				}
				
				in_beginning = false;
				ConstProcessContext<T> c_out (c, &c.data()[frame_index], c.frames() - frame_index);
				ListedSource<T>::output (c_out);
			}
			
		} else if (trim_end) { // Only check zero samples if trimming end
			
			if (find_first_non_zero_sample (c, frame_index)) {
				
				if (debug_level (DebugVerbose)) {
					debug_stream () << DebugUtils::demangled_name (*this) <<
						" flushing intermediate silence and outputting frame" << std::endl;
				}
				
				// context contains non-zero data
				output_silence_frames (c, silence_frames); // flush intermediate silence
				ListedSource<T>::output (c); // output rest of data
			} else { // whole context is zero
				
				if (debug_level (DebugVerbose)) {
					debug_stream () << DebugUtils::demangled_name (*this) <<
						" no, output, adding frames to silence count" << std::endl;
				}
				
				silence_frames += c.frames();
			}
			
		} else { // no need to do anything special
			
			if (debug_level (DebugVerbose)) {
				debug_stream () << DebugUtils::demangled_name (*this) <<
					" outputting whole frame in middle" << std::endl;
			}
			
			ListedSource<T>::output (c);
		}
		
		// Finally, if in end, add silence to end
		if (in_end && add_to_end) {
			c.set_flag (ProcessContext<T>::EndOfInput);

			if (debug_level (DebugVerbose)) {
				debug_stream () << DebugUtils::demangled_name (*this) <<
					" adding to end" << std::endl;
			}
			
			add_to_end *= c.channels();
			output_silence_frames (c, add_to_end, true);
		}
	}

	using Sink<T>::process;

  private:

	bool find_first_non_zero_sample (ProcessContext<T> const & c, framecnt_t & result_frame)
	{
		for (framecnt_t i = 0; i < c.frames(); ++i) {
			if (c.data()[i] != static_cast<T>(0.0)) {
				result_frame = i;
				// Round down to nearest interleaved "frame" beginning
				result_frame -= result_frame % c.channels();
				return true;
			}
		}
		return false;
	}
	
	void output_silence_frames (ProcessContext<T> const & c, framecnt_t & total_frames, bool adding_to_end = false)
	{
		bool end_of_input = c.has_flag (ProcessContext<T>::EndOfInput);
		c.remove_flag (ProcessContext<T>::EndOfInput);
		
		while (total_frames > 0) {
			framecnt_t frames = std::min (silence_buffer_size, total_frames);
			if (max_output_frames) {
				frames = std::min (frames, max_output_frames);
			}
			frames -= frames % c.channels();
			
			total_frames -= frames;
			ConstProcessContext<T> c_out (c, silence_buffer, frames);
			
			// boolean commentation :)
			bool const no_more_silence_will_be_added = adding_to_end || (add_to_end == 0);
			bool const is_last_frame_output_in_this_function = (total_frames == 0);
			if (end_of_input && no_more_silence_will_be_added && is_last_frame_output_in_this_function) {
				c_out().set_flag (ProcessContext<T>::EndOfInput);
			}
			ListedSource<T>::output (c_out);
		}

		// Add the flag back if it was removed
		if (end_of_input) { c.set_flag (ProcessContext<T>::EndOfInput); }
	}


	bool       in_beginning;
	bool       in_end;
	
	bool       trim_beginning;
	bool       trim_end;
	
	framecnt_t silence_frames;
	framecnt_t max_output_frames;
	
	framecnt_t add_to_beginning;
	framecnt_t add_to_end;
	
	framecnt_t silence_buffer_size;
	T *        silence_buffer;
};

} // namespace

#endif // AUDIOGRAPHER_SILENCE_TRIMMER_H
