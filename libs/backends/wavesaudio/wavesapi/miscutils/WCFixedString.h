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

#ifndef __WCFixedString_h__
	#define __WCFixedString_h__

/* Copy to include.
#include "WCFixedString.h"
*/
// do not #include anything else here but standard C++ library files, this file should be free from any and all depandencies
// do not put any DEBUG_s or TRACE_s in this file, since it is used in BgkConsole functions

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>

#ifdef __APPLE__
#include <strings.h>
#endif

#include "BasicTypes/WUDefines.h"
#include "BasicTypes/WTByteOrder.h"
#include "WavesPublicAPI/wstdint.h"
#include "MiscUtils/MinMaxUtilities.h"

// use this macro instead of std :: string to mark the that use of std :: string could not be replaced
// by WFixedString.
#define std_string_approved std::string

#ifdef __POSIX__
const char* const kStrNewLine = "\n";
#endif
#ifdef PLATFORM_WINDOWS
const char* const kStrNewLine = "\r\n";
#endif

class DllExport WCFixedStringBase
{
public:
    typedef size_t pos_t;
    typedef intptr_t spos_t; // signed position, defined to intptr_t because Windows does not have ssize_t
	static const pos_t npos = UINTPTR_MAX; // Same as size_max

	WCFixedStringBase(char* const in_begin, const size_t in_MaxFixedStringLength) :	
		m_begin(in_begin),
		m_MaxFixedStringLength(in_MaxFixedStringLength),
		m_end(in_begin)
	{
		*m_end = '\0';
	}

	inline WCFixedStringBase& operator=(const WCFixedStringBase& in_fixedStrToAssign)
	{
		if (this != &in_fixedStrToAssign)
		{
			clear();
			operator<<(in_fixedStrToAssign);
		}

		return *this;
	}

	inline WCFixedStringBase& operator=(const char* in_CStrToAssign)
	{
		clear();
		operator<<(in_CStrToAssign);

		return *this;
	}

	inline WCFixedStringBase& operator=(const char in_charToAssign)
	{
		clear();
		operator<<(in_charToAssign);

		return *this;
	}

	char operator[](const pos_t in_index) const
	{
		if (in_index < m_MaxFixedStringLength)
			return m_begin[in_index];
		else
			return m_begin[m_MaxFixedStringLength]; // in_index was too big 
	}

	char& operator[](const pos_t in_index)
	{
		if (in_index < m_MaxFixedStringLength)
			return m_begin[in_index];
		else
			return m_begin[m_MaxFixedStringLength]; // in_index was too big 
	}

	inline size_t resize(const size_t in_newSize)
	{
		m_end = m_begin + WUMin<size_t>(in_newSize, m_MaxFixedStringLength);
		*m_end = '\0';
		return size();
	}

	size_t max_size()
	{
		return m_MaxFixedStringLength;
	}

	size_t capacity()
	{
		return m_MaxFixedStringLength;
	}


	inline char * peek()
	{
		return m_begin;
	}

	inline const char * c_str() const
	{
		*m_end = '\0';
		return m_begin;
	}

	inline void clear()
	{
		m_end = m_begin;
		*m_end = '\0';
	}

	inline size_t size() const
	{
		return m_end - m_begin;
	}

    inline char* begin() const
    {
        return m_begin;
    }

    inline char* end() const
    {
        return m_end;
    }

	inline size_t length() const
	{
		return size();
	}

	inline bool empty() const
	{
		return m_begin == m_end;
	}

	inline void reverse(char* in_left, char* in_right)
	{
		char* left = in_left;
		char* right = in_right;
		while (left < right)
		{
			char temp = *--right;
			*right = *left;
			*left++ = temp;
		}
	}

	inline void reverse()
	{
		reverse(m_begin, m_end);
	}

	inline void to_lower()
	{
		char* pToDo = m_begin;

		while (pToDo < m_end)
		{
			*pToDo = static_cast<char>(std::tolower(*pToDo));
			++pToDo;
		}
	}

	inline void to_upper()
	{
		char* pToDo = m_begin;

		while (pToDo < m_end)
		{
			*pToDo = static_cast<char>(std::toupper(*pToDo));
			++pToDo;
		}
	}

