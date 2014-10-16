/*
    Copyright (C) 2014 Waves Audio Ltd.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __WCFourCC_h__
	#define __WCFourCC_h__
	
/* Copy to include
#include "BasicTypes/WCFourCC.h"
*/

//#include "BasicTypes/WTByteOrder.h"
#include "WCFixedString.h"


// These are preprocessor macros rather than inline functions because most compilers can't
// resolve functions at compile-time.
#if _BYTEORDER_BIG_ENDIAN==1
	#define FOURCC_BIG(a, b, c, d)    ((uint32_t(a)<<24)|(uint32_t(b)<<16)|(uint32_t(c)<< 8)|(uint32_t(d)<< 0))
	#define FOURCC_LITTLE(a, b, c, d) ((uint32_t(a)<< 0)|(uint32_t(b)<< 8)|(uint32_t(c)<<16)|(uint32_t(d)<<24))
	#define FOURCC_COMPILER(a, b, c, d) FOURCC_BIG(a,b,c,d)
#elif _BYTEORDER_BIG_ENDIAN==0
	#define FOURCC_BIG(a, b, c, d)    ((uint32_t(a)<< 0)|(uint32_t(b)<< 8)|(uint32_t(c)<<16)|(uint32_t(d)<<24))
	#define FOURCC_LITTLE(a, b, c, d) ((uint32_t(a)<<24)|(uint32_t(b)<<16)|(uint32_t(c)<< 8)|(uint32_t(d)<< 0))
	#define FOURCC_COMPILER(a, b, c, d) FOURCC_LITTLE(a,b,c,d)
#else
	#error _BYTEORDER_BIG_ENDIAN not defined proparly
#endif // _BYTEORDER_HPP_BIG_ENDIAN

typedef uint32_t WTFourCharCode;

#ifndef kEnableWCFourCCDebug
	#define kEnableWCFourCCDebug 0 // set to 1 to enable debug members
#endif


class WCFourCC
{
private:
	template<class _iter> 
	static WTFourCharCode stored_from_iter(_iter& i)
	{
		return s_stored_byte_order==wvNS::wvBO::byte_order_big_endian ? FOURCC_BIG(i[0], i[1], i[2], i[3]) : FOURCC_LITTLE(i[0], i[1], i[2], i[3]);
	}

public:

	//	static const WCFourCC kDefaultFourCC_prv;

	static WCFourCC kDefaultFourCC_prv() { return WCFourCC(); }

	// change this line will change the byte order in which WCFourCC keeps the four char code
	static const wvNS::wvBO::byte_order_type s_stored_byte_order = wvNS::wvBO::compiler_byte_order;

	WCFourCC(const char a, const char b, const char c, const char d) : 
		m_stored_value(s_stored_byte_order==wvNS::wvBO::compiler_byte_order ? FOURCC_BIG(a,b,c,d) : FOURCC_LITTLE(a,b,c,d))
	{
#if kEnableWCFourCCDebug == 1
		m_c_str_stored_value[sizeof(WTFourCharCode)] = '\0';
#endif
	}

	WCFourCC() :
		m_stored_value(FOURCC_BIG('?','?','?','?'))	 // since the four chars are the same, there is no need to choose between big & little
	{
#if kEnableWCFourCCDebug == 1
		m_c_str_stored_value[sizeof(WTFourCharCode)] = '\0';
#endif
	}

	WCFourCC(const WTFourCharCode in_fourCharCode, const wvNS::wvBO::byte_order_type in_byteOrder = wvNS::wvBO::compiler_byte_order) :
		m_stored_value(in_byteOrder==s_stored_byte_order ? in_fourCharCode : wvNS::wvBO::swap32(in_fourCharCode))
	{
#if kEnableWCFourCCDebug == 1
		m_c_str_stored_value[sizeof(WTFourCharCode)] = '\0';
#endif
	}

	explicit WCFourCC(const char* in_source_string) :
		m_stored_value(stored_from_iter(in_source_string))
	{
#if kEnableWCFourCCDebug == 1
		m_c_str_stored_value[sizeof(WTFourCharCode)] = '\0';
#endif
	}

	explicit WCFourCC(const WCFixedStringBase& in_source_string) :
		m_stored_value(stored_from_iter(in_source_string))
	{
#if kEnableWCFourCCDebug == 1
		m_c_str_stored_value[sizeof(WTFourCharCode)] = '\0';
#endif
	}

	WTFourCharCode GetAsSomeEndian(const wvNS::wvBO::byte_order_type in_byteOrder) const
	{
		return s_stored_byte_order==in_byteOrder ? m_stored_value : wvNS::wvBO::swap32(m_stored_value);
	}

	WTFourCharCode GetAsBigEndian() const
	{
		return s_stored_byte_order==wvNS::wvBO::byte_order_big_endian ? m_stored_value : wvNS::wvBO::swap32(m_stored_value);
	}

	WTFourCharCode GetAsLittleEndian() const
	{
		return s_stored_byte_order==wvNS::wvBO::byte_order_little_endian ? m_stored_value : wvNS::wvBO::swap32(m_stored_value);
	}

	WTFourCharCode GetAsCompilerEndian() const
	{
		return s_stored_byte_order==wvNS::wvBO::compiler_byte_order ? m_stored_value : wvNS::wvBO::swap32(m_stored_value);
	}

	WTFourCharCode GetAsStored() const
	{
		return m_stored_value;
	}

	char operator[](const unsigned int in_character_index) const
	{
		return char(m_stored_value >> (8 * (s_stored_byte_order==wvNS::wvBO::compiler_byte_order ? 3-in_character_index : in_character_index)));
	}

	char& operator[](const unsigned int in_character_index)
	{
		return reinterpret_cast<char*>(&m_stored_value)[s_stored_byte_order==wvNS::wvBO::byte_order_little_endian ? 3-in_character_index : in_character_index];
	}
    
    static size_t size()
    {
        return sizeof(WTFourCharCode);
    }

	static size_t max_size()
	{
		return size();
	}
    
	static size_t capacity()
	{
		return size();
	}
    
	WCFixedString4 GetString() const
	{
		WCFixedString4 retVal;
		retVal << operator[](0) << operator[](1) << operator[](2) << operator[](3);

		return retVal;
	}

#if kEnableWCFourCCDebug == 1
	const char* c_str() const
	{
		return m_c_str_stored_value;
	}
#endif

protected:

private:
#if kEnableWCFourCCDebug == 1
	union
	{
#endif
		WTFourCharCode m_stored_value;
#if kEnableWCFourCCDebug == 1
		char m_c_str_stored_value[sizeof(WTFourCharCode)+1];
	};
#endif

	WCFourCC& operator=(const WTFourCharCode); // we want initialization from literal to be dome through the constructor
};

inline bool operator<(const WCFourCC in_left, const WCFourCC in_right)
{
	return in_left.GetAsSomeEndian(WCFourCC::s_stored_byte_order) < in_right.GetAsSomeEndian(WCFourCC::s_stored_byte_order);
}
inline bool operator==(const WCFourCC in_left, const WCFourCC in_right)
{
	return in_left.GetAsSomeEndian(WCFourCC::s_stored_byte_order) == in_right.GetAsSomeEndian(WCFourCC::s_stored_byte_order);
}

inline bool operator!=(const WCFourCC in_left, const WCFourCC in_right)
{
	return ! operator==(in_left, in_right);
}


#define kDefaultFourCC WCFourCC::kDefaultFourCC_prv()

static const WCFourCC kZeroFourCC(0, wvNS::wvBO::compiler_byte_order);
	
#endif //#if !defined(__WCFourCC_h__)



