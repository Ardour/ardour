/*
 *  basics.hpp
 *  Akupara
 *
 *  Created by Udi on 12/19/06.
 *  Copyright 2006 __MyCompanyName__. All rights reserved.
 *
 */
#if !defined(_AKUPARA_BASICS_HPP__INCLUDED_)
#define _AKUPARA_BASICS_HPP__INCLUDED_

#include "WavesPublicAPI/wstdint.h"

namespace Akupara
{
	// The ultimate nothingness
	// This is useful for writing constructors that nullify their object, and for testing nullness
	struct null_type 
	{
		null_type() {}
		null_type(const null_type *) {} // this allows 0 to be implicitly converted to null_type
	};
	inline null_type null() { return null_type(); }
	

	// This is a byte, guaranteed to be unsigned regardless of your compiler's char signedness
	typedef uint8_t byte_type;


	// derive from this if your class needs to be noncopyable
	class noncopyable_type
	{
	private:
		noncopyable_type(const noncopyable_type &);
		noncopyable_type &operator=(const noncopyable_type &);
	public:
		noncopyable_type() {}
	};


} // namespace Akupara


#if defined(__GNUC__)
#define AKUPARA_EXPECT_FALSE(x) __builtin_expect(x,false)
#define AKUPARA_EXPECT_TRUE(x)  __builtin_expect(x,true )
#else
#define AKUPARA_EXPECT_FALSE(x) x
#define AKUPARA_EXPECT_TRUE(x)  x
#endif // __GNUC__


#endif // _AKUPARA_BASICS_HPP__INCLUDED_
