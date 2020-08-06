#include <iostream>

#include "ardour/plugin_manager.h"
#include "ardour/search_paths.h"

#include "plugins_test.h"
#include "test_util.h"

CPPUNIT_TEST_SUITE_REGISTRATION (PluginsTest);

using namespace std;
using namespace ARDOUR;
using namespace PBD;

void
print_plugin_info (PluginInfoPtr pp)
{
	cout << "LADSPA Plugin, name " << pp->name
		<< ", category " << pp->category
		<< ", creator " << pp->creator
		<< ", path " << pp->path
		<< ", n_inputs " << pp->n_inputs.n_audio ()
		<< ", n_outputs " << pp->n_outputs.n_audio ()
		<< endl;

}

void
PluginsTest::test ()
{
	create_and_start_dummy_backend ();

	PluginManager& pm = PluginManager::instance ();

	pm.refresh (true);

	Searchpath ladspa_paths(ladspa_search_path ());

	cout << "Number of Ladspa paths found: " << ladspa_paths.size () << endl;

	for (vector<std::string>::iterator i = ladspa_paths.begin (); i != ladspa_paths.end(); ++i)
	{
		cout << "LADSPA search path includes: " << *i << endl;
	}

	const PluginInfoList& ladspa_list = pm.ladspa_plugin_info ();

	cout << "Number of Ladspa plugins found: " << ladspa_list.size () << endl;

	for (PluginInfoList::const_iterator i = ladspa_list.begin (); i != ladspa_list.end(); ++i)
	{
		print_plugin_info (*i);
	}

	stop_and_destroy_backend ();
}
