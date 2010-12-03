#include "tests/utils.h"

#include "audiographer/utils/identity_vertex.h"

using namespace AudioGrapher;

class IdentityVertexTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE (IdentityVertexTest);
  CPPUNIT_TEST (testProcess);
  CPPUNIT_TEST (testRemoveOutput);
  CPPUNIT_TEST (testClearOutputs);
  CPPUNIT_TEST_SUITE_END ();

  public:
	void setUp()
	{
		frames = 128;
		random_data = TestUtils::init_random_data(frames);
		
		zero_data = new float[frames];
		memset (zero_data, 0, frames * sizeof(float));

		sink_a.reset (new VectorSink<float>());
		sink_b.reset (new VectorSink<float>());
	}

	void tearDown()
	{
		delete [] random_data;
		delete [] zero_data;
	}

	void testProcess()
	{
		vertex.reset (new IdentityVertex<float>());
		vertex->add_output (sink_a);
		vertex->add_output (sink_b);
		
		framecnt_t frames_output = 0;
		
		ProcessContext<float> c (random_data, frames, 1);
		vertex->process (c);
		
		frames_output = sink_a->get_data().size();
		CPPUNIT_ASSERT_EQUAL (frames, frames_output);
		
		frames_output = sink_b->get_data().size();
		CPPUNIT_ASSERT_EQUAL (frames, frames_output);
		
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, sink_a->get_array(), frames));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, sink_b->get_array(), frames));
	}
	
	void testRemoveOutput()
	{
		vertex.reset (new IdentityVertex<float>());
		vertex->add_output (sink_a);
		vertex->add_output (sink_b);
		
		ProcessContext<float> c (random_data, frames, 1);
		vertex->process (c);
		
		vertex->remove_output (sink_a);
		ProcessContext<float> zc (zero_data, frames, 1);
		vertex->process (zc);
		
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, sink_a->get_array(), frames));
		CPPUNIT_ASSERT (TestUtils::array_equals (zero_data, sink_b->get_array(), frames));
	}
	
	void testClearOutputs()
	{
		vertex.reset (new IdentityVertex<float>());
		vertex->add_output (sink_a);
		vertex->add_output (sink_b);
		
		ProcessContext<float> c (random_data, frames, 1);
		vertex->process (c);
		
		vertex->clear_outputs ();
		ProcessContext<float> zc (zero_data, frames, 1);
		vertex->process (zc);
		
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, sink_a->get_array(), frames));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, sink_b->get_array(), frames));
	}

  private:
	boost::shared_ptr<IdentityVertex<float> > vertex;
	boost::shared_ptr<VectorSink<float> > sink_a;
	boost::shared_ptr<VectorSink<float> > sink_b;

	float * random_data;
	float * zero_data;
	framecnt_t frames;
};

CPPUNIT_TEST_SUITE_REGISTRATION (IdentityVertexTest);

