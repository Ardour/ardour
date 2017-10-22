#include <limits>

#include "beats_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(BeatsTest);

using Temporal::Beats;

void BeatsTest::basic_test()
{
	// Test default constructor.
	Beats beats;
	basic_beats_check(beats, 0, 0);

	beats = Beats::beats(100);
	basic_beats_check(beats, 100, 0);
	
	beats = Beats::beats(-100);
	basic_beats_check(beats, -100, 0);

	beats = Beats::beats(0);
	basic_beats_check(beats, 0, 0);

	beats = Beats::ticks(PPQN - 1);
	basic_beats_check(beats, 0, PPQN - 1);
	
	beats = Beats::ticks(-1);
	basic_beats_check(beats, -1, PPQN - 1);

	beats = Beats::ticks(0);
	basic_beats_check(beats, 0, 0);

	beats = Beats(100, PPQN - 1);
	basic_beats_check(beats, 100, PPQN - 1);
	
	beats = Beats(100, PPQN + 1);
	basic_beats_check(beats, 101, 1);
	
	beats = Beats(-100, PPQN - 1);
	basic_beats_check(beats, -100, PPQN - 1);
	
	beats = Beats(-100, PPQN + 1);
	basic_beats_check(beats, -99, 1);
	
	beats = Beats(10, -1);
	basic_beats_check(beats, 9, PPQN - 1);
	
	beats = Beats(10, -(PPQN + 1));
	basic_beats_check(beats, 8, PPQN - 1);
	
	beats = Beats(-10, -1);
	basic_beats_check(beats, -11, PPQN - 1);
	
	beats = Beats(-10, -(PPQN + 1));
	basic_beats_check(beats, -12, PPQN - 1);
	
	beats = Beats(10.5);
	basic_beats_check(beats, 10, PPQN / 2);

	beats = Beats(-10.5);
	basic_beats_check(beats, -11, PPQN / 2);

	beats = Beats(0.0);
	basic_beats_check(beats, 0, 0);

	// Test cases where the number of beats does not fit in an int32_t.

	beats = Beats(MAX_INT32, PPQN + 1);
	basic_beats_check(beats, MAX_INT32, PPQN + 1);

	beats = Beats(MAX_INT32, PPQN * 2 + 1);
	basic_beats_check(beats, MAX_INT32, PPQN * 2 + 1);

	beats = Beats(LOWEST_INT32, -1);
	basic_beats_check(beats, LOWEST_INT32, -1);

	beats = Beats(LOWEST_INT32, -(PPQN + 1));
	basic_beats_check(beats, LOWEST_INT32, -(PPQN + 1));
}

void BeatsTest::limits_test()
{
	basic_beats_check(std::numeric_limits<Beats>::max(), MAX_INT32, MAX_INT32);
	
	basic_beats_check(std::numeric_limits<Beats>::lowest(),  LOWEST_INT32,
	                  LOWEST_INT32);
}

void BeatsTest::rounding_test()
{
	Beats beats = Beats(4.6);
	basic_beats_check(beats.round_to_beat(), 5, 0);
	basic_beats_check(beats.round_up_to_beat(), 5, 0);
	basic_beats_check(beats.round_down_to_beat(), 4, 0);
	basic_beats_check(beats.snap_to(Beats::beats(3)), 6, 0);
	basic_beats_check(beats.snap_to(Beats::beats(-3)), 6, 0);

	beats = -4.6;
	basic_beats_check(beats.round_to_beat(), -5, 0);
	basic_beats_check(beats.round_up_to_beat(), -4, 0);
	basic_beats_check(beats.round_down_to_beat(), -5, 0);
	basic_beats_check(beats.snap_to(Beats::beats(3)), -3, 0);
	basic_beats_check(beats.snap_to(Beats::beats(-3)), -3, 0);

	// Test rounding when we are already exactly on a beat.
	beats = Beats::beats(6);
	//basic_beats_check(beats.round_to_beat(), 6, 0);
	basic_beats_check(beats.round_up_to_beat(), 6, 0);
	//basic_beats_check(beats.round_down_to_beat(), 6, 0);
	//basic_beats_check(beats.snap_to(Beats::beats(3)), 6, 0);
	//basic_beats_check(beats.snap_to(Beats::beats(-3)), 6, 0);

	// Test cases where the number of beats does not fit in an int32_t.

	beats = Beats(MAX_INT32, PPQN + 1 + PPQN / 2);
	basic_beats_check(beats.round_to_beat(), MAX_INT32, PPQN * 2);
	basic_beats_check(beats.round_up_to_beat(), MAX_INT32, PPQN * 2);
	basic_beats_check(beats.round_down_to_beat(), MAX_INT32, PPQN);
	basic_beats_check(beats.snap_to(Beats(0.5)), MAX_INT32, PPQN * 2);
	
	beats = Beats(LOWEST_INT32, -(PPQN + 1 + PPQN / 2));
	basic_beats_check(beats.round_to_beat(), LOWEST_INT32, -(PPQN * 2));
	basic_beats_check(beats.round_up_to_beat(), LOWEST_INT32, -PPQN);
	basic_beats_check(beats.round_down_to_beat(), LOWEST_INT32, -(PPQN * 2));
	basic_beats_check(beats.snap_to(Beats(0.5)), LOWEST_INT32,
	                  -(PPQN + PPQN / 2));
}

