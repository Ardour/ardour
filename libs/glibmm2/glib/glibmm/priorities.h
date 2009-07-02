// -*- c++ -*-
#ifndef _GLIBMM_PRIORITIES_H
#define _GLIBMM_PRIORITIES_H

/* $Id: priorities.h 420 2007-06-22 15:29:58Z murrayc $ */

/* Copyright (C) 2002-2008 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

namespace Glib
{

enum
{
  /*! Use this for high priority event sources.  It is not used within
   * GLib or GTK+.<br><br>
   */
  PRIORITY_HIGH = -100,

  /*! Use this for default priority event sources.  In glibmm this
   * priority is used by default when installing timeout handlers with
   * SignalTimeout::connect().  In GDK this priority is used for events
   * from the X server.<br><br>
   */
  PRIORITY_DEFAULT = 0,

  /*! Use this for high priority idle functions.  GTK+ uses
   * <tt>PRIORITY_HIGH_IDLE&nbsp;+&nbsp;10</tt> for resizing operations, and
   * <tt>PRIORITY_HIGH_IDLE&nbsp;+&nbsp;20</tt> for redrawing operations.
   * (This is done to ensure that any pending resizes are processed before
   * any pending redraws, so that widgets are not redrawn twice unnecessarily.)
   * <br><br>
   */
  PRIORITY_HIGH_IDLE = 100,

  /*! Use this for default priority idle functions.  In glibmm this priority is
   * used by default when installing idle handlers with SignalIdle::connect().
   * <br><br>
   */
  PRIORITY_DEFAULT_IDLE = 200,

  /*! Use this for very low priority background tasks.  It is not used within
   * GLib or GTK+.
   */
  PRIORITY_LOW = 300
};

} //namespace Glib

#endif //#ifndef _GLIBMM_PRIORITIES_H

