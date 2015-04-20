#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "glibmm/main.h"

#include "pbd/timing.h"

/**
 * The main point of this test is to the compare the different
 * ways of setting a timeout with glib and glibmm and the
 * PBD::Timers class and then to test them all again with
 * the maximum multimedia timer resolution(1ms) set with
 * timeBeginPeriod on Windows.
 *
 * The test demonstrates that when using Glibmm TimeoutSources
 * the frequency of the timers is different that using Glib based
 * timeouts. In Ardour that resulted in a noticable increase in
 * CPU Usage, but behaviour may vary.
 *
 * The other thing being tested is what effect adding two short
 * timeouts(<40ms) to a glib context has on the idle timeout on
 * Windows.
 *
 * Glib Timeout sources run at a higher priority than the idle
 * handler, so the more work performed in the timeout handlers
 * the less frequent the idle handler will run until doesn't get
 * scheduled at all. The consequence of this is blocking the UI.
 *
 * Similarily because timeout sources and UI updates/rendering
 * occur in the same context in Gtk the length of expose/draw
 * operations will affect the accuracy of the timeouts.
 */
class TimerTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (TimerTest);
	CPPUNIT_TEST (testGlibTimeoutSources);
	CPPUNIT_TEST (testGlibmmSignalTimeouts);
	CPPUNIT_TEST (testGlibmmTimeoutSources);
	CPPUNIT_TEST (testTimers);
	CPPUNIT_TEST (testTimersIdleFrequency);
	CPPUNIT_TEST (testTimersBlockIdle);
#ifdef PLATFORM_WINDOWS
	CPPUNIT_TEST (testGlibTimeoutSourcesHR);
	CPPUNIT_TEST (testGlibmmSignalTimeoutsHR);
	CPPUNIT_TEST (testGlibmmTimeoutSourcesHR);
	CPPUNIT_TEST (testTimersHR);
	CPPUNIT_TEST (testTimersIdleFrequencyHR);
	CPPUNIT_TEST (testTimersBlockIdleHR);
#endif
	CPPUNIT_TEST_SUITE_END ();

public:

	TimerTest ()
		: m_connect_idle(false)
		, m_block_idle(false)
	{ }

	void _testGlibTimeoutSources ();
	void _testGlibmmSignalTimeouts ();
	void _testGlibmmTimeoutSources ();
	void _testTimers ();
	void _testTimersIdleFrequency ();
	void _testTimersBlockIdle ();

	void testGlibTimeoutSources ();
	void testGlibmmSignalTimeouts ();
	void testGlibmmTimeoutSources ();
	void testTimers ();
	void testTimersIdleFrequency ();
	void testTimersBlockIdle ();

#ifdef PLATFORM_WINDOWS
	void testGlibTimeoutSourcesHR ();
	void testGlibmmSignalTimeoutsHR ();
	void testGlibmmTimeoutSourcesHR ();
	void testTimersHR ();
	void testTimersIdleFrequencyHR ();
	void testTimersBlockIdleHR ();
#endif

private:

	static guint64 second_timer_usecs ()
	{ return 1000000; }

	static guint64 fast_timer_usecs ()
	{ return 100000; }

	static guint64 rapid1_timer_usecs ()
	{ return 40000; }

	static guint64 rapid2_timer_usecs ()
	{ return 15000; }

	static guint64 second_timer_ms ()
	{ return second_timer_usecs () / 1000; }

	static guint64 fast_timer_ms ()
	{ return fast_timer_usecs () / 1000; }

	static guint64 rapid1_timer_ms ()
	{ return rapid1_timer_usecs () / 1000; }

	static guint64 rapid2_timer_ms ()
	{ return rapid2_timer_usecs () / 1000; }

	static guint64 test_length_ms ()
	{ return 2 * 1000; }

	std::string m_current_test_name;

	bool m_connect_idle;
	bool m_block_idle;

	bool on_idle_handler ();
	bool on_quit_handler ();

	void on_second_timeout ();
	void on_fast_timeout ();
	void on_rapid1_timeout ();
	void on_rapid2_timeout ();

	bool on_second_timeout_glibmm ();
	bool on_fast_timeout_glibmm ();
	bool on_rapid1_timeout_glibmm ();
	bool on_rapid2_timeout_glibmm ();

	static gboolean _second_timeout_handler (void*);
	static gboolean _fast_timeout_handler (void*);
	static gboolean _rapid1_timeout_handler (void*);
	static gboolean _rapid2_timeout_handler (void*);

	void start_timing ();
	void reset_timing ();

	void reset_timing_run_main ();

	static void
	simulate_load (const std::string& name, guint64 time_usecs);
	Glib::RefPtr<Glib::MainLoop> m_main;
	Glib::RefPtr<Glib::MainContext> m_context;

	void connect_idle_handler ();
	void connect_quit_timeout ();

	PBD::TimingData m_idle_timing_data;
	PBD::TimingData m_second_timing_data;
	PBD::TimingData m_fast_timing_data;
	PBD::TimingData m_rapid1_timing_data;
	PBD::TimingData m_rapid2_timing_data;
};
