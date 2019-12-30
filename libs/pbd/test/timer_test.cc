#include "timer_test.h"

#include <iostream>
#include <sstream>
#include <algorithm>

#include "pbd/timer.h"

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#endif

#ifndef G_SOURCE_FUNC
#define G_SOURCE_FUNC(f) ((GSourceFunc) (void (*)(void)) (f))
#endif

CPPUNIT_TEST_SUITE_REGISTRATION (TimerTest);

using namespace std;

#ifdef PLATFORM_WINDOWS
UINT&
min_timer_resolution ()
{
	static UINT min_res_ms = 0;
	return min_res_ms;
}

bool
set_min_timer_resolution ()
{
	TIMECAPS caps;

	if (timeGetDevCaps(&caps, sizeof(TIMECAPS)) != TIMERR_NOERROR) {
		cerr << "Could not get timer device capabilities..." << endl;
	} else {
		if (timeBeginPeriod(caps.wPeriodMin) != TIMERR_NOERROR) {
			cerr << "Could not set minimum timer resolution to: " << caps.wPeriodMin << "ms" << endl;
			return false;
		}
		else {
			cerr << "Multimedia timer resolution set to: " << caps.wPeriodMin << "ms" << endl;
			min_timer_resolution() = caps.wPeriodMin;
			return true;
		}
	}
	return false;
}

bool
reset_timer_resolution ()
{
	if (min_timer_resolution()) {
		if (timeEndPeriod(min_timer_resolution()) != TIMERR_NOERROR) {
			cerr << "Could not reset timer resolution" << endl;
			return false;
		} else {
			cerr << "Multimedia timer resolution reset" << endl;
			return true;
		}
	}
	return true;
}

#endif

void
TimerTest::simulate_load (const string& name, guint64 load_usecs)
{
	PBD::Timing timing;
	std::ostringstream oss;
	oss << name << " Load.";

	guint64 i = 0;
	do {
		timing.update ();

		// totally arbitrary
		if (i % 10000 == 0) {
			oss << ".";
		}

		++i;
	} while (timing.elapsed () < load_usecs);

	oss << "Expected = " << load_usecs;
	oss << ", Elapsed = " << timing.elapsed ();
	oss << endl;
	//cerr << oss.str();
}

void
TimerTest::on_second_timeout ()
{
	cerr << endl;
	cerr << "Timing Summary: " << m_current_test_name << endl;

	if (m_idle_timing_data.size()) {
		cerr << "Idle Timing: " << m_idle_timing_data.summary();
	}
	if (m_fast_timing_data.size()) {
		cerr << "Fast Timing: " << m_fast_timing_data.summary();
	}
	if (m_rapid1_timing_data.size()) {
		cerr << "Rapid1 Timing: " << m_rapid1_timing_data.summary();
	}
	if (m_rapid2_timing_data.size()) {
		cerr << "Rapid2 Timing: " << m_rapid2_timing_data.summary();
	}
	reset_timing ();
}

bool
TimerTest::on_second_timeout_glibmm ()
{
	TimerTest::on_second_timeout ();
	return true;
}

void
TimerTest::on_fast_timeout ()
{
	m_fast_timing_data.add_interval ();
	if (m_block_idle) {
		// do nothing, handled in rapid timers
	} else {
		simulate_load ("Rapid1", 4000);
	}
}

bool
TimerTest::on_fast_timeout_glibmm ()
{
	on_fast_timeout ();
	return true;
}

void
TimerTest::on_rapid1_timeout ()
{
	m_rapid1_timing_data.add_interval ();
	if (m_block_idle) {
		simulate_load ("Rapid1", rapid1_timer_usecs () * 0.5);
	} else {
		simulate_load ("Rapid1", 2000);
	}
}

bool
TimerTest::on_rapid1_timeout_glibmm ()
{
	on_rapid1_timeout ();
	return true;
}

