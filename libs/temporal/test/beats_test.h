#ifndef TEMPORAL_BEATS_TEST_H
#define TEMPORAL_BEATS_TEST_H

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "temporal/beats.h"

class BeatsTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(BeatsTest);
	CPPUNIT_TEST(basic_test);
	CPPUNIT_TEST(limits_test);
	CPPUNIT_TEST(rounding_test);
	CPPUNIT_TEST(logical_op_test);
	CPPUNIT_TEST(add_test);
	CPPUNIT_TEST(subtract_test);
	CPPUNIT_TEST(multiply_test);
	CPPUNIT_TEST(divide_test);
	CPPUNIT_TEST(serialization_test);
	CPPUNIT_TEST(misc_test);
	CPPUNIT_TEST_SUITE_END();

public:
	void basic_test();
	void limits_test();
	void rounding_test();
	void logical_op_test();
	void add_test();
	void subtract_test();
	void multiply_test();
	void divide_test();
	void serialization_test();
	void misc_test();

private:
	static const int32_t PPQN = Temporal::Beats::PPQN;
	static const int32_t MAX_INT32 = std::numeric_limits<int32_t>::max();
	static const int32_t LOWEST_INT32 = std::numeric_limits<int32_t>::lowest();

	/**
	 * Performs basic checks on a Temporal::Beats object, including specific
	 * tests based on the arguments and general tests that all Beats objects
	 * should pass.
	 */
	void basic_beats_check(const Temporal::Beats& beats, int32_t expected_beats,
	                       int32_t expected_ticks);

	/**
	 * Tests the behavior of comparison operators on greater and smaller, where
	 * greater is assumed to be strictly greater than smaller.
	 */
	void beats_comparison_check(const Temporal::Beats& greater,
	                            const Temporal::Beats& smaller);

	/**
	 * Tests serialization by serializing beats to an ostringstream, checking
	 * that the serialized string matches expected_str, then deserializing a new
	 * Beats object and checking that it equals the original.
	 */
	void beats_serialize_check(const Temporal::Beats& beats,
	                           const std::string& expected_str);

	/**
	 * Tests the + and += operators by adding to_add to beats, and checking that
	 * the result has the expected numbers of beats and ticks.
	 */
	template <typename T>
	void add_check(const Temporal::Beats& beats, T to_add,
	               int32_t expected_beats, int32_t expected_ticks)
	{
		// Check the + operator.

		Temporal::Beats result_beats = beats + to_add;
		basic_beats_check(result_beats, expected_beats, expected_ticks);

		// Check the += operator.

		result_beats = beats;
		result_beats += to_add;
		basic_beats_check(result_beats, expected_beats, expected_ticks);
	}

	/**
	 * Tests the - and -= operators by subtracting to_subtract from beats, and
	 * checking that the result has the expected numbers of beats and ticks.
	 */
	template <typename T>
	void subtract_check(const Temporal::Beats& beats, T to_subtract,
	                    int32_t expected_beats, int32_t expected_ticks)
	{
		// Check the - operator.

		Temporal::Beats result_beats = beats - to_subtract;
		basic_beats_check(result_beats, expected_beats, expected_ticks);

		// Check the -= operator.

		result_beats = beats;
		result_beats -= to_subtract;
		basic_beats_check(result_beats, expected_beats, expected_ticks);
	}
	
	/**
	 * Tests the * operator by multiplying beats by to_multiply, and checking
	 * that the result has the expected numbers of beats and ticks.
	 */
	template <typename T>
	void multiply_check(const Temporal::Beats& beats, T to_multiply,
	                    int32_t expected_beats, int32_t expected_ticks)
	{
		Temporal::Beats result_beats = beats * to_multiply;
		basic_beats_check(result_beats, expected_beats, expected_ticks);
	}

	/**
	 * Tests the / operator by dividing beats by to_divide, and checking that
	 * the result has the expected numbers of beats and ticks.
	 */
	template <typename T>
	void divide_check(const Temporal::Beats& beats, T to_divide,
	                  int32_t expected_beats, int32_t expected_ticks)
	{
		Temporal::Beats result_beats = beats / to_divide;
		basic_beats_check(result_beats, expected_beats, expected_ticks);
	}
};

#endif
