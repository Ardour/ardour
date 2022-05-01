/*
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
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
#include <getopt.h>
#include <iostream>

#include <glibmm.h>

#include "pbd/abstract_ui.h"
#include "pbd/signals.h"

#include "common.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace SessionUtils;

static PBD::Signal0<void> static_signal;
static CrossThreadChannel xthread (true);

struct TestRequest : public BaseUI::BaseRequestObject {
public:
	TestRequest () {}
	~TestRequest () {}
};

class TestUI : public AbstractUI<TestRequest>, PBD::ScopedConnectionList
{
public:
	TestUI ()
		: AbstractUI<TestRequest> ("eventlooptest")
	{
		pthread_set_name ("test_ui_thread");
		_run_loop_thread = PBD::Thread::self ();
		set_event_loop_for_thread (this);
		SessionEvent::create_per_thread_pool ("test", 512);
	}

	~TestUI ()
	{
		cout << "TestUI::~TestUI\n";
		stop ();
	}

	void set_active (bool yn)
	{
		if (yn) {
			BaseUI::run ();

			Glib::RefPtr<Glib::TimeoutSource> periodic_timeout = Glib::TimeoutSource::create (1000); // milliseconds
			periodic_connection                                = periodic_timeout->connect (sigc::mem_fun (*this, &TestUI::periodic));
			periodic_timeout->attach (main_loop ()->get_context ());

			static_signal.connect (*this, MISSING_INVALIDATOR, boost::bind (&TestUI::static_signal_handler, this), this);

			xthread.set_receive_handler (sigc::mem_fun (this, &TestUI::static_xthread_handler));
			xthread.attach (main_loop ()->get_context ());

		} else {
			stop ();
		}
	}

	void stop ()
	{
		cout << "TestUI::stop\n";
		periodic_connection.disconnect ();
		BaseUI::quit ();
	}

	void do_request (TestRequest* req)
	{
		cout << "TestUI::do_request\n";
		if (req->type == CallSlot) {
			call_slot (MISSING_INVALIDATOR, req->the_slot);
		} else if (req->type == Quit) {
			stop ();
		}
	}

private:
	void static_signal_handler ()
	{
		cout << "TestUI::static_signal_handler\n";
	}

	bool static_xthread_handler (Glib::IOCondition ioc)
	{
		if (ioc & ~Glib::IO_IN) {
			cout << "TestUI::static_xthread_handler ~IO_IN: " << ioc << "\n";
			return false;
		}
		if (ioc & Glib::IO_IN) {
			cout << "TestUI::static_xthread_handler IO_IN\n";
			xthread.drain ();
		}
		return true;
	}

	bool periodic ()
	{
		cout << "TestUI::periodic\n";
		return true;
	}

	sigc::connection periodic_connection;
};

#include "pbd/abstract_ui.cc" // instantiate template

static void
usage ()
{
	// help2man compatible format (standard GNU help-text)
	printf (UTILNAME " - x-thread signal test tool.\n\n");
	printf ("Usage: " UTILNAME " [ OPTIONS ] \n\n");
	printf ("Options:\n\
  -h, --help                 display this help and exit\n\
  -V, --version              print version information and exit\n\
\n");
	::exit (EXIT_SUCCESS);
}

int
main (int argc, char* argv[])
{
	const char* optstring = "hV";

	const struct option longopts[] = {
		{ "help",    0, 0, 'h' },
		{ "version", 0, 0, 'V' },
	};

	int c = 0;
	while (EOF != (c = getopt_long (argc, argv,
	                                optstring, longopts, (int*)0))) {
		switch (c) {
			case 'V':
				printf ("ardour-utils version %s\n\n", VERSIONSTRING);
				printf ("Copyright (C) GPL 2022 Robin Gareus <robin@gareus.org>\n");
				exit (EXIT_SUCCESS);
				break;

			case 'h':
				usage ();
				break;

			default:
				cerr << "Error: unrecognized option. See --help for usage information.\n";
				::exit (EXIT_FAILURE);
				break;
		}
	}

	std::string snapshot_name;

	if (optind != argc) {
		cerr << "Error: Invalid parameter. See --help for usage information.\n";
		::exit (EXIT_FAILURE);
	}

	/* all systems go */

	SessionUtils::init ();

	TestUI* test_ui = new TestUI ();

	test_ui->set_active (true);

	Glib::usleep (2 * G_USEC_PER_SEC);
	static_signal (); /* EMIT SIGNAL */
	Glib::usleep (2 * G_USEC_PER_SEC);
	xthread.wakeup ();
	Glib::usleep (2 * G_USEC_PER_SEC);
	xthread.wakeup ();
	Glib::usleep (3 * G_USEC_PER_SEC);

	delete test_ui;

	SessionUtils::cleanup ();

	return 0;
}
