/*
    Copyright (C) 2000-2009 Paul Davis 

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

#ifndef __pbd_base_ui_h__
#define __pbd_base_ui_h__

#include <string>
#include <stdint.h>

#include <sigc++/slot.h>
#include <sigc++/trackable.h>

#include <glibmm/threads.h>
#include <glibmm/main.h>

#include "pbd/crossthread.h"
#include "pbd/event_loop.h"

/** A BaseUI is an abstraction designed to be used with any "user
 * interface" (not necessarily graphical) that needs to wait on
 * events/requests and dispatch/process them as they arrive.
 *
 * This implementation starts up a thread that runs a Glib main loop
 * to wait on events/requests etc. 
 */


class BaseUI : public sigc::trackable, public PBD::EventLoop
{
  public:
	BaseUI (const std::string& name);
	virtual ~BaseUI();

	BaseUI* base_instance() { return base_ui_instance; }

	Glib::RefPtr<Glib::MainLoop> main_loop() const { return _main_loop; }
        Glib::Threads::Thread* event_loop_thread() const { return run_loop_thread; }
        bool caller_is_self () const { return Glib::Threads::Thread::self() == run_loop_thread; }

	std::string name() const { return _name; }

	bool ok() const { return _ok; }
	
	static RequestType new_request_type();
	static RequestType CallSlot;
	static RequestType Quit;

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
        Glib::Threads::Thread*       run_loop_thread;
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

	/** Called when there input ready on the request_channel
	 */
	bool request_handler (Glib::IOCondition);

	void signal_new_request ();
	void attach_request_source (Glib::RefPtr<Glib::MainContext> context);

	/** Derived UI objects must implement this method,
	 * which will be called whenever there are requests
	 * to be dealt with.
	 */
	virtual void handle_ui_requests () = 0;

  private:
	std::string _name; 
	BaseUI* base_ui_instance;

	CrossThreadChannel request_channel;
	
	static uint64_t rt_bit;

	int setup_request_pipe ();
	void main_thread ();
};

#endif /* __pbd_base_ui_h__ */
