#ifndef AUDIOGRAPHER_PROCESS_CONTEXT_H
#define AUDIOGRAPHER_PROCESS_CONTEXT_H

#include <boost/static_assert.hpp>
#include <boost/type_traits.hpp>
#include <boost/format.hpp>

#include "audiographer/visibility.h"
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
class /*LIBAUDIOGRAPHER_API*/ ProcessContext
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

	/// Basic constructor with data, sample and channel count
	ProcessContext (T * data, samplecnt_t samples, ChannelCount channels)
		: _data (data), _samples (samples), _channels (channels)
	{ validate_data(); }

	/// Normal copy constructor
	ProcessContext (ProcessContext<T> const & other)
		: Throwing<throwLevel>(), _data (other._data), _samples (other._samples), _channels (other._channels), _flags (other._flags)
	{ /* No need to validate data */ }

	/// "Copy constructor" with unique data, sample and channel count, but copies flags
	template<typename Y>
	ProcessContext (ProcessContext<Y> const & other, T * data, samplecnt_t samples, ChannelCount channels)
		: Throwing<throwLevel>(), _data (data), _samples (samples), _channels (channels), _flags (other.flags())
	{ validate_data(); }

	/// "Copy constructor" with unique data and sample count, but copies channel count and flags
	template<typename Y>
	ProcessContext (ProcessContext<Y> const & other, T * data, samplecnt_t samples)
		: Throwing<throwLevel>(), _data (data), _samples (samples), _channels (other.channels()), _flags (other.flags())
	{ validate_data(); }

	/// "Copy constructor" with unique data, but copies sample and channel count + flags
	template<typename Y>
	ProcessContext (ProcessContext<Y> const & other, T * data)
		: Throwing<throwLevel>(), _data (data), _samples (other.samples()), _channels (other.channels()), _flags (other.flags())
	{ /* No need to validate data */ }

	/// Make new Context out of the beginning of this context
	ProcessContext beginning (samplecnt_t samples)
	{
		if (throw_level (ThrowProcess) && samples > _samples) {
			throw Exception (*this, boost::str (boost::format
				("Trying to use too many samples of %1% for a new Context: %2% instead of %3%")
				% DebugUtils::demangled_name (*this) % samples % _samples));
		}
		validate_data ();

		return ProcessContext (*this, _data, samples);
	}

	virtual ~ProcessContext () {}

	/// \a data points to the array of data to process
	inline T const *            data()     const { return _data; }
	inline T *                  data()           { return _data; }

	/// \a samples tells how many samples the array pointed by data contains
	inline samplecnt_t const &    samples()   const { return _samples; }

	/** \a channels tells how many interleaved channels \a data contains
	  * If \a channels is greater than 1, each channel contains \a samples / \a channels samples of data
	  */
	inline ChannelCount const & channels() const { return _channels; }

	/// Returns the amount of samples per channel
	inline samplecnt_t            samples_per_channel() const { return _samples / _channels; }

	/* Flags */

	inline bool has_flag (Flag flag)    const { return _flags.has (flag); }
	inline void set_flag (Flag flag)    const { _flags.set (flag); }
	inline void remove_flag (Flag flag) const { _flags.remove (flag); }
	inline FlagField const & flags ()   const { return _flags; }

protected:
	T * const              _data;
	samplecnt_t              _samples;
	ChannelCount           _channels;

	mutable FlagField      _flags;

  private:
	inline void validate_data()
	{
		if (throw_level (ThrowProcess) && (_samples % _channels != 0)) {
			throw Exception (*this, boost::str (boost::format
				("Number of samples given to %1% was not a multiple of channels: %2% samples with %3% channels")
				% DebugUtils::demangled_name (*this) % _samples % _channels));
		}
	}
};

/// A process context that allocates and owns it's data buffer
template <typename T = DefaultSampleType>
class /*LIBAUDIOGRAPHER_API*/ AllocatingProcessContext : public ProcessContext<T>
{
public:
	/// Allocates uninitialized memory
	AllocatingProcessContext (samplecnt_t samples, ChannelCount channels)
		: ProcessContext<T> (new T[samples], samples, channels) {}

	/// Allocates and copies data from raw buffer
	AllocatingProcessContext (T const * data, samplecnt_t samples, ChannelCount channels)
		: ProcessContext<T> (new T[samples], samples, channels)
	{ TypeUtils<float>::copy (data, ProcessContext<T>::_data, samples); }

	/// Copy constructor, copies data from other ProcessContext
	AllocatingProcessContext (ProcessContext<T> const & other)
		: ProcessContext<T> (other, new T[other._samples])
	{ TypeUtils<float>::copy (ProcessContext<T>::_data, other._data, other._samples); }

	/// "Copy constructor" with uninitialized data, unique sample and channel count, but copies flags
	template<typename Y>
	AllocatingProcessContext (ProcessContext<Y> const & other, samplecnt_t samples, ChannelCount channels)
		: ProcessContext<T> (other, new T[samples], samples, channels) {}

	/// "Copy constructor" with uninitialized data, unique sample count, but copies channel count and flags
	template<typename Y>
	AllocatingProcessContext (ProcessContext<Y> const & other, samplecnt_t samples)
		: ProcessContext<T> (other, new T[samples], samples, other.channels()) {}

	/// "Copy constructor" uninitialized data, that copies sample and channel count + flags
	template<typename Y>
	AllocatingProcessContext (ProcessContext<Y> const & other)
		: ProcessContext<T> (other, new T[other._samples]) {}

	~AllocatingProcessContext () { delete [] ProcessContext<T>::_data; }
};

/// A wrapper for a const ProcesContext which can be created from const data
template <typename T = DefaultSampleType>
class /*LIBAUDIOGRAPHER_API*/ ConstProcessContext
{
  public:
	/// Basic constructor with data, sample and channel count
	ConstProcessContext (T const * data, samplecnt_t samples, ChannelCount channels)
	  : context (const_cast<T *>(data), samples, channels) {}

	/// Copy constructor from const ProcessContext
	ConstProcessContext (ProcessContext<T> const & other)
	  : context (const_cast<ProcessContext<T> &> (other)) {}

	/// "Copy constructor", with unique data, sample and channel count, but copies flags
	template<typename ProcessContext>
	ConstProcessContext (ProcessContext const & other, T const * data, samplecnt_t samples, ChannelCount channels)
		: context (other, const_cast<T *>(data), samples, channels) {}

	/// "Copy constructor", with unique data and sample count, but copies channel count and flags
	template<typename ProcessContext>
	ConstProcessContext (ProcessContext const & other, T const * data, samplecnt_t samples)
		: context (other, const_cast<T *>(data), samples) {}

	/// "Copy constructor", with unique data, but copies sample and channel count + flags
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
