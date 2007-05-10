// -*- c++ -*-
/* $Id$ */

/* Copyright (C) 2002 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <glibmm/ustring.h>
#include <glibmm/convert.h>
#include <glibmm/error.h>
#include <glibmm/utility.h>

#include <algorithm>
#include <iostream>

#include <glibmmconfig.h>
GLIBMM_USING_STD(find)


namespace
{

using Glib::ustring;

// Little helper to make the conversion from gunichar to UTF-8 a one-liner.
//
struct UnicharToUtf8
{
  char                buf[6];
  ustring::size_type  len;

  explicit UnicharToUtf8(gunichar uc)
    : len (g_unichar_to_utf8(uc, buf)) {}
};


// All utf8_*_offset() functions return npos if offset is out of range.
// The caller should decide if npos is a valid argument and just marks
// the whole string, or if it is not allowed (e.g. for start positions).
// In the latter case std::out_of_range should be thrown, but usually
// std::string will do that for us.

// First overload: stop on '\0' character.
//
static ustring::size_type utf8_byte_offset(const char* str, ustring::size_type offset)
{
  if(offset == ustring::npos)
    return ustring::npos;

  const char *const utf8_skip = g_utf8_skip;
  const char* p = str;

  for(; offset != 0; --offset)
  {
    const unsigned int c = static_cast<unsigned char>(*p);

    if(c == 0)
      return ustring::npos;

    p += utf8_skip[c];
  }

  return (p - str);
}

// Second overload: stop when reaching maxlen.
//
static ustring::size_type utf8_byte_offset(const char* str, ustring::size_type offset,
                                    ustring::size_type maxlen)
{
  if(offset == ustring::npos)
    return ustring::npos;

  const char *const utf8_skip = g_utf8_skip;
  const char *const pend = str + maxlen;
  const char* p = str;

  for(; offset != 0; --offset)
  {
    if(p >= pend)
      return ustring::npos;

    p += utf8_skip[static_cast<unsigned char>(*p)];
  }

  return (p - str);
}

// Third overload: stop when reaching str.size().
//
inline
ustring::size_type utf8_byte_offset(const std::string& str, ustring::size_type offset)
{
  return utf8_byte_offset(str.data(), offset, str.size());
}

// Takes UTF-8 character offset and count in ci and cn.
// Returns the byte offset and count in i and n.
//
struct Utf8SubstrBounds
{
  ustring::size_type i;
  ustring::size_type n;

  Utf8SubstrBounds(const std::string& str, ustring::size_type ci, ustring::size_type cn)
  :
    i (utf8_byte_offset(str, ci)),
    n (ustring::npos)
  {
    if(i != ustring::npos)
      n = utf8_byte_offset(str.data() + i, cn, str.size() - i);
  }
};

// Converts byte offset to UTF-8 character offset.
inline
ustring::size_type utf8_char_offset(const std::string& str, ustring::size_type offset)
{
  if(offset == ustring::npos)
    return ustring::npos;

  const char *const pdata = str.data();
  return g_utf8_pointer_to_offset(pdata, pdata + offset);
}


// Helper to implement ustring::find_first_of() and find_first_not_of().
// Returns the UTF-8 character offset, or ustring::npos if not found.
//
ustring::size_type utf8_find_first_of(const std::string& str, ustring::size_type offset,
                                      const char* utf8_match, long utf8_match_size,
                                      bool find_not_of)
{
  const ustring::size_type byte_offset = utf8_byte_offset(str, offset);
  if(byte_offset == ustring::npos)
    return ustring::npos;

  long ucs4_match_size = 0;
  const Glib::ScopedPtr<gunichar> ucs4_match
      (g_utf8_to_ucs4_fast(utf8_match, utf8_match_size, &ucs4_match_size));

  const gunichar *const match_begin = ucs4_match.get();
  const gunichar *const match_end   = match_begin + ucs4_match_size;

  const char *const str_begin = str.data();
  const char *const str_end   = str_begin + str.size();

  for(const char* pstr = str_begin + byte_offset;
      pstr < str_end;
      pstr = g_utf8_next_char(pstr))
  {
    const gunichar *const pfound = std::find(match_begin, match_end, g_utf8_get_char(pstr));

    if((pfound != match_end) != find_not_of)
      return offset;

    ++offset;
  }

  return ustring::npos;
}

// Helper to implement ustring::find_last_of() and find_last_not_of().
// Returns the UTF-8 character offset, or ustring::npos if not found.
//
ustring::size_type utf8_find_last_of(const std::string& str, ustring::size_type offset,
                                     const char* utf8_match, long utf8_match_size,
                                     bool find_not_of)
{
  long ucs4_match_size = 0;
  const Glib::ScopedPtr<gunichar> ucs4_match
      (g_utf8_to_ucs4_fast(utf8_match, utf8_match_size, &ucs4_match_size));

  const gunichar *const match_begin = ucs4_match.get();
  const gunichar *const match_end   = match_begin + ucs4_match_size;

  const char *const str_begin = str.data();
  const char* pstr = str_begin;

  // Set pstr one byte beyond the actual start position.
  const ustring::size_type byte_offset = utf8_byte_offset(str, offset);
  pstr += (byte_offset < str.size()) ? byte_offset + 1 : str.size();

  while(pstr > str_begin)
  {
    // Move to previous character.
    do --pstr; while((*pstr & '\xC0') == '\x80');

    const gunichar *const pfound = std::find(match_begin, match_end, g_utf8_get_char(pstr));

    if((pfound != match_end) != find_not_of)
      return g_utf8_pointer_to_offset(str_begin, pstr);
  }

  return ustring::npos;
}

} // anonymous namespace


namespace Glib
{

#ifndef GLIBMM_HAVE_ALLOWS_STATIC_INLINE_NPOS
// Initialize static member here, 
// because the compiler did not allow us do it inline.
const ustring::size_type ustring::npos = std::string::npos;
#endif

// We need our own version of g_utf8_get_char(), because the std::string
// iterator is not necessarily a plain pointer (it's in fact not in GCC's
// libstdc++-v3).  Copying the UTF-8 data into a temporary buffer isn't an
// option since this operation is quite time critical.  The implementation
// is quite different from g_utf8_get_char() -- both more generic and faster.
//
// By looking at the first byte of a UTF-8 character one can determine the
// number of bytes used.  GLib offers the g_utf8_skip[] array for this purpose,
// but accessing this global variable would introduce a function call to fetch
// the Global Offset Table, plus two levels of indirection in order to read the
// value.  Even worse, fetching the GOT is always done right at the start of
// the function instead of the branch that actually uses the variable.
//
// Fortunately, there's a better way to get the byte count.  As this table
// shows, there's a nice regular pattern in the UTF-8 encoding scheme:
//
// 0x00000000 - 0x0000007F: 0xxxxxxx
// 0x00000080 - 0x000007FF: 110xxxxx 10xxxxxx
// 0x00000800 - 0x0000FFFF: 1110xxxx 10xxxxxx 10xxxxxx
// 0x00010000 - 0x001FFFFF: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
// 0x00200000 - 0x03FFFFFF: 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
// 0x04000000 - 0x7FFFFFFF: 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
//
// Except for the single byte case, the number of leading 1-bits equals the
// byte count.  All that is needed is to shift the first byte to the left
// until bit 7 becomes 0.  Naturally, doing so requires a loop -- but since
// we already have one, no additional cost is introduced.  This shifting can
// further be combined with the computation of the bitmask needed to eliminate
// the leading length bits, thus saving yet another register.
//
gunichar get_unichar_from_std_iterator(std::string::const_iterator pos)
{
  unsigned int result = static_cast<unsigned char>(*pos);

  if((result & 0x80) != 0)
  {
    unsigned int mask = 0x40;

    // This loop may look somewhat strange, but this happens to be the
    // one variant g++ 3.3 generates optimum code for.  (In particular,
    // the unconditional break avoids counterproductive loop alignment.)
    for(++pos;; ++pos)
    {
      mask <<= 5;
      const unsigned int c = static_cast<unsigned char>(*pos);
      result = (result << 6) + c - 0x80;

      if((result & mask) != 0)
        continue;
      break;
    }

    result &= --mask;
  }

  return result;
}


/**** Glib::ustring ********************************************************/

