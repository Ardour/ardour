// -*- c++ -*-
#ifndef _GLIBMM_EXCEPTIONHANDLER_H
#define _GLIBMM_EXCEPTIONHANDLER_H

/* $Id: exceptionhandler.h 291 2006-05-12 08:08:45Z murrayc $ */

/* exceptionhandler.h
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

#include <sigc++/sigc++.h>
#include <glibmmconfig.h>

#ifdef GLIBMM_EXCEPTIONS_ENABLED

namespace Glib
{

/** Specify a slot to be called when an exception is thrown by a signal handler.
 */
sigc::connection add_exception_handler(const sigc::slot<void>& slot);

#ifndef DOXYGEN_SHOULD_SKIP_THIS
// internal
void exception_handlers_invoke() throw();
#endif

} // namespace Glib

#endif //GLIBMM_EXCEPTIONS_ENABLED

#endif /* _GLIBMM_EXCEPTIONHANDLER_H */