	// append a single char in_count times
	inline void append(const char in_charToAppend, const size_t in_count)
	{
		size_t counter = 0;
		while ((m_end < m_begin+m_MaxFixedStringLength) && counter++ < in_count)
			*m_end++ = in_charToAppend;
#if kEnableDebug == 1
		if (counter < in_count)    // if there wasn't enough room for some appended chars
		{
			m_begin[0] = '@';       // mark the string as overflowed
		}
#endif
		*m_end = '\0';
	}

	inline void append(const char* in_chars)
	{
		operator<<(in_chars);
	}

	// append "iterator style"
	inline void append(const char* in_chars_begin, const char* in_chars_end)
	{
		const char* curr_char = in_chars_begin;
		while ((m_end < m_begin+m_MaxFixedStringLength) && curr_char < in_chars_end && *curr_char != '\0')
			*m_end++ = *curr_char++;

#if kEnableDebug == 1
		if (curr_char < in_chars_end)   // if there wasn't enough room for some appended chars  
		{
			m_begin[0] = '@';           // mark the string as overflowed
		}
#endif
		*m_end = '\0';
	}

	// append from a char* in_count chars, (no \0 is required to terminate the input string) 
	inline void append(const char* in_chars_begin, const size_t in_count)
	{
		append(in_chars_begin, in_chars_begin + in_count);
	}

	// assign from a char* in_count chars, (no \0 is required to terminate the input string) 
	inline void assign(const char* in_chars_begin, const size_t in_count)
	{
		clear();
		append(in_chars_begin, in_chars_begin + in_count);
	}

	// assign from a char* , (a \0 is required to terminate the input string) 
	inline void assign(const char* in_chars_ptr)
	{
		clear();
		operator<<(in_chars_ptr);
	}

    // assign from a char* to a char*
    inline void assign(const char* in_begin, const char* in_end)
    {
        assign(in_begin, size_t(in_end - in_begin));
    }

	inline void append_double_with_precision(const double in_double, const int in_precision)
	{
		const unsigned int tempBufSize = 32;
		char buf[tempBufSize];

	#ifdef PLATFORM_WINDOWS
		_snprintf_s(buf, tempBufSize, tempBufSize - 1, "%.*f", in_precision, in_double);
	#endif
	#ifdef __APPLE__
		std::snprintf(buf, tempBufSize, "%.*f", in_precision, in_double);
	#endif		
	#ifdef __linux__	
		snprintf(buf, tempBufSize, "%.*f", in_precision, in_double);
	#endif		

		operator<<(buf);
	}

	inline void append_uint(const uint64_t in_uint, const int_fast16_t in_base = 10)
	{
		uint_fast64_t num = in_uint;

		char* lasr_char_before = m_end;

		do {
			char  remainder(static_cast<char>(num % in_base));

			if ( remainder < 10 )
				operator<<(char(remainder + '0'));
			else
				operator<<(char(remainder - 10 + 'A'));

			num /= in_base;
		} while (num != 0);

		reverse(lasr_char_before, m_end);
	}

	inline void append_hex_binary(const uint8_t* in_binary, const size_t in_size)
	{
		static const char hexdigits[] = "0123456789ABCDEF";

#if _BYTEORDER_BIG_ENDIAN==1
		for (size_t ibyte = 0; ibyte < in_size; ++ibyte)
#elif _BYTEORDER_BIG_ENDIAN==0
			for (size_t ibyte = in_size; ibyte > 0; --ibyte)
#endif
			{
				operator<<(hexdigits[in_binary[ibyte - 1] >> 4]);
				operator<<(hexdigits[in_binary[ibyte - 1] & 0x0F]);
			}
	}

	inline WCFixedStringBase& operator<<(const char in_charToAppend)
	{
		if (m_end < m_begin+m_MaxFixedStringLength)
			*m_end++ = in_charToAppend;
#if kEnableDebug == 1
		else                    // if there wasn't enough room for the appended char
		{
			m_begin[0] = '@'; // mark the string as overflowed
		}
#endif

		*m_end = '\0';

		return *this;
	}

	inline WCFixedStringBase& operator<<(const char* const in_strToAppend)
	{
		if (0 != in_strToAppend)
		{
			const char* pSource = in_strToAppend;

			while (*pSource != '\0' && m_end < m_begin+m_MaxFixedStringLength)
				*m_end++ = *pSource++;

#if kEnableDebug == 1
			if (*pSource != '\0')   // if there wasn't enough room for some appended chars  
			{
				m_begin[0] = '@';  // mark the string as overflowed
			}
#endif
			*m_end = '\0';
		}

		return *this;
	}