ustring::ustring()
:
  string_ ()
{}

ustring::ustring(const ustring& other)
:
  string_ (other.string_)
{}

ustring::ustring(const ustring& src, ustring::size_type i, ustring::size_type n)
:
  string_ ()
{
  const Utf8SubstrBounds bounds (src.string_, i, n);
  string_.assign(src.string_, bounds.i, bounds.n);
}

ustring::ustring(const char* src, ustring::size_type n)
:
  string_ (src, utf8_byte_offset(src, n))
{}

ustring::ustring(const char* src)
:
  string_ (src)
{}

ustring::ustring(ustring::size_type n, gunichar uc)
:
  string_ ()
{
  if(uc < 0x80)
  {
    // Optimize the probably most common case.
    string_.assign(n, static_cast<char>(uc));
  }
  else
  {
    const UnicharToUtf8 conv (uc);
    string_.reserve(n * conv.len);

    for(; n > 0; --n)
      string_.append(conv.buf, conv.len);
  }
}

ustring::ustring(ustring::size_type n, char c)
:
  string_ (n, c)
{}

ustring::ustring(const std::string& src)
:
  string_ (src)
{}

ustring::~ustring()
{}

void ustring::swap(ustring& other)
{
  string_.swap(other.string_);
}


/**** Glib::ustring::operator=() *******************************************/

