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

		if (Glib::path_get_basename (spi->path).find ("__") == 0) {
			continue;
		}

		if (Glib::path_get_basename (spi->path).at(0) == '_') {
			std::cout << "LuaSession: " << spi->name << " (not bundled)\n";
		} else {
			std::cout << "LuaSession: " << spi->name << "\n";
		}

		try {
			script = Glib::file_get_contents (spi->path);
		} catch (Glib::FileError const& e) {
			CPPUNIT_FAIL (spi->name + ": Cannot read script file");
			continue;
		}

		try {
			LuaScriptParamList lsp = LuaScriptParams::script_params (spi, "sess_params");
			_session->register_lua_function ("test", script, lsp);
		} catch (...) {
			CPPUNIT_FAIL (spi->name + ": Cannot add script to session");
			continue;
		}
		CPPUNIT_ASSERT_MESSAGE (spi->name, !_session->registered_lua_functions ().empty());
		Glib::usleep(200000); // wait to script to execute during process()
		// if the script fails, it'll be removed.
		CPPUNIT_ASSERT_MESSAGE (spi->name, !_session->registered_lua_functions ().empty());
		_session->unregister_lua_function ("test");
		CPPUNIT_ASSERT_MESSAGE (spi->name, _session->registered_lua_functions ().empty());
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

		if (Glib::path_get_basename ((*i)->path).find ("__") == 0) {
			/* Example scripts (filename with leading underscore), that
			 * use a double-underscore at the beginning of the file-name
			 * are excluded from unit-tests (e.g. "Lua Convolver"
			 * requires IR files).
			 */
			continue;
		}

		if (Glib::path_get_basename ((*i)->path).at(0) == '_') {
			std::cout << "LuaProc: " <<(*i)->name << " (not bundled)\n";
		} else {
			std::cout << "LuaProc: " <<(*i)->name << "\n";
		}

		PluginPtr p = (*i)->load (*_session);
		CPPUNIT_ASSERT_MESSAGE ((*i)->name, p);

		boost::shared_ptr<Processor> processor (new PluginInsert (*_session, r->time_domain(), p));
		processor->enable (true);

		int rv = r->add_processor (processor, boost::shared_ptr<Processor>(), 0);
		CPPUNIT_ASSERT_MESSAGE ((*i)->name, rv == 0);
		processor->enable (true);
		Glib::usleep(200000); // run process, failing plugins will be deactivated.
		CPPUNIT_ASSERT_MESSAGE ((*i)->name, processor->active());
		rv = r->remove_processor (processor, NULL, true);
		CPPUNIT_ASSERT_MESSAGE ((*i)->name, rv == 0);
	}
}
