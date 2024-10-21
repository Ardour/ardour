/*
 * Copyright (C) 2000-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#pragma once

#include <string>
#include <stdint.h>

#include <sigc++/slot.h>
#include <sigc++/trackable.h>

#include <glibmm/threads.h>
#include <glibmm/main.h>

#include "pbd/libpbd_visibility.h"
#include "pbd/crossthread.h"
#include "pbd/event_loop.h"
#include "pbd/pthread_utils.h"

/** A BaseUI is an abstraction designed to be used with any "user
 * interface" (not necessarily graphical) that needs to wait on
 * events/requests and dispatch/process them as they arrive.
 *
 * This implementation starts up a thread that runs a Glib main loop
 * to wait on events/requests etc.
 */


class LIBPBD_API BaseUI : public sigc::trackable, public PBD::EventLoop
{
  public:
	BaseUI (const std::string& name);
	virtual ~BaseUI();

	BaseUI* base_instance() { return base_ui_instance; }

	Glib::RefPtr<Glib::MainLoop> main_loop() const { return _main_loop; }
	bool caller_is_self () const { return _run_loop_thread ? _run_loop_thread->caller_is_self () : true; }

	bool ok() const { return _ok; }

	static RequestType new_request_type();
	static RequestType CallSlot;
	static RequestType Quit;

	static void set_thread_priority (int p) {
		_thread_priority = p;
	}

	/** start up a thread to run the main loop
	 */
	void run ();

	/** stop the thread running the main loop (and block
	 *   until it exits)
	 */
	void quit ();

  protected:
	bool _ok;

	Glib::RefPtr<Glib::MainLoop> _main_loop;
	Glib::RefPtr<Glib::MainContext> m_context;
	PBD::Thread*                _run_loop_thread;
	Glib::Threads::Mutex        _run_lock;
	Glib::Threads::Cond         _running;

	/* this signals _running from within the event loop,
	   from an idle callback
	*/

	bool signal_running ();

	/** Derived UI objects can implement thread_init()
	 * which will be called by the event loop thread
	 * immediately before it enters the event loop.
	 */

	virtual void thread_init () {};

	int set_thread_priority () const;

	/** Called when there input ready on the request_channel
	 */
	bool request_handler (Glib::IOCondition);

	void signal_new_request ();
	void attach_request_source ();

	virtual void maybe_install_precall_handler (Glib::RefPtr<Glib::MainContext>) {}

	/** Derived UI objects must implement this method,
	 * which will be called whenever there are requests
	 * to be dealt with.
	 */
	virtual void handle_ui_requests () = 0;

  private:
	BaseUI* base_ui_instance;

	CrossThreadChannel request_channel;

	static uint64_t rt_bit;
	static int _thread_priority;

	int setup_request_pipe ();
	void main_thread ();
};