ustring& ustring::operator=(const ustring& other)
{
  string_ = other.string_;
  return *this;
}

ustring& ustring::operator=(const std::string& src)
{
  string_ = src;
  return *this;
}

ustring& ustring::operator=(const char* src)
{
  string_ = src;
  return *this;
}

ustring& ustring::operator=(gunichar uc)
{
  const UnicharToUtf8 conv (uc);
  string_.assign(conv.buf, conv.len);
  return *this;
}

ustring& ustring::operator=(char c)
{
  string_ = c;
  return *this;
}


/**** Glib::ustring::assign() **********************************************/

ustring& ustring::assign(const ustring& src)
{
  string_ = src.string_;
  return *this;
}

ustring& ustring::assign(const ustring& src, ustring::size_type i, ustring::size_type n)
{
  const Utf8SubstrBounds bounds (src.string_, i, n);
  string_.assign(src.string_, bounds.i, bounds.n);
  return *this;
}

ustring& ustring::assign(const char* src, ustring::size_type n)
{
  string_.assign(src, utf8_byte_offset(src, n));
  return *this;
}

ustring& ustring::assign(const char* src)
{
  string_ = src;
  return *this;
}

ustring& ustring::assign(ustring::size_type n, gunichar uc)
{
  ustring temp (n, uc);
  string_.swap(temp.string_);
  return *this;
}

ustring& ustring::assign(ustring::size_type n, char c)
{
  string_.assign(n, c);
  return *this;
}


/**** Glib::ustring::operator+=() ******************************************/

ustring& ustring::operator+=(const ustring& src)
{
  string_ += src.string_;
  return *this;
}

ustring& ustring::operator+=(const char* src)
{
  string_ += src;
  return *this;
}

ustring& ustring::operator+=(gunichar uc)
{
  const UnicharToUtf8 conv (uc);
  string_.append(conv.buf, conv.len);
  return *this;
}

ustring& ustring::operator+=(char c)
{
  string_ += c;
  return *this;
}


/**** Glib::ustring::push_back() *******************************************/

void ustring::push_back(gunichar uc)
{
  const UnicharToUtf8 conv (uc);
  string_.append(conv.buf, conv.len);
}

void ustring::push_back(char c)
{
  string_ += c;
}


/**** Glib::ustring::append() **********************************************/

ustring& ustring::append(const ustring& src)
{
  string_ += src.string_;
  return *this;
}

ustring& ustring::append(const ustring& src, ustring::size_type i, ustring::size_type n)
{
  const Utf8SubstrBounds bounds (src.string_, i, n);
  string_.append(src.string_, bounds.i, bounds.n);
  return *this;
}

ustring& ustring::append(const char* src, ustring::size_type n)
{
  string_.append(src, utf8_byte_offset(src, n));
  return *this;
}

