#include "CurveTest.h"
#include "evoral/ControlList.h"
#include "evoral/Curve.h"
#include <stdlib.h>

CPPUNIT_TEST_SUITE_REGISTRATION (CurveTest);

#if defined(PLATFORM_WINDOWS) && defined(COMPILER_MINGW)
/* cppunit-1.13.2  uses assertion_traits<double>
 *    sprintf( , "%.*g", precision, x)
 * to format a double. The actual comparison is performed on a string.
 * This is problematic with mingw/windows|wine, "%.*g" formatting fails.
 *
 * This quick hack compares float, however float compatisons are at most Y.MMMM+eXX,
 * the max precision needs to be limited. to the last mantissa digit.
 *
 * Anyway, actual maths is verified with Linux and OSX unit-tests,
 * and this needs to go to https://sourceforge.net/p/cppunit/bugs/
 */
#define MAXPREC(P) ((P) < .0005 ? .0005 : (P))
#define CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE(M,A,B,P) CPPUNIT_ASSERT_EQUAL_MESSAGE(M, (float)rint ((A) / MAXPREC(P)),(float)rint ((B) / MAXPREC(P)))
#define CPPUNIT_ASSERT_DOUBLES_EQUAL(A,B,P) CPPUNIT_ASSERT_EQUAL((float)rint ((A) / MAXPREC(P)),(float)rint ((B) / MAXPREC(P)))
#endif

using namespace Evoral;
using namespace Temporal;

// linear y = Y0 + YS * x ;  with x = i * (X1 - X0) + X0; and i = [0..1023]
#define VEC1024LINCMP(X0, X1, Y0, YS)                                        \
    cl->curve ().get_vector ((X0), (X1), vec, 1024);                         \
    for (int i = 0; i < 1024; ++i) {                                         \
        char msg[64];                                                        \
        snprintf (msg, 64, "at i=%d (x0=%s, x1=%s, y0=%.1f, ys=%.3f)",       \
            i, (X0).str().c_str(), (X1).str().c_str(), Y0, YS);              \
        CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE (                               \
            msg,                                                             \
            (Y0) + i * (YS), vec[i],                                         \
            1e-24                                                            \
            );                                                               \
    }

void
CurveTest::trivial ()
{
	float vec[1024];

	std::shared_ptr<Evoral::ControlList> cl = TestCtrlList();

	cl->create_curve ();

	timepos_t t1024 (1024.0);
	timepos_t t2047 (2047.0);

	// Empty curve
	cl->curve().get_vector (t1024, t2047, vec, 1024);
	for (int i = 0; i < 1024; ++i) {
		CPPUNIT_ASSERT_EQUAL (0.0f, vec[i]);
	}

	// Single point curve
	cl->fast_simple_add(timepos_t (0), 42.0);
	cl->curve().get_vector (t1024, t2047, vec, 1024);
	for (int i = 0; i < 1024; ++i) {
		CPPUNIT_ASSERT_EQUAL (42.0f, vec[i]);
	}
}

void
CurveTest::rtGet ()
{
	float vec[1024];

	timepos_t t1024 (1024.0);
	timepos_t t2047 (2047.0);

	// Create simple control list
	std::shared_ptr<Evoral::ControlList> cl = TestCtrlList();
	cl->create_curve ();
	cl->fast_simple_add(timepos_t(0), 42.0);

	{
		// Write-lock list
		Glib::Threads::RWLock::WriterLock lm(cl->lock());

		// Attempt to get vector in RT (expect failure)
		CPPUNIT_ASSERT (!cl->curve().rt_safe_get_vector (t1024, t2047, vec, 1024));
	}

	// Attempt to get vector in RT (expect success)
	CPPUNIT_ASSERT (cl->curve().rt_safe_get_vector (t1024, t2047, vec, 1024));
	for (int i = 0; i < 1024; ++i) {
		CPPUNIT_ASSERT_EQUAL (42.0f, vec[i]);
	}
}