void
TimerTest::on_rapid2_timeout ()
{
	m_rapid2_timing_data.add_interval ();
	if (m_block_idle) {
		simulate_load ("Rapid2", rapid2_timer_usecs () * 0.5);
	} else {
		simulate_load ("Rapid2", 2000);
	}
}

bool
TimerTest::on_rapid2_timeout_glibmm ()
{
	on_rapid2_timeout ();
	return true;
}

bool
TimerTest::on_idle_handler ()
{
	m_idle_timing_data.add_interval ();
	if (m_block_idle) {
		simulate_load ("Idle", rapid2_timer_usecs ());
	}
	return true;
}

bool
TimerTest::on_quit_handler ()
{
	cerr << "Quit Handler" << endl;
	m_main->quit ();
	return false;
}

void
TimerTest::reset_timing ()
{
	m_idle_timing_data.reset ();
	m_fast_timing_data.reset ();
	m_rapid1_timing_data.reset ();
	m_rapid2_timing_data.reset ();
}

void
TimerTest::start_timing ()
{
	m_idle_timing_data.start_timing ();
	m_fast_timing_data.start_timing ();
	m_rapid1_timing_data.start_timing ();
	m_rapid2_timing_data.start_timing ();
}

gboolean
TimerTest::_second_timeout_handler (void *data)
{
	TimerTest *const tt = static_cast<TimerTest*>(data);
	tt->on_second_timeout ();
	return TRUE;
}

gboolean
TimerTest::_fast_timeout_handler (void *data)
{
	TimerTest *const tt = static_cast<TimerTest*>(data);
	tt->on_fast_timeout ();
	return TRUE;
}

gboolean
TimerTest::_rapid1_timeout_handler (void *data)
{
	TimerTest *const tt = static_cast<TimerTest*>(data);
	tt->on_rapid1_timeout ();
	return TRUE;
}

gboolean
TimerTest::_rapid2_timeout_handler (void *data)
{
	TimerTest *const tt = static_cast<TimerTest*>(data);
	tt->on_rapid2_timeout ();
	return TRUE;
}

void
TimerTest::reset_timing_run_main ()
{
	reset_timing ();
	start_timing ();

	connect_quit_timeout ();

	m_main = Glib::MainLoop::create (m_context);
	m_main->run ();
}

void
TimerTest::testGlibTimeoutSources ()
{
	m_current_test_name = "testGlibTimeoutSources";
	_testGlibTimeoutSources ();
}

void
TimerTest::_testGlibTimeoutSources ()
{
	m_context = Glib::MainContext::create ();

	GSource * second_timeout_source = g_timeout_source_new (second_timer_ms ());

	g_source_set_callback (second_timeout_source , &TimerTest::_second_timeout_handler, this, NULL);

	g_source_attach (second_timeout_source, m_context->gobj());

	if (m_connect_idle) {
		connect_idle_handler ();
		reset_timing_run_main ();
	}

	GSource * fast_timeout_source = g_timeout_source_new (fast_timer_ms ());

	g_source_set_callback (fast_timeout_source , &TimerTest::_fast_timeout_handler, this, NULL);

	g_source_attach (fast_timeout_source, m_context->gobj());

	// now run with fast timeout
	reset_timing_run_main ();

	GSource * rapid1_timeout_source = g_timeout_source_new (rapid1_timer_ms ());

	g_source_set_callback (rapid1_timeout_source , &TimerTest::_rapid1_timeout_handler, this, NULL);

	g_source_attach (rapid1_timeout_source, m_context->gobj());

	// now run with fast and rapid1 timeouts
	reset_timing_run_main ();

	GSource * rapid2_timeout_source = g_timeout_source_new (rapid2_timer_ms ());

	g_source_set_callback (rapid2_timeout_source , &TimerTest::_rapid2_timeout_handler, this, NULL);

	g_source_attach (rapid2_timeout_source, m_context->gobj());

	// now run with fast, rapid1 and rapid2 timeouts
	reset_timing_run_main ();

	// cleanup
	g_source_destroy (second_timeout_source);
	g_source_unref (second_timeout_source);

	g_source_destroy (fast_timeout_source);
	g_source_unref (fast_timeout_source);

	g_source_destroy (rapid1_timeout_source);
	g_source_unref (rapid1_timeout_source);

	g_source_destroy (rapid2_timeout_source);
	g_source_unref (rapid2_timeout_source);
}

