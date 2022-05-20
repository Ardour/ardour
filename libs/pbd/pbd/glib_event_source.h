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

#include <glib.h>
#include <glibmm/main.h>
#include "pbd/libpbd_visibility.h"

class LIBPBD_API GlibEventLoopCallback
{
  public:
	GlibEventLoopCallback (boost::function<void()> callback);
	~GlibEventLoopCallback();

	static gboolean c_prepare (GSource*, gint* timeout);
	void attach (Glib::RefPtr<Glib::MainContext>);

  private:
	struct GSourceWithParent {
		GSource c;
		GlibEventLoopCallback* cpp;
	};

	bool cpp_prepare();

	GSourceWithParent* gsource;
	GSourceFuncs funcs;
	boost::function<void()> _callback;
};

#endif /* __libpbd_glib_event_source_h__ */
