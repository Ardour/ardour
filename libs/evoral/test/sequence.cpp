#include <evoral/Sequence.hpp>
#include <evoral/TypeMap.hpp>

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TestRunner.h>
#include <cppunit/BriefTestProgressListener.h>

using namespace Evoral;

class DummyTypeMap : public TypeMap {
public:
	virtual ~DummyTypeMap() {}

	virtual bool type_is_midi(uint32_t type) const {return true;}

	virtual uint8_t parameter_midi_type(const Parameter& param) const {return 0;}
	
	virtual uint32_t midi_event_type(uint8_t status) const {return 0;}
	
	virtual bool is_integer(const Evoral::Parameter& param) const {return true;}

	virtual Parameter new_parameter(uint32_t type, uint8_t channel, uint32_t id) const {return Parameter(type, channel, id);}

	virtual std::string to_symbol(const Parameter& param) const {return "foo";}
};

template<typename Time>
class MySequence : public Sequence<Time> {
public:
	MySequence(DummyTypeMap&map, int size) : Sequence<Time>(map, size) {}

	boost::shared_ptr<Control> control_factory(const Parameter& param) {return boost::shared_ptr<Control>();}
};

class SequenceTest : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE (SequenceTest);
    CPPUNIT_TEST (createTest);
    CPPUNIT_TEST_SUITE_END ();

    public:
        void setUp (void) {
        	Glib::thread_init();
        }
        
        void tearDown (void);

    protected:
        void createTest (void) {
        	DummyTypeMap type_map;
        	MySequence<double> s(type_map, 100);
        }

    private:
};


int
main()
{

    CPPUNIT_NS::TestResult testresult;

    CPPUNIT_NS::TestResultCollector collectedresults;
    testresult.addListener (&collectedresults);

    CPPUNIT_NS::BriefTestProgressListener progress;
    testresult.addListener (&progress);

    CPPUNIT_NS::TestRunner testrunner;
    testrunner.addTest (CPPUNIT_NS::TestFactoryRegistry::getRegistry ().makeTest ());
    testrunner.run (testresult);

    CPPUNIT_NS::CompilerOutputter compileroutputter (&collectedresults, std::cerr);
    compileroutputter.write ();

    return collectedresults.wasSuccessful () ? 0 : 1;
}
