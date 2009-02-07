#include "sequence.hpp"
#include <cassert>

CPPUNIT_TEST_SUITE_REGISTRATION( SequenceTest );

void 
SequenceTest::createTest (void)
{
       	DummyTypeMap type_map;
       	MySequence<double> seq(type_map, 100);

       	CPPUNIT_ASSERT_EQUAL(size_t(0), seq.sysexes().size());
       	CPPUNIT_ASSERT_EQUAL(size_t(100), seq.notes().size());
       	CPPUNIT_ASSERT(seq.notes().begin() != seq.notes().end());
}


