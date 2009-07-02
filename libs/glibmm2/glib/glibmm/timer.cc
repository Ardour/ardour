// -*- c++ -*-
/* $Id: timer.cc 749 2008-12-10 14:23:33Z jjongsma $ */

/* timer.cc
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

#include <glib.h>
#include <glibmm/timer.h>


namespace Glib
{

Timer::Timer()
:
  gobject_ (g_timer_new())
{}

Timer::~Timer()
{
  g_timer_destroy(gobject_);
}

void Timer::start()
{
  g_timer_start(gobject_);
}

void Timer::stop()
{
  g_timer_stop(gobject_);
}

void Timer::reset()
{
  g_timer_reset(gobject_);
}

double Timer::elapsed() const
{
  return g_timer_elapsed(gobject_, 0);
}

double Timer::elapsed(unsigned long& microseconds) const
{
  return g_timer_elapsed(gobject_, &microseconds);
}


void usleep(unsigned long microseconds)
{
  g_usleep(microseconds);
}

} // namespace Glib

