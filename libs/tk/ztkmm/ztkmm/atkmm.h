/* $Id$ */

/* atkmm - a C++ wrapper for the GLib toolkit
 *
 * Copyright 2002 The gtkmm Development Team
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

#ifndef _ATKMM_H
#define _ATKMM_H


/** @mainpage atkmm Reference Manual
 *
 * @section description Description
 *
 * atkmm is the official C++ interface for the <a href="http://library.gnome.org/devel/atk/stable/">ATK</a> accessibility toolkit library. It may be used, for instance, by user interfaces implemented with gtkmm.
 *
 * @section basics Basic usage
 *
 * Include the atkmm header:
 * @code
 * #include <atkmm.h>
 * @endcode
 * This includes every header installed by atkmm, so can slow down
 * compilation, but suffices for this simple example. Assuming that your
 * program source file is @c program.cc, compile it with:
 * @code
 * g++ program.cc -o program  `pkg-config --cflags --libs atkmm-1.6`
 * @endcode
 * Alternatively, if using autoconf, use the following in @c configure.ac:
 * @code
 * PKG_CHECK_MODULES([ATKMM], [atkmm-1.6])
 * @endcode
 * Then use the generated @c ATKMM_CFLAGS and @c ATKMM_LIBS variables in
 * the project @c Makefile.am files. For example:
 * @code
 * program_CPPFLAGS = $(ATKMM_CFLAGS)
 * program_LDADD = $(ATKMM_LIBS)
 * @endcode
 */


#include <atkmm/action.h>
#include <atkmm/component.h>
#include <atkmm/document.h>
#include <atkmm/editabletext.h>
#include <atkmm/image.h>
#include <atkmm/implementor.h>
#include <atkmm/object.h>
#include <atkmm/objectaccessible.h>
#include <atkmm/relation.h>
#include <atkmm/relationset.h>
#include <atkmm/selection.h>
#include <atkmm/stateset.h>
#include <atkmm/table.h>
#include <atkmm/text.h>
#include <atkmm/value.h>

#endif /* _ATKMM_H */