void
CurveTest::twoPointLinear ()
{
	float vec[1024];

	std::shared_ptr<Evoral::ControlList> cl = TestCtrlList();

	cl->create_curve ();
	cl->set_interpolation (ControlList::Linear);

	timepos_t t0 (0);
	timepos_t t1024 (1024);
	timepos_t t2048 (2048);
	timepos_t t2047 (2047);
	timepos_t t2049 (2049);
	timepos_t t2056 (2056);
	timepos_t t4092 (4092);
	timepos_t t8192 (8192);

	// add two points to curve
	cl->fast_simple_add (t0 , 2048.0);
	cl->fast_simple_add (t8192, 4096.0);

	cl->curve ().get_vector (t1024, t2047, vec, 1024);

	VEC1024LINCMP (t1024, t2047, 2304.f,  .25f);
	//VEC1024LINCMP (t2048, timepos_t (2559.5), 2560.f,  .125f);
	VEC1024LINCMP (t0, t4092, 2048.f, 1.f);

	// greetings to tartina
	cl->curve ().get_vector (t2048, t2048, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 @ 2048..2048", 2560.f, vec[0]);

	/* XXX WHAT DO WE EXPECT WITH veclen=1 AND  x1 > x0 ? */
#if 0
	/* .. interpolated value at (x1+x0)/2 */
	cl->curve ().get_vector (2048.0, t2049, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 @ 2048-2049", 2560.125f, vec[0]);

	cl->curve ().get_vector (2048.0, 2056.0, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 @ 2048-2049", 2561.f, vec[0]);
#else
	/* .. value at x0 */
	cl->curve ().get_vector (t2048, t2049, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 , 2048..2049", 2560.f, vec[0]);

	cl->curve ().get_vector (t2048, t2056, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 , 2048..2049", 2560.f, vec[0]);
#endif

	cl->curve ().get_vector (t2048, t2048, vec, 2);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=2 , 2048..2048 @ 0", 2560.f, vec[0]);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=2 , 2048..2048 @ 1", 2560.f, vec[1]);

	cl->curve ().get_vector (t2048, t2056, vec, 2);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=2 , 2048..2056 @ 0", 2560.f, vec[0]);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=2 , 2048..2056 @ 0", 2562.f, vec[1]);

	cl->curve ().get_vector (t2048, t2056, vec, 3);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=3 , 2048..2056 @ 0", 2560.f, vec[0]);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=3 , 2048..2056 @ 1", 2561.f, vec[1]);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=3 , 2048..2056 @ 2", 2562.f, vec[2]);

	/* check out-of range..
	 * we expect the first and last value - no interpolation
	 */
	timepos_t tm1 (-1);
	timepos_t tm999 (-999);
	timepos_t t9998 (9998);
	timepos_t t9999 (9999);

	cl->curve ().get_vector (tm1, tm1, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 @ -1", 2048.f, vec[0]);

	cl->curve ().get_vector (t9999, t9999, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 @ 9999", 4096.f, vec[0]);

	cl->curve ().get_vector (tm999, t0, vec, 13);
	for (int i = 0; i < 13; ++i) {
		CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=13 @ -999..0", 2048.f, vec[i]);
	}
	cl->curve ().get_vector (t9998, t9999, vec, 8);
	for (int i = 0; i < 8; ++i) {
		CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=8 @ 9998..9999", 4096.f, vec[i]);
	}
}

void
CurveTest::threePointLinear ()
{
	float vec[4];

	std::shared_ptr<Evoral::ControlList> cl = TestCtrlList();

	cl->create_curve ();
	cl->set_interpolation (ControlList::Linear);

	timepos_t t0 (0);
	timepos_t t50 (50);
	timepos_t t60 (60);
	timepos_t t80 (80);
	timepos_t t100 (100);
	timepos_t t130 (130);
	timepos_t t150 (150);
	timepos_t t160 (160);
	timepos_t t200 (200);

	// add 3 points to curve
	cl->fast_simple_add (  t0 , 2.0);
	cl->fast_simple_add (t100 , 4.0);
	cl->fast_simple_add (t200 , 0.0);

	cl->curve ().get_vector (t50, t60, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 @ 50", 3.f, vec[0]);

	cl->curve ().get_vector (t100, t100, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 @ 100", 4.f, vec[0]);

	cl->curve ().get_vector (t150, t150, vec, 1);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=1 @ 150", 2.f, vec[0]);

	cl->curve ().get_vector (t130, t150, vec, 3);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=3 130..150 @ 0", 2.8f, vec[0]);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=3 130..150 @ 2", 2.4f, vec[1]);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=3 130..150 @ 3", 2.0f, vec[2]);

	cl->curve ().get_vector (t80, t160, vec, 3);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=3 80..160 @ 0", 3.6f, vec[0]);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=3 80..160 @ 2", 3.2f, vec[1]);
	CPPUNIT_ASSERT_EQUAL_MESSAGE ("veclen=3 80..160 @ 3", 1.6f, vec[2]);
}

void
CurveTest::threePointDiscete ()
{
	std::shared_ptr<Evoral::ControlList> cl = TestCtrlList();
	cl->set_interpolation (ControlList::Discrete);

	timepos_t t0 (0);
	timepos_t t80 (80);
	timepos_t t100 (100);
	timepos_t t120 (120);
	timepos_t t160 (160);
	timepos_t t200 (200);

	// add 3 points to curve
	cl->fast_simple_add (  t0 , 2.0);
	cl->fast_simple_add (t100 , 4.0);
	cl->fast_simple_add (t200 , 0.0);

	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(t80));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(t120));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(t160));

	cl->set_interpolation (ControlList::Linear);

	CPPUNIT_ASSERT_EQUAL(3.6, cl->unlocked_eval(t80));
	CPPUNIT_ASSERT_EQUAL(3.2, cl->unlocked_eval(t120));
	CPPUNIT_ASSERT_EQUAL(1.6, cl->unlocked_eval(t160));
}

void
CurveTest::ctrlListEval ()
{
	std::shared_ptr<Evoral::ControlList> cl = TestCtrlList();

	timepos_t t0 (0);
	timepos_t t80 (80);
	timepos_t t100 (100);
	timepos_t t120 (120);
	timepos_t t160 (160);
	timepos_t t200 (200);
	timepos_t t250 (250);
	timepos_t t300 (300);
	timepos_t t350 (350);
	timepos_t t400 (400);
	timepos_t t999 (999);

	cl->fast_simple_add (t0 , 2.0);

	cl->set_interpolation (ControlList::Discrete);
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(t80));
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(t120));
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(t160));

	cl->set_interpolation (ControlList::Linear);
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(t80));
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(t120));
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(t160));

	cl->fast_simple_add (t100 , 4.0);

	cl->set_interpolation (ControlList::Discrete);
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(t80));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(t120));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(t160));

	cl->set_interpolation (ControlList::Linear);
	CPPUNIT_ASSERT_EQUAL(3.6, cl->unlocked_eval(t80));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(t120));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(t160));

	cl->fast_simple_add (t200 , 0.0);

	cl->set_interpolation (ControlList::Discrete);
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(t80));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(t120));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(t160));

	cl->set_interpolation (ControlList::Linear);
	CPPUNIT_ASSERT_EQUAL(3.6, cl->unlocked_eval(t80));
	CPPUNIT_ASSERT_EQUAL(3.2, cl->unlocked_eval(t120));
	CPPUNIT_ASSERT_EQUAL(1.6, cl->unlocked_eval(t160));

	cl->fast_simple_add (t300 , 8.0);

	cl->set_interpolation (ControlList::Discrete);
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(t80));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(t120));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(t160));
	CPPUNIT_ASSERT_EQUAL(0.0, cl->unlocked_eval(t250));
	CPPUNIT_ASSERT_EQUAL(8.0, cl->unlocked_eval(t999));

	cl->set_interpolation (ControlList::Linear);
	CPPUNIT_ASSERT_EQUAL(3.6, cl->unlocked_eval(t80));
	CPPUNIT_ASSERT_EQUAL(3.2, cl->unlocked_eval(t120));
	CPPUNIT_ASSERT_EQUAL(1.6, cl->unlocked_eval(t160));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(t250));
	CPPUNIT_ASSERT_EQUAL(8.0, cl->unlocked_eval(t999));

	cl->fast_simple_add (t400 , 9.0);

	cl->set_interpolation (ControlList::Discrete);
	CPPUNIT_ASSERT_EQUAL(2.0, cl->unlocked_eval(t80));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(t120));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(t160));
	CPPUNIT_ASSERT_EQUAL(0.0, cl->unlocked_eval(t250));
	CPPUNIT_ASSERT_EQUAL(8.0, cl->unlocked_eval(t350));
	CPPUNIT_ASSERT_EQUAL(9.0, cl->unlocked_eval(t999));

	cl->set_interpolation (ControlList::Linear);
	CPPUNIT_ASSERT_EQUAL(3.6, cl->unlocked_eval(t80));
	CPPUNIT_ASSERT_EQUAL(3.2, cl->unlocked_eval(t120));
	CPPUNIT_ASSERT_EQUAL(1.6, cl->unlocked_eval(t160));
	CPPUNIT_ASSERT_EQUAL(4.0, cl->unlocked_eval(t250));
	CPPUNIT_ASSERT_EQUAL(8.5, cl->unlocked_eval(t350));
	CPPUNIT_ASSERT_EQUAL(9.0, cl->unlocked_eval(t999));
}

