/*
    Copyright (C) 2015 Tim Mayberry

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "test_ui.h"

#include <glibmm/threads.h>

#include "pbd/error.h"

#include "ardour/session_event.h"
#include "ardour/rc_configuration.h"

#include "pbd/abstract_ui.cc" // instantiate template

using namespace ARDOUR;

template class AbstractUI<TestUIRequest>;

TestUI::TestUI ()
	: AbstractUI<TestUIRequest> ("test_ui")
{

	pthread_set_name ("test_ui_thread");

	run_loop_thread = Glib::Threads::Thread::self ();

	set_event_loop_for_thread (this);

	SessionEvent::create_per_thread_pool ("test", 512);

	m_test_receiver.listen_to (PBD::error);
	m_test_receiver.listen_to (PBD::info);
	m_test_receiver.listen_to (PBD::fatal);
	m_test_receiver.listen_to (PBD::warning);

	/* We can't use VSTs here as we have a stub instead of the
	   required bits in gtk2_ardour.
	*/
	Config->set_use_lxvst (false);
}

TestUI::~TestUI ()
{
	m_test_receiver.hangup ();
}

void
TestUI::do_request (TestUIRequest* req)
{

}
