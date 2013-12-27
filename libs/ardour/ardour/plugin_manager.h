/*
    Copyright (C) 2000-2007 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __ardour_plugin_manager_h__
#define __ardour_plugin_manager_h__

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <list>
#include <map>
#include <string>
#include <set>
#include <boost/utility.hpp>

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/plugin.h"

namespace ARDOUR {

class Plugin;

class LIBARDOUR_API PluginManager : public boost::noncopyable {
  public:
	static PluginManager& instance();

	~PluginManager ();

	ARDOUR::PluginInfoList &windows_vst_plugin_info ();
	ARDOUR::PluginInfoList &lxvst_plugin_info ();
	ARDOUR::PluginInfoList &ladspa_plugin_info ();
	ARDOUR::PluginInfoList &lv2_plugin_info ();
	ARDOUR::PluginInfoList &au_plugin_info ();

	void refresh ();

	int add_ladspa_directory (std::string dirpath);
	int add_windows_vst_directory (std::string dirpath);
	int add_lxvst_directory (std::string dirpath);

	enum PluginStatusType {
		Normal = 0,
		Favorite,
		Hidden
	};

	void load_statuses ();
	void save_statuses ();
	void set_status (ARDOUR::PluginType type, std::string unique_id, PluginStatusType status);
	PluginStatusType get_status (const PluginInfoPtr&);

	/** plugins were added to or removed from one of the PluginInfoLists */
	PBD::Signal0<void> PluginListChanged;

  private:
	struct PluginStatus {
	    ARDOUR::PluginType type;
	    std::string unique_id;
	    PluginStatusType status;

	    PluginStatus (ARDOUR::PluginType t, std::string id, PluginStatusType s = Normal)
	    : type (t), unique_id (id), status (s) {}

	    bool operator==(const PluginStatus& other) const {
		    return other.type == type && other.unique_id == unique_id;
	    }

	    bool operator<(const PluginStatus& other) const {
		    if (other.type < type) {
			    return true;
		    } else if (other.type == type && other.unique_id < unique_id) {
			    return true;
		    }
		    return false;
	    }
	};
	typedef std::set<PluginStatus> PluginStatusList;
	PluginStatusList statuses;

	ARDOUR::PluginInfoList  _empty_plugin_info;
	ARDOUR::PluginInfoList* _windows_vst_plugin_info;
	ARDOUR::PluginInfoList*	_lxvst_plugin_info;
	ARDOUR::PluginInfoList* _ladspa_plugin_info;
	ARDOUR::PluginInfoList* _lv2_plugin_info;
	ARDOUR::PluginInfoList* _au_plugin_info;

	std::map<uint32_t, std::string> rdf_type;

	std::string ladspa_path;
	std::string windows_vst_path;
	std::string lxvst_path;

	void ladspa_refresh ();
	void windows_vst_refresh ();
	void lxvst_refresh ();

	void add_lrdf_data (const std::string &path);
	void add_ladspa_presets ();
	void add_windows_vst_presets ();
	void add_lxvst_presets ();
	void add_presets (std::string domain);

	void au_refresh ();

	void lv2_refresh ();

	int windows_vst_discover_from_path (std::string path);
	int windows_vst_discover (std::string path);
	
	int lxvst_discover_from_path (std::string path);
	int lxvst_discover (std::string path);

	int ladspa_discover_from_path (std::string path);
	int ladspa_discover (std::string path);

	std::string get_ladspa_category (uint32_t id);
	std::vector<uint32_t> ladspa_plugin_whitelist;

	static PluginManager* _instance; // singleton
	PluginManager ();
};

} /* namespace ARDOUR */

#endif /* __ardour_plugin_manager_h__ */

