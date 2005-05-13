// -*- c++ -*-
#ifndef _GLIBMM_STRINGUTILS_H
#define _GLIBMM_STRINGUTILS_H

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


namespace Glib
{

/** @defgroup StringUtils String Utility Functions
 *
 * This section describes a number of utility functions for creating
 * and manipulating strings, as well as other string-related stuff.
 */

/** Looks whether the string @a str begins with @a prefix.
 * @ingroup StringUtils
 * @param str A string.
 * @param prefix The prefix to look for.
 * @return <tt>true</tt> if @a str begins with @a prefix, <tt>false</tt> otherwise.
 */
bool str_has_prefix(const std::string& str, const std::string& prefix);

/** Looks whether the string @a str ends with @a suffix.
 * @ingroup StringUtils
 * @param str A string.
 * @param suffix The suffix to look for.
 * @return <tt>true</tt> if @a str ends with @a suffix, <tt>false</tt> otherwise.
 */
bool str_has_suffix(const std::string& str, const std::string& suffix);


namespace Ascii
{

/** Converts a string to a <tt>double</tt> value.
 * @ingroup StringUtils
 * This function behaves like the standard <tt>%strtod()</tt> function does in
 * the C&nbsp;locale. It does this without actually changing the current
 * locale, since that would not be thread-safe.
 *
 * This function is typically used when reading configuration files or other
 * non-user input that should be locale independent. To handle input from the
 * user you should normally use locale-sensitive C++ streams.
 *
 * To convert from a string to <tt>double</tt> in a locale-insensitive way, use
 * Glib::Ascii::dtostr().
 *
 * @param str The string to convert to a numeric value.
 * @return The <tt>double</tt> value.
 * @throw std::overflow_error  Thrown if the correct value would cause overflow.
 * @throw std::underflow_error Thrown if the correct value would cause underflow.
 */
double strtod(const std::string& str);

/** Converts a string to a <tt>double</tt> value.
 * @ingroup StringUtils
 * This function behaves like the standard <tt>%strtod()</tt> function does in
 * the C&nbsp;locale. It does this without actually changing the current
 * locale, since that would not be thread-safe.
 *
 * This function is typically used when reading configuration files or other
 * non-user input that should be locale independent. To handle input from the
 * user you should normally use locale-sensitive C++ streams.
 *
 * To convert from a string to <tt>double</tt> in a locale-insensitive way, use
 * Glib::Ascii::dtostr().
 *
 * @param str The string to convert to a numeric value.
 * @param start_index The index of the first character that should be used in the conversion.
 * @retval end_index The index of the character after the last character used in the conversion.
 * @return The <tt>double</tt> value.
 * @throw std::out_of_range    Thrown if @a start_index is out of range.
 * @throw std::overflow_error  Thrown if the correct value would cause overflow.
 * @throw std::underflow_error Thrown if the correct value would cause underflow.
 */
double strtod(const std::string&      str,
              std::string::size_type& end_index,
              std::string::size_type  start_index = 0);

/** Converts a <tt>double</tt> to a string, using the <tt>'.'</tt> as decimal point.
 * @ingroup StringUtils
 * This functions generates enough precision that converting the string back
 * using Glib::Ascii::strtod() gives the same machine-number (on machines with
 * IEEE compatible 64bit doubles).
 *
 * @param d The <tt>double</tt> value to convert.
 * @return The converted string.
 */
std::string dtostr(double d);

} // namespace Ascii


/** Escapes all special characters in the string.
 * @ingroup StringUtils
 * Escapes the special characters <tt>'\\b'</tt>, <tt>'\\f'</tt>, <tt>'\\n'</tt>,
 * <tt>'\\r'</tt>, <tt>'\\t'</tt>, <tt>'\\'</tt> and <tt>'"'</tt> in the string
 * @a source by inserting a <tt>'\\'</tt> before them. Additionally all characters
 * in the range <tt>0x01</tt>&nbsp;-&nbsp;<tt>0x1F</tt> (everything below <tt>SPACE</tt>)
 * and in the range <tt>0x80</tt>&nbsp;-&nbsp;<tt>0xFF</tt> (all non-ASCII chars)
 * are replaced with a <tt>'\\'</tt> followed by their octal representation.
 *
 * Glib::strcompress() does the reverse conversion.
 *
 * @param source A string to escape.
 * @return A copy of @a source with certain characters escaped. See above.
 */
std::string strescape(const std::string& source);

/** Escapes all special characters in the string.
 * @ingroup StringUtils
 * Escapes the special characters <tt>'\\b'</tt>, <tt>'\\f'</tt>, <tt>'\\n'</tt>,
 * <tt>'\\r'</tt>, <tt>'\\t'</tt>, <tt>'\\'</tt> and <tt>'"'</tt> in the string
 * @a source by inserting a <tt>'\\'</tt> before them. Additionally all characters
 * in the range <tt>0x01</tt>&nbsp;-&nbsp;<tt>0x1F</tt> (everything below <tt>SPACE</tt>)
 * and in the range <tt>0x80</tt>&nbsp;-&nbsp;<tt>0xFF</tt> (all non-ASCII chars)
 * are replaced with a <tt>'\\'</tt> followed by their octal representation.
 * Characters supplied in @a exceptions are not escaped.
 *
 * Glib::strcompress() does the reverse conversion.
 *
 * @param source A string to escape.
 * @param exceptions A string of characters not to escape in @a source.
 * @return A copy of @a source with certain characters escaped. See above.
 */
std::string strescape(const std::string& source, const std::string& exceptions);

/** Replaces all escaped characters with their one byte equivalent.
 * @ingroup StringUtils
 * This function does the reverse conversion of Glib::strescape().
 *
 * @param source A string to compress.
 * @return A copy of @a source with all escaped characters compressed.
 */
std::string strcompress(const std::string& source);

/** Returns a string corresponding to the given error code, e.g.\ <tt>"no such process"</tt>.
 * @ingroup StringUtils
 * This function is included since not all platforms support the
 * <tt>%strerror()</tt> function.
 *
 * @param errnum The system error number. See the standard C <tt>errno</tt> documentation.
 * @return A string describing the error code. If the error code is unknown,
 * <tt>&quot;unknown error (<em>\<errnum\></em>)&quot;</tt> is returned.
 */
Glib::ustring strerror(int errnum);

/** Returns a string describing the given signal, e.g.\ <tt>"Segmentation fault"</tt>.
 * @ingroup StringUtils
 * This function is included since not all platforms support the
 * <tt>%strsignal()</tt> function.
 *
 * @param signum The signal number. See the <tt>signal()</tt> documentation.
 * @return A string describing the signal. If the signal is unknown,
 * <tt>&quot;unknown signal (<em>\<signum\></em>)&quot;</tt> is returned.
 */
Glib::ustring strsignal(int signum);

} // namespace Glib


#endif /* _GLIBMM_STRINGUTILS_H */

