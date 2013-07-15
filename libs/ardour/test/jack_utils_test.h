
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class JackUtilsTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (JackUtilsTest);
	CPPUNIT_TEST (test_driver_names);
	CPPUNIT_TEST (test_device_names);
	CPPUNIT_TEST (test_samplerates);
	CPPUNIT_TEST (test_period_sizes);
	CPPUNIT_TEST (test_dither_modes);
	CPPUNIT_TEST (test_connect_server);
	CPPUNIT_TEST (test_set_jack_path_env);
	CPPUNIT_TEST (test_server_paths);
	CPPUNIT_TEST (test_config);
	CPPUNIT_TEST (test_command_line);
	CPPUNIT_TEST (test_start_server);
	CPPUNIT_TEST_SUITE_END ();

public:
	void test_driver_names ();
	void test_device_names ();
	void test_samplerates ();
	void test_period_sizes ();
	void test_dither_modes ();
	void test_connect_server ();
	void test_set_jack_path_env ();
	void test_server_paths ();
	void test_config ();
	void test_command_line ();
	void test_start_server ();
};
