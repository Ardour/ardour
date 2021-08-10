/*
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

#include <iostream>
#include <cstdlib>
#include <glibmm.h>


#include "pbd/debug.h"
#include "pbd/event_loop.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/pthread_utils.h"
#include "pbd/receiver.h"
#include "pbd/transmitter.h"

#include "ardour/audioengine.h"
#include "ardour/filename_extensions.h"
#include "ardour/types.h"

#include "common.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

static const char* localedir = LOCALEDIR;

class LogReceiver : public Receiver
{
protected:
	void receive (Transmitter::Channel chn, const char * str) {
		const char *prefix = "";
		switch (chn) {
			case Transmitter::Debug:
				/* ignore */
				return;
			case Transmitter::Info:
				/* ignore */
				return;
			case Transmitter::Warning:
				prefix = ": [WARNING]: ";
				break;
			case Transmitter::Error:
				prefix = ": [ERROR]: ";
				break;
			case Transmitter::Fatal:
				prefix = ": [FATAL]: ";
				break;
			case Transmitter::Throw:
				/* this isn't supposed to happen */
				abort ();
		}

		/* note: iostreams are already thread-safe: no external
			 lock required.
			 */

		std::cout << prefix << str << std::endl;

		if (chn == Transmitter::Fatal) {
			::exit (9);
		}
	}
};

LogReceiver log_receiver;

/* temporarily required due to some code design confusion (Feb 2014) */

#include "ardour/vst_types.h"

int vstfx_init (void*) { return 0; }
void vstfx_exit () {}
void vstfx_destroy_editor (VSTState*) {}

class MyEventLoop : public sigc::trackable, public EventLoop
{
	public:
		MyEventLoop (std::string const& name) : EventLoop (name) {
			run_loop_thread = Glib::Threads::Thread::self();
		}

		void call_slot (InvalidationRecord*, const boost::function<void()>& f) {
			if (Glib::Threads::Thread::self() == run_loop_thread) {
				f ();
			}
		}

		Glib::Threads::Mutex& slot_invalidation_mutex() { return request_buffer_map_lock; }

	private:
		Glib::Threads::Thread* run_loop_thread;
		Glib::Threads::Mutex   request_buffer_map_lock;
};

static MyEventLoop *event_loop;

void
SessionUtils::init (bool print_log)
{
	if (!ARDOUR::init (true, localedir)) {
		cerr << "Ardour failed to initialize\n" << endl;
		::exit (EXIT_FAILURE);
	}

	event_loop = new MyEventLoop ("util");
	EventLoop::set_event_loop_for_thread (event_loop);
	SessionEvent::create_per_thread_pool ("util", 512);

	if (print_log) {
		log_receiver.listen_to (warning);
		log_receiver.listen_to (error);
		log_receiver.listen_to (fatal);
	}
}

// TODO return NULL, rather than exit() ?!
static Session * _load_session (string dir, string state)
{
	AudioEngine* engine = AudioEngine::create ();

	if (!engine->set_backend ("None (Dummy)", "Unit-Test", "")) {
		std::cerr << "Cannot create Audio/MIDI engine\n";
		::exit (EXIT_FAILURE);
	}

	engine->set_input_channels (256);
	engine->set_output_channels (256);

	float sr;
	SampleFormat sf;
	std::string v;

	std::string s = Glib::build_filename (dir, state + statefile_suffix);

	if (!Glib::file_test (s, Glib::FILE_TEST_EXISTS | Glib::FILE_TEST_IS_REGULAR)) {
		std::cerr << "Cannot read session '"<< s << "'\n";
		return 0;
	}

	if (Session::get_info_from_path (s, sr, sf, v) == 0) {
		if (engine->set_sample_rate (sr)) {
			std::cerr << "Cannot set session's samplerate.\n";
			return 0;
		}
	} else {
		std::cerr << "Cannot get samplerate from session.\n";
		return 0;
	}

	if (engine->start () != 0) {
		std::cerr << "Cannot start Audio/MIDI engine\n";
		return 0;
	}

	Session* session = new Session (*engine, dir, state);
	engine->set_session (session);
	return session;
}

Session *
SessionUtils::load_session (string dir, string state, bool exit_at_failure)
{
	Session* s = 0;
	try {
		s = _load_session (dir, state);
	} catch (failed_constructor& e) {
		cerr << "failed_constructor: " << e.what() << "\n";
		::exit (EXIT_FAILURE);
	} catch (AudioEngine::PortRegistrationFailure& e) {
		cerr << "PortRegistrationFailure: " << e.what() << "\n";
		::exit (EXIT_FAILURE);
	} catch (exception& e) {
		cerr << "exception: " << e.what() << "\n";
		::exit (EXIT_FAILURE);
	} catch (...) {
		cerr << "unknown exception.\n";
		::exit (EXIT_FAILURE);
	}
	if (!s && exit_at_failure) {
		::exit (EXIT_FAILURE);
	}
	return s;
}

Session *
SessionUtils::create_session (string dir, string state, float sample_rate)
{
	AudioEngine* engine = AudioEngine::create ();

	if (!engine->set_backend ("None (Dummy)", "Unit-Test", "")) {
		std::cerr << "Cannot create Audio/MIDI engine\n";
		::exit (EXIT_FAILURE);
	}

	engine->set_input_channels (256);
	engine->set_output_channels (256);

	if (engine->set_sample_rate (sample_rate)) {
		std::cerr << "Cannot set session's samplerate.\n";
		return 0;
	}

	if (engine->start () != 0) {
		std::cerr << "Cannot start Audio/MIDI engine\n";
		return 0;
	}

	std::string s = Glib::build_filename (dir, state + statefile_suffix);

	if (Glib::file_test (dir, Glib::FILE_TEST_EXISTS)) {
		std::cerr << "Session folder already exists '"<< dir << "'\n";
	}
	if (Glib::file_test (s, Glib::FILE_TEST_EXISTS)) {
		std::cerr << "Session file exists '"<< s << "'\n";
		return 0;
	}

	Session* session = new Session (*engine, dir, state);
	engine->set_session (session);
	return session;
}

void
SessionUtils::unload_session (Session *s)
{
	delete s;
	AudioEngine::instance()->stop ();
	AudioEngine::destroy ();
}

void
SessionUtils::cleanup ()
{
	ARDOUR::cleanup ();
	delete event_loop;
	pthread_cancel_all ();
}
