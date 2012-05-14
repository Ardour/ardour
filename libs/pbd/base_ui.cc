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
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

#include "pbd/base_ui.h"
#include "pbd/debug.h"
#include "pbd/pthread_utils.h"
#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/failed_constructor.h"

#include "i18n.h"

using namespace std;
using namespace PBD;
using namespace Glib;
	
uint64_t BaseUI::rt_bit = 1;
BaseUI::RequestType BaseUI::CallSlot = BaseUI::new_request_type();
BaseUI::RequestType BaseUI::Quit = BaseUI::new_request_type();

BaseUI::BaseUI (const string& str)
	: request_channel (true)
	, run_loop_thread (0)
	, _name (str)
{
	base_ui_instance = this;

	request_channel.ios()->connect (sigc::mem_fun (*this, &BaseUI::request_handler));

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

void
BaseUI::main_thread ()
{
	DEBUG_TRACE (DEBUG::EventLoop, string_compose ("%1: event loop running in thread %2\n", name(), pthread_self()));
	set_event_loop_for_thread (this);
	thread_init ();
	_main_loop->get_context()->signal_idle().connect (sigc::mem_fun (*this, &BaseUI::signal_running));
	_main_loop->run ();
}

bool
BaseUI::signal_running ()
{
	Glib::Mutex::Lock lm (_run_lock);
	_running.signal ();
	
	return false; // don't call it again
}

void
BaseUI::run ()
{
	/* to be called by UI's that need/want their own distinct, self-created event loop thread.
	*/

	_main_loop = MainLoop::create (MainContext::create());
	request_channel.ios()->attach (_main_loop->get_context());

	/* glibmm hack - drop the refptr to the IOSource now before it can hurt */
	request_channel.drop_ios ();

	Glib::Mutex::Lock lm (_run_lock);
	run_loop_thread = Thread::create (mem_fun (*this, &BaseUI::main_thread), true);
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

		handle_ui_requests ();
	}

	return true;
}
	
