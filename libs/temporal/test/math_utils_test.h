#ifndef TEMPORAL_MATH_UTILS_TEST_H
#define TEMPORAL_MATH_UTILS_TEST_H

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class MathUtilsTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(MathUtilsTest);
	CPPUNIT_TEST(div_rem_in_range_test);
	CPPUNIT_TEST_SUITE_END();

public:
	void div_rem_in_range_test();

private:
	/**
	 * Tests the div_rem_in_range function with the specified test case and
	 * expected behavior.
	 */
	void div_rem_in_range_check(int numer, int denom, int expected_quotient,
	                            int expected_remainder);
};

#endif