	WCFixedStringBase& operator<<(const uint64_t in_uint)
	{
		append_uint(in_uint, 10);

		return *this;
	}


	// Warning prevention: the operator<< function overload for unsigneds used to create lots
    // of warnings once size_t usage was becoming widespread. So for each OS we define only
    // those overloads that are actually needed. On Windows 32 bit we still get
	// 'warning C4267: 'argument' : conversion from 'size_t' to 'const unsigned int', possible loss of data'
	// warning which we do not know how to solve yet. The function DummyFunctionsForWarningTest
    // in file WCFixedStringStream.cpp calls all combinations of operator<<(unsigned something)
    // And should produce no warnings - (except the C4267 on windows).
#if defined(__APPLE__) // both 32 & 64 bit
	WCFixedStringBase& operator<<(const size_t in_uint) {
		return operator<<(static_cast<unsigned long long>(in_uint));
	}
#endif   
//	WCFixedStringBase& operator<<(const unsigned char in_uint) {
//		return operator<<(static_cast<const unsigned long long>(in_uint));
//		}
//		
//	WCFixedStringBase& operator<<(const size_t in_uint) {
//		return operator<<(static_cast<const uint64_t>(in_uint));
//		}
//		
#if defined(__APPLE__) || defined(PLATFORM_WINDOWS) || defined(__linux__) // both 32 & 64 bit
	WCFixedStringBase& operator<<(const unsigned int in_uint) {
		return operator<<(static_cast<uint64_t>(in_uint));
	}
#endif   
//		
#if defined(PLATFORM_WINDOWS) || defined(__linux__) // both 32 & 64 bit
	WCFixedStringBase& operator<<(const unsigned long in_uint) {
		return operator<<(static_cast<uint64_t>(in_uint));
	}
#endif   

	WCFixedStringBase& operator<<(const long long in_int)
	{
		if (in_int < 0)
			operator<<('-');
#ifdef PLATFORM_WINDOWS
//        uintmax_t unsigned_in_num = _abs64(in_int);
		uintmax_t unsigned_in_num = in_int < 0 ? static_cast<uintmax_t>(-in_int) : static_cast<uintmax_t>(in_int);
#else
		uintmax_t unsigned_in_num = std::abs(in_int);
#endif
		append_uint(unsigned_in_num, 10);

		return *this;
	}

	WCFixedStringBase& operator<<(const short in_int) {
		return operator<<(static_cast<int64_t>(in_int));
	}

	WCFixedStringBase& operator<<(const int in_int) {
		return operator<<(static_cast<int64_t>(in_int));
	}

	WCFixedStringBase& operator<<(const long in_int) {
		return operator<<(static_cast<int64_t>(in_int));
	}

	WCFixedStringBase& operator<<(const double in_doubleToWrite)
	{
		append_double_with_precision(in_doubleToWrite, 10);

		return *this;
	}

	WCFixedStringBase& operator<<(const float in_floatToWrite)
	{
		append_double_with_precision(double(in_floatToWrite), 5);

		return *this;
	}

	inline WCFixedStringBase& operator<<(const WCFixedStringBase& in_fixedStrToAppend)
	{
		operator<<(in_fixedStrToAppend.c_str());

		return *this;
	}

	WCFixedStringBase& operator<< (bool abool)
	{
		return abool ? operator<<("true") : operator<<("false");
	}

	template<typename T> WCFixedStringBase& operator+=(T in_type)
	{
		return operator<<(in_type);
	}

	ptrdiff_t compare(const char* in_to_compare) const
	{
		ptrdiff_t retVal = 1;

		if (0 != in_to_compare)
		{
			retVal =  strcmp(c_str(), in_to_compare);
		}

		return retVal;
	}


	ptrdiff_t compare(const WCFixedStringBase& in_to_compare) const
	{
		ptrdiff_t retVal = compare(in_to_compare.c_str());		
		return retVal;
	}

