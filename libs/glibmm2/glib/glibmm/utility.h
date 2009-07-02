// -*- c++ -*-
#ifndef _GLIBMM_UTILITY_H
#define _GLIBMM_UTILITY_H
/* $Id: utility.h 749 2008-12-10 14:23:33Z jjongsma $ */

/* Copyright 2002 The gtkmm Development Team
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

#include <glib.h>
#include <glibmm/ustring.h>


#ifndef DOXYGEN_SHOULD_SKIP_THIS

/* Occasionally, a struct variable has to be initialized after its definition,
 * i.e. when using structs as class member data.  For convenience, the macro
 * GLIBMM_INITIALIZE_STRUCT(Var, Type) is provided.  It even avoids creating
 * a temporary if the compiler is GCC.
 */
#if ((__GNUC__ >= 3) || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96)) && !defined(__STRICT_ANSI__)

#define GLIBMM_INITIALIZE_STRUCT(Var, Type) __builtin_memset(&(Var), 0, sizeof(Type))

#else

#define GLIBMM_INITIALIZE_STRUCT(Var, Type) \
    G_STMT_START{ \
        Type const temp_initializer__ = { 0, }; \
        (Var) = temp_initializer__; \
    }G_STMT_END

#endif /* ((__GNUC__ >= 3) || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96)) && !defined(__STRICT_ANSI__) */


namespace Glib
{

// These are used by gtkmmproc-generated type conversions:

// Helper to deal with memory allocated
// by GLib functions in an exception-safe manner.
template <typename T>
class ScopedPtr
{
private:
  T* ptr_;
  ScopedPtr(const ScopedPtr<T>&);
  ScopedPtr<T>& operator=(const ScopedPtr<T>&);

public:
  ScopedPtr()                 : ptr_ (0)   {}
  explicit ScopedPtr(T* ptr)  : ptr_ (ptr) {}
  ~ScopedPtr()                { g_free(ptr_); }
  T*  get() const             { return ptr_;  }
  T** addr()                  { return &ptr_; }
};

// Removes the const nature of a ptr
template <class T>
inline T* unconst(const T* t)
  { return const_cast<T*>(t); }

// Convert const gchar* to ustring, while treating NULL as empty string.
inline
Glib::ustring convert_const_gchar_ptr_to_ustring(const char* str)
{
  return (str) ? Glib::ustring(str) : Glib::ustring();
}

// Convert const gchar* to std::string, while treating NULL as empty string.
inline
std::string convert_const_gchar_ptr_to_stdstring(const char* str)
{
  return (str) ? std::string(str) : std::string();
}

// Convert a non-const gchar* return value to ustring, freeing it too.
inline
Glib::ustring convert_return_gchar_ptr_to_ustring(char* str)
{
  return (str) ? Glib::ustring(Glib::ScopedPtr<char>(str).get())
               : Glib::ustring();
}

// Convert a non-const gchar* return value to std::string, freeing it too.
inline
std::string convert_return_gchar_ptr_to_stdstring(char* str)
{
  return (str) ? std::string(Glib::ScopedPtr<char>(str).get())
               : std::string();
}

// Append type_name to dest, while replacing special characters with '+'.
void append_canonical_typename(std::string& dest, const char* type_name);

} // namespace Glib

#endif /* DOXYGEN_SHOULD_SKIP_THIS */


#endif /* _GLIBMM_UTILITY_H */

