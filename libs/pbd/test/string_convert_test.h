#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class StringConvertTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (StringConvertTest);
	CPPUNIT_TEST (test_required_locales);
	CPPUNIT_TEST (test_int16_conversion);
	CPPUNIT_TEST (test_uint16_conversion);
	CPPUNIT_TEST (test_int32_conversion);
	CPPUNIT_TEST (test_uint32_conversion);
	CPPUNIT_TEST (test_int64_conversion);
	CPPUNIT_TEST (test_uint64_conversion);
	CPPUNIT_TEST (test_float_conversion);
	CPPUNIT_TEST (test_double_conversion);
	CPPUNIT_TEST (test_bool_conversion);
	CPPUNIT_TEST (test_convert_thread_safety);
	CPPUNIT_TEST_SUITE_END ();

public:
	void test_required_locales ();
	void test_int16_conversion ();
	void test_uint16_conversion ();
	void test_int32_conversion ();
	void test_uint32_conversion ();
	void test_int64_conversion ();
	void test_uint64_conversion ();
	void test_float_conversion ();
	void test_double_conversion ();
	void test_bool_conversion ();
	void test_convert_thread_safety ();
};
