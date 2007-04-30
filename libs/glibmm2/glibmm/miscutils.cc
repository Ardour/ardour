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

#include <cstddef>
#include <cstring>

#include <glibmm/miscutils.h>
#include <glibmm/utility.h>
#include <glib.h>


namespace Glib
{

Glib::ustring get_application_name()
{
  if(const char *const application_name = g_get_application_name())
  {
    // Lets be a bit more strict than the original GLib function and ensure
    // we always return valid UTF-8.  gtkmm coders surely won't expect invalid
    // UTF-8 in a Glib::ustring returned by a glibmm function.

    if(g_utf8_validate(application_name, -1, 0))
      return Glib::ustring(application_name);

    char *const appname_utf8 = g_filename_to_utf8(application_name, -1, 0, 0, 0);
    g_return_val_if_fail(appname_utf8 != 0, "");

    return Glib::ustring(ScopedPtr<char>(appname_utf8).get());
  }

  return Glib::ustring();
}

void set_application_name(const Glib::ustring& application_name)
{
  g_set_application_name(application_name.c_str());
}

std::string get_prgname()
{
  const char *const prgname = g_get_prgname();
  return (prgname) ? std::string(prgname) : std::string();
}

void set_prgname(const std::string& prgname)
{
  g_set_prgname(prgname.c_str());
}

std::string getenv(const std::string& variable, bool& found)
{
  const char *const value = g_getenv(variable.c_str());
  found = (value != 0);
  return (value) ? std::string(value) : std::string();
}

std::string getenv(const std::string& variable)
{
  const char *const value = g_getenv(variable.c_str());
  return (value) ? std::string(value) : std::string();
}

bool setenv(const std::string& variable, const std::string& value, bool overwrite)
{
  return g_setenv(variable.c_str(), value.c_str(), overwrite);
}

void unsetenv(const std::string& variable)
{
  g_unsetenv(variable.c_str());
}

std::string get_user_name()
{
  return std::string(g_get_user_name());
}

std::string get_real_name()
{
  return std::string(g_get_real_name());
}

std::string get_home_dir()
{
  const char *const value = g_get_home_dir();
  return (value) ? std::string(value) : std::string();
}

std::string get_tmp_dir()
{
  return std::string(g_get_tmp_dir());
}

std::string get_current_dir()
{
  const ScopedPtr<char> buf (g_get_current_dir());
  return std::string(buf.get());
}

bool path_is_absolute(const std::string& filename)
{
  return g_path_is_absolute(filename.c_str());
}

std::string path_skip_root(const std::string& filename)
{
  // g_path_skip_root() returns a pointer _into_ the argument string,
  // or NULL if there was no root component.

  if(const char *const ptr = g_path_skip_root(filename.c_str()))
    return std::string(ptr);
  else
    return std::string();
}

std::string path_get_basename(const std::string& filename)
{
  const ScopedPtr<char> buf (g_path_get_basename(filename.c_str()));
  return std::string(buf.get());
}

std::string path_get_dirname(const std::string& filename)
{
  const ScopedPtr<char> buf (g_path_get_dirname(filename.c_str()));
  return std::string(buf.get());
}

std::string build_filename(const Glib::ArrayHandle<std::string>& elements)
{
  return Glib::convert_return_gchar_ptr_to_stdstring( g_build_filenamev(const_cast<char**>(elements.data())) );

}

std::string build_filename(const std::string& elem1, const std::string& elem2)
{
  const char *const elements[] = { elem1.c_str(), elem2.c_str(), 0 };
  return build_filename(elements);                                          
}

std::string build_path(const std::string& separator, const Glib::ArrayHandle<std::string>& elements)
{
  return Glib::convert_return_gchar_ptr_to_stdstring( g_build_pathv(separator.c_str(), const_cast<char**>(elements.data())) );

/* Yes, this reimplements the functionality of g_build_path() -- because
 * it takes a varargs list, and calling it several times would result
 * in different behaviour.
 */
  /*
  std::string result;
  result.reserve(256); //TODO: Explain why this magic number is useful. murrayc

  const char *const sep = separator.c_str();
  const size_t seplen   = separator.length();

  bool is_first     = true;
  bool have_leading = false;
  const char* single_element = 0;
  const char* last_trailing  = 0;

  const char *const *const elements_begin = elements.data();
  const char *const *const elements_end   = elements_begin + elements.size();

  for(const char *const * pelement = elements_begin; pelement != elements_end; ++pelement)
  {
    const char* start = *pelement;

    if(*start == '\0')
      continue; // ignore empty elements

    if(seplen != 0)
    {
      while(strncmp(start, sep, seplen) == 0)
        start += seplen;
    }

    const char* end = start + strlen(start);

    if(seplen != 0)
    {
      while(end >= start + seplen && strncmp(end - seplen, sep, seplen) == 0)
        end -= seplen;

      last_trailing = end;

      while(last_trailing >= *pelement + seplen && strncmp(last_trailing - seplen, sep, seplen) == 0)
        last_trailing -= seplen;

      if(!have_leading)
      {
        // If the leading and trailing separator strings are in the
        // same element and overlap, the result is exactly that element.
        //
        if(last_trailing <= start)
          single_element = *pelement;

        result.append(*pelement, start);
        have_leading = true;
      }
      else
        single_element = 0;
    }

    if(end == start)
      continue;

    if(!is_first)
      result += separator;

    result.append(start, end);
    is_first = false;
  }

  if(single_element)
    result = single_element;
  else if(last_trailing)
    result += last_trailing;

  return result;
  */
}

std::string find_program_in_path(const std::string& program)
{
  if(char *const buf = g_find_program_in_path(program.c_str()))
    return std::string(ScopedPtr<char>(buf).get());
  else
    return std::string();
}

} // namespace Glib