ustring& ustring::append(const char* src)
{
  string_ += src;
  return *this;
}

ustring& ustring::append(ustring::size_type n, gunichar uc)
{
  string_.append(ustring(n, uc).string_);
  return *this;
}

ustring& ustring::append(ustring::size_type n, char c)
{
  string_.append(n, c);
  return *this;
}


/**** Glib::ustring::insert() **********************************************/

ustring& ustring::insert(ustring::size_type i, const ustring& src)
{
  string_.insert(utf8_byte_offset(string_, i), src.string_);
  return *this;
}

ustring& ustring::insert(ustring::size_type i, const ustring& src,
                         ustring::size_type i2, ustring::size_type n)
{
  const Utf8SubstrBounds bounds2 (src.string_, i2, n);
  string_.insert(utf8_byte_offset(string_, i), src.string_, bounds2.i, bounds2.n);
  return *this;
}

ustring& ustring::insert(ustring::size_type i, const char* src, ustring::size_type n)
{
  string_.insert(utf8_byte_offset(string_, i), src, utf8_byte_offset(src, n));
  return *this;
}

ustring& ustring::insert(ustring::size_type i, const char* src)
{
  string_.insert(utf8_byte_offset(string_, i), src);
  return *this;
}

ustring& ustring::insert(ustring::size_type i, ustring::size_type n, gunichar uc)
{
  string_.insert(utf8_byte_offset(string_, i), ustring(n, uc).string_);
  return *this;
}

ustring& ustring::insert(ustring::size_type i, ustring::size_type n, char c)
{
  string_.insert(utf8_byte_offset(string_, i), n, c);
  return *this;
}

ustring::iterator ustring::insert(ustring::iterator p, gunichar uc)
{
  const size_type offset = p.base() - string_.begin();
  const UnicharToUtf8 conv (uc);
  string_.insert(offset, conv.buf, conv.len);
  return iterator(string_.begin() + offset);
}

ustring::iterator ustring::insert(ustring::iterator p, char c)
{
  return iterator(string_.insert(p.base(), c));
}

void ustring::insert(ustring::iterator p, ustring::size_type n, gunichar uc)
{
  string_.insert(p.base() - string_.begin(), ustring(n, uc).string_);
}

void ustring::insert(ustring::iterator p, ustring::size_type n, char c)
{
  string_.insert(p.base(), n, c);
}


/**** Glib::ustring::replace() *********************************************/

ustring& ustring::replace(ustring::size_type i, ustring::size_type n, const ustring& src)
{
  const Utf8SubstrBounds bounds (string_, i, n);
  string_.replace(bounds.i, bounds.n, src.string_);
  return *this;
}

ustring& ustring::replace(ustring::size_type i, ustring::size_type n,
                          const ustring& src, ustring::size_type i2, ustring::size_type n2)
{
  const Utf8SubstrBounds bounds (string_, i, n);
  const Utf8SubstrBounds bounds2 (src.string_, i2, n2);
  string_.replace(bounds.i, bounds.n, src.string_, bounds2.i, bounds2.n);
  return *this;
}

ustring& ustring::replace(ustring::size_type i, ustring::size_type n,
                          const char* src, ustring::size_type n2)
{
  const Utf8SubstrBounds bounds (string_, i, n);
  string_.replace(bounds.i, bounds.n, src, utf8_byte_offset(src, n2));
  return *this;
}

ustring& ustring::replace(ustring::size_type i, ustring::size_type n, const char* src)
{
  const Utf8SubstrBounds bounds (string_, i, n);
  string_.replace(bounds.i, bounds.n, src);
  return *this;
}

ustring& ustring::replace(ustring::size_type i, ustring::size_type n,
                          ustring::size_type n2, gunichar uc)
{
  const Utf8SubstrBounds bounds (string_, i, n);
  string_.replace(bounds.i, bounds.n, ustring(n2, uc).string_);
  return *this;
}

ustring& ustring::replace(ustring::size_type i, ustring::size_type n,
                          ustring::size_type n2, char c)
{
  const Utf8SubstrBounds bounds (string_, i, n);
  string_.replace(bounds.i, bounds.n, n2, c);
  return *this;
}

