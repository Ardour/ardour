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

#ifndef __pbd_base_ui_h__
#define __pbd_base_ui_h__

#include <string>
#include <stdint.h>

#include <sigc++/slot.h>
#include <sigc++/trackable.h>

#include <glibmm/thread.h>
#include <glibmm/main.h>

#include "pbd/crossthread.h"

class BaseUI : virtual public sigc::trackable {
  public:
	BaseUI (const std::string& name);
	virtual ~BaseUI();

	BaseUI* base_instance() { return base_ui_instance; }

	Glib::RefPtr<Glib::MainLoop> main_loop() const { return _main_loop; }
	Glib::Thread* event_loop_thread() const { return run_loop_thread; }
	bool caller_is_self () const { return Glib::Thread::self() == run_loop_thread; }

	std::string name() const { return _name; }

	bool ok() const { return _ok; }

	enum RequestType {
		range_guarantee = ~0
	};

	struct BaseRequestObject {
	    RequestType type;
	    sigc::slot<void> the_slot;
	};

	static RequestType new_request_type();
	static RequestType CallSlot;

	void run ();
	void quit ();

	virtual void call_slot (sigc::slot<void> theSlot) = 0;

  protected:
	CrossThreadChannel request_channel;
	bool _ok; 

	Glib::RefPtr<Glib::MainLoop> _main_loop;
	Glib::Thread*                 run_loop_thread;

	virtual void thread_init () {};
	bool request_handler (Glib::IOCondition);

	virtual void handle_ui_requests () = 0;

  private:
	std::string _name; 
	BaseUI* base_ui_instance;
	
	static uint64_t rt_bit;

	int setup_request_pipe ();
	void main_thread ();
};

#endif /* __pbd_base_ui_h__ */