	ptrdiff_t case_insensitive_compare(const char* in_to_compare) const
	{
		ptrdiff_t retVal = 1;

		if (0 != in_to_compare)
		{
#ifdef PLATFORM_WINDOWS
			retVal = _stricmp(c_str(), in_to_compare);
#endif
#if defined(__linux__) || defined(__APPLE__)
			retVal =  strcasecmp(c_str(), in_to_compare);
#endif
		}

		return retVal;
	}

	ptrdiff_t case_insensitive_compare(const WCFixedStringBase& in_to_compare) const
	{
		ptrdiff_t retVal = case_insensitive_compare(in_to_compare.c_str());		
		return retVal;
	}

	pos_t find(const char in_char_to_find) const
	{
		const char* pCurrChar = m_begin;
		while (pCurrChar < m_end && *pCurrChar != in_char_to_find)
			++pCurrChar;

		return (pCurrChar < m_end) ? (pCurrChar - m_begin) : npos;
	}

    pos_t rfind(const char in_char_to_find) const
    {
        pos_t retVal = npos;
        const char* pCurrChar = m_end;

        while (pCurrChar != m_begin) 
        {
            --pCurrChar;
            if (*pCurrChar == in_char_to_find)
            {
                retVal = pCurrChar - m_begin;
                break;
            }
        }
        
        return retVal;
    }
    
	pos_t find(const char* in_chars_to_find, const pos_t in_start_from = 0) const
	{
		pos_t retVal = npos;
		size_t to_find_size = ::strlen(in_chars_to_find);
        
		if (to_find_size > 0 && to_find_size <= size() && in_start_from < size())
		{
			const char* pCurrChar = m_begin + in_start_from;
			while ((m_end - pCurrChar) >= (ptrdiff_t)to_find_size)
			{
				int found = ::memcmp(pCurrChar, in_chars_to_find, to_find_size);
				if (0 == found)
				{
					retVal = (pCurrChar - m_begin);
					break;
				}
                
				++pCurrChar;
			}
		}
        
		return retVal;
	}
    
	pos_t rfind(const char* in_chars_to_find) const
	{
		pos_t retVal = npos;
		size_t to_find_size = ::strlen(in_chars_to_find);
        
		if (to_find_size > 0 && to_find_size <= size())
		{
			const char* pCurrChar = m_end - to_find_size;
			while (m_begin <= pCurrChar)
			{
				int found = ::memcmp(pCurrChar, in_chars_to_find, to_find_size);
				if (0 == found)
				{
					retVal = (pCurrChar - m_begin);
					break;
				}
                
				--pCurrChar;
			}
		}
        
		return retVal;
	}
    
	pos_t find_case_insensitive(const char* in_chars_to_find, const pos_t in_start_from = 0) const
	{
		pos_t retVal = npos;
		size_t to_find_size = ::strlen(in_chars_to_find);
        
		if (to_find_size > 0 && to_find_size <= size() && in_start_from < size())
		{
			const char* pCurrChar = m_begin + in_start_from;
			while ((m_end - pCurrChar) >= (ptrdiff_t)to_find_size)
			{
            	size_t i;
            	for (i = 0; i < to_find_size; ++i)
                {
                	if (tolower(*(pCurrChar+i)) != tolower(in_chars_to_find[i]))
                    	break;
                }
                
				if (i == to_find_size)
				{
					retVal = (pCurrChar - m_begin);
					break;
				}
                
				++pCurrChar;
			}
		}
        
		return retVal;
	}
    
	pos_t find_first_of(const char* in_possibe_chars_to_find, const pos_t in_start_from = 0) const
	{
		pos_t retVal = npos;

		if (in_start_from < size())
		{
			const char* pFoundChar = strpbrk(m_begin + in_start_from, in_possibe_chars_to_find);
			if (0 != pFoundChar)
			{
				retVal = (pFoundChar - m_begin);
			}
		}

		return retVal;
	}

	pos_t find_last_of(const char* in_possibe_chars_to_find, const pos_t in_start_from = 0) const
	{
		pos_t retVal = npos;

		pos_t curr_location = in_start_from;

		while (size() > curr_location)
		{
			pos_t found = find_first_of(in_possibe_chars_to_find, curr_location);
			if (npos != found)
			{
				retVal = found;
				curr_location = found + 1;
			}
			else
				break;
		}

		return retVal;
	}

