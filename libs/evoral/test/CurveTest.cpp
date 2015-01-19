#include "CurveTest.hpp"
#include "evoral/ControlList.hpp"
#include "evoral/Curve.hpp"
#include <stdlib.h>

CPPUNIT_TEST_SUITE_REGISTRATION (CurveTest);

using namespace Evoral;

// linear y = Y0 + YS * x ;  with x = i * (X1 - X0) + X0; and i = [0..1023]
#define VEC1024LINCMP(X0, X1, Y0, YS)                                        \
    cl->curve ().get_vector ((X0), (X1), vec, 1024);                         \
    for (int i = 0; i < 1024; ++i) {                                         \
        char msg[64];                                                        \
        snprintf (msg, 64, "at i=%d (x0=%.1f, x1=%.1f, y0=%.1f, ys=%.3f)",   \
            i, X0, X1, Y0, YS);                                              \
        CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE (                               \
            msg,                                                             \
            (Y0) + i * (YS), vec[i],                                         \
            1e-24                                                            \
            );                                                               \
    }

void
CurveTest::twoPointLinear ()
{
	float vec[1024];

	boost::shared_ptr<Evoral::ControlList> cl = TestCtrlList();

	cl->create_curve ();
	cl->set_interpolation (ControlList::Linear);

	// add two points to curve
	cl->fast_simple_add (   0.0 , 2048.0);
	cl->fast_simple_add (8192.0 , 4096.0);

	cl->curve ().get_vector (1024.0, 2047.0, vec, 1024);

	VEC1024LINCMP (1024.0, 2047.0, 2304.f,  .25f);
	VEC1024LINCMP (2048.0, 2559.5, 2560.f,  .125f);
	VEC1024LINCMP (   0.0, 4092.0, 2048.f, 1.f);

	// greetings to tartina
	cl->curve ().get_vector (2048.0, 2048.0, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 @ 2048..2048", 2560.f, vec[0]);

	/* XXX WHAT DO WE EXPECT WITH veclen=1 AND  x1 > x0 ? */
#if 0
	/* .. interpolated value at (x1+x0)/2 */
	cl->curve ().get_vector (2048.0, 2049.0, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 @ 2048-2049", 2560.125f, vec[0]);

	cl->curve ().get_vector (2048.0, 2056.0, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 @ 2048-2049", 2561.f, vec[0]);
#else
	/* .. value at x0 */
	cl->curve ().get_vector (2048.0, 2049.0, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 , 2048..2049", 2560.f, vec[0]);

	cl->curve ().get_vector (2048.0, 2056.0, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 , 2048..2049", 2560.f, vec[0]);
#endif

	cl->curve ().get_vector (2048.0, 2048.0, vec, 2);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=2 , 2048..2048 @ 0", 2560.f, vec[0]);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=2 , 2048..2048 @ 1", 2560.f, vec[1]);

	cl->curve ().get_vector (2048.0, 2056.0, vec, 2);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=2 , 2048..2056 @ 0", 2560.f, vec[0]);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=2 , 2048..2056 @ 0", 2562.f, vec[1]);

	cl->curve ().get_vector (2048.0, 2056.0, vec, 3);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=3 , 2048..2056 @ 0", 2560.f, vec[0]);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=3 , 2048..2056 @ 1", 2561.f, vec[1]);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=3 , 2048..2056 @ 2", 2562.f, vec[2]);

	/* check out-of range..
	 * we expect the first and last value - no interpolation
	 */
	cl->curve ().get_vector (-1, -1, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 @ -1", 2048.f, vec[0]);

	cl->curve ().get_vector (9999.0, 9999.0, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 @ 9999", 4096.f, vec[0]);

	cl->curve ().get_vector (-999.0, 0, vec, 13);
	for (int i = 0; i < 13; ++i) {
		CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=13 @ -999..0", 2048.f, vec[i]);
	}

	cl->curve ().get_vector (9998.0, 9999.0, vec, 8);
	for (int i = 0; i < 8; ++i) {
		CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=8 @ 9998..9999", 4096.f, vec[i]);
	}
}

void
CurveTest::threePointLinear ()
{
	float vec[4];

	boost::shared_ptr<Evoral::ControlList> cl = TestCtrlList();

	cl->create_curve ();
	cl->set_interpolation (ControlList::Linear);

	// add 3 points to curve
	cl->fast_simple_add (   0.0 , 2.0);
	cl->fast_simple_add ( 100.0 , 4.0);
	cl->fast_simple_add ( 200.0 , 0.0);

	cl->curve ().get_vector (50.0, 60.0, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 @ 50", 3.f, vec[0]);

	cl->curve ().get_vector (100.0, 100.0, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 @ 100", 4.f, vec[0]);

	cl->curve ().get_vector (150.0, 150.0, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 @ 150", 2.f, vec[0]);

	cl->curve ().get_vector (130.0, 150.0, vec, 3);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=3 130..150 @ 0", 2.8f, vec[0]);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=3 130..150 @ 2", 2.4f, vec[1]);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=3 130..150 @ 3", 2.0f, vec[2]);

	cl->curve ().get_vector (80.0, 160.0, vec, 3);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=3 80..160 @ 0", 3.6f, vec[0]);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=3 80..160 @ 2", 3.2f, vec[1]);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=3 80..160 @ 3", 1.6f, vec[2]);
}

void
CurveTest::threePointDiscete ()
{
	boost::shared_ptr<Evoral::ControlList> cl = TestCtrlList();
	cl->set_interpolation (ControlList::Discrete);

	// add 3 points to curve
	cl->fast_simple_add (   0.0 , 2.0);
	cl->fast_simple_add ( 100.0 , 4.0);
	cl->fast_simple_add ( 200.0 , 0.0);

	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(80.));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(120.));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(160.));

	cl->set_interpolation (ControlList::Linear);

	CPPUNIT_ASSERT_EQUAL(3.6, cl->unlocked_eval(80.));
	CPPUNIT_ASSERT_EQUAL(3.2, cl->unlocked_eval(120.));
	CPPUNIT_ASSERT_EQUAL(1.6, cl->unlocked_eval(160.));
}

void
CurveTest::ctrlListEval ()
{
	boost::shared_ptr<Evoral::ControlList> cl = TestCtrlList();

	cl->fast_simple_add (   0.0 , 2.0);

	cl->set_interpolation (ControlList::Discrete);
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(80.));
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(120.));
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(160.));

	cl->set_interpolation (ControlList::Linear);
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(80.));
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(120.));
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(160.));

	cl->fast_simple_add ( 100.0 , 4.0);

	cl->set_interpolation (ControlList::Discrete);
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(80.));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(120.));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(160.));

	cl->set_interpolation (ControlList::Linear);
	CPPUNIT_ASSERT_EQUAL(3.6, cl->unlocked_eval(80.));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(120.));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(160.));

	cl->fast_simple_add ( 200.0 , 0.0);

	cl->set_interpolation (ControlList::Discrete);
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(80.));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(120.));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(160.));

	cl->set_interpolation (ControlList::Linear);
	CPPUNIT_ASSERT_EQUAL(3.6, cl->unlocked_eval(80.));
	CPPUNIT_ASSERT_EQUAL(3.2, cl->unlocked_eval(120.));
	CPPUNIT_ASSERT_EQUAL(1.6, cl->unlocked_eval(160.));

	cl->fast_simple_add ( 300.0 , 8.0);

	cl->set_interpolation (ControlList::Discrete);
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(80.));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(120.));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(160.));
	CPPUNIT_ASSERT_EQUAL(0.0, cl->unlocked_eval(250.));
	CPPUNIT_ASSERT_EQUAL(8.0, cl->unlocked_eval(999.));

	cl->set_interpolation (ControlList::Linear);
	CPPUNIT_ASSERT_EQUAL(3.6, cl->unlocked_eval(80.));
	CPPUNIT_ASSERT_EQUAL(3.2, cl->unlocked_eval(120.));
	CPPUNIT_ASSERT_EQUAL(1.6, cl->unlocked_eval(160.));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(250.));
	CPPUNIT_ASSERT_EQUAL(8.0, cl->unlocked_eval(999.));

	cl->fast_simple_add ( 400.0 , 9.0);

	cl->set_interpolation (ControlList::Discrete);
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(80.));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(120.));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(160.));
	CPPUNIT_ASSERT_EQUAL(0.0, cl->unlocked_eval(250.));
	CPPUNIT_ASSERT_EQUAL(8.0, cl->unlocked_eval(350.));
	CPPUNIT_ASSERT_EQUAL(9.0, cl->unlocked_eval(999.));

	cl->set_interpolation (ControlList::Linear);
	CPPUNIT_ASSERT_EQUAL(3.6, cl->unlocked_eval(80.));
	CPPUNIT_ASSERT_EQUAL(3.2, cl->unlocked_eval(120.));
	CPPUNIT_ASSERT_EQUAL(1.6, cl->unlocked_eval(160.));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(250.));
	CPPUNIT_ASSERT_EQUAL(8.5, cl->unlocked_eval(350.));
	CPPUNIT_ASSERT_EQUAL(9.0, cl->unlocked_eval(999.));
}