void BeatsTest::logical_op_test()
{
	Beats beats1;
	Beats beats2;

	// Test the ! operator.

	CPPUNIT_ASSERT(!beats1);
	beats1 = 10.5;
	CPPUNIT_ASSERT(!!beats1);

	beats2 = 7.25;

	// Test comparisons between Beats objects.

	beats_comparison_check(beats1, beats2);

	// Test comparisons between Beats and other types of objects.

	CPPUNIT_ASSERT(beats1 == 10.5);
	CPPUNIT_ASSERT(beats1 == 10);
	CPPUNIT_ASSERT(!(beats1 == 10.0));
	CPPUNIT_ASSERT(!(beats1 == 9));
	CPPUNIT_ASSERT(beats1 < 11.0);
	CPPUNIT_ASSERT(beats1 <= 11.0);
	CPPUNIT_ASSERT(beats1 <= 10.5);
	CPPUNIT_ASSERT(!(beats1 < 10.0));
	CPPUNIT_ASSERT(!(beats1 <= 10.0));
	CPPUNIT_ASSERT(beats1 > 10.0);
	CPPUNIT_ASSERT(beats1 >= 10.0);
	CPPUNIT_ASSERT(beats1 >= 10.5);
	CPPUNIT_ASSERT(!(beats1 > 11.0));
	CPPUNIT_ASSERT(!(beats1 >= 11.0));
	
	// Test cases where the number of beats is outside the range of int32_t.

	beats1 = Beats(MAX_INT32, PPQN + 1);
	beats2 = Beats(MAX_INT32, PPQN);
	beats_comparison_check(beats1, beats2);

	beats2 = Beats();
	beats_comparison_check(beats1, beats2);

	beats1 = Beats(LOWEST_INT32, -1);
	beats2 = Beats(LOWEST_INT32, -2);
	beats_comparison_check(beats1, beats2);
}

void BeatsTest::add_test()
{
	// Adding Beats
	add_check(Beats(10, PPQN - 1), Beats(14, 2), 25, 1);
	add_check(Beats(10, PPQN - 1), Beats(-14, 2), -3, 1);
	add_check(Beats(-10, 0), Beats(14, 1), 4, 1);
	add_check(Beats(-10, 0), Beats(-14, 1), -24, 1);

	// Adding ints
	add_check(Beats(10, PPQN - 1), 10, 20, PPQN - 1);
	add_check(Beats(10, PPQN - 1), -5, 5, PPQN - 1);
	add_check(Beats(-10, 1), 10, 0, 1);
	add_check(Beats(-10, 1), -5, -15, 1);

	// Adding doubles
	add_check(Beats(5, 1), 10.5, 15, 1 + PPQN / 2);
	add_check(Beats(5, 1), -10.5, -6, 1 + PPQN / 2);
	add_check(Beats(-5, 1), 10.5, 5, 1 + PPQN / 2);
	add_check(Beats(-5, 1), -10.5, -16, 1 + PPQN / 2);

	// Test cases where the resulting number of beats does not fit in an
	// int32_t.

	// Adding Beats
	add_check(Beats(MAX_INT32 - 1, 1), Beats(2, 1), MAX_INT32, PPQN + 2);
	add_check(Beats(LOWEST_INT32 + 1, -1), Beats(-2, -1), LOWEST_INT32,
	          -(PPQN + 2));

	// Adding ints
	add_check(Beats(MAX_INT32 - 1, 1), 2, MAX_INT32, PPQN + 1);
	add_check(Beats(LOWEST_INT32 + 1, -1), -2, LOWEST_INT32, -(PPQN + 1));

	// Adding doubles
	add_check(Beats(MAX_INT32 - 1, 0), 2.5, MAX_INT32, PPQN + PPQN / 2);
	add_check(Beats(LOWEST_INT32 + 1, -1), -2.5, LOWEST_INT32,
	          -(1 + PPQN + PPQN / 2));
}

