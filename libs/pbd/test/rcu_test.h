#include <string>
#include <pthread.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "pbd/rcu.h"

class RCUTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (RCUTest);
	CPPUNIT_TEST (race);
	CPPUNIT_TEST_SUITE_END ();

public:
	RCUTest ();
	void setUp ();
	void race ();

	void read_thread ();
	void write_thread ();

private:
	class Value {
		public:
			Value (std::string const& v)
				: val (v)
		{}
		std::string val;
	};

	typedef std::map<std::string, boost::shared_ptr<Value> > Values;

	SerializedRCUManager<Values> _values;

#ifdef __APPLE__
	pthread_mutex_t _mutex;
	pthread_cond_t  _cond;
	size_t          _cnt;
#else
	pthread_barrier_t _barrier;
#endif
};
