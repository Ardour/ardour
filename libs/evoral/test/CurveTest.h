#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <boost/shared_ptr.hpp>
#include "evoral/ControlList.h"

class CurveTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (CurveTest);
	CPPUNIT_TEST (trivial);
	CPPUNIT_TEST (rtGet);
	CPPUNIT_TEST (twoPointLinear);
	CPPUNIT_TEST (threePointLinear);
	CPPUNIT_TEST (threePointDiscete);
	CPPUNIT_TEST (constrainedCubic);
	CPPUNIT_TEST (ctrlListEval);
	CPPUNIT_TEST_SUITE_END ();

public:
	void trivial ();
	void rtGet ();
	void twoPointLinear ();
	void threePointLinear ();
	void threePointDiscete ();
	void constrainedCubic ();
	void ctrlListEval ();

private:
	boost::shared_ptr<Evoral::ControlList> TestCtrlList() {
		Evoral::Parameter param (Evoral::Parameter(0));
		const Evoral::ParameterDescriptor desc;
		return boost::shared_ptr<Evoral::ControlList> (new Evoral::ControlList(param, desc));
	}
};
