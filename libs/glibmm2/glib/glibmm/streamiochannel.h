// -*- c++ -*-
/* $Id: streamiochannel.h,v 1.2 2003/01/22 12:08:52 murrayc Exp $ */

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

#ifndef _GLIBMM_STREAMIOCHANNEL_H
#define _GLIBMM_STREAMIOCHANNEL_H

#include <glibmm/iochannel.h>
#include <glibmmconfig.h>
#include <iosfwd>

GLIBMM_USING_STD(istream)
GLIBMM_USING_STD(ostream)
GLIBMM_USING_STD(iostream)


namespace Glib
{

/** This whole class is deprecated in glibmm&nbsp;2.2.
 * See the Glib::IOChannel documentation for an explanation.
 */
class StreamIOChannel : public Glib::IOChannel
{
public:
  virtual ~StreamIOChannel();

  static Glib::RefPtr<StreamIOChannel> create(std::istream& stream);
  static Glib::RefPtr<StreamIOChannel> create(std::ostream& stream);
  static Glib::RefPtr<StreamIOChannel> create(std::iostream& stream);

protected:
  std::istream* stream_in_;
  std::ostream* stream_out_;

  StreamIOChannel(std::istream* stream_in, std::ostream* stream_out);

  virtual IOStatus read_vfunc(char* buf, gsize count, gsize& bytes_read);
  virtual IOStatus write_vfunc(const char* buf, gsize count, gsize& bytes_written);
  virtual IOStatus seek_vfunc(gint64 offset, SeekType type);
  virtual IOStatus close_vfunc();
  virtual IOStatus set_flags_vfunc(IOFlags flags);
  virtual IOFlags  get_flags_vfunc();
  virtual Glib::RefPtr<Glib::Source> create_watch_vfunc(IOCondition cond);
};

} // namespace Glib


#endif /* _GLIBMM_STREAMIOCHANNEL_H */

