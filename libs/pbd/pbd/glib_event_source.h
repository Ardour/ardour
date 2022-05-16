/*
 * Copyright (C) 2022 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libpbd_glib_event_source_h__
#define __libpbd_glib_event_source_h__

#include <boost/function.hpp>

#include <glibmm/main.h>
#include "pbd/libpbd_visibility.h"

class LIBPBD_API GlibEventLoopSource : public Glib::Source
{
  public:
	GlibEventLoopSource () {};

	bool prepare (int& timeout);
	bool check();
	bool dispatch (sigc::slot_base*);
};


class LIBPBD_API GlibEventLoopCallback : public GlibEventLoopSource
{
  public:
	GlibEventLoopCallback (boost::function<void()> callback) : _callback (callback) {}

	bool check() {
		_callback();
		return false;
	}

  private:
	boost::function<void()> _callback;
};

#endif /* __libpbd_glib_event_source_h__ */