void
CurveTest::constrainedCubic ()
{

	struct point {
		int x, y;
	};

	static const struct point data[] = {
		/* values from worked example in www.korf.co.uk/spline.pdf */
		{   0,  30 },
		{  10, 130 },
		{  30, 150 },
		{  50, 150 },
		{  70, 170 },
		{  90, 220 },
		{ 100, 320 },
	};

	int32_t type = 0;
	Evoral::Parameter p(type);
	Evoral::ParameterDescriptor pd;
	pd.lower = 5;
	pd.upper = 325;
	Evoral::ControlList l(p, pd, Temporal::TimeDomainProvider (AudioTime));

	size_t i;
	l.set_interpolation(Evoral::ControlList::Curved);

	for (i=0; i<sizeof(data)/sizeof(data[0]); i++) {
		l.add (timepos_t (data[i].x), data[i].y);
	}

	Evoral::Curve curve(l);

	float f[121];
	curve.get_vector(timepos_t (-10), timepos_t (110), f, 121);

	const float *g = &f[10]; /* so g starts at x==0 */

	/* given points - should be exactly equal */
	CPPUNIT_ASSERT_EQUAL( 30.0f, g[-10]);
	CPPUNIT_ASSERT_EQUAL( 30.0f, g[  0]);
	CPPUNIT_ASSERT_EQUAL(130.0f, g[ 10]);
	CPPUNIT_ASSERT_EQUAL(150.0f, g[ 30]);
	CPPUNIT_ASSERT_EQUAL(150.0f, g[ 40]);
	CPPUNIT_ASSERT_EQUAL(150.0f, g[ 50]);
	CPPUNIT_ASSERT_EQUAL(320.0f, g[100]);
	CPPUNIT_ASSERT_EQUAL(320.0f, g[110]);

	/*
	   First segment, i=1, for 0 <= x <= 10
	   f'1(x1) = 2/((x2 – x1)/(y2 – y1) + (x1 – x0)/(y1 – y0))
	           = 2/((30 – 10)/(150 – 130) + (10 – 0)/(130 – 30))
	           = 1.8181
	   f'1(x0) = 3/2*(y1 – y0)/(x1 – x0) - f'1(x1)/2
	           = 3/2*(130 – 30)/(10 – 0) – 1.818/2
	           = 14.0909
	   f"1(x0) = -2*(f'1(x1) + 2* f'1(x0))/(x1 – x0) + 6*(y1 – y0)/ (x1 – x0)^2
	           = -2*(1.8181 + 2*14.0909)/(10 – 0) + 6*(130 – 30)/(10 – 0)^2
	           = 0
	   f"1(x1) = 2*(2*f'1(x1) + f'1(x0))/(x1 – x0) - 6*(y1 – y0)/ (x1 – x0)^2
	           = 2*(2*1.818 + 14.0909)/(10 – 0) – 6*(130 – 30)/(10 – 0)^2
	           = -2.4545
	   d1 = 1/6 * (f"1(x1) - f"1(x0))/(x1 – x0)
	      = 1/6 * (-2.4545 – 0)/(10 – 0)
	      = -0.0409
	   c1 = 1/2 * (x1*f"1(x0) – x0*f"1(x1))/(x1 – x0)
	      = 1/2 * (10*0 – 0*1.8181)/(10 – 0)
	      = 0
	   b1 = ((y1 – y0) – c1*(x21 – x20) – d1*( x31 – x30))/(x1 – x0)
	      = ((130 – 30) – 0*(102 – 02) + 0.0409*(103 – 03))/(10 – 0)
	      = 14.09
	   a1 = y0 – b1*x0 – c1*x20 – d1*x30
	      = 30
	   y1 = 30 + 14.09x - 0.0409x3 for 0 <= x <= 10
	 */
	/*
	   Second segment, i=2, for 10 <= x <= 30
	   f'2(x2) = 2/((x3 – x2)/(y3 – y2) + (x2 – x1)/(y2 – y1))
	           = 2/((50 – 30)/(150 – 150) + (30 – 10)/(150 – 130))
	           = 0
	   f'2(x1) = 2/((x2 – x1)/(y2 – y1) + (x1 – x0)/(y1 – y0))
	           = 1.8181

	   f"2(x1) = -2*(f'2(x2) + 2* f'2(x1))/(x2 – x1) + 6*(y2 – y1)/ (x2 – x1)^2
	           = -2*(0 + 2*1.8181)/(30 – 10) + 6*(150 – 130)/(30 – 10)2
	           = -0.063636
	   f"2(x2) = 2*(2*f'2(x2) + f'2(x1))/(x2 – x1) - 6*(y2 – y1)/ (x2 – x1)^2
	           = 2*(2*0 + 1.8181)/(30 – 10) – 6*(150 – 130)/(30 – 10)^2
	           = -0.11818

	   d2 = 1/6 * (f"2(x2) - f"2(x1))/(x2 – x1)
	      = 1/6 * (-0.11818 + 0.063636)/(30 – 10)
	      = -0.0004545
	   c2 = 1/2 * (x2*f"2(x1) – x1*f"2(x2))/(x2 – x1)
	      = 1/2 * (-30*0.063636 + 10*0.11818)/(30 – 10)
	      = -0.01818
	   b2 = ((y2 – y1) – c2*(x2^2 – x1^2) – d2*( x2^3 – x1^3))/(x2 – x1)
	      = ((150 – 130) + 0.01818*(302 – 102) + 0.0004545*(303 – 103))/(30 – 10)
	      = 2.31818
	   a2 = y1 – b2*x1 – c2*x1^2 – d2*x1^3
	      = 130 – 2.31818*10 + 0.01818*102 + 0.0004545*103
	      = 109.09
	   y2 = 109.09 + 2.31818x - 0.01818x^2 - 0.0004545x^3 for 10 <= x <= 30
	 */


	int x;
	long double a1, b1, c1, d1, a2, b2, c2, d2, fdx0, fddx0, fdx1, fdx2, fddx1, fddx2;
	double x0 = data[0].x;
	double y0 = data[0].y;
	double x1 = data[1].x;
	double y1 = data[1].y;
	double x2 = data[2].x;
	double y2 = data[2].y;
	double x3 = data[3].x;
	double y3 = data[3].y;

	double dx0 = x1 - x0;
	double dy0 = y1 - y0;
	double dx1 = x2 - x1;
	double dy1 = y2 - y1;
	double dx2 = x3 - x2;
	double dy2 = y3 - y2;

	// First (leftmost) segment
	fdx1 = 2.0 / ( dx1 / dy1 + dx0 / dy0 );
	fdx0 = 3.0 / 2.0 * dy0 / dx0 - fdx1 / 2.0;

	fddx0 = -2.0 * (fdx1 + 2.0 * fdx0) / dx0 + 6.0 * dy0 / (dx0*dx0);
	fddx1 =  2.0 * (2.0 * fdx1 + fdx0) / dx0 - 6.0 * dy0 / (dx0*dx0);
	d1 = 1.0 / 6.0 * (fddx1 - fddx0) / dx0;
	c1 = 1.0 / 2.0 * (x1 * fddx0 - x0 * fddx1) / dx0;
	b1 = (dy0 - c1 * (x1* x1 - x0*x0) - d1 * (x1*x1*x1 - x0*x0*x0)) / dx0;
	a1 = y0 - b1*x0 - c1*x0*x0 - d1*x0*x0*x0;

	// printf("dx0=%f, dy0=%f, dx1=%f, dy1=%f\n", dx0, dy0, dx1, dy1);
	// printf("fdx0=%Lf, fdx1=%Lf, fddx0=%Lf, fddx1=%Lf\n", fdx0, fdx1, fddx0, fddx1);
	// printf("a1=%Lf, b1=%Lf, c1=%Lf, d1=%Lf\n", a1, b1, c1, d1);

	// values from worked example: deltas rather arbitrary, I'm afraid
	CPPUNIT_ASSERT_DOUBLES_EQUAL(30.0, a1, 0.1);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(14.09, b1, 0.01);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, c1, 0.1);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(-0.0409, d1, 0.0001);

	for (x = 0; x <= 10; x++) {
		double v = a1 + b1*x + c1*x*x  + d1*x*x*x;
		char msg[64];
		snprintf(msg, 64, "interpolating %d: v=%f, x=%f...\n", x, v, g[x]);
		CPPUNIT_ASSERT_DOUBLES_EQUAL(v, g[x], 0.000004);
	}

	// Second segment
	fdx2 = 2.0 / ( dx2 / dy2 + dx1 / dy1 );

	fddx1 = -2.0 * (fdx2 + 2.0 * fdx1) / dx1 + 6.0 * dy1 / (dx1*dx1);
	fddx2 =  2.0 * (2.0 * fdx2 + fdx1) / dx1 - 6.0 * dy1 / (dx1*dx1);
	d2 = 1.0 / 6.0 * (fddx2 - fddx1) / dx1;
	c2 = 1.0 / 2.0 * (x2 * fddx1 - x1 * fddx2) / dx1;
	b2 = (dy1 - c2 * (x2*x2 - x1*x1) - d2 * (x2*x2*x2 - x1*x1*x1)) / dx1;
	a2 = y1 - b2*x1 - c2*x1*x1 - d2*x1*x1*x1;

	// printf("dx0=%f, dy0=%f, dx1=%f, dy1=%f dx2=%f, dy2=%f\n", dx0, dy0, dx1, dy1, dx2, dy2);
	// printf("fdx1=%Lf, fdx2=%Lf, fddx1=%Lf, fddx2=%Lf\n", fdx1, fdx2, fddx1, fddx2);
	// printf("a2=%Lf, b2=%Lf, c2=%Lf, d2=%Lf\n", a2, b2, c2, d2);

	// values from worked example: deltas rather arbitrary, I'm afraid
	CPPUNIT_ASSERT_DOUBLES_EQUAL(109.09, a2, 0.01);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(2.31818, b2, 0.00001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(-0.01818, c2, 0.00001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(-0.0004545, d2, 0.0000001);

	for (x = 10; x <= 30; x++) {
		double v = a2 + b2*x + c2*x*x  + d2*x*x*x;
		char msg[64];
		snprintf(msg, 64, "interpolating %d: v=%f, x=%f...\n", x, v, g[x]);
		CPPUNIT_ASSERT_DOUBLES_EQUAL(v, g[x], 0.000008);
	}
}