void
TimerTest::testGlibmmSignalTimeouts ()
{
	m_current_test_name = "testGlibmmSignalTimeouts";
	_testGlibmmSignalTimeouts ();
}

void
TimerTest::_testGlibmmSignalTimeouts ()
{
	m_context = Glib::MainContext::get_default ();

	Glib::signal_timeout().connect(sigc::mem_fun(*this, &TimerTest::on_second_timeout_glibmm), second_timer_ms());

	if (m_connect_idle) {
		connect_idle_handler ();
		reset_timing_run_main ();
	}

	Glib::signal_timeout().connect(sigc::mem_fun(*this, &TimerTest::on_fast_timeout_glibmm), fast_timer_ms());

	reset_timing_run_main ();

	Glib::signal_timeout().connect(sigc::mem_fun(*this, &TimerTest::on_rapid1_timeout_glibmm), rapid1_timer_ms());

	reset_timing_run_main ();

	Glib::signal_timeout().connect(sigc::mem_fun(*this, &TimerTest::on_rapid2_timeout_glibmm), rapid2_timer_ms());

	reset_timing_run_main ();
}

void
TimerTest::testGlibmmTimeoutSources ()
{
	m_current_test_name = "testGlibmmTimeoutSources";
	_testGlibmmTimeoutSources ();
}

void
TimerTest::_testGlibmmTimeoutSources ()
{
	m_context = Glib::MainContext::create ();

	const Glib::RefPtr<Glib::TimeoutSource> second_source = Glib::TimeoutSource::create(second_timer_ms());
	second_source->connect(sigc::mem_fun(*this, &TimerTest::on_second_timeout_glibmm));

	second_source->attach(m_context);

	if (m_connect_idle) {
		connect_idle_handler ();
		reset_timing_run_main ();
	}

	const Glib::RefPtr<Glib::TimeoutSource> fast_source = Glib::TimeoutSource::create(fast_timer_ms());
	fast_source->connect(sigc::mem_fun(*this, &TimerTest::on_fast_timeout_glibmm));

	fast_source->attach(m_context);

	reset_timing_run_main ();

	const Glib::RefPtr<Glib::TimeoutSource> rapid1_source = Glib::TimeoutSource::create(rapid1_timer_ms());
	sigc::connection rapid1_connection = rapid1_source->connect(sigc::mem_fun(*this, &TimerTest::on_rapid1_timeout_glibmm));

	rapid1_source->attach(m_context);

	reset_timing_run_main ();

	const Glib::RefPtr<Glib::TimeoutSource> rapid2_source = Glib::TimeoutSource::create(rapid2_timer_ms());
	sigc::connection rapid2_connection = rapid2_source->connect(sigc::mem_fun(*this, &TimerTest::on_rapid2_timeout_glibmm));

	rapid2_source->attach(m_context);

	reset_timing_run_main ();
}

void
TimerTest::connect_idle_handler ()
{
	const Glib::RefPtr<Glib::IdleSource> idle_source = Glib::IdleSource::create();
	idle_source->connect(sigc::mem_fun(*this, &TimerTest::on_idle_handler));

	idle_source->attach(m_context);
}

void
TimerTest::connect_quit_timeout ()
{
	const Glib::RefPtr<Glib::TimeoutSource> quit_source = Glib::TimeoutSource::create(test_length_ms());
	quit_source->connect(sigc::mem_fun(*this, &TimerTest::on_quit_handler));

	quit_source->attach(m_context);
}

