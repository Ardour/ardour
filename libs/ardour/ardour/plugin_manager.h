/*
 * Copyright (C) 2000-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2008-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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

#ifdef VST3_SUPPORT
struct VST3Info;
#endif

class LIBARDOUR_API PluginManager : public boost::noncopyable {
public:
	static PluginManager& instance();
	static std::string scanner_bin_path;
	static std::string vst3_scanner_bin_path;

	~PluginManager ();

	const ARDOUR::PluginInfoList& windows_vst_plugin_info ();
	const ARDOUR::PluginInfoList& lxvst_plugin_info ();
	const ARDOUR::PluginInfoList& mac_vst_plugin_info ();
	const ARDOUR::PluginInfoList& ladspa_plugin_info ();
	const ARDOUR::PluginInfoList& lv2_plugin_info ();
	const ARDOUR::PluginInfoList& au_plugin_info ();
	const ARDOUR::PluginInfoList& lua_plugin_info ();
	const ARDOUR::PluginInfoList& vst3_plugin_info ();

	void refresh (bool cache_only = false);
	void cancel_plugin_scan();
	void cancel_plugin_timeout();
	void clear_vst_cache ();
	void clear_vst_blacklist ();
	void clear_au_cache ();
	void clear_au_blacklist ();
	void clear_vst3_cache ();
	void clear_vst3_blacklist ();

	const std::string get_default_windows_vst_path() const { return windows_vst_path; }
	const std::string get_default_lxvst_path() const { return lxvst_path; }

	static uint32_t cache_version ();
	bool cache_valid () const;

	/* always return LXVST for any VST subtype */
	static PluginType to_generic_vst (const PluginType);

	/* format plugin type to human readable name
	 * @param short use at most 4 chars (useful for ctrl-surface displays)
	 */
	static std::string plugin_type_name (const PluginType, bool short_name = true);

	bool cancelled () const { return _cancel_scan; }
	bool no_timeout () const { return _cancel_timeout; }

	void reset_stats ();
	void stats_use_plugin (PluginInfoPtr const&);
	bool stats (PluginInfoPtr const&, int64_t& lru, uint64_t& use_count) const;
	void save_stats ();

	enum PluginStatusType {
		Normal = 0,
		Favorite,
		Hidden,
		Concealed
	};

	std::string user_plugin_metadata_dir () const;
	void save_statuses ();
	void set_status (ARDOUR::PluginType type, std::string unique_id, PluginStatusType status);
	PluginStatusType get_status (const PluginInfoPtr&) const;

	void save_tags ();

	std::string dump_untagged_plugins ();

	bool load_plugin_order_file (XMLNode &n) const;  //returns TRUE if the passed-in node has valid info
	void save_plugin_order_file (XMLNode &elem) const;

	enum TagType {
		FromPlug,           //tag info is being set from plugin metadata
		FromFactoryFile,    // ... from the factory metadata file
		FromUserFile,       // ... from the user's config data
		FromGui             // ... from the UI, in realtime: will emit a signal so ui can show "sanitized" string as it is generated
	};
	void set_tags (ARDOUR::PluginType type, std::string unique_id, std::string tags, std::string name, TagType tagtype);
	void reset_tags (PluginInfoPtr const&);
	std::string get_tags_as_string (PluginInfoPtr const&) const;
	std::vector<std::string> get_tags (PluginInfoPtr const&) const;

	enum TagFilter {
		All,
		OnlyFavorites,
		NoHidden
	};
	std::vector<std::string> get_all_tags (enum TagFilter) const;

	/** plugins were added to or removed from one of the PluginInfoLists */
	PBD::Signal0<void> PluginListChanged;

	/** Plugin Statistics (use-count, recently-used) changed */
	PBD::Signal0<void> PluginStatsChanged;

	/** A single plugin's Hidden/Favorite status changed */
	PBD::Signal3<void, ARDOUR::PluginType, std::string, PluginStatusType> PluginStatusChanged; //PluginType t, string id, string tag

	/** A single plugin's Tags status changed */
	PBD::Signal3<void, ARDOUR::PluginType, std::string, std::string> PluginTagChanged; //PluginType t, string id, string tag