ustring& ustring::replace(ustring::iterator pbegin, ustring::iterator pend, const ustring& src)
{
  string_.replace(pbegin.base(), pend.base(), src.string_);
  return *this;
}

ustring& ustring::replace(ustring::iterator pbegin, ustring::iterator pend,
                          const char* src, ustring::size_type n)
{
  string_.replace(pbegin.base(), pend.base(), src, utf8_byte_offset(src, n));
  return *this;
}

ustring& ustring::replace(ustring::iterator pbegin, ustring::iterator pend, const char* src)
{
  string_.replace(pbegin.base(), pend.base(), src);
  return *this;
}

ustring& ustring::replace(ustring::iterator pbegin, ustring::iterator pend,
                          ustring::size_type n, gunichar uc)
{
  string_.replace(pbegin.base(), pend.base(), ustring(n, uc).string_);
  return *this;
}

ustring& ustring::replace(ustring::iterator pbegin, ustring::iterator pend,
                          ustring::size_type n, char c)
{
  string_.replace(pbegin.base(), pend.base(), n, c);
  return *this;
}


/**** Glib::ustring::erase() ***********************************************/

void ustring::clear()
{
  string_.erase();
}

ustring& ustring::erase(ustring::size_type i, ustring::size_type n)
{
  const Utf8SubstrBounds bounds (string_, i, n);
  string_.erase(bounds.i, bounds.n);
  return *this;
}

ustring& ustring::erase()
{
  string_.erase();
  return *this;
}

ustring::iterator ustring::erase(ustring::iterator p)
{
  return iterator(string_.erase(p.base()));
}

ustring::iterator ustring::erase(ustring::iterator pbegin, ustring::iterator pend)
{
  return iterator(string_.erase(pbegin.base(), pend.base()));
}


/**** Glib::ustring::compare() *********************************************/

int ustring::compare(const ustring& rhs) const
{
  return g_utf8_collate(string_.c_str(), rhs.string_.c_str());
}

int ustring::compare(const char* rhs) const
{
  return g_utf8_collate(string_.c_str(), rhs);
}

int ustring::compare(ustring::size_type i, ustring::size_type n, const ustring& rhs) const
{
  return ustring(*this, i, n).compare(rhs);
}

int ustring::compare(ustring::size_type i, ustring::size_type n,
                     const ustring& rhs, ustring::size_type i2, ustring::size_type n2) const
{
  return ustring(*this, i, n).compare(ustring(rhs, i2, n2));
}

int ustring::compare(ustring::size_type i, ustring::size_type n,
                     const char* rhs, ustring::size_type n2) const
{
  return ustring(*this, i, n).compare(ustring(rhs, n2));
}

int ustring::compare(ustring::size_type i, ustring::size_type n, const char* rhs) const
{
  return ustring(*this, i, n).compare(rhs);
}


/**** Glib::ustring -- index access ****************************************/

ustring::value_type ustring::operator[](ustring::size_type i) const
{
  return g_utf8_get_char(g_utf8_offset_to_pointer(string_.data(), i));
}

ustring::value_type ustring::at(ustring::size_type i) const
{
  const size_type byte_offset = utf8_byte_offset(string_, i);

  // Throws std::out_of_range if the index is invalid.
  return g_utf8_get_char(&string_.at(byte_offset));
}


/**** Glib::ustring -- iterator access *************************************/

ustring::iterator ustring::begin()
{
  return iterator(string_.begin());
}

ustring::iterator ustring::end()
{
  return iterator(string_.end());
}

ustring::const_iterator ustring::begin() const
{
  return const_iterator(string_.begin());
}

ustring::const_iterator ustring::end() const
{
  return const_iterator(string_.end());
}

ustring::reverse_iterator ustring::rbegin()
{
  return reverse_iterator(iterator(string_.end()));
}

ustring::reverse_iterator ustring::rend()
{
  return reverse_iterator(iterator(string_.begin()));
}

ustring::const_reverse_iterator ustring::rbegin() const
{
  return const_reverse_iterator(const_iterator(string_.end()));
}

