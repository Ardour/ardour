/*
 * Copyright (C) 2009-2015 Paul Davis <paul@linuxaudiosystems.com>
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


#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#ifdef PLATFORM_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "pbd/error.h"
#include "pbd/crossthread.h"

using namespace std;
using namespace PBD;
using namespace Glib;

#ifndef PLATFORM_WINDOWS
#include "crossthread.posix.cc"
#else
#include "crossthread.win.cc"
#endif

#ifndef G_SOURCE_FUNC
#define G_SOURCE_FUNC(f) ((GSourceFunc) (void (*)(void)) (f))
#endif

gboolean
cross_thread_channel_call_receive_slot (GIOChannel*, GIOCondition condition, void *data)
{
        CrossThreadChannel* ctc = static_cast<CrossThreadChannel*>(data);
        return ctc->receive_slot (Glib::IOCondition (condition));
}

void
CrossThreadChannel::set_receive_handler (sigc::slot<bool,Glib::IOCondition> s)
{
        receive_slot = s;
}

void
CrossThreadChannel::attach (Glib::RefPtr<Glib::MainContext> context)
{
        receive_source = g_io_create_watch (receive_channel, GIOCondition(G_IO_IN|G_IO_PRI|G_IO_ERR|G_IO_HUP|G_IO_NVAL));

        g_source_set_callback (receive_source, G_SOURCE_FUNC(cross_thread_channel_call_receive_slot), this, NULL);
        g_source_attach (receive_source, context->gobj());
}