	pos_t find_first_not_of(const char* in_acceptable_chars, const pos_t in_start_from = 0) const
	{
		pos_t retVal = npos;

		if (in_start_from < size())
		{
			retVal = (strspn(m_begin + in_start_from, in_acceptable_chars));
			if (size() <= retVal + in_start_from)
			{
				retVal = npos;
			}
			else
			{
				retVal += in_start_from;
			}
		}

		return retVal;
	}

	pos_t find_last_not_of(const char* in_acceptable_chars, const pos_t in_start_from = 0) const
	{
		pos_t retVal = npos;

		pos_t curr_location = in_start_from;

		while (size() > curr_location)
		{
			pos_t found = find_first_not_of(in_acceptable_chars, curr_location);
			if (npos != found)
			{
				retVal = found;
				curr_location = found + 1;
			}
			else
				break;
		}

		return retVal;
	}

	// return true if in_begin_text is found at position 0 OR if in_begin_text is empty
	bool begins_with(const char* in_begin_text) const
    {
        pos_t where = find(in_begin_text, 0);
        bool retVal = (0 == where) || (0 == ::strlen(in_begin_text));
        return retVal;
    }
	
    // return true if in_end_text is found at th end OR if in_end_text is empty
	bool ends_with(const char* in_end_text) const
    {
        pos_t where = rfind(in_end_text);
        bool retVal = ((size() - strlen(in_end_text)) == where) || (0 == ::strlen(in_end_text));
        return retVal;
    }
    
	size_t replace(const char in_look_for, const char in_replace_with)
	{
		size_t retVal = 0;

		char* pCurrChar = m_begin;
		while (pCurrChar < m_end)
		{
			if (*pCurrChar == in_look_for)
			{
				*pCurrChar = in_replace_with;
				++retVal;
			}
            ++pCurrChar;
		}

		return retVal;
	}

    // erase in_size chars starting from in_location
	void erase(const pos_t in_location, const size_t in_num_chars = 1)
	{
		if (size() > in_location && in_num_chars > 0)
		{
			size_t actual_num_chars = WUMin(in_num_chars, size_t(size() - in_location));
			char* pTo = m_begin + in_location;
			char* pFrom = pTo + actual_num_chars;

			while (pFrom < m_end)
				*pTo++ = *pFrom++;

			resize(size() - actual_num_chars);
		}
	}

    // erase any char that appear in in_forbidden_chars
	void erase_all_of(const char* in_forbidden_chars)
	{
		pos_t curr_location = 0;

		while (npos != curr_location)
		{
			curr_location = find_first_of(in_forbidden_chars, curr_location);
			if (npos != curr_location)
				erase(curr_location);
		}
	}

    // erase any char that do not appear in in_allowed_chars
	void erase_all_not_of(const char* in_allowed_chars)
	{
		pos_t curr_location = 0;

		while (npos != curr_location)
		{
			curr_location = find_first_not_of(in_allowed_chars, curr_location);
			if (npos != curr_location)
				erase(curr_location);
		}
	}

	//! Copy the content of fixed string to a buffer appending a '\0' at the end.
    //! If in_buffer_size is more than the allocated buffer size memory over write will happen!
	void copy_to_buffer(const size_t in_buffer_size, char* out_buffer)
    {
    	if (in_buffer_size > 0 && 0 != out_buffer)
        {
            char* cur_buffer = out_buffer;
            const char* cur_fixed = m_begin;
            const char* end_buffer = out_buffer + (WUMin<size_t>(in_buffer_size - 1, m_end - m_begin));
            while (cur_buffer < end_buffer)
                *cur_buffer++ = *cur_fixed++;
            
            *cur_buffer = '\0';
        }
    }
    
protected:	
	~WCFixedStringBase() {}

	char* const m_begin;
	const size_t m_MaxFixedStringLength;
	char* m_end;

private:
	WCFixedStringBase();
	WCFixedStringBase(const WCFixedStringBase& in_fixedStrToCopy);
#if 0
	:
	m_begin(in_fixedStrToCopy.m_begin),
	m_MaxFixedStringLength(in_fixedStrToCopy.m_MaxFixedStringLength),
	m_end(in_fixedStrToCopy.m_end)
	{
	}
#endif
};

