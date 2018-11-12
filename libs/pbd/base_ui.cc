/*
    Copyright (C) 2000-2007 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <cstring>
#include <stdint.h>
#ifdef COMPILER_MSVC
#include <io.h>      // Microsoft's nearest equivalent to <unistd.h>
#else
#include <unistd.h>
#endif
#include <fcntl.h>
#include <cerrno>
#include <cstring>

#include <pthread.h>
#include <sched.h>

#include "pbd/base_ui.h"
#include "pbd/debug.h"
#include "pbd/pthread_utils.h"
#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/failed_constructor.h"

#include "pbd/i18n.h"

#include "pbd/debug.h"

using namespace std;
using namespace PBD;
using namespace Glib;

uint64_t BaseUI::rt_bit = 1;
BaseUI::RequestType BaseUI::CallSlot = BaseUI::new_request_type();
BaseUI::RequestType BaseUI::Quit = BaseUI::new_request_type();

BaseUI::BaseUI (const string& loop_name)
	: EventLoop (loop_name)
	, m_context(MainContext::get_default())
	, run_loop_thread (0)
	, request_channel (true)
{
	base_ui_instance = this;
	request_channel.set_receive_handler (sigc::mem_fun (*this, &BaseUI::request_handler));

	/* derived class must set _ok */
}

BaseUI::~BaseUI()
{
}

BaseUI::RequestType
BaseUI::new_request_type ()
{
	RequestType rt;

	/* XXX catch out-of-range */

	rt = RequestType (rt_bit);
	rt_bit <<= 1;

	return rt;
}

int
BaseUI::set_thread_priority (const int policy, int priority) const
{
	return pbd_set_thread_priority (pthread_self(), policy, priority);
}

void
BaseUI::main_thread ()
{
	DEBUG_TRACE (DEBUG::EventLoop, string_compose ("%1: event loop running in thread %2\n", event_loop_name(), pthread_name()));
	set_event_loop_for_thread (this);
	thread_init ();
	_main_loop->get_context()->signal_idle().connect (sigc::mem_fun (*this, &BaseUI::signal_running));
	_main_loop->run ();
}

bool
BaseUI::signal_running ()
{
	Glib::Threads::Mutex::Lock lm (_run_lock);
	_running.signal ();

	return false; // don't call it again
}

void
BaseUI::run ()
{
	/* to be called by UI's that need/want their own distinct, self-created event loop thread.
	*/

	m_context = MainContext::create();
	_main_loop = MainLoop::create (m_context);
	attach_request_source ();

	Glib::Threads::Mutex::Lock lm (_run_lock);
	run_loop_thread = Glib::Threads::Thread::create (mem_fun (*this, &BaseUI::main_thread));
	_running.wait (_run_lock);
}

void
BaseUI::quit ()
{
	if (_main_loop && _main_loop->is_running()) {
		_main_loop->quit ();
		run_loop_thread->join ();
	}
}

bool
BaseUI::request_handler (Glib::IOCondition ioc)
{
	/* check the request pipe */

	if (ioc & ~IO_IN) {
		_main_loop->quit ();
	}

	if (ioc & IO_IN) {
		request_channel.drain ();

		/* there may been an error. we'd rather handle requests first,
		   and then get IO_HUP or IO_ERR on the next loop.
		*/

		/* handle requests */

		DEBUG_TRACE (DEBUG::EventLoop, string_compose ("%1: request handler\n", event_loop_name()));
		handle_ui_requests ();
	}

	return true;
}

void
BaseUI::signal_new_request ()
{
	DEBUG_TRACE (DEBUG::EventLoop, string_compose ("%1: signal_new_request\n", event_loop_name()));
	request_channel.wakeup ();
}

/**
 * This method relies on the caller having already set m_context
 */
void
BaseUI::attach_request_source ()
{
	DEBUG_TRACE (DEBUG::EventLoop, string_compose ("%1: attach request source\n", event_loop_name()));
	request_channel.attach (m_context);
}
