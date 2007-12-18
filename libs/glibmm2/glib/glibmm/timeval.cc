// -*- c++ -*-
/* $Id: timeval.cc 2 2003-01-07 16:59:16Z murrayc $ */

/* timeval.cc
 *
 * Copyright (C) 2002 The gtkmm Development Team
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

#include <glib/gmain.h>
#include <glib/gmessages.h>
#include <glib/gtimer.h>

#include <glibmm/timeval.h>


namespace Glib
{

void TimeVal::assign_current_time()
{
  g_get_current_time(this);
}

void TimeVal::add(const TimeVal& rhs)
{
  g_return_if_fail(tv_usec >= 0 && tv_usec < G_USEC_PER_SEC);
  g_return_if_fail(rhs.tv_usec >= 0 && rhs.tv_usec < G_USEC_PER_SEC);

  tv_usec += rhs.tv_usec;

  if(tv_usec >= G_USEC_PER_SEC)
  {
    tv_usec -= G_USEC_PER_SEC;
    ++tv_sec;
  }

  tv_sec += rhs.tv_sec;
}

void TimeVal::subtract(const TimeVal& rhs)
{
  g_return_if_fail(tv_usec >= 0 && tv_usec < G_USEC_PER_SEC);
  g_return_if_fail(rhs.tv_usec >= 0 && rhs.tv_usec < G_USEC_PER_SEC);

  tv_usec -= rhs.tv_usec;

  if(tv_usec < 0)
  {
    tv_usec += G_USEC_PER_SEC;
    --tv_sec;
  }

  tv_sec -= rhs.tv_sec;
}

void TimeVal::add_seconds(long seconds)
{
  g_return_if_fail(tv_usec >= 0 && tv_usec < G_USEC_PER_SEC);

  tv_sec += seconds;
}

void TimeVal::subtract_seconds(long seconds)
{
  g_return_if_fail(tv_usec >= 0 && tv_usec < G_USEC_PER_SEC);

  tv_sec -= seconds;
}

void TimeVal::add_milliseconds(long milliseconds)
{
  g_return_if_fail(tv_usec >= 0 && tv_usec < G_USEC_PER_SEC);

  tv_usec += (milliseconds % 1000) * 1000;

  if(tv_usec < 0)
  {
    tv_usec += G_USEC_PER_SEC;
    --tv_sec;
  }
  else if(tv_usec >= G_USEC_PER_SEC)
  {
    tv_usec -= G_USEC_PER_SEC;
    ++tv_sec;
  }

  tv_sec += milliseconds / 1000;
}

void TimeVal::subtract_milliseconds(long milliseconds)
{
  add_milliseconds(-1 * milliseconds);
}

void TimeVal::add_microseconds(long microseconds)
{ 
  g_time_val_add(this, microseconds);
}

void TimeVal::subtract_microseconds(long microseconds)
{
  g_time_val_add(this, -1 * microseconds);
}

} // namespace Glib

