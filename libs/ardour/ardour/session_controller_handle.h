/*
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __libardour_session_controller_handle_h__
#define __libardour_session_controller_handle_h__

#include "ardour/libardour_visibility.h"
#include "ardour/session_controller.h"
#include "ardour/session_handle.h"

namespace ARDOUR {

class Session;
class SessionController;

class LIBARDOUR_API SessionControllerHandleRef : public SessionHandleRef
{
  public:
	SessionControllerHandleRef (Session& s);

	virtual ~SessionControllerHandleRef ();

  protected:
	SessionController _controller;
};

class LIBARDOUR_API SessionControllerHandlePtr : public SessionHandlePtr
{
  public:
	SessionControllerHandlePtr (Session* s);
	SessionControllerHandlePtr ();

	virtual ~SessionControllerHandlePtr ();

	virtual void set_session (Session *);

  protected:
	SessionController _controller;
};

} /* namespace */

#endif /* __libardour_session_controller_handle_h__ */
