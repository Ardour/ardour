/* Copyright (C) 2004 The glibmm Development Team
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

/*
 * The fdstream/fdstreambuf example classes provide a streambuffer
 * interface for Glib::IOChannel, so that standard iostreams can be
 * used with fifos, pipes and sockets, with safe temporary files
 * opened with mkstemp() and with files opened with other system
 * functions such as Unix open().
 * 
 * It does not make use of the Glib::IOChannel automatic charset code
 * conversion facilities (which when enabled will convert from UTF-8
 * to the locale codeset when writing out, and vice-versa when reading
 * in).  Such automatic codeset conversion is usually undesirable as
 * it makes the target file unportable - a file written out in the
 * locale charset can only be used by those expecting the same locale
 * codeset.  It is also unnecessary as the <<() and >>() operators for
 * Glib::ustring already carry out this codeset conversion (to avoid
 * this use Glib::ustring::raw() when writing out to a stream via
 * operator <<(), and read in via a std::string object with operator
 * >>()).
 *
 * If an automatic codeset conversion option is thought to be
 * valuable, it would be possible to provide this by having a read
 * buffer in fdstreambuf large enough to take and putback six bytes
 * (the largest space occupied by a UTF-8 character).  This would
 * require rewriting fdstreambuf::underflow(), but in compensation
 * fdstreambuf::xsgetn() could be omitted, as if a read buffer were
 * provided then std::streambuf::xsgetn() would be adequate for the
 * purpose by itself.
 *
 * A serious implementation would probably also provide separate
 * read-only ifdstream classes and write-only ofdstream classes, as
 * fdstream provides both read and write facilities.
*/


#ifndef GLIBMMEXAMPLE_FDSTREAM_H
#define GLIBMMEXAMPLE_FDSTREAM_H

#include <istream>
#include <ostream>
#include <streambuf>
#include <glibmm/iochannel.h>

struct fdstream_error
{
  bool error;
  Glib::IOChannelError::Code code;  
};

class fdstreambuf: public std::streambuf
{
public:
  fdstreambuf(int fd, bool manage);
  fdstreambuf();
  ~fdstreambuf();

  void create_iochannel(int fd, bool manage);
  void detach_fd();
  void close_iochannel();
  void connect(const sigc::slot<bool, Glib::IOCondition>& callback, Glib::IOCondition condition);
  fdstream_error get_error() const;

protected:
  virtual int_type underflow();
  virtual std::streamsize xsgetn(char* dest, std::streamsize num);
  virtual int sync();
  virtual int_type overflow(int_type c);
  virtual std::streamsize xsputn(const char* source, std::streamsize num);

private:
  Glib::RefPtr<Glib::IOChannel> iochannel_;
  fdstream_error error_condition;

  // putback_buffer does not do any buffering: it reserves one character
  // for putback and one character for a peek() and/or for bumping
  // with sbumpc/uflow()
  char putback_buffer[2];

  void reset();
};

class fdstream : 
  public std::istream, 
  public std::ostream
{
public:

  explicit fdstream(int fd, bool manage = true);
  fdstream();

  // If fdstream is managing a file descriptor, attaching a new
  // one will close the old one - call detach() to unmanage it
  void attach(int fd, bool manage = true);
  void detach();

  void close();
  void connect(const sigc::slot<bool, Glib::IOCondition>& callback,
	       Glib::IOCondition condition);
  fdstream_error get_error() const;

private:
  fdstreambuf buf;
};

#endif /*GLIBMMEXAMPLE_FDSTREAM_H*/
