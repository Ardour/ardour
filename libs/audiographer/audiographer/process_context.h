#ifndef AUDIOGRAPHER_PROCESS_CONTEXT_H
#define AUDIOGRAPHER_PROCESS_CONTEXT_H

#include <boost/static_assert.hpp>
#include <boost/type_traits.hpp>
#include <boost/format.hpp>

#include "exception.h"
#include "debug_utils.h"
#include "types.h"
#include "flag_field.h"
#include "throwing.h"
#include "type_utils.h"

namespace AudioGrapher
{


/**
 * Processing context. Constness only applies to data, not flags
 */

template <typename T = DefaultSampleType>
class ProcessContext
  : public Throwing<>
{
	// Support older compilers that don't support template base class initialization without template parameters
	// This will need to be modified if if it's modified above
	static const ThrowLevel throwLevel = DEFAULT_THROW_LEVEL;

	BOOST_STATIC_ASSERT (boost::has_trivial_destructor<T>::value);

public:

	typedef FlagField::Flag Flag;

	enum Flags {
		EndOfInput = 0
	};

public:

	/// Basic constructor with data, frame and channel count
	ProcessContext (T * data, framecnt_t frames, ChannelCount channels)
		: _data (data), _frames (frames), _channels (channels)
	{ validate_data(); }
	
	/// Normal copy constructor
	ProcessContext (ProcessContext<T> const & other)
		: Throwing<throwLevel>(), _data (other._data), _frames (other._frames), _channels (other._channels), _flags (other._flags)
	{ /* No need to validate data */ }
	
	/// "Copy constructor" with unique data, frame and channel count, but copies flags
	template<typename Y>
	ProcessContext (ProcessContext<Y> const & other, T * data, framecnt_t frames, ChannelCount channels)
		: Throwing<throwLevel>(), _data (data), _frames (frames), _channels (channels), _flags (other.flags())
	{ validate_data(); }
	
	/// "Copy constructor" with unique data and frame count, but copies channel count and flags
	template<typename Y>
	ProcessContext (ProcessContext<Y> const & other, T * data, framecnt_t frames)
		: Throwing<throwLevel>(), _data (data), _frames (frames), _channels (other.channels()), _flags (other.flags())
	{ validate_data(); }
	
	/// "Copy constructor" with unique data, but copies frame and channel count + flags
	template<typename Y>
	ProcessContext (ProcessContext<Y> const & other, T * data)
		: Throwing<throwLevel>(), _data (data), _frames (other.frames()), _channels (other.channels()), _flags (other.flags())
	{ /* No need to validate data */ }
	
	/// Make new Context out of the beginning of this context
	ProcessContext beginning (framecnt_t frames)
	{
		if (throw_level (ThrowProcess) && frames > _frames) {
			throw Exception (*this, boost::str (boost::format
				("Trying to use too many frames of %1% for a new Context: %2% instead of %3%")
				% DebugUtils::demangled_name (*this) % frames % _frames));
		}
		validate_data ();
		
		return ProcessContext (*this, _data, frames);
	}
	
	virtual ~ProcessContext () {}
	
	/// \a data points to the array of data to process
	inline T const *            data()     const { return _data; }
	inline T *                  data()           { return _data; }
	
	/// \a frames tells how many frames the array pointed by data contains
	inline framecnt_t const &    frames()   const { return _frames; }
	
	/** \a channels tells how many interleaved channels \a data contains
	  * If \a channels is greater than 1, each channel contains \a frames / \a channels frames of data
	  */
	inline ChannelCount const & channels() const { return _channels; }
	
	/// Returns the amount of frames per channel
	inline framecnt_t            frames_per_channel() const { return _frames / _channels; }

	/* Flags */
	
	inline bool has_flag (Flag flag)    const { return _flags.has (flag); }
	inline void set_flag (Flag flag)    const { _flags.set (flag); }
	inline void remove_flag (Flag flag) const { _flags.remove (flag); }
	inline FlagField const & flags ()   const { return _flags; }
	
protected:
	T * const              _data;
	framecnt_t              _frames;
	ChannelCount           _channels;
	
	mutable FlagField      _flags;

  private:
	inline void validate_data()
	{
		if (throw_level (ThrowProcess) && (_frames % _channels != 0)) {
			throw Exception (*this, boost::str (boost::format
				("Number of frames given to %1% was not a multiple of channels: %2% frames with %3% channels")
				% DebugUtils::demangled_name (*this) % _frames % _channels));
		}
	}
};

/// A process context that allocates and owns it's data buffer
template <typename T = DefaultSampleType>
class AllocatingProcessContext : public ProcessContext<T>
{
public:
	/// Allocates uninitialized memory
	AllocatingProcessContext (framecnt_t frames, ChannelCount channels)
		: ProcessContext<T> (new T[frames], frames, channels) {}
	
	/// Allocates and copies data from raw buffer
	AllocatingProcessContext (T const * data, framecnt_t frames, ChannelCount channels)
		: ProcessContext<T> (new T[frames], frames, channels)
	{ TypeUtils<float>::copy (data, ProcessContext<T>::_data, frames); }
	
	/// Copy constructor, copies data from other ProcessContext
	AllocatingProcessContext (ProcessContext<T> const & other)
		: ProcessContext<T> (other, new T[other._frames])
	{ TypeUtils<float>::copy (ProcessContext<T>::_data, other._data, other._frames); }
	
	/// "Copy constructor" with uninitialized data, unique frame and channel count, but copies flags
	template<typename Y>
	AllocatingProcessContext (ProcessContext<Y> const & other, framecnt_t frames, ChannelCount channels)
		: ProcessContext<T> (other, new T[frames], frames, channels) {}
	
	/// "Copy constructor" with uninitialized data, unique frame count, but copies channel count and flags
	template<typename Y>
	AllocatingProcessContext (ProcessContext<Y> const & other, framecnt_t frames)
		: ProcessContext<T> (other, new T[frames], frames, other.channels()) {}
	
	/// "Copy constructor" uninitialized data, that copies frame and channel count + flags
	template<typename Y>
	AllocatingProcessContext (ProcessContext<Y> const & other)
		: ProcessContext<T> (other, new T[other._frames]) {}
	
	~AllocatingProcessContext () { delete [] ProcessContext<T>::_data; }
};

/// A wrapper for a const ProcesContext which can be created from const data
template <typename T = DefaultSampleType>
class ConstProcessContext
{
  public:
	/// Basic constructor with data, frame and channel count
	ConstProcessContext (T const * data, framecnt_t frames, ChannelCount channels)
	  : context (const_cast<T *>(data), frames, channels) {}
	
	/// Copy constructor from const ProcessContext
	ConstProcessContext (ProcessContext<T> const & other)
	  : context (const_cast<ProcessContext<T> &> (other)) {}
	
	/// "Copy constructor", with unique data, frame and channel count, but copies flags
	template<typename ProcessContext>
	ConstProcessContext (ProcessContext const & other, T const * data, framecnt_t frames, ChannelCount channels)
		: context (other, const_cast<T *>(data), frames, channels) {}
	
	/// "Copy constructor", with unique data and frame count, but copies channel count and flags
	template<typename ProcessContext>
	ConstProcessContext (ProcessContext const & other, T const * data, framecnt_t frames)
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
