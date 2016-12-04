#include <list>
#include <glibmm.h>

#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/luascripting.h"
#include "ardour/lua_script_params.h"
#include "ardour/plugin_manager.h"
#include "ardour/plugin_insert.h"
#include "ardour/session.h"

#include "lua_script_test.h"

using namespace ARDOUR;

CPPUNIT_TEST_SUITE_REGISTRATION(LuaScriptTest);

void
LuaScriptTest::session_script_test ()
{
	LuaScriptList scripts (LuaScripting::instance ().scripts (LuaScriptInfo::Session));
	printf("\n * Testing %ld Lua session scripts\n", scripts.size());

	for (LuaScriptList::const_iterator s = scripts.begin(); s != scripts.end(); ++s) {
		const LuaScriptInfoPtr& spi (*s);

		std::string script = "";

		if (Glib::path_get_basename (spi->path).at(0) == '_') {
			continue;
		}

		try {
			script = Glib::file_get_contents (spi->path);
		} catch (Glib::FileError e) {
			CPPUNIT_FAIL ("Cannot read script file");
			continue;
		}

		try {
			LuaScriptParamList lsp = LuaScriptParams::script_params (spi, "sess_params");
			_session->register_lua_function ("test", script, lsp);
		} catch (SessionException e) {
			CPPUNIT_FAIL ("Cannot add script to session");
			continue;
		}
		CPPUNIT_ASSERT (!_session->registered_lua_functions ().empty());
		Glib::usleep(200000); // wait to script to execute during process()
		// if the script fails, it'll be removed.
		CPPUNIT_ASSERT (!_session->registered_lua_functions ().empty());
		_session->unregister_lua_function ("test");
		CPPUNIT_ASSERT (_session->registered_lua_functions ().empty());
	}
}

void
LuaScriptTest::dsp_script_test ()
{
	PluginManager& pm = PluginManager::instance ();
	std::list<boost::shared_ptr<AudioTrack> > tracks;

	tracks = _session->new_audio_track (2, 2, NULL, 1, "", PresentationInfo::max_order);
	CPPUNIT_ASSERT (tracks.size() == 1);
	boost::shared_ptr<Route> r = tracks.front ();

	std::cout << "\n";
	const PluginInfoList& plugs = pm.lua_plugin_info();
	for (PluginInfoList::const_iterator i = plugs.begin(); i != plugs.end(); ++i) {
		std::cout << "LuaProc: " <<(*i)->name << "\n";

		PluginPtr p = (*i)->load (*_session);
		CPPUNIT_ASSERT (p);

		boost::shared_ptr<Processor> processor (new PluginInsert (*_session, p));
		processor->enable (true);

		int rv = r->add_processor (processor, boost::shared_ptr<Processor>(), 0);
		CPPUNIT_ASSERT (rv == 0);
		processor->enable (true);
		Glib::usleep(200000); // run process, failing plugins will be deactivated.
		CPPUNIT_ASSERT (processor->active());
		rv = r->remove_processor (processor, NULL, true);
		CPPUNIT_ASSERT (rv == 0);
	}
}
