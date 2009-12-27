#ifndef AUDIOGRAPHER_TYPE_UTILS_H
#define AUDIOGRAPHER_TYPE_UTILS_H

#include "types.h"
#include <boost/static_assert.hpp>
#include <boost/type_traits.hpp>
#include <memory>
#include <algorithm>

namespace AudioGrapher
{

class TypeUtilsBase
{
  protected:
	
	template<typename T, bool b>
	static void do_fill(T * buffer, nframes_t frames, const boost::integral_constant<bool, b>&)
		{ std::uninitialized_fill_n (buffer, frames, T()); }

	template<typename T>
	static void do_fill(T * buffer, nframes_t frames, const boost::true_type&)
		{ memset (buffer, frames * sizeof(T), 0); }
	
  private:
};

template<typename T>
class TypeUtils : private TypeUtilsBase
{
	BOOST_STATIC_ASSERT (boost::has_trivial_destructor<T>::value);
	
	typedef boost::integral_constant<bool, 
			boost::is_floating_point<T>::value ||
			boost::is_signed<T>::value> zero_fillable;
  public:
	inline static void zero_fill (T * buffer, nframes_t frames)
		{ do_zero_fill(buffer, frames, zero_fillable()); }
	
	inline static void copy (T* source, T* destination, nframes_t frames)
		{ std::uninitialized_copy (source, &source[frames], destination); }
	
	inline static void move (T* source, T* destination, nframes_t frames)
	{
		if (destination < source) {
			std::copy (source, &source[frames], destination);
		} else {
			std::copy_backward (source, &source[frames], destination);
		}
	}
	
  private:

};


} // namespace

#endif // AUDIOGRAPHER_TYPE_UTILS_H
