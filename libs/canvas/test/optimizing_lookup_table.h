#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class OptimizingLookupTableTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (OptimizingLookupTableTest);
	CPPUNIT_TEST (build_1);
	CPPUNIT_TEST (build_2);
	CPPUNIT_TEST (build_negative);
	CPPUNIT_TEST (get_big);
	CPPUNIT_TEST (get_small);
	CPPUNIT_TEST (check_ordering);
	CPPUNIT_TEST_SUITE_END ();

public:
	void build_1 ();
	void build_2 ();
	void build_negative ();
	void get_big ();
	void get_small ();
	void check_ordering ();
};



