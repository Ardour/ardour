#include <iostream>

#include <glibmm/timer.h>

#include "ardour/audioengine.h"
#include "ardour/audio_backend.h"
#include "ardour/search_paths.h"

#include "audio_engine_test.h"
#include "test_util.h"

CPPUNIT_TEST_SUITE_REGISTRATION (AudioEngineTest);

using namespace std;
using namespace ARDOUR;
using namespace PBD;

void
print_audio_backend_info (AudioBackendInfo const* abi)
{
	cerr << "Audio Backend, name:" << abi->name << endl;
}

void
AudioEngineTest::test_backends ()
{
	AudioEngine* engine = AudioEngine::create ();

	CPPUNIT_ASSERT (engine);

	std::vector<AudioBackendInfo const *> backends = engine->available_backends ();

	CPPUNIT_ASSERT (backends.size () != 0);

	for (std::vector<AudioBackendInfo const *>::const_iterator i = backends.begin();
		i != backends.end(); ++i) {
		print_audio_backend_info(*i);
	}

	AudioEngine::destroy ();
}

void
AudioEngineTest::test_start ()
{
	AudioEngine* engine = AudioEngine::create ();

	CPPUNIT_ASSERT (AudioEngine::instance ());

	boost::shared_ptr<AudioBackend> backend = engine->set_backend ("None (Dummy)", "Unit-Test", "");

	CPPUNIT_ASSERT (backend);

	CPPUNIT_ASSERT (engine->start () == 0);

	Glib::usleep(2000);

	CPPUNIT_ASSERT (engine->stop () == 0);

	AudioEngine::destroy ();
}
