/* $Id: streamiochannel.cc,v 1.3 2006/05/12 08:08:43 murrayc Exp $ */

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

#include <glibmm/streamiochannel.h>
#include <glib.h>
#include <fstream>
#include <iostream>

GLIBMM_USING_STD(ios)


namespace Glib
{

// static
Glib::RefPtr<StreamIOChannel> StreamIOChannel::create(std::istream& stream)
{
  return Glib::RefPtr<StreamIOChannel>(new StreamIOChannel(&stream, 0));
}

// static
Glib::RefPtr<StreamIOChannel> StreamIOChannel::create(std::ostream& stream)
{
  return Glib::RefPtr<StreamIOChannel>(new StreamIOChannel(0, &stream));
}

// static
Glib::RefPtr<StreamIOChannel> StreamIOChannel::create(std::iostream& stream)
{
  return Glib::RefPtr<StreamIOChannel>(new StreamIOChannel(&stream, &stream));
}

StreamIOChannel::StreamIOChannel(std::istream* stream_in, std::ostream* stream_out)
:
  stream_in_  (stream_in),
  stream_out_ (stream_out)
{
  get_flags_vfunc(); // initialize GIOChannel flag bits
}

StreamIOChannel::~StreamIOChannel()
{}

IOStatus StreamIOChannel::read_vfunc(char* buf, gsize count, gsize& bytes_read)
{
  g_return_val_if_fail(stream_in_ != 0, IO_STATUS_ERROR);

  stream_in_->clear();
  stream_in_->read(buf, count);
  bytes_read = stream_in_->gcount();

  if(stream_in_->eof())
    return IO_STATUS_EOF;

  if(stream_in_->fail())
  {
    #ifdef GLIBMM_EXCEPTIONS_ENABLED
    throw Glib::Error(G_IO_CHANNEL_ERROR, G_IO_CHANNEL_ERROR_FAILED, "Reading from stream failed");
    #else
    return IO_STATUS_ERROR;
    #endif //GLIBMM_EXCEPTIONS_ENABLED
  }

  return IO_STATUS_NORMAL;
}

IOStatus StreamIOChannel::write_vfunc(const char* buf, gsize count, gsize& bytes_written)
{
  g_return_val_if_fail(stream_out_ != 0, IO_STATUS_ERROR);

  bytes_written = 0;

  stream_out_->clear();
  stream_out_->write(buf, count);

  if(stream_out_->fail())
  {
    #ifdef GLIBMM_EXCEPTIONS_ENABLED
    throw Glib::Error(G_IO_CHANNEL_ERROR, G_IO_CHANNEL_ERROR_FAILED, "Writing to stream failed");
    #else
      return IO_STATUS_ERROR;
    #endif //GLIBMM_EXCEPTIONS_ENABLED
  }

  bytes_written = count; // all or nothing ;)

  return IO_STATUS_NORMAL;
}

IOStatus StreamIOChannel::seek_vfunc(gint64 offset, SeekType type)
{
  std::ios::seekdir direction = std::ios::beg;

  switch(type)
  {
    case SEEK_TYPE_SET: direction = std::ios::beg; break;
    case SEEK_TYPE_CUR: direction = std::ios::cur; break;
    case SEEK_TYPE_END: direction = std::ios::end; break;
  }

  bool failed = false;

  if(stream_in_)
  {
    stream_in_->clear();
    stream_in_->seekg(offset, direction);
    failed = stream_in_->fail();
  }
  if(stream_out_)
  {
    stream_out_->clear();
    stream_out_->seekp(offset, direction);
    failed = (failed || stream_out_->fail());
  }

  if(failed)
  {
    #ifdef GLIBMM_EXCEPTIONS_ENABLED
    throw Glib::Error(G_IO_CHANNEL_ERROR, G_IO_CHANNEL_ERROR_FAILED, "Seeking into stream failed");
    #else
    return IO_STATUS_ERROR;
    #endif //GLIBMM_EXCEPTIONS_ENABLED
  }

  return Glib::IO_STATUS_NORMAL;
}

IOStatus StreamIOChannel::close_vfunc()
{
  bool failed = false;

  if(std::fstream *const stream = dynamic_cast<std::fstream*>(stream_in_))
  {
    stream->clear();
    stream->close();
    failed = stream->fail();
  }
  else if(std::ifstream *const stream = dynamic_cast<std::ifstream*>(stream_in_))
  {
    stream->clear();
    stream->close();
    failed = stream->fail();
  }
  else if(std::ofstream *const stream = dynamic_cast<std::ofstream*>(stream_out_))
  {
    stream->clear();
    stream->close();
    failed = stream->fail();
  }
  else
  {
    #ifdef GLIBMM_EXCEPTIONS_ENABLED
    throw Glib::Error(G_IO_CHANNEL_ERROR, G_IO_CHANNEL_ERROR_FAILED,
                      "Attempt to close non-file stream");
    #else
    return IO_STATUS_ERROR;
    #endif //GLIBMM_EXCEPTIONS_ENABLED
  }

  if(failed)
  {
    #ifdef GLIBMM_EXCEPTIONS_ENABLED
    throw Glib::Error(G_IO_CHANNEL_ERROR, G_IO_CHANNEL_ERROR_FAILED, "Failed to close stream");
    #else
    return IO_STATUS_ERROR;
    #endif //GLIBMM_EXCEPTIONS_ENABLED
  }

  return IO_STATUS_NORMAL;
}

IOStatus StreamIOChannel::set_flags_vfunc(IOFlags)
{
  return IO_STATUS_NORMAL;
}

IOFlags StreamIOChannel::get_flags_vfunc()
{
  gobj()->is_seekable  = 1;
  gobj()->is_readable  = (stream_in_  != 0);
  gobj()->is_writeable = (stream_out_ != 0);

  IOFlags flags = IO_FLAG_IS_SEEKABLE;

  if(stream_in_)
    flags |= IO_FLAG_IS_READABLE;
  if(stream_out_)
    flags |= IO_FLAG_IS_WRITEABLE;

  return flags;
}

Glib::RefPtr<Glib::Source> StreamIOChannel::create_watch_vfunc(IOCondition)
{
  g_warning("Glib::StreamIOChannel::create_watch_vfunc() not implemented");
  return Glib::RefPtr<Glib::Source>();
}

} // namespace Glib

