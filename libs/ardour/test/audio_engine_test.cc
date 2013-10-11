#include <iostream>

#include "ardour/audioengine.h"
#include "ardour/audio_backend.h"
#include "ardour/backend_search_path.h"

#include "audio_engine_test.h"
#include "test_common.h"

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
}

void
AudioEngineTest::test_start ()
{
	AudioEngine* engine = AudioEngine::create ();

	CPPUNIT_ASSERT_NO_THROW (engine->set_default_backend ());

	init_post_engine ();

	CPPUNIT_ASSERT (engine->start () == 0);

	// sleep
	// stop
	// destroy
}
