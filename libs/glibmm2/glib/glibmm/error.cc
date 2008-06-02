// -*- c++ -*-
/* $Id: error.cc 291 2006-05-12 08:08:45Z murrayc $ */

/* error.cc
 *
 * Copyright 2002 The gtkmm Development Team
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

#include <glib/gerror.h>
#include <glib.h>

#include <map>
#include <glibmmconfig.h>
#include <glibmm/error.h>
#include <glibmm/wrap_init.h>

GLIBMM_USING_STD(map)

namespace
{

typedef std::map<GQuark,Glib::Error::ThrowFunc> ThrowFuncTable;

static ThrowFuncTable* throw_func_table = 0;

} // anonymous namespace


namespace Glib
{

Error::Error()
:
  gobject_ (0)
{}

Error::Error(GQuark domain, int code, const Glib::ustring& message)
:
  gobject_ (g_error_new_literal(domain, code, message.c_str()))
{}

Error::Error(GError* gobject, bool take_copy)
:
  gobject_ ((take_copy && gobject) ? g_error_copy(gobject) : gobject)
{}

Error::Error(const Error& other)
:
  Exception(other),
  gobject_ ((other.gobject_) ? g_error_copy(other.gobject_) : 0)
{}

Error& Error::operator=(const Error& other)
{
  if(gobject_ != other.gobject_)
  {
    if(gobject_)
    {
      g_error_free(gobject_);
      gobject_ = 0;
    }
    if(other.gobject_)
    {
      gobject_ = g_error_copy(other.gobject_);
    }
  }
  return *this;
}

Error::~Error() throw()
{
  if(gobject_)
    g_error_free(gobject_);
}

GQuark Error::domain() const
{
  g_return_val_if_fail(gobject_ != 0, 0);

  return gobject_->domain;
}

int Error::code() const
{
  g_return_val_if_fail(gobject_ != 0, -1);

  return gobject_->code;
}

Glib::ustring Error::what() const
{
  g_return_val_if_fail(gobject_ != 0, "");
  g_return_val_if_fail(gobject_->message != 0, "");

  return gobject_->message;
}

bool Error::matches(GQuark domain, int code) const
{
  return g_error_matches(gobject_, domain, code);
}

GError* Error::gobj()
{
  return gobject_;
}

const GError* Error::gobj() const
{
  return gobject_;
}

void Error::propagate(GError** dest)
{
  g_propagate_error(dest, gobject_);
  gobject_ = 0;
}

// static
void Error::register_init()
{
  if(!throw_func_table)
  {
    throw_func_table = new ThrowFuncTable();
    Glib::wrap_init(); // make sure that at least the Glib exceptions are registered
  }
}

// static
void Error::register_cleanup()
{
  if(throw_func_table)
  {
    delete throw_func_table;
    throw_func_table = 0;
  }
}

// static
void Error::register_domain(GQuark domain, Error::ThrowFunc throw_func)
{
  g_assert(throw_func_table != 0);

  (*throw_func_table)[domain] = throw_func;
}

#ifdef GLIBMM_EXCEPTIONS_ENABLED
// static, noreturn
void Error::throw_exception(GError* gobject)
#else
std::auto_ptr<Glib::Error> Error::throw_exception(GError* gobject)
#endif //GLIBMM_EXCEPTIONS_ENABLED
{
  g_assert(gobject != 0);

  // Just in case Gtk::Main hasn't been instantiated yet.
  if(!throw_func_table)
    register_init();

  if(const ThrowFunc throw_func = (*throw_func_table)[gobject->domain])
  {
    #ifdef GLIBMM_EXCEPTIONS_ENABLED
    (*throw_func)(gobject);
    #else
    return (*throw_func)(gobject);
    #endif //GLIBMM_EXCEPTIONS_ENABLED
    g_assert_not_reached();
  }

  g_warning("Glib::Error::throw_exception():\n  "
            "unknown error domain '%s': throwing generic Glib::Error exception\n",
            (gobject->domain) ? g_quark_to_string(gobject->domain) : "(null)");

#ifdef GLIBMM_EXCEPTIONS_ENABLED
  // Doesn't copy, because error-returning functions return a newly allocated GError for us.
  throw Glib::Error(gobject);
#else
  return std::auto_ptr<Glib::Error>(new Glib::Error(gobject));
#endif //GLIBMM_EXCEPTIONS_ENABLED
}


} // namespace Glib