void
TimerTest::testTimers ()
{
	m_current_test_name = "testTimers";
	_testTimers ();
}

void
TimerTest::_testTimers ()
{
	m_context = Glib::MainContext::create ();

	PBD::StandardTimer second_timer (second_timer_ms (), m_context);
	sigc::connection second_connection = second_timer.connect (sigc::mem_fun (this, &TimerTest::on_second_timeout));

	if (m_connect_idle) {
		connect_idle_handler ();
		// let the idle handler run as fast as it can
		reset_timing_run_main();
	}

	PBD::StandardTimer fast_timer (fast_timer_ms (), m_context);
	sigc::connection fast_connection = fast_timer.connect (sigc::mem_fun (this, &TimerTest::on_fast_timeout));

	reset_timing_run_main();

	PBD::StandardTimer rapid1_timer (rapid1_timer_ms (), m_context);
	sigc::connection rapid1_connection = rapid1_timer.connect (sigc::mem_fun (this, &TimerTest::on_rapid1_timeout));

	reset_timing_run_main();

	PBD::StandardTimer rapid2_timer (rapid2_timer_ms (), m_context);
	sigc::connection rapid2_connection = rapid2_timer.connect (sigc::mem_fun (this, &TimerTest::on_rapid2_timeout));

	reset_timing_run_main();
}

void
TimerTest::testTimersIdleFrequency ()
{
	m_current_test_name = "testTimersIdleFrequency";
	_testTimersIdleFrequency ();
}

void
TimerTest::_testTimersIdleFrequency ()
{
	m_block_idle = false;
	m_connect_idle = true;

	_testTimers ();

	m_block_idle = false;
	m_connect_idle = false;
}

void
TimerTest::testTimersBlockIdle ()
{
	m_current_test_name = "testTimersBlockIdle";
	_testTimersBlockIdle ();
}

void
TimerTest::_testTimersBlockIdle ()
{
	m_block_idle = true;
	m_connect_idle = true;

	_testTimers ();

	m_block_idle = false;
	m_connect_idle = false;
}

#ifdef PLATFORM_WINDOWS
void
TimerTest::testGlibTimeoutSourcesHR ()
{
	CPPUNIT_ASSERT(set_min_timer_resolution());

	m_current_test_name = "testGlibTimeoutSourcesHR";
	_testGlibTimeoutSources ();

	CPPUNIT_ASSERT(reset_timer_resolution());
}

void
TimerTest::testGlibmmSignalTimeoutsHR ()
{
	CPPUNIT_ASSERT(set_min_timer_resolution());

	m_current_test_name = "testGlibmmSignalTimeoutsHR";
	_testGlibmmSignalTimeouts ();

	CPPUNIT_ASSERT(reset_timer_resolution());
}

void
TimerTest::testGlibmmTimeoutSourcesHR ()
{
	CPPUNIT_ASSERT(set_min_timer_resolution());

	m_current_test_name = "testGlibmmTimeoutSourcesHR";
	_testGlibmmTimeoutSources ();

	CPPUNIT_ASSERT(reset_timer_resolution());
}

void
TimerTest::testTimersHR ()
{
	CPPUNIT_ASSERT(set_min_timer_resolution());

	m_current_test_name = "testTimersHR";
	_testTimers ();

	CPPUNIT_ASSERT(reset_timer_resolution());
}

void
TimerTest::testTimersIdleFrequencyHR ()
{
	CPPUNIT_ASSERT(set_min_timer_resolution());

	m_current_test_name = "testTimersIdleFrequencyHR";
	_testTimersIdleFrequency ();

	CPPUNIT_ASSERT(reset_timer_resolution());
}

void
TimerTest::testTimersBlockIdleHR ()
{
	CPPUNIT_ASSERT(set_min_timer_resolution());

	m_current_test_name = "testTimersIdleFrequencyHR";
	_testTimersBlockIdle ();

	CPPUNIT_ASSERT(reset_timer_resolution());
}

#endif