ustring::const_reverse_iterator ustring::rend() const
{
  return const_reverse_iterator(const_iterator(string_.begin()));
}


/**** Glib::ustring::find() ************************************************/

ustring::size_type ustring::find(const ustring& str, ustring::size_type i) const
{
  return utf8_char_offset(string_, string_.find(str.string_, utf8_byte_offset(string_, i)));
}

ustring::size_type ustring::find(const char* str, ustring::size_type i, ustring::size_type n) const
{
  return utf8_char_offset(string_, string_.find(str, utf8_byte_offset(string_, i),
                                                     utf8_byte_offset(str, n)));
}

ustring::size_type ustring::find(const char* str, ustring::size_type i) const
{
  return utf8_char_offset(string_, string_.find(str, utf8_byte_offset(string_, i)));
}

ustring::size_type ustring::find(gunichar uc, ustring::size_type i) const
{
  const UnicharToUtf8 conv (uc);
  return utf8_char_offset(string_, string_.find(conv.buf, utf8_byte_offset(string_, i), conv.len));
}

ustring::size_type ustring::find(char c, ustring::size_type i) const
{
  return utf8_char_offset(string_, string_.find(c, utf8_byte_offset(string_, i)));
}


/**** Glib::ustring::rfind() ***********************************************/

ustring::size_type ustring::rfind(const ustring& str, ustring::size_type i) const
{
  return utf8_char_offset(string_, string_.rfind(str.string_, utf8_byte_offset(string_, i)));
}

ustring::size_type ustring::rfind(const char* str, ustring::size_type i,
                                  ustring::size_type n) const
{
  return utf8_char_offset(string_, string_.rfind(str, utf8_byte_offset(string_, i),
                                                      utf8_byte_offset(str, n)));
}

ustring::size_type ustring::rfind(const char* str, ustring::size_type i) const
{
  return utf8_char_offset(string_, string_.rfind(str, utf8_byte_offset(string_, i)));
}

ustring::size_type ustring::rfind(gunichar uc, ustring::size_type i) const
{
  const UnicharToUtf8 conv (uc);
  return utf8_char_offset(string_, string_.rfind(conv.buf, utf8_byte_offset(string_, i), conv.len));
}

ustring::size_type ustring::rfind(char c, ustring::size_type i) const
{
  return utf8_char_offset(string_, string_.rfind(c, utf8_byte_offset(string_, i)));
}


/**** Glib::ustring::find_first_of() ***************************************/

ustring::size_type ustring::find_first_of(const ustring& match, ustring::size_type i) const
{
  return utf8_find_first_of(string_, i, match.string_.data(), match.string_.size(), false);
}

ustring::size_type ustring::find_first_of(const char* match,
                                          ustring::size_type i, ustring::size_type n) const
{
  return utf8_find_first_of(string_, i, match, n, false);
}

ustring::size_type ustring::find_first_of(const char* match, ustring::size_type i) const
{
  return utf8_find_first_of(string_, i, match, -1, false);
}

ustring::size_type ustring::find_first_of(gunichar uc, ustring::size_type i) const
{
  return find(uc, i);
}

ustring::size_type ustring::find_first_of(char c, ustring::size_type i) const
{
  return find(c, i);
}


/**** Glib::ustring::find_last_of() ****************************************/

ustring::size_type ustring::find_last_of(const ustring& match, ustring::size_type i) const
{
  return utf8_find_last_of(string_, i, match.string_.data(), match.string_.size(), false);
}

ustring::size_type ustring::find_last_of(const char* match,
                                         ustring::size_type i, ustring::size_type n) const
{
  return utf8_find_last_of(string_, i, match, n, false);
}

ustring::size_type ustring::find_last_of(const char* match, ustring::size_type i) const
{
  return utf8_find_last_of(string_, i, match, -1, false);
}

ustring::size_type ustring::find_last_of(gunichar uc, ustring::size_type i) const
{
  return rfind(uc, i);
}

ustring::size_type ustring::find_last_of(char c, ustring::size_type i) const
{
  return rfind(c, i);
}


/**** Glib::ustring::find_first_not_of() ***********************************/

