#include <iostream>

#include "ardour/plugin_manager.h"
#include "ardour/ladspa_search_path.h"

#include "plugins_test.h"
#include "test_common.h"

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
	PluginManager& pm = PluginManager::instance ();

	pm.refresh ();

	SearchPath ladspa_paths(ladspa_search_path ());

	cout << "Number of Ladspa paths found: " << ladspa_paths.size () << endl;

	for (vector<std::string>::iterator i = ladspa_paths.begin (); i != ladspa_paths.end(); ++i)
	{
		cout << "LADSPA search path includes: " << *i << endl;
	}

	PluginInfoList& ladspa_list = pm.ladspa_plugin_info ();

	cout << "Number of Ladspa plugins found: " << ladspa_list.size () << endl;

	for (PluginInfoList::iterator i = ladspa_list.begin (); i != ladspa_list.end(); ++i)
	{
		print_plugin_info (*i);
	}


}