void BeatsTest::subtract_test()
{
	// Subtracting Beats
	subtract_check(Beats(10, PPQN - 1), Beats(14, 2), -4, PPQN - 3);
	subtract_check(Beats(10, PPQN - 1), Beats(-14, -2), 25, 1);
	subtract_check(Beats(-10, 0), Beats(14, 1), -25, PPQN - 1);
	subtract_check(Beats(-10, 0), Beats(-14, -1), 4, 1);

	// Subtracting ints
	subtract_check(Beats(10, PPQN - 1), 5, 5, PPQN - 1);
	subtract_check(Beats(10, PPQN - 1), -5, 15, PPQN - 1);
	subtract_check(Beats(-10, 1), 5, -15, 1);
	subtract_check(Beats(-10, 1), -5, -5, 1);

	// Subtracting doubles
	subtract_check(Beats(5, 1), 10.5, -6, 1 + PPQN / 2);
	subtract_check(Beats(5, 1), -10.5, 15, 1 + PPQN / 2);
	subtract_check(Beats(-5, 1), 10.5, -16, 1 + PPQN / 2);
	subtract_check(Beats(-5, 1), -10.5, 5, 1 + PPQN / 2);

	// Test cases where the resulting number of beats does not fit in an
	// int32_t.

	// Subtracting Beats
	subtract_check(Beats(MAX_INT32 - 1, 1), Beats(-2, -1), MAX_INT32,
	               PPQN + 2);
	subtract_check(Beats(LOWEST_INT32 + 1, -1), Beats(2, 1), LOWEST_INT32,
	               -(PPQN + 2));

	// Subtracting ints
	subtract_check(Beats(MAX_INT32 - 1, 1), -2, MAX_INT32, PPQN + 1);
	subtract_check(Beats(LOWEST_INT32 + 1, -1), 2, LOWEST_INT32, -(PPQN + 1));

	// Subtracting doubles
	subtract_check(Beats(MAX_INT32 - 1, 0), -2.5, MAX_INT32, PPQN + PPQN / 2);
	subtract_check(Beats(LOWEST_INT32 + 1, -1), 2.5, LOWEST_INT32,
	               -(1 + PPQN + PPQN / 2));
}

void BeatsTest::multiply_test()
{
	Beats beats(25, 5);
	multiply_check(beats, 2, 50, 10);
	multiply_check(beats, 2.0, 50, 10);
	multiply_check(beats, 0.2, 5, 1);
	multiply_check(beats, -2, -51, PPQN - 10);
	multiply_check(beats, -2.0, -51, PPQN - 10);
	multiply_check(beats, -0.2, -6, PPQN - 1);

	beats = Beats(-25, -5);
	multiply_check(beats, 2, -51, PPQN - 10);
	multiply_check(beats, 2.0, -51, PPQN - 10);
	multiply_check(beats, 0.2, -6, PPQN - 1);
	multiply_check(beats, -2, 50, 10);
	multiply_check(beats, -2.0, 50, 10);
	multiply_check(beats, -0.2, 5, 1);

	// Test cases where the resulting number of beats does not fit in an
	// int32_t.
	beats = Beats(MAX_INT32 / 2, PPQN * 2);
	multiply_check(beats, 2, MAX_INT32, PPQN * 3);
	multiply_check(beats, 2.0, MAX_INT32, PPQN * 3);
	multiply_check(beats, -2, LOWEST_INT32, -PPQN * 2);
	multiply_check(beats, -2.0, LOWEST_INT32, -PPQN * 2);

	beats = Beats(LOWEST_INT32 / 2, -PPQN * 2);
	multiply_check(beats, 2, LOWEST_INT32, -PPQN * 4);
	multiply_check(beats, 2.0, LOWEST_INT32, -PPQN * 4);
	multiply_check(beats, -2, MAX_INT32, PPQN * 5);
	multiply_check(beats, -2.0, MAX_INT32, PPQN * 5);
}