ustring::size_type ustring::find_first_not_of(const ustring& match, ustring::size_type i) const
{
  return utf8_find_first_of(string_, i, match.string_.data(), match.string_.size(), true);
}

ustring::size_type ustring::find_first_not_of(const char* match,
                                              ustring::size_type i, ustring::size_type n) const
{
  return utf8_find_first_of(string_, i, match, n, true);
}

ustring::size_type ustring::find_first_not_of(const char* match, ustring::size_type i) const
{
  return utf8_find_first_of(string_, i, match, -1, true);
}

// Unfortunately, all of the find_*_not_of() methods for single
// characters need their own special implementation.
//
ustring::size_type ustring::find_first_not_of(gunichar uc, ustring::size_type i) const
{
  const size_type bi = utf8_byte_offset(string_, i);
  if(bi != npos)
  {
    const char *const pbegin = string_.data();
    const char *const pend   = pbegin + string_.size();

    for(const char* p = pbegin + bi;
        p < pend;
        p = g_utf8_next_char(p), ++i)
    {
      if(g_utf8_get_char(p) != uc)
        return i;
    }
  }
  return npos;
}

ustring::size_type ustring::find_first_not_of(char c, ustring::size_type i) const
{
  const size_type bi = utf8_byte_offset(string_, i);
  if(bi != npos)
  {
    const char *const pbegin = string_.data();
    const char *const pend   = pbegin + string_.size();

    for(const char* p = pbegin + bi;
        p < pend;
        p = g_utf8_next_char(p), ++i)
    {
      if(*p != c)
        return i;
    }
  }
  return npos;
}


/**** Glib::ustring::find_last_not_of() ************************************/

ustring::size_type ustring::find_last_not_of(const ustring& match, ustring::size_type i) const
{
  return utf8_find_last_of(string_, i, match.string_.data(), match.string_.size(), true);
}

ustring::size_type ustring::find_last_not_of(const char* match,
                                             ustring::size_type i, ustring::size_type n) const
{
  return utf8_find_last_of(string_, i, match, n, true);
}

ustring::size_type ustring::find_last_not_of(const char* match, ustring::size_type i) const
{
  return utf8_find_last_of(string_, i, match, -1, true);
}

// Unfortunately, all of the find_*_not_of() methods for single
// characters need their own special implementation.
//
ustring::size_type ustring::find_last_not_of(gunichar uc, ustring::size_type i) const
{
  const char *const pbegin = string_.data();
  const char *const pend   = pbegin + string_.size();
  size_type i_cur   = 0;
  size_type i_found = npos;

  for(const char* p = pbegin;
      p < pend && i_cur <= i;
      p = g_utf8_next_char(p), ++i_cur)
  {
    if(g_utf8_get_char(p) != uc)
      i_found = i_cur;
  }
  return i_found;
}

ustring::size_type ustring::find_last_not_of(char c, ustring::size_type i) const
{
  const char *const pbegin = string_.data();
  const char *const pend   = pbegin + string_.size();
  size_type i_cur   = 0;
  size_type i_found = npos;

  for(const char* p = pbegin;
      p < pend && i_cur <= i;
      p = g_utf8_next_char(p), ++i_cur)
  {
    if(*p != c)
      i_found = i_cur;
  }
  return i_found;
}


/**** Glib::ustring -- get size and resize *********************************/

bool ustring::empty() const
{
  return string_.empty();
}

ustring::size_type ustring::size() const
{
  const char *const pdata = string_.data();
  return g_utf8_pointer_to_offset(pdata, pdata + string_.size());
}

ustring::size_type ustring::length() const
{
  const char *const pdata = string_.data();
  return g_utf8_pointer_to_offset(pdata, pdata + string_.size());
}

ustring::size_type ustring::bytes() const
{
  return string_.size();
}

ustring::size_type ustring::capacity() const
{
  return string_.capacity();
}

ustring::size_type ustring::max_size() const
{
  return string_.max_size();
}

void ustring::resize(ustring::size_type n, gunichar uc)
{
  const size_type size_now = size();
  if(n < size_now)
    erase(n, npos);
  else if(n > size_now)
    append(n - size_now, uc);
}

