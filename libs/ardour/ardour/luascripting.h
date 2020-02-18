/*
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
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
#ifndef _ardour_luascripting_h_
#define _ardour_luascripting_h_
#include <vector>

#include <boost/shared_ptr.hpp>
#include <glibmm/threads.h>

#include "pbd/signals.h"
#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class LIBARDOUR_API LuaScriptInfo {
  public:

	enum ScriptType {
		Invalid,
		DSP,
		Session,
		EditorHook,
		EditorAction,
		Snippet,
		SessionInit,
	};

	/* binary flags, valid for ActionScripts */
	enum ScriptSubType {
		None          = 0x00,
		RouteSetup    = 0x01,
		SessionSetup  = 0x02,
	};

	static std::string type2str (const ScriptType t);
	static ScriptType str2type (const std::string& str);

	LuaScriptInfo (ScriptType t, const std::string &n, const std::string &p, const std::string &uid)
	: type (t)
	, subtype (0)
	, name (n)
	, path (p)
	, unique_id (uid)
	{ }

	virtual ~LuaScriptInfo () { }

	ScriptType type;
	uint32_t   subtype;

	std::string name;
	std::string path;
	std::string unique_id;

	std::string author;
	std::string license;
	std::string category;
	std::string description;
};

struct LIBARDOUR_API LuaScriptParam {
	public:
		LuaScriptParam (
				const std::string& n,
				const std::string& t,
				const std::string& d,
				bool o, bool p)
			: name (n)
			, title (t)
			, dflt (d)
			, optional (o)
			, preseeded (p)
			, is_set (false)
			, value (d)
	{}

		std::string name;
		std::string title;
		std::string dflt;
		bool optional;
		bool preseeded;
		bool is_set;
		std::string value;
};


typedef boost::shared_ptr<LuaScriptInfo> LuaScriptInfoPtr;
typedef std::vector<LuaScriptInfoPtr> LuaScriptList;

typedef boost::shared_ptr<LuaScriptParam> LuaScriptParamPtr;
typedef std::vector<LuaScriptParamPtr> LuaScriptParamList;


class LIBARDOUR_API LuaScripting {

public:
	static LuaScripting& instance();

	~LuaScripting ();

	LuaScriptList &scripts (LuaScriptInfo::ScriptType);
	void refresh (bool run_scan = false);
	PBD::Signal0<void> scripts_changed;

	LuaScriptInfoPtr by_name (const std::string&, LuaScriptInfo::ScriptType);

	static LuaScriptInfoPtr script_info (const std::string &script);
	static bool try_compile (const std::string&, const LuaScriptParamList&);
	static std::string get_factory_bytecode (const std::string&, const std::string& ffn = "factory", const std::string& fp = "f");
	static std::string user_script_dir ();

	struct LIBARDOUR_API Sorter {
		bool operator() (LuaScriptInfoPtr const a, LuaScriptInfoPtr const b) const;
	};

private:
	static LuaScripting* _instance; // singleton
	LuaScripting ();

	void scan ();
	static LuaScriptInfoPtr scan_script (const std::string &, const std::string & sc = "");
	static void lua_print (std::string s);

	LuaScriptList *_sl_dsp;
	LuaScriptList *_sl_session;
	LuaScriptList *_sl_hook;
	LuaScriptList *_sl_action;
	LuaScriptList *_sl_snippet;
	LuaScriptList *_sl_setup;
	LuaScriptList *_sl_tracks;
	LuaScriptList  _empty_script_info;

	Glib::Threads::Mutex _lock;
};

} // namespace ARDOUR

#endif // _ardour_luascripting_h_
