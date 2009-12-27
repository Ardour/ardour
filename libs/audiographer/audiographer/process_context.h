#ifndef AUDIOGRAPHER_PROCESS_CONTEXT_H
#define AUDIOGRAPHER_PROCESS_CONTEXT_H

#include <boost/static_assert.hpp>
#include <boost/type_traits.hpp>

#include "types.h"
#include "type_utils.h"

namespace AudioGrapher
{

/**
 * Processing context. Constness only applies to data, not flags
 */

template <typename T>
class ProcessContext  {

	BOOST_STATIC_ASSERT (boost::has_trivial_destructor<T>::value);

public:

	typedef FlagField::Flag Flag;

	enum Flags {
		EndOfInput = 0
	};

public:

	/// Basic constructor with data, frame and channel count
	ProcessContext (T * data, nframes_t frames, ChannelCount channels)
		: _data (data), _frames (frames), _channels (channels) {}
	
	/// Normal copy constructor
	ProcessContext (ProcessContext<T> const & other)
		: _data (other._data), _frames (other._frames), _channels (other._channels), _flags (other._flags) {}
	
	/// "Copy constructor" with unique data, frame and channel count, but copies flags
	template<typename Y>
	ProcessContext (ProcessContext<Y> const & other, T * data, nframes_t frames, ChannelCount channels)
		: _data (data), _frames (frames), _channels (channels), _flags (other.flags()) {}
	
	/// "Copy constructor" with unique data and frame count, but copies channel count and flags
	template<typename Y>
	ProcessContext (ProcessContext<Y> const & other, T * data, nframes_t frames)
		: _data (data), _frames (frames), _channels (other.channels()), _flags (other.flags()) {}
	
	/// "Copy constructor" with unique data, but copies frame and channel count + flags
	template<typename Y>
	ProcessContext (ProcessContext<Y> const & other, T * data)
		: _data (data), _frames (other.frames()), _channels (other.channels()), _flags (other.flags()) {}
	
	virtual ~ProcessContext () {}
	
	/// \a data points to the array of data to process
	inline T const *            data()     const { return _data; }
	inline T *                  data()           { return _data; }
	
	/// \a frames tells how many frames the array pointed by data contains
	inline nframes_t const &    frames()   const { return _frames; }
	inline nframes_t &          frames()         { return _frames; }
	
	/** \a channels tells how many interleaved channels \a data contains
	  * If \a channels is greater than 1, each channel contains \a frames / \a channels frames of data
	  */
	inline ChannelCount const & channels() const { return _channels; }
	inline ChannelCount &       channels()       { return _channels; }
	
	/// Returns the amount of frames per channel
	inline nframes_t            frames_per_channel() const { return _frames / _channels; }

	/* Flags */
	
	inline bool has_flag (Flag flag)    const { return _flags.has (flag); }
	inline void set_flag (Flag flag)    const { _flags.set (flag); }
	inline void remove_flag (Flag flag) const { _flags.remove (flag); }
	inline FlagField const & flags ()   const { return _flags; }
	
protected:
	T * const              _data;
	nframes_t              _frames;
	ChannelCount           _channels;
	
	mutable FlagField      _flags;
};

/// A process context that allocates and owns it's data buffer
template <typename T>
struct AllocatingProcessContext : public ProcessContext<T>
{
	/// Allocates uninitialized memory
	AllocatingProcessContext (nframes_t frames, ChannelCount channels)
		: ProcessContext<T> (new T[frames], frames, channels) {}
	
	/// Copy constructor, copies data from other ProcessContext
	AllocatingProcessContext (ProcessContext<T> const & other)
		: ProcessContext<T> (other, new T[other._frames])
	{ memcpy (ProcessContext<T>::_data, other._data, other._channels * other._frames * sizeof (T)); }
	
	/// "Copy constructor" with uninitialized data, unique frame and channel count, but copies flags
	template<typename Y>
	AllocatingProcessContext (ProcessContext<Y> const & other, nframes_t frames, ChannelCount channels)
		: ProcessContext<T> (other, new T[frames], frames, channels) {}
	
	/// "Copy constructor" with uninitialized data, unique frame count, but copies channel count and flags
	template<typename Y>
	AllocatingProcessContext (ProcessContext<Y> const & other, nframes_t frames)
		: ProcessContext<T> (other, new T[frames], frames, other.channels()) {}
	
	/// "Copy constructor" uninitialized data, that copies frame and channel count + flags
	template<typename Y>
	AllocatingProcessContext (ProcessContext<Y> const & other)
		: ProcessContext<T> (other, new T[other._frames]) {}
	
	~AllocatingProcessContext () { delete [] ProcessContext<T>::_data; }
};

/// A wrapper for a const ProcesContext which can be created from const data
template <typename T>
class ConstProcessContext
{
  public:
	/// Basic constructor with data, frame and channel count
	ConstProcessContext (T const * data, nframes_t frames, ChannelCount channels)
	  : context (const_cast<T *>(data), frames, channels) {}
	
	/// Copy constructor from const ProcessContext
	ConstProcessContext (ProcessContext<T> const & other)
	  : context (const_cast<ProcessContext<T> &> (other)) {}
	
	/// "Copy constructor", with unique data, frame and channel count, but copies flags
	template<typename ProcessContext>
	ConstProcessContext (ProcessContext const & other, T const * data, nframes_t frames, ChannelCount channels)
		: context (other, const_cast<T *>(data), frames, channels) {}
	
	/// "Copy constructor", with unique data and frame count, but copies channel count and flags
	template<typename ProcessContext>
	ConstProcessContext (ProcessContext const & other, T const * data, nframes_t frames)
		: context (other, const_cast<T *>(data), frames) {}
	
	/// "Copy constructor", with unique data, but copies frame and channel count + flags
	template<typename ProcessContext>
	ConstProcessContext (ProcessContext const & other, T const * data)
		: context (other, const_cast<T *>(data)) {}

	inline operator ProcessContext<T> const & () { return context; }
	inline ProcessContext<T> const & operator() () { return context; }
	inline ProcessContext<T> const * operator& () { return &context; }

  private:
	  ProcessContext<T> const context;
};

} // namespace

#endif // AUDIOGRAPHER_PROCESS_CONTEXT_H
