#ifndef __ardour_plugin_manager_h__
#define __ardour_plugin_manager_h__

#include <list>
#include <map>
#include <string>

#include <boost/shared_ptr.hpp>

#include <ardour/types.h>
#include <ardour/plugin.h>
#include <ardour/audio_unit.h>

namespace ARDOUR {

class PluginInfo;
class Plugin;
class Session;
class AudioEngine;

class PluginManager {
  public:
	PluginManager (ARDOUR::AudioEngine&);
	~PluginManager ();

	ARDOUR::PluginInfoList &vst_plugin_info () { return _vst_plugin_info; }
	ARDOUR::PluginInfoList &ladspa_plugin_info () { return _ladspa_plugin_info; }
	ARDOUR::PluginInfoList &au_plugin_info () { return _au_plugin_info; }

	void refresh ();

	int add_ladspa_directory (std::string dirpath);
	int add_vst_directory (std::string dirpath);

	boost::shared_ptr<Plugin> load (ARDOUR::Session& s, PluginInfoPtr info);

	static PluginManager* the_manager() { return _manager; }

  private:
	ARDOUR::AudioEngine&   _engine;
	ARDOUR::PluginInfoList _vst_plugin_info;
	ARDOUR::PluginInfoList _ladspa_plugin_info;
	ARDOUR::PluginInfoList _au_plugin_info;
	std::map<uint32_t, std::string> rdf_type;

	std::string ladspa_path;
	std::string vst_path;

	void ladspa_refresh ();
	void vst_refresh ();

	void add_lrdf_data (const std::string &path);
	void add_ladspa_presets ();
	void add_vst_presets ();
	void add_presets (std::string domain);

	int vst_discover_from_path (std::string path);
	int vst_discover (std::string path);

	int ladspa_discover_from_path (std::string path);
	int ladspa_discover (std::string path);

	int au_discover ();

	std::string get_ladspa_category (uint32_t id);

	static PluginManager* _manager; // singleton
};

} /* namespace ARDOUR */

#endif /* __ardour_plugin_manager_h__ */

