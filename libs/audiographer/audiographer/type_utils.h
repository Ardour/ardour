#ifndef AUDIOGRAPHER_TYPE_UTILS_H
#define AUDIOGRAPHER_TYPE_UTILS_H

#include <boost/static_assert.hpp>
#include <boost/type_traits.hpp>
#include <memory>
#include <algorithm>
#include <cstring>

#include "audiographer/visibility.h"
#include "audiographer/types.h"

namespace AudioGrapher
{

/// Non-template base class for TypeUtils
class LIBAUDIOGRAPHER_API TypeUtilsBase
{
  protected:

	template<typename T, bool b>
	static void do_zero_fill(T * buffer, samplecnt_t samples, const boost::integral_constant<bool, b>&)
		{ std::uninitialized_fill_n (buffer, samples, T()); }

	template<typename T>
	static void do_zero_fill(T * buffer, samplecnt_t samples, const boost::true_type&)
		{ memset (buffer, 0, samples * sizeof(T)); }
};

/// Utilities for initializing, copying, moving, etc. data
template<typename T = DefaultSampleType>
class /*LIBAUDIOGRAPHER_API*/ TypeUtils : private TypeUtilsBase
{
	BOOST_STATIC_ASSERT (boost::has_trivial_destructor<T>::value);

	typedef boost::integral_constant<bool,
			boost::is_floating_point<T>::value ||
			boost::is_signed<T>::value> zero_fillable;
  public:
	/** Fill buffer with a zero value
	  * The value used for filling is either 0 or the value of T()
	  * if T is not a floating point or signed integer type
	  * \n RT safe
	  */
	inline static void zero_fill (T * buffer, samplecnt_t samples)
		{ do_zero_fill(buffer, samples, zero_fillable()); }

	/** Copies \a samples frames of data from \a source to \a destination
	  * The source and destination may NOT overlap.
	  * \n RT safe
	  */
	inline static void copy (T const * source, T * destination, samplecnt_t samples)
		{ std::uninitialized_copy (source, &source[samples], destination); }

	/** Moves \a samples frames of data from \a source to \a destination
	  * The source and destination may overlap in any way.
	  * \n RT safe
	  */
	inline static void move (T const * source, T * destination, samplecnt_t samples)
	{
		if (destination < source) {
			std::copy (source, &source[samples], destination);
		} else if (destination > source) {
			std::copy_backward (source, &source[samples], destination + samples);
		}
	}
};


} // namespace

#endif // AUDIOGRAPHER_TYPE_UTILS_H