void BeatsTest::divide_test()
{
	Beats beats(25, 5);
	divide_check(beats, 5, 5, 1);
	divide_check(beats, 5.0, 5, 1);
	divide_check(beats, 0.5, 50, 10);
	divide_check(beats, -0.5, -51, PPQN - 10);

	beats = Beats(-25, -5);
	divide_check(beats, 5, -6, PPQN - 1);
	divide_check(beats, 5.0, -6, PPQN - 1);
	divide_check(beats, 0.5, -51, PPQN - 10);
	divide_check(beats, -0.5, 50, 10);

	// Test cases where the resulting number of beats does not fit in an
	// int32_t.

	beats = Beats(MAX_INT32 / 2, PPQN * 2);
	divide_check(beats, 0.5, MAX_INT32, PPQN * 3);
	divide_check(beats, -0.5, LOWEST_INT32, -PPQN * 2);

	beats = Beats(LOWEST_INT32 / 2, -PPQN * 2);
	divide_check(beats, 0.5, LOWEST_INT32, -PPQN * 4);
	divide_check(beats, -0.5, MAX_INT32, PPQN * 5);
}

void BeatsTest::serialization_test()
{
	Beats beats;
	beats_serialize_check(beats);

	beats = Beats(10, 1);
	beats_serialize_check(beats);

	beats = Beats(-10, 1);
	beats_serialize_check(beats);

	beats = Beats(-10, -(PPQN - 1));
	beats_serialize_check(beats);
}

void BeatsTest::misc_test()
{
	Beats one_tick = Beats::tick();
	basic_beats_check(one_tick, 0, 1);

	Beats beats(100, PPQN);
	CPPUNIT_ASSERT_EQUAL(((int64_t) 101) * PPQN, beats.to_ticks());
	CPPUNIT_ASSERT_EQUAL(((int64_t) 101) * PPQN * 2, beats.to_ticks(PPQN * 2));

	beats = Beats::ticks_at_rate(PPQN, PPQN * 2);
	basic_beats_check(beats, 0, PPQN / 2);

	beats = 10.5;
	basic_beats_check(beats, 10, PPQN / 2);

	beats = -Beats(10, 1);
	basic_beats_check(beats, -11, PPQN - 1);
}

void BeatsTest::basic_beats_check(const Beats& beats, int32_t expected_beats,
                                  int32_t expected_ticks)
{
	// Value-specific checks.

	CPPUNIT_ASSERT_EQUAL(expected_beats, beats.get_beats());
	CPPUNIT_ASSERT_EQUAL(expected_ticks, beats.get_ticks());
	CPPUNIT_ASSERT_EQUAL(((int64_t) expected_beats) * PPQN + expected_ticks,
	                     beats.to_ticks());
	CPPUNIT_ASSERT_DOUBLES_EQUAL(expected_beats + ((double) expected_ticks) / PPQN,
	                             beats.to_double(), 1.0 / PPQN);

	// Miscellaneous checks.

	Beats clone = beats;
	CPPUNIT_ASSERT_EQUAL(beats, clone);
	
	clone = beats.to_double();
	CPPUNIT_ASSERT_DOUBLES_EQUAL(beats.to_double(), clone.to_double(),
	                             1.0 / PPQN);
}

void BeatsTest::beats_comparison_check(const Beats& greater,
                                       const Beats& smaller)
{
	CPPUNIT_ASSERT(greater == greater);
	CPPUNIT_ASSERT(!(greater != greater));
	CPPUNIT_ASSERT(!(greater == smaller));
	CPPUNIT_ASSERT(greater != smaller);
	CPPUNIT_ASSERT(greater <= greater);
	CPPUNIT_ASSERT(greater >= greater);
	CPPUNIT_ASSERT(smaller < greater);
	CPPUNIT_ASSERT(greater > smaller);
	CPPUNIT_ASSERT(smaller <= greater);
	CPPUNIT_ASSERT(greater >= smaller);
	CPPUNIT_ASSERT(!(greater < smaller));
	CPPUNIT_ASSERT(!(greater < greater));
	CPPUNIT_ASSERT(!(smaller > greater));
	CPPUNIT_ASSERT(!(greater > greater));
	CPPUNIT_ASSERT(!(greater <= smaller));
	CPPUNIT_ASSERT(!(smaller >= greater));
}

void BeatsTest::beats_serialize_check(const Beats& beats)
{
	std::ostringstream out_stream;
	out_stream << beats;
	std::istringstream in_stream(out_stream.str());
	Beats deserialized_beats;
	in_stream >> deserialized_beats;
	CPPUNIT_ASSERT_EQUAL(beats, deserialized_beats);
}

