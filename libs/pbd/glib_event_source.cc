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

#include "pbd/glib_event_source.h"

GlibEventLoopCallback::GlibEventLoopCallback (boost::function<void()> callback)
	: _callback (callback)
{
	funcs.prepare = c_prepare;;
	funcs.check = NULL;
	funcs.dispatch = NULL;
	funcs.finalize = NULL;

	gsource = (GSourceWithParent*) g_source_new (&funcs, sizeof (GSourceWithParent));
	gsource->cpp = this;
}

GlibEventLoopCallback::~GlibEventLoopCallback ()
{
	g_source_destroy ((GSource*) gsource);
}

void
GlibEventLoopCallback::attach (Glib::RefPtr<Glib::MainContext> ctxt)
{
	g_source_attach ((GSource*) gsource, ctxt->gobj());
}

gboolean
GlibEventLoopCallback::c_prepare (GSource* gsrc, int* timeout)
{
	GSourceWithParent* gwp = reinterpret_cast<GSourceWithParent*> (gsrc);
	GlibEventLoopCallback* cpp = gwp->cpp;
	return cpp->cpp_prepare ();
}

bool
GlibEventLoopCallback::cpp_prepare ()
{
	_callback();
	return false;
}