template<size_t kMaxFixedStringLength> class DllExport WCFixedString : public WCFixedStringBase
{
public:

	inline WCFixedString() : 
		WCFixedStringBase(m_fixedString, kMaxFixedStringLength)
	{
	}

	inline  WCFixedString(const char* const in_strToAssign) :
		WCFixedStringBase(m_fixedString, kMaxFixedStringLength)
	{
		operator<<(in_strToAssign);
	}

	inline  WCFixedString(const WCFixedStringBase& in_fixedStrToAssign) :
		WCFixedStringBase(m_fixedString, kMaxFixedStringLength)
	{
		operator<<(in_fixedStrToAssign);
	}

	inline  WCFixedString(const WCFixedString& in_fixedStrToAssign) :
		WCFixedStringBase(m_fixedString, kMaxFixedStringLength)
	{
		operator<<(in_fixedStrToAssign);
	}

	inline  WCFixedString(const char in_char, const size_t in_count = 1) :
		WCFixedStringBase(m_fixedString, kMaxFixedStringLength)
	{
		append(in_char, in_count);
	}

	inline  WCFixedString(const char* in_chars, const size_t in_count) :
		WCFixedStringBase(m_fixedString, kMaxFixedStringLength)
	{
		append(in_chars, in_count);
	}

    // substr now supports negative in_length, which means "from the end" so
    // "abcdefg".substr(1, -1) == "bcdef"
	inline const WCFixedString substr(const pos_t in_pos = 0, const spos_t in_length = kMaxFixedStringLength) const
	{
		pos_t adjusted_pos = WUMin<size_t>(in_pos, size());
    	size_t adjusted_length = 0;
        if (in_length < 0)
        {
        	adjusted_length = size_t(WUMax<spos_t>(0, spos_t(size() - adjusted_pos) + in_length));
        }
        else
        	adjusted_length = WUMin<size_t>(in_length, size() - adjusted_pos);

		WCFixedString retVal;
		retVal.append(m_begin + adjusted_pos, adjusted_length);

		return retVal;
	}

protected:	

	char m_fixedString[kMaxFixedStringLength + 1]; // the "+ 1" is so that *m_end is always valid, and we can put the '\0' there};
};

inline bool operator==(const WCFixedStringBase& in_left, const WCFixedStringBase& in_right)	
{
	return 0 == in_left.compare(in_right.c_str());
}

inline bool operator==(const WCFixedStringBase& in_left, const char* const in_right)
{
	return 0 == in_left.compare(in_right);
}

inline bool operator!=(const WCFixedStringBase& in_left, const WCFixedStringBase& in_right)	
{
	return 0 != in_left.compare(in_right.c_str());
}

inline bool operator!=(const WCFixedStringBase& in_left, const char* const in_right)
{
	return 0 != in_left.compare(in_right);
}

// class WCFixedStringBase
typedef WCFixedString<4>    	WCFixedString4;
typedef WCFixedString<15>	WCFixedString15;
typedef WCFixedString<31>	WCFixedString31;
typedef WCFixedString<63>	WCFixedString63;
typedef WCFixedString<127>	WCFixedString127;
typedef WCFixedString<255>	WCFixedString255;
typedef WCFixedString<511>	WCFixedString511;
typedef WCFixedString<1023>	WCFixedString1023;
typedef WCFixedString<2047>	WCFixedString2047;

template<size_t kSizeOfFirst, size_t kSizeOfSecond> 
	class WCFixedStringPair : public std::pair< WCFixedString<kSizeOfFirst>, WCFixedString<kSizeOfSecond> >
{
public:
	WCFixedStringPair(const char* const in_firstStr = 0, const char* const in_secondStr = 0) :	
	std::pair< WCFixedString<kSizeOfFirst>, WCFixedString<kSizeOfSecond> >(in_firstStr, in_secondStr) {}
	WCFixedStringPair(const WCFixedStringBase& in_firstStr, const char* const in_secondStr = 0) :	
	std::pair< WCFixedString<kSizeOfFirst>, WCFixedString<kSizeOfSecond> >(in_firstStr, in_secondStr) {}
	WCFixedStringPair(const WCFixedStringBase& in_firstStr, const WCFixedStringBase& in_secondStr) :	
	std::pair< WCFixedString<kSizeOfFirst>, WCFixedString<kSizeOfSecond> >(in_firstStr, in_secondStr) {}
};

#endif  //  #ifndef __WCFixedString_h__
