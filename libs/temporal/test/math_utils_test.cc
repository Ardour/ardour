#include "temporal/math_utils.h"

#include "math_utils_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(MathUtilsTest);

void MathUtilsTest::div_rem_in_range_test()
{
	div_rem_in_range_check(7, 5, 1, 2);
	div_rem_in_range_check(-7, -5, 2, 3);
	div_rem_in_range_check(-7, 5, -2, 3);
	div_rem_in_range_check(7, -5, -1, 2);
	div_rem_in_range_check(5, 7, 0, 5);
	div_rem_in_range_check(-5, -7, 1, 2);
	div_rem_in_range_check(-5, 7, -1, 2);
	div_rem_in_range_check(5, -7, 0, 5);
	div_rem_in_range_check(0, 2, 0, 0);
	div_rem_in_range_check(0, -2, 0, 0);
}

void MathUtilsTest::div_rem_in_range_check(int numer, int denom,
                                           int expected_quotient,
                                           int expected_remainder)
{
	// Test case where a remainder pointer is provided.
	int remainder;
	int quotient = Temporal::div_rem_in_range(numer, denom, &remainder);
	CPPUNIT_ASSERT_EQUAL(expected_quotient, quotient);
	CPPUNIT_ASSERT_EQUAL(expected_remainder, remainder);

	// Test case where no remainder pointer is provided.
	CPPUNIT_ASSERT_EQUAL(expected_quotient,
	                     Temporal::div_rem_in_range(numer, denom, (int*) 0));
}