private:

	struct PluginTag {
		PluginType const  type;
		std::string const unique_id;
		std::string const name;
		std::string       tags;
		TagType           tagtype;

		PluginTag (ARDOUR::PluginType t, std::string const& id, std::string const& tag, std::string const& n, TagType tt)
			: type (t), unique_id (id), name(n), tags (tag), tagtype (tt) {}

		bool operator== (PluginTag const& other) const {
			return other.type == type && other.unique_id == unique_id;
		}

		bool operator< (PluginTag const& other) const {
			if (other.type == type) {
				return other.unique_id < unique_id;
			}
			return other.type < type;
		}
	};

	struct PluginStatus {
		PluginType const       type;
		std::string const      unique_id;
		PluginStatusType const status;

		PluginStatus (ARDOUR::PluginType t, std::string const& id, PluginStatusType s = Normal)
			: type (t), unique_id (id), status (s) {}

		bool operator==(const PluginStatus& other) const {
			return other.type == type && other.unique_id == unique_id;
		}

		bool operator<(const PluginStatus& other) const {
			if (other.type == type) {
				return other.unique_id < unique_id;
			}
			return other.type < type;
		}
	};

	struct PluginStats {
		PluginType const  type;
		std::string const unique_id;
		int64_t           lru;
		uint64_t          use_count;

		PluginStats (ARDOUR::PluginType t, std::string const& id, int64_t lru = 0, uint64_t use_count = 0)
			: type (t), unique_id (id), lru (lru), use_count (use_count) {}

		bool operator==(const PluginStats& other) const {
			return other.type == type && other.unique_id == unique_id;
		}

		bool operator<(const PluginStats& other) const {
			if (other.type == type) {
				return other.unique_id < unique_id;
			}
			return other.type < type;
		}
	};

	typedef std::set<PluginTag> PluginTagList;
	PluginTagList ptags;
	PluginTagList ftags; /* factory-file defaults */

	typedef std::set<PluginStatus> PluginStatusList;
	PluginStatusList statuses;

	typedef std::set<PluginStats> PluginStatsList;
	PluginStatsList statistics;

	ARDOUR::PluginInfoList  _empty_plugin_info;
	ARDOUR::PluginInfoList* _windows_vst_plugin_info;
	ARDOUR::PluginInfoList* _lxvst_plugin_info;
	ARDOUR::PluginInfoList* _mac_vst_plugin_info;
	ARDOUR::PluginInfoList* _vst3_plugin_info;
	ARDOUR::PluginInfoList* _ladspa_plugin_info;
	ARDOUR::PluginInfoList* _lv2_plugin_info;
	ARDOUR::PluginInfoList* _au_plugin_info;
	ARDOUR::PluginInfoList* _lua_plugin_info;

	std::map<uint32_t, std::string> rdf_type;

	std::string windows_vst_path;
	std::string lxvst_path;

	bool _cancel_scan;
	bool _cancel_timeout;

	void detect_name_ambiguities (ARDOUR::PluginInfoList*);
	void detect_type_ambiguities (ARDOUR::PluginInfoList&);

	void conceal_duplicates (ARDOUR::PluginInfoList*, ARDOUR::PluginInfoList*);

	void load_statuses ();
	void load_tags ();
	void load_stats ();

	std::string sanitize_tag (const std::string) const;

	void ladspa_refresh ();
	void lua_refresh ();
	void lua_refresh_cb ();
	void windows_vst_refresh (bool cache_only = false);
	void mac_vst_refresh (bool cache_only = false);
	void lxvst_refresh (bool cache_only = false);
	void vst3_refresh (bool cache_only = false);

	void add_lrdf_data (const std::string &path);
	void add_ladspa_presets ();
	void add_windows_vst_presets ();
	void add_mac_vst_presets ();
	void add_lxvst_presets ();
	void add_presets (std::string domain);

	void au_refresh (bool cache_only = false);

	void lv2_refresh ();

	int windows_vst_discover_from_path (std::string path, bool cache_only = false);
	int windows_vst_discover (std::string path, bool cache_only = false);

	int mac_vst_discover_from_path (std::string path, bool cache_only = false);
	int mac_vst_discover (std::string path, bool cache_only = false);

	int vst3_discover_from_path (std::string const& path, bool cache_only = false);
	int vst3_discover (std::string const& path, bool cache_only = false);
#ifdef VST3_SUPPORT
	void vst3_plugin (std::string const& module_path, VST3Info const&);
	bool run_vst3_scanner_app (std::string bundle_path) const;
#endif

	int lxvst_discover_from_path (std::string path, bool cache_only = false);
	int lxvst_discover (std::string path, bool cache_only = false);

	int ladspa_discover (std::string path);

	std::string get_ladspa_category (uint32_t id);
	std::vector<uint32_t> ladspa_plugin_whitelist;

	PBD::ScopedConnection lua_refresh_connection;
	Glib::Threads::Mutex _lock;

	static PluginManager* _instance; // singleton
	PluginManager ();
};

} /* namespace ARDOUR */

#endif /* __ardour_plugin_manager_h__ */