void ustring::resize(ustring::size_type n, char c)
{
  const size_type size_now = size();
  if(n < size_now)
    erase(n, npos);
  else if(n > size_now)
    string_.append(n - size_now, c);
}

void ustring::reserve(ustring::size_type n)
{
  string_.reserve(n);
}


/**** Glib::ustring -- C string access *************************************/

const char* ustring::data() const
{
  return string_.data();
}

const char* ustring::c_str() const
{
  return string_.c_str();
}

// Note that copy() requests UTF-8 character offsets as
// parameters, but returns the number of copied bytes.
//
ustring::size_type ustring::copy(char* dest, ustring::size_type n, ustring::size_type i) const
{
  const Utf8SubstrBounds bounds (string_, i, n);
  return string_.copy(dest, bounds.n, bounds.i);
}


/**** Glib::ustring -- UTF-8 utilities *************************************/

bool ustring::validate() const
{
  return (g_utf8_validate(string_.data(), string_.size(), 0) != 0);
}

bool ustring::validate(ustring::iterator& first_invalid)
{
  const char *const pdata = string_.data();
  const char* valid_end = pdata;
  const int is_valid = g_utf8_validate(pdata, string_.size(), &valid_end);

  first_invalid = iterator(string_.begin() + (valid_end - pdata));
  return (is_valid != 0);
}

bool ustring::validate(ustring::const_iterator& first_invalid) const
{
  const char *const pdata = string_.data();
  const char* valid_end = pdata;
  const int is_valid = g_utf8_validate(pdata, string_.size(), &valid_end);

  first_invalid = const_iterator(string_.begin() + (valid_end - pdata));
  return (is_valid != 0);
}

bool ustring::is_ascii() const
{
  const char* p = string_.data();
  const char *const pend = p + string_.size();

  for(; p != pend; ++p)
  {
    if((*p & '\x80') != 0)
      return false;
  }

  return true;
}

ustring ustring::normalize(NormalizeMode mode) const
{
  const ScopedPtr<char> buf (g_utf8_normalize(string_.data(), string_.size(),
                                              static_cast<GNormalizeMode>(int(mode))));
  return ustring(buf.get());
}

ustring ustring::uppercase() const
{
  const ScopedPtr<char> buf (g_utf8_strup(string_.data(), string_.size()));
  return ustring(buf.get());
}

ustring ustring::lowercase() const
{
  const ScopedPtr<char> buf (g_utf8_strdown(string_.data(), string_.size()));
  return ustring(buf.get());
}

ustring ustring::casefold() const
{
  const ScopedPtr<char> buf (g_utf8_casefold(string_.data(), string_.size()));
  return ustring(buf.get());
}

std::string ustring::collate_key() const
{
  const ScopedPtr<char> buf (g_utf8_collate_key(string_.data(), string_.size()));
  return std::string(buf.get());
}

std::string ustring::casefold_collate_key() const
{
  char *const casefold_buf = g_utf8_casefold(string_.data(), string_.size());
  char *const key_buf      = g_utf8_collate_key(casefold_buf, -1);
  g_free(casefold_buf);
  return std::string(ScopedPtr<char>(key_buf).get());
}


/**** Glib::ustring::SequenceToString **************************************/

ustring::SequenceToString<Glib::ustring::iterator,gunichar>
  ::SequenceToString(Glib::ustring::iterator pbegin, Glib::ustring::iterator pend)
:
  std::string(pbegin.base(), pend.base())
{}

ustring::SequenceToString<Glib::ustring::const_iterator,gunichar>
  ::SequenceToString(Glib::ustring::const_iterator pbegin, Glib::ustring::const_iterator pend)
:
  std::string(pbegin.base(), pend.base())
{}


/**** Glib::ustring -- stream I/O operators ********************************/

std::istream& operator>>(std::istream& is, Glib::ustring& utf8_string)
{
  std::string locale_string;
  is >> locale_string;
  utf8_string = Glib::locale_to_utf8(locale_string);
  return is;
}

std::ostream& operator<<(std::ostream& os, const Glib::ustring& utf8_string)
{
  os << Glib::locale_from_utf8(utf8_string);
  return os;
}

} // namespace Glib

