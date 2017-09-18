#include "tests/utils.h"

#include "audiographer/general/threader.h"

using namespace AudioGrapher;

class ThreaderTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE (ThreaderTest);
  CPPUNIT_TEST (testProcess);
  CPPUNIT_TEST (testRemoveOutput);
  CPPUNIT_TEST (testClearOutputs);
  CPPUNIT_TEST (testExceptions);
  CPPUNIT_TEST_SUITE_END ();

  public:
	void setUp()
	{
		samples = 128;
		random_data = TestUtils::init_random_data (samples, 1.0);

		zero_data = new float[samples];
		memset (zero_data, 0, samples * sizeof(float));

		thread_pool = new Glib::ThreadPool (3);
		threader.reset (new Threader<float> (*thread_pool));

		sink_a.reset (new VectorSink<float>());
		sink_b.reset (new VectorSink<float>());
		sink_c.reset (new VectorSink<float>());
		sink_d.reset (new VectorSink<float>());
		sink_e.reset (new VectorSink<float>());
		sink_f.reset (new VectorSink<float>());

		throwing_sink.reset (new ThrowingSink<float>());
	}

	void tearDown()
	{
		delete [] random_data;
		delete [] zero_data;

		thread_pool->shutdown();
		delete thread_pool;
	}

	void testProcess()
	{
		threader->add_output (sink_a);
		threader->add_output (sink_b);
		threader->add_output (sink_c);
		threader->add_output (sink_d);
		threader->add_output (sink_e);
		threader->add_output (sink_f);

		ProcessContext<float> c (random_data, samples, 1);
		threader->process (c);

		CPPUNIT_ASSERT (TestUtils::array_equals(random_data, sink_a->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals(random_data, sink_b->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals(random_data, sink_c->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals(random_data, sink_d->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals(random_data, sink_e->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals(random_data, sink_f->get_array(), samples));
	}

	void testRemoveOutput()
	{
		threader->add_output (sink_a);
		threader->add_output (sink_b);
		threader->add_output (sink_c);
		threader->add_output (sink_d);
		threader->add_output (sink_e);
		threader->add_output (sink_f);

		ProcessContext<float> c (random_data, samples, 1);
		threader->process (c);

		// Remove a, b and f
		threader->remove_output (sink_a);
		threader->remove_output (sink_b);
		threader->remove_output (sink_f);

		ProcessContext<float> zc (zero_data, samples, 1);
		threader->process (zc);

		CPPUNIT_ASSERT (TestUtils::array_equals(random_data, sink_a->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals(random_data, sink_b->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals(zero_data, sink_c->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals(zero_data, sink_d->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals(zero_data, sink_e->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals(random_data, sink_f->get_array(), samples));
	}

	void testClearOutputs()
	{
		threader->add_output (sink_a);
		threader->add_output (sink_b);
		threader->add_output (sink_c);
		threader->add_output (sink_d);
		threader->add_output (sink_e);
		threader->add_output (sink_f);

		ProcessContext<float> c (random_data, samples, 1);
		threader->process (c);

		threader->clear_outputs();
		ProcessContext<float> zc (zero_data, samples, 1);
		threader->process (zc);

		CPPUNIT_ASSERT (TestUtils::array_equals(random_data, sink_a->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals(random_data, sink_b->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals(random_data, sink_c->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals(random_data, sink_d->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals(random_data, sink_e->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals(random_data, sink_f->get_array(), samples));
	}

	void testExceptions()
	{
		threader->add_output (sink_a);
		threader->add_output (sink_b);
		threader->add_output (sink_c);
		threader->add_output (throwing_sink);
		threader->add_output (sink_e);
		threader->add_output (throwing_sink);

		ProcessContext<float> c (random_data, samples, 1);
		CPPUNIT_ASSERT_THROW (threader->process (c), Exception);

		CPPUNIT_ASSERT (TestUtils::array_equals(random_data, sink_a->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals(random_data, sink_b->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals(random_data, sink_c->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals(random_data, sink_e->get_array(), samples));
	}

  private:
	Glib::ThreadPool * thread_pool;

	boost::shared_ptr<Threader<float> > threader;
	boost::shared_ptr<VectorSink<float> > sink_a;
	boost::shared_ptr<VectorSink<float> > sink_b;
	boost::shared_ptr<VectorSink<float> > sink_c;
	boost::shared_ptr<VectorSink<float> > sink_d;
	boost::shared_ptr<VectorSink<float> > sink_e;
	boost::shared_ptr<VectorSink<float> > sink_f;

	boost::shared_ptr<ThrowingSink<float> > throwing_sink;

	float * random_data;
	float * zero_data;
	samplecnt_t samples;
};

CPPUNIT_TEST_SUITE_REGISTRATION (ThreaderTest);

