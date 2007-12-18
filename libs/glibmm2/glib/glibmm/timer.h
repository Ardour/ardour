// -*- c++ -*-
#ifndef _GLIBMM_TIMER_H
#define _GLIBMM_TIMER_H

/* $Id: timer.h 2 2003-01-07 16:59:16Z murrayc $ */

/* timer.h
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

extern "C" { typedef struct _GTimer GTimer; }


namespace Glib
{

/** Portable stop watch interface.
 * This resembles a convient and portable timer with microseconds resolution.
 */
class Timer
{
public:
  /** Create a new timer.
   * Also starts timing by calling start() implicitly.
   */
  Timer();
  ~Timer();

  void start();
  void stop();
  void reset();

  /** Get the elapsed time.
   * @return The value in seconds.
   */
  double elapsed() const;

  /** Get the elapsed time.
   * @return The value in seconds.  Also fills @p microseconds
   * with the corresponding @htmlonly&micro;s@endhtmlonly value.
   */
  double elapsed(unsigned long& microseconds) const;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  GTimer*       gobj()       { return gobject_; }
  const GTimer* gobj() const { return gobject_; }
#endif

private:
  GTimer* gobject_;

  // not copyable
  Timer(const Timer&);
  Timer& operator=(const Timer&);
};


void usleep(unsigned long microseconds);

} // namespace Glib


#endif /* _GLIBMM_TIMER_H */

