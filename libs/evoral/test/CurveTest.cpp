#include "CurveTest.hpp"
#include "evoral/ControlList.hpp"
#include "evoral/Curve.hpp"
#include <stdlib.h>

CPPUNIT_TEST_SUITE_REGISTRATION (CurveTest);

using namespace Evoral;

void
CurveTest::interpolateTest1 ()
{
	float vec[1024];

	Parameter param (Parameter(0));
	const Evoral::ParameterDescriptor desc;
	ControlList *cl (new ControlList(param, desc));

	cl->create_curve();

	cl->fast_simple_add(0.0   , 0.0);
	cl->fast_simple_add(8191.0 , 8191.0);

	cl->curve().get_vector(1024, 2047, vec, 1024);

	for (int i = 0; i < 1024; ++i) {
		CPPUNIT_ASSERT_EQUAL (1024.f + i, vec[i]);
	}

	cl->destroy_curve();
	delete cl;
}
